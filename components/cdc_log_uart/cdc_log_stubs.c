#include "cdc_log.h"

static log_level_t s_level = CDC_LOG_LEVEL_VERBOSE;

void log_init(void) {
    esp_log_level_set("*", ESP_LOG_VERBOSE);
}

void log_set_level(log_level_t level) {
    s_level = level;
    esp_log_level_t esp_level = ESP_LOG_VERBOSE;
    switch (level) {
        case CDC_LOG_LEVEL_NONE:    esp_level = ESP_LOG_NONE;    break;
        case CDC_LOG_LEVEL_ERROR:   esp_level = ESP_LOG_ERROR;   break;
        case CDC_LOG_LEVEL_WARN:    esp_level = ESP_LOG_WARN;    break;
        case CDC_LOG_LEVEL_INFO:    esp_level = ESP_LOG_INFO;    break;
        case CDC_LOG_LEVEL_DEBUG:   esp_level = ESP_LOG_DEBUG;   break;
        case CDC_LOG_LEVEL_VERBOSE: esp_level = ESP_LOG_VERBOSE; break;
    }
    esp_log_level_set("*", esp_level);
}

log_level_t log_get_level(void) {
    return s_level;
}

void console_flush(void) {
    fflush(stdout);
    fflush(stderr);
}
