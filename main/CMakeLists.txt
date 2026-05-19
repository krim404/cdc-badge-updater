idf_component_register(
    SRCS "updater_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        freertos
        nvs_flash
        esp_hw_support
        esp_timer
        driver
        cdc_log_uart
        cdc_hal
        cdc_core
        libtropic_sdk
        tropic_fw_blob
        CalEPD
)
