/*
 * Copyright © 2021 John Viega
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
 *  Name:           debug.h
 *  Description:    Debugging via in-memory ring buffer, for use when
 *                  HATRACK_DEBUG is on.
 *
 *  Author:         John Viega, john@zork.org
 */

#ifndef __HATRACK_DEBUG_H__
#define __HATRACK_DEBUG_H__

#ifdef HATRACK_DEBUG

#include "config.h"

#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    _Alignas(32) uint64_t sequence;
    char    msg[HATRACK_DEBUG_MSG_SIZE];
    int64_t thread;
    char    null;
} hatrack_debug_record_t;

extern hatrack_debug_record_t __hatrack_debug[];
extern _Atomic uint64_t       __hatrack_debug_sequence;
extern const char             __hatrack_hex_conversion_table[];
extern __thread int64_t       mmm_mytid;

// clang-format off
void debug_dump         (uint64_t);
void debug_thread       ();
void debug_other_thread (int64_t);
void debug_grep         (char *);
void debug_pgrep        (uintptr_t);
// clang-format on

static inline void
hatrack_debug(char *msg)
{
    uint64_t                mysequence;
    uint64_t                index;
    hatrack_debug_record_t *record_ptr;

    mysequence           = atomic_fetch_add(&__hatrack_debug_sequence, 1);
    index                = mysequence & HATRACK_DEBUG_RING_LAST_SLOT;
    record_ptr           = &__hatrack_debug[index];
    record_ptr->sequence = mysequence;
    record_ptr->thread   = mmm_mytid;
    strncpy(record_ptr->msg, msg, HATRACK_DEBUG_MSG_SIZE);
}

#undef HATRACK_PTR_FMT_CHRS
#define HATRACK_PTR_FMT_CHRS 4

static inline void
hatrack_debug_ptr(void *addr, char *msg)
{
    char buf[HATRACK_PTR_CHRS + HATRACK_PTR_FMT_CHRS + 1] = {
        '0',
        'x',
    };
    char                   *p = buf + HATRACK_PTR_CHRS + HATRACK_PTR_FMT_CHRS;
    uint64_t                i;
    uintptr_t               n = (uintptr_t)addr;
    uint64_t                mysequence;
    hatrack_debug_record_t *record_ptr;

    mysequence = atomic_fetch_add(&__hatrack_debug_sequence, 1);
    record_ptr = &__hatrack_debug[mysequence & HATRACK_DEBUG_RING_LAST_SLOT];

    record_ptr->sequence = mysequence;
    record_ptr->thread   = mmm_mytid;

    *--p = ' ';
    *--p = ':';

    for (i = 0; i < HATRACK_PTR_CHRS; i++) {
        *--p = __hatrack_hex_conversion_table[n & 0xf];
        n >>= 4;
    }
    strcpy(record_ptr->msg, buf);
    strncpy(record_ptr->msg + HATRACK_PTR_CHRS + HATRACK_PTR_FMT_CHRS,
            msg,
            HATRACK_DEBUG_MSG_SIZE - HATRACK_PTR_CHRS - HATRACK_PTR_FMT_CHRS);
}

#define DEBUG(x)        hatrack_debug(x)
#define DEBUG_PTR(x, y) hatrack_debug_ptr((void *)(x), y)

#else

#define DEBUG(x)
#define DEBUG_PTR(x, y)

#endif

#endif
