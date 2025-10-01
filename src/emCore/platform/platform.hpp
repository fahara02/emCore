#pragma once

#include "../core/types.hpp"

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

// Forward declare FreeRTOS functions (global namespace)
#if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
extern "C" unsigned long xTaskGetTickCount();
extern "C" void vTaskDelay(const unsigned long xTicksToDelay);
#endif

namespace emCore {
namespace platform {

// Platform-specific time function
inline timestamp_t get_system_time() noexcept {
    #if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
        // ESP32-Arduino: Use FreeRTOS xTaskGetTickCount for better accuracy
        // Arduino ESP32 includes FreeRTOS
        // Convert ticks to milliseconds (1000Hz = 1ms per tick with CONFIG_FREERTOS_HZ=1000)
        return static_cast<timestamp_t>(::xTaskGetTickCount() * portTICK_PERIOD_MS);
        
    #elif defined(ARDUINO)
        // Other Arduino platforms: Use millis()
        return static_cast<timestamp_t>(millis());
        
    #elif defined(EMCORE_PLATFORM_ESP32)
        // ESP-IDF native: Use esp_timer for microsecond precision
        return static_cast<timestamp_t>(esp_timer_get_time() / 1000);
        
    #elif defined(EMCORE_PLATFORM_STM32)
        // STM32 CMSIS-RTOS: Use osKernelGetTickCount()
        return static_cast<timestamp_t>(osKernelGetTickCount());
        
    #else
        // Generic/test implementation
        static timestamp_t time_counter = 0;
        return time_counter++;
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
        // STM32: Cycle-accurate delay (approximate)
        volatile u32 count = microseconds * (SystemCoreClock / 1000000U) / 5U;
        while (count--) {
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
        return {"STM32", 168000000, true};  // CMSIS-RTOS enabled
    #else
        return {"Generic", 1000000, false};
    #endif
}

}  // namespace platform
}  // namespace emCore
