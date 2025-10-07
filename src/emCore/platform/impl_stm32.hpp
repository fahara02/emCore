#pragma once

#include "platform_base.hpp"

// Only compile this implementation on STM32 builds
#if defined(EMCORE_PLATFORM_STM32) || defined(STM32) || defined(STM32F4) || defined(STM32F7) || defined(STM32H7)

#include <cstdio>
extern "C" {
#include "cmsis_os2.h"
#include "stm32_hal.h"
}

namespace emCore::platform::impl_stm32 {

struct critical_section {
    void enter() const noexcept { __disable_irq(); }
    void exit() const noexcept { __enable_irq(); }
};

inline timestamp_t get_system_time_us() noexcept {
    static bool dwt_initialized = false;
    if (!dwt_initialized) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        DWT->CYCCNT = 0;
        dwt_initialized = true;
    }
    const uint32_t cycles = DWT->CYCCNT;
    return static_cast<timestamp_t>((static_cast<uint64_t>(cycles) * 1000000ULL) / SystemCoreClock);
}
inline timestamp_t get_system_time() noexcept { return get_system_time_us() / 1000ULL; }

inline void delay_ms(duration_t ms) noexcept { osDelay(static_cast<uint32_t>(ms)); }
inline void delay_us(u32 us) noexcept {
    if (us == 0) return;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    const uint32_t start = DWT->CYCCNT;
    const uint32_t target = (us * SystemCoreClock) / 1000000U;
    while ((DWT->CYCCNT - start) < target) { __NOP(); }
}

/* logging provided centrally by platform.hpp */

inline void system_reset() noexcept { NVIC_SystemReset(); }
inline void task_yield() noexcept { osThreadYield(); }
inline size_t get_stack_high_water_mark() noexcept { return 0; }

using task_handle_t = platform::task_handle_t;
using task_function_t = platform::task_function_t;
using task_create_params = platform::task_create_params;

inline bool create_native_task(const task_create_params& p) noexcept {
    osThreadAttr_t attr{};
    attr.name = p.name;
    attr.priority = static_cast<osPriority_t>(p.priority);
    attr.stack_size = p.stack_size;
    osThreadId_t id = osThreadNew(reinterpret_cast<osThreadFunc_t>(p.function), p.parameters, &attr);
    if (p.handle) { *p.handle = id; }
    if (p.start_suspended && id) { osThreadSuspend(id); }
    // pin_to_core not applicable on STM32 single core
    return id != nullptr;
}
inline bool delete_native_task(task_handle_t h) noexcept { if (!h) return false; osThreadTerminate(reinterpret_cast<osThreadId_t>(h)); return true; }
inline bool suspend_native_task(task_handle_t h) noexcept { if (!h) return false; osThreadSuspend(reinterpret_cast<osThreadId_t>(h)); return true; }
inline bool resume_native_task(task_handle_t h) noexcept { if (!h) return false; osThreadResume(reinterpret_cast<osThreadId_t>(h)); return true; }
inline task_handle_t get_current_task_handle() noexcept { return reinterpret_cast<task_handle_t>(osThreadGetId()); }

inline bool notify_task(task_handle_t h, u32 value) noexcept {
    if (!h) return false; return osThreadFlagsSet(reinterpret_cast<osThreadId_t>(h), value) != 0U;
}
inline bool wait_notification(u32 timeout_ms, u32* out) noexcept {
    uint32_t flags = osThreadFlagsWait(0xFFFFFFFFU, osFlagsWaitAny, timeout_ms);
    if (out) { *out = (flags & 0x80000000U) ? 0U : flags; }
    return (flags & 0x80000000U) == 0U; // osFlagsError* encoded as high bits
}
inline void clear_notification() noexcept { (void)osThreadFlagsClear(0xFFFFFFFFU); }

using semaphore_handle_t = osSemaphoreId_t;
inline semaphore_handle_t create_binary_semaphore() noexcept { return osSemaphoreNew(1U, 0U, nullptr); }
inline void delete_semaphore(semaphore_handle_t sem) noexcept { if (sem) { osSemaphoreDelete(sem); } }
inline bool semaphore_give(semaphore_handle_t sem) noexcept { return sem && (osSemaphoreRelease(sem) == osOK); }
inline bool semaphore_take(semaphore_handle_t sem, duration_t timeout_us) noexcept {
    const uint32_t ms = static_cast<uint32_t>(timeout_us / 1000U);
    // Convert ms to ticks for CMSIS v2 if needed
    const uint32_t ticks = (ms * osKernelGetTickFreq()) / 1000U;
    return sem && (osSemaphoreAcquire(sem, ticks ? ticks : 1U) == osOK);
}

inline constexpr platform_info get_platform_info() noexcept { return {"STM32", static_cast<u32>(SystemCoreClock), true}; }

} // namespace emCore::platform::impl_stm32

#endif // EMCORE_PLATFORM_STM32
