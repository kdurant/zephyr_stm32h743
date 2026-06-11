/*
 * Zephyr UART DMA 接收 + 回显示例
 *
 * 使用 USART1 (PA9/PA10) 的异步 API，DMA 自动收发。
 * 收到任意数据后原样回显。
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

/* 使用控制台所在的 UART */
#define UART_NODE DT_CHOSEN(zephyr_console)
static const struct device *const uart = DEVICE_DT_GET(UART_NODE);

/* 双缓冲：DMA 收发可同时进行 */
#define BUF_SIZE 64
static uint8_t rx_buf[2][BUF_SIZE];
static uint8_t rx_buf_idx;

/* 接收超时: 收到第一个字节后，60ms 内无新字节则触发 UART_RX_RDY */
#define RX_TIMEOUT_US 60000

/*
 * UART 异步回调
 * 所有 UART 事件都在中断上下文中触发，不可调用阻塞函数。
 */
static void uart_cb(const struct device *dev, struct uart_event *evt,
		    void *user_data)
{
	int err;

	switch (evt->type) {

	case UART_RX_BUF_REQUEST:
		/* DMA 需要下一个缓冲区 */
		err = uart_rx_buf_rsp(dev,
				      rx_buf[rx_buf_idx], BUF_SIZE);
		__ASSERT_NO_MSG(err == 0);
		rx_buf_idx = rx_buf_idx ? 0 : 1;
		break;

	case UART_RX_RDY:
		/*
		 * 收到数据: evt->data.rx.buf 指向当前接收缓冲区
		 * evt->data.rx.offset 是数据起始偏移
		 * evt->data.rx.len 是接收到的字节数
		 */
		{
			uint8_t *data = &evt->data.rx.buf[evt->data.rx.offset];
			int len = evt->data.rx.len;

			/*
			 * 回显: 通过 DMA 发送回去
			 * uart_tx() 是异步的，立即返回，不阻塞
			 */
			err = uart_tx(dev, data, len, SYS_FOREVER_US);
			if (err) {
				printk("TX error %d\n", err);
			}
		}
		break;

	case UART_RX_BUF_RELEASED:
		/* DMA 释放了缓冲区（已被驱动处理完毕） */
		break;

	case UART_RX_DISABLED:
		/* 接收已停止 */
		break;

	case UART_TX_DONE:
		/* 发送完成 */
		break;

	case UART_TX_ABORTED:
		/* 发送被中止 */
		break;

	default:
		break;
	}
}

int main(void)
{
	int err;

	if (!device_is_ready(uart)) {
		printk("UART device not ready\n");
		return -1;
	}

	printk("\n=== UART DMA Echo Demo ===\n");
	printk("USART1 on PA9(TX)/PA10(RX), DMA mode\n");
	printk("Send any data, it will be echoed back.\n\n");

	/* 注册异步回调 */
	err = uart_callback_set(uart, uart_cb, NULL);
	if (err) {
		printk("uart_callback_set failed: %d\n", err);
		return err;
	}

	/* 启动 DMA 接收: 第一个缓冲区 + 超时 */
	rx_buf_idx = 1;
	err = uart_rx_enable(uart, rx_buf[0], BUF_SIZE, RX_TIMEOUT_US);
	if (err) {
		printk("uart_rx_enable failed: %d\n", err);
		return err;
	}

	printk("UART DMA RX started. Waiting for data...\n");

	/* 主循环空闲，所有收发由 DMA + 中断驱动 */
	while (1) {
		k_msleep(1000);
	}

	return 0;
}
