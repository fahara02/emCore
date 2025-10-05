#pragma once

#include <cstddef>

#include "../core/types.hpp"
#include "../platform/platform.hpp"
#include "message_types.hpp"

#include <etl/array.h>

namespace emCore::messaging {

// Append-only event log with fixed capacity and replay
template <typename EventT = medium_message,
          size_t Capacity = 128,
          bool DropOldest = true>
class event_log {
public:
    struct stats { u64 appended; u32 dropped; u32 readers; size_t used; size_t capacity; };
    event_log() noexcept { reset(); }

    void reset() noexcept { cs_ = platform::critical_section(); head_ = 0; tail_ = 0; size_ = 0; next_index_ = 1; appended_ = 0; dropped_ = 0; }

    u64 append(const EventT& evt) noexcept {
        cs_.enter();
        if (size_ == Capacity) {
            if constexpr (DropOldest) { head_ = (head_ + 1) % Capacity; --size_; ++dropped_; }
            else { cs_.exit(); return 0; }
        }
        const size_t idx = tail_;
        buffer_[idx] = evt;
        indices_[idx] = next_index_;
        tail_ = (tail_ + 1) % Capacity; ++size_;
        const u64 assigned = next_index_++;
        ++appended_;
        cs_.exit();
        return assigned;
    }

    template <typename Fn>
    void replay_all(Fn&& fn) const noexcept {
        cs_.enter(); size_t count = size_; size_t pos = head_;
        while (count--) { const EventT& e = buffer_[pos]; const u64 idx = indices_[pos]; cs_.exit(); fn(idx, e); cs_.enter(); pos = (pos + 1) % Capacity; }
        cs_.exit();
    }

    template <typename Fn>
    void replay_from(u64 from_index, Fn&& fn) const noexcept {
        cs_.enter(); size_t count = size_; size_t pos = head_;
        while (count--) { if (indices_[pos] >= from_index) { break; } pos = (pos + 1) % Capacity; }
        size_t remaining = count + 1;
        while (remaining--) { const EventT& e = buffer_[pos]; const u64 idx = indices_[pos]; cs_.exit(); fn(idx, e); cs_.enter(); pos = (pos + 1) % Capacity; }
        cs_.exit();
    }

    [[nodiscard]] stats get_stats() const noexcept { cs_.enter(); stats s{appended_, dropped_, 0, size_, Capacity}; cs_.exit(); return s; }

private:
    mutable platform::critical_section cs_;
    etl::array<EventT, Capacity> buffer_{};
    etl::array<u64, Capacity> indices_{};
    size_t head_{0}; size_t tail_{0}; size_t size_{0};
    u64 next_index_{1}; u64 appended_{0}; u32 dropped_{0};
};

} // namespace emCore::messaging
