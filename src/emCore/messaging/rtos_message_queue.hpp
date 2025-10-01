#pragma once

#include "../core/types.hpp"
#include "../platform/platform.hpp"
#include "../error/result.hpp"

#include <etl/circular_buffer.h>
#include <etl/array.h>
#include <etl/algorithm.h>
#include <cstddef>


namespace emCore::messaging {

/**
 * @brief High-performance RTOS message queue
 * Optimized for embedded systems with zero-copy and non-blocking operations
 */

/**
 * @brief Message queue configuration for RTOS optimization
 */
struct rtos_queue_config {
    size_t queue_size{32};           // Number of messages
    bool zero_copy_mode{true};       // Use memory pool for zero-copy
    bool non_blocking_send{true};    // Never block on send
    bool priority_queue{false};      // Priority-based ordering
    duration_t max_wait_us{1000};    // Max wait time for receive
};

/**
 * @brief Zero-copy message wrapper
 */
template<typename MessageType>
struct message_wrapper {
    MessageType* message_ptr{nullptr};
    timestamp_t timestamp{0};
    u8 priority{0};
    bool is_valid{false};
    
    message_wrapper() noexcept = default;
    
    explicit message_wrapper(MessageType* ptr, u8 prio = 0) noexcept 
        : message_ptr(ptr), timestamp(platform::get_system_time_us()), 
          priority(prio), is_valid(ptr != nullptr) {}
    
    MessageType* get() noexcept { return message_ptr; }
    const MessageType* get() const noexcept { return message_ptr; }
    
    void release() noexcept {
        message_ptr = nullptr;
        is_valid = false;
    }
};

/**
 * @brief RTOS-optimized message queue
 */
template<typename MessageType, size_t QueueSize = 32>
class rtos_message_queue {
private:
    using message_wrapper_t = message_wrapper<MessageType>;
    
    etl::circular_buffer<message_wrapper_t, QueueSize> queue_;
    rtos_queue_config config_;
    
    // RTOS synchronization (platform-agnostic)
    mutable platform::critical_section critical_section_;
    platform::semaphore_handle_t send_semaphore_{nullptr};
    platform::semaphore_handle_t receive_semaphore_{nullptr};
    
    // Statistics
    u32 messages_sent_{0};
    u32 messages_received_{0};
    u32 messages_dropped_{0};
    u32 peak_queue_size_{0};
    
public:
    explicit rtos_message_queue(const rtos_queue_config& config = {}) noexcept 
        : config_(config),
          send_semaphore_(platform::create_binary_semaphore()),
          receive_semaphore_(platform::create_binary_semaphore()) {
    }
    
    // Rule of Five: Delete copy/move operations for RTOS resources
    rtos_message_queue(const rtos_message_queue&) = delete;
    rtos_message_queue& operator=(const rtos_message_queue&) = delete;
    rtos_message_queue(rtos_message_queue&&) = delete;
    rtos_message_queue& operator=(rtos_message_queue&&) = delete;
    
    ~rtos_message_queue() noexcept {
        // Clean up platform-agnostic semaphores
        platform::delete_semaphore(send_semaphore_);
        platform::delete_semaphore(receive_semaphore_);
    }
    
    /**
     * @brief Non-blocking send (RTOS-safe)
     */
    result<void, error_code> send_nonblocking(MessageType* message, u8 priority = 0) noexcept {
        if (message == nullptr) {
            return result<void, error_code>(error_code::invalid_parameter);
        }
        
        critical_section_.enter();
        
        if (queue_.full()) {
            critical_section_.exit();
            messages_dropped_++;
            return result<void, error_code>(error_code::out_of_memory);
        }
        
        message_wrapper_t wrapper(message, priority);
        
        if (config_.priority_queue) {
            // Insert based on priority (higher priority first)
            auto iter = queue_.begin();
            while (iter != queue_.end() && iter->priority >= priority) {
                ++iter;
            }
            queue_.insert(iter, wrapper);
        } else {
            // FIFO insertion
            queue_.push_back(wrapper);
        }
        
        messages_sent_++;
        peak_queue_size_ = etl::max(peak_queue_size_, static_cast<u32>(queue_.size()));
        
        // Signal waiting receivers (platform-agnostic)
        platform::semaphore_give(receive_semaphore_);
        
        critical_section_.exit();
        
        return ok();
    }
    
