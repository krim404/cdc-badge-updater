idf_component_register(
    SRCS "tropic_fw_blob.c"
    INCLUDE_DIRS "include"
)

# Target bootloader of the embedded blobs. The RISC-V and SPECT FW versions come
# from fw_CPU_ver / fw_SPECT_ver in the blob headers.
target_compile_definitions(${COMPONENT_LIB} INTERFACE
    TROPIC_FW_BOOT_VERSION="2.0.1"
)
