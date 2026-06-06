/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

/*
 * Zephyr RTOS Pipe 使用示例
 *
 * Pipe 是一个字节流（byte stream）管道，类似 Unix 管道：
 *   - 写入端用 k_pipe_write() 写入任意字节
 *   - 读取端用 k_pipe_read() 读取字节
 *   - 内部使用环形缓冲区，支持部分读写
 *   - 可 k_pipe_reset() 清空管道
 *
 * 与 Message Queue 区别:
 *   MSGQ: 固定大小消息，按消息单位传递（复制整个 struct）
 *   Pipe:  字节流，可写入/读取任意长度，无消息边界
 *
 * 本示例演示：
 * 1. 基本字节流读写
 * 2. 部分读写 + 管道重置
 */

// ==================== 示例1: 基本字节流读写 ====================

/*
 * K_PIPE_DEFINE: 静态定义管道（带内部缓冲区）
 * 参数: 名称, 缓冲区大小(字节), 对齐
 */
K_PIPE_DEFINE(data_pipe, 64, 4);

/* 写入线程: 周期性写入字符串 */
void writer_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int count = 0;

	while (1) {
		k_msleep(2000);
		count++;

		char msg[32];
		int len = snprintf(msg, sizeof(msg), "Hello-%d", count);

		printk("[Writer] 写入: \"%s\" (%d 字节)\n", msg, len);

		/*
		 * k_pipe_write: 写入字节流
		 * 参数: pipe, 数据, 长度, 超时
		 * 返回值: 实际写入的字节数，负值=错误
		 * 如果管道满则阻塞等待空间
		 */
		int written = k_pipe_write(&data_pipe, (uint8_t *)msg, len,
					    K_FOREVER);

		printk("[Writer] 实际写入 %d 字节\n", written);
	}
}

/* 读取线程: 从管道读取字节 */
void reader_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint8_t buf[64];

	while (1) {
		printk("[Reader] 等待数据...\n");

		/*
		 * k_pipe_read: 读取字节流
		 * 参数: pipe, 缓冲区, 期望读取长度, 超时
		 * 返回值: 实际读取的字节数
		 * 注意: 可能读到少于期望的字节数（部分读取）
		 */
		int n = k_pipe_read(&data_pipe, buf, sizeof(buf),
				    K_FOREVER);

		if (n > 0) {
			buf[n] = '\0';
			printk("[Reader] << 读到 %d 字节: \"%s\"\n",
			       n, (char *)buf);
		}
	}
}

// ==================== 示例2: 部分读写 + 管道重置 ====================

/* 使用 k_pipe_init 动态创建（运行时初始化） */
static struct k_pipe cmd_pipe;
static uint8_t cmd_pipe_buf[32];  /* 管道内部环形缓冲区 */

/* 命令写入: 写入大块数据，可能部分写入 */
void cmd_writer_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_msleep(3000);

		/* 写入一条超过缓冲区大小的命令（会被截断） */
		char cmd[] = "CMD:START_CONFIGURATION_PROCESS=1";
		int len = sizeof(cmd) - 1;  /* 去掉 \0 */

		printk("[CmdWriter] 写入 %d 字节: \"%s\"\n", len, cmd);

		/*
		 * 管道缓冲区只有 32 字节，cmd 长 36 字节
		 * k_pipe_write 会阻塞等消费者读走数据腾出空间
		 * 或者用 K_NO_WAIT 立即返回实际写入的字节数
		 */
		int written = k_pipe_write(&cmd_pipe, (uint8_t *)cmd, len,
					    K_NO_WAIT);

		printk("[CmdWriter] 实际写入 %d 字节 (管道满则部分写入)\n",
		       written);
	}
}

/* 命令处理: 分次读取 + 管道重置演示 */
void cmd_reader_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint8_t buf[8];  /* 小缓冲区，演示部分读取 */
	int reset_count = 0;

	while (1) {
		/*
		 * 每次只读一小段（最多 8 字节）
		 * k_pipe_read 返回实际读到的字节数
		 */
		int n = k_pipe_read(&cmd_pipe, buf, sizeof(buf),
				    K_MSEC(5000));

		if (n > 0) {
			buf[n] = '\0';
			printk("[CmdReader] << 读到 %d 字节: \"%s\"\n",
			       n, (char *)buf);
		} else {
			printk("[CmdReader] 读取超时 (n=%d)\n", n);
		}

		reset_count++;

		if (reset_count == 3) {
			/*
			 * k_pipe_reset: 清空管道中所有未读数据
			 * 所有阻塞的读写线程都会返回 -ECANCELED
			 */
			printk("[CmdReader] 重置管道，丢弃未读数据\n");
			k_pipe_reset(&cmd_pipe);
			reset_count = 0;
		}
	}
}

// ==================== 主函数 ====================

K_THREAD_STACK_DEFINE(writer_stack, 1024);
K_THREAD_STACK_DEFINE(reader_stack, 1024);
K_THREAD_STACK_DEFINE(cmd_writer_stack, 1024);
K_THREAD_STACK_DEFINE(cmd_reader_stack, 1024);

static struct k_thread writer_data;
static struct k_thread reader_data;
static struct k_thread cmd_writer_data;
static struct k_thread cmd_reader_data;

int main(void)
{
	printk("\n========================================\n");
	printk("Zephyr Pipe 使用示例\n");
	printk("========================================\n\n");

	// --- 示例1: 基本字节流读写 ---
	printk("--- 示例1: 基本字节流读写 ---\n");
	printk("说明: Pipe 是字节流，写入任意长度，读取任意长度。\n");
	printk("      K_PIPE_DEFINE 自动分配内部 64 字节缓冲区。\n\n");

	k_thread_create(&writer_data, writer_stack,
			K_THREAD_STACK_SIZEOF(writer_stack),
			writer_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&reader_data, reader_stack,
			K_THREAD_STACK_SIZEOF(reader_stack),
			reader_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(8000);

	// --- 示例2: 部分读写 + 管道重置 ---
	printk("\n--- 示例2: 部分读写 + 管道重置 ---\n");
	printk("说明: 缓冲区仅 32 字节，大数据可能部分写入。\n");
	printk("      用 k_pipe_reset 清空管道未读数据。\n\n");

	k_pipe_init(&cmd_pipe, cmd_pipe_buf, sizeof(cmd_pipe_buf));

	k_thread_create(&cmd_writer_data, cmd_writer_stack,
			K_THREAD_STACK_SIZEOF(cmd_writer_stack),
			cmd_writer_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&cmd_reader_data, cmd_reader_stack,
			K_THREAD_STACK_SIZEOF(cmd_reader_stack),
			cmd_reader_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(12000);

	printk("\n========================================\n");
	printk("所有示例演示完毕\n");
	printk("========================================\n\n");

	return 0;
}
