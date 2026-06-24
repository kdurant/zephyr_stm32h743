/* SPDX-License-Identifier: Apache-2.0 */
#include "log_buf.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/sys/printk.h>

/* 环形缓冲区数据 */
static char g_lines[LOG_BUF_LINES][LOG_LINE_MAX];
static int  g_write_idx; /* 当前写入行索引 (0..LOG_BUF_LINES-1) */
static int  g_read_idx;  /* 当前读取行索引 (≤ write_idx)  */
static bool g_wrapped;   /* 是否绕过一圈 */

void log_buf_init(void)
{
    memset(g_lines, 0, sizeof(g_lines));
    g_write_idx = 0;
    g_read_idx  = 0;
    g_wrapped   = false;
}

void log_buf_printf(const char* fmt, ...)
{
    va_list args;

    /* 格式化到当前行 */
    va_start(args, fmt);
    vsnprintf(g_lines[g_write_idx], LOG_LINE_MAX, fmt, args);
    va_end(args);

    /* 推进写指针 */
    g_write_idx = (g_write_idx + 1) % LOG_BUF_LINES;

    /* 如果写指针追上读指针，读指针同步前移（丢弃最旧内容） */
    if(g_write_idx == g_read_idx && g_wrapped)
    {
        g_read_idx = (g_read_idx + 1) % LOG_BUF_LINES;
    }

    if(g_write_idx == 0)
    {
        g_wrapped = true;
    }

    /* 同时输出到串口控制台（仅作 fallback，上位机可通过 CMD_GET_LOG 获取） */
    printk("[OTA] %s\n", g_lines[(g_write_idx + LOG_BUF_LINES - 1) % LOG_BUF_LINES]);
}

int log_buf_read(char* buf, int buf_size)
{
    if(!g_wrapped && g_read_idx == g_write_idx)
    {
        return 0; /* 无新日志 */
    }

    int pos = 0;
    while(pos < buf_size - 1)
    {
        if(!g_wrapped && g_read_idx == g_write_idx)
        {
            break;
        }

        int len       = (int)strnlen(g_lines[g_read_idx], LOG_LINE_MAX);
        int remaining = buf_size - pos - 1;

        if(len > 0)
        {
            int copy_len = (len < remaining) ? len : remaining;
            memcpy(&buf[pos], g_lines[g_read_idx], copy_len);
            pos += copy_len;
            /* 追加换行 */
            if(pos < buf_size - 1)
            {
                buf[pos++] = '\n';
            }
        }

        g_read_idx = (g_read_idx + 1) % LOG_BUF_LINES;
        if(g_read_idx == 0)
        {
            g_wrapped = false;
        }
    }

    buf[pos] = '\0';
    return pos;
}

void log_buf_mark_read(void)
{
    if(g_wrapped)
    {
        g_read_idx = g_write_idx;
        g_wrapped  = false;
    }
}

bool log_buf_has_unread(void)
{
    if(g_wrapped)
        return true;
    return g_read_idx != g_write_idx;
}
