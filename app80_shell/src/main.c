#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(app80_shell, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

static bool led0_state;
static bool led1_state;

static int cmd_led_on(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: led on <0|1>");
        return -EINVAL;
    }

    int led_num = atoi(argv[1]);
    if (led_num == 0) {
        gpio_pin_set_raw(led0.port, led0.pin, 0);
        led0_state = true;
        shell_print(sh, "LED0 ON");
    } else if (led_num == 1) {
        gpio_pin_set_raw(led1.port, led1.pin, 0);
        led1_state = true;
        shell_print(sh, "LED1 ON");
    } else {
        shell_error(sh, "Invalid LED number: %s", argv[1]);
        return -EINVAL;
    }

    return 0;
}

static int cmd_led_off(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: led off <0|1>");
        return -EINVAL;
    }

    int led_num = atoi(argv[1]);
    if (led_num == 0) {
        gpio_pin_set_raw(led0.port, led0.pin, 1);
        led0_state = false;
        shell_print(sh, "LED0 OFF");
    } else if (led_num == 1) {
        gpio_pin_set_raw(led1.port, led1.pin, 1);
        led1_state = false;
        shell_print(sh, "LED1 OFF");
    } else {
        shell_error(sh, "Invalid LED number: %s", argv[1]);
        return -EINVAL;
    }

    return 0;
}

static int cmd_led_status(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "LED0: %s", led0_state ? "ON" : "OFF");
    shell_print(sh, "LED1: %s", led1_state ? "ON" : "OFF");
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_led,
    SHELL_CMD(on, NULL, "Turn LED on", cmd_led_on),
    SHELL_CMD(off, NULL, "Turn LED off", cmd_led_off),
    SHELL_CMD(status, NULL, "Show LED status", cmd_led_status),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(led, &sub_led, "LED control commands", NULL);

static int cmd_info(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "Application : app80_shell");
    shell_print(sh, "Version     : %s", APP_VERSION);
    shell_print(sh, "Board       : %s", CONFIG_BOARD);
    shell_print(sh, "Uptime      : %llu ms", k_uptime_get());
    return 0;
}

SHELL_CMD_REGISTER(info, NULL, "Show application info", cmd_info);

static int cmd_reboot(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "Rebooting...");
    k_msleep(100);
    sys_reboot(SYS_REBOOT_COLD);
    return 0;
}

SHELL_CMD_REGISTER(reboot, NULL, "Reboot system", cmd_reboot);

int main(void)
{
    if (!gpio_is_ready_dt(&led0) || !gpio_is_ready_dt(&led1)) {
        LOG_ERR("GPIO device not ready");
        return -1;
    }

    gpio_pin_configure_dt(&led0, GPIO_OUTPUT);
    gpio_pin_configure_dt(&led1, GPIO_OUTPUT);

    gpio_pin_set_raw(led0.port, led0.pin, 1);
    gpio_pin_set_raw(led1.port, led1.pin, 1);

    LOG_INF("app80_shell started, type 'help' for commands");

    return 0;
}
