#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app30_task, LOG_LEVEL_INF);

/* 定义任务1 */
void task1_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        uint32_t uptime_ms = k_uptime_get_32();
        k_tid_t tid = k_current_get();
        
        LOG_INF("Task1 - Time: %u ms, Task ID: %p", uptime_ms, (void *)tid);
        
        /* 每1秒打印一次 */
        k_msleep(1000);
    }
}

/* 定义任务2 */
void task2_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        uint32_t uptime_ms = k_uptime_get_32();
        k_tid_t tid = k_current_get();
        
        LOG_INF("Task2 - Time: %u ms, Task ID: %p", uptime_ms, (void *)tid);
        
        /* 每2秒打印一次 */
        k_msleep(2000);
    }
}

/* 定义任务栈大小和优先级 */
#define TASK1_STACK_SIZE 512
#define TASK2_STACK_SIZE 512
#define TASK1_PRIORITY 7
#define TASK2_PRIORITY 8

/* 使用K_THREAD_DEFINE宏定义两个任务 */
K_THREAD_DEFINE(task1_id, TASK1_STACK_SIZE, task1_func, NULL, NULL, NULL,
                TASK1_PRIORITY, 0, 0);

K_THREAD_DEFINE(task2_id, TASK2_STACK_SIZE, task2_func, NULL, NULL, NULL,
                TASK2_PRIORITY, 0, 0);

int main(void)
{
    LOG_INF("app30_task started");
    
    /* 主循环可以为空，或者执行其他任务 */
    while (1) {
        k_msleep(1000);
    }
    
    return 0;
}
