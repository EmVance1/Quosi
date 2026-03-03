#ifndef CQUOSI_IMPL_VEC_H
#define CQUOSI_IMPL_VEC_H
#include "cquosi/quosi.h"


typedef struct quosiDynArrayHeader {
    size_t len;
    size_t cap;
} quosiDynArrayHeader;

#ifndef QUOSIDS_INIT_LEN
#define QUOSIDS_INIT_LEN 8
#endif

#ifndef QUOSIDS_ALLOCATOR
#define QUOSIDS_ALLOCATOR (quosi_malloc_allocator())
#endif


void* quosids_arrgrowf(void* ptr, size_t elemsize, size_t addlen, size_t min_cap, quosiAllocator alloc);


#define quosids_arrgrow(a,b)        ((a) = quosids_arrgrowf((a), sizeof *(a), (b), QUOSIDS_INIT_LEN, QUOSIDS_ALLOCATOR))

#define quosids_arrmaybegrow(a,n)   ((!(a) || quosids_header(a)->len + (n) > quosids_header(a)->cap) ? (quosids_arrgrow(a,n),0) : 0)

#define quosids_header(arr)         ((quosiDynArrayHeader*)(arr) - 1)
#define quosids_arraddn(arr, n)     (quosids_arrmaybegrow(arr,n), (n) ? (quosids_header(arr)->len += (n)) : (0))
#define quosids_arraddnptr(arr, n)  (quosids_arrmaybegrow(arr,n), (n) ? (quosids_header(arr)->len += (n), &(arr)[quosids_header(arr)->len-(n)]) : (arr))
#define quosids_arrpush(arr, val)   (quosids_arrmaybegrow(arr,1), (arr)[quosids_header(arr)->len++] = (val))
#define quosids_arrlenu(arr)        ((arr) ? quosids_header(arr)->len : 0)
#define quosids_arrlast(arr)        ((arr)[quosids_header(arr)->len-1])
#define quosids_arrfree(arr)        ((void) ((arr) ? quosi_allocator_deallocate(QUOSIDS_ALLOCATOR, quosids_header(arr)) : (void)0), (arr)=NULL)

#endif
