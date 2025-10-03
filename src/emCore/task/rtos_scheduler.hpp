#pragma once

#include "../core/types.hpp"
#include "../core/config.hpp"
#include "../core/strong_types.hpp"
#include "../platform/platform.hpp"

#include <etl/vector.h>
#include <cstddef>

namespace emCore::task {

// Only the strong types actually needed for RTOS scheduler - with automatic unique IDs!
using cpu_core_id = core::strong_type_generator<u8>::type;
using execution_time_us = core::strong_type_generator<duration_t>::type;
using deadline_us = core::strong_type_generator<duration_t>::type;

/**
 * @brief RTOS-specific task scheduling optimizations
 * Embedded systems focused improvements
 */
/**
 * @brief Task yield strategies for cooperative multitasking
 */
enum class yield_strategy : u8 {
    never,          // Never yield (real-time critical)
    periodic,       // Yield every N iterations
    on_idle,        // Yield when no work available
    adaptive        // Yield based on system load
};


/**
 * @brief Task execution context for RTOS optimization
 */
struct task_execution_context {
    // Stack monitoring
    size_t stack_size_bytes{0};
    size_t stack_used_bytes{0};
    size_t stack_high_water_mark{0};
    
    // CPU affinity (for multi-core MCUs like ESP32)
    u8 cpu_core_id{0};
    bool pin_to_core{false};
    
    // Scheduling behavior
    yield_strategy yield_behavior{yield_strategy::adaptive};
    u32 yield_interval{100};  // Iterations between yields
    
    // Real-time constraints
    duration_t max_execution_time_us{10000}; // 10ms max
    duration_t deadline_us{0}; // 0 = no deadline
    bool is_realtime{false};
    
    // Performance tracking
    u32 execution_count{0};
    duration_t total_execution_time_us{0};
    timestamp_t last_execution_start{0};
};

/**
 * @brief RTOS task scheduler with embedded optimizations
 */
class rtos_scheduler {
private:
    static constexpr size_t max_contexts = config::max_tasks;
    
    etl::vector<task_execution_context, max_contexts> contexts_;
    etl::vector<task_id_t, max_contexts> task_ids_;
    
    // System load tracking
    u32 total_cpu_time_us_{0};
    u32 idle_time_us_{0};
    [[maybe_unused]] timestamp_t last_load_calculation_{0};
    
    /**
     * @brief Find execution context for task
     */
    task_execution_context* find_context(task_id_t task_id) noexcept {
        for (size_t i = 0; i < task_ids_.size(); ++i) {
            if (task_ids_[i] == task_id) {
                return &contexts_[i];
            }
        }
        return nullptr;
    }
    
public:
    rtos_scheduler() noexcept = default;
    
    /**
     * @brief Register task for RTOS scheduling optimization
     */
    bool register_task(task_id_t task_id, const task_execution_context& context = {}) noexcept {
        if (task_ids_.full() || find_context(task_id) != nullptr) {
            return false;
        }
        
        task_ids_.push_back(task_id);
        contexts_.push_back(context);
        return true;
    }
    
    /**
     * @brief Set CPU affinity for multi-core MCUs (ESP32, etc.)
     * @param task_id Task identifier
     * @param core_id CPU core number (0 or 1 for ESP32)
     * @param pin_to_core Whether to pin task to specific core
     */
    void set_cpu_affinity(task_id_t task_id, cpu_core_id core_id, bool pin_to_core = true) noexcept {
        auto* context = find_context(task_id);
        if (context != nullptr) {
            context->cpu_core_id = core_id.value();
            context->pin_to_core = pin_to_core;
            
            // Note: CPU core pinning would need integration with taskmaster 
            // to get the actual task handle for platform-specific calls
            // This is stored for future use when taskmaster integration is added
        }
    }
    
    /**
     * @brief Set real-time constraints for critical tasks
     * @param task_id Task identifier
     * @param max_execution Maximum execution time in microseconds
     * @param deadline Deadline in microseconds (0 = no deadline)
     */
    void set_realtime_constraints(task_id_t task_id, execution_time_us max_execution, 
                                 deadline_us deadline = deadline_us(0)) noexcept {
        auto* context = find_context(task_id);
        if (context != nullptr) {
            context->max_execution_time_us = max_execution.value();
            context->deadline_us = deadline.value();
            context->is_realtime = true;
            context->yield_behavior = yield_strategy::never; // RT tasks don't yield
        }
    }
    
