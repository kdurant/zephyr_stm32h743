/* SPDX-License-Identifier: Apache-2.0 */
/* QSPI NOR Flash 操作封装
 *
 * 布局:
 *   [0x000000 .. FIRMWARE_MAX_SIZE]   固件存储区
 *   [FW_INFO_OFFSET]                  OTA 信息区 (固件版本/CRC/大小)
 *   [BOOT_FLAG_OFFSET]                启动标志 (1 page)
 */

#ifndef FLASH_OPS_H
#define FLASH_OPS_H

#include <stdint.h>
#include <stdbool.h>

/* ── Flash 布局常量 ── */
#define FIRMWARE_OFFSET   0x00000000
#define FIRMWARE_MAX_SIZE 0x00200000 /* 2MB — QSPI driver reports 2MB usable */
/* Place OTA metadata within the valid 2MB range */
#define FW_INFO_OFFSET   0x001FE000 /* 倒数第 2 个 4K 扇区 (2MB - 8KB) */
#define BOOT_FLAG_OFFSET 0x001FF000 /* 最后一个 4K 扇区 (2MB - 4KB) */

/* 启动标志值 */
#define BOOT_FLAG_NEW_FW   0x01
#define BOOT_FLAG_ROLLBACK 0x00
#define BOOT_FLAG_EMPTY    0xFF

/* ── OTA 信息结构 (存储在 FW_INFO_OFFSET) ── */
#pragma pack(push, 1)
typedef struct
{
    uint32_t magic; /* 0x4F544101 = "OTA\x01" */
    uint32_t fw_size;
    uint32_t fw_crc32;
    uint8_t  fw_version[4];
    uint8_t  reserved[16];
} ota_info_t;
#pragma pack(pop)

#define OTA_INFO_MAGIC 0x4F544101

/* ── API ── */
void     flash_read_boot_config(void);
int      flash_erase_firmware_region(uint32_t size);
int      flash_write_firmware_chunk(uint32_t offset, const uint8_t* data, uint32_t len);
void     flash_finalize_firmware_write(void);
void     flash_invalidate_firmware(void);
int      flash_write_boot_flag(uint8_t flag);
uint32_t flash_calc_firmware_crc32(uint32_t size);

#endif /* FLASH_OPS_H */
