#pragma once

// Fixed-capacity byte ring buffer (SPSC-friendly)
// - Header-only, no dynamic allocation, no RTTI
// - OS-agnostic; caller ensures concurrency discipline
// - ETL only dependency (for array)

#include <etl/array.h>
#include <emCore/core/types.hpp>

namespace emCore::protocol {

template <size_t Capacity>
class byte_ring {
public:
    static_assert(Capacity > 0, "Capacity must be > 0");

    constexpr byte_ring() = default;

    // Reset buffer indices (not thread-safe by itself)
    inline void reset() noexcept { head_ = tail_ = 0; }

    // Push one byte. Returns false if ring is full (byte not stored).
    inline bool push(u8 b) noexcept {
        const size_t next = next_index(head_);
        if (next == tail_) { return false; } // full
        buf_[head_] = b;
        head_ = next;
        return true;
    }

    // Push many bytes; returns number stored
    inline size_t push_n(const u8* data, size_t len) noexcept {
        size_t stored = 0;
        while (stored < len && push(data[stored])) { ++stored; }
        return stored;
    }

    // Pop one byte into out. Returns false if empty.
    inline bool pop(u8& out) noexcept {
        if (empty()) { return false; }
        out = buf_[tail_];
        tail_ = next_index(tail_);
        return true;
    }

    // Pop up to max bytes into dst; returns count popped
    inline size_t pop_n(u8* dst, size_t max) noexcept {
        size_t count = 0;
        while (count < max) {
            if (!pop(dst[count])) { break; }
            ++count;
        }
        return count;
    }

    // State
    inline bool empty() const noexcept { return head_ == tail_; }

    inline bool full() const noexcept { return next_index(head_) == tail_; }

    inline size_t size() const noexcept {
        if (head_ >= tail_) { return head_ - tail_; }
        return Capacity - (tail_ - head_);
    }

    static constexpr size_t capacity() noexcept { return Capacity; }

private:
    static constexpr size_t next_index(size_t i) noexcept { return (i + 1U) % Capacity; }

    etl::array<u8, Capacity> buf_{};
    size_t head_{0};
    size_t tail_{0};
};

} // namespace emCore::protocol
