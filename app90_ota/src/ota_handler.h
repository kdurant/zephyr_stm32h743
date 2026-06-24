/* SPDX-License-Identifier: Apache-2.0 */
/* OTA command handler — dispatches each command to the appropriate logic */

#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include "frame.h"

/* ── OTA 升级阶段 ── */
typedef enum
{
    OTA_STAGE_IDLE      = 0x00,
    OTA_STAGE_ERASING   = 0x01,
    OTA_STAGE_RECEIVING = 0x02,
    OTA_STAGE_VERIFYING = 0x03,
    OTA_STAGE_COMPLETE  = 0x04,
    OTA_STAGE_ERROR     = 0xFF,
} ota_stage_t;

/* ── 设备状态 ── */
typedef enum
{
    DEV_STATUS_IDLE      = 0x00,
    DEV_STATUS_UPGRADING = 0x01,
    DEV_STATUS_READY     = 0x02,
} dev_status_t;

/* ── OTA 上下文 ── */
typedef struct
{
    dev_status_t dev_status;
    ota_stage_t  stage;
    uint32_t     fw_total_size;
    uint32_t     fw_crc32;
    uint32_t     fw_version; /* packed: major|minor|rev_hi|rev_lo */
    uint16_t     max_packet_size;
    uint32_t     bytes_received;
    uint8_t      error_code;
    bool         cancelled;
} ota_ctx_t;

/* ── 设备信息 ── */
typedef struct
{
    char     mcu_model[16];
    uint8_t  fw_ver[4]; /* major, minor, rev_hi, rev_lo */
    uint32_t flash_total_size;
    uint16_t flash_page_size;
    uint32_t flash_used_size;
    uint32_t fw_start_addr;
} device_info_t;

/* ── API ── */
void ota_ctx_init(ota_ctx_t* ctx);
void ota_handler_init(const device_info_t* info);

/* 处理收到的请求帧，若需要响应则填充 resp */
/* 返回 true 表示需要发送 resp */
bool ota_handle_frame(const frame_t* req, frame_t* resp);

#endif /* OTA_HANDLER_H */