    /**
     * @brief Non-blocking receive with timeout (RTOS-safe)
     */
    result<message_wrapper_t, error_code> receive_nonblocking(duration_t timeout_us = 0) noexcept {
        message_wrapper_t msg_wrapper;
        
        // Quick check without blocking
        critical_section_.enter();
        
        if (!queue_.empty()) {
            msg_wrapper = queue_.front();
            queue_.pop();
            messages_received_++;
            critical_section_.exit();
            return result<message_wrapper_t, error_code>(msg_wrapper);
        }
        
        critical_section_.exit();
        
        // If no message and timeout requested, wait
        if (timeout_us > 0) {
            if (platform::semaphore_take(receive_semaphore_, timeout_us)) {
                // Try again after semaphore signal
                critical_section_.enter();
                if (!queue_.empty()) {
                    msg_wrapper = queue_.front();
                    queue_.pop();
                    messages_received_++;
                    critical_section_.exit();
                    return result<message_wrapper_t, error_code>(msg_wrapper);
                }
                critical_section_.exit();
            } else {
                // Fallback: busy wait for systems without semaphore support
                timestamp_t start = platform::get_system_time_us();
                while ((platform::get_system_time_us() - start) < timeout_us) {
                    auto retry_result = receive_nonblocking(0); // Try again
                    if (retry_result.is_ok()) {
                        return retry_result;
                    }
                    platform::delay_ms(1); // Small delay
                }
            }
        }
        
        return result<message_wrapper_t, error_code>(error_code::not_found);
    }
    
    /**
     * @brief Check if queue is empty (RTOS-safe)
     */
    bool empty() const noexcept {
        critical_section_.enter();
        bool is_empty = queue_.empty();
        critical_section_.exit();
        return is_empty;
    }
    
    /**
     * @brief Get current queue size (RTOS-safe)
     */
    size_t size() const noexcept {
        critical_section_.enter();
        size_t current_size = queue_.size();
        critical_section_.exit();
        return current_size;
    }
    
    /**
     * @brief Get queue statistics
     */
    struct queue_stats {
        u32 messages_sent;
        u32 messages_received;
        u32 messages_dropped;
        u32 peak_queue_size;
        u32 current_queue_size;
        f32 drop_rate_percent;
    };
    
    queue_stats get_statistics() const noexcept {
        queue_stats stats;
        stats.messages_sent = messages_sent_;
        stats.messages_received = messages_received_;
        stats.messages_dropped = messages_dropped_;
        stats.peak_queue_size = peak_queue_size_;
        stats.current_queue_size = static_cast<u32>(size());
        
        if (messages_sent_ > 0) {
            stats.drop_rate_percent = static_cast<f32>(messages_dropped_ * 100) / static_cast<f32>(messages_sent_);
        } else {
            stats.drop_rate_percent = 0.0F;
        }
        
        return stats;
    }
    
    /**
     * @brief Clear all messages (RTOS-safe)
     */
    void clear() noexcept {
        critical_section_.enter();
        queue_.clear();
        critical_section_.exit();
    }
};

/**
 * @brief Memory pool for zero-copy messaging
 */
template<typename MessageType, size_t PoolSize = 64>
class message_memory_pool {
private:
    struct pool_entry {
        MessageType message;
        bool is_allocated{false};
    };
    
    etl::array<pool_entry, PoolSize> pool_;
    size_t next_index_{0};
    
    mutable platform::critical_section critical_section_;
    
public:
    /**
     * @brief Allocate message from pool (zero-copy)
     */
    result<MessageType*, error_code> allocate() noexcept {
        critical_section_.enter();
        
        // Find free slot
        for (size_t i = 0; i < PoolSize; ++i) {
            size_t index = (next_index_ + i) % PoolSize;
            if (!pool_[index].is_allocated) {
                pool_[index].is_allocated = true;
                MessageType* message_ptr = &pool_[index].message;
                next_index_ = (index + 1) % PoolSize;
                critical_section_.exit();
                return result<MessageType*, error_code>(message_ptr);
            }
        }
        
        critical_section_.exit();
        
        return result<MessageType*, error_code>(error_code::out_of_memory);
    }
    
    /**
     * @brief Release message back to pool
     */
    result<void, error_code> release(MessageType* message) noexcept {
        if (message == nullptr) {
            return result<void, error_code>(error_code::invalid_parameter);
        }
        
        critical_section_.enter();
        
        // Find and release the message
        for (auto& entry : pool_) {
            if (&entry.message == message) {
                entry.is_allocated = false;
                critical_section_.exit();
                return ok();
            }
        }
        
        critical_section_.exit();
        return result<void, error_code>(error_code::not_found);
    }
    
    /**
     * @brief Get pool utilization statistics
     */
    struct pool_stats {
        size_t total_slots;
        size_t allocated_slots;
        size_t free_slots;
        f32 utilization_percent;
    };
    
    pool_stats get_statistics() const noexcept {
        critical_section_.enter();
        
        size_t allocated = 0;
        for (const auto& entry : pool_) {
            if (entry.is_allocated) {
                allocated++;
            }
        }
        
        critical_section_.exit();
        
        pool_stats stats;
        stats.total_slots = PoolSize;
        stats.allocated_slots = allocated;
        stats.free_slots = PoolSize - allocated;
        stats.utilization_percent = static_cast<f32>(allocated * 100) / static_cast<f32>(PoolSize);
        
        return stats;
    }
};

} // namespace emCore::messaging

