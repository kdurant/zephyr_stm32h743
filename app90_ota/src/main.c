#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "ota_protocol.h"
#include "ota_handler.h"

int main(void)
{
    printk("\n=== OTA Firmware Upgrade Demo ===\n");
    printk("STM32H743 OTA via MCUboot + Dual Slot\n\n");

    ota_handler_init();
    ota_protocol_init();

    printk("System ready. Waiting for OTA commands...\n\n");

    while (1) {
        ota_process_rx();
        k_msleep(10);
    }

    return 0;
}
