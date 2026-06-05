# app01_led

STM32H743 LED闪烁的Zephyr示例应用。

## LED定义

- `led0` 在自定义开发板 `stm32h743_user` 中定义
- 引脚: `PB3`
- 电气特性: `IO=0 -> LED亮`, `IO=1 -> LED灭`

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