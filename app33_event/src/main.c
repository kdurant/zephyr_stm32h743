/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

/*
 * Zephyr RTOS Event 使用示例
 *
 * Event 是一个 32 位的事件标志组，线程可以等待任意或所有指定事件。
 * 与信号量不同，Event 可以同时传递多种不同类型的事件。
 *
 * 本示例演示了 Event 的两种常见使用场景：
 * 1. 基本的事件发送与等待（任意事件触发）
 * 2. 等待所有指定事件（k_event_wait_all）
 */

/* 定义事件位 */
#define EVENT_BIT_READY    (1 << 0)   /* BIT 0: 设备就绪 */
#define EVENT_BIT_DATA     (1 << 1)   /* BIT 1: 数据到达 */
#define EVENT_BIT_SENSOR_A (1 << 2)   /* BIT 2: 传感器A数据 */
#define EVENT_BIT_SENSOR_B (1 << 3)   /* BIT 3: 传感器B数据 */

// ==================== 示例1: 基本的事件发送与等待 ====================

/* 定义一个事件对象 */
static struct k_event basic_event;

/* 生产者线程：周期性发送事件 */
void producer_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_msleep(2000);
		printk("[Producer] 发送 EVENT_BIT_READY 事件\n");
		/* 发送事件，多个位可以同时发送 */
		k_event_post(&basic_event, EVENT_BIT_READY);

		k_msleep(1000);
		printk("[Producer] 发送 EVENT_BIT_DATA 事件\n");
		k_event_post(&basic_event, EVENT_BIT_DATA);
	}
}

/* 消费者线程：等待任意事件 */
void consumer_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		printk("[Consumer] 等待任意事件...\n");

		/*
		 * k_event_wait: 等待 events 中的任意一个事件
		 * 参数: event, 期望的事件位, reset(是否清除), timeout
		 * 返回值: 实际收到的事件位组合
		 */
		uint32_t received = k_event_wait(&basic_event,
						 EVENT_BIT_READY | EVENT_BIT_DATA,
						 true,    /* 等待成功后自动清除 */
						 K_FOREVER);

		/* 检查收到的是哪个事件 */
		if (received & EVENT_BIT_READY) {
			printk("[Consumer] << 收到 EVENT_BIT_READY\n");
		}
		if (received & EVENT_BIT_DATA) {
			printk("[Consumer] << 收到 EVENT_BIT_DATA\n");
		}
	}
}

// ==================== 示例2: 等待所有指定事件 ====================

static struct k_event all_event;

/* 传感器A线程 */
void sensor_a_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_msleep(1000);
		printk("[SensorA] 数据就绪\n");
		k_event_post(&all_event, EVENT_BIT_SENSOR_A);
	}
}

/* 传感器B线程 */
void sensor_b_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_msleep(1500);
		printk("[SensorB] 数据就绪\n");
		k_event_post(&all_event, EVENT_BIT_SENSOR_B);
	}
}

/* 数据融合线程：必须等所有传感器数据就绪才处理 */
void fusion_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		printk("[Fusion] 等待所有传感器数据就绪...\n");

		/*
		 * k_event_wait_all: 等待所有指定事件都发生
		 * 参数: event, 事件位掩码（必须全部满足）, reset, timeout
		 */
		uint32_t received = k_event_wait_all(&all_event,
						      EVENT_BIT_SENSOR_A | EVENT_BIT_SENSOR_B,
						      true,     /* 清除所有事件 */
						      K_FOREVER);

		printk("[Fusion] << 所有传感器数据已就绪 (received: 0x%x)\n",
		       received);
		printk("[Fusion] 执行数据融合...\n");
		k_msleep(300);
	}
}

// ==================== 主函数 ====================

/* 定义线程栈 */
K_THREAD_STACK_DEFINE(producer_stack, 1024);
K_THREAD_STACK_DEFINE(consumer_stack, 1024);
K_THREAD_STACK_DEFINE(sensor_a_stack, 1024);
K_THREAD_STACK_DEFINE(sensor_b_stack, 1024);
K_THREAD_STACK_DEFINE(fusion_stack, 1024);

/* 定义线程控制块 */
static struct k_thread producer_data;
static struct k_thread consumer_data;
static struct k_thread sensor_a_data;
static struct k_thread sensor_b_data;
static struct k_thread fusion_data;

int main(void)
{
	printk("\n========================================\n");
	printk("Zephyr Event 使用示例\n");
	printk("========================================\n\n");

	// --- 示例1: 基本的事件发送与等待 ---
	printk("--- 示例1: 基本的事件发送与等待 ---\n");

	/* 初始化事件对象 */
	k_event_init(&basic_event);

	/* 创建生产者和消费者线程 */
	k_thread_create(&producer_data, producer_stack,
			K_THREAD_STACK_SIZEOF(producer_stack),
			producer_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&consumer_data, consumer_stack,
			K_THREAD_STACK_SIZEOF(consumer_stack),
			consumer_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(8000);  // 运行8秒

	// --- 示例2: 等待所有指定事件 ---
	printk("\n--- 示例2: 等待所有指定事件 (k_event_wait_all) ---\n");

	/* 初始化事件对象 */
	k_event_init(&all_event);

	/* 创建传感器和数据融合线程 */
	k_thread_create(&sensor_a_data, sensor_a_stack,
			K_THREAD_STACK_SIZEOF(sensor_a_stack),
			sensor_a_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&sensor_b_data, sensor_b_stack,
			K_THREAD_STACK_SIZEOF(sensor_b_stack),
			sensor_b_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&fusion_data, fusion_stack,
			K_THREAD_STACK_SIZEOF(fusion_stack),
			fusion_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(8000);  // 运行8秒

	printk("\n========================================\n");
	printk("所有示例演示完毕\n");
	printk("========================================\n\n");

	return 0;
}
