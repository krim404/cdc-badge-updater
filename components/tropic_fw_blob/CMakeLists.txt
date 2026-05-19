idf_component_register(
    SRCS "tropic_fw_blob.c"
    INCLUDE_DIRS "include"
)

target_compile_definitions(${COMPONENT_LIB} INTERFACE
    TROPIC_FW_BOOT_VERSION="2.0.1"
    TROPIC_FW_RISCV_VERSION="2.0.0"
    TROPIC_FW_SPECT_VERSION="1.0.0"
)
