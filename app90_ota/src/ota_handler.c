#include "ota_handler.h"
#include "ota_protocol.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/reboot.h>
#include <string.h>

static const struct flash_area *slot0_area;
static const struct flash_area *slot1_area;

ota_state_t ota_state;

#define PROTOCOL_VERSION_MAJOR  1
#define PROTOCOL_VERSION_MINOR  0

#define MCU_MODEL "STM32H743"

void ota_handler_init(void)
{
    int err = flash_area_open(PARTITION_ID(slot0_partition), &slot0_area);
    if (err) {
        printk("Failed to open slot0 area: %d\n", err);
        return;
    }

    err = flash_area_open(PARTITION_ID(slot1_partition), &slot1_area);
    if (err) {
        printk("Failed to open slot1 area: %d\n", err);
        return;
    }

    ota_state.status = STATUS_IDLE;
    ota_state.error_code = ERR_NONE;
    ota_state.received_size = 0;
    ota_state.firmware_size = 0;
    ota_state.firmware_crc = 0;
    ota_state.calculated_crc = 0;
    ota_state.current_offset = 0;
    ota_state.max_packet_size = MAX_PACKET_SIZE;
    ota_state.in_progress = false;

    if (slot0_area && slot1_area) {
        ota_state.slot0_offset = slot0_area->fa_off;
        ota_state.slot1_offset = slot1_area->fa_off;
        ota_state.slot_size = slot0_area->fa_size;
    } else {
        ota_state.slot0_offset = 0x20000;
        ota_state.slot1_offset = 0x110000;
        ota_state.slot_size = 0xF0000;
    }

    printk("OTA handler initialized, slot0=0x%x, slot1=0x%x, size=%d\n",
           ota_state.slot0_offset, ota_state.slot1_offset, ota_state.slot_size);
}

void ota_cmd_handshake(uint16_t seq, const uint8_t *data, uint16_t len)
{
    uint8_t resp[3];
    resp[0] = ota_state.status;
    resp[1] = PROTOCOL_VERSION_MINOR;
    resp[2] = PROTOCOL_VERSION_MAJOR;

    ota_send_response(seq, CMD_HANDSHAKE, resp, sizeof(resp));
    printk("Handshake: status=%d\n", ota_state.status);
}

void ota_cmd_get_dev_info(uint16_t seq, const uint8_t *data, uint16_t len)
{
    uint8_t resp[34];
    memset(resp, 0, sizeof(resp));

    strncpy((char *)resp, MCU_MODEL, 16);

    uint32_t version = 0x01000000;
    memcpy(&resp[16], &version, 4);

    uint32_t flash_size = 2 * 1024 * 1024;
    memcpy(&resp[20], &flash_size, 4);

    uint16_t page_size = 4096;
    memcpy(&resp[24], &page_size, 2);

    uint32_t used_size = 0x20000;
    memcpy(&resp[26], &used_size, 4);

    uint32_t fw_start = ota_state.slot0_offset;
    memcpy(&resp[30], &fw_start, 4);

    ota_send_response(seq, CMD_GET_DEV_INFO, resp, sizeof(resp));
    printk("Get device info\n");
}

