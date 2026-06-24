/* SPDX-License-Identifier: Apache-2.0 */
/*
 * OTA 固件升级 — MCU 端 (STM32H743 + Zephyr)
 * 架构: UART ISR → ring buffer → semaphore → dedicated processing thread
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>

#include "frame.h"
#include "ota_handler.h"
#include "log_buf.h"

#ifndef APP_VERSION_STRING
#define APP_VERSION_STRING "unknown"
#endif

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define UART_NODE DT_CHOSEN(zephyr_console)
#define LED0_NODE DT_ALIAS(led0)

static const struct device* const uart = DEVICE_DT_GET(UART_NODE);

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
#include <zephyr/drivers/gpio.h>
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#endif

#define RINGBUF_SIZE     4096
#define FRAME_STACK_SIZE 4096

RING_BUF_DECLARE(uart_ringbuf, RINGBUF_SIZE);
K_THREAD_STACK_DEFINE(frame_stack, FRAME_STACK_SIZE);
K_SEM_DEFINE(g_frame_sem, 0, 1);

static frame_decoder_t g_decoder;
static struct k_thread frame_thread_data;
volatile bool          g_reset_requested = false;

static void send_response(const frame_t* resp)
{
    static uint8_t tx_buf[MAX_FRAME_SIZE];
    int            len = frame_to_bytes(resp, tx_buf, sizeof(tx_buf));
    if(len > 0)
    {
        for(int i = 0; i < len; i++)
        {
            uart_poll_out(uart, tx_buf[i]);
        }
    }
}

static void uart_isr(const struct device* dev, void* user_data)
{
    ARG_UNUSED(user_data);
    uart_irq_update(dev);
    while(uart_irq_is_pending(dev))
    {
        if(uart_irq_rx_ready(dev))
        {
            uint8_t byte;
            uart_fifo_read(dev, &byte, 1);
            ring_buf_put(&uart_ringbuf, &byte, 1);
        }
        uart_irq_update(dev);
    }
    k_sem_give(&g_frame_sem);
}

static void frame_thread_fn(void* a, void* b, void* c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);
    while(1)
    {
        k_sem_take(&g_frame_sem, K_FOREVER);
        uint8_t byte;
        while(ring_buf_get(&uart_ringbuf, &byte, 1) == 1)
        {
            frame_decoder_feed(&g_decoder, byte);
            if(frame_decoder_ready(&g_decoder))
            {
                const frame_t* req = frame_decoder_get(&g_decoder);
                if(req)
                {
                    static frame_t resp;
                    if(ota_handle_frame(req, &resp))
                    {
                        send_response(&resp);
                    }
                }
                frame_decoder_reset(&g_decoder);
            }
        }
    }
}

static const device_info_t g_device_info = {
    .mcu_model        = "STM32H743XIT",
    .fw_ver           = {1, 0, 5, 0}, /* V1.0.1 */
    .flash_total_size = 2 * 1024 * 1024,
    .flash_page_size  = 4096,
    .flash_used_size  = 256 * 1024,
    .fw_start_addr    = 0x00000000,
};

int main(void)
{
    LOG_INF("\n=== OTA Firmware Upgrade MCU ===\n");
    LOG_INF("Build version: %s", APP_VERSION_STRING);
    if(!device_is_ready(uart))
    {
        LOG_ERR("UART device not ready");
        return -1;
    }
    uart_irq_callback_set(uart, uart_isr);
    uart_irq_rx_enable(uart);
    LOG_INF("UART initialized (USART1, interrupt mode)");
#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
    if(gpio_is_ready_dt(&led0))
    {
        gpio_pin_configure_dt(&led0, GPIO_OUTPUT);
        gpio_pin_set_dt(&led0, 0);
    }
#endif
    frame_decoder_init(&g_decoder);
    log_buf_init();
    ota_handler_init(&g_device_info);
    k_thread_create(&frame_thread_data, frame_stack, FRAME_STACK_SIZE,
                    frame_thread_fn, NULL, NULL, NULL,
                    K_PRIO_COOP(7), 0, K_NO_WAIT);
    LOG_INF("MCU ready. Waiting for commands...\n");
    while(1)
    {
        if(g_reset_requested)
        {
            LOG_INF("Resetting system...");
            k_sleep(K_MSEC(100));
#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
            gpio_pin_set_dt(&led0, 1);
#endif
            sys_reboot(SYS_REBOOT_COLD);
        }
        k_sleep(K_MSEC(100));
    }
    return 0;
}
