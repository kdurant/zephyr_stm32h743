/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

/*
 * Zephyr RTOS Mutex 使用示例
 * 
 * 本示例演示了互斥锁的三种常见使用场景：
 * 1. 基本的互斥锁操作（lock/unlock）
 * 2. 保护共享资源（防止竞态条件）
 * 3. 优先级继承机制演示
 */

// ==================== 示例1: 基本互斥锁操作 ====================

/* 定义一个互斥锁 */
static struct k_mutex basic_mutex;

/* 线程1: 获取并持有互斥锁 */
void thread1_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        printk("[Thread1] 尝试获取互斥锁...\n");
        
        /* 锁定互斥锁（如果已被其他线程锁定，则阻塞等待） */
        k_mutex_lock(&basic_mutex, K_FOREVER);
        
        printk("[Thread1] 已获取互斥锁，执行临界区代码\n");
        k_msleep(2000);  // 模拟临界区操作
        
        printk("[Thread1] 释放互斥锁\n");
        /* 释放互斥锁 */
        k_mutex_unlock(&basic_mutex);
        
        k_msleep(1000);
    }
}

/* 线程2: 获取并持有互斥锁 */
void thread2_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        printk("[Thread2] 尝试获取互斥锁...\n");
        
        /* 锁定互斥锁 */
        k_mutex_lock(&basic_mutex, K_FOREVER);
        
        printk("[Thread2] 已获取互斥锁，执行临界区代码\n");
        k_msleep(1500);  // 模拟临界区操作
        
        printk("[Thread2] 释放互斥锁\n");
        /* 释放互斥锁 */
        k_mutex_unlock(&basic_mutex);
        
        k_msleep(800);
    }
}

// ==================== 示例2: 保护共享资源 ====================

/* 定义互斥锁用于保护共享资源 */
static struct k_mutex resource_mutex;

/* 共享资源 */
static int shared_counter = 0;
static char shared_buffer[64];

/* 线程A: 修改共享资源 */
void writer_thread_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    int write_count = 0;

    while (1) {
        /* 锁定互斥锁以保护共享资源 */
        k_mutex_lock(&resource_mutex, K_FOREVER);
        
        /* 临界区：安全地修改共享资源 */
        shared_counter++;
        snprintf(shared_buffer, sizeof(shared_buffer), 
                 "Writer update #%d at %u ms", 
                 write_count++, k_uptime_get_32());
        
        printk("[Writer] 更新共享资源 - Counter: %d, Buffer: %s\n", 
               shared_counter, shared_buffer);
        
        k_msleep(500);  // 模拟写入操作
        
        /* 释放互斥锁 */
        k_mutex_unlock(&resource_mutex);
        
        k_msleep(1000);
    }
}

/* 线程B: 读取共享资源 */
void reader_thread_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        /* 锁定互斥锁以保护共享资源 */
        k_mutex_lock(&resource_mutex, K_FOREVER);
        
        /* 临界区：安全地读取共享资源 */
        int current_counter = shared_counter;
        char local_buffer[64];
        strncpy(local_buffer, shared_buffer, sizeof(local_buffer) - 1);
        local_buffer[sizeof(local_buffer) - 1] = '\0';
        
        printk("[Reader] 读取共享资源 - Counter: %d, Buffer: %s\n", 
               current_counter, local_buffer);
        
        /* 释放互斥锁 */
        k_mutex_unlock(&resource_mutex);
        
        k_msleep(1500);
    }
}

// ==================== 示例3: 带超时的互斥锁操作 ====================

static struct k_mutex timeout_mutex;

void timeout_holder_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        printk("[TimeoutHolder] 获取互斥锁并长时间持有...\n");
        
        k_mutex_lock(&timeout_mutex, K_FOREVER);
        printk("[TimeoutHolder] 已获取互斥锁，将持有3秒\n");
        
        k_msleep(3000);  // 长时间持有锁
        
        printk("[TimeoutHolder] 释放互斥锁\n");
        k_mutex_unlock(&timeout_mutex);
        
        k_msleep(1000);
    }
}

void timeout_waiter_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        printk("[TimeoutWaiter] 尝试获取互斥锁（超时1秒）...\n");
        
        /* 带超时的锁定操作 */
        int ret = k_mutex_lock(&timeout_mutex, K_MSEC(1000));
        
        if (ret == 0) {
            printk("[TimeoutWaiter] 成功获取互斥锁\n");
            k_msleep(500);
            k_mutex_unlock(&timeout_mutex);
            printk("[TimeoutWaiter] 释放互斥锁\n");
        } else {
            printk("[TimeoutWaiter] 超时！未能获取互斥锁（错误码: %d）\n", ret);
        }
        
        k_msleep(2000);
    }
}

// ==================== 示例4: 递归互斥锁 ====================

