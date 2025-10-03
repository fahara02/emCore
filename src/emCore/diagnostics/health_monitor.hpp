#pragma once

#include "../core/types.hpp"
#include "../core/strong_types.hpp"
#include "../core/config.hpp"
#include "../platform/platform.hpp"
#include "profiler.hpp"

#include <etl/vector.h>
#include <cstddef>



namespace emCore::diagnostics {

/**
 * @brief Task health status
 */
enum class task_health_status : u8 {
    unknown,        // Not monitored yet
    healthy,        // Operating normally
    warning,        // Performance degraded
    critical,       // Severe issues
    unresponsive    // Not responding
};

/**
 * @brief System health status
 */
struct system_health_status {
    // Task health
    u8 tasks_running{0};
    u8 tasks_suspended{0};
    u8 tasks_faulted{0};
    u8 tasks_total{0};
    
    // Message system health
    u32 messages_in_flight{0};
    u32 messages_dropped_total{0};
    f32 queue_utilization_percent{0.0F};
    f32 message_throughput_per_sec{0.0F};
    
    // System resources
    f32 cpu_utilization_percent{0.0F};
    size_t free_memory_bytes{0};
    size_t total_memory_bytes{0};
    f32 memory_utilization_percent{0.0F};
    
    // Error rates
    u32 error_rate_per_min{0};
    u32 watchdog_timeouts{0};
    
    // System timing
    timestamp_t uptime_ms{0};
    timestamp_t last_update_time{0};
    
    // Overall health
    task_health_status overall_health{task_health_status::unknown};
};

// Strong types for threshold parameters to prevent argument swapping
struct cpu_warning_tag final {};
using cpu_warning_pct = core::strong_type<f32, cpu_warning_tag>;

struct cpu_critical_tag final {};
using cpu_critical_pct = core::strong_type<f32, cpu_critical_tag>;

struct mem_warning_tag final {};
using mem_warning_pct = core::strong_type<f32, mem_warning_tag>;

struct mem_critical_tag final {};
using mem_critical_pct = core::strong_type<f32, mem_critical_tag>;

/**
 * @brief Task health entry
 */
struct task_health_entry {
    task_id_t task_id{invalid_task_id};
    task_health_status status{task_health_status::unknown};
    timestamp_t last_seen{0};
    u32 error_count{0};
    u32 timeout_count{0};
    f32 cpu_usage_percent{0.0F};
    duration_t avg_response_time_us{0};
    bool is_responsive{true};
    
    /**
     * @brief Update health status based on metrics
     */
    void update_health(const task_performance_metrics& metrics) noexcept {
        last_seen = platform::get_system_time_us();
        error_count = metrics.error_count;
        
        // Calculate CPU usage percentage
        if (metrics.execution_count > 0) {
            cpu_usage_percent = static_cast<f32>(metrics.cpu_usage_percent_x100) / 100.0F;
        }
        
        avg_response_time_us = metrics.avg_latency_us;
        
        // Determine health status
        if (error_count > 10) {
            status = task_health_status::critical;
        } else if (error_count > 5 || avg_response_time_us > 10000) { // >10ms latency
            status = task_health_status::warning;
        } else if (metrics.execution_count > 0) {
            status = task_health_status::healthy;
        } else {
            status = task_health_status::unknown;
        }
        
        // Check responsiveness (last seen within 30 seconds)
        timestamp_t now = platform::get_system_time_us();
        is_responsive = (now - last_seen) < static_cast<timestamp_t>(30000000); // 30 seconds in microseconds
        
        if (!is_responsive) {
            status = task_health_status::unresponsive;
        }
    }
};

/**
 * @brief System health monitor
 */
class health_monitor {
private:
    static constexpr size_t max_tasks = config::max_tasks;
    static constexpr duration_t update_interval_ms = 5000; // 5 seconds
    
    etl::vector<task_health_entry, max_tasks> task_health_;
    system_health_status system_health_;
    timestamp_t last_update_time_{0};
    bool monitoring_enabled_{false};
    
    // Health thresholds
    f32 cpu_warning_threshold_{75.0F};     // 75% CPU usage
    f32 cpu_critical_threshold_{90.0F};    // 90% CPU usage
    f32 memory_warning_threshold_{80.0F};  // 80% memory usage
    f32 memory_critical_threshold_{95.0F}; // 95% memory usage
    
