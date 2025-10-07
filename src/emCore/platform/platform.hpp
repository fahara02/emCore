#pragma once

#if 1 // EMCORE platform facade (new)

#include "platform_base.hpp"
#include <cstdio>

// Auto-detect platform if the build system did not provide one
#if !defined(EMCORE_PLATFORM_ESP32) && \
    !defined(EMCORE_PLATFORM_ARDUINO) && \
    !defined(EMCORE_PLATFORM_STM32) && \
    !defined(EMCORE_PLATFORM_POSIX) && \
    !defined(EMCORE_PLATFORM_GENERIC)
  #if defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
    #define EMCORE_PLATFORM_ESP32
  #elif defined(ARDUINO)
    #define EMCORE_PLATFORM_ARDUINO
  #elif defined(__unix__) || defined(__APPLE__)
    #define EMCORE_PLATFORM_POSIX
  #else
    #define EMCORE_PLATFORM_GENERIC
  #endif
#endif

#if defined(EMCORE_PLATFORM_ESP32)
#  include "impl_esp32.hpp"
   namespace emCore::platform { namespace impl = emCore::platform::impl_esp32; }
#elif defined(EMCORE_PLATFORM_ARDUINO)
#  if defined(__has_include)
#    if __has_include(<Arduino.h>)
#      include "impl_arduino.hpp"
       namespace emCore::platform { namespace impl = emCore::platform::impl_arduino; }
#    else
#      include "impl_generic.hpp"
       namespace emCore::platform { namespace impl = emCore::platform::impl_generic; }
#    endif
#  else
#    include "impl_arduino.hpp"
     namespace emCore::platform { namespace impl = emCore::platform::impl_arduino; }
#  endif
#elif defined(EMCORE_PLATFORM_STM32)
#  include "impl_stm32.hpp"
   namespace emCore::platform { namespace impl = emCore::platform::impl_stm32; }
#elif defined(EMCORE_PLATFORM_POSIX)
#  include "impl_posix.hpp"
   namespace emCore::platform { namespace impl = emCore::platform::impl_posix; }
#else
#  include "impl_generic.hpp"
   namespace emCore::platform { namespace impl = emCore::platform::impl_generic; }
#endif

