/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

/*
 * Zephyr RTOS Message Queue 使用示例
 *
 * Message Queue 是线程间传递数据的机制。每条消息按固定大小复制到队列缓冲区中。
 * 常用于生产者-消费者模式，发送方 put 消息，接收方 get 消息。
 *
 * 本示例演示了两种常见使用场景：
 * 1. 基本的生产者-消费者消息传递
 * 2. 带超时的消息队列操作
 */

/* 定义消息结构体 */
struct sensor_data {
	int id;
	int temperature;
	int humidity;
	uint32_t timestamp;
};

// ==================== 示例1: 基本生产者-消费者 ====================

/*
 * K_MSGQ_DEFINE: 静态定义消息队列（编译时创建）
 * 参数: 队列名, 每条消息大小, 最大消息数, 对齐字节
 */
K_MSGQ_DEFINE(sensor_msgq, sizeof(struct sensor_data), 5, 4);

/* 传感器线程: 周期性采集数据并发送 */
void sensor_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct sensor_data data;

	while (1) {
		k_msleep(1500);

		/* 采集传感器数据 */
		data.id++;
		data.temperature = 25 + (data.id % 10);
		data.humidity    = 60 + (data.id % 20);
		data.timestamp   = k_uptime_get_32();

		/*
		 * k_msgq_put: 发送消息到队列
		 * 参数: 队列, 数据指针, 超时（K_NO_WAIT=非阻塞, K_FOREVER=阻塞等待）
		 * 返回值: 0=成功, -ENOMSG=队列满且K_NO_WAIT
		 */
		int ret = k_msgq_put(&sensor_msgq, &data, K_NO_WAIT);

		if (ret == 0) {
			printk("[Sensor] 发送传感器数据: id=%d temp=%d hum=%d\n",
			       data.id, data.temperature, data.humidity);
		} else {
			printk("[Sensor] 队列已满，丢弃数据: id=%d\n", data.id);
		}
	}
}

/* 处理器线程: 从队列取出数据并处理 */
void processor_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct sensor_data data;

	while (1) {
		/*
		 * k_msgq_get: 从队列取出消息
		 * 参数: 队列, 数据缓冲区, 超时
		 */
		int ret = k_msgq_get(&sensor_msgq, &data, K_FOREVER);

		if (ret == 0) {
			printk("[Processor] << 处理传感器数据: id=%d temp=%d hum=%d ts=%d\n",
			       data.id, data.temperature,
			       data.humidity, data.timestamp);

			/* 模拟处理时间比采集慢，队列会逐渐填满 */
			k_msleep(2000);
		}
	}
}

// ==================== 示例2: 带超时和队列状态查询 ====================

/* 使用 k_msgq_init 动态创建（运行时创建） */
static struct k_msgq cmd_msgq;
static char __aligned(4) cmd_msgq_buffer[3 * 16];  /* 3条消息, 每条16字节 */

/* 命令结构体 */
struct command {
	int cmd_id;
	int param;
};

/* 命令发送线程 */
void cmd_sender_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct command cmd;

	while (1) {
		k_msleep(2000);

		cmd.cmd_id++;
		cmd.param = cmd.cmd_id * 10;

		printk("[CmdSender] 发送命令: id=%d param=%d "
		       "(空闲: %d/%d)\n",
		       cmd.cmd_id, cmd.param,
		       k_msgq_num_free_get(&cmd_msgq), 3);

		/*
		 * 带超时发送: 等待最多 1 秒
		 * 如果队列满且1秒内没有空间，返回 -EAGAIN
		 */
		int ret = k_msgq_put(&cmd_msgq, &cmd, K_MSEC(1000));

		if (ret != 0) {
			printk("[CmdSender] 发送超时: id=%d\n", cmd.cmd_id);
		}
	}
}

/* 命令执行线程 */
void cmd_handler_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct command cmd;

	while (1) {
		printk("[CmdHandler] 等待命令 (已用: %d/%d)...\n",
		       k_msgq_num_used_get(&cmd_msgq), 3);

		/*
		 * 带超时接收: 等待最多 5 秒
		 * 用于检测是否有命令到达
		 */
		int ret = k_msgq_get(&cmd_msgq, &cmd, K_MSEC(5000));

		if (ret == 0) {
			printk("[CmdHandler] << 执行命令: id=%d param=%d\n",
			       cmd.cmd_id, cmd.param);
			k_msleep(3500);  /* 模拟较慢的处理 */
		} else {
			printk("[CmdHandler] 等待超时，未收到命令\n");
		}
	}
}

// ==================== 主函数 ====================

/* 定义线程栈 */
K_THREAD_STACK_DEFINE(sensor_stack, 1024);
K_THREAD_STACK_DEFINE(processor_stack, 1024);
K_THREAD_STACK_DEFINE(cmd_sender_stack, 1024);
K_THREAD_STACK_DEFINE(cmd_handler_stack, 1024);

/* 定义线程控制块 */
static struct k_thread sensor_data_thread;
static struct k_thread processor_data_thread;
static struct k_thread cmd_sender_data;
static struct k_thread cmd_handler_data;

int main(void)
{
	printk("\n========================================\n");
	printk("Zephyr Message Queue 使用示例\n");
	printk("========================================\n\n");

	// --- 示例1: 基本生产者-消费者 ---
	printk("--- 示例1: 基本生产者-消费者 ---\n");

	/* sensor_msgq 已通过 K_MSGQ_DEFINE 静态创建，无需初始化 */

	k_thread_create(&sensor_data_thread, sensor_stack,
			K_THREAD_STACK_SIZEOF(sensor_stack),
			sensor_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&processor_data_thread, processor_stack,
			K_THREAD_STACK_SIZEOF(processor_stack),
			processor_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(12000);  // 运行12秒

	// --- 示例2: 带超时和队列状态查询 ---
	printk("\n--- 示例2: 带超时和队列状态查询 ---\n");

	/*
	 * k_msgq_init: 动态初始化消息队列
	 * 参数: 队列, 缓冲区, 消息大小, 最大消息数
	 */
	k_msgq_init(&cmd_msgq, cmd_msgq_buffer, sizeof(struct command), 3);

	k_thread_create(&cmd_sender_data, cmd_sender_stack,
			K_THREAD_STACK_SIZEOF(cmd_sender_stack),
			cmd_sender_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&cmd_handler_data, cmd_handler_stack,
			K_THREAD_STACK_SIZEOF(cmd_handler_stack),
			cmd_handler_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(15000);  // 运行15秒

	printk("\n========================================\n");
	printk("所有示例演示完毕\n");
	printk("========================================\n\n");

	return 0;
}