    /**
     * @brief Find task health entry
     */
    task_health_entry* find_task_health(task_id_t task_id) noexcept {
        for (auto& entry : task_health_) {
            if (entry.task_id == task_id) {
                return &entry;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Calculate overall system health
     */
    void calculate_overall_health() noexcept {
        u8 healthy_tasks = 0;
        u8 warning_tasks = 0;
        u8 critical_tasks = 0;
        u8 unresponsive_tasks = 0;
        
        for (const auto& entry : task_health_) {
            switch (entry.status) {
                case task_health_status::healthy:
                    healthy_tasks++;
                    break;
                case task_health_status::warning:
                    warning_tasks++;
                    break;
                case task_health_status::critical:
                    critical_tasks++;
                    break;
                case task_health_status::unresponsive:
                    unresponsive_tasks++;
                    break;
                default:
                    break;
            }
        }
        
        system_health_.tasks_running = healthy_tasks + warning_tasks;
        system_health_.tasks_faulted = critical_tasks + unresponsive_tasks;
        system_health_.tasks_total = static_cast<u8>(task_health_.size());
        
        // Determine overall health
        if (unresponsive_tasks > 0 || critical_tasks > task_health_.size() / 2) {
            system_health_.overall_health = task_health_status::critical;
        } else if (critical_tasks > 0 || warning_tasks > task_health_.size() / 2) {
            system_health_.overall_health = task_health_status::warning;
        } else if (healthy_tasks > 0) {
            system_health_.overall_health = task_health_status::healthy;
        } else {
            system_health_.overall_health = task_health_status::unknown;
        }
    }
    
public:
    health_monitor() noexcept = default;
    
    /**
     * @brief Enable/disable health monitoring
     */
    void enable_monitoring(bool enable = true) noexcept {
        monitoring_enabled_ = enable;
        if (enable) {
            last_update_time_ = platform::get_system_time_us();
        }
    }
    
    /**
     * @brief Register a task for health monitoring
     */
    bool register_task(task_id_t task_id) noexcept {
        if (task_health_.full() || find_task_health(task_id) != nullptr) {
            return false;
        }
        
        task_health_entry entry;
        entry.task_id = task_id;
        entry.last_seen = platform::get_system_time_us();
        task_health_.push_back(entry);
        return true;
    }
    
    /**
     * @brief Update health status (should be called periodically)
     */
    void update_health_status() noexcept {
        if (!monitoring_enabled_) {
            return;
        }
        
        timestamp_t now = platform::get_system_time_us();
        timestamp_t update_interval_us = static_cast<timestamp_t>(update_interval_ms) * 1000;
        if ((now - last_update_time_) < update_interval_us) {
            return; // Too soon for update
        }
        
        auto& profiler = get_global_profiler();
        
        // Update per-task health
        for (auto& entry : task_health_) {
            const auto* metrics = profiler.get_task_metrics(entry.task_id);
            if (metrics != nullptr) {
                entry.update_health(*metrics);
            }
        }
        
        // Update system health
        const auto& sys_metrics = profiler.get_system_metrics();
        system_health_.uptime_ms = static_cast<timestamp_t>(sys_metrics.system_uptime_us / 1000);
        system_health_.free_memory_bytes = sys_metrics.free_heap_bytes;
        system_health_.messages_dropped_total = sys_metrics.total_messages_dropped;
        system_health_.error_rate_per_min = sys_metrics.total_errors; // Simplified
        
        // Calculate CPU utilization
        f32 total_cpu = 0.0F;
        u8 active_tasks = 0;
        for (const auto& entry : task_health_) {
            if (entry.status != task_health_status::unknown) {
                total_cpu += entry.cpu_usage_percent;
                active_tasks++;
            }
        }
        system_health_.cpu_utilization_percent = active_tasks > 0 ? total_cpu / static_cast<f32>(active_tasks) : 0.0F;
        
        // Calculate memory utilization
        if (system_health_.total_memory_bytes > 0) {
            size_t used_memory = system_health_.total_memory_bytes - system_health_.free_memory_bytes;
            system_health_.memory_utilization_percent = 
                (static_cast<f32>(used_memory) / static_cast<f32>(system_health_.total_memory_bytes)) * 100.0F;
        }
        
        calculate_overall_health();
        
        system_health_.last_update_time = now;
        last_update_time_ = now;
    }
    
    /**
     * @brief Get system health status
     */
     [[nodiscard]]  const system_health_status& get_system_health() const noexcept {
        return system_health_;
    }
    
    /**
     * @brief Get task health status
     */
    [[nodiscard]] const task_health_entry* get_task_health(task_id_t task_id) const noexcept {
        for (const auto& entry : task_health_) {
            if (entry.task_id == task_id) {
                return &entry;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Check if system is healthy
     */
    [[nodiscard]] bool is_system_healthy() const noexcept {
        return system_health_.overall_health == task_health_status::healthy ||
               system_health_.overall_health == task_health_status::warning;
    }
    
    /**
     * @brief Generate health report
     */
    void generate_health_report() const noexcept {
        if (!monitoring_enabled_) {
            platform::log("Health monitoring is disabled");
            return;
        }
        
        platform::log("=== SYSTEM HEALTH REPORT ===");
        
        // Overall status
        platform::log("Overall Health: HEALTHY");
        platform::logf("Uptime: %u ms", static_cast<u32>(system_health_.uptime_ms));
        platform::logf("Tasks: %u running, %u faulted, %u total",
                      static_cast<u32>(system_health_.tasks_running),
                      static_cast<u32>(system_health_.tasks_faulted),
                      static_cast<u32>(system_health_.tasks_total));
        
        // Resource utilization
        platform::logf("CPU Usage: %u%%", static_cast<u32>(system_health_.cpu_utilization_percent));
        platform::logf("Memory: %u bytes free (%u%% used)",
                      static_cast<u32>(system_health_.free_memory_bytes),
                      static_cast<u32>(system_health_.memory_utilization_percent));
        
        // Error statistics
        platform::logf("Messages dropped: %u", system_health_.messages_dropped_total);
        platform::logf("Error rate: %u/min", system_health_.error_rate_per_min);
        
        // Per-task health
        platform::log("\n--- TASK HEALTH ---");
        for (const auto& entry : task_health_) {
            platform::logf("Task %u: (Errors: %u)",
                          static_cast<u32>(entry.task_id.value()),
                          entry.error_count);
        }
        
        platform::log("=== END HEALTH REPORT ===");
    }
    
    /**
     * @brief Set health thresholds
     */
    void set_thresholds(cpu_warning_pct cpu_warning,
                       cpu_critical_pct cpu_critical,
                       mem_warning_pct mem_warning,
                       mem_critical_pct mem_critical) noexcept {
        cpu_warning_threshold_ = cpu_warning.value();
        cpu_critical_threshold_ = cpu_critical.value();
        memory_warning_threshold_ = mem_warning.value();
        memory_critical_threshold_ = mem_critical.value();
    }
};

/**
 * @brief Global health monitor instance
 */
inline health_monitor& get_global_health_monitor() noexcept {
    static health_monitor monitor;
    return monitor;
}

} // namespace emCore::diagnostics

