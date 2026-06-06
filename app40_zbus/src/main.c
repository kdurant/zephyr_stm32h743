/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/zbus/zbus.h>

/*
 * Zephyr ZBus 使用示例
 *
 * ZBus 是 Zephyr 的发布-订阅消息总线:
 *   - Channel: 消息通道，承载一种类型的消息
 *   - Publisher: 发布者，向通道发布消息
 *   - Subscriber: 订阅者线程，通过 FIFO 队列接收消息
 *   - Listener: 监听者回调，发布时立即被调用
 *
 * 核心宏:
 *   ZBUS_CHAN_DEFINE(name, type, validator, user_data, observers, init_val)
 *   ZBUS_SUBSCRIBER_DEFINE(name, queue_size)
 *   ZBUS_LISTENER_DEFINE(name, callback)
 *
 * 本示例:
 * 1. 一个传感器通道 + 一个 Subscriber + 一个 Listener
 * 2. Subscriber 通过 zbus_chan_read 接收消息
 * 3. Listener 通过回调函数自动响应
 */

// ==================== Channel 定义 ====================

/* 消息类型 */
struct sensor_msg {
	int  temperature;
	int  humidity;
};

/*
 * 消息校验器: 发布时自动检查消息合法性
 * 返回 true=有效, false=无效（发布失败）
 */
static bool sensor_validator(const void *msg, size_t msg_size)
{
	const struct sensor_msg *m = (const struct sensor_msg *)msg;

	if (m->temperature < -50 || m->temperature > 150) {
		return false;  /* 温度超出范围 */
	}
	if (m->humidity < 0 || m->humidity > 100) {
		return false;  /* 湿度超出范围 */
	}
	return true;
}

// ==================== Observer 定义 ====================

/* Subscriber: 订阅线程，通过 FIFO 接收消息 */
ZBUS_SUBSCRIBER_DEFINE(sensor_sub, 4);

/* Listener 回调: 发布时立即被调用 */
static void sensor_listener_cb(const struct zbus_channel *chan)
{
	const struct sensor_msg *msg = zbus_chan_const_msg(chan);

	printk("[Listener] 收到通知: temp=%d hum=%d (通道=%p)\n",
	       msg->temperature, msg->humidity, (void *)chan);
}
ZBUS_LISTENER_DEFINE(sensor_lis, sensor_listener_cb);

/*
 * ZBUS_CHAN_DEFINE: 定义通道
 * 参数: 名称, 消息类型, 校验器(NULL=不校验), 用户数据,
 *       observer列表(ZBUS_OBSERVERS(obs1, obs2, ...) 或 ZBUS_OBSERVERS_EMPTY),
 *       初始值
 */
ZBUS_CHAN_DEFINE(sensor_chan, struct sensor_msg, sensor_validator, NULL,
		 ZBUS_OBSERVERS(sensor_sub, sensor_lis), {0});

// ==================== Publisher ====================

void publisher_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct sensor_msg msg;
	int count = 0;

	while (1) {
		k_msleep(2000);
		count++;

		msg.temperature = 25 + count * 3;
		msg.humidity    = 60 + count;

		printk("[Publisher] 发布: temp=%d hum=%d\n",
		       msg.temperature, msg.humidity);

		/*
		 * zbus_chan_pub: 发布消息到通道
		 * 参数: 通道, 消息指针, 超时
		 * 发布成功时: validator 校验通过 -> Listener 回调
		 *            -> Subscriber FIFO 入队
		 * validator 校验失败返回 -ENOMSG
		 */
		int ret = zbus_chan_pub(&sensor_chan, &msg, K_MSEC(100));

		if (ret != 0) {
			printk("[Publisher] 发布失败 (ret=%d, validator拒绝?)\n",
			       ret);
		}
	}
}

// ==================== Subscriber ====================

void subscriber_func(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct sensor_msg msg;

	while (1) {
		printk("[Subscriber] 等待消息...\n");

		/*
		 * zbus_chan_read: 从通道读取消息（供 subscriber 使用）
		 * 参数: 通道, 消息缓冲区, 超时
		 * Subscriber 通过其内部 FIFO 接收消息
		 */
		int ret = zbus_chan_read(&sensor_chan, &msg, K_FOREVER);

		if (ret == 0) {
			printk("[Subscriber] << 收到: temp=%d hum=%d\n",
			       msg.temperature, msg.humidity);

			/* 模拟处理 */
			k_msleep(500);
		}
	}
}

// ==================== 主函数 ====================

K_THREAD_STACK_DEFINE(pub_stack, 1024);
K_THREAD_STACK_DEFINE(sub_stack, 1024);

static struct k_thread pub_data;
static struct k_thread sub_data;

int main(void)
{
	printk("\n========================================\n");
	printk("Zephyr ZBus 使用示例\n");
	printk("========================================\n\n");

	printk("架构: Publisher -> sensor_chan -> [Subscriber, Listener]\n");
	printk("  - Subscriber: FIFO 接收, zbus_chan_read 取消息\n");
	printk("  - Listener:   回调自动触发, 实时响应\n");
	printk("  - Channel 定义在编译时完成, 无需运行时 init\n\n");

	k_thread_create(&pub_data, pub_stack,
			K_THREAD_STACK_SIZEOF(pub_stack),
			publisher_func, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&sub_data, sub_stack,
			K_THREAD_STACK_SIZEOF(sub_stack),
			subscriber_func, NULL, NULL, NULL,
			6, 0, K_NO_WAIT);

	k_msleep(12000);

	printk("\n========================================\n");
	printk("示例演示完毕\n");
	printk("========================================\n\n");

	return 0;
}
