/*
 * Copyright © 2022 John Viega
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License atn
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  Name:           queue.c
 *  Description:    A fast, wait-free queue implementation.
 *
 *  Author:         John Viega, john@zork.org
 */

#include <hatrack.h>

static const queue_item_t empty_cell      = { NULL, QUEUE_EMPTY };
static const queue_item_t too_slow_marker = { NULL, QUEUE_TOOSLOW };

static queue_segment_t *
queue_new_segment(uint64_t num_cells)
{
    queue_segment_t *ret;
    uint64_t         len;

    len       = sizeof(queue_segment_t) + sizeof(queue_item_t) * num_cells;
    ret       = mmm_alloc_committed(len);
    ret->size = num_cells;

    return ret;
}

void
queue_init(queue_t *self)
{
    return queue_init_size(self, QSIZE_LOG_DEFAULT);
}

void
queue_init_size(queue_t *self, char size_log)
{
    queue_seg_ptrs_t segments;
    queue_segment_t *initial_segment;
    uint64_t         seg_cells; // Number of cells per segment
    
    if (!size_log) {
	size_log = QSIZE_LOG_DEFAULT;
    } else {
	if (size_log < QSIZE_LOG_MIN || size_log > QSIZE_LOG_MAX) {
	    abort();
	}
    }
    
    seg_cells                  = 1 << size_log;
    self->default_segment_size = seg_cells;
    initial_segment            = queue_new_segment(seg_cells);
    segments.enqueue_segment   = initial_segment;
    segments.dequeue_segment   = initial_segment;

    atomic_store(&self->segments, segments);
    atomic_store(&self->help_needed, 0);
    atomic_store(&self->len, 0);

    return;
}

queue_t *
queue_new(void)
{
    return queue_new_size(QSIZE_LOG_DEFAULT);
}

queue_t *
queue_new_size(char size)
{
    queue_t *ret;

    ret = (queue_t *)malloc(sizeof(queue_t));
    queue_init_size(ret, size);

    return ret;
}

/* We assume here that this is only going to get called when there are
 * definitely no more enqueuers/dequeuers in the queue.  If you need
 * to decref or free any remaining contents, drain the queue before
 * calling cleanup.
 */
void
queue_cleanup(queue_t *self)
{
    queue_seg_ptrs_t  segments;
    queue_segment_t  *cur;
    queue_segment_t  *next;

    segments = atomic_load(&self->segments);
    cur      = segments.dequeue_segment;

    while (cur) {
	next = atomic_load(&cur->next);
	
	mmm_retire_unused(cur);

	cur = next;
    }
    
    return;
}

void
queue_delete(queue_t *self)
{
    queue_cleanup(self);
    free(self);

    return;
}

/* queue_enqueue is pretty simple in the average case. It only gets
 * complicated when the segment we're working in runs out of cells
 * in which we're allowed to enqueue.  Otherwise, we're just
 * using FAA to get a new slot to write into, and if it fails, 
 * it's because a dequeue thinks we're too slow, so we start
 * increasing the "step" value exponentially (dequeue ops only
 * ever increase in steps of 1).
 */
