# SPDX-License-Identifier: Apache-2.0

# 使用系统安装的 OpenOCD 脚本路径（SDK 中找不到）
set(OPENOCD_DEFAULT_PATH /usr/share/openocd/scripts)

# CMSIS-DAP 适配器初始化
board_runner_args(openocd
  --target-handle=_CHIPNAME.cpu0
  --cmd-pre-init "adapter driver cmsis-dap"
  --cmd-pre-init "transport select swd"
  --cmd-pre-init "adapter speed 4000"
  --cmd-pre-init "source [find target/stm32h7x.cfg]"
  )

include(${ZEPHYR_BASE}/boards/common/openocd-stm32.board.cmake)
