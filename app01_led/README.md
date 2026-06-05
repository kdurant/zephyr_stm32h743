# app01_led

STM32H743 LED闪烁的Zephyr示例应用。

## LED定义

- `led0` 在自定义开发板 `stm32h743_user` 中定义
- 引脚: `PB3`
- 电气特性: `IO=0 -> LED亮`, `IO=1 -> LED灭`

## 开发板文件说明

自定义开发板配置文件位于 `/home/wj/study/stm32h743_user/boards/arm/stm32h743_user/` 目录下，各文件作用如下：

### board.yml
开发板元数据配置文件，定义了开发板的基本信息：
- 开发板名称：stm32h743_user
- 完整名称：STM32H743 User Board
- 厂商：wj
- SoC型号：stm32h743xx

### stm32h743_user.yaml
开发板硬件规格描述文件，包含：
- 架构类型：ARM
- 工具链支持：zephyr、gnuarmemb
- 内存配置：RAM 512KB，Flash 2048KB
- 支持的驱动：GPIO、串口、看门狗

### stm32h743_user.dts
设备树源文件，定义硬件资源配置：
- 系统时钟配置（HSE 25MHz，PLL倍频到240MHz）
- USART1串口配置（PA9/TX, PA10/RX，波特率115200）
- LED0 GPIO配置（PB3，低电平有效）
- 电源管理配置（LDO模式）
- 控制台和Shell UART绑定到USART1

### stm32h743_user_defconfig
开发板默认内核配置，启用基础功能：
- MPU（内存保护单元）
- 硬件栈保护
- 串口驱动
- 控制台
- GPIO驱动

### Kconfig.defconfig
开发板特定的Kconfig默认值配置，设置GPIO默认为启用状态。

### Kconfig.stm32h743_user
开发板Kconfig选项定义，声明BOARD_STM32H743_USER配置项并选择对应的SoC。

## VSCode 配置说明

### 已配置的扩展
项目已推荐以下VSCode扩展（`.vscode/extensions.json`）：
- **ms-vscode.cpptools** - C/C++语言支持
- **ms-vscode.cpptools-extension-pack** - C/C++扩展包
- **zephyrproject-rtos.zephyr** - Zephyr RTOS支持

### IntelliSense 配置
已配置 `.vscode/c_cpp_properties.json` 和 `.vscode/settings.json`：
- 使用 `compile_commands.json` 提供准确的包含路径和宏定义
- 编译器路径指向 Zephyr SDK (`/home/wj/program/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc`)
- C标准为 C17
- 添加了必要的宏定义：`__ZEPHYR__`, `KERNEL`, `STM32H743xx`, `CORE_CM7` 等

### ⚠️ 重要提示

如果代码中仍然出现红色波浪线或跳转不工作：

1. **重新加载窗口**：按 `Ctrl+Shift+P` → 输入 "Reload Window"
2. **等待索引完成**：查看右下角状态栏，等待 "Parsing..." 完成
3. **重新编译项目**（如果修改了配置）：
   ```bash
   cd ~/study/stm32h743_user/app01_led
   west build -b stm32h743_user
   ```
4. **检查C/C++扩展是否安装**：在扩展商店搜索并安装推荐的扩展

### 常见问题

**问题1**: F12无法跳转
- 解决：确保已重新加载窗口，并等待IntelliSense索引完成

**问题2**: 头文件找不到
- 解决：重新编译项目以更新 `compile_commands.json`

**问题3**: 宏定义报错
- 解决：检查 `c_cpp_properties.json` 中的 `defines` 是否包含所需宏

## 编译

```bash
source ~/program/zephyrproject/.venv_zephyr/bin/activate
export ZEPHYR_BASE=~/program/zephyrproject/zephyr

cd ~/study/stm32h743_user/app01_led
west build -b stm32h743_user
```

如果你想在命令行中显式指定开发板根目录:

```bash
west build -b stm32h743_user -- -DBOARD_ROOT=..
```

## 烧录

```bash
west flash
```