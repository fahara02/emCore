#pragma once

#include <cstddef>

#include "../core/types.hpp"
#include "../core/config.hpp"
#include "../error/result.hpp"
#include "../os/tasks.hpp"
#include <new>
#include "task_config.hpp"
#include "../messaging/message_broker.hpp"
#include "../messaging/message_types.hpp"
#include "../messaging/broker_global.hpp"
#if EMCORE_ENABLE_ZC
#include "../messaging/zero_copy.hpp"
#endif
#if EMCORE_ENABLE_EVENT_LOGS
#include "../messaging/event_log.hpp"
#endif
#include "../messaging/qos_pubsub.hpp"
#include "../messaging/distributed_state.hpp"
#include "rtos_scheduler.hpp"
#include "watchdog.hpp"

#include "../os/time.hpp"

#include <etl/algorithm.h>
#include <etl/utility.h>
#include "../messaging/broker_global.hpp"

namespace emCore {

/* Re-export messaging types for convenience */
using messaging::message_broker;
using messaging::medium_message;
using messaging::small_message;
using messaging::large_message;
using messaging::message_priority;
using messaging::message_flags;


enum class task_state : u8 {
    idle,
    ready,
    running,
    suspended,
    completed
};

/* task_id_t and invalid_task_id now defined in core/types.hpp */

struct task_statistics {
    duration_t min_execution_time{0xFFFFFFFF};
    duration_t max_execution_time{0};
    duration_t avg_execution_time{0};
    u32 missed_deadlines{0};
    u32 total_execution_time{0};
};

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
    duration_t deadline_ms{0};
    u32 run_count{0};
    task_statistics stats;
    os::task_handle_t native_handle{nullptr};  /* RTOS task handle */
    u32 stack_size{4096};  /* Stack size in bytes */
    bool is_native{false};  /* True if created as native RTOS task */
};
class taskmaster {
private:
    etl::vector<task_control_block, config::max_tasks> tasks_;
    task_id_t next_task_id_{invalid_task_id};
    bool initialized_{false};
    
    volatile bool tasks_ready_{false};  /* Flag to signal tasks can start */
    timestamp_t scheduler_start_time_{0};
    u32 total_context_switches_{0};
    // Brokers & pools
    using medium_broker_t = messaging::message_broker<medium_message, config::max_tasks>;

    #if EMCORE_ENABLE_SMALL_BROKER
    using small_broker_t = messaging::message_broker<small_message, config::max_tasks>;
    alignas(small_broker_t) unsigned char EMCORE_BSS_ATTR small_broker_storage_[sizeof(small_broker_t)]{};
    etl::unique_ptr<small_broker_t, messaging::pool_deleter<small_broker_t>> small_broker_{};
    #endif

    // Zero-copy pool and broker (sized via config)
    #if EMCORE_ENABLE_ZC
    static constexpr size_t zc_block_size_  = config::zc_block_size;
    static constexpr size_t zc_block_count_ = config::zc_block_count;
    using zc_pool_t = messaging::zero_copy_pool<zc_block_size_, zc_block_count_>;
    using zc_msg_t  = messaging::zc_message_envelope<zc_pool_t>;
    using zc_broker_t = messaging::message_broker<zc_msg_t, config::max_tasks>;

    alignas(zc_pool_t)   unsigned char EMCORE_BSS_ATTR zc_pool_storage_[sizeof(zc_pool_t)]{};
    zc_pool_t* zc_pool_ptr_{nullptr};
    alignas(zc_broker_t) unsigned char EMCORE_BSS_ATTR zc_broker_storage_[sizeof(zc_broker_t)]{};
    etl::unique_ptr<zc_broker_t, messaging::pool_deleter<zc_broker_t>> zc_broker_{};
    #endif

