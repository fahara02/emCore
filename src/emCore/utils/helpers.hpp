#pragma once

#include "../core/types.hpp"

namespace emCore {
    namespace utils {
        
        /**
         * @brief Simple assert macro for embedded systems
         */
        #ifdef EMCORE_DEBUG
            #define EMCORE_ASSERT(condition) \
                do { \
                    if (!(condition)) { \
                        /* Platform-specific assert handling */ \
                        while(1); /* Halt execution */ \
                    } \
                } while(0)
        #else
            #define EMCORE_ASSERT(condition) ((void)0)
        #endif
        
        /**
         * @brief Debug logging (platform-specific implementation needed)
         */
        #ifdef EMCORE_DEBUG
            void debug_log(const char* message) noexcept;
            void debug_log_format(const char* format, ...) noexcept;
        #else
            inline void debug_log(const char*) noexcept {}
            inline void debug_log_format(const char*, ...) noexcept {}
        #endif
        
        /**
         * @brief Constexpr min function
         */
        template<typename T>
        constexpr T min(const T& a, const T& b) noexcept {
            return (a < b) ? a : b;
        }
        
        /**
         * @brief Constexpr max function
         */
        template<typename T>
        constexpr T max(const T& a, const T& b) noexcept {
            return (a > b) ? a : b;
        }
        
        /**
         * @brief Constexpr clamp function
         */
        template<typename T>
        constexpr T clamp(const T& value, const T& min_val, const T& max_val) noexcept {
            return min(max(value, min_val), max_val);
        }
        
        /**
         * @brief Bit manipulation utilities
         */
        template<typename T>
        constexpr bool is_bit_set(T value, u8 bit) noexcept {
            return (value & (T(1) << bit)) != 0;
        }
        
        template<typename T>
        constexpr T set_bit(T value, u8 bit) noexcept {
            return value | (T(1) << bit);
        }
        
        template<typename T>
        constexpr T clear_bit(T value, u8 bit) noexcept {
            return value & ~(T(1) << bit);
        }
        
        template<typename T>
        constexpr T toggle_bit(T value, u8 bit) noexcept {
            return value ^ (T(1) << bit);
        }
        
        /**
         * @brief CRC-8 calculation for data integrity
         */
        class crc8 {
        private:
            static constexpr u8 polynomial = 0x07; // CRC-8-CCITT
            
        public:
            static constexpr u8 calculate(const u8* data, size_t length) noexcept {
                u8 crc = 0x00;
                
                for (size_t i = 0; i < length; ++i) {
                    crc ^= data[i];
                    
                    for (u8 bit = 0; bit < 8; ++bit) {
                        if (crc & 0x80) {
                            crc = (crc << 1) ^ polynomial;
                        } else {
                            crc <<= 1;
                        }
                    }
                }
                
                return crc;
            }
        };
        
        /**
         * @brief Simple ring buffer for embedded systems
         */
        template<typename T, size_t Size>
        class ring_buffer {
        private:
            T buffer_[Size];
            size_t head_;
            size_t tail_;
            size_t count_;
            
        public:
            ring_buffer() noexcept : head_(0), tail_(0), count_(0) {}
            
            bool push(const T& item) noexcept {
                if (is_full()) {
                    return false;
                }
                
                buffer_[head_] = item;
                head_ = (head_ + 1) % Size;
                ++count_;
                return true;
            }
            
            bool pop(T& item) noexcept {
                if (is_empty()) {
                    return false;
                }
                
                item = buffer_[tail_];
                tail_ = (tail_ + 1) % Size;
                --count_;
                return true;
            }
            
            bool peek(T& item) const noexcept {
                if (is_empty()) {
                    return false;
                }
                
                item = buffer_[tail_];
                return true;
            }
            
            void clear() noexcept {
                head_ = 0;
                tail_ = 0;
                count_ = 0;
            }
            
            bool is_empty() const noexcept {
                return count_ == 0;
            }
            
            bool is_full() const noexcept {
                return count_ == Size;
            }
            
            size_t size() const noexcept {
                return count_;
            }
            
            constexpr size_t capacity() const noexcept {
                return Size;
            }
        };
        
        /**
         * @brief Simple state machine base class
         */
        template<typename StateEnum>
        class state_machine {
        private:
            StateEnum current_state_;
            StateEnum previous_state_;
            
        public:
            explicit state_machine(StateEnum initial_state) noexcept 
                : current_state_(initial_state), previous_state_(initial_state) {}
            
            void transition_to(StateEnum new_state) noexcept {
                previous_state_ = current_state_;
                current_state_ = new_state;
            }
            
            StateEnum get_current_state() const noexcept {
                return current_state_;
            }
            
            StateEnum get_previous_state() const noexcept {
                return previous_state_;
            }
            
            bool is_in_state(StateEnum state) const noexcept {
                return current_state_ == state;
            }
            
            bool was_in_state(StateEnum state) const noexcept {
                return previous_state_ == state;
            }
        };
        
        // Note: Platform-specific time functions are in platform/platform.hpp
    // - platform::get_system_time()
    // - platform::delay_ms()
    // - platform::delay_us()
        
    } // namespace utils
} // namespace emCore
