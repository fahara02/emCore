#pragma once

#include "../core/types.hpp"
#include "../core/config.hpp"
#include "../platform/platform.hpp"
#include <etl/vector.h>
#include <etl/circular_buffer.h>
#include <cstddef>

 #include <etl/algorithm.h>


namespace emCore::diagnostics {

/**
 * @brief Task performance metrics
 */
struct task_performance_metrics {
    // Timing statistics (microseconds)
    duration_t min_execution_time_us{0xFFFFFFFF};
    duration_t max_execution_time_us{0};
    duration_t avg_execution_time_us{0};
    duration_t total_execution_time_us{0};
    
    // Latency statistics (microseconds)
    duration_t min_latency_us{0xFFFFFFFF};
    duration_t max_latency_us{0};
    duration_t avg_latency_us{0};
    
    // Execution counts
    u32 execution_count{0};
    u32 message_count{0};
    u32 error_count{0};
    
    // CPU utilization (percentage * 100 for integer math)
    u32 cpu_usage_percent_x100{0};
    
    // Memory usage (if available)
    size_t stack_usage_bytes{0};
    size_t peak_stack_usage_bytes{0};
    
    // Last update timestamp
    timestamp_t last_update_time{0};
    
    /**
     * @brief Update execution time statistics
     */
    void update_execution_time(duration_t execution_time_us) noexcept {
        execution_count++;
        total_execution_time_us += execution_time_us;
        
        min_execution_time_us = etl::min(min_execution_time_us, execution_time_us);
        max_execution_time_us = etl::max(max_execution_time_us, execution_time_us);
        
        avg_execution_time_us = total_execution_time_us / execution_count;
        last_update_time = platform::get_system_time_us();
    }
    
    /**
     * @brief Update latency statistics
     */
    void update_latency(duration_t latency_us) noexcept {
        message_count++;
        
        min_latency_us = etl::min(min_latency_us, latency_us);
        max_latency_us = etl::max(max_latency_us, latency_us);
        
        // Simple moving average
        if (avg_latency_us == 0) {
            avg_latency_us = latency_us;
        } else {
            avg_latency_us = (avg_latency_us * 7 + latency_us) / 8; // 7/8 weight to previous
        }
    }
    
    /**
     * @brief Update CPU usage
     */
    void update_cpu_usage(u32 usage_percent_x100) noexcept {
        cpu_usage_percent_x100 = usage_percent_x100;
    }
    
    /**
     * @brief Reset all statistics
     */
    void reset() noexcept {
        min_execution_time_us = 0xFFFFFFFF;
        max_execution_time_us = 0;
        avg_execution_time_us = 0;
        total_execution_time_us = 0;
        min_latency_us = 0xFFFFFFFF;
        max_latency_us = 0;
        avg_latency_us = 0;
        execution_count = 0;
        message_count = 0;
        error_count = 0;
        cpu_usage_percent_x100 = 0;
        stack_usage_bytes = 0;
        peak_stack_usage_bytes = 0;
    }
};

/**
 * @brief System-wide performance metrics
 */
struct system_performance_metrics {
    // Overall system stats
    u32 total_messages_sent{0};
    u32 total_messages_received{0};
    u32 total_messages_dropped{0};
    u32 total_errors{0};
    
    // Memory statistics
    size_t total_heap_usage{0};
    size_t peak_heap_usage{0};
    size_t free_heap_bytes{0};
    
    // System timing
    timestamp_t system_uptime_us{0};
    timestamp_t last_update_time{0};
    
    // Task switching
    u32 context_switches{0};
    u32 context_switch_rate{0}; // per second
    
