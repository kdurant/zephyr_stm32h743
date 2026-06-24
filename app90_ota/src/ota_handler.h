#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t  status;
    uint8_t  error_code;
    uint32_t received_size;
    uint32_t firmware_size;
    uint32_t firmware_crc;
    uint32_t calculated_crc;
    uint32_t current_offset;
    uint16_t max_packet_size;
    uint8_t  boot_flag;
    uint32_t slot0_offset;
    uint32_t slot1_offset;
    uint32_t slot_size;
    bool     in_progress;
} ota_state_t;

extern ota_state_t ota_state;

void ota_handler_init(void);

void ota_cmd_handshake(uint16_t seq, const uint8_t *data, uint16_t len);
void ota_cmd_get_dev_info(uint16_t seq, const uint8_t *data, uint16_t len);
void ota_cmd_start_upgrade(uint16_t seq, const uint8_t *data, uint16_t len);
void ota_cmd_transfer_data(uint16_t seq, const uint8_t *data, uint16_t len);
void ota_cmd_transfer_done(uint16_t seq, const uint8_t *data, uint16_t len);
void ota_cmd_verify_firmware(uint16_t seq, const uint8_t *data, uint16_t len);
void ota_cmd_reset_run(uint16_t seq, const uint8_t *data, uint16_t len);
void ota_cmd_cancel_upgrade(uint16_t seq, const uint8_t *data, uint16_t len);
void ota_cmd_query_status(uint16_t seq, const uint8_t *data, uint16_t len);
void ota_cmd_set_boot_flag(uint16_t seq, const uint8_t *data, uint16_t len);

#endif
