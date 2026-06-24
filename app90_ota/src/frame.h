/* SPDX-License-Identifier: Apache-2.0 */
/* OTA 协议帧格式定义 (与上位机协议一致) */

#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── 帧常量 ── */
#define FRAME_HEADER0 0x12
#define FRAME_HEADER1 0x34
#define FRAME_TAIL0   0xCD
#define FRAME_TAIL1   0xEF

#define HEADER_SIZE      2
#define FIXED_FIELD_SIZE 14                               /* seq+src+dst+total+pkt_seq+cmd+len = 14 */
#define PREFIX_SIZE      (HEADER_SIZE + FIXED_FIELD_SIZE) /* 16 */
#define SUFFIX_SIZE      4                                /* rsv(2) + tail(2) */
#define FRAME_OVERHEAD   (PREFIX_SIZE + SUFFIX_SIZE)      /* 20 */
#define MAX_DATA_SIZE    4096
#define MAX_FRAME_SIZE   (FRAME_OVERHEAD + MAX_DATA_SIZE)
#define MIN_FRAME_SIZE   FRAME_OVERHEAD

/* ── 地址常量 ── */
#define ADDR_PC     0x0000
#define ADDR_DEVICE 0x0020
#define ADDR_DAQ    0x0010

/* ── 指令码 ── */
#define CMD_HANDSHAKE         0x0001
#define CMD_GET_DEVICE_INFO   0x0002
#define CMD_START_UPGRADE     0x0003
#define CMD_TRANSFER_DATA     0x0004
#define CMD_TRANSFER_COMPLETE 0x0005
#define CMD_VERIFY_FIRMWARE   0x0006
#define CMD_RESET_RUN         0x0007
#define CMD_CANCEL_UPGRADE    0x0008
#define CMD_QUERY_STATUS      0x0009
#define CMD_SET_BOOT_FLAG     0x000A
#define CMD_GET_LOG           0x000B

/* ── 错误标记 ── */
#define ERROR_MARKER_BYTE 0xFF

/* ── 帧结构体 ── */
typedef struct
{
    uint16_t seq;
    uint16_t src_addr;
    uint16_t dst_addr;
    uint16_t total_packets;
    uint16_t packet_seq;
    uint16_t command;
    uint16_t data_len;
    uint8_t  data[MAX_DATA_SIZE];
} frame_t;

/* ── 帧解析器状态机 ── */
typedef enum
{
    FRAME_STATE_HEADER0,
    FRAME_STATE_HEADER1,
    FRAME_STATE_FIXED,
    FRAME_STATE_DATA,
    FRAME_STATE_RSV_TAIL,
} frame_state_t;

typedef struct
{
    frame_state_t state;
    uint16_t      pos;
    uint8_t       fixed_buf[FIXED_FIELD_SIZE];
    uint16_t      expected_data_len;
    frame_t       frame;
    bool          frame_ready;
} frame_decoder_t;

/* ── API ── */
void           frame_decoder_init(frame_decoder_t* dec);
void           frame_decoder_feed(frame_decoder_t* dec, uint8_t byte);
bool           frame_decoder_ready(const frame_decoder_t* dec);
const frame_t* frame_decoder_get(const frame_decoder_t* dec);
void           frame_decoder_reset(frame_decoder_t* dec);

/* 将 frame 序列化为字节流，返回实际长度 */
int frame_to_bytes(const frame_t* f, uint8_t* buf, int buf_size);

/* 构建响应帧（数据区填充 4 字节 0xFF 表示错误） */
void frame_build_error_resp(const frame_t* req, frame_t* resp);
void frame_build_resp(const frame_t* req, frame_t* resp,
                      const uint8_t* data, uint16_t data_len);

#endif /* FRAME_H */
