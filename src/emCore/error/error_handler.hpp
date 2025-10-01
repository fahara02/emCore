#pragma once

#include "../core/types.hpp"
#include "../platform/platform.hpp"
#include "result.hpp"

namespace emCore {
namespace error {

/**
 * @brief Error event types for callbacks
 */
enum class error_event : u8 {
    message_dropped,
    queue_overflow,
    task_deadline_miss,
    task_fault,
    task_timeout,
    task_stack_overflow,
    memory_exhaustion,
    invalid_state,
    watchdog_timeout
};

/**
 * @brief Error severity levels
 */
enum class error_severity : u8 {
    info,       // Informational, no action needed
    warning,    // Warning, may need attention
    error,      // Error, requires handling
    critical,   // Critical, system may be unstable
    fatal       // Fatal, system must restart
};

/**
 * @brief Error context information
 */
struct error_context {
    error_event event{error_event::invalid_state};
    error_severity severity{error_severity::error};
    error_code code{error_code::invalid_parameter};
    task_id_t task_id{invalid_task_id};
    timestamp_t timestamp{0};
    u32 data[4]{0, 0, 0, 0};  // Event-specific data
    
    error_context() noexcept = default;
};

/**
 * @brief Error handler callback type
 */
using error_handler_fn = void(*)(const error_context& ctx) noexcept;

/**
 * @brief Retry policy configuration
 */
struct retry_policy {
    u8 max_retries{3};
    duration_t initial_delay_ms{100};
    duration_t max_delay_ms{5000};
    bool exponential_backoff{true};
    f32 backoff_multiplier{2.0F};
    
    /**
     * @brief Calculate delay for given retry attempt
     */
    duration_t get_delay(u8 attempt) const noexcept {
        if (attempt >= max_retries) {
            return 0;
        }
        
        if (!exponential_backoff) {
            return initial_delay_ms;
        }
        
        // Exponential backoff: delay = initial * (multiplier ^ attempt)
        duration_t delay = initial_delay_ms;
        for (u8 i = 0; i < attempt; ++i) {
            delay = static_cast<duration_t>(static_cast<f32>(delay) * backoff_multiplier);
            if (delay > max_delay_ms) {
                delay = max_delay_ms;
                break;
            }
        }
        return delay;
    }
};

/**
 * @brief Global error handler configuration
 */
class error_handler {
private:
    error_handler_fn callback_{nullptr};
    retry_policy retry_policy_;
    bool enabled_{false};
    u32 error_count_{0};
    error_context last_error_;
    
public:
    error_handler() noexcept = default;
    
    /**
     * @brief Set error handler callback
     */
    void set_callback(error_handler_fn callback) noexcept {
        callback_ = callback;
        enabled_ = (callback != nullptr);
    }
    
    /**
     * @brief Configure retry policy
     */
    void set_retry_policy(const retry_policy& policy) noexcept {
        retry_policy_ = policy;
    }
    
    /**
     * @brief Get current retry policy
     */
    const retry_policy& get_retry_policy() const noexcept {
        return retry_policy_;
    }
    
    /**
     * @brief Report an error
     */
    void report_error(const error_context& ctx) noexcept {
        error_count_++;
        last_error_ = ctx;
        
        if (enabled_ && callback_ != nullptr) {
            callback_(ctx);
        }
        
        // Log critical and fatal errors
        if (ctx.severity >= error_severity::critical) {
            platform::logf("CRITICAL ERROR: event=%u task=%u code=%u",
                          static_cast<u32>(ctx.event),
                          static_cast<u32>(ctx.task_id),
                          static_cast<u32>(ctx.code));
        }
    }
    
    /**
     * @brief Create error context helper
     */
    static error_context make_context(
        error_event event,
        error_severity severity,
        task_id_t task_id,
        error_code code = error_code::success
    ) noexcept {
        error_context ctx;
        ctx.event = event;
        ctx.severity = severity;
        ctx.task_id = task_id;
        ctx.code = code;
        ctx.timestamp = platform::get_system_time_us();
        return ctx;
    }
    
    /**
     * @brief Get total error count
     */
    u32 get_error_count() const noexcept {
        return error_count_;
    }
    
    /**
     * @brief Get last error context
     */
    const error_context& get_last_error() const noexcept {
        return last_error_;
    }
    
    /**
     * @brief Reset error statistics
     */
    void reset() noexcept {
        error_count_ = 0;
    }
};

/**
 * @brief Global error handler instance
 */
inline error_handler& get_global_error_handler() noexcept {
    static error_handler handler;
    return handler;
}

/**
 * @brief Convenience function to report errors
 */
inline void report_error(const error_context& ctx) noexcept {
    get_global_error_handler().report_error(ctx);
}

} // namespace error
} // namespace emCore