namespace emCore::platform {

// Re-export primitives and functions from selected impl
using critical_section = impl::critical_section;

inline timestamp_t get_system_time_us() noexcept { return impl::get_system_time_us(); }
inline timestamp_t get_system_time() noexcept { return impl::get_system_time(); }
inline void delay_ms(duration_t milliseconds) noexcept { impl::delay_ms(milliseconds); }
inline void delay_us(u32 microseconds) noexcept { impl::delay_us(microseconds); }

inline void system_reset() noexcept { impl::system_reset(); }
inline void task_yield() noexcept { impl::task_yield(); }
inline size_t get_stack_high_water_mark() noexcept { return impl::get_stack_high_water_mark(); }

// Types come from platform_base.hpp
inline bool create_native_task(const task_create_params& params) noexcept { return impl::create_native_task(params); }
inline bool delete_native_task(task_handle_t handle) noexcept { return impl::delete_native_task(handle); }
inline bool suspend_native_task(task_handle_t handle) noexcept { return impl::suspend_native_task(handle); }
inline bool resume_native_task(task_handle_t handle) noexcept { return impl::resume_native_task(handle); }
inline task_handle_t get_current_task_handle() noexcept { return impl::get_current_task_handle(); }

inline bool notify_task(task_handle_t handle, u32 value = 0x01) noexcept { return impl::notify_task(handle, value); }
inline bool wait_notification(u32 timeout_ms, u32* out_value) noexcept { return impl::wait_notification(timeout_ms, out_value); }
inline void clear_notification() noexcept { impl::clear_notification(); }

using semaphore_handle_t = decltype(impl::create_binary_semaphore());
inline semaphore_handle_t create_binary_semaphore() noexcept { return impl::create_binary_semaphore(); }
inline void delete_semaphore(semaphore_handle_t handle) noexcept { impl::delete_semaphore(handle); }
inline bool semaphore_give(semaphore_handle_t handle) noexcept { return impl::semaphore_give(handle); }
inline bool semaphore_take(semaphore_handle_t handle, duration_t timeout_us) noexcept { return impl::semaphore_take(handle, timeout_us); }

constexpr platform_info get_platform_info() noexcept { return impl::get_platform_info(); }

// Centralized logging
#ifndef EMCORE_ENABLE_LOGGING
#define EMCORE_ENABLE_LOGGING 1 /* NOLINT(cppcoreguidelines-macro-usage) */
#endif

namespace detail {
inline void log_sink(const char* msg) noexcept {
#if defined(EMCORE_PLATFORM_ARDUINO)
    if (msg) { Serial.println(msg); }
#elif defined(EMCORE_PLATFORM_ESP32)
    if (msg) { std::printf("%s\n", msg); }
#elif defined(EMCORE_PLATFORM_POSIX)
    if (msg) { std::puts(msg); }
#elif defined(EMCORE_PLATFORM_STM32)
    (void)msg; // Optionally add ITM here if CMSIS is included
#else
    (void)msg;
#endif
}
} // namespace detail

inline void log(const char* message) noexcept {
#if EMCORE_ENABLE_LOGGING
    detail::log_sink(message);
#else
    (void)message;
#endif
}

inline void logf(const char* fmt, u32 arg1) noexcept {
#if EMCORE_ENABLE_LOGGING
    char buffer[256]; std::snprintf(buffer, sizeof(buffer), fmt, arg1); /* NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay) */ log(buffer);
#else
    (void)fmt; (void)arg1;
#endif
}
inline void logf(const char* fmt, u32 arg1, u32 arg2) noexcept { char buffer[256]; std::snprintf(buffer, sizeof(buffer), fmt, arg1, arg2); /* NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay) */ log(buffer); }
inline void logf(const char* fmt, u32 arg1, u32 arg2, u32 arg3) noexcept { char buffer[256]; std::snprintf(buffer, sizeof(buffer), fmt, arg1, arg2, arg3); /* NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay) */ log(buffer); }
inline void logf(const char* fmt, u32 arg1, u32 arg2, u32 arg3, u32 arg4) noexcept { char buffer[256]; std::snprintf(buffer, sizeof(buffer), fmt, arg1, arg2, arg3, arg4); /* NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay) */ log(buffer); }
inline void logf(const char* fmt, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5) noexcept { char buffer[256]; std::snprintf(buffer, sizeof(buffer), fmt, arg1, arg2, arg3, arg4, arg5); /* NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay) */ log(buffer); }

} // namespace emCore::platform

#endif // EMCORE platform facade (new)

#if 0 // Legacy block disabled
#include "../core/types.hpp"
#include <cstdio>  // For snprintf

// Platform detection and selection
// Define one of these before including emCore, or let auto-detection work

// Auto-detect platform
#if !defined(EMCORE_PLATFORM_ESP32) && !defined(EMCORE_PLATFORM_ARDUINO) && \
    !defined(EMCORE_PLATFORM_STM32) && !defined(EMCORE_PLATFORM_GENERIC)
    
    #if defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
        #define EMCORE_PLATFORM_ESP32
    #elif defined(ARDUINO)
        #define EMCORE_PLATFORM_ARDUINO
    #elif defined(STM32) || defined(STM32F4) || defined(STM32F7) || defined(STM32H7)
        #define EMCORE_PLATFORM_STM32
    #else
        #define EMCORE_PLATFORM_GENERIC
    #endif
#endif

// Include platform-specific headers
#if defined(ARDUINO)
    // Arduino framework (includes ESP32-Arduino)
    #include <Arduino.h>
#elif defined(EMCORE_PLATFORM_ESP32)
    // ESP-IDF (native ESP32)
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
    #include <esp_timer.h>
    #include <rom/ets_sys.h>
