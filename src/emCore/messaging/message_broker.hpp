#pragma once

#include <cstddef>

#include "../core/types.hpp"
#include "../core/config.hpp"
#include "../error/result.hpp"
#include "../os/time.hpp"
#include "../os/sync.hpp"
#include "../os/tasks.hpp"
#include "message_types.hpp"

#include <etl/circular_buffer.h>
#include <etl/vector.h>
#include <etl/map.h>
#include <etl/pool.h>
#include <etl/array.h>
#include <etl/memory.h>

#include "../memory/pool.hpp"
#include <utility>



namespace emCore::messaging {

// Lightweight broker interface to decouple advanced features from concrete broker.
// Header-only, no dynamic allocation required. Virtuals are used without RTTI.
template <typename MessageType>
class Ibroker {
public:
    using message_type = MessageType;
    virtual ~Ibroker() = default;

    // Non-copyable, non-movable interface
    Ibroker(const Ibroker&) = delete;
    Ibroker& operator=(const Ibroker&) = delete;
    Ibroker(Ibroker&&) = delete;
    Ibroker& operator=(Ibroker&&) = delete;

public:
    Ibroker() = default;

    virtual result<void, error_code> subscribe(topic_id_t topic_id, task_id_t subscriber_task_id) noexcept = 0;
    virtual result<void, error_code> publish(u16 topic_id, MessageType& msg, task_id_t from_task_id) noexcept = 0;
    virtual result<MessageType, error_code> receive(task_id_t task_id, timeout_ms_t timeout) noexcept = 0;
    virtual result<MessageType, error_code> try_receive(task_id_t task_id) noexcept = 0;
};

/**
 * @brief Professional message broker with pub/sub
 * Clean implementation that actually works
 */
template<typename MessageType = medium_message, size_t MaxTasks = config::max_tasks>
class message_broker : public Ibroker<MessageType> {
private:
    static constexpr size_t queue_capacity = config::default_mailbox_queue_capacity;
    static constexpr size_t max_topics = config::default_max_topics;
    static constexpr size_t max_subscribers_per_topic = config::default_max_subscribers_per_topic;
    static_assert(queue_capacity >= 1, "EMCORE_MSG_QUEUE_CAPACITY must be >= 1");
    static_assert(max_topics >= 1, "EMCORE_MSG_MAX_TOPICS must be >= 1");
    static_assert(max_subscribers_per_topic >= 1, "EMCORE_MSG_MAX_SUBS_PER_TOPIC must be >= 1");
    
    /* Task mailbox with per-topic sub-queues (each with high/normal) */
    struct task_mailbox {
        task_id_t task_id{invalid_task_id};
        os::task_handle_t handle{nullptr};
        mutable os::critical_section critical_section;
        // Soft limit across all per-topic queues
        u16 depth_limit{static_cast<u16>(queue_capacity)};
        // Stats
        u32 dropped_overflow{0};
        u32 received_count{0};
        bool overflow_drop_oldest{true};
        bool notify_on_empty_only{true};

        // Compile-time per-topic capacities
        static constexpr size_t topic_slots = config::default_max_topic_queues_per_mailbox;
        static_assert(topic_slots >= 1, "EMCORE_MSG_TOPIC_QUEUES_PER_MAILBOX must be >= 1");
        static_assert(config::default_topic_high_ratio_den != 0, "default_topic_high_ratio_den must not be zero");
        static constexpr size_t min_per_topic_total = 2;
        static constexpr size_t per_topic_total = ((queue_capacity / topic_slots) >= min_per_topic_total)
                                                  ? (queue_capacity / topic_slots)
                                                  : min_per_topic_total;
        static constexpr size_t calc_high = (per_topic_total * config::default_topic_high_ratio_num)
                                            / config::default_topic_high_ratio_den;
        static constexpr size_t high_capacity = (calc_high >= 1) ? calc_high : 1;
        static constexpr size_t normal_capacity_tmp = (per_topic_total > high_capacity)
                                                      ? (per_topic_total - high_capacity)
                                                      : 0;
        static constexpr size_t normal_capacity = (normal_capacity_tmp >= 1) ? normal_capacity_tmp : 1;

        struct topic_queue_entry {
            u16 topic_id{0xFFFF};
            etl::circular_buffer<MessageType, high_capacity> high_queue;
            etl::circular_buffer<MessageType, normal_capacity> normal_queue;
        };