void
queue_enqueue(queue_t *self, void *item)
{
    queue_seg_ptrs_t  segments;
    queue_seg_ptrs_t  candidate_segments;
    queue_segment_t  *segment;
    queue_item_t      expected;
    queue_item_t      candidate;
    uint64_t          end_size;
    uint64_t          cur_ix;
    uint64_t          step;
    bool              need_help;
    bool              need_to_enqueue;
    queue_segment_t  *new_segment;
    queue_segment_t  *expected_segment;    
    uint64_t          new_size;
    
    step = 1;
    
    mmm_start_basic_op();

    need_help       = false;
    segments        = atomic_read(&self->segments);
    segment         = segments.enqueue_segment;
    end_size        = segment->size;
    cur_ix          = atomic_fetch_add(&segment->enqueue_index, step);
    candidate.state = QUEUE_USED;
    candidate.item  = item;

 try_again:
    while (cur_ix < end_size) {
	expected = empty_cell;
	if (CAS(&segment->cells[cur_ix], &expected, candidate)) {
	    
	    if (need_help) {
		atomic_fetch_sub(&self->help_needed, 1);
	    }
	    mmm_end_op();

	    atomic_fetch_add(&self->len, 1);
	    return;
	}
	step <<= 1;
	cur_ix = atomic_fetch_add(&segment->enqueue_index, step);
    }
    
    if (step >= QUEUE_HELP_VALUE && !need_help) {
	need_help = true;
	atomic_fetch_add(&self->help_needed, 1);
	
	segments = atomic_read(&self->segments);
	
	if (segments.enqueue_segment != segment) {
	    segment = segments.enqueue_segment;
	    cur_ix  = atomic_fetch_add(&segment->enqueue_index, step);
	    goto try_again;
	}
	
	new_size = segment->size << 1;
    }
    else {
	segments = atomic_read(&self->segments);
	
	if (segments.enqueue_segment != segment) {
	    segment = segments.enqueue_segment;
	    cur_ix = atomic_fetch_add(&segment->enqueue_index, step);
	    goto try_again;
	}
	    
	if (atomic_read(&self->help_needed)) {
	    new_size = segment->size << 1;
	}
	else {
	    new_size = self->default_segment_size;
	}
    }

    new_segment                = queue_new_segment(new_size);
    new_segment->enqueue_index = 1;
    expected_segment           = NULL;
    
    atomic_store(&new_segment->cells[0], candidate);
    

    if (!CAS(&segment->next, &expected_segment, new_segment)) {
	mmm_retire_unused(new_segment);
	new_segment = expected_segment;
	need_to_enqueue = true;
    }
    else {
	need_to_enqueue = false;
    }

    candidate_segments.enqueue_segment = new_segment;
    candidate_segments.dequeue_segment = segments.dequeue_segment;

    while (!CAS(&self->segments, &segments, candidate_segments)) {
	if (segments.enqueue_segment != segment) {
	    break;
	}
	candidate_segments.dequeue_segment = segments.dequeue_segment;
    }

    if (!need_to_enqueue) {
	if (need_help) {
	    atomic_fetch_sub(&self->help_needed, 1);
	}

	mmm_end_op();
	atomic_fetch_add(&self->len, 1);
	
	return;
    }
    segment = new_segment;
    cur_ix  = atomic_fetch_add(&segment->enqueue_index, step);

    goto try_again;
}

void *
queue_dequeue(queue_t *self, bool *found)
{
    queue_seg_ptrs_t segments;
    queue_seg_ptrs_t candidate_segments;
    queue_segment_t *segment;
    queue_segment_t *new_segment;
    queue_item_t     cell_contents;
    uint64_t         cur_ix;
    uint64_t         head_ix;
    void            *ret;

    mmm_start_basic_op();

    segments = atomic_read(&self->segments);
    segment  = segments.dequeue_segment;

 retry_dequeue:
    
    while (true) {
	cur_ix  = atomic_load(&segment->dequeue_index);
	head_ix = atomic_load(&segment->enqueue_index);
	
	if (cur_ix >= segment->size) {
	    break;
	}
	
	if (cur_ix >= head_ix) {
	    return hatrack_not_found_w_mmm(found);
	}
	
	cur_ix = atomic_fetch_add(&segment->dequeue_index, 1);
	if (cur_ix >= segment->size) {
	    break;
	}
	
	cell_contents = empty_cell;
	
	if (CAS(&segment->cells[cur_ix], &cell_contents, too_slow_marker)) {
	    continue;
	}
	
	ret = cell_contents.item;

	atomic_fetch_sub(&self->len, 1);
	
	return hatrack_found_w_mmm(found, ret);
    }
    
    new_segment = atomic_read(&segment->next);
    if (!new_segment) {
	/* The enqueuer threads have not completed setting up a new segment
	 * yet, so the queue is officially empty.
	 *
	 * Some future dequeuer will be back here to change the
	 * dequeue segment pointer.
	 */
	return hatrack_not_found_w_mmm(found);
    }

    candidate_segments.enqueue_segment = segments.enqueue_segment;
    candidate_segments.dequeue_segment = new_segment;
    
    while (!CAS(&self->segments, &segments, candidate_segments)) {
	/* If we fail, and someone else updated the dequeue segment,
	 * then we try again in that new segment.
	 */
	if (segments.dequeue_segment != segment) {
	    // We must be way behind.
	    segment = segments.dequeue_segment;
	    
	    goto retry_dequeue;
	}
	
	/* Otherwise, the enqueue segment was updated, and 
	 * we should try again w/ the proper enqueue segment.
	 */
	candidate_segments.enqueue_segment = segments.enqueue_segment;
    }
    
    mmm_retire(segment);
    segments = candidate_segments;
    segment  = new_segment;
    
    goto retry_dequeue;
}
