#pragma once

#include <cstddef>

#include "../core/types.hpp"
#include "../error/result.hpp"
#include "../platform/platform.hpp"

#include <etl/circular_buffer.h>


namespace emCore::messaging {

/**
 * @brief Thread-safe message queue with blocking operations
 * Uses FreeRTOS notifications for zero-CPU-usage blocking
 * Uses etl::circular_buffer for FIFO message storage
 */
template<typename MessageType, size_t QueueSize = 16>
class message_queue {
private:
    etl::circular_buffer<MessageType, QueueSize> queue_;
    platform::task_handle_t owner_handle_{nullptr};
    u16 owner_id_{0};
    u32 dropped_messages_{0};
    u32 received_messages_{0};
    
    mutable platform::critical_section critical_section_;
    
public:
    explicit message_queue(u16 owner_id) noexcept 
        : queue_(), owner_id_(owner_id) {}
    
    
    /* Send message (non-blocking) - MPSC safe */
    result<void, error_code> send(const MessageType& msg) noexcept {
        /* Enter critical section for thread safety */
        critical_section_.enter();
        
        if (queue_.full()) {
            critical_section_.exit();
            dropped_messages_++;
            return result<void, error_code>(error_code::out_of_memory);
        }
        
        queue_.push(msg);
        critical_section_.exit();
        
        /* Notify waiting task (outside critical section) */
        if (owner_handle_ != nullptr) {
            platform::notify_task(owner_handle_, 0x01);
        }
        return ok();
    }
    
    /* Receive message (non-blocking) - Single consumer */
    result<MessageType, error_code> receive() noexcept {
        critical_section_.enter();
        
        if (queue_.empty()) {
            critical_section_.exit();
            return result<MessageType, error_code>(error_code::not_found);
        }
        
        MessageType msg = queue_.front();
        queue_.pop();
        received_messages_++;
        
        bool is_empty = queue_.empty();
        critical_section_.exit();
        
        /* Clear notification if queue empty (outside critical section) */
        if (is_empty) {
            platform::clear_notification();
        }
        
        return result<MessageType, error_code>(msg);
    }
    
    /* Receive message (blocking with timeout) */
    result<MessageType, error_code> receive_wait(u32 timeout_ms) noexcept {
        /* Fast path: message already available */
        if (!queue_.empty()) {
            return receive();
        }
        
        /* Wait for notification */
        u32 notification_value = 0;
        bool notified = platform::wait_notification(timeout_ms, &notification_value);
        
        if (!notified || (notification_value & 0x01) == 0) {
            return result<MessageType, error_code>(error_code::timeout);
        }
        
        /* Try to receive after notification */
        return receive();
    }
    
    /* Peek at next message without removing */
    result<MessageType, error_code> peek() const noexcept {
        if (queue_.empty()) {
            return result<MessageType, error_code>(error_code::not_found);
        }
        return result<MessageType, error_code>(queue_.top());
    }
    
    /* Queue status */
    [[nodiscard]] bool empty() const noexcept { return queue_.empty(); }
    [[nodiscard]] bool full() const noexcept { return queue_.full(); }
    [[nodiscard]] size_t size() const noexcept { return queue_.size(); }
    [[nodiscard]] size_t capacity() const noexcept { return QueueSize; }
    [[nodiscard]] u32 dropped_count() const noexcept { return dropped_messages_; }
    [[nodiscard]] u32 received_count() const noexcept { return received_messages_; }
    
    /* Clear all messages */
    void clear() noexcept {
        while (!queue_.empty()) {
            queue_.pop();
        }
        platform::clear_notification();
    }
};

}  // namespace emCore::messaging

