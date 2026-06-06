/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

/*
 * Zephyr RTOS Work Queue 使用示例
 *
 * Work Queue 将任务提交到后台线程执行，避免在 ISR 或高优先级线程中
 * 执行耗时操作。有三种工作模式：
 *
 * 1. 普通 Work:      提交后立即执行（FIFO）
 * 2. Delayable Work: 延迟指定时间后执行（可重调度）
 * 3. 自定义队列:     创建独立的工作队列线程
 *
 * 本示例演示：
 * 1. 系统工作队列 — 普通 work + delayable work
 * 2. 自定义工作队列 — 独立的队列线程
 */

// ==================== 示例1: 系统工作队列 ====================

/*
 * K_WORK_DEFINE: 静态定义普通 work 项
 * 参数: work名称, 处理函数
 */
static void normal_work_handler(struct k_work *work);
K_WORK_DEFINE(normal_work, normal_work_handler);

/*
 * K_WORK_DELAYABLE_DEFINE: 静态定义可延迟 work 项
 * 参数: dwork名称, 处理函数
 */
static void delayed_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(delayed_work, delayed_work_handler);

static int work_count;

/* 普通 work 处理函数 */
static void normal_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	work_count++;
	printk("[SysWorkQ] 执行普通 work (第 %d 次)\n", work_count);
}

/* 延迟 work 处理函数 */
static void delayed_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	printk("[SysWorkQ] 延迟 work 触发\n");
}

/* 提交 work 的线程 */
void submitter_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* 立即提交一个普通 work */
	printk("[Submitter] 提交普通 work\n");
	k_work_submit(&normal_work);

	k_msleep(2000);

	/* 再提交一个 */
	printk("[Submitter] 再次提交普通 work\n");
	k_work_submit(&normal_work);

	k_msleep(2000);

	/*
	 * k_work_schedule: 延迟 3 秒后提交到系统队列
	 * 参数: dwork, 延迟时间
	 * 可用于周期性任务: 在 handler 中再次 schedule 自己
	 */
	printk("[Submitter] 提交延迟 work (3秒后执行)\n");
	k_work_schedule(&delayed_work, K_MSEC(3000));

	k_msleep(5000);
}

// ==================== 示例2: 自定义工作队列 ====================

/*
 * 定义一个独立的 work queue
 * struct k_work_q 声明即可，无需特殊宏
 */
static struct k_work_q my_workq;

/* 定义该队列的线程栈 */
K_THREAD_STACK_DEFINE(my_workq_stack, 1024);

/* 定义两个 work 项，提交到自定义队列 */
static void long_work_handler(struct k_work *work);
static void urgent_work_handler(struct k_work *work);

K_WORK_DEFINE(long_work, long_work_handler);
K_WORK_DEFINE(urgent_work, urgent_work_handler);

static void long_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	printk("[MyWorkQ] 开始执行耗时 work...\n");
	k_msleep(2000);  /* 模拟耗时操作 */
	printk("[MyWorkQ] 耗时 work 完成\n");
}

static void urgent_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	printk("[MyWorkQ] 执行紧急 work\n");
}

/* 提交到自定义队列的线程 */
void dispatcher_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* 先提交耗时 work */
	printk("[Dispatcher] 提交耗时 work 到自定义队列\n");
	k_work_submit_to_queue(&my_workq, &long_work);

	k_msleep(500);

	/* 再提交紧急 work（排在耗时 work 之后） */
	printk("[Dispatcher] 提交紧急 work 到自定义队列\n");
	k_work_submit_to_queue(&my_workq, &urgent_work);

	k_msleep(5000);
}

// ==================== 主函数 ====================

K_THREAD_STACK_DEFINE(submitter_stack, 1024);
K_THREAD_STACK_DEFINE(dispatcher_stack, 1024);

static struct k_thread submitter_data;
static struct k_thread dispatcher_data;

int main(void)
{
	printk("\n========================================\n");
	printk("Zephyr Work Queue 使用示例\n");
	printk("========================================\n\n");

	// --- 示例1: 系统工作队列 ---
	printk("--- 示例1: 系统工作队列 ---\n");
	printk("说明: k_work_submit 立即入队, k_work_schedule 延迟入队。\n");
	printk("      系统队列在启动时自动创建 (system_work_q)。\n\n");

	k_thread_create(&submitter_data, submitter_stack,
			K_THREAD_STACK_SIZEOF(submitter_stack),
			submitter_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_msleep(10000);

	// --- 示例2: 自定义工作队列 ---
	printk("\n--- 示例2: 自定义工作队列 ---\n");
	printk("说明: 创建独立队列线程, 与系统队列分离。\n");
	printk("      耗时 work 不会阻塞系统队列的其他任务。\n\n");

	/*
	 * k_work_queue_start: 启动自定义队列线程
	 * 参数: 队列, 栈, 栈大小, 优先级, 配置(NULL=默认)
	 */
	struct k_work_queue_config cfg = {
		.name = "my_workq",
	};
	k_work_queue_start(&my_workq, my_workq_stack,
			   K_THREAD_STACK_SIZEOF(my_workq_stack),
			   5, &cfg);

	k_thread_create(&dispatcher_data, dispatcher_stack,
			K_THREAD_STACK_SIZEOF(dispatcher_stack),
			dispatcher_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_msleep(8000);

	printk("\n========================================\n");
	printk("所有示例演示完毕\n");
	printk("========================================\n\n");

	return 0;
}
