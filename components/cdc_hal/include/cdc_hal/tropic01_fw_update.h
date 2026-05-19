#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Result code of \ref tropic01_perform_fw_update.
 */
typedef enum {
    TROPIC01_FW_UPDATE_OK = 0,
    TROPIC01_FW_UPDATE_BUS_FAILED,
    TROPIC01_FW_UPDATE_REBOOT_MAINTENANCE_FAILED,
    TROPIC01_FW_UPDATE_CPU_FAILED,
    TROPIC01_FW_UPDATE_SPECT_FAILED,
    TROPIC01_FW_UPDATE_REBOOT_APP_FAILED,
    TROPIC01_FW_UPDATE_PARAM_ERR
} tropic01_fw_update_result_t;

/**
 * \brief Performs the full TROPIC01 firmware update sequence.
 *
 * Sequence (ACAB silicon):
 *   1. End any active session.
 *   2. Acquire shared SPI bus.
 *   3. lt_reboot(TR01_MAINTENANCE_REBOOT).
 *   4. lt_do_mutable_fw_update(fw_cpu)   - RISC-V FW.
 *   5. lt_do_mutable_fw_update(fw_spect) - SPECT FW.
 *   6. lt_reboot(TR01_REBOOT).
 *   7. Release SPI bus.
 *
 * Pre-conditions:
 *   - getSecureElementInstance()->init() must have returned true.
 *   - Caller has VERIFIED stable power (battery >= 3.6V or USB connected).
 *
 * \param fw_cpu        Pointer to RISC-V FW blob (signed_chunks format).
 * \param fw_cpu_size   Size of RISC-V FW blob in bytes.
 * \param fw_spect      Pointer to SPECT FW blob (signed_chunks format).
 * \param fw_spect_size Size of SPECT FW blob in bytes.
 * \param lt_ret_out    Optional output for the last libtropic return code.
 *                      May be NULL.
 * \return TROPIC01_FW_UPDATE_OK on success, otherwise a specific failure code.
 *
 * \warning This call is NOT interruptible. Power loss or reset during the
 *          critical phase can brick the TROPIC01. Caller must disable any
 *          watchdog and ensure no other SPI users contend for the bus.
 */
tropic01_fw_update_result_t tropic01_perform_fw_update(const uint8_t *fw_cpu, uint16_t fw_cpu_size,
                                                       const uint8_t *fw_spect, uint16_t fw_spect_size,
                                                       int *lt_ret_out);

#ifdef __cplusplus
}
#endif
