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
 *  Name:           array.c
 *  Description:    Example for flexarrays.
 *
 *  Author:         John Viega, john@zork.org
 */

#include <hatrack.h>
#include <stdio.h>

/* This starts out with an empty array, and spawns 8 threads.
 *
 * Each thread counts i from 0 to 10,000,000 inserting it's ID | i into
 * the i'th element.
 *
 * We do this to keep each thread writing something different, but
 * when we read the items back at the end (to confirm that they got
 * written out), we mask out the thread IDs.
 *
 * Whenever an array insert operation fails due to an out of bounds
 * error, we increase the size of the array by a mere 100 items.
 */

const int      NUM_ITERS   = 10000000;
const int      NUM_THREADS = 8;
const int      GROW_SIZE   = 100;
const uint64_t MASK        = 0x00000000ffffffff;

flexarray_t *array;

static inline void *
get_fill_value(uint64_t i)
{
    return (void *)((mmm_mytid << 32) | i);
}

void *
fill_array(void * unused)
{
    (void)unused;
    uint64_t i, arr_sz, store_sz, new_arr_sz, new_store_sz;

    for (i = 0; i < NUM_ITERS; i++) {
        while (!flexarray_set(array, i, get_fill_value(i))) {
	    flexarray_get_sizes(array, &arr_sz, &store_sz);
	    flexarray_grow(array, arr_sz + GROW_SIZE);
	    flexarray_get_sizes(array, &new_arr_sz, &new_store_sz);
	    if (new_store_sz != store_sz) {
		printf("grow: new size=%lu, store=%lu\n", new_arr_sz, new_store_sz);
		fflush(stdout);
	    }
        }
    }

    mmm_clean_up_before_exit();
    return NULL;
}

int64_t
sum_range(int64_t low, int64_t high)
{
    int64_t num_items  = high - low + 1;
    int64_t pair_value = low + high;

    return (pair_value * num_items) >> 1;
}

int
main(void)
{
    pthread_t threads[NUM_THREADS];
    int       i;
    int       status;
    int64_t   sum1 = sum_range(0, NUM_ITERS - 1);
    int64_t   sum2 = 0;
    uint64_t  item;
    uint64_t  arr_sz, store_sz;

    mmm_init("hatrack-array", GB(8));
    array = flexarray_new(0);
    flexarray_get_sizes(array, &arr_sz, &store_sz);
    printf("Initial sizes: array=%lu, store=%lu\n", arr_sz, store_sz);
    fflush(stdout);

    for (i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, fill_array, NULL);
    }

    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    for (i = 0; i < NUM_ITERS; i++) {
        item = (uint64_t)flexarray_get(array, i, &status);
        sum2 += (item & MASK);
    }

    flexarray_get_sizes(array, &arr_sz, &store_sz);
    printf("Final sizes: array=%lu, store=%lu\n", arr_sz, store_sz);
    printf("Expected sum: %ld\n", sum1);
    printf("Computed sum: %ld\n", sum2);
    fflush(stdout);

    flexarray_delete(array);

    mmm_clean_up_before_exit();
    return 0;
}