    void update_uptime() noexcept {
        timestamp_t now = platform::get_system_time_us();
        system_uptime_us = now;
        last_update_time = now;
    }
};

/**
 * @brief Performance trace entry
 */
struct trace_entry {
    task_id_t task_id{invalid_task_id};
    timestamp_t timestamp{0};
    duration_t duration_us{0};
    u16 event_type{0}; // Custom event types
    u16 data{0};       // Event-specific data
};

/**
 * @brief Performance profiler
 */
class performance_profiler {
private:
    static constexpr size_t max_tasks = config::max_tasks;
    static constexpr size_t trace_buffer_size = 128;
    
    // Per-task metrics
    etl::vector<task_performance_metrics, max_tasks> task_metrics_;
    etl::vector<task_id_t, max_tasks> task_ids_;
    
    // System metrics
    system_performance_metrics system_metrics_;
    
    // Trace buffer (circular)
    etl::circular_buffer<trace_entry, trace_buffer_size> trace_buffer_;
    
    // Profiling state
    bool profiling_enabled_{false};
    bool tracing_enabled_{false};
    timestamp_t profiling_start_time_{0};
    
    /**
     * @brief Find task metrics by ID
     */
    task_performance_metrics* find_task_metrics(task_id_t task_id) noexcept {
        for (size_t i = 0; i < task_ids_.size(); ++i) {
            if (task_ids_[i] == task_id) {
                return &task_metrics_[i];
            }
        }
        return nullptr;
    }
    
public:
    performance_profiler() noexcept = default;
    
    /**
     * @brief Enable/disable profiling
     */
    void enable_profiling(bool enable = true) noexcept {
        profiling_enabled_ = enable;
        if (enable) {
            profiling_start_time_ = platform::get_system_time_us();
            system_metrics_.update_uptime();
        }
    }
    
    /**
     * @brief Enable/disable tracing
     */
    void enable_tracing(bool enable = true) noexcept {
        tracing_enabled_ = enable;
        if (enable && trace_buffer_.empty()) {
            // Clear trace buffer when starting
            trace_buffer_.clear();
        }
    }
    
    /**
     * @brief Register a task for profiling
     */
    bool register_task(task_id_t task_id) noexcept {
        if (task_ids_.full() || find_task_metrics(task_id) != nullptr) {
            return false;
        }
        
        task_ids_.push_back(task_id);
        task_metrics_.emplace_back();
        return true;
    }
    
    /**
     * @brief Record task execution time
     */
    void record_execution_time(task_id_t task_id, duration_t execution_time_us) noexcept { 
        if (!profiling_enabled_) {
            return;
        }
        
        auto* metrics = find_task_metrics(task_id);
        if (metrics != nullptr) {
            metrics->update_execution_time(execution_time_us);
        }
        
        // Add to trace if enabled
        if (tracing_enabled_ && !trace_buffer_.full()) {
            trace_entry entry;
            entry.task_id = task_id;
            entry.timestamp = platform::get_system_time_us();
            entry.duration_us = execution_time_us;
            entry.event_type = 1; // Execution event
            trace_buffer_.push(entry);
        }
    }
    
    /**
     * @brief Record message latency
     */
    void record_message_latency(task_id_t task_id, duration_t latency_us) noexcept { 
        if (!profiling_enabled_) {
            return;
        }
        
        auto* metrics = find_task_metrics(task_id);
        if (metrics != nullptr) {
            metrics->update_latency(latency_us);
        }
        
        system_metrics_.total_messages_received++;
        
        // Add to trace if enabled
        if (tracing_enabled_ && !trace_buffer_.full()) {
            trace_entry entry;
            entry.task_id = task_id;
            entry.timestamp = platform::get_system_time_us();
            entry.duration_us = latency_us;
            entry.event_type = 2; // Message latency event
            trace_buffer_.push(entry);
        }
    }
    
    /**
     * @brief Record error
     */
    void record_error(task_id_t task_id) noexcept {
        if (!profiling_enabled_) {
            return;
        }
        
        auto* metrics = find_task_metrics(task_id);
        if (metrics != nullptr) {
            metrics->error_count++;
        }
        
        system_metrics_.total_errors++;
    }
    
