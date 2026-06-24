/* SPDX-License-Identifier: Apache-2.0 */
/* 环形日志缓冲区 — 供 OTA 协议传输到上位机 */

#ifndef LOG_BUF_H
#define LOG_BUF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* 缓冲区可容纳 48 行，每行最多 120 字符 */
#define LOG_BUF_LINES 48
#define LOG_LINE_MAX  120

void log_buf_init(void);
void log_buf_printf(const char* fmt, ...);

/* 读取所有未读日志，返回写入字节数 (不含末尾 null)，读完后标记为已读 */
int log_buf_read(char* buf, int buf_size);

/* 强制标记为已读（丢弃未读内容） */
void log_buf_mark_read(void);

/* 判断是否有未读日志 */
bool log_buf_has_unread(void);

#endif /* LOG_BUF_H */
