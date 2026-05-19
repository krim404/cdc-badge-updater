#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Embedded TROPIC01 RISC-V CPU firmware blob.
 *
 * Bytes from third_party/libtropic/TROPIC01_fw_update_files/boot_v_2_0_1/fw_v_2_0_0/fw_CPU.h.
 * Defined once in tropic_fw_blob.c.
 */
extern const uint8_t fw_CPU[];
extern const size_t tropic_fw_cpu_size;

/**
 * \brief Embedded TROPIC01 SPECT firmware blob.
 *
 * Bytes from third_party/libtropic/TROPIC01_fw_update_files/boot_v_2_0_1/fw_v_2_0_0/fw_SPECT.h.
 * Defined once in tropic_fw_blob.c.
 */
extern const uint8_t fw_SPECT[];
extern const size_t tropic_fw_spect_size;

#ifdef __cplusplus
}
#endif