    /**
     * @brief Adaptive yield - call this in task loops
     */
    void adaptive_yield(task_id_t task_id) noexcept {
        auto* context = find_context(task_id);
        if (context == nullptr) {
            return;
        }
        
        context->execution_count++;
        
        // Check if we should yield based on strategy
        bool should_yield = false;
        
        switch (context->yield_behavior) {
            case yield_strategy::never:
                return; // Never yield (real-time tasks)
                
            case yield_strategy::periodic:
                should_yield = (context->execution_count % context->yield_interval) == 0;
                break;
                
            case yield_strategy::on_idle:
                // Yield if no messages waiting (would need message queue integration)
                should_yield = true; // Simplified for now
                break;
                
            case yield_strategy::adaptive:
                // Yield based on execution time and system load
                timestamp_t now = platform::get_system_time_us();
                if (context->last_execution_start > 0) {
                    duration_t execution_time = now - context->last_execution_start;
                    should_yield = execution_time > (context->max_execution_time_us / 2);
                }
                break;
        }
        
        if (should_yield) {
            platform::task_yield(); // Clean platform abstraction
        }
    }
    
    /**
     * @brief Start execution timing for a task
     */
    void start_execution_timing(task_id_t task_id) noexcept {
        auto* context = find_context(task_id);
        if (context != nullptr) {
            context->last_execution_start = platform::get_system_time_us();
        }
    }
    
    /**
     * @brief End execution timing and update statistics
     */
    void end_execution_timing(task_id_t task_id) noexcept {
        auto* context = find_context(task_id);
        if (context != nullptr && context->last_execution_start > 0) {
            timestamp_t now = platform::get_system_time_us();
            duration_t execution_time = now - context->last_execution_start;
            
            context->total_execution_time_us += execution_time;
            
            // Check for deadline violations
            if (context->deadline_us > 0 && execution_time > context->deadline_us) {
                // Log deadline miss (integrate with error system)
                platform::logf("DEADLINE MISS: Task %u took %u us (limit: %u us)",
                              static_cast<u32>(task_id.value()), execution_time, context->deadline_us);
            }
        }
    }
    
    /**
     * @brief Monitor stack usage (platform-specific)
     */
    void update_stack_usage(task_id_t task_id) noexcept {
        auto* context = find_context(task_id);
        if (context == nullptr) {
            return;
        }
        
        // Platform-agnostic stack monitoring
        size_t stack_free_bytes = platform::get_stack_high_water_mark();
        if (stack_free_bytes > 0 && context->stack_size_bytes > 0) {
            context->stack_used_bytes = context->stack_size_bytes - stack_free_bytes;
            context->stack_high_water_mark = context->stack_used_bytes;
            
            // Warn if stack usage > 80%
            if (context->stack_used_bytes > (context->stack_size_bytes * 80 / 100)) {
                platform::logf("STACK WARNING: Task %u using %u/%u bytes",
                              static_cast<u32>(task_id.value()),
                              static_cast<u32>(context->stack_used_bytes),
                              static_cast<u32>(context->stack_size_bytes));
            }
        }
    }
    
    /**
     * @brief Get task execution statistics
     */
    const task_execution_context* get_task_context(task_id_t task_id) const noexcept {
        for (size_t i = 0; i < task_ids_.size(); ++i) {
            if (task_ids_[i] == task_id) {
                return &contexts_[i];
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Calculate system CPU load
     */
    f32 get_cpu_load_percent() const noexcept {
        if (total_cpu_time_us_ == 0) {
            return 0.0F;
        }
        
        u32 total_time = total_cpu_time_us_ + idle_time_us_;
        return static_cast<f32>(total_cpu_time_us_ * 100) / static_cast<f32>(total_time);
    }
    
    /**
     * @brief Generate scheduler performance report
     */
    void generate_scheduler_report() const noexcept {
        platform::log("=== RTOS SCHEDULER REPORT ===");
        platform::logf("System CPU Load: %u%%", static_cast<u32>(get_cpu_load_percent()));
        
        for (size_t i = 0; i < task_ids_.size(); ++i) {
            const auto& context = contexts_[i];
            u32 avg_execution_us = context.execution_count > 0 ? 
                static_cast<u32>(context.total_execution_time_us / context.execution_count) : 0;
            
            platform::logf("Task %u: %u executions, avg %u us",
                          static_cast<u32>(task_ids_[i].value()),
                          context.execution_count,  
                          avg_execution_us);
        }
        platform::log("=== END SCHEDULER REPORT ===");
    }
};

/**
 * @brief Global RTOS scheduler instance
 */
inline rtos_scheduler& get_global_scheduler() noexcept {
    static rtos_scheduler scheduler;
    return scheduler;
}

} // namespace emCore::task
