#include <cstddef>
#include <new>

// EASTL's debug allocator ABI delegates allocation policy to the application.
void* operator new[](std::size_t size, const char*, int, unsigned,
                     const char*, int) {
    return ::operator new[](size);
}
