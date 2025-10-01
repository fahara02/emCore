#pragma once

#include <cstddef>

#include "../core/types.hpp"
#include "../core/config.hpp"
#include "../error/result.hpp"
#include "../platform/platform.hpp"
#include "task_config.hpp"

#include <etl/vector.h>

namespace emCore {

enum class task_state : u8 {
    idle,
    ready,
    running,
    suspended,
    completed
};

using task_id_t = u16;
constexpr task_id_t invalid_task_id = 0xFFFF;

struct task_control_block {
    task_id_t id{invalid_task_id};
    string32 name;
    task_config::task_function_ptr function{nullptr};
    void* parameters{nullptr};
    priority priority_level{priority::normal};
    task_state state{task_state::idle};
    timestamp_t created_time{0};
    timestamp_t last_run_time{0};
    timestamp_t next_run_time{0};
    duration_t period_ms{0};
    duration_t execution_time{0};
    u32 run_count{0};
};

class taskmaster {
private:
    etl::vector<task_control_block, config::max_tasks> tasks_;
    task_id_t next_task_id_{0};
    bool initialized_{false};
    
    taskmaster() : tasks_(), next_task_id_(0), initialized_(false) {}
    
    [[nodiscard]] static timestamp_t get_current_time() noexcept {
        return platform::get_system_time();
    }
    
    task_control_block* find_task(task_id_t task_id) noexcept {
        for (auto& task : tasks_) {
            if (task.id == task_id) {
                return &task;
            }
        }
        return nullptr;
    }
    
public:
    ~taskmaster() = default;
    
    taskmaster(const taskmaster&) = delete;
    taskmaster& operator=(const taskmaster&) = delete;
    taskmaster(taskmaster&&) = delete;
    taskmaster& operator=(taskmaster&&) = delete;
    
    static taskmaster& instance() noexcept {
        static taskmaster instance;
        return instance;
    }
    
    result<void, error_code> initialize() noexcept {
        if (initialized_) {
            return result<void, error_code>(error_code::already_exists);
        }
        
        tasks_.clear();
        next_task_id_ = 0;
        initialized_ = true;
        
        return ok();
    }
    
    template<size_t N>
    result<void, error_code> create_all_tasks(const task_config (&config_table)[N]) noexcept {
        if (!initialized_) {
            return result<void, error_code>(error_code::not_initialized);
        }
        
        for (size_t i = 0; i < N; ++i) {
            if (config_table[i].enabled) {
                auto res = create_task(config_table[i]);
                if (res.is_error()) {
                    return result<void, error_code>(res.error());
                }
            }
        }
        
        return ok();
    }
    
    result<task_id_t, error_code> create_task(const task_config& cfg) noexcept {
        if (!initialized_) {
            return result<task_id_t, error_code>(error_code::not_initialized);
        }
        
        if (tasks_.full()) {
            return result<task_id_t, error_code>(error_code::out_of_memory);
        }
        
        task_control_block tcb;
        tcb.id = next_task_id_++;
        tcb.name = cfg.name;
        tcb.function = cfg.function;
        tcb.parameters = cfg.parameters;
        tcb.priority_level = cfg.priority_level;
        tcb.state = task_state::ready;
        tcb.created_time = get_current_time();
        tcb.period_ms = cfg.period_ms;
        tcb.next_run_time = tcb.created_time;
        
        tasks_.push_back(tcb);
        
        return result<task_id_t, error_code>(tcb.id);
    }
    
    result<void, error_code> start_task(task_id_t task_id) noexcept {
        auto* task = find_task(task_id);
        if (task == nullptr) {
            return result<void, error_code>(error_code::not_found);
        }
        
        if (task->state == task_state::suspended) {
            task->state = task_state::ready;
            return ok();
        }
        
        return result<void, error_code>(error_code::invalid_parameter);
    }
    
    result<void, error_code> suspend_task(task_id_t task_id) noexcept {
        auto* task = find_task(task_id);
        if (task == nullptr) {
            return result<void, error_code>(error_code::not_found);
        }
        
        task->state = task_state::suspended;
        return ok();
    }
    
    result<void, error_code> resume_task(task_id_t task_id) noexcept {
        return start_task(task_id);
    }
    
    result<void, error_code> delete_task(task_id_t task_id) noexcept {
        for (auto* it = tasks_.begin(); it != tasks_.end(); ++it) {
            if (it->id == task_id) {
                tasks_.erase(it);
                return ok();
            }
        }
        
        return result<void, error_code>(error_code::not_found);
    }
    
    void run() noexcept {
        if (!initialized_) {
            return;
        }
        
        timestamp_t current_time = get_current_time();
        
        // Find highest priority ready task
        task_control_block* task_to_run = nullptr;
        priority highest_priority = priority::idle;
        
        for (auto& task : tasks_) {
            if (task.state != task_state::ready) {
                continue;
            }
            
            // Check if periodic task is due
            if (task.period_ms > 0 && current_time < task.next_run_time) {
                continue;
            }
            
            // Select highest priority task
            if (task.priority_level > highest_priority) {
                highest_priority = task.priority_level;
                task_to_run = &task;
            }
        }
        
        // Execute the selected task
        if (task_to_run != nullptr && task_to_run->function != nullptr) {
            task_to_run->state = task_state::running;
            task_to_run->last_run_time = current_time;
            
            timestamp_t start_time = get_current_time();
            task_to_run->function(task_to_run->parameters);
            timestamp_t end_time = get_current_time();
            
            task_to_run->execution_time = static_cast<duration_t>(end_time - start_time);
            task_to_run->run_count++;
            
            // Update next run time for periodic tasks
            if (task_to_run->period_ms > 0) {
                task_to_run->next_run_time = current_time + task_to_run->period_ms;
                task_to_run->state = task_state::ready;
            } else {
                task_to_run->state = task_state::completed;
            }
        }
    }
    
    [[nodiscard]] result<const task_control_block*, error_code> get_task_info(task_id_t task_id) const noexcept {
        for (const auto& task : tasks_) {
            if (task.id == task_id) {
                return result<const task_control_block*, error_code>(&task);
            }
        }
        return result<const task_control_block*, error_code>(error_code::not_found);
    }
    
    [[nodiscard]] size_t get_task_count() const noexcept { return tasks_.size(); }
    
    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }
};

}  // namespace emCore