    /**
     * @brief Get task metrics
     */
    const task_performance_metrics* get_task_metrics(task_id_t task_id) const noexcept {
        for (size_t i = 0; i < task_ids_.size(); ++i) {
            if (task_ids_[i] == task_id) {
                return &task_metrics_[i];
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Get system metrics
     */
    [[nodiscard]] const system_performance_metrics& get_system_metrics() const noexcept {
        return system_metrics_;
    }
    
    /**
     * @brief Get trace buffer
     */
    [[nodiscard]] const etl::circular_buffer<trace_entry, trace_buffer_size>& get_trace_buffer() const noexcept {
        return trace_buffer_;
    }
    
    /**
     * @brief Update system statistics
     */
    void update_system_stats() noexcept {
        if (!profiling_enabled_) {
            return;
        }
        
        system_metrics_.update_uptime();
        
        // Calculate system-wide CPU usage
        u32 total_cpu_usage = 0;//NOLINT
        u32 active_tasks = 0;//NOLINT
        
        for (const auto& metrics : task_metrics_) {
            if (metrics.execution_count > 0) {
                total_cpu_usage += metrics.cpu_usage_percent_x100;
                active_tasks++;
            }
        }
        
        // Update memory stats if platform supports it
        #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
            system_metrics_.free_heap_bytes = esp_get_free_heap_size();
            system_metrics_.total_heap_usage = esp_get_minimum_free_heap_size();
        #endif
    }
    
    /**
     * @brief Reset all statistics
     */
    void reset_statistics() noexcept {
        for (auto& metrics : task_metrics_) {
            metrics.reset();
        }
        
        system_metrics_ = system_performance_metrics{};
        trace_buffer_.clear();
        
        if (profiling_enabled_) {
            profiling_start_time_ = platform::get_system_time_us();
            system_metrics_.update_uptime();
        }
    }
    
    /**
     * @brief Generate performance report
     */
    void generate_report() const noexcept {
        if (!profiling_enabled_) {
            platform::log("Profiling is disabled");
            return;
        }
        
        platform::log("=== PERFORMANCE REPORT ===");
        
        // System overview
        platform::logf("System uptime: %u ms", 
                      static_cast<u32>(system_metrics_.system_uptime_us / 1000));
        platform::logf("Total messages: %u sent, %u received, %u dropped",
                      system_metrics_.total_messages_sent,
                      system_metrics_.total_messages_received,
                      system_metrics_.total_messages_dropped);
        platform::logf("Total errors: %u", system_metrics_.total_errors);
        platform::logf("Free heap: %u bytes", static_cast<u32>(system_metrics_.free_heap_bytes));
        
        // Per-task statistics
        platform::log("\n--- TASK STATISTICS ---");
        for (size_t i = 0; i < task_ids_.size(); ++i) {
            const auto& metrics = task_metrics_[i];
            if (metrics.execution_count > 0) {
                platform::logf("Task %u:", task_ids_[i].value());
                platform::logf("  Executions: %u", metrics.execution_count);
                platform::logf("  Avg exec time: %u us", metrics.avg_execution_time_us);
                platform::logf("  Min/Max exec: %u/%u us", 
                              metrics.min_execution_time_us, metrics.max_execution_time_us);
                if (metrics.message_count > 0) {
                    platform::logf("  Messages: %u", metrics.message_count);
                    platform::logf("  Avg latency: %u us", metrics.avg_latency_us);
                    platform::logf("  Min/Max latency: %u/%u us",
                                  metrics.min_latency_us, metrics.max_latency_us);
                }
                platform::logf("  Errors: %u", metrics.error_count);
            }
        }
        
        platform::log("=== END REPORT ===");
    }
};

/**
 * @brief Global profiler instance
 */
inline performance_profiler& get_global_profiler() noexcept {
    static performance_profiler profiler;
    return profiler;
}

} // namespace emCore::diagnostics

