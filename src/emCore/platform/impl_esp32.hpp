#pragma once

#include "platform_base.hpp"

// Only compile this implementation on ESP32 builds
#if defined(EMCORE_PLATFORM_ESP32) || defined(ESP_PLATFORM) || defined(ESP32)

// ESP-IDF headers
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/portmacro.h>
#include <esp_timer.h>
#include <rom/ets_sys.h>

namespace emCore::platform::impl_esp32 {

struct critical_section {
    mutable portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
    void enter() const noexcept { portENTER_CRITICAL(&spinlock); }
    void exit() const noexcept { portEXIT_CRITICAL(&spinlock); }
};

inline timestamp_t get_system_time_us() noexcept { return static_cast<timestamp_t>(esp_timer_get_time()); }
inline timestamp_t get_system_time() noexcept { return get_system_time_us() / 1000ULL; }

inline void delay_ms(duration_t ms) noexcept {
    TickType_t ticks = pdMS_TO_TICKS(ms);
    vTaskDelay(ticks > 0 ? ticks : 1);
}
inline void delay_us(u32 us) noexcept { ets_delay_us(us); }

/* logging provided centrally by platform.hpp */

inline void system_reset() noexcept { esp_restart(); }
inline void task_yield() noexcept { taskYIELD(); }
inline size_t get_stack_high_water_mark() noexcept {
    UBaseType_t marks = uxTaskGetStackHighWaterMark(nullptr);
    return static_cast<size_t>(marks) * sizeof(StackType_t);
}

using task_handle_t = platform::task_handle_t;
using task_function_t = platform::task_function_t;
using task_create_params = platform::task_create_params;

inline bool create_native_task(const task_create_params& p) noexcept {
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
}
inline bool delete_native_task(task_handle_t h) noexcept { if (!h) return false; vTaskDelete(static_cast<TaskHandle_t>(h)); return true; }
inline bool suspend_native_task(task_handle_t h) noexcept { if (!h) return false; vTaskSuspend(static_cast<TaskHandle_t>(h)); return true; }
inline bool resume_native_task(task_handle_t h) noexcept { if (!h) return false; vTaskResume(static_cast<TaskHandle_t>(h)); return true; }
inline task_handle_t get_current_task_handle() noexcept { return xTaskGetCurrentTaskHandle(); }

inline bool notify_task(task_handle_t h, u32 value) noexcept {
    if (!h) return false;
    xTaskNotify(static_cast<TaskHandle_t>(h), value, eSetBits);
    return true;
}
inline bool wait_notification(u32 timeout_ms, u32* out) noexcept {
    u32 nv = 0;
    BaseType_t r = xTaskNotifyWait(0x00, 0xFFFFFFFF, &nv, pdMS_TO_TICKS(timeout_ms));
    if (out) { *out = nv; }
    return r == pdTRUE;
}
inline void clear_notification() noexcept { xTaskNotifyStateClear(nullptr); }

using semaphore_handle_t = void*;
inline semaphore_handle_t create_binary_semaphore() noexcept { return xSemaphoreCreateBinary(); }
inline void delete_semaphore(semaphore_handle_t h) noexcept { if (h) vSemaphoreDelete(static_cast<SemaphoreHandle_t>(h)); }
inline bool semaphore_give(semaphore_handle_t h) noexcept {
    if (!h) return false;
    if (xPortInIsrContext()) {
        BaseType_t hpw = pdFALSE;
        BaseType_t r = xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(h), &hpw);
        if (hpw == pdTRUE) {
            portYIELD_FROM_ISR();
        }
        return r == pdTRUE;
    } else {
        return xSemaphoreGive(static_cast<SemaphoreHandle_t>(h)) == pdTRUE;
    }
}
inline bool semaphore_take(semaphore_handle_t h, duration_t timeout_us) noexcept {
    if (!h) { return false; }
    TickType_t t = pdMS_TO_TICKS(timeout_us / 1000);
    return xSemaphoreTake(static_cast<SemaphoreHandle_t>(h), t) == pdTRUE;
}

inline constexpr platform_info get_platform_info() noexcept { return {"ESP32", 240000000U, true}; }

} // namespace emCore::platform::impl_esp32

#endif // EMCORE_PLATFORM_ESP32 || ESP_PLATFORM || ESP32
