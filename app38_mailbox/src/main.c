/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

/*
 * Zephyr RTOS Mailbox 使用示例
 *
 * Mailbox 是一种增强的消息传递机制，相比 MSGQ 多了以下能力：
 * - 同步发送: 发送方可以等待接收方处理完毕才返回
 * - 异步发送: 发送方立即返回，可收到处理完成的信号量通知
 * - 双向通信: 接收方可获取发送方线程 ID 以回复
 * - 独立数据: tx_data 和 info 分开，info 可作消息类型标识
 *
 * 核心结构体:
 *   struct k_mbox_msg { size; info; tx_data; rx_source_thread; ... }
 *
 * 本示例演示两种场景：
 * 1. 同步发送-接收（发送方阻塞等接收方处理完）
 * 2. 异步发送 + 信号量通知
 */

// ==================== 示例1: 同步 Mailbox ====================

K_MBOX_DEFINE(sync_mbox);

struct sensor_msg {
	int  sensor_id;
	int  temperature;
	int  humidity;
};

/* 发送线程: 同步发送，等接收方处理完才继续 */
void sync_sender_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct sensor_msg data;
	struct k_mbox_msg tx_msg;

	while (1) {
		k_msleep(3000);

		/* 准备传感器数据 */
		data.sensor_id++;
		data.temperature = 25 + (data.sensor_id % 10);
		data.humidity    = 55 + (data.sensor_id % 20);

		/* 填充消息描述符 */
		tx_msg.info    = data.sensor_id;  /* info 用作消息序号 */
		tx_msg.size    = sizeof(data);     /* 数据大小 */
		tx_msg.tx_data = &data;            /* 数据指针 */
		/* tx_target_thread = NULL 表示不指定接收线程 */

		printk("[SyncSender] 同步发送 sensor_id=%d temp=%d hum=%d\n",
		       data.sensor_id, data.temperature, data.humidity);

		/*
		 * k_mbox_put: 同步发送，阻塞到接收方处理完成
		 * 参数: mbox, tx_msg, 等待接收方的超时
		 * 注意: 一旦被接收，会继续等待接收方调用 k_mbox_data_get
		 */
		k_mbox_put(&sync_mbox, &tx_msg, K_FOREVER);

		printk("[SyncSender] << 确认接收方已处理完毕\n");
	}
}

/* 接收线程: 接收并处理 */
void sync_receiver_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct sensor_msg data;
	struct k_mbox_msg rx_msg;

	while (1) {
		printk("[SyncReceiver] 等待消息...\n");

		/*
		 * k_mbox_get: 接收消息
		 * 参数: mbox, rx_msg(输出), 数据缓冲区, 超时
		 * buffer != NULL: 同时取回数据
		 */
		k_mbox_get(&sync_mbox, &rx_msg, &data, K_FOREVER);

		printk("[SyncReceiver] << 收到: sensor_id=%d temp=%d hum=%d"
		       " (info=%d)\n",
		       data.sensor_id, data.temperature,
		       data.humidity, rx_msg.info);

		/* 模拟处理数据 */
		k_msleep(1000);
		printk("[SyncReceiver] 数据处理完成，释放发送方\n");

		/*
		 * k_mbox_data_get: 通知发送方数据已处理完毕
		 * buffer = NULL 表示仅做确认，不额外取数据
		 * （k_mbox_get 中已通过 buffer 参数取回数据时不需要再调用）
		 * 注意: 如果 k_mbox_get 中 buffer=NULL 做了延迟接收，
		 *       则这里才真正取数据。
		 */
	}
}

// ==================== 示例2: 异步发送 + 信号量通知 ====================

K_MBOX_DEFINE(async_mbox);

/* 异步消息结构 */
struct async_msg {
	char text[32];
};

/* 信号量: 通知发送方消息已处理完 */
static struct k_sem done_sem;

