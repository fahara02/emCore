#pragma once

#include "../core/types.hpp"
#include "../core/config.hpp"
#include <etl/vector.h>
#include <etl/deque.h>
#include <etl/queue.h>
#include <etl/function.h>
#include <etl/variant.h>
#include <etl/array.h>

namespace emCore {
    
    /**
     * @brief Event ID type
     */
    using event_id_t = u16;
    constexpr event_id_t invalid_event_id = 0xFFFF;
    
    /**
     * @brief Event data variant (supports common embedded data types)
     */
    using event_data_t = etl::variant<
        etl::monostate,  // No data
        i32,             // Integer data
        f32,             // Float data
        string32,        // String data
        etl::array<u8, 16> // Binary data
    >;
    
    /**
     * @brief Event structure
     */
    struct event {
        event_id_t id;
        timestamp_t timestamp;
        event_data_t data;
        
        event() noexcept : id(invalid_event_id), timestamp(0) {}
        
        event(event_id_t event_id, timestamp_t time = 0) noexcept 
            : id(event_id), timestamp(time) {}
        
        template<typename T>
        event(event_id_t event_id, const T& event_data, timestamp_t time = 0) noexcept
            : id(event_id), timestamp(time), data(event_data) {}
    };
    
    /**
     * @brief Event handler function signature
     */
    using event_handler_t = etl::function<void(const event&), 32>;
    
    /**
     * @brief Event handler registration
     */
    struct event_handler_registration {
        event_id_t event_id;
        event_handler_t handler;
        priority priority_level;
        bool active;
        
        event_handler_registration() noexcept 
            : event_id(invalid_event_id)
            , priority_level(priority::normal)
            , active(false) {}
    };
    
    /**
     * @brief Event dispatcher - manages events without dynamic allocation
     */
    class event_dispatcher {
    private:
        etl::vector<event_handler_registration, config::max_event_handlers> handlers_;
        using event_queue_container_t = etl::deque<event, config::event_queue_size>;
        etl::queue<event, event_queue_container_t> event_queue_;
        bool initialized_;
        
        timestamp_t get_current_time() const noexcept;
        
    public:
        event_dispatcher() noexcept : initialized_(false) {}
        
        /**
         * @brief Initialize the event dispatcher
         * @return Result indicating success or failure
         */
        result<void, error_code> initialize() noexcept {
            initialized_ = true;
            return {};
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
        ) noexcept {
            if (!initialized_) {
                return error_code::not_initialized;
            }
            
            if (handlers_.full()) {
                return error_code::out_of_memory;
            }
            
            event_handler_registration registration;
            registration.event_id = event_id;
            registration.handler = handler;
            registration.priority_level = prio;
            registration.active = true;
            
            handlers_.push_back(registration);
            return {};
        }
        
        /**
         * @brief Unregister an event handler
         * @param event_id Event ID
         * @return Result indicating success or failure
         */
        result<void, error_code> unregister_handler(event_id_t event_id) noexcept {
            if (!initialized_) {
                return error_code::not_initialized;
            }
            
            for (auto& handler : handlers_) {
                if (handler.event_id == event_id && handler.active) {
                    handler.active = false;
                    return {};
                }
            }
            
            return error_code::not_found;
        }
        
        /**
         * @brief Post an event to the queue
         * @param evt Event to post
         * @return Result indicating success or failure
         */
        result<void, error_code> post_event(const event& evt) noexcept {
            if (!initialized_) {
                return error_code::not_initialized;
            }
            
            if (event_queue_.full()) {
                return error_code::out_of_memory;
            }
            
            event timestamped_event = evt;
            if (timestamped_event.timestamp == 0) {
                timestamped_event.timestamp = get_current_time();
            }
            
            event_queue_.push(timestamped_event);
            return {};
        }
        
        /**
         * @brief Post an event with data
         * @tparam T Data type
         * @param event_id Event ID
         * @param data Event data
         * @return Result indicating success or failure
         */
        template<typename T>
        result<void, error_code> post_event(event_id_t event_id, const T& data) noexcept {
            return post_event(event(event_id, data, get_current_time()));
        }
        
        /**
         * @brief Post an event without data
         * @param event_id Event ID
         * @return Result indicating success or failure
         */
        result<void, error_code> post_event(event_id_t event_id) noexcept {
            return post_event(event(event_id, get_current_time()));
        }
        
        /**
         * @brief Process pending events (call this in main loop)
         * @param max_events Maximum number of events to process per call
         */
        void process_events(size_t max_events = 10) noexcept {
            if (!initialized_) {
                return;
            }
            
            size_t processed = 0;
            while (!event_queue_.empty() && processed < max_events) {
                event current_event = event_queue_.front();
                event_queue_.pop();
                
                // Find and call handlers for this event
                for (const auto& handler : handlers_) {
                    if (handler.active && handler.event_id == current_event.id) {
                        handler.handler(current_event);
                    }
                }
                
                ++processed;
            }
        }
        
        /**
         * @brief Get number of pending events
         * @return Number of events in queue
         */
        size_t get_pending_event_count() const noexcept {
            return event_queue_.size();
        }
        
        /**
         * @brief Get number of registered handlers
         * @return Number of active handlers
         */
        size_t get_handler_count() const noexcept {
            size_t count = 0;
            for (const auto& handler : handlers_) {
                if (handler.active) {
                    ++count;
                }
            }
            return count;
        }
        
        /**
         * @brief Check if dispatcher is initialized
         * @return True if initialized
         */
        bool is_initialized() const noexcept {
            return initialized_;
        }
    };
    
} // namespace emCore
