/**
 * \file
 * \brief Implementation of the SystemLock lockdown latch.
 */

#include "cdc_core/SystemLock.h"
#include "cdc_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "SysLock";

namespace cdc::core {

/**
 * \brief Returns the process-wide lockdown latch singleton.
 */
SystemLock& SystemLock::instance() {
    static SystemLock s_instance;
    return s_instance;
}

/**
 * \brief Latches the lockdown flag. Idempotent.
 * \param reason Reason recorded only on the first call.
 * \param detail Optional static-lifetime string with extra context.
 */
void SystemLock::triggerLockdown(LockdownReason reason, const char* detail) {
    bool expected = false;
    if (locked_.compare_exchange_strong(expected, true,
                                        std::memory_order_acq_rel,
                                        std::memory_order_acquire)) {
        reason_.store(reason, std::memory_order_release);
        detail_.store(detail, std::memory_order_release);
        LOG_E(TAG, "LOCKDOWN triggered (reason=%u, detail=%s)",
              static_cast<unsigned>(reason), detail ? detail : "(none)");
    }
}

/**
 * \brief Stores the UI shutdown handler pointer.
 * \param handler Handler invoked once from main context before deep sleep.
 */
void SystemLock::setShutdownHandler(ShutdownHandler handler) {
    handler_.store(handler, std::memory_order_release);
}

/**
 * \brief Main-loop poll. Runs the shutdown sequence if the latch is set.
 */
void SystemLock::enforceIfLocked() {
    if (!locked_.load(std::memory_order_acquire)) {
        return;
    }
    performShutdown();
}

/**
 * \brief Final shutdown: handler, log flush, then deep sleep without wakes.
 */
[[noreturn]] void SystemLock::performShutdown() {
    LockdownReason reason = reason_.load(std::memory_order_acquire);
    LOG_E(TAG, "Entering hardware lockdown - reason=%u", static_cast<unsigned>(reason));

    const char* detail = detail_.load(std::memory_order_acquire);
    ShutdownHandler handler = handler_.load(std::memory_order_acquire);
    if (handler) {
        handler(reason, detail);
    }

    // Give the UI handler time to drive a full e-paper refresh (~1 s) and the
    // CDC / UART transports time to drain the lockdown log entries before
    // power is cut. Other tasks see isLocked()==true and are expected to
    // drop any new input events for the duration.
    console_flush();
    vTaskDelay(pdMS_TO_TICKS(2000));
    console_flush();

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_deep_sleep_start();

    // Defensive: deep sleep does not return.
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

} // namespace cdc::core