void ota_cmd_start_upgrade(uint16_t seq, const uint8_t *data, uint16_t len)
{
    if (len < 12) {
        uint8_t fail[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
        ota_send_response(seq, CMD_START_UPGRADE, fail, 4);
        return;
    }

    if (ota_state.status != STATUS_IDLE && ota_state.status != STATUS_COMPLETE) {
        uint8_t resp[4];
        resp[0] = 0xFF;
        memset(&resp[1], 0xFF, 3);
        ota_send_response(seq, CMD_START_UPGRADE, resp, 4);
        printk("Start upgrade rejected: busy\n");
        return;
    }

    memcpy(&ota_state.firmware_size, &data[0], 4);
    memcpy(&ota_state.firmware_crc, &data[4], 4);

    if (ota_state.firmware_size > ota_state.slot_size) {
        ota_state.error_code = ERR_FIRMWARE_SIZE;
        uint8_t resp[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
        ota_send_response(seq, CMD_START_UPGRADE, resp, 4);
        printk("Firmware size exceeds slot: %d > %d\n",
               ota_state.firmware_size, ota_state.slot_size);
        return;
    }

    ota_state.status = STATUS_ERASING;
    ota_state.received_size = 0;
    ota_state.current_offset = 0;
    ota_state.in_progress = true;
    ota_state.calculated_crc = 0;

    printk("Start upgrade: size=%d, crc=0x%08x\n",
           ota_state.firmware_size, ota_state.firmware_crc);

    int err = flash_area_erase(slot1_area, 0, ota_state.slot_size);
    if (err) {
        ota_state.status = STATUS_IDLE;
        ota_state.error_code = ERR_FLASH_ERASE;
        ota_state.in_progress = false;
        uint8_t resp[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
        ota_send_response(seq, CMD_START_UPGRADE, resp, 4);
        printk("Flash erase failed: %d\n", err);
        return;
    }

    ota_state.status = STATUS_RECEIVING;

    uint8_t resp[4];
    resp[0] = 0x00;
    uint16_t max_pkt = MAX_PACKET_SIZE;
    memcpy(&resp[1], &max_pkt, 2);
    resp[3] = 100;

    ota_send_response(seq, CMD_START_UPGRADE, resp, sizeof(resp));
    printk("Upgrade ready, max packet=%d\n", max_pkt);
}

void ota_cmd_transfer_data(uint16_t seq, const uint8_t *data, uint16_t len)
{
    if (!ota_state.in_progress || ota_state.status != STATUS_RECEIVING) {
        uint8_t fail[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        ota_send_response(seq, CMD_TRANSFER_DATA, fail, 5);
        return;
    }

    if (len < 4) {
        uint8_t fail[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        ota_send_response(seq, CMD_TRANSFER_DATA, fail, 5);
        return;
    }

    uint32_t offset;
    memcpy(&offset, &data[0], 4);

    const uint8_t *fw_data = &data[4];
    uint16_t fw_len = len - 4;

    if (offset != ota_state.current_offset) {
        ota_state.error_code = ERR_OFFSET_ERROR;
        ota_state.status = STATUS_ERROR;
        uint8_t resp[5] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF };
        ota_send_response(seq, CMD_TRANSFER_DATA, resp, 5);
        printk("Offset error: expected %d, got %d\n",
               ota_state.current_offset, offset);
        return;
    }

    if (offset + fw_len > ota_state.firmware_size) {
        ota_state.error_code = ERR_FIRMWARE_SIZE;
        ota_state.status = STATUS_ERROR;
        uint8_t resp[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        ota_send_response(seq, CMD_TRANSFER_DATA, resp, 5);
        printk("Data exceeds firmware size\n");
        return;
    }

    int err = flash_area_write(slot1_area, offset, fw_data, fw_len);
    if (err) {
        ota_state.error_code = ERR_FLASH_WRITE;
        ota_state.status = STATUS_ERROR;
        uint8_t resp[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        ota_send_response(seq, CMD_TRANSFER_DATA, resp, 5);
        printk("Flash write failed: %d\n", err);
        return;
    }

    ota_state.calculated_crc = crc32_ieee_update(ota_state.calculated_crc,
                                                  fw_data, fw_len);
    ota_state.current_offset += fw_len;
    ota_state.received_size += fw_len;

    uint8_t resp[5];
    resp[0] = 0x00;
    memcpy(&resp[1], &ota_state.received_size, 4);

    ota_send_response(seq, CMD_TRANSFER_DATA, resp, sizeof(resp));
}

void ota_cmd_transfer_done(uint16_t seq, const uint8_t *data, uint16_t len)
{
    if (!ota_state.in_progress) {
        uint8_t fail[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        ota_send_response(seq, CMD_TRANSFER_DONE, fail, 5);
        return;
    }

    uint32_t declared_crc, declared_size;
    memcpy(&declared_crc, &data[0], 4);
    memcpy(&declared_size, &data[4], 4);

    if (declared_size != ota_state.received_size) {
        ota_state.error_code = ERR_FIRMWARE_SIZE;
        ota_state.status = STATUS_ERROR;
        uint8_t resp[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        ota_send_response(seq, CMD_TRANSFER_DONE, resp, 5);
        printk("Size mismatch: declared=%d, received=%d\n",
               declared_size, ota_state.received_size);
        return;
    }

    ota_state.firmware_size = declared_size;
    ota_state.firmware_crc = declared_crc;
    ota_state.status = STATUS_COMPLETE;

    uint8_t resp[5];
    resp[0] = 0x00;
    memcpy(&resp[1], &ota_state.received_size, 4);

    ota_send_response(seq, CMD_TRANSFER_DONE, resp, sizeof(resp));
    printk("Transfer done: size=%d, crc=0x%08x\n",
           ota_state.received_size, ota_state.calculated_crc);
}

void ota_cmd_verify_firmware(uint16_t seq, const uint8_t *data, uint16_t len)
{
    if (ota_state.status != STATUS_COMPLETE) {
        uint8_t fail[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        ota_send_response(seq, CMD_VERIFY_FIRMWARE, fail, 5);
        return;
    }

    ota_state.status = STATUS_VERIFYING;

    uint32_t crc = crc32_ieee(NULL, 0);

    uint8_t buf[256];
    uint32_t remaining = ota_state.firmware_size;
    uint32_t offset = 0;

    while (remaining > 0) {
        uint32_t chunk = remaining > sizeof(buf) ? sizeof(buf) : remaining;
        int err = flash_area_read(slot1_area, offset, buf, chunk);
        if (err) {
            ota_state.status = STATUS_ERROR;
            ota_state.error_code = ERR_FLASH_WRITE;
            uint8_t fail[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
            ota_send_response(seq, CMD_VERIFY_FIRMWARE, fail, 5);
            printk("Verify read failed: %d\n", err);
            return;
        }
        crc = crc32_ieee_update(crc, buf, chunk);
        offset += chunk;
        remaining -= chunk;
    }

    ota_state.calculated_crc = crc;

    uint8_t resp[5];
    if (crc == ota_state.firmware_crc) {
        resp[0] = 0x00;
        printk("CRC verify OK: 0x%08x\n", crc);
    } else {
        resp[0] = 0x01;
        ota_state.error_code = ERR_CRC_MISMATCH;
        printk("CRC mismatch: expected 0x%08x, got 0x%08x\n",
               ota_state.firmware_crc, crc);
    }
    memcpy(&resp[1], &ota_state.calculated_crc, 4);

    ota_send_response(seq, CMD_VERIFY_FIRMWARE, resp, sizeof(resp));
}

void ota_cmd_reset_run(uint16_t seq, const uint8_t *data, uint16_t len)
{
    if (ota_state.status != STATUS_COMPLETE) {
        uint8_t fail[1] = { 0xFF };
        ota_send_response(seq, CMD_RESET_RUN, fail, 1);
        return;
    }

    uint16_t delay_ms = 0;
    if (len >= 2) {
        memcpy(&delay_ms, &data[0], 2);
    }

    uint8_t resp[1] = { 0x00 };
    ota_send_response(seq, CMD_RESET_RUN, resp, 1);

    printk("Reset in %d ms\n", delay_ms);
    k_msleep(delay_ms);

    sys_reboot(SYS_REBOOT_COLD);
}

void ota_cmd_cancel_upgrade(uint16_t seq, const uint8_t *data, uint16_t len)
{
    ota_state.status = STATUS_IDLE;
    ota_state.error_code = ERR_NONE;
    ota_state.received_size = 0;
    ota_state.current_offset = 0;
    ota_state.in_progress = false;
    ota_state.calculated_crc = 0;

    uint8_t resp[1] = { 0x00 };
    ota_send_response(seq, CMD_CANCEL_UPGRADE, resp, 1);
    printk("Upgrade cancelled\n");
}

void ota_cmd_query_status(uint16_t seq, const uint8_t *data, uint16_t len)
{
    uint8_t resp[14];
    memset(resp, 0, sizeof(resp));

    resp[0] = ota_state.status;
    memcpy(&resp[1], &ota_state.received_size, 4);
    memcpy(&resp[5], &ota_state.firmware_size, 4);
    resp[9] = ota_state.error_code;

    ota_send_response(seq, CMD_QUERY_STATUS, resp, sizeof(resp));
}

void ota_cmd_set_boot_flag(uint16_t seq, const uint8_t *data, uint16_t len)
{
    if (len < 1) {
        uint8_t fail[1] = { 0xFF };
        ota_send_response(seq, CMD_SET_BOOT_FLAG, fail, 1);
        return;
    }

    uint8_t flag = data[0];

    if (flag == 0x01 && ota_state.status == STATUS_COMPLETE) {
        ota_state.boot_flag = 1;
        uint8_t resp[1] = { 0x00 };
        ota_send_response(seq, CMD_SET_BOOT_FLAG, resp, 1);
        printk("Boot flag set to new firmware\n");
    } else if (flag == 0x00) {
        ota_state.boot_flag = 0;
        uint8_t resp[1] = { 0x00 };
        ota_send_response(seq, CMD_SET_BOOT_FLAG, resp, 1);
        printk("Boot flag cleared\n");
    } else {
        uint8_t resp[1] = { 0xFF };
        ota_send_response(seq, CMD_SET_BOOT_FLAG, resp, 1);
        printk("Set boot flag failed\n");
    }
}
