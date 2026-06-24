/* SPDX-License-Identifier: Apache-2.0 */
#include "ota_handler.h"
#include "flash_ops.h"
#include "log_buf.h"
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(ota_handler, LOG_LEVEL_INF);

/* ── 全局状态 ── */
static ota_ctx_t     g_ota;
static device_info_t g_dev_info;
static uint32_t      g_frame_seq; /* 自增帧序号 */

/* ── CRC32 (使用 Zephyr CRC 模块) ── */
#include <zephyr/sys/crc.h>

static uint32_t calc_crc32(const uint8_t* data, size_t len)
{
    return crc32_ieee(data, len);
}

/* ═══════════════════════════════════════════
 * 初始化
 * ═══════════════════════════════════════════ */

void ota_ctx_init(ota_ctx_t* ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->dev_status      = DEV_STATUS_IDLE;
    ctx->stage           = OTA_STAGE_IDLE;
    ctx->max_packet_size = 1024;
}

void ota_handler_init(const device_info_t* info)
{
    memcpy(&g_dev_info, info, sizeof(g_dev_info));
    ota_ctx_init(&g_ota);
    g_frame_seq = 0;

    /* 读取已存储的启动标志和 OTA 信息 */
    flash_read_boot_config();

    LOG_INF("OTA handler ready. FW: V%d.%d.%d, Flash: %u bytes",
            g_dev_info.fw_ver[0], g_dev_info.fw_ver[1],
            g_dev_info.fw_ver[2],
            g_dev_info.flash_total_size);
}

/* ═══════════════════════════════════════════
 * 辅助函数
 * ═══════════════════════════════════════════ */

/* 读 u16 LE */
static uint16_t read_u16_le(const uint8_t* p)
{
    return p[0] | ((uint16_t)p[1] << 8);
}

