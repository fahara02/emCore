#pragma once


#include "../core/config.hpp"
#include "event.hpp"
#include"event_types.hpp"
#include <etl/vector.h>
#include <etl/deque.h>
#include <etl/delegate.h>
#include <cstddef>
namespace emCore::events {

using handler_t = etl::delegate<void(const Event&)>;

struct handler_registration {
    id ident{};                // match category+code or cat=any for wildcard
    handler_t fn;
    bool active{false};
};

// Universal event bus (no RTTI/alloc). Copy/move disabled.
class event_bus {
private:
    static constexpr size_t max_handlers = config::max_event_handlers;
    static constexpr size_t queue_cap    = config::event_queue_size;

    etl::vector<handler_registration, max_handlers> handlers_;
    etl::deque<Event, queue_cap> queue_;
    bool initialized_{false};

public:
    event_bus() = default;
    event_bus(const event_bus&) = delete;
    event_bus& operator=(const event_bus&) = delete;
    event_bus(event_bus&&) = delete;
    event_bus& operator=(event_bus&&) = delete;
    ~event_bus()=default;

    bool initialize() noexcept { initialized_ = true; return true; }

    bool register_handler(id ident, handler_t hnd) noexcept {
        if (!initialized_ || handlers_.full()) { return false; }
        handler_registration reg; reg.ident = ident; reg.fn = hnd; reg.active = true;
        handlers_.push_back(reg);
        return true;
    }

    bool unregister_handler(id ident) noexcept {
        if (!initialized_) { return false; }
        for (auto& hnd : handlers_) {
            if (hnd.active && hnd.ident.cat == ident.cat && hnd.ident.code == ident.code) { hnd.active = false; return true; }
        }
        return false;
    }

    // Post event by value (Event is a small value type)
    bool post(const Event& evt) noexcept {
        if (!initialized_ || queue_.full()) { return false; }
        queue_.push_back(evt);
        return true;
    }

    // Convenience builders
    bool post(category cat, code_t code, severity lvl = severity::info, flags flg = flags::none) noexcept {
        Event evt = Event::make(cat, code, lvl, flg); evt.ts = 0; return post(evt);
    }

    // Process up to max events
    size_t process(size_t max_events = static_cast<size_t>(-1)) noexcept {
        if (!initialized_) { return 0; }
        size_t count = 0;
        while (!queue_.empty() && count < max_events) {
            Event evt = queue_.front();
            queue_.pop_front();
            dispatch(evt);
            ++count;
        }
        return count;
    }

    // Dispatch immediately to all matching handlers
    void dispatch(const Event& evt) noexcept {
        for (const auto& hnd : handlers_) {
            if (!hnd.active) { continue; }
            const bool cat_match = (hnd.ident.cat == category::any) || (hnd.ident.cat == evt.ident.cat);
            const bool code_match = (hnd.ident.code == 0xFFFFU) || (hnd.ident.code == evt.ident.code);
            if (cat_match && code_match) {
                hnd.fn(evt);
            }
        }
    }

    // Introspection helpers
    [[nodiscard]] size_t pending() const noexcept { return queue_.size(); }
    [[nodiscard]] size_t active_handlers() const noexcept {
        size_t cnt = 0;
        for (const auto& hnd : handlers_) { if (hnd.active) { ++cnt; } }
        return cnt;
    }
};

} // namespace emCore::events
