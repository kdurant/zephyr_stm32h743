/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

/*
 * Zephyr RTOS FIFO 使用示例
 *
 * FIFO (First In, First Out) 是先进先出队列。
 * 与 Message Queue 不同: FIFO 传递的是数据指针（引用），而不是复制数据。
 *
 * 重要: 使用 k_fifo_put() 时，数据项的第一个字(word)会被内核占用，
 *       所以数据必须保持有效直到被 k_fifo_get() 取出。
 *
 * 本示例演示了两种使用场景：
 * 1. k_fifo_alloc_put — 内核分配节点，用户数据不被修改
 * 2. k_fifo_put — 预分配数据池，第一字被内核使用
 */

/* 注意: 使用 k_fifo_allocate_put 时，数据是普通结构体即可 */
struct sensor_data {
	int id;
	int value;
};

// ==================== 示例1: k_fifo_alloc_put ====================

/* 静态定义一个 FIFO */
K_FIFO_DEFINE(sensor_fifo);

/* 传感器线程: 动态分配数据并放入 FIFO */
void sensor_alloc_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_msleep(1000);

		/* 动态分配数据 */
		struct sensor_data *data = k_malloc(sizeof(*data));

		if (data == NULL) {
			printk("[Sensor] 内存分配失败\n");
			continue;
		}

		data->id++;
		data->value = data->id * 100;

		/*
		 * k_fifo_alloc_put: 放入 FIFO，内核自动分配节点
		 * 数据本身不会被修改，可以包含任意字段
		 * 返回值: 0=成功, -ENOMEM=内存不足
		 */
		int ret = k_fifo_alloc_put(&sensor_fifo, data);

		if (ret == 0) {
			printk("[Sensor] 发送数据: id=%d value=%d\n",
			       data->id, data->value);
		} else {
			printk("[Sensor] put 失败: id=%d\n", data->id);
			k_free(data);
		}
	}
}

/* 处理器线程: 从 FIFO 取出并处理 */
void processor_alloc_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		/*
		 * k_fifo_get: 从 FIFO 取出数据（返回指针）
		 * 参数: fifo, 超时
		 * 返回: 数据的 void * 指针，超时返回 NULL
		 */
		struct sensor_data *data = k_fifo_get(&sensor_fifo, K_FOREVER);

		if (data != NULL) {
			printk("[Processor] << 处理数据: id=%d value=%d\n",
			       data->id, data->value);
			k_msleep(800);

			/* 处理完成后释放内存 */
			k_free(data);
		}
	}
}

// ==================== 示例2: k_fifo_put（预分配数据池） ====================

/*
 * 使用 k_fifo_put() 时，数据项的第一个 word 被内核占用。
 * 通常用一个 void * 或 sys_snode_t 作为第一个字段预留。
 */
struct data_item {
	void *fifo_reserved;  /* 第一个 word 预留给内核 */
	int  count;
	char msg[32];
};

/* 预分配数据池 */
#define POOL_SIZE 4
static struct data_item data_pool[POOL_SIZE];
static int pool_index;

K_FIFO_DEFINE(data_fifo);

/* 生产者: 从池中取空闲项，填充后放入 FIFO */
void producer_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_msleep(1500);

		/* 从池中找一个空闲项（简化: 轮转使用） */
		struct data_item *item = &data_pool[pool_index];
		pool_index = (pool_index + 1) % POOL_SIZE;

		item->count++;
		snprintf(item->msg, sizeof(item->msg),
			 "msg #%d", item->count);

		printk("[Producer] 放入: %s\n", item->msg);

		/*
		 * k_fifo_put: 放入 FIFO（不复制，仅存指针）
		 * 注意: 同一个 item 被取出前不能再次放入！
		 */
		k_fifo_put(&data_fifo, item);
	}
}

/* 消费者: 从 FIFO 取出数据 */
void consumer_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		struct data_item *item = k_fifo_get(&data_fifo, K_FOREVER);

		if (item != NULL) {
			printk("[Consumer] << 取出: %s\n", item->msg);
			k_msleep(2500);

			/* 使用完毕后归还给池（这里自动被下次 producer 覆盖） */
		}
	}
}

// ==================== 主函数 ====================

K_THREAD_STACK_DEFINE(sensor_stack, 1024);
K_THREAD_STACK_DEFINE(processor_stack, 1024);
K_THREAD_STACK_DEFINE(producer_stack, 1024);
K_THREAD_STACK_DEFINE(consumer_stack, 1024);

static struct k_thread sensor_data_thread;
static struct k_thread processor_data_thread;
static struct k_thread producer_data_thread;
static struct k_thread consumer_data_thread;

int main(void)
{
	printk("\n========================================\n");
	printk("Zephyr FIFO 使用示例\n");
	printk("========================================\n\n");

	// --- 示例1: k_fifo_alloc_put（动态分配） ---
	printk("--- 示例1: k_fifo_alloc_put（内核分配节点） ---\n");
	printk("说明: 数据通过 k_malloc 动态分配，\n");
	printk("      k_fifo_alloc_put 不修改用户数据内容。\n\n");

	/* sensor_fifo 已通过 K_FIFO_DEFINE 静态创建 */

	k_thread_create(&sensor_data_thread, sensor_stack,
			K_THREAD_STACK_SIZEOF(sensor_stack),
			sensor_alloc_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&processor_data_thread, processor_stack,
			K_THREAD_STACK_SIZEOF(processor_stack),
			processor_alloc_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(8000);

	// --- 示例2: k_fifo_put（预分配数据池） ---
	printk("\n--- 示例2: k_fifo_put（预分配数据池） ---\n");
	printk("说明: 数据项的第一个 word 被内核占用，\n");
	printk("      因此 struct 预留了 fifo_reserved 字段。\n");
	printk("      数据通过指针传递，不复制内容。\n\n");

	/* data_fifo 已通过 K_FIFO_DEFINE 静态创建 */

	k_thread_create(&producer_data_thread, producer_stack,
			K_THREAD_STACK_SIZEOF(producer_stack),
			producer_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&consumer_data_thread, consumer_stack,
			K_THREAD_STACK_SIZEOF(consumer_stack),
			consumer_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(12000);

	printk("\n========================================\n");
	printk("所有示例演示完毕\n");
	printk("========================================\n\n");

	return 0;
}
