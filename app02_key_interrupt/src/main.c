#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app02_key_interrupt, LOG_LEVEL_INF);

/* 通过别名获取按键节点 */
#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)

#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "No sw0 alias found in device tree"
#endif

#if !DT_NODE_HAS_STATUS(SW1_NODE, okay)
#error "No sw1 alias found in device tree"
#endif

/* 获取按键的 GPIO 规格 */
static const struct gpio_dt_spec key0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static const struct gpio_dt_spec key1 = GPIO_DT_SPEC_GET(SW1_NODE, gpios);

/* 定义回调数据结构 */
static struct gpio_callback key0_cb_data;
static struct gpio_callback key1_cb_data;

/**
 * @brief KEY0 中断回调函数
 */
void key0_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    
    LOG_INF("Interrupt detected: KEY0 (PE3) pressed/released");
}

/**
 * @brief KEY1 中断回调函数
 */
void key1_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    
    LOG_INF("Interrupt detected: KEY1 (PE4) pressed/released");
}

int main(void)
{
    int ret;

    printk("Starting app02_key_interrupt\n");

    /* 检查 KEY0 GPIO 设备是否就绪 */
    if (!gpio_is_ready_dt(&key0)) {
        LOG_ERR("KEY0 GPIO device not ready");
        return -1;
    }

    /* 检查 KEY1 GPIO 设备是否就绪 */
    if (!gpio_is_ready_dt(&key1)) {
        LOG_ERR("KEY1 GPIO device not ready");
        return -1;
    }

    /* 配置 KEY0 为输入模式，带中断触发（下降沿和上升沿） */
    ret = gpio_pin_configure_dt(&key0, GPIO_INPUT | GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        LOG_ERR("Failed to configure KEY0 pin: %d", ret);
        return ret;
    }

    /* 配置 KEY1 为输入模式，带中断触发（下降沿和上升沿） */
    ret = gpio_pin_configure_dt(&key1, GPIO_INPUT | GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        LOG_ERR("Failed to configure KEY1 pin: %d", ret);
        return ret;
    }

    /* 初始化 KEY0 回调数据结构 */
    gpio_init_callback(&key0_cb_data, key0_isr, BIT(key0.pin));
    
    /* 添加 KEY0 回调 */
    ret = gpio_add_callback(key0.port, &key0_cb_data);
    if (ret < 0) {
        LOG_ERR("Failed to add KEY0 callback: %d", ret);
        return ret;
    }

    /* 初始化 KEY1 回调数据结构 */
    gpio_init_callback(&key1_cb_data, key1_isr, BIT(key1.pin));
    
    /* 添加 KEY1 回调 */
    ret = gpio_add_callback(key1.port, &key1_cb_data);
    if (ret < 0) {
        LOG_ERR("Failed to add KEY1 callback: %d", ret);
        return ret;
    }

    /* 使能 KEY0 中断 */
    ret = gpio_pin_interrupt_configure_dt(&key0, GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        LOG_ERR("Failed to configure KEY0 interrupt: %d", ret);
        return ret;
    }

    /* 使能 KEY1 中断 */
    ret = gpio_pin_interrupt_configure_dt(&key1, GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        LOG_ERR("Failed to configure KEY1 interrupt: %d", ret);
        return ret;
    }

    LOG_INF("Key interrupt detection started");
    LOG_INF("Press KEY0 (PE3) or KEY1 (PE4) to trigger interrupts");

    /* 主循环 - 保持运行以接收中断 */
    while (1) {
        k_msleep(1000);
    }

    return 0;
}
