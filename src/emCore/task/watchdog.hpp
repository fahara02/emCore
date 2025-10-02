#pragma once

#include "../core/types.hpp"
#include "../core/config.hpp"
#include "../core/strong_types.hpp"
#include "../platform/platform.hpp"
#include "../error/result.hpp"
#include "../error/error_handler.hpp"

#include <etl/vector.h>

namespace emCore {

// Only the strong types actually needed for watchdog - with automatic unique ID!
using watchdog_timeout_ms = core::strong_type_generator<duration_t>::type;

/**
{{ ... }}
 * @brief Watchdog recovery action type
 */
enum class watchdog_action : u8 {
    none,           // No action
    log_warning,    // Just log a warning
    reset_task,     // Reset/restart the task
    system_reset    // Reset entire system
};

/**
 * @brief Recovery callback function
 */
using recovery_fn = void(*)(task_id_t task_id) noexcept;


/**
 * @brief Watchdog entry for a single task
 */
struct watchdog_entry {
    task_id_t task_id{invalid_task_id};
    volatile timestamp_t last_feed_time{0};  // Volatile to prevent compiler optimization races
    duration_t timeout_ms{5000};
    watchdog_action action{watchdog_action::log_warning};
    recovery_fn recovery_callback{nullptr};
    u32 timeout_count{0};
    bool enabled{false};
    
    watchdog_entry() noexcept = default;
};

/**
 * @brief Task watchdog monitor
 * Monitors task health and triggers recovery actions
 */
class task_watchdog {
private:
    etl::vector<watchdog_entry, config::max_tasks> entries_;
    bool system_watchdog_enabled_{false};
    duration_t system_timeout_ms_{10000};
    timestamp_t last_system_feed_{0};
    
