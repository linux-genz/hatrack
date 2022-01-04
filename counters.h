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
 *  Name:           counters.h
 *  Description:    In-memory counters for performance monitoring,
 *                  when HATRACK_DEBUG is on.
 *
 *  Author:         John Viega, john@zork.org
 *
 */

#ifndef __COUNTERS_H__
#define __COUNTERS_H__

#include <stdint.h>
#include <stdalign.h>
#include <stdatomic.h>

#ifdef HATRACK_COUNTERS
extern _Atomic uint64_t hatrack_counters[];
extern char            *hatrack_counter_names[];

extern _Atomic uint64_t hatrack_yn_counters[][2];
extern char            *hatrack_yn_counter_names[];

enum : uint64_t
{
    HATRACK_CTR_MALLOCS,
    HATRACK_CTR_FREES,
    HATRACK_CTR_RETIRE_UNUSED,
    HATRACK_CTR_STORE_SHRINK,
    HATRACK_CTR_WH_HELP_REQUESTS,
    HATRACK_COUNTERS_NUM,
};

enum : uint64_t
{
    HATRACK_CTR_LINEAR_EPOCH_EQ = 0,
    HATRACK_CTR_COMMIT          = 1,
    HATRACK_CTR_COMMIT_HELPS    = 2,
    LOHAT0_CTR_BUCKET_ACQUIRE   = 3,
    LOHAT0_CTR_REC_INSTALL      = 4,
    LOHAT0_CTR_DEL              = 5,
    LOHAT0_CTR_NEW_STORE        = 6,
    LOHAT0_CTR_F_MOVING         = 7,
    LOHAT0_CTR_F_MOVED1         = 8,
    LOHAT0_CTR_F_MOVED2         = 9,
    LOHAT0_CTR_MIGRATE_HV       = 10,
    LOHAT0_CTR_MIG_REC          = 11,
    LOHAT0_CTR_F_MOVED3         = 12,
    LOHAT0_CTR_LEN_INSTALL      = 13,
    LOHAT0_CTR_STORE_INSTALL    = 14,
    LOHAT1_CTR_BUCKET_ACQUIRE   = 15,
    LOHAT1_CTR_PTR_INSTALL      = 16,
    LOHAT1_CTR_HIST_HASH        = 17,
    LOHAT1_CTR_REC_INSTALL      = 18,
    LOHAT1_CTR_DEL              = 19,
    LOHAT1_CTR_NEW_STORE        = 20,
    LOHAT1_CTR_F_MOVING         = 21,
    LOHAT1_CTR_F_MOVED1         = 22,
    LOHAT1_CTR_F_MOVED2         = 23,
    LOHAT1_CTR_MIGRATE_HV       = 24,
    LOHAT1_CTR_MIG_REC          = 25,
    LOHAT1_CTR_MV_IH            = 26,
    LOHAT1_CTR_NEW_PTR          = 27,
    LOHAT1_CTR_F_MOVED3         = 28,
    LOHAT1_CTR_F_HIST           = 29,
    LOHAT1_CTR_STORE_INSTALL    = 30,
    LOHAT2_CTR_BUCKET_ACQUIRE   = 31,
    LOHAT2_CTR_PTR_INSTALL      = 32,
    LOHAT2_CTR_HIST_HASH        = 33,
    LOHAT2_CTR_FWD              = 34,
    LOHAT2_CTR_REC_INSTALL      = 35,
    LOHAT2_CTR_DEL              = 36,
    LOHAT2_CTR_NEW_STORE        = 37,
    LOHAT2_CTR_F_MOVING         = 38,
    LOHAT2_CTR_F_MOVED1         = 39,
    LOHAT2_CTR_F_MOVED2         = 40,
    LOHAT2_CTR_MIGRATE_HV       = 41,
    LOHAT2_CTR_MIG_REC          = 42,
    LOHAT2_CTR_MV_IH            = 43,
    LOHAT2_CTR_NEW_PTR          = 44,
    LOHAT2_CTR_F_MOVED3         = 45,
    LOHAT2_CTR_F_HIST           = 46,
    LOHAT2_CTR_STORE_INSTALL    = 47,
    HIHAT_CTR_BUCKET_ACQUIRE    = 48,
    HIHAT_CTR_REC_INSTALL       = 49,
    HIHAT_CTR_DEL               = 50,
    HIHAT_CTR_NEW_STORE         = 51,
    HIHAT_CTR_F_MOVING          = 52,
    HIHAT_CTR_F_MOVED1          = 53,
    HIHAT_CTR_MIGRATE_HV        = 54,
    HIHAT_CTR_MIG_REC           = 55,
    HIHAT_CTR_F_MOVED2          = 56,
    HIHAT_CTR_LEN_INSTALL       = 57,
    HIHAT_CTR_STORE_INSTALL     = 58,
    HIHAT_CTR_SLEEP_NO_JOB      = 59,
    WITCHHAT_CTR_BUCKET_ACQUIRE = 60,
    WITCHHAT_CTR_REC_INSTALL    = 61,
    WITCHHAT_CTR_DEL            = 62,
    WITCHHAT_CTR_F_MOVING       = 63,
    WITCHHAT_CTR_NEW_STORE      = 64,
    WITCHHAT_CTR_F_MOVED1       = 65,
    WITCHHAT_CTR_MIGRATE_HV     = 66,
    WITCHHAT_CTR_MIG_REC        = 67,
    WITCHHAT_CTR_F_MOVED2       = 68,
    WITCHHAT_CTR_LEN_INSTALL    = 69,
    WITCHHAT_CTR_STORE_INSTALL  = 70,
    WOOLHAT_CTR_BUCKET_ACQUIRE  = 71,
    WOOLHAT_CTR_REC_INSTALL     = 72,
    WOOLHAT_CTR_DEL             = 73,
    WOOLHAT_CTR_NEW_STORE       = 74,
    WOOLHAT_CTR_F_MOVING        = 75,
    WOOLHAT_CTR_F_MOVED1        = 76,
    WOOLHAT_CTR_F_MOVED2        = 77,
    WOOLHAT_CTR_MIGRATE_HV      = 78,
    WOOLHAT_CTR_MIG_REC         = 79,
    WOOLHAT_CTR_F_MOVED3        = 80,
    WOOLHAT_CTR_LEN_INSTALL     = 81,
    WOOLHAT_CTR_STORE_INSTALL   = 82,
    HATRACK_YN_COUNTERS_NUM
};

