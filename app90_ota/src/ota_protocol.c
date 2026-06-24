#include "ota_protocol.h"
#include "ota_handler.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <string.h>

#define UART_NODE DT_CHOSEN(zephyr_console)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_NODE);

#define RX_BUF_SIZE 4096
static uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_len;

static ota_cmd_entry_t cmd_table[] = {
    { CMD_HANDSHAKE,       ota_cmd_handshake },
    { CMD_GET_DEV_INFO,    ota_cmd_get_dev_info },
    { CMD_START_UPGRADE,   ota_cmd_start_upgrade },
    { CMD_TRANSFER_DATA,   ota_cmd_transfer_data },
    { CMD_TRANSFER_DONE,   ota_cmd_transfer_done },
    { CMD_VERIFY_FIRMWARE, ota_cmd_verify_firmware },
    { CMD_RESET_RUN,       ota_cmd_reset_run },
    { CMD_CANCEL_UPGRADE,  ota_cmd_cancel_upgrade },
    { CMD_QUERY_STATUS,    ota_cmd_query_status },
    { CMD_SET_BOOT_FLAG,   ota_cmd_set_boot_flag },
};

static void uart_cb(const struct device *dev, struct uart_event *evt,
                    void *user_data)
{
    switch (evt->type) {
    case UART_RX_RDY:
        {
            uint16_t copy_len = evt->data.rx.len;
            if (rx_len + copy_len <= RX_BUF_SIZE) {
                memcpy(&rx_buf[rx_len],
                       &evt->data.rx.buf[evt->data.rx.offset],
                       copy_len);
                rx_len += copy_len;
            }
        }
        break;
    case UART_RX_BUF_REQUEST:
        {
            int err = uart_rx_buf_rsp(dev, rx_buf + rx_len,
                                       RX_BUF_SIZE - rx_len);
            if (err) {
                printk("uart_rx_buf_rsp err %d\n", err);
            }
        }
        break;
    case UART_RX_BUF_RELEASED:
    case UART_RX_DISABLED:
    case UART_TX_DONE:
    case UART_TX_ABORTED:
        break;
    default:
        break;
    }
}

void ota_protocol_init(void)
{
    int err;

    if (!device_is_ready(uart_dev)) {
        printk("UART not ready\n");
        return;
    }

    err = uart_callback_set(uart_dev, uart_cb, NULL);
    if (err) {
        printk("uart_callback_set failed: %d\n", err);
        return;
    }

    rx_len = 0;
    err = uart_rx_enable(uart_dev, rx_buf, RX_BUF_SIZE, 100000);
    if (err) {
        printk("uart_rx_enable failed: %d\n", err);
        return;
    }

    printk("OTA protocol initialized\n");
}

static uint16_t find_frame_start(const uint8_t *buf, uint16_t len, uint16_t offset)
{
    for (uint16_t i = offset; i < len - 1; i++) {
        if (buf[i] == FRAME_HEADER_0 && buf[i + 1] == FRAME_HEADER_1) {
            return i;
        }
    }
    return len;
}

int ota_frame_parse(const uint8_t *buf, uint16_t len, ota_frame_t *frame)
{
    if (len < FRAME_MIN_SIZE) {
        return -1;
    }

    uint16_t pos = 0;
    while (pos <= len - FRAME_MIN_SIZE) {
        if (buf[pos] == FRAME_HEADER_0 && buf[pos + 1] == FRAME_HEADER_1) {
            break;
        }
        pos++;
    }

    if (pos > len - FRAME_MIN_SIZE) {
        return -1;
    }

    const ota_frame_header_t *hdr = (const ota_frame_header_t *)&buf[pos];
    uint16_t data_len = hdr->data_len;

    uint16_t total_frame_len = sizeof(ota_frame_header_t) + data_len + sizeof(ota_frame_tail_t);
    if (pos + total_frame_len > len) {
        return -1;
    }

    memcpy(frame->header, hdr->header, 2);
    frame->seq = hdr->seq;
    frame->src_addr = hdr->src_addr;
    frame->dst_addr = hdr->dst_addr;
    frame->total_packets = hdr->total_packets;
    frame->packet_seq = hdr->packet_seq;
    frame->cmd = hdr->cmd;
    frame->data_len = data_len;

    if (data_len > 0) {
        memcpy(frame->data, &buf[pos + sizeof(ota_frame_header_t)], data_len);
    }

    const ota_frame_tail_t *tail =
        (const ota_frame_tail_t *)&buf[pos + sizeof(ota_frame_header_t) + data_len];
    frame->reserved = tail->reserved;
    frame->tail[0] = tail->tail[0];
    frame->tail[1] = tail->tail[1];

    return total_frame_len;
}

int ota_frame_build(ota_frame_t *frame, uint16_t seq, uint16_t src, uint16_t dst,
                    uint16_t total, uint16_t pkt_seq, uint16_t cmd,
                    const uint8_t *data, uint16_t data_len)
{
    frame->header[0] = FRAME_HEADER_0;
    frame->header[1] = FRAME_HEADER_1;
    frame->seq = seq;
    frame->src_addr = src;
    frame->dst_addr = dst;
    frame->total_packets = total;
    frame->packet_seq = pkt_seq;
    frame->cmd = cmd;
    frame->data_len = data_len;

    if (data && data_len > 0) {
        memcpy(frame->data, data, data_len);
    }

    frame->reserved = 0;
    frame->tail[0] = FRAME_TAIL_0;
    frame->tail[1] = FRAME_TAIL_1;

    return sizeof(ota_frame_header_t) + data_len + sizeof(ota_frame_tail_t);
}

void ota_send_response(uint16_t seq, uint16_t cmd, const uint8_t *data, uint16_t len)
{
    ota_frame_t resp;
    uint16_t frame_len = ota_frame_build(&resp, seq, ADDR_DEVICE, ADDR_PC,
                                         1, 0, cmd, data, len);
    uart_tx(uart_dev, (uint8_t *)&resp, frame_len, SYS_FOREVER_US);
}

void ota_frame_process(const ota_frame_t *frame)
{
    if (frame->dst_addr != ADDR_DEVICE && frame->dst_addr != 0xFF) {
        return;
    }

    for (int i = 0; i < ARRAY_SIZE(cmd_table); i++) {
        if (cmd_table[i].cmd == frame->cmd) {
            cmd_table[i].handler(frame->seq, frame->data, frame->data_len);
            return;
        }
    }

    uint8_t fail_resp[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    ota_send_response(frame->seq, frame->cmd, fail_resp, 4);
}

void ota_process_rx(void)
{
    uint16_t processed = 0;

    while (processed < rx_len) {
        ota_frame_t frame;
        int frame_len = ota_frame_parse(&rx_buf[processed], rx_len - processed, &frame);

        if (frame_len <= 0) {
            break;
        }

        ota_frame_process(&frame);
        processed += frame_len;
    }

    if (processed > 0 && processed < rx_len) {
        memmove(rx_buf, &rx_buf[processed], rx_len - processed);
        rx_len -= processed;
    } else if (processed >= rx_len) {
        rx_len = 0;
    }
}