static struct k_mutex recursive_mutex;

/* 递归函数，多次锁定同一个互斥锁 */
void recursive_function(int depth)
{
    printk("[Recursive] 深度 %d: 尝试锁定互斥锁\n", depth);
    
    /* 锁定互斥锁 */
    k_mutex_lock(&recursive_mutex, K_FOREVER);
    
    printk("[Recursive] 深度 %d: 已锁定，执行操作\n", depth);
    k_msleep(100);
    
    if (depth < 3) {
        /* 递归调用，再次锁定同一个互斥锁 */
        recursive_function(depth + 1);
    }
    
    printk("[Recursive] 深度 %d: 解锁互斥锁\n", depth);
    
    /* 解锁互斥锁（必须与锁定次数匹配） */
    k_mutex_unlock(&recursive_mutex);
}

void recursive_thread_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        printk("\n[RecursiveThread] 开始递归测试\n");
        recursive_function(0);
        printk("[RecursiveThread] 递归测试完成\n\n");
        
        k_msleep(5000);
    }
}

// ==================== 主函数 ====================

/* 定义线程栈 */
K_THREAD_STACK_DEFINE(thread1_stack, 1024);
K_THREAD_STACK_DEFINE(thread2_stack, 1024);
K_THREAD_STACK_DEFINE(writer_stack, 1024);
K_THREAD_STACK_DEFINE(reader_stack, 1024);
K_THREAD_STACK_DEFINE(timeout_holder_stack, 1024);
K_THREAD_STACK_DEFINE(timeout_waiter_stack, 1024);
K_THREAD_STACK_DEFINE(recursive_stack, 1024);

/* 定义线程控制块 */
static struct k_thread thread1_data;
static struct k_thread thread2_data;
static struct k_thread writer_data;
static struct k_thread reader_data;
static struct k_thread timeout_holder_data;
static struct k_thread timeout_waiter_data;
static struct k_thread recursive_data;

int main(void)
{
    printk("\n========================================\n");
    printk("Zephyr Mutex 使用示例\n");
    printk("========================================\n\n");

    // --- 示例1: 基本互斥锁操作 ---
    printk("--- 示例1: 基本互斥锁操作 ---\n");
    
    /* 初始化互斥锁 */
    k_mutex_init(&basic_mutex);
    
    /* 创建两个竞争互斥锁的线程 */
    k_thread_create(&thread1_data, thread1_stack, K_THREAD_STACK_SIZEOF(thread1_stack),
                    thread1_func, NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);
    
    k_thread_create(&thread2_data, thread2_stack, K_THREAD_STACK_SIZEOF(thread2_stack),
                    thread2_func, NULL, NULL, NULL,
                    6, 0, K_NO_WAIT);
    
    k_msleep(6000);  // 运行6秒
    
    // --- 示例2: 保护共享资源 ---
    printk("\n--- 示例2: 保护共享资源 ---\n");
    
    /* 初始化互斥锁 */
    k_mutex_init(&resource_mutex);
    
    /* 创建读写线程 */
    k_thread_create(&writer_data, writer_stack, K_THREAD_STACK_SIZEOF(writer_stack),
                    writer_thread_func, NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);
    
    k_thread_create(&reader_data, reader_stack, K_THREAD_STACK_SIZEOF(reader_stack),
                    reader_thread_func, NULL, NULL, NULL,
                    6, 0, K_NO_WAIT);
    
    k_msleep(6000);  // 运行6秒
    
    // --- 示例3: 带超时的互斥锁操作 ---
    printk("\n--- 示例3: 带超时的互斥锁操作 ---\n");
    
    /* 初始化互斥锁 */
    k_mutex_init(&timeout_mutex);
    
    /* 创建持有者和等待者线程 */
    k_thread_create(&timeout_holder_data, timeout_holder_stack, 
                    K_THREAD_STACK_SIZEOF(timeout_holder_stack),
                    timeout_holder_func, NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);
    
    k_thread_create(&timeout_waiter_data, timeout_waiter_stack, 
                    K_THREAD_STACK_SIZEOF(timeout_waiter_stack),
                    timeout_waiter_func, NULL, NULL, NULL,
                    6, 0, K_NO_WAIT);
    
    k_msleep(8000);  // 运行8秒
    
    // --- 示例4: 递归互斥锁 ---
    printk("\n--- 示例4: 递归互斥锁 ---\n");
    
    /* 初始化递归互斥锁 */
    k_mutex_init(&recursive_mutex);
    
    /* 创建递归测试线程 */
    k_thread_create(&recursive_data, recursive_stack, 
                    K_THREAD_STACK_SIZEOF(recursive_stack),
                    recursive_thread_func, NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);
    
    k_msleep(10000);  // 运行10秒

    return 0;
}
