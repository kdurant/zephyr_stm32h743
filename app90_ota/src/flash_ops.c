/* SPDX-License-Identifier: Apache-2.0 */
#include "flash_ops.h"
#include "log_buf.h"
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/sys/crc.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(flash_ops, LOG_LEVEL_INF);

/* QSPI flash sector size */
#define OTA_FLASH_SECTOR_SIZE 4096

static const struct device* flash_dev;

/* ── 内部状态 ── */
static uint32_t fw_next_offset; /* 下一次写入的偏移 */

/* ── 初始化 ── */

void flash_read_boot_config(void)
{
    flash_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller));
    if(!device_is_ready(flash_dev))
    {
        LOG_ERR("Flash device not ready");
        return;
    }

    fw_next_offset = FIRMWARE_OFFSET;

    /* Read OTA info */
    ota_info_t info;
    int        ret = flash_read(flash_dev, FW_INFO_OFFSET, &info, sizeof(info));
    if(ret == 0 && info.magic == OTA_INFO_MAGIC)
    {
        LOG_INF("Stored OTA info: size=%u, CRC32=0x%08X, ver=%d.%d.%d",
                info.fw_size, info.fw_crc32,
                info.fw_version[0], info.fw_version[1],
                (info.fw_version[2] << 8) | info.fw_version[3]);
    }

    /* Read boot flag */
    uint8_t boot_flag;
    ret = flash_read(flash_dev, BOOT_FLAG_OFFSET, &boot_flag, 1);
    if(ret == 0)
    {
        LOG_INF("Boot flag: 0x%02X", boot_flag);
    }
}

/* ═══════════════════════════════════════════
 * Flash 操作
 * ═══════════════════════════════════════════ */

int flash_erase_firmware_region(uint32_t size)
{
    if(!flash_dev)
    {
        return -1;
    }

    /* Round up to sector boundary */
    uint32_t erase_size = ((size + OTA_FLASH_SECTOR_SIZE - 1) / OTA_FLASH_SECTOR_SIZE) * OTA_FLASH_SECTOR_SIZE;
    if(erase_size > FIRMWARE_MAX_SIZE)
    {
        erase_size = FIRMWARE_MAX_SIZE;
    }

    LOG_INF("Erasing %u bytes at offset 0x%08X...", erase_size, FIRMWARE_OFFSET);

    int ret = flash_erase(flash_dev, FIRMWARE_OFFSET, erase_size);
    if(ret < 0)
    {
        LOG_ERR("Erase failed: %d", ret);
        return ret;
    }

    fw_next_offset = FIRMWARE_OFFSET;
    LOG_INF("Erase done");
    return 0;
}

int flash_write_firmware_chunk(uint32_t offset, const uint8_t* data, uint32_t len)
{
    if(!flash_dev)
    {
        return -1;
    }

    uint32_t write_addr = FIRMWARE_OFFSET + offset;

    int ret = flash_write(flash_dev, write_addr, data, len);
    if(ret < 0)
    {
        LOG_ERR("Write failed at 0x%08X len=%u: %d", write_addr, len, ret);
        return ret;
    }

    fw_next_offset = write_addr + len;
    return 0;
}

void flash_finalize_firmware_write(void)
{
    LOG_INF("Firmware write finalized at %u bytes", fw_next_offset - FIRMWARE_OFFSET);
}

void flash_invalidate_firmware(void)
{
    /* Invalidate OTA info by clearing magic */
    ota_info_t empty = {0};
    flash_write(flash_dev, FW_INFO_OFFSET, &empty, sizeof(empty));
    fw_next_offset = FIRMWARE_OFFSET;
    LOG_INF("Firmware storage invalidated");
}

int flash_write_boot_flag(uint8_t flag)
{
    if(!flash_dev)
    {
        return -1;
    }

    /* Don't use flash_get_page_info_by_offs — it can return -EINVAL on QSPI.
     * Instead use a direct 4KB erase + write at the target offset. */

    log_buf_printf("BOOT_FLAG_OFFSET=0x%08X", BOOT_FLAG_OFFSET);

    /* Erase one sector containing BOOT_FLAG_OFFSET */
    uint32_t sector_start = BOOT_FLAG_OFFSET & ~(OTA_FLASH_SECTOR_SIZE - 1);
    int      ret          = flash_erase(flash_dev, sector_start, OTA_FLASH_SECTOR_SIZE);
    if(ret < 0)
    {
        log_buf_printf("Boot flag erase failed: %d", ret);
        return ret;
    }

    log_buf_printf("Boot flag sector erased: 0x%08X", sector_start);

    ret = flash_write(flash_dev, BOOT_FLAG_OFFSET, &flag, 1);
    if(ret < 0)
    {
        log_buf_printf("Boot flag write failed: %d", ret);
        return ret;
    }

    log_buf_printf("Boot flag set to 0x%02X", flag);
    return 0;
}

uint32_t flash_calc_firmware_crc32(uint32_t size)
{
    if(!flash_dev || size == 0)
    {
        return 0;
    }

    uint32_t crc = 0xFFFFFFFF;
    uint8_t  buf[256];
    uint32_t remaining = size;
    uint32_t offset    = FIRMWARE_OFFSET;

    while(remaining > 0)
    {
        uint32_t chunk = remaining > sizeof(buf) ? sizeof(buf) : remaining;
        int      ret   = flash_read(flash_dev, offset, buf, chunk);
        if(ret < 0)
        {
            LOG_ERR("CRC read failed at 0x%08X", offset);
            return 0;
        }

        for(uint32_t i = 0; i < chunk; i++)
        {
            crc ^= buf[i];
            for(int j = 0; j < 8; j++)
            {
                if(crc & 1)
                {
                    crc = (crc >> 1) ^ 0xEDB88320;
                }
                else
                {
                    crc >>= 1;
                }
            }
        }

        offset += chunk;
        remaining -= chunk;
    }

    return crc ^ 0xFFFFFFFF;
}
