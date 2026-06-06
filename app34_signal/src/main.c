/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

/*
 * Zephyr RTOS Poll Signal 使用示例
 *
 * Poll Signal 是一种轻量级的线程间通知机制。一个线程通过 k_poll() 等待
 * signal，另一个线程（或 ISR）通过 k_poll_signal_raise() 发送信号。
 * 与 semaphore 不同，signal 可以携带一个整数结果值（result）。
 *
 * 本示例演示了 Signal 的两种常见使用场景：
 * 1. 基本信号发送与等待
 * 2. 携带结果值的信号传递
 */

// ==================== 示例1: 基本信号发送与等待 ====================

/* 定义信号对象 */
static struct k_poll_signal basic_signal;

/* 定义 poll 事件，用于 k_poll() 等待 */
static struct k_poll_event basic_events[1];

/* 信号发送线程 */
void signal_sender_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int count = 0;

	while (1) {
		k_msleep(2000);
		count++;

		printk("[Sender] 第 %d 次: 触发信号\n", count);
		/*
		 * k_poll_signal_raise: 发送信号，可携带一个结果值
		 * 参数: signal对象, 结果值
		 * 返回值: 0=成功
		 */
		k_poll_signal_raise(&basic_signal, 0);
	}
}

/* 信号等待线程 */
void signal_waiter_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		printk("[Waiter] 等待信号...\n");

		/*
		 * k_poll: 等待 poll 事件
		 * 参数: 事件数组, 事件数量, 超时
		 * 返回值: 0=有事件发生
		 */
		int ret = k_poll(basic_events, ARRAY_SIZE(basic_events),
				 K_FOREVER);

		if (ret == 0) {
			/* 检查事件状态 */
			if (basic_events[0].state == K_POLL_STATE_SIGNALED) {
				printk("[Waiter] << 收到信号\n");
			}

			/*
			 * 重置信号状态，为下次等待做准备
			 * 重要: 每次收到信号后必须重置!
			 */
			k_poll_signal_reset(&basic_signal);
			basic_events[0].state = K_POLL_STATE_NOT_READY;
		}
	}
}

// ==================== 示例2: 携带结果值的信号 ====================

static struct k_poll_signal result_signal;
static struct k_poll_event result_events[1];

/* 定义操作码 */
#define CMD_READ   1
#define CMD_WRITE  2
#define CMD_ERASE  3

/* 命令发送线程: 发送不同命令及结果值 */
void commander_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_msleep(1000);
		printk("[Commander] 发送 READ 命令 (result=CMD_READ)\n");
		/*
		 * 发送信号并携带结果值，用于区分不同命令
		 * result 可以是任意整数值
		 */
		k_poll_signal_raise(&result_signal, CMD_READ);
		k_msleep(500);  /* 等待 worker 处理完 */

		k_msleep(2000);
		printk("[Commander] 发送 WRITE 命令 (result=CMD_WRITE)\n");
		k_poll_signal_raise(&result_signal, CMD_WRITE);
		k_msleep(500);

		k_msleep(2000);
		printk("[Commander] 发送 ERASE 命令 (result=CMD_ERASE)\n");
		k_poll_signal_raise(&result_signal, CMD_ERASE);
		k_msleep(500);
	}
}

/* 工作线程: 根据信号的结果值执行不同操作 */
void worker_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		printk("[Worker] 等待命令...\n");

		int ret = k_poll(result_events, ARRAY_SIZE(result_events),
				 K_FOREVER);

		if (ret == 0 && result_events[0].state == K_POLL_STATE_SIGNALED) {
			/*
			 * 从 signal 中读取携带的结果值
			 * signal->result 就是 k_poll_signal_raise() 传入的值
			 */
			int cmd = result_signal.result;

			switch (cmd) {
			case CMD_READ:
				printk("[Worker] << 执行 READ 操作\n");
				break;
			case CMD_WRITE:
				printk("[Worker] << 执行 WRITE 操作\n");
				break;
			case CMD_ERASE:
				printk("[Worker] << 执行 ERASE 操作\n");
				break;
			default:
				printk("[Worker] << 未知命令: %d\n", cmd);
				break;
			}

			/* 重置信号和事件状态 */
			k_poll_signal_reset(&result_signal);
			result_events[0].state = K_POLL_STATE_NOT_READY;
		}
	}
}

// ==================== 主函数 ====================

/* 定义线程栈 */
K_THREAD_STACK_DEFINE(sender_stack, 1024);
K_THREAD_STACK_DEFINE(waiter_stack, 1024);
K_THREAD_STACK_DEFINE(commander_stack, 1024);
K_THREAD_STACK_DEFINE(worker_stack, 1024);

/* 定义线程控制块 */
static struct k_thread sender_data;
static struct k_thread waiter_data;
static struct k_thread commander_data;
static struct k_thread worker_data;

int main(void)
{
	printk("\n========================================\n");
	printk("Zephyr Poll Signal 使用示例\n");
	printk("========================================\n\n");

	// --- 示例1: 基本信号发送与等待 ---
	printk("--- 示例1: 基本信号发送与等待 ---\n");

	/* 初始化信号对象 */
	k_poll_signal_init(&basic_signal);

	/*
	 * 初始化 poll 事件，绑定到信号对象
	 * K_POLL_TYPE_SIGNAL: 表示这是一个信号类型的事件
	 */
	k_poll_event_init(&basic_events[0], K_POLL_TYPE_SIGNAL,
			  K_POLL_MODE_NOTIFY_ONLY, &basic_signal);

	/* 创建发送和等待线程 */
	k_thread_create(&sender_data, sender_stack,
			K_THREAD_STACK_SIZEOF(sender_stack),
			signal_sender_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&waiter_data, waiter_stack,
			K_THREAD_STACK_SIZEOF(waiter_stack),
			signal_waiter_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(8000);  // 运行8秒

	// --- 示例2: 携带结果值的信号 ---
	printk("\n--- 示例2: 携带结果值的信号 ---\n");

	/* 初始化信号对象 */
	k_poll_signal_init(&result_signal);

	/* 初始化 poll 事件 */
	k_poll_event_init(&result_events[0], K_POLL_TYPE_SIGNAL,
			  K_POLL_MODE_NOTIFY_ONLY, &result_signal);

	/* 创建命令者和工作者线程 */
	k_thread_create(&commander_data, commander_stack,
			K_THREAD_STACK_SIZEOF(commander_stack),
			commander_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&worker_data, worker_stack,
			K_THREAD_STACK_SIZEOF(worker_stack),
			worker_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(12000);  // 运行12秒

	printk("\n========================================\n");
	printk("所有示例演示完毕\n");
	printk("========================================\n\n");

	return 0;
}
