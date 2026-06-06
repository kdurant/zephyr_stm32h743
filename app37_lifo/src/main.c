/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

/*
 * Zephyr RTOS LIFO 使用示例
 *
 * LIFO (Last In, First Out) 是后进先出栈。
 * 与 FIFO 相反：后放入的数据会被先取出。
 *
 * API 与 FIFO 基本一致，区别在于 put 时插入到头部：
 * - k_lifo_put()   — 插入到头部，第一字被内核占用
 * - k_lifo_alloc_put() — 内核分配节点，数据不被修改
 * - k_lifo_get()   — 从头部取出
 *
 * 本示例演示两种场景：
 * 1. 展示 LIFO 的后进先出特性
 * 2. 紧急命令栈：高优先级命令插队处理
 */

// ==================== 示例1: 演示 LIFO 顺序 ====================

struct task {
	void *lifo_reserved;  /* 第一字预留给内核 */
	int  id;
};

/* 预分配任务节点 */
static struct task tasks[5];
static int task_idx;

K_LIFO_DEFINE(task_stack);

/* 生产者: 依次放入 task 0,1,2,3,4 */
void push_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (int i = 0; i < 5; i++) {
		tasks[task_idx].id = i;
		k_lifo_put(&task_stack, &tasks[task_idx]);
		printk("[Push] 压入 task %d  (栈顶)\n", i);
		task_idx++;
		k_msleep(500);
	}
}

/* 消费者: 弹出 — 由于是 LIFO，将看到 4,3,2,1,0 */
void pop_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_msleep(3000);  /* 等所有 push 完成 */

	for (int i = 0; i < 5; i++) {
		struct task *t = k_lifo_get(&task_stack, K_FOREVER);
		printk("[Pop]  << 弹出 task %d  (后进先出)\n", t->id);
		k_msleep(500);
	}
}

// ==================== 示例2: 紧急命令栈 ====================

enum cmd_type {
	CMD_NORMAL   = 0,
	CMD_URGENT   = 1,
	CMD_CRITICAL = 2,
};

struct command {
	void *lifo_reserved;
	enum cmd_type type;
	char name[16];
};

static struct command cmd_pool[6];
static int cmd_idx;

K_LIFO_DEFINE(cmd_stack);

/* 命令分发: 普通命令定时产生，紧急命令插队 */
void dispatcher_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int seq = 0;

	while (1) {
		k_msleep(2000);

		/* 产生一条普通命令 */
		struct command *cmd = &cmd_pool[cmd_idx++];
		cmd->type = CMD_NORMAL;
		snprintf(cmd->name, sizeof(cmd->name), "normal-%d", seq++);
		printk("[Dispatch] 压入普通命令: %s\n", cmd->name);
		k_lifo_put(&cmd_stack, cmd);

		k_msleep(1000);

		/* 模拟突发事件: 产生一条紧急命令 */
		cmd = &cmd_pool[cmd_idx++];
		cmd->type = CMD_URGENT;
		snprintf(cmd->name, sizeof(cmd->name), "urgent-%d", seq);
		printk("[Dispatch] 压入紧急命令: %s (将优先处理!)\n", cmd->name);
		k_lifo_put(&cmd_stack, cmd);
	}
}

/* 命令执行: LIFO 保证紧急命令优先处理 */
void executor_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		struct command *cmd = k_lifo_get(&cmd_stack, K_FOREVER);

		const char *label;
		switch (cmd->type) {
		case CMD_URGENT:
			label = "[URGENT]";
			break;
		case CMD_CRITICAL:
			label = "[CRITICAL]";
			break;
		default:
			label = "[NORMAL]";
			break;
		}

		printk("[Executor] %s 执行: %s\n", label, cmd->name);
		k_msleep(1200);  /* 忙一会再处理下一个 */
	}
}

// ==================== 主函数 ====================

K_THREAD_STACK_DEFINE(push_stack, 1024);
K_THREAD_STACK_DEFINE(pop_stack, 1024);
K_THREAD_STACK_DEFINE(dispatch_stack, 1024);
K_THREAD_STACK_DEFINE(executor_stack, 1024);

static struct k_thread push_data;
static struct k_thread pop_data;
static struct k_thread dispatch_data;
static struct k_thread executor_data;

int main(void)
{
	printk("\n========================================\n");
	printk("Zephyr LIFO 使用示例\n");
	printk("========================================\n\n");

	// --- 示例1: 演示 LIFO 顺序 ---
	printk("--- 示例1: 演示后进先出顺序 ---\n");
	printk("说明: 依次压入 task 0,1,2,3,4\n");
	printk("      LIFO 会按 4,3,2,1,0 的顺序弹出。\n\n");

	k_thread_create(&push_data, push_stack,
			K_THREAD_STACK_SIZEOF(push_stack),
			push_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&pop_data, pop_stack,
			K_THREAD_STACK_SIZEOF(pop_stack),
			pop_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(7000);

	// --- 示例2: 紧急命令栈 ---
	printk("\n--- 示例2: 紧急命令栈 ---\n");
	printk("说明: 紧急命令在普通命令之后压入，\n");
	printk("      但因 LIFO 特性会优先被取出执行。\n\n");

	k_thread_create(&dispatch_data, dispatch_stack,
			K_THREAD_STACK_SIZEOF(dispatch_stack),
			dispatcher_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&executor_data, executor_stack,
			K_THREAD_STACK_SIZEOF(executor_stack),
			executor_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(10000);

	printk("\n========================================\n");
	printk("所有示例演示完毕\n");
	printk("========================================\n\n");

	return 0;
}
