#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app01_led, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "No led0 alias found in device tree"
#endif

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

int main(void)
{
    int ret;

    if(!gpio_is_ready_dt(&led0))
    {
        LOG_ERR("LED0 GPIO device not ready");
        return -1;
    }

    ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT);
    if(ret < 0)
    {
        LOG_ERR("Failed to configure LED0 pin: %d", ret);
        return ret;
    }

    if(!gpio_is_ready_dt(&led1))
    {
        LOG_ERR("LED1 GPIO device not ready");
        return -1;
    }

    ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT);
    if(ret < 0)
    {
        LOG_ERR("Failed to configure LED1 pin: %d", ret);
        return ret;
    }

    LOG_INF("Start blinking LED0 on PB3 (IO=0 on, IO=1 off)");
    LOG_INF("Start blinking LED1 on PB4 (IO=0 on, IO=1 off)");

    int count = 0;
    while(1)
    {
        gpio_pin_set_raw(led0.port, led0.pin, 0);
        gpio_pin_set_raw(led0.port, led1.pin, 1);

        count++;
        printk("[%d] Version : %s (uptime=%u ms)\n",
               count, APP_VERSION, k_uptime_get_32());
        k_msleep(500);

        gpio_pin_set_raw(led0.port, led0.pin, 1);
        gpio_pin_set_raw(led1.port, led1.pin, 0);
        k_msleep(500);
    }

    return 0;
}
