#pragma once

#include "driver/spi_master.h"
#include "esp_err.h"

namespace cdc::hal {

/**
 * Shared SPI host for Display + Secure Element
 * The SPI bus is initialized once and shared between devices
 */

// Get the shared SPI host (initialized by CalEPD or first user)
spi_host_device_t getSharedSpiHost();

// Initialize shared SPI bus if not already done
esp_err_t initSharedSpiBus();

} // namespace cdc::hal
