# SPDX-License-Identifier: AGPL-3.0-only
#
# Copyright (c) 2025 Vincent Jardin <vjardin@free.fr>, Free Mobile
#
# CMakeLists.txt for zephyr-softsim module

if(CONFIG_SOFTSIM)

# Find onomondo-uicc library path
# When used as a Zephyr module, it's fetched via west to modules/lib/onomondo-uicc
# The path is relative to the Zephyr workspace root
if(DEFINED ZEPHYR_ONOMONDO_UICC_MODULE_DIR)
    set(ONOMONDO_UICC_DIR ${ZEPHYR_ONOMONDO_UICC_MODULE_DIR})
elseif(EXISTS ${ZEPHYR_BASE}/../modules/lib/onomondo-uicc)
    set(ONOMONDO_UICC_DIR ${ZEPHYR_BASE}/../modules/lib/onomondo-uicc)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../onomondo-uicc)
    set(ONOMONDO_UICC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../onomondo-uicc)
else()
    message(FATAL_ERROR "onomondo-uicc library not found. "
            "Please run 'west update' or set ZEPHYR_ONOMONDO_UICC_MODULE_DIR")
endif()

message(STATUS "Using onomondo-uicc from: ${ONOMONDO_UICC_DIR}")

# Collect onomondo-uicc library sources
set(ONOMONDO_UICC_SOURCES
    # UICC core sources
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/access.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/apdu.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/btlv_enc.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/btlv_dec.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/btlv_utils.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/ctlv.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/command.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/df_name.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/fcp.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/file.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/fs.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/fs_chg.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/fs_utils.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/uicc_lchan.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/sfi.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/sms.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/sw.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/softsim.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/tlv8.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/uicc_admin.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/uicc_auth.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/uicc_cat.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/uicc_sms_rx.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/uicc_sms_tx.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/uicc_remote_cmd.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/uicc_file_ops.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/uicc_pin.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/uicc_refresh.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/utils.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/utils_ota.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/utils_3des.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/utils_aes.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/proactive.c
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc/uicc_suspend.c
    # Crypto sources
    ${ONOMONDO_UICC_DIR}/src/softsim/crypto/aes-encblock.c
    ${ONOMONDO_UICC_DIR}/src/softsim/crypto/aes-internal-dec.c
    ${ONOMONDO_UICC_DIR}/src/softsim/crypto/aes-internal-enc.c
    ${ONOMONDO_UICC_DIR}/src/softsim/crypto/aes-internal.c
    ${ONOMONDO_UICC_DIR}/src/softsim/crypto/aes-wrap.c
    ${ONOMONDO_UICC_DIR}/src/softsim/crypto/des-internal.c
    # Milenage sources
    ${ONOMONDO_UICC_DIR}/src/softsim/milenage/milenage.c
    ${ONOMONDO_UICC_DIR}/src/softsim/milenage/milenage_usim.c
    # Storage abstraction
    ${ONOMONDO_UICC_DIR}/src/softsim/storage.c
    # Static SIM files
    ${ONOMONDO_UICC_DIR}/utils/files-c-array/ss_static_files_hex.c
)

# Zephyr-specific platform sources
set(ZEPHYR_SOFTSIM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fs_zephyr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/log_zephyr.c
)

# Create the library
zephyr_library_named(softsim)

zephyr_library_sources(
    ${ONOMONDO_UICC_SOURCES}
    ${ZEPHYR_SOFTSIM_SOURCES}
)

# Compile definitions
zephyr_library_compile_definitions(
    CONFIG_USE_SYSTEM_HEAP
    CONFIG_ALT_FILE_SEPARATOR
    SS_STORAGE_PATH_DEFAULT="${CONFIG_SOFTSIM_STORAGE_PATH}"
    SS_STORAGE_PATH_MAX=${CONFIG_SOFTSIM_MAX_PATH_LEN}
)

# Include directories
zephyr_library_include_directories(
    ${ONOMONDO_UICC_DIR}/include
    ${ONOMONDO_UICC_DIR}/src/softsim
    ${ONOMONDO_UICC_DIR}/src/softsim/uicc
    ${ONOMONDO_UICC_DIR}/src/softsim/crypto
    ${ONOMONDO_UICC_DIR}/src/softsim/milenage
    ${ONOMONDO_UICC_DIR}/utils/files-c-array
)

# Public includes for application use
zephyr_include_directories(
    ${ONOMONDO_UICC_DIR}/include
    ${ONOMONDO_UICC_DIR}/utils/files-c-array
)

# Public compile definitions for application use
# Required for correct macro expansion (e.g., SS_FREE -> free() vs port_free())
zephyr_compile_definitions(
    CONFIG_USE_SYSTEM_HEAP
)

endif() # CONFIG_SOFTSIM