/* 异步发送线程 */
void async_sender_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct async_msg data;
	struct k_mbox_msg tx_msg;
	int count = 0;

	while (1) {
		k_msleep(2000);
		count++;
		snprintf(data.text, sizeof(data.text), "async-msg-%d", count);

		tx_msg.info    = count;
		tx_msg.size    = sizeof(data);
		tx_msg.tx_data = &data;

		printk("[AsyncSender] 异步发送: %s\n", data.text);

		/*
		 * k_mbox_async_put: 异步发送，立即返回不等待
		 * 参数: mbox, tx_msg, sem(处理完成时触发)
		 */
		k_mbox_async_put(&async_mbox, &tx_msg, &done_sem);

		printk("[AsyncSender] 发送完毕，继续做其他事...\n");
		k_msleep(500);
		printk("[AsyncSender] 等待处理完成通知...\n");

		/* 等待处理完成的信号量 */
		k_sem_take(&done_sem, K_FOREVER);

		printk("[AsyncSender] << 收到信号量，确认已处理完成\n");
	}
}

/* 异步接收线程 */
void async_receiver_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct async_msg data;
	struct k_mbox_msg rx_msg;

	while (1) {
		printk("[AsyncReceiver] 等待消息...\n");

		/* 先接收消息但不取数据（延迟接收） */
		k_mbox_get(&async_mbox, &rx_msg, NULL, K_FOREVER);

		printk("[AsyncReceiver] << 收到消息 (info=%d)\n", rx_msg.info);

		/* 模拟一部分处理 */
		k_msleep(500);

		/*
		 * k_mbox_data_get: 真正取回数据，并释放发送方
		 * 如果发送方在等待信号量，此时信号量会被 give
		 */
		k_mbox_data_get(&rx_msg, &data);
		printk("[AsyncReceiver] 数据: %s, 已通知发送方\n", data.text);

		k_msleep(500);
	}
}

// ==================== 主函数 ====================

K_THREAD_STACK_DEFINE(sync_sender_stack, 1024);
K_THREAD_STACK_DEFINE(sync_receiver_stack, 1024);
K_THREAD_STACK_DEFINE(async_sender_stack, 1024);
K_THREAD_STACK_DEFINE(async_receiver_stack, 1024);

static struct k_thread sync_sender_data;
static struct k_thread sync_receiver_data;
static struct k_thread async_sender_data;
static struct k_thread async_receiver_data;

int main(void)
{
	printk("\n========================================\n");
	printk("Zephyr Mailbox 使用示例\n");
	printk("========================================\n\n");

	// --- 示例1: 同步 Mailbox ---
	printk("--- 示例1: 同步发送-接收 ---\n");
	printk("说明: 发送方阻塞，直到接收方处理完数据才返回。\n");
	printk("      info 字段用作消息序号。\n\n");

	k_thread_create(&sync_sender_data, sync_sender_stack,
			K_THREAD_STACK_SIZEOF(sync_sender_stack),
			sync_sender_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&sync_receiver_data, sync_receiver_stack,
			K_THREAD_STACK_SIZEOF(sync_receiver_stack),
			sync_receiver_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(10000);

	// --- 示例2: 异步发送 + 信号量 ---
	printk("\n--- 示例2: 异步发送 + 信号量通知 ---\n");
	printk("说明: 发送方立即返回，通过信号量获知处理完成。\n");
	printk("      接收方用 k_mbox_data_get 延迟取数据。\n\n");

	k_sem_init(&done_sem, 0, 1);

	k_thread_create(&async_sender_data, async_sender_stack,
			K_THREAD_STACK_SIZEOF(async_sender_stack),
			async_sender_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&async_receiver_data, async_receiver_stack,
			K_THREAD_STACK_SIZEOF(async_receiver_stack),
			async_receiver_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(12000);

	printk("\n========================================\n");
	printk("所有示例演示完毕\n");
	printk("========================================\n\n");

	return 0;
}