/* 读 u32 LE */
static uint32_t read_u32_le(const uint8_t* p)
{
    return p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* 写 u16 LE */
static void write_u16_le(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

/* 写 u32 LE */
static void write_u32_le(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* 发送错误响应 */
static void send_error(const frame_t* req, frame_t* resp)
{
    frame_build_error_resp(req, resp);
}

/* 构建成功响应 — extra 必须已包含 status(1字节) 作为第一字节 */
static void send_ok(const frame_t* req, frame_t* resp,
                    const uint8_t* extra, uint16_t extra_len)
{
    frame_build_resp(req, resp, extra, extra_len);
}

/* ═══════════════════════════════════════════
 * 0x0001 — 握手
 * ═══════════════════════════════════════════ */
static void handle_handshake(const frame_t* req, frame_t* resp)
{
    uint8_t buf[3];
    buf[0] = (uint8_t)g_ota.dev_status;
    buf[1] = 1; /* protocol version major */
    buf[2] = 0; /* protocol version minor */
    send_ok(req, resp, buf, 3);
    LOG_INF("Handshake: status=%d", g_ota.dev_status);
}

/* ═══════════════════════════════════════════
 * 0x0002 — 获取设备信息
 * ═══════════════════════════════════════════ */
static void handle_get_device_info(const frame_t* req, frame_t* resp)
{
    uint8_t buf[64];
    int     pos = 0;

    /* MCU model (16 bytes, zero-padded ASCII) */
    memcpy(&buf[pos], g_dev_info.mcu_model, 16);
    pos += 16;

    /* FW version (4 bytes) */
    memcpy(&buf[pos], g_dev_info.fw_ver, 4);
    pos += 4;

    /* Flash total size (4 bytes LE) */
    write_u32_le(&buf[pos], g_dev_info.flash_total_size);
    pos += 4;

    /* Flash page size (2 bytes LE) */
    write_u16_le(&buf[pos], g_dev_info.flash_page_size);
    pos += 2;

    /* Used flash size (4 bytes LE) — approximate */
    write_u32_le(&buf[pos], g_dev_info.flash_used_size);
    pos += 4;

    /* FW start address (4 bytes LE) */
    write_u32_le(&buf[pos], g_dev_info.fw_start_addr);
    pos += 4;

    send_ok(req, resp, buf, pos);
    LOG_INF("Device info sent");
}

/* ═══════════════════════════════════════════
 * 0x0003 — 开始升级
 * ═══════════════════════════════════════════ */
static void handle_start_upgrade(const frame_t* req, frame_t* resp)
{
    if(req->data_len < 12)
    {
        send_error(req, resp);
        return;
    }

    uint32_t fw_size  = read_u32_le(&req->data[0]);
    uint32_t fw_crc32 = read_u32_le(&req->data[4]);
    /* [8..11] = fw version bytes */

    LOG_INF("Start upgrade: size=%u, CRC32=0x%08X", fw_size, fw_crc32);

    /* Validate size */
    if(fw_size > g_dev_info.flash_total_size)
    {
        LOG_ERR("Firmware too large: %u > %u", fw_size, g_dev_info.flash_total_size);
        send_error(req, resp);
        g_ota.error_code = 0x01;
        g_ota.stage      = OTA_STAGE_ERROR;
        return;
    }

    /* Erase flash area for firmware */
    g_ota.stage          = OTA_STAGE_ERASING;
    g_ota.dev_status     = DEV_STATUS_UPGRADING;
    g_ota.bytes_received = 0;

    int ret = flash_erase_firmware_region(fw_size);
    if(ret < 0)
    {
        LOG_ERR("Flash erase failed: %d", ret);
        send_error(req, resp);
        g_ota.error_code = 0x02;
        g_ota.stage      = OTA_STAGE_ERROR;
        g_ota.dev_status = DEV_STATUS_IDLE;
        return;
    }

    /* Store upgrade info for validation */
    g_ota.fw_total_size = fw_size;
    g_ota.fw_crc32      = fw_crc32;
    g_ota.fw_version    = read_u32_le(&req->data[8]);
    g_ota.error_code    = 0x00;
    g_ota.cancelled     = false;

    /* Response: status=0x00, max_packet_size, erase_progress=100 */
    uint8_t buf[3];
    buf[0] = 0x00; /* status ok */
    write_u16_le(&buf[1], g_ota.max_packet_size);
    /* buf[3] is not used in this layout — but protocol says erase_progress at offset 3 */
    /* Actually: status(1) + max_pkt_size(2) + erase_progress(1) = 4 bytes */
    uint8_t resp_data[4];
    resp_data[0] = 0x00;
    write_u16_le(&resp_data[1], g_ota.max_packet_size);
    resp_data[3] = 100; /* erase done */
    send_ok(req, resp, resp_data, 4);

    g_ota.stage = OTA_STAGE_RECEIVING;
    LOG_INF("Ready to receive firmware");
}

/* ═══════════════════════════════════════════
 * 0x0004 — 传输固件数据
 * ═══════════════════════════════════════════ */
static void handle_transfer_data(const frame_t* req, frame_t* resp)
{
    if(g_ota.stage != OTA_STAGE_RECEIVING)
    {
        LOG_ERR("Not in receiving stage");
        send_error(req, resp);
        return;
    }
    if(req->data_len < 5)
    {
        send_error(req, resp);
        return;
    }

    uint32_t       offset    = read_u32_le(&req->data[0]);
    uint16_t       chunk_len = req->data_len - 4;
    const uint8_t* chunk     = &req->data[4];

    int ret = flash_write_firmware_chunk(offset, chunk, chunk_len);
    if(ret < 0)
    {
        LOG_ERR("Flash write failed at offset 0x%08X: %d", offset, ret);
        send_error(req, resp);
        g_ota.error_code = 0x03;
        g_ota.stage      = OTA_STAGE_ERROR;
        g_ota.dev_status = DEV_STATUS_IDLE;
        return;
    }

    g_ota.bytes_received = offset + chunk_len;

    /* Response: status + received offset */
    uint8_t buf[5];
    buf[0] = 0x00;
    write_u32_le(&buf[1], g_ota.bytes_received);
    send_ok(req, resp, buf, 5);

#if LOG_LEVEL_INF
    if((g_ota.bytes_received % (g_ota.fw_total_size / 10)) < g_ota.max_packet_size + 1 ||
       g_ota.bytes_received >= g_ota.fw_total_size)
    {
        LOG_INF("Receive: %u / %u bytes (%d%%)",
                g_ota.bytes_received, g_ota.fw_total_size,
                (int)(g_ota.bytes_received * 100ULL / g_ota.fw_total_size));
    }
#endif
}

/* ═══════════════════════════════════════════
 * 0x0005 — 传输完成
 * ═══════════════════════════════════════════ */
static void handle_transfer_complete(const frame_t* req, frame_t* resp)
{
    if(req->data_len < 8)
    {
        send_error(req, resp);
        return;
    }

    uint32_t pc_crc32 = read_u32_le(&req->data[0]);
    uint32_t pc_size  = read_u32_le(&req->data[4]);

    LOG_INF("Transfer complete check: size=%u, CRC=0x%08X", pc_size, pc_crc32);

    if(pc_size != g_ota.bytes_received)
    {
        LOG_ERR("Size mismatch: PC says %u, we got %u", pc_size, g_ota.bytes_received);
        uint8_t buf[5];
        buf[0] = 0x00;
        write_u32_le(&buf[1], g_ota.bytes_received);
        frame_build_resp(req, resp, buf, 5);
        /* After this, PC should still proceed to verify */
    }
    else
    {
        uint8_t buf[5];
        buf[0] = 0x00;
        write_u32_le(&buf[1], g_ota.bytes_received);
        frame_build_resp(req, resp, buf, 5);
    }

    g_ota.stage = OTA_STAGE_VERIFYING;
    flash_finalize_firmware_write();
}

/* ═══════════════════════════════════════════
 * 0x0006 — 校验固件
 * ═══════════════════════════════════════════ */
static void handle_verify_firmware(const frame_t* req, frame_t* resp)
{
    LOG_INF("Verifying firmware...");

    uint32_t calc_crc = flash_calc_firmware_crc32(g_ota.bytes_received);

    uint8_t result;
    if(calc_crc == g_ota.fw_crc32)
    {
        result = 0x00; /* match */
        LOG_INF("CRC32 verified OK: 0x%08X", calc_crc);
    }
    else
    {
        result = 0x01; /* mismatch */
        LOG_ERR("CRC32 mismatch: expected 0x%08X, got 0x%08X",
                g_ota.fw_crc32, calc_crc);
        g_ota.error_code = 0x04;
    }

    uint8_t buf[5];
    buf[0] = result;
    write_u32_le(&buf[1], calc_crc);
    frame_build_resp(req, resp, buf, 5);
}

/* ═══════════════════════════════════════════
 * 0x0007 — 复位运行
 * ═══════════════════════════════════════════ */
static void handle_reset_run(const frame_t* req, frame_t* resp)
{
    uint16_t delay_ms = 0;
    if(req->data_len >= 2)
    {
        delay_ms = read_u16_le(&req->data[0]);
    }

    LOG_INF("Reset requested, delay=%u ms", delay_ms);

    /* Ack */
    uint8_t buf[1] = {0x00};
    frame_build_resp(req, resp, buf, 1);

    /* The main loop will detect this and perform reset */
    g_ota.dev_status = DEV_STATUS_IDLE;
    g_ota.stage      = OTA_STAGE_COMPLETE;

    /* Signal main to reset */
    extern volatile bool g_reset_requested;
    g_reset_requested = true;
}

/* ═══════════════════════════════════════════
 * 0x0008 — 取消升级
 * ═══════════════════════════════════════════ */
static void handle_cancel_upgrade(const frame_t* req, frame_t* resp)
{
    LOG_WRN("Upgrade cancelled by PC");

    g_ota.cancelled  = true;
    g_ota.stage      = OTA_STAGE_IDLE;
    g_ota.dev_status = DEV_STATUS_IDLE;
    g_ota.error_code = 0x00;

    /* Clear partial firmware */
    flash_invalidate_firmware();

    uint8_t buf[1] = {0x00};
    frame_build_resp(req, resp, buf, 1);
}

/* ═══════════════════════════════════════════
 * 0x0009 — 查询升级状态
 * ═══════════════════════════════════════════ */
static void handle_query_status(const frame_t* req, frame_t* resp)
{
    uint8_t buf[10];
    buf[0] = (uint8_t)g_ota.stage;
    write_u32_le(&buf[1], g_ota.bytes_received);
    write_u32_le(&buf[5], g_ota.fw_total_size);
    buf[9] = g_ota.error_code;
    frame_build_resp(req, resp, buf, 10);
}

/* ═══════════════════════════════════════════
 * 0x000A — 设置启动标志
 * ═══════════════════════════════════════════ */
static void handle_set_boot_flag(const frame_t* req, frame_t* resp)
{
    log_buf_printf("CMD 0x000A received: data_len=%u", req->data_len);
    uint8_t flag = BOOT_FLAG_NEW_FW;
    if(req->data_len < 1)
    {
        log_buf_printf("CMD 0x000A empty payload, defaulting to BOOT_FLAG_NEW_FW");
    }
    else
    {
        flag = req->data[0];
    }

    if(flag != BOOT_FLAG_NEW_FW && flag != BOOT_FLAG_ROLLBACK)
    {
        log_buf_printf("Unexpected boot flag 0x%02X, forcing BOOT_FLAG_NEW_FW", flag);
        flag = BOOT_FLAG_NEW_FW;
    }

    log_buf_printf("Set boot flag: %s", flag ? "new firmware" : "rollback");
    log_buf_printf("Starting flash_write_boot_flag...");

    int ret = flash_write_boot_flag(flag);
    if(ret < 0)
    {
        log_buf_printf("Boot flag write FAILED: %d", ret);
        send_error(req, resp);
        return;
    }

    log_buf_printf("Boot flag write OK");
    uint8_t buf[1] = {0x00};
    frame_build_resp(req, resp, buf, 1);
}

/* ═══════════════════════════════════════════
 * 0x000B — 获取日志
 * ═══════════════════════════════════════════ */
static void handle_get_log(const frame_t* req, frame_t* resp)
{
    log_buf_printf("CMD 0x000B received");
    char text[1024];
    int  len = log_buf_read(text, sizeof(text));
    if(len > 0)
    {
        frame_build_resp(req, resp, (const uint8_t*)text, (uint16_t)len);
    }
    else
    {
        frame_build_resp(req, resp, NULL, 0);
    }
}

/* ═══════════════════════════════════════════
 * 总调度
 * ═══════════════════════════════════════════ */

bool ota_handle_frame(const frame_t* req, frame_t* resp)
{
    /* 只处理发给本设备的帧 */
    if(req->dst_addr != ADDR_DEVICE && req->dst_addr != 0xFFFF)
    {
        return false;
    }

    /* 更新帧序号 */
    g_frame_seq = req->seq + 1;

    switch(req->command)
    {
        case CMD_HANDSHAKE:
            handle_handshake(req, resp);
            return true;
        case CMD_GET_DEVICE_INFO:
            handle_get_device_info(req, resp);
            return true;
        case CMD_START_UPGRADE:
            handle_start_upgrade(req, resp);
            return true;
        case CMD_TRANSFER_DATA:
            handle_transfer_data(req, resp);
            return true;
        case CMD_TRANSFER_COMPLETE:
            handle_transfer_complete(req, resp);
            return true;
        case CMD_VERIFY_FIRMWARE:
            handle_verify_firmware(req, resp);
            return true;
        case CMD_RESET_RUN:
            handle_reset_run(req, resp);
            return true;
        case CMD_CANCEL_UPGRADE:
            handle_cancel_upgrade(req, resp);
            return true;
        case CMD_QUERY_STATUS:
            handle_query_status(req, resp);
            return true;
        case CMD_SET_BOOT_FLAG:
            handle_set_boot_flag(req, resp);
            return true;
        case CMD_GET_LOG:
            handle_get_log(req, resp);
            return true;
        default:
            LOG_WRN("Unknown command: 0x%04X", req->command);
            send_error(req, resp);
            return true;
    }
}