#elif defined(EMCORE_PLATFORM_STM32)
    #ifdef __cplusplus
    extern "C" {
    #endif
    #include "cmsis_os.h"
    #include "stm32_hal.h"
    #ifdef __cplusplus
    }
    #endif
#endif

// FreeRTOS functions are already declared by Arduino.h for ESP32
// No need to forward declare them

namespace emCore::platform {

// Platform-specific time function with microsecond precision (returns microseconds)
inline timestamp_t get_system_time_us() noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        // ESP32-Arduino: Use esp_timer directly (64-bit)
        return static_cast<timestamp_t>(esp_timer_get_time());
        
    #elif defined(ARDUINO)
        // Other Arduino platforms: Use micros() 
        return static_cast<timestamp_t>(micros());
        
    #elif defined(EMCORE_PLATFORM_ESP32)
        // ESP-IDF native: Use esp_timer directly (64-bit)
        return static_cast<timestamp_t>(esp_timer_get_time());
        
    #elif defined(EMCORE_PLATFORM_STM32)
        // STM32: Use DWT cycle counter for microsecond precision
        static bool dwt_initialized = false;
        if (!dwt_initialized) {
            // Enable DWT cycle counter
            CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
            DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
            DWT->CYCCNT = 0;
            dwt_initialized = true;
        }
        
        // Convert CPU cycles to microseconds
        uint32_t cycles = DWT->CYCCNT;
        return static_cast<timestamp_t>((uint64_t)cycles * 1000000ULL / SystemCoreClock);
        
    #else
        // Generic platform: Use simple counter (placeholder)
        static timestamp_t counter = 0;
        return ++counter;
    #endif
}

// Platform-specific time function (returns milliseconds for compatibility)
inline timestamp_t get_system_time() noexcept {
    return get_system_time_us() / 1000;
}

// Global logging control - can be set via build flags
#ifndef EMCORE_ENABLE_LOGGING
#define EMCORE_ENABLE_LOGGING 1  // Default enabled, disable via build flag
#endif

// Platform-specific logging function
inline void log(const char* message) noexcept {
    #if EMCORE_ENABLE_LOGGING
        #if defined(ARDUINO)
            // Arduino: Use Serial
            Serial.println(message);
        #elif defined(EMCORE_PLATFORM_STM32)
            // STM32: Use ITM (Instrumentation Trace Macrocell) for debug output
            if ((ITM->TCR & ITM_TCR_ITMENA_Msk) && (ITM->TER & 1UL)) {
                // ITM is enabled, send via ITM port 0
                const char* ptr = message;
                while (*ptr) {
                    ITM_SendChar(*ptr++);
                }
                ITM_SendChar('\n');
            }
        #else
            // Generic: No logging (placeholder)
            (void)message;
        #endif
    #else
        // Logging disabled
        (void)message;
    #endif
}

// Platform-specific formatted logging functions
inline void logf(const char* format, u32 arg1) noexcept {
    #if EMCORE_ENABLE_LOGGING
        #if defined(ARDUINO)
            char buffer[256];
            snprintf(buffer, sizeof(buffer), format, arg1);
            Serial.println(buffer);
        #elif defined(EMCORE_PLATFORM_STM32)
            char buffer[256];
            snprintf(buffer, sizeof(buffer), format, arg1);
            log(buffer);
        #else
            (void)format; (void)arg1;
        #endif
    #else
        (void)format; (void)arg1;
    #endif
}

inline void logf(const char* format, u32 arg1, u32 arg2) noexcept {
    #if defined(ARDUINO)
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, arg1, arg2);
        Serial.println(buffer);
    #elif defined(EMCORE_PLATFORM_STM32)
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, arg1, arg2);
        log(buffer);
    #else
        (void)format; (void)arg1; (void)arg2;
    #endif
}

