# SPDX-License-Identifier: Apache-2.0
#
# Sysbuild entry point for app95_bootloader.
# Sets up custom board root and MCUboot overlay, then delegates to standard sysbuild.

# Custom board root (stm32h743_user is out-of-tree)
list(APPEND BOARD_ROOT ${APP_DIR}/..)

# Pass partition overlay to MCUboot sub-image build
set(mcuboot_EXTRA_DTC_OVERLAY_FILE
    ${APP_DIR}/mcuboot/boards/stm32h743_user.overlay
    CACHE STRING "MCUboot extra DTC overlay file"
)

# Standard sysbuild entry (same as template/CMakeLists.txt)
find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sysbuild LANGUAGES)
