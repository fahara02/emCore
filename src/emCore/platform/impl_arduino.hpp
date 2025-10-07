#pragma once

#include "platform_base.hpp"

// Only compile this implementation when building for Arduino
#if defined(EMCORE_PLATFORM_ARDUINO) || defined(ARDUINO)

#include <Arduino.h>
#include <cstdio>

namespace emCore::platform::impl_arduino {

struct critical_section {
#if defined(ESP32) || defined(ESP_PLATFORM)
    mutable portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
    void enter() const noexcept { portENTER_CRITICAL(&spinlock); }
    void exit() const noexcept { portEXIT_CRITICAL(&spinlock); }
#else
    void enter() const noexcept {}
    void exit() const noexcept {}
#endif
};

inline timestamp_t get_system_time_us() noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    extern "C" uint64_t esp_timer_get_time();
    return static_cast<timestamp_t>(esp_timer_get_time());
#else
    return static_cast<timestamp_t>(micros());
#endif
}
inline timestamp_t get_system_time() noexcept { return get_system_time_us() / 1000ULL; }

inline void delay_ms(duration_t ms) noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    TickType_t ticks = ms / portTICK_PERIOD_MS;
    vTaskDelay(ticks > 0 ? ticks : 1);
#else
    delay(static_cast<unsigned long>(ms));
#endif
}
inline void delay_us(u32 us) noexcept { delayMicroseconds(static_cast<unsigned int>(us)); }

/* logging provided centrally by platform.hpp */

inline void system_reset() noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    extern "C" void esp_restart(); esp_restart();
#else
    NVIC_SystemReset();
#endif
}
inline void task_yield() noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    taskYIELD();
#else
    yield();
#endif
}
inline size_t get_stack_high_water_mark() noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    UBaseType_t marks = uxTaskGetStackHighWaterMark(nullptr);
    return static_cast<size_t>(marks) * sizeof(StackType_t);
#else
    return 0;
#endif
}

using task_handle_t = platform::task_handle_t;
using task_function_t = platform::task_function_t;
using task_create_params = platform::task_create_params;

inline bool create_native_task(const task_create_params& p) noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    if (!p.function) return false;
    BaseType_t result;
    uint32_t depth_words = static_cast<uint32_t>(p.stack_size / sizeof(StackType_t));
    if (depth_words == 0) depth_words = 1;
    if (p.pin_to_core && p.core_id >= 0) {
        result = xTaskCreatePinnedToCore(p.function, p.name, depth_words, p.parameters,
                                         static_cast<UBaseType_t>(p.priority),
                                         reinterpret_cast<TaskHandle_t*>(p.handle), p.core_id);
    } else {
        result = xTaskCreate(p.function, p.name, depth_words, p.parameters,
                             static_cast<UBaseType_t>(p.priority),
                             reinterpret_cast<TaskHandle_t*>(p.handle));
    }
    if (result == pdPASS && p.start_suspended && p.handle && *p.handle) {
        vTaskSuspend(static_cast<TaskHandle_t>(*p.handle));
    }
    return result == pdPASS;
#else
    (void)p; return false;
#endif
}
inline bool delete_native_task(task_handle_t h) noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    if (!h) return false; vTaskDelete(static_cast<TaskHandle_t>(h)); return true;
#else
    (void)h; return false;
#endif
}
inline bool suspend_native_task(task_handle_t h) noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    if (!h) return false; vTaskSuspend(static_cast<TaskHandle_t>(h)); return true;
#else
    (void)h; return false;
#endif
}
inline bool resume_native_task(task_handle_t h) noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    if (!h) return false; vTaskResume(static_cast<TaskHandle_t>(h)); return true;
#else
    (void)h; return false;
#endif
}
inline task_handle_t get_current_task_handle() noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    return xTaskGetCurrentTaskHandle();
#else
    return nullptr;
#endif
}

inline bool notify_task(task_handle_t h, u32 value) noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    if (!h) return false; xTaskNotify(static_cast<TaskHandle_t>(h), value, eSetBits); return true;
#else
    (void)h; (void)value; return false;
#endif
}
inline bool wait_notification(u32 timeout_ms, u32* out) noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    u32 nv = 0; BaseType_t r = xTaskNotifyWait(0x00, 0xFFFFFFFF, &nv, pdMS_TO_TICKS(timeout_ms)); if (out) *out = nv; return r == pdTRUE;
#else
    (void)timeout_ms; (void)out; return false;
#endif
}
inline void clear_notification() noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    xTaskNotifyStateClear(nullptr);
#endif
}

using semaphore_handle_t = void*;
inline semaphore_handle_t create_binary_semaphore() noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    return xSemaphoreCreateBinary();
#else
    return nullptr;
#endif
}
inline void delete_semaphore(semaphore_handle_t h) noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    if (h) vSemaphoreDelete(static_cast<SemaphoreHandle_t>(h));
#else
    (void)h;
#endif
}
inline bool semaphore_give(semaphore_handle_t h) noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    if (!h) return false; return xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(h), nullptr) == pdTRUE;
#else
    (void)h; return false;
#endif
}
inline bool semaphore_take(semaphore_handle_t h, duration_t timeout_us) noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    if (!h) return false; TickType_t t = pdMS_TO_TICKS(timeout_us / 1000); return xSemaphoreTake(static_cast<SemaphoreHandle_t>(h), t) == pdTRUE;
#else
    (void)h; (void)timeout_us; return false;
#endif
}

inline constexpr platform_info get_platform_info() noexcept {
#if defined(ESP32) || defined(ESP_PLATFORM)
    return {"Arduino-ESP32", 240000000U, true};
#else
    return {"Arduino", F_CPU, false};
#endif
}

} // namespace emCore::platform::impl_arduino

#endif // EMCORE_PLATFORM_ARDUINO || ARDUINO
