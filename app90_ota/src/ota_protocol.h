#ifndef OTA_PROTOCOL_H
#define OTA_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>

#define FRAME_HEADER_0      0x12
#define FRAME_HEADER_1      0x34
#define FRAME_TAIL_0        0xCD
#define FRAME_TAIL_1        0xEF

#define FRAME_MIN_SIZE      18
#define FRAME_MAX_DATA_LEN  65535

#define ADDR_PC             0x00
#define ADDR_COLLECT        0x10
#define ADDR_DEVICE         0x20

#define CMD_HANDSHAKE       0x0001
#define CMD_GET_DEV_INFO    0x0002
#define CMD_START_UPGRADE   0x0003
#define CMD_TRANSFER_DATA   0x0004
#define CMD_TRANSFER_DONE   0x0005
#define CMD_VERIFY_FIRMWARE 0x0006
#define CMD_RESET_RUN       0x0007
#define CMD_CANCEL_UPGRADE  0x0008
#define CMD_QUERY_STATUS    0x0009
#define CMD_SET_BOOT_FLAG   0x000A

#define STATUS_IDLE         0x00
#define STATUS_ERASING      0x01
#define STATUS_RECEIVING    0x02
#define STATUS_VERIFYING    0x03
#define STATUS_COMPLETE     0x04
#define STATUS_ERROR        0xFF

#define ERR_NONE            0x00
#define ERR_FIRMWARE_SIZE   0x01
#define ERR_FLASH_ERASE     0x02
#define ERR_FLASH_WRITE     0x03
#define ERR_CRC_MISMATCH    0x04
#define ERR_OFFSET_ERROR    0x05
#define ERR_TIMEOUT         0x06
#define ERR_VERSION_LOW     0x07
#define ERR_UNKNOWN         0xFF

#define MAX_PACKET_SIZE     1024

#pragma pack(push, 1)
typedef struct {
    uint8_t  header[2];
    uint16_t seq;
    uint16_t src_addr;
    uint16_t dst_addr;
    uint16_t total_packets;
    uint16_t packet_seq;
    uint16_t cmd;
    uint16_t data_len;
    uint8_t  data[FRAME_MAX_DATA_LEN];
    uint16_t reserved;
    uint8_t  tail[2];
} ota_frame_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t  header[2];
    uint16_t seq;
    uint16_t src_addr;
    uint16_t dst_addr;
    uint16_t total_packets;
    uint16_t packet_seq;
    uint16_t cmd;
    uint16_t data_len;
} ota_frame_header_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint16_t reserved;
    uint8_t  tail[2];
} ota_frame_tail_t;
#pragma pack(pop)

typedef void (*ota_cmd_handler_t)(uint16_t seq, const uint8_t *data, uint16_t len);

typedef struct {
    uint16_t cmd;
    ota_cmd_handler_t handler;
} ota_cmd_entry_t;

void ota_protocol_init(void);
int ota_frame_parse(const uint8_t *buf, uint16_t len, ota_frame_t *frame);
int ota_frame_build(ota_frame_t *frame, uint16_t seq, uint16_t src, uint16_t dst,
                    uint16_t total, uint16_t pkt_seq, uint16_t cmd,
                    const uint8_t *data, uint16_t data_len);
void ota_frame_process(const ota_frame_t *frame);
void ota_send_response(uint16_t seq, uint16_t cmd, const uint8_t *data, uint16_t len);
void ota_process_rx(void);

#endif
