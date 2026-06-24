/* SPDX-License-Identifier: Apache-2.0 */
#include "frame.h"
#include <string.h>

/* ── 帧解析器 ── */

void frame_decoder_init(frame_decoder_t* dec)
{
    memset(dec, 0, sizeof(*dec));
    dec->state = FRAME_STATE_HEADER0;
}

void frame_decoder_feed(frame_decoder_t* dec, uint8_t byte)
{
    /* Don't process new data until previous frame is consumed */
    if(dec->frame_ready)
    {
        return;
    }

    switch(dec->state)
    {
        case FRAME_STATE_HEADER0:
            if(byte == FRAME_HEADER0)
            {
                dec->state = FRAME_STATE_HEADER1;
            }
            /* else: stay, skip garbage */
            break;

        case FRAME_STATE_HEADER1:
            if(byte == FRAME_HEADER1)
            {
                dec->state = FRAME_STATE_FIXED;
                dec->pos   = 0;
            }
            else if(byte == FRAME_HEADER0)
            {
                /* Restart at header0 */
                dec->state = FRAME_STATE_HEADER1;
            }
            else
            {
                dec->state = FRAME_STATE_HEADER0;
            }
            break;

        case FRAME_STATE_FIXED:
            dec->fixed_buf[dec->pos++] = byte;
            if(dec->pos >= FIXED_FIELD_SIZE)
            {
                /* Parse fixed fields */
                int p          = 0;
                dec->frame.seq = dec->fixed_buf[p] | (dec->fixed_buf[p + 1] << 8);
                p += 2;
                dec->frame.src_addr = dec->fixed_buf[p] | (dec->fixed_buf[p + 1] << 8);
                p += 2;
                dec->frame.dst_addr = dec->fixed_buf[p] | (dec->fixed_buf[p + 1] << 8);
                p += 2;
                dec->frame.total_packets = dec->fixed_buf[p] | (dec->fixed_buf[p + 1] << 8);
                p += 2;
                dec->frame.packet_seq = dec->fixed_buf[p] | (dec->fixed_buf[p + 1] << 8);
                p += 2;
                dec->frame.command = dec->fixed_buf[p] | (dec->fixed_buf[p + 1] << 8);
                p += 2;
                dec->frame.data_len = dec->fixed_buf[p] | (dec->fixed_buf[p + 1] << 8);
                p += 2;

                if(dec->frame.data_len > MAX_DATA_SIZE)
                {
                    /* Invalid — reset */
                    frame_decoder_reset(dec);
                    return;
                }

                dec->expected_data_len = dec->frame.data_len;
                dec->pos               = 0;

                if(dec->expected_data_len == 0)
                {
                    dec->state = FRAME_STATE_RSV_TAIL;
                    dec->pos   = 0;
                }
                else
                {
                    dec->state = FRAME_STATE_DATA;
                }
            }
            break;

        case FRAME_STATE_DATA:
            dec->frame.data[dec->pos++] = byte;
            if(dec->pos >= dec->expected_data_len)
            {
                dec->state = FRAME_STATE_RSV_TAIL;
                dec->pos   = 0;
            }
            break;

        case FRAME_STATE_RSV_TAIL:
            dec->pos++;
            if(dec->pos == 3)
            {
                /* First tail byte */
                if(byte != FRAME_TAIL0)
                {
                    frame_decoder_reset(dec);
                    if(byte == FRAME_HEADER0)
                    {
                        dec->state = FRAME_STATE_HEADER1;
                    }
                    return;
                }
            }
            else if(dec->pos == 4)
            {
                if(byte == FRAME_TAIL1)
                {
                    dec->frame_ready = true;
                }
                /* Stay in RSV_TAIL state until consumer resets */
            }
            break;
    }
}

bool frame_decoder_ready(const frame_decoder_t* dec)
{
    return dec->frame_ready;
}

const frame_t* frame_decoder_get(const frame_decoder_t* dec)
{
    return dec->frame_ready ? &dec->frame : NULL;
}

void frame_decoder_reset(frame_decoder_t* dec)
{
    /* Preserve frame_ready if it's set — clear only after consumed */
    bool was_ready         = dec->frame_ready;
    dec->state             = FRAME_STATE_HEADER0;
    dec->pos               = 0;
    dec->expected_data_len = 0;
    if(!was_ready)
    {
        /* Keep the frame data if it was ready; only clear when explicitly reset after get */
    }
    dec->frame_ready = false;
}

/* ── 帧序列化 ── */

int frame_to_bytes(const frame_t* f, uint8_t* buf, int buf_size)
{
    int total = FRAME_OVERHEAD + f->data_len;
    if(buf_size < total)
    {
        return -1;
    }

    int pos    = 0;
    buf[pos++] = FRAME_HEADER0;
    buf[pos++] = FRAME_HEADER1;
    buf[pos++] = (uint8_t)(f->seq & 0xFF);
    buf[pos++] = (uint8_t)(f->seq >> 8);
    buf[pos++] = (uint8_t)(f->src_addr & 0xFF);
    buf[pos++] = (uint8_t)(f->src_addr >> 8);
    buf[pos++] = (uint8_t)(f->dst_addr & 0xFF);
    buf[pos++] = (uint8_t)(f->dst_addr >> 8);
    buf[pos++] = (uint8_t)(f->total_packets & 0xFF);
    buf[pos++] = (uint8_t)(f->total_packets >> 8);
    buf[pos++] = (uint8_t)(f->packet_seq & 0xFF);
    buf[pos++] = (uint8_t)(f->packet_seq >> 8);
    buf[pos++] = (uint8_t)(f->command & 0xFF);
    buf[pos++] = (uint8_t)(f->command >> 8);
    buf[pos++] = (uint8_t)(f->data_len & 0xFF);
    buf[pos++] = (uint8_t)(f->data_len >> 8);

    if(f->data_len > 0)
    {
        memcpy(&buf[pos], f->data, f->data_len);
        pos += f->data_len;
    }

    buf[pos++] = 0x00; /* rsv */
    buf[pos++] = 0x00; /* rsv */
    buf[pos++] = FRAME_TAIL0;
    buf[pos++] = FRAME_TAIL1;

    return pos;
}

/* ── 响应帧构建 ── */

void frame_build_error_resp(const frame_t* req, frame_t* resp)
{
    memset(resp, 0, sizeof(*resp));
    resp->seq           = req->seq;
    resp->src_addr      = ADDR_DEVICE;
    resp->dst_addr      = req->src_addr;
    resp->command       = req->command;
    resp->total_packets = 1;
    resp->packet_seq    = 0;
    resp->data_len      = 4;
    memset(resp->data, ERROR_MARKER_BYTE, 4);
}

void frame_build_resp(const frame_t* req, frame_t* resp,
                      const uint8_t* data, uint16_t data_len)
{
    memset(resp, 0, sizeof(*resp));
    resp->seq           = req->seq;
    resp->src_addr      = ADDR_DEVICE;
    resp->dst_addr      = req->src_addr;
    resp->command       = req->command;
    resp->total_packets = 1;
    resp->packet_seq    = 0;
    resp->data_len      = data_len;
    if(data_len > 0 && data_len <= MAX_DATA_SIZE)
    {
        memcpy(resp->data, data, data_len);
    }
}