static inline _Bool
hatrack_yn_ctr_t(uint64_t id)
{
    atomic_fetch_add(&hatrack_yn_counters[id][0], 1);
    return 1;
}

static inline _Bool
hatrack_yn_ctr_f(uint64_t id)
{
    atomic_fetch_add(&hatrack_yn_counters[id][1], 1);
    return 0;
}

void counters_output_delta(void);
void counters_output_alltime(void);

#define HATRACK_CTR_ON(id) atomic_fetch_add(&hatrack_counters[id], 1)
#define HATRACK_CTR_OFF(id)
#define HATRACK_YN_ON(x, id)  ((x) ? hatrack_yn_ctr_t(id) : hatrack_yn_ctr_f(id))
#define HATRACK_YN_OFF(x, id) (x)

#else

#define HATRACK_CTR_ON(id)
#define HATRACK_CTR_OFF(id)
#define HATRACK_YN_ON(x, id)  (x)
#define HATRACK_YN_OFF(x, id) (x)

#define counters_output_delta()
#define counters_output_alltime()

#endif /* defined(HATRACK_COUNTERS) */

#define HATRACK_CTR(id)       HATRACK_CTR_ON(id)
#define HATRACK_YN_CTR(x, id) HATRACK_YN_ON(x, id)

#endif /* __COUNTERS_H__ */
