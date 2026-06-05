#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app01_led, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "No led0 alias found in device tree"
#endif

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void)
{
    int ret;

    printf("Starting app01_led\n");
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

    LOG_INF("Start blinking LED0 on PB3 (IO=0 on, IO=1 off)");

    while(1)
    {
        /* Force raw physical level: 0 = LED on, 1 = LED off. */
        gpio_pin_set_raw(led0.port, led0.pin, 0);
        k_msleep(500);

        gpio_pin_set_raw(led0.port, led0.pin, 1);
        k_msleep(500);
    }

    return 0;
}