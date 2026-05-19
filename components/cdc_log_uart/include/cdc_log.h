/**
 * \file
 * \brief CDC Badge Updater logging shim.
 *
 * Drop-in replacement for the cdc_log API used by the components copied
 * from cdc-badge-os. Routes everything to ESP_LOG which writes to UART0.
 */
#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CDC_LOG_LEVEL_NONE = 0,
    CDC_LOG_LEVEL_ERROR,
    CDC_LOG_LEVEL_WARN,
    CDC_LOG_LEVEL_INFO,
    CDC_LOG_LEVEL_DEBUG,
    CDC_LOG_LEVEL_VERBOSE
} log_level_t;

void log_init(void);
void log_set_level(log_level_t level);
log_level_t log_get_level(void);

/**
 * \brief Flushes the UART0 console. Compatibility shim for cdc-badge-os code
 *        paths that drain CDC/UART before deep sleep.
 */
void console_flush(void);

#ifdef __cplusplus
}
#endif

#define LOG_E(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define LOG_D(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
#define LOG_V(tag, fmt, ...) ESP_LOGV(tag, fmt, ##__VA_ARGS__)

#define LOGE(fmt, ...) ESP_LOGE("APP", fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) ESP_LOGW("APP", fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) ESP_LOGI("APP", fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) ESP_LOGD("APP", fmt, ##__VA_ARGS__)
#define LOGV(fmt, ...) ESP_LOGV("APP", fmt, ##__VA_ARGS__)
