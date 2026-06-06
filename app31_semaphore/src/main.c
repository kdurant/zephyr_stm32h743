/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

/*
 * Zephyr RTOS Semaphore 使用示例
 * 
 * 本示例演示了信号量的三种常见使用场景：
 * 1. 基本的信号量操作（give/take）
 * 2. 线程间同步
 * 3. 资源访问控制（计数器信号量）
 */

// ==================== 示例1: 基本信号量操作 ====================

/* 定义一个二进制信号量 */
static struct k_sem basic_sem;

/* 线程1: 释放信号量 */
void thread1_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        printk("[Thread1] 执行任务...\n");
        k_msleep(1000);
        
        /* 释放信号量，计数值+1 */
        k_sem_give(&basic_sem);
        printk("[Thread1] 已释放信号量\n");
    }
}

/* 线程2: 获取信号量 */
void thread2_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        /* 等待信号量，如果计数值为0则阻塞 */
        printk("[Thread2] 等待信号量...\n");
        k_sem_take(&basic_sem, K_FOREVER);
        printk("[Thread2] 获取到信号量，继续执行\n");
        
        k_msleep(500);
    }
}

// ==================== 示例2: 生产者-消费者模式 ====================

#define BUFFER_SIZE 5

/* 定义信号量用于同步 */
static struct k_sem empty_slots;   // 空槽位信号量
static struct k_sem full_slots;    // 满槽位信号量

/* 共享缓冲区 */
static int buffer[BUFFER_SIZE];
static int write_idx = 0;
static int read_idx = 0;

/* 生产者线程 */
void producer_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    int value = 0;

    while (1) {
        /* 等待有空闲槽位 */
        k_sem_take(&empty_slots, K_FOREVER);
        
        /* 生产数据 */
        buffer[write_idx] = value++;
        printk("[Producer] 生产数据: %d (写入位置: %d)\n", 
               buffer[write_idx], write_idx);
        
        /* 更新写索引 */
        write_idx = (write_idx + 1) % BUFFER_SIZE;
        
        /* 通知消费者有数据可读 */
        k_sem_give(&full_slots);
        
        k_msleep(800);  // 模拟生产时间
    }
}

/* 消费者线程 */
void consumer_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        /* 等待有数据可读 */
        k_sem_take(&full_slots, K_FOREVER);
        
        /* 消费数据 */
        printk("[Consumer] 消费数据: %d (读取位置: %d)\n", 
               buffer[read_idx], read_idx);
        
        /* 更新读索引 */
        read_idx = (read_idx + 1) % BUFFER_SIZE;
        
        /* 通知生产者有空闲槽位 */
        k_sem_give(&empty_slots);
        
        k_msleep(1200);  // 模拟消费时间
    }
}

// ==================== 示例3: 带超时的信号量操作 ====================

static struct k_sem timeout_sem;

void timeout_thread_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        printk("[TimeoutThread] 尝试获取信号量（超时2秒）...\n");
        
        /* 带超时的信号量获取 */
        int ret = k_sem_take(&timeout_sem, K_MSEC(2000));
        
        if (ret == 0) {
            printk("[TimeoutThread] 成功获取信号量\n");
        } else {
            printk("[TimeoutThread] 超时！未获取到信号量\n");
        }
        
        k_msleep(3000);
    }
}

// ==================== 主函数 ====================

/* 
 * 方式1: 使用 K_THREAD_DEFINE 静态定义（编译时创建）
 * 这种方式会在系统启动时自动创建线程，无需手动调用 k_thread_create()
 */
K_THREAD_DEFINE(static_thread, 1024,
                static_thread_func, NULL, NULL, NULL,
                7, 0, K_NO_WAIT);

/*
 * 方式2: 使用 k_thread_create() 动态创建（运行时创建）
 * 需要先声明栈和控制块，然后在需要时手动创建
 */
/* 定义线程栈 */
K_THREAD_STACK_DEFINE(thread1_stack, 1024);
K_THREAD_STACK_DEFINE(thread2_stack, 1024);
K_THREAD_STACK_DEFINE(producer_stack, 1024);
K_THREAD_STACK_DEFINE(consumer_stack, 1024);
K_THREAD_STACK_DEFINE(timeout_stack, 1024);

/* 定义线程控制块 */
static struct k_thread thread1_data;
static struct k_thread thread2_data;
static struct k_thread producer_data;
static struct k_thread consumer_data;
static struct k_thread timeout_data;

/* 静态定义线程的示例函数 */
void static_thread_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    printk("[StaticThread] 使用 K_THREAD_DEFINE 创建的线程已启动\n");
    
    while (1) {
        printk("[StaticThread] 运行中...\n");
        k_msleep(2000);
    }
}

int main(void)
{
    printk("\n========================================\n");
    printk("Zephyr Semaphore 使用示例\n");
    printk("========================================\n\n");

    // --- 示例1: 基本信号量操作 ---
    printk("--- 示例1: 基本信号量操作 ---\n");
    
    /* 初始化二进制信号量，初始值为0 */
    k_sem_init(&basic_sem, 0, 1);
    
    /* 创建线程 */
    k_thread_create(&thread1_data, thread1_stack, K_THREAD_STACK_SIZEOF(thread1_stack),
                    thread1_func, NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);
    
    k_thread_create(&thread2_data, thread2_stack, K_THREAD_STACK_SIZEOF(thread2_stack),
                    thread2_func, NULL, NULL, NULL,
                    6, 0, K_NO_WAIT);
    
    k_msleep(5000);  // 运行5秒
    
    // --- 示例2: 生产者-消费者模式 ---
    printk("\n--- 示例2: 生产者-消费者模式 ---\n");
    
    /* 初始化信号量 */
    k_sem_init(&empty_slots, BUFFER_SIZE, BUFFER_SIZE);  // 初始时有BUFFER_SIZE个空槽
    k_sem_init(&full_slots, 0, BUFFER_SIZE);             // 初始时没有满槽
    
    /* 创建生产者和消费者线程 */
    k_thread_create(&producer_data, producer_stack, K_THREAD_STACK_SIZEOF(producer_stack),
                    producer_func, NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);
    
    k_thread_create(&consumer_data, consumer_stack, K_THREAD_STACK_SIZEOF(consumer_stack),
                    consumer_func, NULL, NULL, NULL,
                    6, 0, K_NO_WAIT);
    
    k_msleep(5000);  // 运行5秒
    
    // --- 示例3: 带超时的信号量操作 ---
    printk("\n--- 示例3: 带超时的信号量操作 ---\n");
    
    /* 初始化信号量，初始值为0 */
    k_sem_init(&timeout_sem, 0, 1);
    
    /* 创建超时测试线程 */
    k_thread_create(&timeout_data, timeout_stack, K_THREAD_STACK_SIZEOF(timeout_stack),
                    timeout_thread_func, NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);
    
    /* 主线程每隔4秒释放一次信号量 */
    while (1) {
        k_msleep(4000);
        k_sem_give(&timeout_sem);
        printk("[Main] 释放超时信号量\n");
    }

    return 0;
}
