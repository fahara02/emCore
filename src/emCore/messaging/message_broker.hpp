#pragma once

#include <cstddef>

#include "../core/types.hpp"
#include "../core/config.hpp"
#include "../error/result.hpp"
#include "../platform/platform.hpp"
#include "message_types.hpp"

#include <etl/circular_buffer.h>
#include <etl/vector.h>
#include <etl/map.h>
#include <etl/pool.h>



namespace emCore::messaging {

/**
 * @brief Professional message broker with pub/sub
 * Clean implementation that actually works
 */
template<typename MessageType = medium_message, size_t MaxTasks = config::max_tasks>
class message_broker {
private:
    static constexpr size_t queue_capacity = 16;
    static constexpr size_t max_topics = 32;
    static constexpr size_t max_subscribers_per_topic = 8;
    
    /* Task mailbox with MPSC queue */
    struct task_mailbox {
        u16 task_id{0xFFFF};
        etl::circular_buffer<MessageType, queue_capacity> queue;
        platform::task_handle_t handle{nullptr};
        mutable platform::critical_section critical_section;
        
        task_mailbox() = default;
        
        /* Thread-safe send */
        result<void, error_code> send(const MessageType& msg) noexcept {
            critical_section.enter();
            
            if (queue.full()) {
                critical_section.exit();
                return result<void, error_code>(error_code::out_of_memory);
            }
            
            queue.push(msg);
            critical_section.exit();
            
            /* Notify task */
            if (handle != nullptr) {
                platform::notify_task(handle, 0x01);
            }
            
            return ok();
        }
        
        /* Thread-safe receive */
        result<MessageType, error_code> receive() noexcept {
            critical_section.enter();
            
            if (queue.empty()) {
                critical_section.exit();
                return result<MessageType, error_code>(error_code::not_found);
            }
            
            MessageType msg = queue.front();
            queue.pop();
            
            bool is_empty = queue.empty();
            critical_section.exit();
            
            if (is_empty) {
                platform::clear_notification();
            }
            
            return result<MessageType, error_code>(msg);
        }
        
        bool empty() const noexcept {
            return queue.empty();
        }
    };
    
    /* Topic subscription */
    struct topic_subscription {
        u16 topic_id{0xFFFF};
        etl::vector<u16, max_subscribers_per_topic> subscriber_ids;
        
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
    
    /* Find mailbox by task ID - O(1) lookup */
    task_mailbox* find_mailbox(u16 task_id) noexcept {
        // Direct indexing since task_id maps to mailbox index
        if (task_id >= mailboxes_.size()) {
            return nullptr;
        }
        
        auto& mailbox = mailboxes_[task_id];
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
    
    /* Register task with mailbox */
    result<void, error_code> register_task(u16 task_id, platform::task_handle_t handle = nullptr) noexcept {
        if (find_mailbox(task_id) != nullptr) {
            return ok(); /* Already registered */
        }
        
        if (mailboxes_.full()) {
            return result<void, error_code>(error_code::out_of_memory);
        }
        
        /* Resize to add new mailbox in-place */
        size_t idx = mailboxes_.size();
        mailboxes_.resize(idx + 1);
        mailboxes_[idx].task_id = task_id;
        mailboxes_[idx].handle = handle;
        
        return ok();
    }
    
    /* Subscribe task to topic */
    result<void, error_code> subscribe(topic_id_t topic_id, u16 subscriber_task_id) noexcept {
        u16 task_id = subscriber_task_id;
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
        if (topic->subscriber_ids.full()) {
            return result<void, error_code>(error_code::out_of_memory);
        }
        
        /* Check if already subscribed */
        for (u16 subscriber_id : topic->subscriber_ids) {
            if (subscriber_id == task_id) {
                return ok();
            }
        }
        
        topic->subscriber_ids.push_back(task_id);
        return ok();
    }
    
    /* Publish message to topic */
    result<void, error_code> publish(u16 topic_id, MessageType& msg, u16 from_task_id) noexcept {
        msg.header.sender_id = from_task_id;
        msg.header.timestamp = platform::get_system_time_us();
        msg.header.sequence_number = sequence_++;
        msg.header.type = topic_id;
        
        topic_subscription* topic = find_topic(topic_id);
        if (topic == nullptr || topic->subscriber_ids.empty()) {
            return result<void, error_code>(error_code::not_found);
        }
        
        /* Send to all subscribers */
        bool sent_any = false;
        for (u16 subscriber_id : topic->subscriber_ids) {
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
    result<MessageType, error_code> receive(u16 task_id, timeout_ms_t timeout = timeout_ms_t::infinite()) noexcept {
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
        if (platform::wait_notification(timeout_ms, &notification) && ((notification & 0x01) != 0)) {
            receive_result = mailbox->receive();
            if (receive_result.is_ok()) {
                received_count_++;
                return receive_result;
            }
        }
        
        return result<MessageType, error_code>(error_code::timeout);
    }
    
    /* Try receive (non-blocking) */
    result<MessageType, error_code> try_receive(u16 task_id) noexcept {
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
            if (mailbox.task_id != 0xFFFF) {
                if (mailbox.send(msg)) {
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
};

}  // namespace emCore::messaging

