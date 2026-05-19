#pragma once

#include <cstdint>
#include "esp_err.h"
#include "cdc_hal/ISecureElement.h"

namespace cdc::core {

struct TropicWipeResult {
    uint16_t eccDeleted   = 0;
    uint16_t rmemDeleted  = 0;
    bool     sessionReady = false;
};

/**
 * \brief Iterates every TROPIC01 ECC slot (0..ECC_SLOT_COUNT-1) and R-Memory
 *        slot (0..RMEM_SLOT_COUNT-1), deleting whatever is currently
 *        populated. Sets `sessionReady=false` and returns immediately if no
 *        active SE session is available.
 *
 * \param se Secure element instance.
 * \param progressEvery When non-zero, `onRmemProgress` is invoked every
 *        `progressEvery` R-Memory slots and once on completion. Ignored when
 *        `onRmemProgress` is null.
 * \param onRmemProgress Optional progress callback receiving `(current, total)`.
 * \return Wipe statistics.
 */
TropicWipeResult wipeTropic(hal::ISecureElement* se,
                             uint16_t progressEvery = 0,
                             void (*onRmemProgress)(uint16_t current, uint16_t total) = nullptr);

/**
 * \brief Erases the NVS partition and re-initializes it blank.
 * \return ESP_OK on success, propagated error otherwise.
 */
esp_err_t wipeNvs();

} // namespace cdc::core
