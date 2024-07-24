#ifndef __OFFSET_HOLDER_H__
#define __OFFSET_HOLDER_H__

#include <stdint.h>

#ifdef HATRACK_FABRIC
typedef uintptr_t off_holder_t;

#define UINTPTR_T_BITS  (sizeof(uintptr_t) * 8)
#define OFF_HOLDER_FLAG (1UL << (UINTPTR_T_BITS-1))
#define OFF_HOLDER_MASK (OFF_HOLDER_FLAG - 1)
#define OFF_XOR_MASK    (1UL << (UINTPTR_T_BITS-2))

// https://graphics.stanford.edu/~seander/bithacks.html#VariableSignExtend
static inline intptr_t sign_ext_off(uintptr_t off)
{
    intptr_t x;

    x = off & OFF_HOLDER_MASK; // clear MSB
    return (x ^ OFF_XOR_MASK) - OFF_XOR_MASK;
}

static inline uintptr_t _ptr2off(uintptr_t ptr, uintptr_t holder)
{
    return (ptr == 0UL) ? 0UL : ((ptr - holder) | OFF_HOLDER_FLAG);
}

static inline uintptr_t _off2ptr(uintptr_t off, uintptr_t holder)
{
    return (off & OFF_HOLDER_FLAG) ? (sign_ext_off(off) + holder) : off;
}

#define ptr2off(ptr, holder) _ptr2off((uintptr_t)(ptr), (uintptr_t)(holder))

#define off2ptr(off, holder, ptr) (typeof(ptr))_off2ptr((off), (uintptr_t)(holder))

// Revisit: delete these when the above versions are working & fast
// Revisit: ptr is evaluated twice
#define old_ptr2off(ptr, holder) ((ptr) == NULL) ? 0UL : \
    (((uintptr_t)(ptr) - (uintptr_t)&(holder)) | OFF_HOLDER_FLAG)
// Revisit: off is evaluated thrice
#define old_off2ptr(off, ptr) (typeof(ptr))		\
    ((!((off) & OFF_HOLDER_FLAG)) ? (off) :	\
     (sign_ext_off(off)) + (uintptr_t)&(off))
#else // !HATRACK_FABRIC
#define ptr2off(ptr, holder) (ptr)
#define off2ptr(off, holder, ptr) (off)
#endif
#endif
