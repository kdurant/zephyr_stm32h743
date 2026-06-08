# 驱动
```bash 
sudo apt-get install libusb-1.0-0-dev
sudo apt-get install libhidapi-dev

# 启动 OpenOCD 调试服务器，并建立 PC 与 STM32H7 系列目标芯片之间的通信链路
# -f interface/cmsis-dap.cfg, 告诉 OpenOCD 使用 CMSIS-DAP 协议的调试适配器
# -f target/stm32h7x.cfg, 告诉 OpenOCD 目标芯片的型号是 STM32H7 系列
openocd -f interface/cmsis-dap.cfg  -f target/stm32h7x.cfg
```


# 设备连接
USB-typeC连接电脑和开发板，电脑上显示为*QinHeng Electronics WCH-Link*，对应串口为*/dev/ttyACM0*。
```bash 
wj@wj:$ lsusb 
Bus 006 Device 001: ID 1d6b:0003 Linux Foundation 3.0 root hub
Bus 005 Device 004: ID 0b95:1790 ASIX Electronics Corp. AX88179 Gigabit Ethernet
Bus 005 Device 005: ID 1a86:8012 QinHeng Electronics WCH-Link


wj@wj:$ ls /dev/ttyACM0
/dev/ttyACM0
```


# 下载程序 


