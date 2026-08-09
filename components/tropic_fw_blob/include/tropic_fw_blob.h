#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Embedded TROPIC01 RISC-V CPU firmware blob and its version.
 *
 * Bytes from third_party/libtropic/TROPIC01_fw_update_files/boot_v_2_0_1/fw_v_2_1_0/fw_CPU.h.
 * Defined once in tropic_fw_blob.c. Version layout: {reserved, patch, minor, major}.
 */
extern const uint8_t fw_CPU[];
extern const size_t tropic_fw_cpu_size;
extern const uint8_t fw_CPU_ver[4];

/**
 * \brief Embedded TROPIC01 SPECT firmware blob and its version.
 *
 * Bytes from third_party/libtropic/TROPIC01_fw_update_files/boot_v_2_0_1/fw_v_2_1_0/fw_SPECT.h.
 * Defined once in tropic_fw_blob.c. Version layout: {reserved, patch, minor, major}.
 */
extern const uint8_t fw_SPECT[];
extern const size_t tropic_fw_spect_size;
extern const uint8_t fw_SPECT_ver[4];

#ifdef __cplusplus
}
#endif
