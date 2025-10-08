#pragma once

// Compile-time memory layout for emCore subsystems.
// Pure header-only: computes offsets from sizes in budget.hpp.

#include <cstddef>
#include "budget.hpp"

namespace emCore::memory {

struct region {
    std::size_t offset;
    std::size_t size;
};

constexpr std::size_t align_up(std::size_t v, std::size_t a) {
    return (v + (a - 1U)) & ~(a - 1U);
}

struct layout {
    region messaging;    // broker mailboxes + tables
    region events;       // event queue + handlers
    region tasks;        // user-reserved
    region os;           // user-reserved
    region protocol;     // user-reserved
    region diagnostics;  // user-reserved
    std::size_t total;   // total upper-bound including alignment padding

    static constexpr layout compute() {
        constexpr std::size_t A = 8; // 8-byte alignment for mixed types
        std::size_t off = 0;
        layout L{};
        off = align_up(off, A); L.messaging    = { off, messaging_total_upper };    off += messaging_total_upper;
        off = align_up(off, A); L.events       = { off, events_total_upper };       off += events_total_upper;
        off = align_up(off, A); L.tasks        = { off, tasks_total_upper };        off += tasks_total_upper;
        off = align_up(off, A); L.os           = { off, os_total_upper };           off += os_total_upper;
        off = align_up(off, A); L.protocol     = { off, protocol_total_upper };     off += protocol_total_upper;
        off = align_up(off, A); L.diagnostics  = { off, diagnostics_total_upper };  off += diagnostics_total_upper;
        L.total = align_up(off, A);
        return L;
    }
};

inline constexpr layout kLayout = layout::compute();
inline constexpr std::size_t required_bytes = kLayout.total;

#ifdef EMCORE_MEMORY_BUDGET_BYTES
static_assert(required_bytes <= static_cast<std::size_t>(EMCORE_MEMORY_BUDGET_BYTES),
              "emCore layout exceeds EMCORE_MEMORY_BUDGET_BYTES: raise budget or lower caps");
#endif

} // namespace emCore::memory
