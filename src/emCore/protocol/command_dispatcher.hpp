#pragma once

// emCore Command Dispatcher - header-only, no dynamic allocation
// - Fixed-capacity table for opcode -> handler
// - Replace-on-register semantics; supports deregistration
// - Minimal API suitable for ISR/task use (no locks)

#include <emCore/core/types.hpp>
#include <etl/array.h>
#include <cstddef>

namespace emCore::protocol {

// Handler signature alias for clarity
// PacketT is expected to be emCore::protocol::packet<...>
template <typename PacketT>
using command_handler_t = void (*)(const PacketT&);

// Professional-grade fixed-capacity dispatcher
// - MaxHandlers: compile-time capacity
// - PacketT: packet type (e.g., packet<MaxPayload>)
template <size_t MaxHandlers, typename PacketT>
class command_dispatcher {
public:
    using handler_t = command_handler_t<PacketT>;

    enum class reg_result : u8 { ok_new = 0, ok_replaced, full };

    static constexpr size_t capacity() noexcept { return MaxHandlers; }

    command_dispatcher() = default;

    // Register (or replace) a handler for an opcode (bool legacy API)
    bool register_handler(u8 opcode, handler_t fn) noexcept {
        return try_register_handler(opcode, fn) != reg_result::full;
    }

    // Register (or replace) with detailed result
    reg_result try_register_handler(u8 opcode, handler_t fn) noexcept {
        for (size_t i = 0; i < size_; ++i) {
            if (table_[i].used && table_[i].opcode == opcode) {
                table_[i].fn = fn;
                return reg_result::ok_replaced;
            }
        }
        if (size_ >= MaxHandlers) { return reg_result::full; }
        table_[size_] = entry{opcode, fn, true};
        ++size_;
        return reg_result::ok_new;
    }

    // Remove a handler for an opcode; returns true if removed
    bool deregister_handler(u8 opcode) noexcept {
        for (size_t i = 0; i < size_; ++i) {
            if (table_[i].used && table_[i].opcode == opcode) {
                // Compact by swapping last live into this slot
                table_[i] = table_[size_ - 1];
                table_[size_ - 1] = entry{};
                --size_;
                return true;
            }
        }
        return false;
    }

    // Check if a handler exists for an opcode
    [[nodiscard]]    bool has_handler(u8 opcode) const noexcept {
        for (size_t i = 0; i < size_; ++i) {
            if (table_[i].used && table_[i].opcode == opcode) { return true; }
        }
        return false;
    }

    // Get a handler pointer for an opcode (nullptr if not present)
    handler_t get_handler(u8 opcode) const noexcept {
        for (size_t i = 0; i < size_; ++i) {
            if (table_[i].used && table_[i].opcode == opcode) { return table_[i].fn; }
        }
        return nullptr;
    }

    // Set a default handler invoked when no opcode match is found
    void set_unknown_handler(handler_t fn) noexcept { unknown_ = fn; }

    // Remove all handlers
    void clear() noexcept {
        for (size_t i = 0; i < size_; ++i) { table_[i] = entry{}; }
        size_ = 0;
        unknown_ = nullptr;
    }

    // Number of registered handlers
    [[nodiscard]]   size_t size() const noexcept { return size_; }

    // Dispatch a packet to the matching handler or unknown handler
    void dispatch(const PacketT& pkt) const noexcept {
        for (size_t i = 0; i < size_; ++i) {
            if (table_[i].used && table_[i].opcode == pkt.opcode) {
                if (table_[i].fn) { table_[i].fn(pkt); }
                return;
            }
        }
        if (unknown_) { unknown_(pkt); }
    }

private:
    struct entry {
        u8 opcode{0};
        handler_t fn{nullptr};
        bool used{false};
    };

    etl::array<entry, MaxHandlers> table_{};
    size_t size_{0};
    handler_t unknown_{nullptr};
};

} // namespace emCore::protocol
