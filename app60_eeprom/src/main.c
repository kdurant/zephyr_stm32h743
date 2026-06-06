#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/eeprom.h>
#include <string.h>

#define OFF 0

int main(void)
{
    const struct device *eeprom;

    uint8_t w[16] = {
        1,2,3,4,5,6,7,8,
        9,10,11,12,13,14,15,16
    };

    uint8_t r[16];

    eeprom = DEVICE_DT_GET(DT_NODELABEL(eeprom0));

    if (!device_is_ready(eeprom)) {
        printk("EEPROM not ready\n");
        return 0;
    }

    printk("EEPROM OK: %s\n", eeprom->name);

    eeprom_write(eeprom, OFF, w, sizeof(w));
    eeprom_read(eeprom, OFF, r, sizeof(r));

    printk("DATA:\n");
    for (int i = 0; i < sizeof(r); i++) {
        printk("%02X ", r[i]);
    }
    printk("\n");

    return 0;
}