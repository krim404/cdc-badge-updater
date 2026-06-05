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
    TROPIC01_FW_UPDATE_PARAM_ERR,
    TROPIC01_FW_UPDATE_BUS_FAILED,
    TROPIC01_FW_UPDATE_SESSION_FAILED,             /**< Secure session (pairing key) failed. */
    TROPIC01_FW_UPDATE_MAINTENANCE_NOT_ALLOWED,    /**< I-Config forbids Maintenance Mode (OTP). */
    TROPIC01_FW_UPDATE_RCONFIG_FAILED,             /**< R-Config read/erase/write failed. */
    TROPIC01_FW_UPDATE_REBOOT_MAINTENANCE_FAILED,  /**< Could not enter Maintenance Mode. */
    TROPIC01_FW_UPDATE_BOOTLOADER_MISMATCH,        /**< Chip bootloader != embedded blob target. */
    TROPIC01_FW_UPDATE_FAILED,                     /**< lt_do_mutable_fw_update() failed. */
    TROPIC01_FW_UPDATE_MAINTENANCE_DISABLE_FAILED, /**< Could not disable / still reachable. */
    TROPIC01_FW_UPDATE_REBOOT_APP_FAILED
} tropic01_fw_update_result_t;

/**
 * \brief Performs the hardened TROPIC01 firmware update + Maintenance-Mode lockdown.
 *
 * Mitigates security advisory ODR_TR01_SA_2026012900 (laser-fault-injection FW
 * verification bypass that starts from Maintenance Mode). Sequence (bus held):
 *   1. Start secure session with the configured pairing key.
 *   2. Read I-Config[CFG_START_UP] (READ-ONLY): abort if Maintenance Mode is not
 *      permitted at all (OTP).
 *   3. If Maintenance Mode is disabled in R-Config, enable it transiently
 *      (read-whole / erase / set-bit / write-whole / reboot).
 *   4. Safety gate: reboot to Maintenance Mode, read the bootloader version and
 *      abort BEFORE any write if it does not match the embedded blobs' target.
 *   5. lt_do_mutable_fw_update(): update BOTH FW bank pairs (RISC-V + SPECT),
 *      internally version-validated, then reboot to Application Mode.
 *   6. Disable Maintenance Mode in R-Config and verify it is no longer reachable
 *      (lt_reboot(maintenance) must return LT_L2_RESP_DISABLED).
 *
 * I-Config is never written. Only R-Config[CFG_START_UP] is toggled; pairing
 * keys, ECC and R-Memory slots are not touched here. On a failure after step 4,
 * Maintenance Mode is left enabled so the update can be retried.
 *
 * Pre-conditions:
 *   - getSecureElementInstance()->init() must have returned true.
 *   - Caller has VERIFIED stable power (battery >= 3.6V or USB connected).
 *   - Caller disabled any watchdog and holds off all other SPI users.
 *
 * \param fw_cpu              RISC-V FW blob (signed_chunks format).
 * \param fw_cpu_size         Size of RISC-V FW blob in bytes.
 * \param fw_spect            SPECT FW blob (signed_chunks format).
 * \param fw_spect_size       Size of SPECT FW blob in bytes.
 * \param expected_boot_major Expected chip bootloader major version (safety gate).
 * \param expected_boot_minor Expected chip bootloader minor version.
 * \param expected_boot_patch Expected chip bootloader patch version.
 * \param lt_ret_out          Optional output for the last libtropic return code. May be NULL.
 * \return TROPIC01_FW_UPDATE_OK on success, otherwise a specific failure code.
 *
 * \warning NOT interruptible. Power loss or reset during the critical phase can
 *          brick the TROPIC01.
 */
tropic01_fw_update_result_t tropic01_perform_fw_update(const uint8_t *fw_cpu, uint16_t fw_cpu_size,
                                                       const uint8_t *fw_spect, uint16_t fw_spect_size,
                                                       uint8_t expected_boot_major,
                                                       uint8_t expected_boot_minor,
                                                       uint8_t expected_boot_patch,
                                                       int *lt_ret_out);

#ifdef __cplusplus
}
#endif