    /**
     * @brief Find watchdog entry for task
     */
    watchdog_entry* find_entry(task_id_t task_id) noexcept {
        for (auto& entry : entries_) {
            if (entry.task_id == task_id && entry.enabled) {
                return &entry;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Trigger watchdog timeout action
     */
    static void trigger_timeout(watchdog_entry& entry) noexcept {
        entry.timeout_count++;
        
        // Report error
        auto ctx = error::error_handler::make_context(
            error::error_event::watchdog_timeout,
            error::error_severity::critical,
            entry.task_id
        );
        ctx.data[0] = entry.timeout_count;
        ctx.data[1] = entry.timeout_ms;
        error::report_error(ctx);
        
        // Execute recovery action
        switch (entry.action) {
            case watchdog_action::none:
                break;
                
            case watchdog_action::log_warning:
                platform::logf("WATCHDOG: Task %u timeout (%u occurrences)",
                              entry.task_id, entry.timeout_count);
                break;
                
            case watchdog_action::reset_task:
                platform::logf("WATCHDOG: Resetting task %u", entry.task_id);
                if (entry.recovery_callback != nullptr) {
                    entry.recovery_callback(entry.task_id);
                }
                break;
                
            case watchdog_action::system_reset:
                platform::log("WATCHDOG: SYSTEM RESET TRIGGERED!");
                // Trigger system reset via platform abstraction
                platform::delay_ms(100); // Allow log to flush
                platform::system_reset(); // Clean platform abstraction
                break;
        }
    }
    
public:
    task_watchdog() noexcept = default;
    
    /**
     * @brief Register a task with watchdog
     * @param task_id Task identifier
     * @param timeout Watchdog timeout in milliseconds
     * @param action Action to take on timeout
     */
    result<void, error_code> register_task(
        task_id_t task_id,
        watchdog_timeout_ms timeout,
        watchdog_action action = watchdog_action::log_warning
    ) noexcept {
        if (entries_.full()) {
            return result<void, error_code>(error_code::out_of_memory);
        }
        
        watchdog_entry entry;
        entry.task_id = task_id;
        entry.timeout_ms = timeout.value();
        entry.action = action;
        entry.last_feed_time = platform::get_system_time_us();
        entry.enabled = true;
        
        entries_.push_back(entry);
        return ok();
    }
    
    /**
     * @brief Feed the watchdog for a task (task is alive)
     */
    void feed(task_id_t task_id) noexcept {
        auto* entry = find_entry(task_id);
        if (entry != nullptr) {
            // Use volatile write to prevent compiler reordering
            timestamp_t now = platform::get_system_time_us();
            entry->last_feed_time = now;
        }
    }
    
    /**
     * @brief Set timeout for a task
     * @param task_id Task identifier
     * @param timeout New watchdog timeout in milliseconds
     */
    result<void, error_code> set_timeout(task_id_t task_id, watchdog_timeout_ms timeout) noexcept {
        auto* entry = find_entry(task_id);
        if (entry == nullptr) {
            return result<void, error_code>(error_code::not_found);
        }
        
        entry->timeout_ms = timeout.value();
        return ok();
    }
    
    /**
     * @brief Set recovery action for a task
     */
    result<void, error_code> set_action(task_id_t task_id, watchdog_action action) noexcept {
        auto* entry = find_entry(task_id);
        if (entry == nullptr) {
            return result<void, error_code>(error_code::not_found);
        }
        
        entry->action = action;
        return ok();
    }
    
    /**
     * @brief Register recovery callback
     */
    result<void, error_code> register_recovery_action(task_id_t task_id, recovery_fn callback) noexcept {
        auto* entry = find_entry(task_id);
        if (entry == nullptr) {
            return result<void, error_code>(error_code::not_found);
        }
        
        entry->recovery_callback = callback;
        return ok();
    }
    
    /**
     * @brief Check if task is alive (within timeout)
     */
    bool is_alive(task_id_t task_id) const noexcept {
        for (const auto& entry : entries_) {
            if (entry.task_id == task_id && entry.enabled) {
                timestamp_t now = platform::get_system_time_us();
                timestamp_t elapsed_us = now - entry.last_feed_time;
                return (elapsed_us / 1000) < entry.timeout_ms;
            }
        }
        return false;
    }
    
    /**
     * @brief Check all watchdogs and trigger timeouts
     * Should be called periodically from a dedicated watchdog task
     */
    void check_all() noexcept {
        timestamp_t now = platform::get_system_time_us();
        
        for (auto& entry : entries_) {
            if (!entry.enabled) {
                continue;
            }
            
            // Volatile read to get consistent timestamp
            timestamp_t last_feed = entry.last_feed_time;
            timestamp_t elapsed_us = now - last_feed;
            duration_t elapsed_ms = static_cast<duration_t>(elapsed_us / 1000);
            
            if (elapsed_ms >= entry.timeout_ms) {
                trigger_timeout(entry);
                // Reset timer after triggering
                entry.last_feed_time = now;
            }
        }
        
        // Check system watchdog
        if (system_watchdog_enabled_) {
            timestamp_t system_elapsed_us = now - last_system_feed_;
            duration_t system_elapsed_ms = static_cast<duration_t>(system_elapsed_us / 1000);
            
            if (system_elapsed_ms >= system_timeout_ms_) {
                platform::log("SYSTEM WATCHDOG TIMEOUT!");
                platform::delay_ms(100);
                // Trigger system reset via platform abstraction
                platform::system_reset();
            }
        }
    }
    

    void enable_task(task_id_t task_id, bool enable) noexcept {
        auto* entry = find_entry(task_id);
        if (entry != nullptr) {
            entry->enabled = enable;
            if (enable) {
                entry->last_feed_time = platform::get_system_time_us();
            }
        }
    }
    
    /**
     */
    void enable_system_watchdog(duration_t timeout_ms) noexcept {
        system_watchdog_enabled_ = true;
        system_timeout_ms_ = timeout_ms;
        last_system_feed_ = platform::get_system_time_us();
        
        platform::logf("System watchdog enabled: %u ms timeout", timeout_ms);
    }
    
    /**
     * @brief Feed system watchdog
     */
    void feed_system() noexcept {
        last_system_feed_ = platform::get_system_time_us();
    }
    
    /**
     * @brief Get timeout count for task
     */
    u32 get_timeout_count(task_id_t task_id) const noexcept {
        for (const auto& entry : entries_) {
            if (entry.task_id == task_id && entry.enabled) {
                return entry.timeout_count;
            }
        }
        return 0;
    }
    
    /**
     * @brief Reset all statistics
     */
    void reset_statistics() noexcept {
        for (auto& entry : entries_) {
            entry.timeout_count = 0;
        }
    }
};

/**
 * @brief Global watchdog instance
 */
inline task_watchdog& get_global_watchdog() noexcept {
    static task_watchdog watchdog;
    return watchdog;
}

} // namespace emCore