inline void logf(const char* format, u32 arg1, u32 arg2, u32 arg3) noexcept {
    #if defined(ARDUINO)
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, arg1, arg2, arg3);
        Serial.println(buffer);
    #elif defined(EMCORE_PLATFORM_STM32)
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, arg1, arg2, arg3);
        log(buffer);
    #else
        (void)format; (void)arg1; (void)arg2; (void)arg3;
    #endif
}

inline void logf(const char* format, u32 arg1, u32 arg2, u32 arg3, u32 arg4) noexcept {
    #if defined(ARDUINO)
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, arg1, arg2, arg3, arg4);
        Serial.println(buffer);
    #elif defined(EMCORE_PLATFORM_STM32)
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, arg1, arg2, arg3, arg4);
        log(buffer);
    #else
        (void)format; (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    #endif
}

inline void logf(const char* format, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5) noexcept {
    #if defined(ARDUINO)
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, arg1, arg2, arg3, arg4, arg5);
        Serial.println(buffer);
    #elif defined(EMCORE_PLATFORM_STM32)
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, arg1, arg2, arg3, arg4, arg5);
        log(buffer);
    #else
        (void)format; (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    #endif
}

// Platform-specific delay function
inline void delay_ms(duration_t milliseconds) noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        // ESP32-Arduino: Use FreeRTOS vTaskDelay for task-aware delays
        // Convert milliseconds to ticks (with CONFIG_FREERTOS_HZ=1000, 1 tick = 1ms)
        const unsigned long ticks = milliseconds / portTICK_PERIOD_MS;
        ::vTaskDelay(ticks > 0 ? ticks : 1);
        
    #elif defined(ARDUINO)
        // Other Arduino platforms: Use delay()
        delay(static_cast<unsigned long>(milliseconds));
        
    #elif defined(EMCORE_PLATFORM_ESP32)
        // ESP-IDF native: Use FreeRTOS vTaskDelay
        vTaskDelay(pdMS_TO_TICKS(milliseconds));
        
    #elif defined(EMCORE_PLATFORM_STM32)
        // STM32 CMSIS-RTOS: Use osDelay()
        osDelay(static_cast<uint32_t>(milliseconds));
        
    #else
        // Generic implementation - busy wait
        timestamp_t start = get_system_time();
        while ((get_system_time() - start) < milliseconds) {
            // Busy wait
        }
    #endif
}

// Platform-specific microsecond delay
inline void delay_us(u32 microseconds) noexcept {
    #if defined(ARDUINO)
        // Arduino framework: Use delayMicroseconds()
        delayMicroseconds(static_cast<unsigned int>(microseconds));
        
    #elif defined(EMCORE_PLATFORM_ESP32)
        // ESP-IDF: Use ets_delay_us from ROM
        ets_delay_us(microseconds);
        
    #elif defined(EMCORE_PLATFORM_STM32)
        // STM32: Use DWT cycle counter for precise microsecond delay
        if (microseconds == 0) return;
        
        // Ensure DWT is enabled (should be from get_system_time_us)
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        
        uint32_t start_cycles = DWT->CYCCNT;
        uint32_t target_cycles = (microseconds * SystemCoreClock) / 1000000U;
        
        while ((DWT->CYCCNT - start_cycles) < target_cycles) {
            __asm volatile("nop");
        }
        
    #else
        // Generic implementation
        (void)microseconds;
    #endif
}

// Platform information
struct platform_info {
    const char* name;
    u32 clock_hz;
    bool has_rtos;
};

constexpr platform_info get_platform_info() noexcept {
    #if defined(EMCORE_PLATFORM_ESP32)
        return {"ESP32", 240000000, true};
    #elif defined(EMCORE_PLATFORM_ARDUINO)
        return {"Arduino", 16000000, false};
    #elif defined(EMCORE_PLATFORM_STM32)
        return {"STM32", SystemCoreClock, true};  // Use actual system clock, CMSIS-RTOS enabled
    #else
        return {"Generic", 1000000, false};
    #endif
}

