#pragma once

#include <cstddef>
#include <type_traits>
#include <new>

namespace emCore::os {

// Aligned storage buffer helper (header-only, no heap)
template <std::size_t Size, std::size_t Align = alignof(std::max_align_t)>
struct storage_buffer {
    alignas(Align) unsigned char bytes[Size];

    void* data() noexcept { return static_cast<void*>(bytes); }
    const void* data() const noexcept { return static_cast<const void*>(bytes); }
};

// Placement construction helper (no dynamic allocation)
template <typename T, typename... Args>
T* place_construct(void* memory, Args&&... args) noexcept {
    return new (memory) T(static_cast<Args&&>(args)...);
}

// Explicit destructor call helper (no deallocation)
template <typename T>
void place_destroy(T* ptr) noexcept {
    if (ptr) {
        ptr->~T();
    }
}

} // namespace emCore::os