    // Event logs (capacities via config)
    #if EMCORE_ENABLE_EVENT_LOGS
    using med_log_t   = messaging::event_log<medium_message, config::event_log_med_cap, true>;
    using small_log_t = messaging::event_log<small_message, config::event_log_sml_cap, true>;
    using zc_log_t    = messaging::event_log<zc_msg_t,      config::event_log_zc_cap,  true>;
    alignas(med_log_t)   unsigned char EMCORE_BSS_ATTR med_log_storage_[sizeof(med_log_t)]{};
    alignas(small_log_t) unsigned char EMCORE_BSS_ATTR small_log_storage_[sizeof(small_log_t)]{};
    alignas(zc_log_t)    unsigned char EMCORE_BSS_ATTR zc_log_storage_[sizeof(zc_log_t)]{};
    med_log_t*   med_log_{nullptr};
    small_log_t* small_log_{nullptr};
    zc_log_t*    zc_log_{nullptr};
    #endif
    timestamp_t total_idle_time_{0};
    timestamp_t last_idle_time_{0};
    taskmaster() noexcept
        : tasks_()
        , next_task_id_{invalid_task_id}
        , initialized_{false}
        , tasks_ready_{false}
        , scheduler_start_time_{0}
        , total_context_switches_{0}
        , total_idle_time_{0}
        , last_idle_time_{0}
    {
        /* Construct brokers/pools/logs in-place to avoid dynamic allocation */
        // Medium broker now centralized via TLSF arena in messaging::global_medium_broker()
        (void)messaging::global_medium_broker();
        // Small broker
        #if EMCORE_ENABLE_SMALL_BROKER
        auto* sptr = new (static_cast<void*>(small_broker_storage_)) small_broker_t();
        small_broker_ = etl::unique_ptr<small_broker_t, messaging::pool_deleter<small_broker_t>>(sptr, messaging::pool_deleter<small_broker_t>{nullptr});
        #endif
        // Zero-copy pool + broker
        #if EMCORE_ENABLE_ZC
        zc_pool_ptr_ = new (static_cast<void*>(zc_pool_storage_)) zc_pool_t();
        auto* zbptr = new (static_cast<void*>(zc_broker_storage_)) zc_broker_t();
        zc_broker_ = etl::unique_ptr<zc_broker_t, messaging::pool_deleter<zc_broker_t>>(zbptr, messaging::pool_deleter<zc_broker_t>{nullptr});
        #endif
        // Event logs
        #if EMCORE_ENABLE_EVENT_LOGS
        med_log_   = new (static_cast<void*>(med_log_storage_))   med_log_t();
        small_log_ = new (static_cast<void*>(small_log_storage_)) small_log_t();
        zc_log_    = new (static_cast<void*>(zc_log_storage_))    zc_log_t();
        #endif
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
        next_task_id_.value() = 0;
        scheduler_start_time_ = get_current_time();
        total_context_switches_ = 0;
        total_idle_time_ = 0;
        last_idle_time_ = 0;
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
                /* Check config flag to decide native vs cooperative */
                auto res = config_table[i].create_native 
                    ? create_native_task(config_table[i])
                    : create_task(config_table[i]);
                    
                if (res.is_error()) {
                    return result<void, error_code>(res.error());
                }
                
                /* Auto-register task with broker */
                task_id_t task_id = res.value();
                auto* tcb = find_task(task_id);
                if (tcb != nullptr) {
                    get_broker().register_task(task_id, tcb->native_handle);
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
        
        task_id_t new_id = next_task_id_++;
        
        task_control_block tcb;
        tcb.id = new_id;
        tcb.name = cfg.name;
        tcb.function = cfg.function;
        tcb.parameters = cfg.parameters;
        tcb.priority_level = cfg.priority_level;
        tcb.state = task_state::ready;
        tcb.created_time = get_current_time();
        tcb.period_ms = cfg.period_ms;
        tcb.next_run_time = tcb.created_time;
        tcb.is_native = false;
        
        tasks_.push_back(tcb);
        
        return result<task_id_t, error_code>(new_id);
    }
    
    /* Create native RTOS task (FreeRTOS/CMSIS-RTOS) - uses config values */
    result<task_id_t, error_code> create_native_task(const task_config& cfg) noexcept {
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
        tcb.stack_size = cfg.stack_size.value();
        tcb.is_native = true;
        
        /* Reserve space to prevent reallocation invalidating pointers */
        if (tasks_.available() == 0) {
            return result<task_id_t, error_code>(error_code::out_of_memory);
        }
        
        /* Add to vector - safe because we checked capacity */
        tasks_.push_back(tcb);
        
        /* Get stable pointer to handle slot */
        auto* handle_ptr = &tasks_.back().native_handle;
        
        /* Always pass pointer to TCB to trampoline; user params forwarded inside */
        void* task_param = &tasks_.back();
        
        
        /* Now create actual RTOS task - it will start immediately */
        os::task_create_params params{
            &taskmaster::native_task_trampoline,
            cfg.name,  /* Already const char* */
            cfg.stack_size.value(),
            task_param,  /* Pass task ID as parameter */
            cfg.rtos_priority.value(),
            handle_ptr,  /* Use stable pointer */
            false,  /* start_suspended */
            (cfg.cpu_affinity.value() >= 0),  /* pin_to_core */
            static_cast<int>(cfg.cpu_affinity.value())  /* core_id */
        };
        
        if (!os::create_native_task(params)) {
            tasks_.pop_back();  /* Remove TCB if creation failed */
            return result<task_id_t, error_code>(error_code::invalid_parameter);
        }
        
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
    
    /* Create all tasks from configuration table */
    result<void, error_code> create_all_tasks(const task_config* configs, size_t count) noexcept {
        for (size_t i = 0; i < count; ++i) {
            auto res = configs[i].create_native ? 
                create_native_task(configs[i]) : 
                create_task(configs[i]);
            if (res.is_error()) {
                return result<void, error_code>(res.error());
            }
            
            /* Auto-register task with broker */
            task_id_t task_id = res.value();
            
            auto* tcb = find_task(task_id);
            if (tcb != nullptr) {
                get_broker().register_task(task_id, tcb->native_handle);
            }
        }
        return ok();
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
            total_context_switches_++;
            
            /* Update statistics */
            task_to_run->stats.min_execution_time = etl::min(task_to_run->execution_time, task_to_run->stats.min_execution_time);
            task_to_run->stats.max_execution_time = etl::max(task_to_run->execution_time, task_to_run->stats.max_execution_time);
            task_to_run->stats.total_execution_time += task_to_run->execution_time;
            task_to_run->stats.avg_execution_time = task_to_run->stats.total_execution_time / task_to_run->run_count;
            
            /* Check deadline miss */
            if (task_to_run->deadline_ms > 0 && task_to_run->execution_time > task_to_run->deadline_ms) {
                task_to_run->stats.missed_deadlines++;
            }
            
            // Update next run time for periodic tasks
            if (task_to_run->period_ms > 0) {
                task_to_run->next_run_time = current_time + task_to_run->period_ms;
                task_to_run->state = task_state::ready;
            } else {
                task_to_run->state = task_state::completed;
            }
        } else {
            // No task ready to run - yield to prevent watchdog timeout
            os::delay_ms(1);
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
    
    /* Signal all tasks to start - call after initialization complete */
    void start_all_tasks() noexcept {
        tasks_ready_ = true;
    }
    
 
    /* Get current task ID from native handle */
    [[nodiscard]] task_id_t get_current_task_id() const noexcept {
        auto* current_handle = os::current_task();
        if (current_handle == nullptr) {
            return invalid_task_id;
        }
        
        for (const auto& task : tasks_) {
            if (task.native_handle == current_handle) {
                return task.id;
            }
        }
        
        return invalid_task_id;
    }
    
    /* Task priority management */
    result<void, error_code> set_task_priority(task_id_t task_id, priority new_priority) noexcept {
        auto* task = find_task(task_id);
        if (task == nullptr) {
            return result<void, error_code>(error_code::not_found);
        }
        task->priority_level = new_priority;
        return ok();
    }
    
    result<priority, error_code> get_task_priority(task_id_t task_id) const noexcept {
        for (const auto& task : tasks_) {
            if (task.id == task_id) {
                return result<priority, error_code>(task.priority_level);
            }
        }
        return result<priority, error_code>(error_code::not_found);
    }
    
    /* Task period management */
    /* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) - task_id and period_ms are semantically different */
    result<void, error_code> set_task_period(task_id_t task_id, duration_t period_ms) noexcept {
        auto* task = find_task(task_id);
        if (task == nullptr) {
            return result<void, error_code>(error_code::not_found);
        }
        task->period_ms = period_ms;
        return ok();
    }
    
    /* Task deadline management */
   
    result<void, error_code> set_task_deadline(task_id_t task_id, duration_t deadline_ms) noexcept {
        auto* task = find_task(task_id);
        if (task == nullptr) {
            return result<void, error_code>(error_code::not_found);
        }
        task->deadline_ms = deadline_ms;
        return ok();
    }
    
    /* Get task by name */
    result<task_id_t, error_code> get_task_by_name(const char* name) const noexcept {
        for (const auto& task : tasks_) {
            if (task.name == name) {
                return result<task_id_t, error_code>(task.id);
            }
        }
        return result<task_id_t, error_code>(error_code::not_found);
    }
    
    /* Reset task statistics */
    result<void, error_code> reset_task_statistics(task_id_t task_id) noexcept {
        auto* task = find_task(task_id);
        if (task == nullptr) {
            return result<void, error_code>(error_code::not_found);
        }
        task->stats = task_statistics{};
        task->run_count = 0;
        return ok();
    }
    
    /* Get scheduler statistics */
    [[nodiscard]] u32 get_total_context_switches() const noexcept { return total_context_switches_; }
    [[nodiscard]] duration_t get_uptime() const noexcept { 
        return static_cast<duration_t>(get_current_time() - scheduler_start_time_); 
    }
    [[nodiscard]] duration_t get_total_idle_time() const noexcept { return total_idle_time_; }
    
    /* CPU utilization (0-100) */
    [[nodiscard]] u8 get_cpu_utilization() const noexcept {
        duration_t uptime = get_uptime();
        if (uptime == 0) {
            return 0;
        }
        duration_t busy_time = uptime - total_idle_time_;
        return static_cast<u8>((busy_time * 100) / uptime);
    }
    
    /* Iterate over all tasks */
    template<typename Func>
    result<void, error_code> register_task_function(Func func) noexcept {
        for (auto& task : tasks_) {
            etl::forward<Func>(func)(task);
        }
        return ok();
    }
    
    template<typename Func>
    result<void, error_code> schedule_task(Func func, duration_t delay_ms = 0) noexcept {
        (void)delay_ms;  /* Reserved for future use */
        for (const auto& task : tasks_) {
            etl::forward<Func>(func)(task);
        }
        return ok();
    }
    
    
    /* Subscribe task to a topic */
    static result<void, error_code> subscribe(topic_id_t topic_id, task_id_t task_id) noexcept {
        return get_broker().subscribe(topic_id, task_id);
    }

    /* Small-message wrappers */
    #if EMCORE_ENABLE_SMALL_BROKER
    static result<void, error_code> subscribe_small(topic_id_t topic_id, task_id_t task_id) noexcept {
        return taskmaster::broker_small().subscribe(topic_id, task_id);
    }

    static result<void, error_code> publish_small(u16 topic_id, messaging::small_message& msg, task_id_t from) noexcept {
        return taskmaster::broker_small().publish(topic_id, msg, from);
    }

    static result<messaging::small_message, error_code> receive_small(task_id_t self, timeout_ms_t timeout) noexcept {
        return taskmaster::broker_small().receive(self, timeout);
    }
    #endif

    /* Zero-copy wrappers */
    #if EMCORE_ENABLE_ZC
    static result<void, error_code> subscribe_zero(topic_id_t topic_id, task_id_t task_id) noexcept {
        return taskmaster::broker_zero().subscribe(topic_id, task_id);
    }

    static result<zc_msg_t, error_code> receive_zero(task_id_t self, timeout_ms_t timeout) noexcept {
        return taskmaster::broker_zero().receive(self, timeout);
    }

    static result<void, error_code> publish_zero(u16 topic_id, zc_msg_t& msg, task_id_t from) noexcept {
        return taskmaster::broker_zero().publish(topic_id, msg, from);
    }
    #endif

    /* QoS helpers (non-owning, no dynamic allocation) */
    static messaging::qos_publisher<messaging::medium_message>
    make_qos_publisher_medium(task_id_t from_task_id, u16 ack_topic_id) noexcept {
        return messaging::qos_publisher<messaging::medium_message>(taskmaster::broker_medium(), from_task_id, ack_topic_id);
    }

    static messaging::qos_subscriber<messaging::medium_message>
    make_qos_subscriber_medium(task_id_t self_task_id, u16 ack_topic_id) noexcept {
        return messaging::qos_subscriber<messaging::medium_message>(taskmaster::broker_medium(), self_task_id, ack_topic_id);
    }

    #if EMCORE_ENABLE_SMALL_BROKER
    static messaging::qos_publisher<messaging::small_message>
    make_qos_publisher_small(task_id_t from_task_id, u16 ack_topic_id) noexcept {
        return messaging::qos_publisher<messaging::small_message>(taskmaster::broker_small(), from_task_id, ack_topic_id);
    }

    static messaging::qos_subscriber<messaging::small_message>
    make_qos_subscriber_small(task_id_t self_task_id, u16 ack_topic_id) noexcept {
        return messaging::qos_subscriber<messaging::small_message>(taskmaster::broker_small(), self_task_id, ack_topic_id);
    }

    /* Distributed-state helper (small_message coordination) */
    template <typename StateT, u16 ProposeTopicId, u16 AckTopicId, u16 CommitTopicId,
              size_t MaxPeers, size_t MaxOutstanding = 4>
    static messaging::distributed_state<StateT, ProposeTopicId, AckTopicId, CommitTopicId, MaxPeers, MaxOutstanding>
    make_distributed_state(task_id_t self_task_id, const StateT& initial) noexcept {
        return messaging::distributed_state<StateT, ProposeTopicId, AckTopicId, CommitTopicId, MaxPeers, MaxOutstanding>(
            taskmaster::broker_small(), self_task_id, initial);
    }
    #endif
    
    /* Publish message to topic */
    template<typename MessageType = medium_message>
    result<void, error_code> publish(u16 topic_id, MessageType& msg, task_id_t from_task_id) noexcept {
        return get_broker().publish(topic_id, msg, from_task_id);
    }
    
    /* Receive message (blocking) */
    template<typename MessageType = medium_message>
    result<MessageType, error_code> receive(task_id_t task_id, timeout_ms_t timeout = timeout_ms_t::infinite()) noexcept {
        return get_broker().receive(task_id, timeout);
    }
    
    /* Receive message (non-blocking) */
    template<typename MessageType = medium_message>
    result<MessageType, error_code> try_receive(task_id_t task_id) noexcept {
        return get_broker().try_receive(task_id);
    }
       /* Tasks call this to wait until initialization is complete */
       void wait_until_ready() const noexcept {
        while (!tasks_ready_) {
            os::delay_ms(10);
        }
    }
    /* Broadcast to all tasks */
    template<typename MessageType = medium_message>
    result<void, error_code> broadcast(const MessageType& msg) noexcept {
        return get_broker().broadcast(msg);
    }
    
    /* Get broker statistics */
    [[nodiscard]] static u32 messages_sent() noexcept { return get_broker().total_sent(); }
    [[nodiscard]] static u32 messages_received() noexcept { return get_broker().total_received(); }
    [[nodiscard]] static u32 messages_dropped() noexcept { return get_broker().total_dropped(); }
    
    /* Debug: Get mailbox count */
    [[nodiscard]] static size_t mailbox_count() noexcept { return get_broker().mailbox_count(); }

    /* Configure per-task mailbox depth (soft cap) */
    static result<void, error_code> set_mailbox_depth(task_id_t task_id, size_t depth) noexcept {
        return get_broker().set_mailbox_depth(task_id, depth);
    }

    /* Configure per-topic subscriber capacity (soft cap) */
    static result<void, error_code> set_topic_capacity(topic_id_t topic_id, size_t max_subs) noexcept {
        return get_broker().set_topic_capacity(topic_id.value, max_subs);
    }

    /* Configure per-mailbox overflow policy (true = drop_oldest, false = reject new) */
    static result<void, error_code> set_overflow_policy(task_id_t task_id, bool drop_oldest) noexcept {
        return get_broker().set_overflow_policy(task_id, drop_oldest);
    }

    /* Configure global notify policy (true = notify only on empty->non-empty) */
    static result<void, error_code> set_notify_on_empty_only(bool enabled) noexcept {
        return get_broker().set_notify_on_empty_only(enabled);
    }


public:
    // Accessors to unified messaging package
    static messaging::Ibroker<medium_message>& broker_medium() noexcept { return messaging::global_medium_broker(); }
    #if EMCORE_ENABLE_SMALL_BROKER
    static messaging::Ibroker<small_message>&  broker_small()  noexcept { return *(taskmaster::instance().small_broker_); }
    #endif
    #if EMCORE_ENABLE_ZC
    static messaging::Ibroker<zc_msg_t>&       broker_zero()   noexcept { return *(taskmaster::instance().zc_broker_); }
    static zc_pool_t&                          zc_pool()       noexcept { return *(taskmaster::instance().zc_pool_ptr_); }
    #endif
    #if EMCORE_ENABLE_EVENT_LOGS
    static med_log_t&                          event_log_medium() noexcept { return *(taskmaster::instance().med_log_); }
    static small_log_t&                        event_log_small()  noexcept { return *(taskmaster::instance().small_log_); }
    static zc_log_t&                           event_log_zero()   noexcept { return *(taskmaster::instance().zc_log_); }
    #endif

    // Public aliases for zero-copy types
    #if EMCORE_ENABLE_ZC
    using zero_copy_pool_type = zc_pool_t;
    using zero_copy_message  = zc_msg_t;
    #endif

private:
    /* Broker owned by taskmaster (constructed in ctor) */
    static messaging::message_broker<medium_message, config::max_tasks>& get_broker() noexcept {
        return messaging::global_medium_broker();
    }

   
    
    [[nodiscard]] static timestamp_t get_current_time() noexcept {
        return os::time_ms();
    }
    
    task_control_block* find_task(task_id_t task_id) noexcept {
        // O(1) lookup using task_id as direct index
        if (task_id.value() >= tasks_.size()) {
            return nullptr;
        }
        
        // Verify the task at this index has the correct ID
        auto& task = tasks_[task_id.value()];
        return (task.id == task_id) ? &task : nullptr;
    }
    
    /* Native task trampoline to enforce periodic scheduling and instrumentation */
    static void native_task_trampoline(void* param) noexcept {
        if (param == nullptr) {
            return;
        }
        auto* tcb = static_cast<task_control_block*>(param);
        auto& task_mgr = taskmaster::instance();

        /* Ensure system init completed if used */
        task_mgr.wait_until_ready();

        const task_id_t tid = tcb->id;
        auto* user_fn = tcb->function;
        if (user_fn == nullptr) {
            return;
        }
        /* Forward user parameter if provided; otherwise pass TCB pointer */
        void* user_param = (tcb->parameters != nullptr) ? tcb->parameters : static_cast<void*>(tcb);

        if (tcb->period_ms > 0) {
            for (;;) {
                emCore::task::get_global_scheduler().start_execution_timing(tid);
                user_fn(user_param);
                emCore::task::get_global_scheduler().end_execution_timing(tid);
                emCore::get_global_watchdog().feed(tid);
                emCore::task::get_global_scheduler().update_stack_usage(tid);
                emCore::task::get_global_scheduler().adaptive_yield(tid);
                emCore::os::delay_ms(tcb->period_ms);
            }
        } else {
            /* Non-periodic: call once; user may implement its own loop */
            emCore::task::get_global_scheduler().start_execution_timing(tid);
            user_fn(user_param);
            emCore::task::get_global_scheduler().end_execution_timing(tid);
            /* One-time feed to avoid early false positive */
            emCore::get_global_watchdog().feed(tid);
        }
    }





};

}  // namespace emCore
