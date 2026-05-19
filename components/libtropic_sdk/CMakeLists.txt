# Wrapper component that builds libtropic sources without invoking its upstream CMake.
# This avoids script-mode issues during ESP-IDF component requirement scanning.

set(LIBTROPIC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../third_party/libtropic")

set(LIBTROPIC_SRCS
    "${LIBTROPIC_ROOT}/src/libtropic.c"
    "${LIBTROPIC_ROOT}/src/libtropic_default_sh0_keys.c"
    "${LIBTROPIC_ROOT}/src/libtropic_l2.c"
    "${LIBTROPIC_ROOT}/src/libtropic_l3.c"
    "${LIBTROPIC_ROOT}/src/lt_asn1_der.c"
    "${LIBTROPIC_ROOT}/src/lt_crc16.c"
    "${LIBTROPIC_ROOT}/src/lt_hkdf.c"
    "${LIBTROPIC_ROOT}/src/lt_l1.c"
    "${LIBTROPIC_ROOT}/src/lt_l1_port_wrap.c"
    "${LIBTROPIC_ROOT}/src/lt_l2_frame_check.c"
    "${LIBTROPIC_ROOT}/src/lt_l3_process.c"
    "${LIBTROPIC_ROOT}/src/lt_random.c"
    "${LIBTROPIC_ROOT}/src/lt_secure_memzero.c"
    "${LIBTROPIC_ROOT}/src/lt_tr01_attrs.c"
)

set(LIBTROPIC_CAL_SRCS
    "${LIBTROPIC_ROOT}/cal/mbedtls_v4/lt_mbedtls_v4_aesgcm.c"
    "${LIBTROPIC_ROOT}/cal/mbedtls_v4/lt_mbedtls_v4_common.c"
    "${LIBTROPIC_ROOT}/cal/mbedtls_v4/lt_mbedtls_v4_hmac_sha256.c"
    "${LIBTROPIC_ROOT}/cal/mbedtls_v4/lt_mbedtls_v4_sha256.c"
    "${LIBTROPIC_ROOT}/cal/mbedtls_v4/lt_mbedtls_v4_x25519.c"
)

idf_component_register(
    SRCS ${LIBTROPIC_SRCS} ${LIBTROPIC_CAL_SRCS}
    INCLUDE_DIRS
        "${LIBTROPIC_ROOT}/include"
        "${LIBTROPIC_ROOT}/src"
        "${LIBTROPIC_ROOT}/cal/mbedtls_v4"
    REQUIRES mbedtls
)

# TROPIC01 silicon revision (ACAB = production silicon)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ACAB
)

# libtropic logging: keep ERROR on so brick-relevant failures still surface,
# disable DEBUG so the AES-GCM workaround trace (lt_aesgcm_decrypt parameter
# dump) does not flood the serial console once the session is working.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LT_LOG_ENABLE_ERROR=1
    LT_LOG_ENABLE_WARN=0
    LT_LOG_ENABLE_INFO=0
    LT_LOG_ENABLE_DEBUG=0
)
