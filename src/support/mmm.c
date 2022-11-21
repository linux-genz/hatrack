/*
 * Copyright © 2021-2022 John Viega
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  Name:           mmm.c
 *  Description:    Miniature memory manager: a malloc wrapper to support
 *                  linearization and safe reclaimation for my hash tables.
 *
 *  Author:         John Viega, john@zork.org
 *
 */

#include <hatrack.h>

// clang-format off
__thread mmm_header_t  *mmm_retire_list  = NULL;
__thread pthread_once_t mmm_inited       = PTHREAD_ONCE_INIT;
_Atomic  uint64_t       mmm_epoch        = HATRACK_EPOCH_FIRST;
_Atomic  uint64_t       mmm_nexttid      = 0;
__thread int64_t        mmm_mytid        = -1; 
__thread uint64_t       mmm_retire_ctr   = 0;

         uint64_t       mmm_reservations[HATRACK_THREADS_MAX] = { 0, };

//clang-format on


static void    mmm_empty(void);


/*
 * We want to avoid overrunning the reservations array that our memory
 * management system uses.
 *
 * In all cases, the first HATRACK_THREADS_MAX threads to register
 * with us get a steadily increasing id. Also, threads will yield
 * their TID altogether if and when they call the mmm thread cleanup
 * routine-- mmm_clean_up_before_exit().
 *
 * The unused ID then goes on a linked list (mmm_free_tids), and
 * doesn't get touched until we run out of TIDs to give out.
 *
 */

static _Atomic (mmm_free_tids_t *) mmm_free_tids;

/* This grabs an mmm-specific threadid and stashes it in the
 * thread-local variable mmm_mytid.
 * 
 * We have a fixed number of TIDS to give out though (controlled by
 * the preprocessor variable, HATRACK_THREADS_MAX).  We give them out
 * sequentially till they're done, and then we give out ones that have
 * been "given back", which are stored on a stack (mmm_free_tids).
 *
 * If we finally run out, we abort.
 */
void
mmm_register_thread(void)
{
    mmm_free_tids_t *head;

    if (mmm_mytid != -1) {
	return;
    }
    mmm_mytid = atomic_fetch_add(&mmm_nexttid, 1);
    
    if (mmm_mytid >= HATRACK_THREADS_MAX) {
	head = atomic_load(&mmm_free_tids);
	
	do {
	    if (!head) {
		abort();
	    }
	} while (!CAS(&mmm_free_tids, &head, head->next));
	
	mmm_mytid = head->tid;
	mmm_retire(head);
    }
    
    mmm_reservations[mmm_mytid] = HATRACK_EPOCH_UNRESERVED;

    return;
}

// Call when a thread exits to add to the free TID stack.
static void
mmm_tid_giveback(void)
{
    mmm_free_tids_t *new_head;
    mmm_free_tids_t *old_head;

    new_head       = mmm_alloc(sizeof(mmm_free_tids_t));
    new_head->tid  = mmm_mytid;
    old_head       = atomic_load(&mmm_free_tids);

    do {
	new_head->next = old_head;
    } while (!CAS(&mmm_free_tids, &old_head, new_head));

    return;
}

// This is here for convenience of testing; generally this
// is not the way to handle tid recyling!
void mmm_reset_tids(void)
{
    atomic_store(&mmm_nexttid, 0);

    return;
}

/* For now, our cleanup function spins until it is able to retire
 * everything on its list. Soon, when we finally get around to
 * worrying about thread kills, we will change this to add its
 * contents to an "ophan" list.
 */
void
mmm_clean_up_before_exit(void)
{
    if (mmm_mytid == -1) {
	return;
    }

    mmm_end_op();
    
    while (mmm_retire_list) {
	mmm_empty();
    }
    
    mmm_tid_giveback();
    
    return;
}

/* Sets the retirement epoch on the pointer, and adds it to the
 * thread-local retirement list.
 *
 * We also keep a thread-local counter of how many times we've called
 * this function, and every HATRACK_RETIRE_FREQ calls, we run
 * mmm_empty(), which walks our thread-local retirement list, looking
 * for items to free.
 */