/* Native task creation (FreeRTOS/RTOS wrapper) */
using task_handle_t = void*;
using task_function_t = void (*)(void*);

struct task_create_params {
    task_function_t function{nullptr};
    const char* name{nullptr};
    u32 stack_size{0};
    void* parameters{nullptr};
    u32 priority{0};
    task_handle_t* handle{nullptr};
    bool start_suspended{false};  // Create then suspend immediately if true
    bool pin_to_core{false};
    int core_id{-1};
};

inline bool create_native_task(const task_create_params& params) noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        /* ESP32-Arduino: Create FreeRTOS task */
        BaseType_t result;
        // Convert stack size from bytes (API) to words (FreeRTOS expects words)
        uint32_t depth_words = static_cast<uint32_t>(params.stack_size / sizeof(StackType_t));
        if (depth_words == 0) { depth_words = 1; }
        if (params.pin_to_core && params.core_id >= 0) {
            result = xTaskCreatePinnedToCore(
                params.function,
                params.name,
                depth_words,
                params.parameters,
                params.priority,
                reinterpret_cast<TaskHandle_t*>(params.handle),
                static_cast<BaseType_t>(params.core_id)
            );
        } else {
            result = xTaskCreate(
                params.function,
                params.name,
                depth_words,
                params.parameters,
                params.priority,
                reinterpret_cast<TaskHandle_t*>(params.handle)
            );
        }
        if (result == pdPASS && params.start_suspended && params.handle && *params.handle) {
            vTaskSuspend(static_cast<TaskHandle_t>(*params.handle));
        }
        return (result == pdPASS);
        
    #elif defined(EMCORE_PLATFORM_ESP32)
        /* ESP-IDF native: Create FreeRTOS task */
        BaseType_t result;
        // Convert stack size from bytes (API) to words (FreeRTOS expects words)
        uint32_t depth_words = static_cast<uint32_t>(params.stack_size / sizeof(StackType_t));
        if (depth_words == 0) { depth_words = 1; }
        if (params.pin_to_core && params.core_id >= 0) {
            result = xTaskCreatePinnedToCore(
                params.function,
                params.name,
                depth_words,
                params.parameters,
                params.priority,
                reinterpret_cast<TaskHandle_t*>(params.handle),
                static_cast<BaseType_t>(params.core_id)
            );
        } else {
            result = xTaskCreate(
                params.function,
                params.name,
                depth_words,
                params.parameters,
                params.priority,
                reinterpret_cast<TaskHandle_t*>(params.handle)
            );
        }
        if (result == pdPASS && params.start_suspended && params.handle && *params.handle) {
            vTaskSuspend(static_cast<TaskHandle_t>(*params.handle));
        }
        return (result == pdPASS);
        
    #elif defined(EMCORE_PLATFORM_STM32)
        /* STM32 CMSIS-RTOS: Create thread */
        osThreadDef_t thread_def;
        thread_def.name = const_cast<char*>(params.name);
        thread_def.pthread = reinterpret_cast<os_pthread>(params.function);
        thread_def.tpriority = static_cast<osPriority>(params.priority);
        thread_def.instances = 0;
        thread_def.stacksize = params.stack_size;
        
        osThreadId id = osThreadCreate(&thread_def, params.parameters);
        if (params.handle != nullptr) {
            *params.handle = id;
        }
        return (id != nullptr);
        
    #else
        /* Generic platform: No native task support */
        (void)params;
        return false;
    #endif
}

inline bool delete_native_task(task_handle_t handle) noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        vTaskDelete(reinterpret_cast<TaskHandle_t>(handle));
        return true;
    #elif defined(EMCORE_PLATFORM_ESP32)
        vTaskDelete(reinterpret_cast<TaskHandle_t>(handle));
        return true;
    #elif defined(EMCORE_PLATFORM_STM32)
        osThreadTerminate(reinterpret_cast<osThreadId>(handle));
        return true;
    #else
        (void)handle;
        return false;
    #endif
}

