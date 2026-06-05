#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(app10_eeprom, LOG_LEVEL_INF);

/* 通过别名获取 EEPROM 节点 */
#define EEPROM_NODE DT_ALIAS(eeprom0)

#if !DT_NODE_HAS_STATUS(EEPROM_NODE, okay)
#error "No eeprom0 alias found in device tree"
#endif

/* 获取 I2C 设备 */
static const struct i2c_dt_spec eeprom = I2C_DT_SPEC_GET(EEPROM_NODE);

/* 测试数据缓冲区大小 */
#define TEST_DATA_SIZE 16
#define TEST_OFFSET    0x00  /* EEPROM 起始地址 */

/**
 * @brief 向 EEPROM 写入数据
 * 
 * @param offset EEPROM 内部地址偏移
 * @param data 要写入的数据指针
 * @param len 数据长度
 * @return int 0 表示成功，负数表示失败
 */
static int eeprom_write(uint8_t offset, const uint8_t *data, uint8_t len)
{
    uint8_t buf[TEST_DATA_SIZE + 1];  /* 1 字节地址 + 数据 */
    int ret;

    if (len > TEST_DATA_SIZE) {
        LOG_ERR("Data length exceeds buffer size");
        return -EINVAL;
    }

    /* 构建写入缓冲区：[地址][数据] */
    buf[0] = offset;
    memcpy(&buf[1], data, len);

    /* 通过 I2C 写入数据 */
    ret = i2c_write_dt(&eeprom, buf, len + 1);
    if (ret < 0) {
        LOG_ERR("Failed to write to EEPROM: %d", ret);
        return ret;
    }

    /* AT24C02 写周期时间约 5ms，需要等待 */
    k_msleep(10);

    LOG_INF("EEPROM write success: offset=0x%02X, len=%d", offset, len);
    return 0;
}

/**
 * @brief 从 EEPROM 读取数据
 * 
 * @param offset EEPROM 内部地址偏移
 * @param data 读取数据存放的缓冲区
 * @param len 要读取的数据长度
 * @return int 0 表示成功，负数表示失败
 */
static int eeprom_read(uint8_t offset, uint8_t *data, uint8_t len)
{
    int ret;

    if (len > TEST_DATA_SIZE) {
        LOG_ERR("Data length exceeds buffer size");
        return -EINVAL;
    }

    /* 先写入要读取的地址 */
    ret = i2c_write_dt(&eeprom, &offset, 1);
    if (ret < 0) {
        LOG_ERR("Failed to set EEPROM read address: %d", ret);
        return ret;
    }

    /* 再读取数据 */
    ret = i2c_read_dt(&eeprom, data, len);
    if (ret < 0) {
        LOG_ERR("Failed to read from EEPROM: %d", ret);
        return ret;
    }

    LOG_INF("EEPROM read success: offset=0x%02X, len=%d", offset, len);
    return 0;
}

/**
 * @brief 打印十六进制数据
 * 
 * @param label 标签
 * @param data 数据指针
 * @param len 数据长度
 */
static void print_hex_data(const char *label, const uint8_t *data, uint8_t len)
{
    printk("%s: ", label);
    for (int i = 0; i < len; i++) {
        printk("%02X ", data[i]);
    }
    printk("\n");
}

int main(void)
{
    int ret;
    uint8_t write_data[TEST_DATA_SIZE];
    uint8_t read_data[TEST_DATA_SIZE];

    printk("Starting app10_eeprom test\n");

    /* 检查 I2C 设备是否就绪 */
    if (!i2c_is_ready_dt(&eeprom)) {
        LOG_ERR("EEPROM I2C device not ready");
        return -1;
    }

    LOG_INF("EEPROM device is ready (AT24C02, 256 bytes)");

    /* 准备测试数据：0x00 ~ 0x0F */
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        write_data[i] = i;
    }

    print_hex_data("Write data", write_data, TEST_DATA_SIZE);

    /* 步骤 1: 写入数据到 EEPROM */
    LOG_INF("Step 1: Writing data to EEPROM...");
    ret = eeprom_write(TEST_OFFSET, write_data, TEST_DATA_SIZE);
    if (ret < 0) {
        LOG_ERR("EEPROM write failed");
        return ret;
    }

    /* 清空读取缓冲区 */
    memset(read_data, 0, TEST_DATA_SIZE);

    /* 步骤 2: 从 EEPROM 读取数据 */
    LOG_INF("Step 2: Reading data from EEPROM...");
    ret = eeprom_read(TEST_OFFSET, read_data, TEST_DATA_SIZE);
    if (ret < 0) {
        LOG_ERR("EEPROM read failed");
        return ret;
    }

    print_hex_data("Read data", read_data, TEST_DATA_SIZE);

    /* 步骤 3: 校验数据一致性 */
    LOG_INF("Step 3: Verifying data consistency...");
    if (memcmp(write_data, read_data, TEST_DATA_SIZE) == 0) {
        LOG_INF("✓ Data verification PASSED! Write and read data are identical.");
        printk("\n========================================\n");
        printk("  EEPROM Test Result: SUCCESS\n");
        printk("========================================\n");
    } else {
        LOG_ERR("✗ Data verification FAILED! Data mismatch detected.");
        
        /* 找出不一致的字节 */
        printk("\nMismatch details:\n");
        for (int i = 0; i < TEST_DATA_SIZE; i++) {
            if (write_data[i] != read_data[i]) {
                printk("  Offset 0x%02X: Write=0x%02X, Read=0x%02X\n",
                       TEST_OFFSET + i, write_data[i], read_data[i]);
            }
        }
        printk("\n========================================\n");
        printk("  EEPROM Test Result: FAILED\n");
        printk("========================================\n");
        return -1;
    }

    /* 步骤 4: 额外测试 - 读取单个字节 */
    LOG_INF("Step 4: Additional test - reading single byte...");
    uint8_t single_byte;
    ret = eeprom_read(TEST_OFFSET + 5, &single_byte, 1);
    if (ret == 0) {
        LOG_INF("Single byte at offset 0x05: 0x%02X (expected: 0x05)", single_byte);
        if (single_byte == 0x05) {
            LOG_INF("✓ Single byte read correct");
        } else {
            LOG_ERR("✗ Single byte read incorrect");
        }
    }

    LOG_INF("EEPROM test completed successfully!");

    return 0;
}
