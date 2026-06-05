
## 开发板文件说明

自定义开发板配置文件位于 `./boards/arm/stm32h743_user/` 目录下，各文件作用如下：

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