inline bool suspend_native_task(task_handle_t handle) noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        vTaskSuspend(reinterpret_cast<TaskHandle_t>(handle));
        return true;
    #elif defined(EMCORE_PLATFORM_ESP32)
        vTaskSuspend(reinterpret_cast<TaskHandle_t>(handle));
        return true;
    #elif defined(EMCORE_PLATFORM_STM32)
        osThreadSuspend(reinterpret_cast<osThreadId>(handle));
        return true;
    #else
        (void)handle;
        return false;
    #endif
}

inline bool resume_native_task(task_handle_t handle) noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        vTaskResume(reinterpret_cast<TaskHandle_t>(handle));
        return true;
    #elif defined(EMCORE_PLATFORM_ESP32)
        vTaskResume(reinterpret_cast<TaskHandle_t>(handle));
        return true;
    #elif defined(EMCORE_PLATFORM_STM32)
        osThreadResume(reinterpret_cast<osThreadId>(handle));
        return true;
    #else
        (void)handle;
        return false;
    #endif
}

/* Task notification for event-driven messaging */
inline bool notify_task(task_handle_t handle, u32 value = 0x01) noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        if (handle == nullptr) return false;
        xTaskNotify(
            reinterpret_cast<TaskHandle_t>(handle),
            value,
            eSetBits
        );
        return true;
    #elif defined(EMCORE_PLATFORM_ESP32)
        if (handle == nullptr) return false;
        xTaskNotify(
            reinterpret_cast<TaskHandle_t>(handle),
            value,
            eSetBits
        );
        return true;
    #elif defined(EMCORE_PLATFORM_STM32)
        if (handle == nullptr) return false;
        osSignalSet(reinterpret_cast<osThreadId>(handle), value);
        return true;
    #else
        (void)handle;
        (void)value;
        return false;
    #endif
}

// NOLINTNEXTLINE(misc-const-correctness) - out_value is an output parameter
inline bool wait_notification(u32 timeout_ms, u32* out_value) noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        u32 notification_value = 0;
        BaseType_t result = xTaskNotifyWait(
            0x00,      /* Don't clear bits on entry */
            0xFFFFFFFF,/* Clear all bits on exit */
            &notification_value,
            pdMS_TO_TICKS(timeout_ms)
        );
        if (out_value != nullptr) {
            *out_value = notification_value;
        }
        return (result == pdTRUE);
    #elif defined(EMCORE_PLATFORM_ESP32)
        u32 notification_value = 0;
        BaseType_t result = xTaskNotifyWait(
            0x00,
            0xFFFFFFFF,
            &notification_value,
            pdMS_TO_TICKS(timeout_ms)
        );
        if (out_value != nullptr) {
            *out_value = notification_value;
        }
        return (result == pdTRUE);
    #elif defined(EMCORE_PLATFORM_STM32)
        osEvent event = osSignalWait(0x01, timeout_ms);
        if (out_value != nullptr) {
            *out_value = (event.status == osEventSignal) ? event.value.signals : 0;
        }
        return (event.status == osEventSignal);
    #else
        /* Generic: busy wait with polling */
        (void)timeout_ms;
        (void)out_value;
        return false;
    #endif
}

inline void clear_notification() noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        xTaskNotifyStateClear(nullptr);  /* Clear current task */
    #elif defined(EMCORE_PLATFORM_ESP32)
        xTaskNotifyStateClear(nullptr);
    #elif defined(EMCORE_PLATFORM_STM32)
        /* STM32 doesn't need clearing in this model */
    #else
        /* Generic: no-op */
    #endif
}

inline task_handle_t get_current_task_handle() noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        return xTaskGetCurrentTaskHandle();
    #elif defined(EMCORE_PLATFORM_ESP32)
        return xTaskGetCurrentTaskHandle();
    #elif defined(EMCORE_PLATFORM_STM32)
        return reinterpret_cast<task_handle_t>(osThreadGetId());
    #else
        return nullptr;
    #endif
}