        etl::vector<topic_queue_entry, topic_slots> topic_queues;

        task_mailbox() = default;

        size_t total_size() const noexcept {
            size_t total = 0;
            for (const auto& topic_queue : topic_queues) {
                total += topic_queue.high_queue.size();
                total += topic_queue.normal_queue.size();
            }
            return total;
        }

        bool is_empty_unlocked() const noexcept {
            for (const auto& topic_queue : topic_queues) {
                if (!topic_queue.high_queue.empty() || !topic_queue.normal_queue.empty()) {
                    return false;
                }
            }
            return true;
        }

        int find_topic_index(u16 topic_id) const noexcept {
            for (size_t i = 0; i < topic_queues.size(); ++i) {
                if (topic_queues[i].topic_id == topic_id) {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        topic_queue_entry* get_or_create_topic(u16 topic_id) noexcept {
            int idx = find_topic_index(topic_id);
            if (idx >= 0) {
                return &topic_queues[static_cast<size_t>(idx)];
            }
            if (topic_queues.full()) {
                return nullptr;
            }
            topic_queue_entry entry;
            entry.topic_id = topic_id;
            topic_queues.push_back(entry);
            return &topic_queues.back();
        }

        // Drop one message to make room (prefer normal across topics)
        bool drop_one_any() noexcept {
            // Prefer dropping from normal queues
            for (auto& topic_queue : topic_queues) {
                if (!topic_queue.normal_queue.empty()) { topic_queue.normal_queue.pop(); return true; }
            }
            // Then drop from high queues
            for (auto& topic_queue : topic_queues) {
                if (!topic_queue.high_queue.empty()) { topic_queue.high_queue.pop(); return true; }
            }
            return false;
        }

        /* Thread-safe send with per-topic routing and notify-on-empty */
        result<void, error_code> send(const MessageType& msg) noexcept {
            const bool is_urgent = (static_cast<message_flags>(msg.header.flags) & message_flags::urgent) == message_flags::urgent
                                   || (msg.header.priority >= static_cast<u8>(message_priority::high));
            critical_section.enter();

            bool was_empty = is_empty_unlocked();
            const bool depth_reached = (total_size() >= depth_limit);

            topic_queue_entry* topic_queue = get_or_create_topic(msg.header.type);
            if (topic_queue == nullptr) {
                critical_section.exit();
                return result<void, error_code>(error_code::out_of_memory);
            }

            bool target_full = is_urgent ? topic_queue->high_queue.full() : topic_queue->normal_queue.full();
            if (target_full || depth_reached) {
                const bool is_persistent = (static_cast<message_flags>(msg.header.flags) & message_flags::persistent) == message_flags::persistent;
                if (!is_persistent && overflow_drop_oldest && drop_one_any()) {
                    dropped_overflow++;
                    // fallthrough to push
                } else {
                    critical_section.exit();
                    return result<void, error_code>(error_code::out_of_memory);
                }
            }

            if (is_urgent) {
                if (!topic_queue->high_queue.full()) {
                    topic_queue->high_queue.push(msg);
                } else if (!topic_queue->normal_queue.full()) {
                    topic_queue->normal_queue.push(msg);
                } else {
                    critical_section.exit();
                    return result<void, error_code>(error_code::out_of_memory);
                }
            } else {
                if (!topic_queue->normal_queue.full()) {
                    topic_queue->normal_queue.push(msg);
                } else if (!topic_queue->high_queue.full()) {
                    topic_queue->high_queue.push(msg);
                } else {
                    critical_section.exit();
                    return result<void, error_code>(error_code::out_of_memory);
                }
            }

            bool should_notify = notify_on_empty_only ? was_empty : true;
            critical_section.exit();

            if (should_notify && handle != nullptr) {
                os::notify_task(handle, 0x01);
            }
            return ok();
        }

        /* Thread-safe receive: drain high across topics, then normal */
        result<MessageType, error_code> receive() noexcept {
            critical_section.enter();
            if (is_empty_unlocked()) {
                critical_section.exit();
                return result<MessageType, error_code>(error_code::not_found);
            }
            // First pass: high priority
            for (auto& topic_queue : topic_queues) {
                if (!topic_queue.high_queue.empty()) {
                    MessageType msg = topic_queue.high_queue.front();
                    topic_queue.high_queue.pop();
                    received_count++;
                    bool now_empty = is_empty_unlocked();
                    critical_section.exit();
                    if (now_empty) { os::clear_notification(); }
                    return result<MessageType, error_code>(msg);
                }
            }
            // Second pass: normal priority
            for (auto& topic_queue : topic_queues) {
                if (!topic_queue.normal_queue.empty()) {
                    MessageType msg = topic_queue.normal_queue.front();
                    topic_queue.normal_queue.pop();
                    received_count++;
                    bool now_empty = is_empty_unlocked();
                    critical_section.exit();
                    if (now_empty) { os::clear_notification(); }
                    return result<MessageType, error_code>(msg);
                }
            }
            critical_section.exit();
            return result<MessageType, error_code>(error_code::not_found);
        }

        bool empty() const noexcept { return is_empty_unlocked(); }
    };
    
    /* Topic subscription */
    struct topic_subscription {
        u16 topic_id{0xFFFF};
        // Soft capacity limit for subscribers (<= max_subscribers_per_topic)
        u16 capacity_limit{static_cast<u16>(max_subscribers_per_topic)};
        etl::vector<task_id_t, max_subscribers_per_topic> subscriber_ids;
        
        topic_subscription() = default;
        explicit topic_subscription(u16 topic_identifier) : topic_id(topic_identifier) {}
    };
    
    /* Storage */
    etl::vector<task_mailbox, MaxTasks> mailboxes_;
    etl::vector<topic_subscription, max_topics> topics_;
    
    /* Statistics */
    u32 sent_count_{0};
    u32 received_count_{0};
    u32 dropped_count_{0};
    u16 sequence_{0};
    bool notify_on_empty_only_{true};
    
    /* Find mailbox by task ID - O(1) lookup */
    task_mailbox* find_mailbox(task_id_t task_id) noexcept {
        // Direct indexing since task_id maps to mailbox index
        const size_t idx = static_cast<size_t>(task_id.value());
        if (idx >= mailboxes_.size()) {
            return nullptr;
        }
        auto& mailbox = mailboxes_[idx];
        return (mailbox.task_id == task_id) ? &mailbox : nullptr;
    }
    
    /* Find topic - O(log n) using binary search on sorted vector */
    topic_subscription* find_topic(u16 topic_id) noexcept {
        // Binary search since topics are kept sorted by topic_id
        auto iter = etl::lower_bound(topics_.begin(), topics_.end(), topic_id,
            [](const topic_subscription& topic, u16 topic_identifier) {
                return topic.topic_id < topic_identifier;
            });
        
        if (iter != topics_.end() && iter->topic_id == topic_id) {
            return &(*iter);
        }
        return nullptr;
    }
    
public:
    message_broker() noexcept = default;
    
    /* Configure per-mailbox depth limit (soft cap <= queue_capacity) */
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    result<void, error_code> set_mailbox_depth(task_id_t task_id, size_t depth) noexcept {
        task_mailbox* mailbox = find_mailbox(task_id);
        if (mailbox == nullptr) {
            return result<void, error_code>(error_code::not_found);
        }
        const size_t clamped = (depth > queue_capacity) ? queue_capacity : depth;
        mailbox->depth_limit = static_cast<u16>(clamped);
        return ok();
    }

    /* Register task with mailbox */
    result<void, error_code> register_task(task_id_t task_id, os::task_handle_t handle = nullptr) noexcept {
        // Ensure vector index equals task_id for O(1) lookup in find_mailbox()
        const size_t idx = static_cast<size_t>(task_id.value());
        if (idx >= MaxTasks) {
            return result<void, error_code>(error_code::out_of_memory);
        }

        // Expand vector up to task_id + 1, initializing default mailboxes
        if (mailboxes_.size() <= idx) {
            const size_t prev_size = mailboxes_.size();
            const size_t new_size = idx + 1U;
            // Check capacity before resize
            if (new_size > mailboxes_.capacity()) {
                return result<void, error_code>(error_code::out_of_memory);
            }
            mailboxes_.resize(new_size);
            // Initialize any newly created slots to empty task_id
            for (size_t i = prev_size; i < new_size; ++i) {
                mailboxes_[i].task_id = invalid_task_id;
                mailboxes_[i].handle = nullptr;
            }
        }

        // If already registered at correct index, just update handle
        if (mailboxes_[idx].task_id == task_id) {
            mailboxes_[idx].handle = handle;
            return ok();
        }

        // Register mailbox at index == task_id
        mailboxes_[idx].task_id = task_id;
        mailboxes_[idx].handle = handle;
        return ok();
    }

    /* Configure per-mailbox overflow policy */
    result<void, error_code> set_overflow_policy(task_id_t task_id, bool drop_oldest) noexcept {
        task_mailbox* mailbox = find_mailbox(task_id);
        if (mailbox == nullptr) {
            return result<void, error_code>(error_code::not_found);
        }
        mailbox->overflow_drop_oldest = drop_oldest;
        return ok();
    }

    /* Configure global notify policy for all mailboxes */
    result<void, error_code> set_notify_on_empty_only(bool enabled) noexcept {
        for (auto& mailbox : mailboxes_) {
            if (mailbox.task_id != invalid_task_id) {
                mailbox.notify_on_empty_only = enabled;
            }
        }
        return ok();
    }
    
    /* Subscribe task to topic */
    result<void, error_code> subscribe(topic_id_t topic_id, task_id_t subscriber_task_id) noexcept override {
        task_id_t task_id = subscriber_task_id;
        /* Find or create topic */
        topic_subscription* topic = find_topic(topic_id.value);
        if (topic == nullptr) {
            if (topics_.full()) {
                return result<void, error_code>(error_code::out_of_memory);
            }
            // Insert in sorted order to maintain binary search invariant
            auto insert_pos = etl::upper_bound(topics_.begin(), topics_.end(), topic_id.value,
                [](u16 topic_identifier, const topic_subscription& topic) {
                    return topic_identifier < topic.topic_id;
                });
            topic = &(*topics_.insert(insert_pos, topic_subscription(topic_id.value)));
        }
        
        /* Add subscriber */
        if (topic->subscriber_ids.size() >= topic->capacity_limit) {
            return result<void, error_code>(error_code::out_of_memory);
        }
        
        /* Check if already subscribed */
        for (task_id_t subscriber_id : topic->subscriber_ids) {
            if (subscriber_id == task_id) {
                return ok();
            }
        }
        
        topic->subscriber_ids.push_back(task_id);
        return ok();
    }
    
    /* Publish message to topic */
    result<void, error_code> publish(u16 topic_id, MessageType& msg, task_id_t from_task_id) noexcept override {
        msg.header.sender_id = from_task_id.value();
        // Only set timestamp if producer hasn't set it (allows end-to-end latency measurement)
        if (msg.header.timestamp == 0) {
            msg.header.timestamp = os::time_us();
        }
        if (msg.header.sequence_number == 0) {
            msg.header.sequence_number = sequence_++;
        }
        msg.header.type = topic_id;
        
        topic_subscription* topic = find_topic(topic_id);
        if (topic == nullptr || topic->subscriber_ids.empty()) {
            return result<void, error_code>(error_code::not_found);
        }
        
        /* Send to all subscribers */
        bool sent_any = false;
        for (task_id_t subscriber_id : topic->subscriber_ids) {
            task_mailbox* mailbox = find_mailbox(subscriber_id);
            if (mailbox != nullptr) {
                auto send_result = mailbox->send(msg);
                if (send_result.is_ok()) {
                    sent_count_++;
                    sent_any = true;
                } else {
                    dropped_count_++;
                }
            }
        }
        
        return sent_any ? ok() : result<void, error_code>(error_code::out_of_memory);
    }
    
    /* Receive message (blocking) */
    result<MessageType, error_code> receive(task_id_t task_id, timeout_ms_t timeout) noexcept override {
        u32 timeout_ms = timeout.value;
        task_mailbox* mailbox = find_mailbox(task_id);
        if (mailbox == nullptr) {
            return result<MessageType, error_code>(error_code::not_found);
        }
        
        /* Try immediate receive */
        auto receive_result = mailbox->receive();
        if (receive_result.is_ok()) {
            received_count_++;
            return receive_result;
        }
        
        /* Wait for notification */
        u32 notification = 0;
        if (os::wait_notification(timeout_ms, &notification) && ((notification & 0x01) != 0)) {
            receive_result = mailbox->receive();
            if (receive_result.is_ok()) {
                received_count_++;
                return receive_result;
            }
        }
        
        return result<MessageType, error_code>(error_code::timeout);
    }
    
    /* Try receive (non-blocking) */
    result<MessageType, error_code> try_receive(task_id_t task_id) noexcept override {
        task_mailbox* mailbox = find_mailbox(task_id);
        if (mailbox == nullptr) {
            return result<MessageType, error_code>(error_code::not_found);
        }
        
        auto receive_result = mailbox->receive();
        if (receive_result.is_ok()) {
            received_count_++;
            return receive_result;
        }
        
        return result<MessageType, error_code>(error_code::not_found);
    }
    
    /* Broadcast to all tasks */
    result<void, error_code> broadcast(const MessageType& msg) noexcept {
        /* Send to all subscribers */
        bool sent_any = false;
        for (auto& mailbox : mailboxes_) {
            if (mailbox.task_id != invalid_task_id) {
                auto send_result = mailbox.send(msg);
                if (send_result.is_ok()) {
                    sent_count_++;
                    sent_any = true;
                } else {
                    dropped_count_++;
                }
            }
        }
        return sent_any ? ok() : result<void, error_code>(error_code::not_found);
    }
    
    /* Statistics */
    [[nodiscard]] u32 total_sent() const noexcept { return sent_count_; }
    [[nodiscard]] u32 total_received() const noexcept { return received_count_; }
    [[nodiscard]] u32 total_dropped() const noexcept { return dropped_count_; }
    [[nodiscard]] size_t mailbox_count() const noexcept { return mailboxes_.size(); }

    /* Configure per-topic subscriber capacity (soft cap) */
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    result<void, error_code> set_topic_capacity(u16 topic_id, size_t max_subs) noexcept {
        topic_subscription* topic = find_topic(topic_id);
        if (topic == nullptr) {
            // Create topic with default capacity if it doesn't exist yet
            if (topics_.full()) {
                return result<void, error_code>(error_code::out_of_memory);
            }
            auto insert_pos = etl::upper_bound(topics_.begin(), topics_.end(), topic_id,
                [](u16 topic_identifier, const topic_subscription& topic) {
                    return topic_identifier < topic.topic_id;
                });
            topic = &(*topics_.insert(insert_pos, topic_subscription(topic_id)));
        }
        size_t clamped = max_subs > max_subscribers_per_topic ? max_subscribers_per_topic : max_subs;
        topic->capacity_limit = static_cast<u16>(clamped);
        return ok();
    }
};

/* Pool-backed unique_ptr utilities for broker and other messaging objects */
template <typename T>
struct pool_deleter {
    emCore::memory_manager* manager{nullptr};
    void operator()(T* ptr) const noexcept {
        if (ptr != nullptr) {
            ptr->~T();
            if (manager != nullptr) {
                (void)manager->deallocate(static_cast<void*>(ptr));
            }
        }
    }
};

template <typename MessageT>
using broker_uptr = etl::unique_ptr<Ibroker<MessageT>, pool_deleter<Ibroker<MessageT>>>;

template <typename T, typename... Args>
etl::unique_ptr<T, pool_deleter<T>> make_pool_unique(emCore::memory_manager& memory_mgr, Args&&... args) {
    void* mem = memory_mgr.allocate(sizeof(T));
    if (mem == nullptr) {
        return etl::unique_ptr<T, pool_deleter<T>>{};
    }
    return etl::unique_ptr<T, pool_deleter<T>>(new (mem) T(std::forward<Args>(args)...), pool_deleter<T>{&memory_mgr});
}

/* Convenience factory: create a concrete message_broker but return as Ibroker unique_ptr */
template <typename MessageType = medium_message, size_t MaxTasks = config::max_tasks, typename... Args>
etl::unique_ptr<Ibroker<MessageType>, pool_deleter<Ibroker<MessageType>>>
make_message_broker(emCore::memory_manager& memory_mgr, Args&&... args) {
    using broker_t = message_broker<MessageType, MaxTasks>;
    void* mem = memory_mgr.allocate(sizeof(broker_t));
    if (mem == nullptr) {
        return etl::unique_ptr<Ibroker<MessageType>, pool_deleter<Ibroker<MessageType>>>{};
    }
    return etl::unique_ptr<Ibroker<MessageType>, pool_deleter<Ibroker<MessageType>>>(new (mem) broker_t(std::forward<Args>(args)...), pool_deleter<Ibroker<MessageType>>{&memory_mgr});
}


}  // namespace emCore::messaging
