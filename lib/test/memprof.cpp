#include <atomic>
#include <cstdlib>
#include <new>


static std::atomic<std::size_t> g_allocations = 0;

size_t alloc_count() {
    return g_allocations.load();
}


void* operator new(std::size_t size) {
    g_allocations++;
    if (void* p = std::malloc(size))
        return p;
    throw std::bad_alloc();
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void* operator new[](std::size_t size) {
    g_allocations++;
    if (void* p = std::malloc(size))
        return p;
    throw std::bad_alloc();
}

void operator delete[](void* ptr) noexcept {
    std::free(ptr);
}