void
mmm_retire(void *ptr)
{
    mmm_header_t *cell;

    cell = mmm_get_header(ptr);

#ifdef HATRACK_MMM_DEBUG
// Don't need this check when not debugging algorithms.
    if (!cell->write_epoch) {
	DEBUG_MMM_INTERNAL(ptr, "No write epoch??");
	abort();
    }
// Algorithms that steal bits from pointers might steal up to
// three bits, thus the mask of 0x07.
    if (hatrack_pflag_test(ptr, 0x07)) {
	DEBUG_MMM_INTERNAL(ptr, "Bad alignment on retired pointer.");
	abort();
    }

    /* Detect multiple threads adding this to their retire list.
     * Generally, you should be able to avoid this, but with
     * HATRACK_MMM_DEBUG on we explicitly check for it.
     */
    if (cell->retire_epoch) {
	DEBUG_MMM_INTERNAL(ptr, "Double free");
	DEBUG_PTR((void *)atomic_load(&mmm_epoch), "epoch of double free");
	
	abort();
	
	return;
    }
#endif	
    
    cell->retire_epoch = atomic_load(&mmm_epoch);
    cell->next         = mmm_retire_list;
    mmm_retire_list    = cell;

    DEBUG_MMM_INTERNAL(cell->data, "mmm_retire");

    if (++mmm_retire_ctr & HATRACK_RETIRE_FREQ) {
	mmm_retire_ctr = 0;
	mmm_empty();
    }

    return;
}

/* The basic gist of this algorithm is that we're going to look at
 * every reservation we can find, identifying the oldest reservation
 * in the list.
 *
 * Then, we can then safely free anything in the list with an earlier
 * retirement epoch than the reservation time. Since the items in the
 * stack were pushed on in order of their retirement epoch, it
 * suffices to find the first item that is lower than the target,
 * and free everything else.
 */
static void
mmm_empty(void)
{
    mmm_header_t *tmp;
    mmm_header_t *cell;
    uint64_t      lowest;
    uint64_t      reservation;
    uint64_t      lasttid;
    uint64_t      i;

    /* We don't have to search the whole array, just the items assigned
     * to active threads. Even if a new thread comes along, it will
     * not be able to reserve something that's already been retired
     * by the time we call this.
     */
    lasttid = atomic_load(&mmm_nexttid);
    
    if (lasttid > HATRACK_THREADS_MAX) {
	lasttid = HATRACK_THREADS_MAX;
    }

    /* We start out w/ the "lowest" reservation we've seen as
     * HATRACK_EPOCH_MAX.  If this value never changes, then it
     * means no epochs were reserved, and we can safely
     * free every record in our stack.
     */    
    lowest = HATRACK_EPOCH_MAX;

    for (i = 0; i < lasttid; i++) {
	reservation = mmm_reservations[i];
	
	if (reservation < lowest) {
	    lowest = reservation;
	}
    }
    
    /* The list here is ordered by retire epoch, with most recent on
     * top.  Go down the list until the NEXT cell is the first item we
     * should delete.
     * 
     * Then, set the current cell's next pointer to NULL (since
     * it's the new end of the list), and then place the pointer at
     * the top of the list of cells to delete.
     *
     * Note that this function is only called if there's something
     * something on the retire list, so cell will never start out
     * empty.
     */
    cell = mmm_retire_list;

    // Special-case this, in case we have to delete the head cell,
    // to make sure we reinitialize the linked list right.
    if (mmm_retire_list->retire_epoch < lowest) {
	mmm_retire_list = NULL;
    } else {
	while (true) {
	    // We got to the end of the list, and didn't
	    // find one we should bother deleting.
	    if (!cell->next) {
		return;
	    }
	    
	    if (cell->next->retire_epoch < lowest) {
		tmp       = cell;
		cell      = cell->next;
		tmp->next = NULL;
		break;
	    }
	    
	    cell = cell->next;
	}
    }

    // Now cell and everything below it can be freed.
    while (cell) {
	tmp  = cell;
	cell = cell->next;
	HATRACK_FREE_CTR();
	DEBUG_MMM_INTERNAL(tmp->data, "mmm_empty::free");

	// Call the cleanup handler, if one exists.
	if (tmp->cleanup) {
	    (*tmp->cleanup)(&tmp->data, tmp->cleanup_aux);
	}
	
	free(tmp);
    }

    return;
}
