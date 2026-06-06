#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>

#define TEST_OFFSET 0x10000

int main(void)
{
    const struct device *flash_dev;

    uint8_t write_buf[16] = {
        0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88,
        0x99, 0xaa, 0xbb, 0xcc,
        0xdd, 0xee, 0xff, 0x5a
    };

    uint8_t read_buf[16];

    flash_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller));

    if (!device_is_ready(flash_dev)) {
        printk("Flash device not ready\n");
        return 0;
    }

    printk("Flash device: %s\n", flash_dev->name);

    printk("Erase...\n");

    int ret = flash_erase(
        flash_dev,
        TEST_OFFSET,
        4096);

    if (ret) {
        printk("erase failed %d\n", ret);
        return 0;
    }

    printk("Write...\n");

    ret = flash_write(
        flash_dev,
        TEST_OFFSET,
        write_buf,
        sizeof(write_buf));

    if (ret) {
        printk("write failed %d\n", ret);
        return 0;
    }

    printk("Read...\n");

    ret = flash_read(
        flash_dev,
        TEST_OFFSET,
        read_buf,
        sizeof(read_buf));

    if (ret) {
        printk("read failed %d\n", ret);
        return 0;
    }

    printk("Data:\n");

    for (int i = 0; i < sizeof(read_buf); i++) {
        printk("%02X ", read_buf[i]);
    }

    printk("\n");

    return 0;
}