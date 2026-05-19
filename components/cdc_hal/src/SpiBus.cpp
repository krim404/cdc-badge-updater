/**
 * Shared SPI Bus for Display + TROPIC01
 * Both devices share SPI2_HOST with manual chip-select control
 */

#include "cdc_hal/ISpiBus.h"
#include "cdc_hal/hw_config.h"
#include "cdc_log.h"
#include <atomic>

static const char* TAG = "SpiBus";

namespace cdc::hal {

/** \brief Shared SPI host selection for ESP32-S3 user peripherals. */
static constexpr spi_host_device_t SPI_BUS_HOST = SPI2_HOST;
static constexpr uint32_t SPI_DMA_CHAN = SPI_DMA_CH_AUTO;

static std::atomic<bool> g_spiInitialized{false};

/**
 * \brief Returns shared SPI host identifier.
 * \return SPI host used by shared bus.
 */
spi_host_device_t getSharedSpiHost() {
    return SPI_BUS_HOST;
}

/**
 * \brief Initializes shared SPI bus once for all SPI peripherals.
 * \return ESP-IDF error code.
 */
esp_err_t initSharedSpiBus() {
    // Already initialized?
    if (g_spiInitialized.load()) {
        return ESP_OK;
    }

    // Configure SPI bus
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = SPI_MOSI_PIN;
    buscfg.miso_io_num = SPI_MISO_PIN;
    buscfg.sclk_io_num = SPI_SCLK_PIN;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4096;
    buscfg.flags = SPICOMMON_BUSFLAG_MASTER;

    esp_err_t err = spi_bus_initialize(SPI_BUS_HOST, &buscfg, SPI_DMA_CHAN);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        LOG_E(TAG, "SPI bus init failed: %d", err);
        return err;
    }

    if (err == ESP_ERR_INVALID_STATE) {
        // Already initialized (by CalEPD most likely)
        LOG_I(TAG, "SPI bus already initialized (by display)");
    } else {
        LOG_I(TAG, "SPI bus initialized (MOSI=%d, MISO=%d, CLK=%d)",
                 SPI_MOSI_PIN, SPI_MISO_PIN, SPI_SCLK_PIN);
    }

    g_spiInitialized.store(true);
    return ESP_OK;
}

} // namespace cdc::hal
