#include "vec.h"

#define max(a, b) (((a) > (b)) ? (a) : (b))

/*
void* quosids_arrgrowf(void* ptr, size_t elemsize, size_t addlen, size_t min_cap, quosiAllocator alloc) {
    if (ptr == NULL) {
        const size_t newcap = max(addlen, min_cap);
        quosiDynArrayHeader* result = quosi_allocator_allocate(alloc, elemsize * newcap);
        result->cap = newcap;
        result->len = addlen;
        return result + 1;
    } else {
        quosiDynArrayHeader* header = quosids_header(ptr);
        const size_t dobcap = header->cap * 2;
        const size_t newlen = header->len + addlen;
        const size_t newcap = max(newlen, dobcap);
        quosiDynArrayHeader* result = quosi_allocator_reallocate(alloc, header, elemsize * newcap);
        result->cap = newcap;
        result->len = newlen;
        return result + 1;
    }
}
*/

void* quosids_arrgrowf(void* a, size_t elemsize, size_t addlen, size_t min_cap, quosiAllocator alloc) {
    static const quosiDynArrayHeader empty = { 0 };
    const quosiDynArrayHeader* header = a ? quosids_header(a) : &empty;
    const size_t min_len = header->len + addlen;

    if (min_len > min_cap) {
        min_cap = min_len;
    }
    if (min_cap <= header->cap) {
        return a;
    }

    if (min_cap < 2 * header->cap) {
        min_cap = 2 * header->cap;
    } else if (min_cap < QUOSIDS_INIT_LEN) {
        min_cap = QUOSIDS_INIT_LEN;
    }

    quosiDynArrayHeader* b;
    if (a == NULL) {
        b = quosi_allocator_allocate(alloc, elemsize * min_cap + sizeof(quosiDynArrayHeader));
        b->cap = min_cap;
        b->len = 0;
    } else {
        const size_t oldcap = elemsize * header->cap + sizeof(quosiDynArrayHeader);
        b = quosi_allocator_reallocate(alloc, quosids_header(a), oldcap, elemsize * min_cap + sizeof(quosiDynArrayHeader));
        b->cap = min_cap;
    }
    return b + 1;
}