inline void resume_task(task_handle_t handle) noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        if (handle != nullptr) {
            vTaskResume(static_cast<TaskHandle_t>(handle));
        }
    #elif defined(EMCORE_PLATFORM_ESP32)
        if (handle != nullptr) {
            vTaskResume(static_cast<TaskHandle_t>(handle));
        }
    #elif defined(EMCORE_PLATFORM_STM32)
        if (handle != nullptr) {
            osThreadResume(reinterpret_cast<osThreadId>(handle));
        }
    #else
        (void)handle;
    #endif
}

// Platform-specific system reset function
inline void system_reset() noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        esp_restart();
    #elif defined(EMCORE_PLATFORM_STM32)
        NVIC_SystemReset();
    #else
        // Generic fallback - hang and trigger hardware watchdog
        while(true) { 
            // Infinite loop to trigger hardware watchdog reset
        }
    #endif
}

// Platform-specific task yield function
inline void task_yield() noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        taskYIELD(); // FreeRTOS yield
    #elif defined(EMCORE_PLATFORM_STM32)
        osThreadYield(); // CMSIS-RTOS yield
    #else
        // Generic fallback - no yield available
    #endif
}

// Platform-specific stack monitoring
inline size_t get_stack_high_water_mark() noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(nullptr);
        return stack_high_water * sizeof(StackType_t);
    #elif defined(EMCORE_PLATFORM_STM32)
        // STM32 CMSIS-RTOS stack monitoring (if available)
        return 0; // Not implemented
    #else
        return 0; // Not available
    #endif
}

// Platform-specific critical section management
struct critical_section {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        mutable portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
    #endif
    
    void enter() const noexcept {
        #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
            portENTER_CRITICAL(&spinlock);
        #elif defined(EMCORE_PLATFORM_STM32)
            __disable_irq();
        #endif
    }
    
    void exit() const noexcept {
        #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
            portEXIT_CRITICAL(&spinlock);
        #elif defined(EMCORE_PLATFORM_STM32)
            __enable_irq();
        #endif
    }
};

// Platform-specific semaphore handle
using semaphore_handle_t = void*;

// Platform-specific semaphore operations
inline semaphore_handle_t create_binary_semaphore() noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        return xSemaphoreCreateBinary();
    #elif defined(EMCORE_PLATFORM_STM32)
        // STM32 CMSIS-RTOS semaphore creation
        return nullptr; // Not implemented
    #else
        return nullptr; // Not available
    #endif
}

inline void delete_semaphore(semaphore_handle_t handle) noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        if (handle != nullptr) {
            vSemaphoreDelete(static_cast<SemaphoreHandle_t>(handle));
        }
    #elif defined(EMCORE_PLATFORM_STM32)
        // STM32 CMSIS-RTOS semaphore deletion
        (void)handle;
    #else
        (void)handle;
    #endif
}

inline bool semaphore_give(semaphore_handle_t handle) noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        if (handle != nullptr) {
            return xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(handle), nullptr) == pdTRUE;
        }
        return false;
    #elif defined(EMCORE_PLATFORM_STM32)
        (void)handle;
        return false; // Not implemented
    #else
        (void)handle;
        return false; // Not available
    #endif
}

inline bool semaphore_take(semaphore_handle_t handle, duration_t timeout_us) noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        if (handle != nullptr) {
            TickType_t ticks = pdMS_TO_TICKS(timeout_us / 1000);
            return xSemaphoreTake(static_cast<SemaphoreHandle_t>(handle), ticks) == pdTRUE;
        }
        return false;
    #elif defined(EMCORE_PLATFORM_STM32)
        (void)handle;
        (void)timeout_us;
        return false; // Not implemented
    #else
        (void)handle;
        return false; // Not available
    #endif
}

} } // namespace emCore::platform
#endif // Legacy block disabled
