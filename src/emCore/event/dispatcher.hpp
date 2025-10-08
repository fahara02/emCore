#pragma once

#include "../core/types.hpp"
#include "../error/result.hpp"
#include "../platform/platform.hpp"
#include <cstddef>
#include <etl/delegate.h>
#include "event.hpp"
#include "event_runtime.hpp"
#include "event_types.hpp"

namespace emCore {
    
    /**
     * @brief Event ID type
     */
    using event_id_t = u16;
    constexpr event_id_t invalid_event_id = 0xFFFF;
    
    // Use the universal Event type from emCore::event, but keep legacy name 'event'
    using EventT = ::emCore::events::Event;
    using event  = EventT;
    // Event handler delegate signature (no dynamic allocation)
    using event_handler_t = etl::delegate<void(const event&)>;
    
    /**
     * @brief Event handler registration
     */
    struct event_handler_registration {
        event_id_t event_id{invalid_event_id};
        event_handler_t handler;
        priority priority_level{priority::normal};
        bool active{false};
        
        event_handler_registration() noexcept = default;
    };
    
    /**
     * @brief Event dispatcher - manages events without dynamic allocation
     */
    class event_dispatcher {
    private:
        bool initialized_{false};
        
        static timestamp_t get_current_time() noexcept { return platform::get_system_time(); }
        
    public:
        event_dispatcher() noexcept = default;
        event_dispatcher(const event_dispatcher&) = delete;
        event_dispatcher& operator=(const event_dispatcher&) = delete;
        event_dispatcher(event_dispatcher&&) = delete;
        event_dispatcher& operator=(event_dispatcher&&) = delete;
        ~event_dispatcher() = default;
        
        /**
         * @brief Initialize the event dispatcher
         * @return Result indicating success or failure
         */
        result<void, error_code> initialize() noexcept {
            initialized_ = true;
            return ok();
        }
        
        /**
         * @brief Register an event handler
         * @param event_id Event ID to handle
         * @param handler Handler function
         * @param prio Handler priority
         * @return Result indicating success or failure
         */
        result<void, error_code> register_handler(
            event_id_t event_id,
            event_handler_t handler,
            priority prio = priority::normal
        ) const noexcept {
            (void)prio; // priority not used in bus integration
            if (!initialized_) {
                return result<void, error_code>(error_code::not_initialized);
            }
            const bool ok_b = ::emCore::events::runtime::bus().register_handler(
                { ::emCore::events::category::user, static_cast<::emCore::events::code_t>(event_id) }, handler);
            return ok_b ? ok() : result<void, error_code>(error_code::out_of_memory);
        }
        
        /**
         * @brief Unregister an event handler
         * @param event_id Event ID
         * @return Result indicating success or failure
         */
        result<void, error_code> unregister_handler(event_id_t event_id) const noexcept {
            if (!initialized_) {
                return result<void, error_code>(error_code::not_initialized);
            }
            const bool ok_b = ::emCore::events::runtime::bus().unregister_handler(
                { ::emCore::events::category::user, static_cast<::emCore::events::code_t>(event_id) });
            return ok_b ? ok() : result<void, error_code>(error_code::not_found);
        }
        
        /**
         * @brief Post an event to the queue
         * @param evt Event to post
         * @return Result indicating success or failure
         */
        result<void, error_code> post_event(const event& evts) const noexcept {
            if (!initialized_) {
                return result<void, error_code>(error_code::not_initialized);
            }
            event evt = evts;
            if (evt.ts == 0) { evt.ts = get_current_time(); }
            const bool ok_b = ::emCore::events::runtime::bus().post(evt);
            return ok_b ? ok() : result<void, error_code>(error_code::out_of_memory);
        }
        
        /**
         * @brief Post an event with data
         * @tparam T Data type
         * @param event_id Event ID
         * @param data Event data
         * @return Result indicating success or failure
         */
        template<typename T>
        result<void, error_code> post_event(event_id_t event_id, const T& data) const noexcept {
            // Map numeric dispatcher ID to universal event category::user + code
            event evt = ::emCore::events::Event::make(::emCore::events::category::user,
                                                    static_cast<::emCore::events::code_t>(event_id));
            evt.ts = get_current_time();
            evt.data = data; // must match payload_t alternatives
            return post_event(evt);
        }
        
        /**
         * @brief Post an event without data
         * @param event_id Event ID
         * @return Result indicating success or failure
         */
        result<void, error_code> post_event(event_id_t event_id) const noexcept {
            EventT evt = ::emCore::events::Event::make(::emCore::events::category::user,
                                                    static_cast<::emCore::events::code_t>(event_id));
            evt.ts = get_current_time();
            return post_event(evt);
        }
        
        /**
         * @brief Process pending events (call this in main loop)
         * @param max_events Maximum number of events to process per call
         */
        void process_events(size_t max_events = 10) const noexcept {
            if (!initialized_) { return; }
            (void)::emCore::events::runtime::bus().process(max_events);
        }
        
        /**
         * @brief Get number of pending events
         * @return Number of events in queue
         */
        static size_t get_pending_event_count() noexcept { return ::emCore::events::runtime::bus().pending(); }
        
        /**
         * @brief Get number of registered handlers
         * @return Number of active handlers
         */
        static size_t get_handler_count() noexcept { return ::emCore::events::runtime::bus().active_handlers(); }
        
        /**
         * @brief Check if dispatcher is initialized
         * @return True if initialized
         */
        bool is_initialized() const noexcept {
            return initialized_;
        }
    };
    
} // namespace emCore
