# Zephyr RTOS 进程间通信（IPC）方法选型指南

> 基于 `app31` ~ `app41` 示例的深入分析，覆盖 10 种 IPC 机制。
> 每种方法从**核心原理、最佳场景、优点、缺点**四个维度展开，并对场景重叠的方法做对比区分。

---

## 目录

| 序号 | IPC 方法 | 类型 | 核心特点 |
|:---:|---------|------|---------|
| 1 | [Semaphore](#1-semaphore-信号量) | 同步原语 | 计数型信号，控制并发和资源访问 |
| 2 | [Mutex](#2-mutex-互斥锁) | 同步原语 | 独占锁 + 优先级继承，保护临界区 |
| 3 | [Event](#3-event-事件) | 通知机制 | 32位标志组，OR/AND 多事件等待 |
| 4 | [Poll Signal](#4-poll-signal-轮询信号) | 通知机制 | 轻量信号 + 可携带整数值 |
| 5 | [Message Queue](#5-message-queue-消息队列) | 数据传递 | 固定大小消息复制，生产者-消费者 |
| 6 | [FIFO](#6-fifo-先进先出队列) | 数据传递 | 指针传递，不复制数据 |
| 7 | [LIFO](#7-lifo-后进先出栈) | 数据传递 | 栈结构，后进先出，支持插队 |
| 8 | [Mailbox](#8-mailbox-邮箱) | 数据传递 | 同步/异步发送，双向通信 |
| 9 | [Pipe](#9-pipe-管道) | 数据传递 | 字节流，无消息边界，部分读写 |
| 10 | [ZBus](#10-zbus-发布订阅总线) | 消息总线 | 发布-订阅，Channel + Observer 模式 |
| 11 | [Work Queue](#11-work-queue-工作队列) | 任务调度 | 异步任务执行，ISR 卸载 |

---

## 1. Semaphore（信号量）

### 核心原理

信号量维护一个计数值。`k_sem_give()` 使计数值 +1，`k_sem_take()` 使计数值 -1。当计数值为 0 时，`take` 操作阻塞等待。

```c
k_sem_init(&sem, 0, 1);          // 初始0，最大1（二进制信号量）
k_sem_give(&sem);                // 释放
k_sem_take(&sem, K_FOREVER);    // 获取（阻塞）
k_sem_take(&sem, K_MSEC(100));  // 获取（超时）
```

### 最佳使用场景

- **生产-消费同步**：生产者生产数据后 `give`，消费者 `take` 等待数据就绪
- **资源池管理**：初始计数值=N 表示 N 个可用资源，每分配一个 `take`，每归还一个 `give`
- **线程间简单事件通知**：A 线程完成任务后 `give`，B 线程 `take` 获知
- **ISR 到线程的通知**：ISR 中 `give` 信号量唤醒等待线程

### 优点

- 简单直观，API 只有 give/take 两个核心操作
- 支持 ISR 中使用（`k_sem_give` from ISR）
- 计数型信号量天然适合资源池管理
- 支持超时等待（`K_MSEC` / `K_NO_WAIT`）

### 缺点

- **无数据传递能力**：仅能做通知，不能携带数据
- 无法区分事件类型——只有一个计数值
- 二进制信号量有优先级反转风险（无优先级继承）
- 误操作（多 give 或遗忘 take）导致计数错误，难以调试
- 不适合表达"等待 A **且** B 同时发生"的复合条件

---

## 2. Mutex（互斥锁）

### 核心原理

互斥锁提供互斥访问。同一时刻只有一个线程能持有锁。Zephyr mutex 实现**优先级继承协议**，防止优先级反转。

```c
k_mutex_init(&mutex);
k_mutex_lock(&mutex, K_FOREVER);   // 加锁（阻塞）
// --- 临界区 ---
k_mutex_unlock(&mutex);            // 解锁
k_mutex_lock(&mutex, K_MSEC(100)); // 加锁（超时）
```

### 最佳使用场景

- **保护共享资源**：多个线程读写同一全局变量、缓冲区、设备寄存器
- **临界区保护**：确保一段代码不会被多个线程同时执行
- **需要优先级继承的场景**：高优先级线程等低优先级线程释放资源时避免无限反转
- **递归锁定**：同一线程可多次 lock（但需匹配解锁次数），适合嵌套调用

### 优点

- **优先级继承**：自动继承等待者中最高优先级，防止优先级反转
- 支持带超时的 lock（`K_MSEC`）
- 支持**递归锁**（同一线程可多次加锁）
- 语义清晰：lock = 进入临界区，unlock = 离开临界区

### 缺点

- **只做保护**，不做通知——不能用于线程间事件同步
- 持有锁期间不能睡眠太久（会阻塞所有其他竞争线程）
- 可能死锁：A 等 B 释放锁、B 等 A 释放锁，需要开发者自行避免
- 递归锁需匹配 lock/unlock 次数，否则锁泄漏

### Semaphore vs Mutex 区别

| 维度 | Semaphore | Mutex |
|------|-----------|-------|
| 用途 | 同步 + 资源计数 | 互斥 + 临界区保护 |
| 持有者 | 任何线程/ISR 可 give | 谁 lock 谁 unlock |
| 优先级继承 | ❌ | ✅ |
| 递归 | ❌ | ✅ |
| 典型场景 | 等待事件发生 | 保护共享变量不被并发修改 |

---

## 3. Event（事件）

### 核心原理

Event 是一个 32 位标志组。`k_event_post()` 置位，`k_event_wait()` 等待**任意**位触发（OR），`k_event_wait_all()` 等待**所有**位触发（AND）。

```c
k_event_init(&event);
k_event_post(&event, BIT_READY | BIT_DATA);        // 同时置多个位
uint32_t r = k_event_wait(&event, BIT_A | BIT_B,
                          true, K_FOREVER);         // 等任意一个
uint32_t r = k_event_wait_all(&event, BIT_A | BIT_B,
                              true, K_FOREVER);     // 等所有
```

### 最佳使用场景

- **多条件同步**：必须等传感器 A **且** 传感器 B 都就绪（`k_event_wait_all` AND 语义）
- **多事件监听**：等待网络数据、超时、错误等**任一**事件（`k_event_wait` OR 语义）
- **状态机驱动**：每个事件位代表一种状态变化，状态机等待事件组合
- **ISR 批量通知**：多个 ISR 分别 post 不同事件位，线程统一等待

### 优点

- **32 个独立事件位**，可同时传递多种不同事件类型
- OR 和 AND 两种等待模式，覆盖绝大多数组合条件场景
- `reset=true` 参数自动清除已触发事件
- ISR 安全（`k_event_post` from ISR）

### 缺点

- **无法传递数据**——仅做事件通知
- 事件位需手动管理分组，大型系统可能不够用（32位上限）
- 没有优先级继承
- 过度使用多个 event 对象会使代码逻辑复杂

### Event vs Semaphore 区别

| 维度 | Event | Semaphore |
|------|-------|-----------|
| 信息量 | 32 种独立事件 | 1 个计数值 |
| AND 等待 | ✅ `k_event_wait_all` | ❌（需多个信号量组合） |
| OR 等待 | ✅ 默认行为 | ❌（单个信号量无"或"概念） |
| 数据携带 | ❌ | ❌ |
| 典型场景 | 多传感器同步 | 简单生产-消费通知 |

---

## 4. Poll Signal（轮询信号）

### 核心原理

`k_poll_signal` 是一种轻量级通知，可通过 `k_poll()` + `k_poll_event` 统一等待多种事件。与 semaphore 的本质区别：**可携带一个整数值（result）**。

```c
k_poll_signal_init(&sig);
k_poll_event_init(&evt, K_POLL_TYPE_SIGNAL,
                  K_POLL_MODE_NOTIFY_ONLY, &sig);
k_poll(&evt, 1, K_FOREVER);           // 等待
k_poll_signal_raise(&sig, result);    // 触发（携带整数值）
// 接收方: evt[0].signal->result 获取携带值
k_poll_signal_reset(&sig);            // 重置（必须！）
```

### 最佳使用场景

- **命令分发**：携带 `CMD_READ` / `CMD_WRITE` 等命令码，一个 signal 区分多种操作
- **k_poll 多路复用**：与 FIFO、信号量等一起被 `k_poll()` 统一等待
- **轻量级 ISR 通知**：ISR 中 `raise` 携带错误码/状态码
- **线程间 RPC 调用**：携带函数编号或操作码

### 优点

- **携带整数结果值**：semaphore 做不到的——区分命令类型
- 可被 `k_poll()` 统一管理，同时等待 signal + FIFO + semaphore
- ISR 安全
- 轻量——仅需 `struct k_poll_signal` + `struct k_poll_event`

### 缺点

- **使用门槛较高**：必须手动 `reset` + 重置 `events[].state`，遗忘会导致死等
- 只能携带 1 个 int，无法传递复杂数据
- 必须搭配 `k_poll()` 使用，不能单独 `wait`（不像 semaphore 有 `k_sem_take`）
- 需要 `CONFIG_POLL=y`

### Poll Signal vs Semaphore 区别

| 维度 | Poll Signal | Semaphore |
|------|-------------|-----------|
| 数据携带 | ✅ 整数值 result | ❌ |
| 等待方式 | `k_poll()` 多路复用 | `k_sem_take()` 单一等待 |
| 重置要求 | ⚠️ 必须手动 reset | 自动减计数 |
| k_poll 配合 | ✅ 天然支持 | ✅ 也可配合 |
| 典型场景 | 命令分发、多路等待 | 资源计数、二值通知 |

---

## 5. Message Queue（消息队列）

### 核心原理

MSGQ 是固定大小的消息队列。每条消息按 `sizeof(msg_type)` **完整复制**到队列的内部环形缓冲区。生产者 `put`，消费者 `get`。

```c
K_MSGQ_DEFINE(mq, sizeof(struct msg), 8, 4);  // 8条消息，4字节对齐
k_msgq_put(&mq, &msg, K_NO_WAIT);              // 不满则入队
k_msgq_get(&mq, &msg, K_FOREVER);              // 阻塞获取
k_msgq_num_free_get(&mq);                      // 查询剩余空间
```

### 最佳使用场景

- **固定数据采集**：传感器定时采集，每次数据大小固定，放入队列缓冲
- **生产者-消费者**（数据复制模式）：生产者产生数据，消费者以自己的节奏处理
- **日志/事件队列**：多个线程写入结构化日志到 MSGQ，专门线程异步写出
- **命令队列**：每条命令固定大小，串行化处理

### 优点

- **数据完整复制**：put 后发送方可立即重用/释放源数据，不影响队列内容
- **固定大小**：内存占用可预测，无碎片
- 查询队列状态：`num_free_get` / `num_used_get`
- 支持超时 put/get（`K_NO_WAIT` / `K_MSEC` / `K_FOREVER`）
- ISR 安全

### 缺点

- **复制开销**：大消息复制耗时，不适合传递大数据
- **固定消息大小**：不能传递变长数据（需额外设计 buffer 管理）
- 队列满时 put 阻塞（除非 `K_NO_WAIT` 返回 -ENOMSG）
- 静态创建（`K_MSGQ_DEFINE`）后大小不可变

### MSGQ vs FIFO 区别

| 维度 | Message Queue | FIFO |
|------|--------------|------|
| 传递方式 | **复制**整个消息到队列缓冲区 | **传递指针**，不复制数据 |
| 源数据安全 | put 后可立即释放 | ⚠️ get 前不能释放 |
| 内存开销 | 队列缓冲区 = `max_msgs × msg_size` | 仅存指针，极小开销 |
| 适合数据大小 | 小消息（几十字节） | 大块数据 / 复杂结构体 |
| 动态消息大小 | ❌ | ✅（变长数据通过指针） |

---

## 6. FIFO（先进先出队列）

### 核心原理

FIFO 是一个**指针队列**——只存储数据地址，不复制数据本身。`k_fifo_put()` 存入指针，`k_fifo_get()` 取出指针。

```c
K_FIFO_DEFINE(fifo);
k_fifo_put(&fifo, &data);            // 存入指针（第一字被内核占用）
struct item *p = k_fifo_get(&fifo, K_FOREVER);
//                     ↑ 直接返回数据指针
k_fifo_alloc_put(&fifo, &data);      // 内核分配节点，数据不被修改
```

> ⚠️ **两种 put 模式的关键差别**：
> - `k_fifo_put`：数据 struct 的第一个 word 被内核覆写，必须预留 `void *fifo_reserved` 字段
> - `k_fifo_alloc_put`：内核单独分配管理节点，数据 struct **不被修改**，但需 `k_free`

### 最佳使用场景

- **传输大数据块**：传感器帧、图像数据、网络包——复制成本高，传指针经济
- **内存池 + FIFO 模式**：预分配一组 buffer，FIFO 管理空闲/忙两个队列
- **变长数据处理**：不同消息大小不同时，指针传递天然支持变长
- **零拷贝数据管道**：A 线程填充数据，B 线程消费后归还内存池

### 优点

- **零拷贝**：只传指针，极大降低内存和 CPU 开销
- 数据大小无限制（只要内存够）
- `k_fifo_alloc_put` 不修改用户数据内容
- ISR 安全

### 缺点

- **数据生命周期管理复杂**：get 前源数据不能释放/重用
- 需要内存池配合：预分配池或 `k_malloc` 动态分配
- `k_fifo_put` 会覆写数据第一字——容易犯错
- `k_fifo_alloc_put` 可能返回 `-ENOMEM`
- 需要 `CONFIG_HEAP_MEM_POOL_SIZE`（如果用 `k_malloc`）

### FIFO vs LIFO 区别

| 维度 | FIFO | LIFO |
|------|------|------|
| 取出顺序 | 先入先出 | 后入先出 |
| 公平性 | ✅ 公平（顺序处理） | ❌ 旧数据可能"饿死" |
| 典型场景 | 数据流/任务队列 | 紧急命令插队 |
| API | 完全相同 | 完全相同 |

---

## 7. LIFO（后进先出栈）

### 核心原理

API 与 FIFO 完全一致，唯一区别：`k_lifo_put()` 将数据**插入头部**（栈顶），因此最后放入的项最先被 `k_lifo_get()` 取出。

```c
K_LIFO_DEFINE(stack);
k_lifo_put(&stack, &task);     // 压栈
struct task *t = k_lifo_get(&stack, K_FOREVER); // 弹栈（后进先出）
// 放入 0→1→2→3→4，取出顺序: 4→3→2→1→0
```

### 最佳使用场景

- **紧急命令插队**：先放入普通命令，后放入紧急命令（LIFO 保证紧急优先执行）
- **表达式求值栈**：如 RPN 计算器的操作数栈
- **深度优先遍历辅助**：路径回溯时后访问的节点先处理
- **撤销/回退操作**：最后执行的操作最先被撤销

### 优点

- 同 FIFO：零拷贝、变长数据
- **天然优先级**：后放入 = 高优先级（紧急消息自动插队）
- API 与 FIFO 统一，切换只需改 `fifo` → `lifo`

### 缺点

- 同 FIFO 的数据生命周期管理问题
- **可能饥饿**：早放入的低优先级数据可能永远不被处理
- 不适合"公平"处理场景——如果需要顺序处理，用 FIFO

### MSGQ vs FIFO vs LIFO 三向对比

| 维度 | MSGQ | FIFO | LIFO |
|------|------|------|------|
| 传递方式 | 复制数据 | 传指针 | 传指针 |
| 顺序 | FIFO | FIFO | LIFO |
| 变长数据 | ❌ | ✅ | ✅ |
| 数据安全 | ✅ 复制后源可释放 | ⚠️ 取出前不能释放 | ⚠️ 取出前不能释放 |
| 优先级 | 无（纯 FIFO） | 无（纯 FIFO） | 有（后进 = 高优先级） |

---

## 8. Mailbox（邮箱）

### 核心原理

Mailbox 是**同步/异步双模**消息传递。发送方可选择阻塞等接收方处理完（`k_mbox_put`）或立即返回并等信号量通知（`k_mbox_async_put`）。消息通过 `struct k_mbox_msg` 传递，其中 `info` 字段可作消息类型标识，`tx_data` 传递实际数据。

```c
K_MBOX_DEFINE(mbox);

// 同步发送：阻塞到接收方处理完
struct k_mbox_msg tx = { .size=sizeof(d), .tx_data=&d, .info=id };
k_mbox_put(&mbox, &tx, K_FOREVER);

// 异步发送：立即返回，通过信号量通知完成
k_mbox_async_put(&mbox, &tx, &done_sem);
k_sem_take(&done_sem, K_FOREVER);  // 等待处理完成

// 接收
k_mbox_get(&mbox, &rx, &data, K_FOREVER);     // 直接取数据
k_mbox_get(&mbox, &rx, NULL, K_FOREVER);       // 延迟：先收消息描述
k_mbox_data_get(&rx, &data);                   // 再取数据+释放发送方
```

### 最佳使用场景

- **需要发送确认**：发送方必须知道接收方已处理完数据（如配置命令）
- **异步任务提交**：发送后立即返回继续做其他事，信号量通知完成
- **双向通信**：接收方可获取发送线程 ID（`rx_source_thread`），实现回复
- **远程过程调用 (RPC) 模拟**：发送方"调用"，等接收方"执行并返回"

### 优点

- **同步/异步双模式**：一个 API 满足两种需求
- **info 字段**：元数据与数据分离，如 `info=命令ID, tx_data=命令参数`
- **延迟接收**：先收消息头判断类型，再决定如何处理数据体
- **发送确认**：同步模式下 `k_mbox_put` 返回即保证数据已被处理
- 可指定目标线程（`tx_target_thread`）

### 缺点

- **复杂的生命周期语义**：同步 put → get → data_get 三阶段，容易混淆
- 同步模式下接收方慢会阻塞发送方
- 结构体字段较多（`size, info, tx_data, tx_target_thread`），使用繁琐
- 异步模式需要 `CONFIG_NUM_MBOX_ASYNC_MSGS` 配置

### Mailbox vs MSGQ 区别

| 维度 | Mailbox | MSGQ |
|------|---------|------|
| 发送确认 | ✅ 同步模式下发送方等接收方处理完 | ❌ put 后立即返回 |
| 异步+回调 | ✅ async_put + sem | ❌ |
| 元数据 | ✅ `info` 字段 | ❌（需在消息体内嵌入） |
| 双向通信 | ✅ `rx_source_thread` | ❌ |
| 使用复杂度 | 高（三阶段） | 低（put/get 两步） |
| 适合场景 | 需要确认的关键命令 | 大批量数据缓冲 |

---

## 9. Pipe（管道）

### 核心原理

Pipe 是**字节流管道**（类似 Unix pipe），内部使用环形缓冲区。`k_pipe_write()` 写入任意长度字节，`k_pipe_read()` 读取任意长度字节，**支持部分读写**。可 `k_pipe_reset()` 清空管道。

```c
K_PIPE_DEFINE(pipe, 64, 4);                 // 64字节缓冲区
int n = k_pipe_write(&pipe, buf, len, K_NO_WAIT); // 写入（可能部分写）
int n = k_pipe_read(&pipe, buf, 64, K_FOREVER);   // 读取（可能部分读）
k_pipe_reset(&pipe);                        // 清空管道，解除阻塞
```

### 最佳使用场景

- **字节流传输**：串口数据、网络 TCP 流、音频采样流
- **变长协议解析**：数据无固定消息边界，逐字节或逐块消费
- **日志缓冲管道**：多线程写入日志，后台线程攒批写出
- **需要清空/重置的场景**：`k_pipe_reset` 一键丢弃所有未读数据
- **流式数据处理**：数据长度不固定，消费端逐步累积累到完整消息

### 优点

- **字节流语义**：无消息边界，最灵活
- **部分读写支持**：缓冲区不够时不丢数据，返回实际传输字节数
- `k_pipe_reset`：特殊场景下快速清空
- 环形缓冲区实现，无碎片

### 缺点

- **无消息边界**：这是双刃剑——需要应用层自行解析消息边界
- 不适合"收发固定结构体"的场景（此时 MSGQ 更合适）
- `K_NO_WAIT` 时可能写入/读出不完整，需循环处理
- 无法像 MSGQ 那样按"消息条数"计数

### Pipe vs MSGQ vs FIFO 区别

| 维度 | Pipe | MSGQ | FIFO |
|------|------|------|------|
| 数据单元 | 字节流 | 固定大小消息 | 单个指针 |
| 消息边界 | ❌ 无 | ✅ 有 | ❌ 无 |
| 部分读写 | ✅ | ❌ | ❌ |
| 内容复制 | ✅ 复制 | ✅ 复制 | ❌ 不复制 |
| 典型场景 | 串口流 | 传感器数据采集 | 大块数据传递 |

---

## 10. ZBus（发布-订阅总线）

### 核心原理

ZBus 是**发布-订阅消息总线**。核心三要素：**Channel**（消息通道）、**Subscriber**（订阅线程，通过 FIFO 异步接收）、**Listener**（监听回调，发布时同步调用）。一个 Channel 可绑定多个 Observer。

```c
// 1. 定义消息类型
struct sensor_msg { int temp, hum; };

// 2. 定义校验器
static bool validator(const void *msg, size_t len) { /* 校验逻辑 */ }

// 3. 定义 Observer
ZBUS_SUBSCRIBER_DEFINE(sub, 4);              // 订阅者（FIFO 容量4）
ZBUS_LISTENER_DEFINE(lis, callback);          // 监听者（回调）

// 4. 定义 Channel（编译时绑定所有 observer）
ZBUS_CHAN_DEFINE(chan, struct sensor_msg, validator, NULL,
                 ZBUS_OBSERVERS(sub, lis), {0});

// 5. 发布和接收
zbus_chan_pub(&chan, &msg, K_MSEC(100));     // 发布（触发 listener + 通知 subscriber）
zbus_chan_read(&chan, &msg, K_FOREVER);       // Subscriber 读取
```

### 最佳使用场景

- **多模块松耦合通信**：传感器模块发布，显示模块、存储模块、网络模块各自订阅
- **一对多广播**：一条消息同时送达多个消费者（Subscriber FIFO + Listener 回调）
- **需要消息校验**：发布时自动调用 validator 过滤非法数据
- **需要实时 + 异步双模式的系统**：Listener 实时响应，Subscriber 异步处理
- **编译时静态绑定**：所有通道和 observer 关系在编译时确定，适合资源受限的嵌入式系统

### 优点

- **发布-订阅解耦**：发布者不关心谁在消费，消费者不关心谁在发布
- **一对多广播**：原生支持，一个 publish 送达所有 observer
- **消息校验器**：自动过滤，非法消息在入口被拒绝
- **双观察模式**：Listener 同步回调（实时）+ Subscriber 异步 FIFO（解耦）
- 编译时静态绑定，运行时零开销
- 支持优先级：observer 列表顺序即优先级顺序

### 缺点

- **需要额外 include**：`<zephyr/zbus/zbus.h>`，非 kernel.h 内置
- 需要 `CONFIG_ZBUS=y`
- 编译时绑定意味着**无法运行时动态添加/移除 observer**（除非启用 `CONFIG_ZBUS_RUNTIME_OBSERVERS`）
- 学习曲线比 MSGQ/FIFO 陡峭（Channel + Subscriber + Listener 三概念）
- `ZBUS_CHAN_DEFINE` 宏参数中 `{0}` 不能用嵌套逗号（如 `{0, 0}` 会被预处理器错误解析）
- 不适合"点对点"的简单通信（用 MSGQ 更直接）

### ZBus vs Mailbox vs MSGQ 区别

| 维度 | ZBus | Mailbox | MSGQ |
|------|------|---------|------|
| 通信模式 | 一对多广播 | 点对点 + 确认 | 点对点 |
| 发送确认 | ❌ | ✅ 同步模式 | ❌ |
| 消息校验 | ✅ validator | ❌ | ❌ |
| Listener 回调 | ✅ 内置 | ❌ | ❌ |
| 动态绑定 | ❌（默认静态） | ❌ | ❌ |
| 适合系统规模 | 多模块大系统 | 关键命令交互 | 简单数据队列 |

---

## 11. Work Queue（工作队列）

### 核心原理

Work Queue 提供**异步任务执行**。提交 work 项到队列，后台线程按 FIFO 顺序执行 handler。三种变体：

- **普通 Work**：`k_work_submit` → 立即入队执行
- **Delayable Work**：`k_work_schedule` → 延迟指定时间后执行（可周期性重调度）
- **自定义队列**：`k_work_queue_start` → 独立队列线程，与系统队列分离

```c
// 普通 work
K_WORK_DEFINE(work, handler);
k_work_submit(&work);                        // 提交到系统队列

// 延迟 work
K_WORK_DELAYABLE_DEFINE(dwork, handler);
k_work_schedule(&dwork, K_MSEC(500));        // 500ms 后执行

// 自定义队列
struct k_work_q my_q;
k_work_queue_start(&my_q, stack, size, prio, &cfg);
k_work_submit_to_queue(&my_q, &work);        // 提交到自定义队列
```

### 最佳使用场景

- **ISR 卸载**：ISR 中只做最轻的标记操作，耗时处理提交到 work queue
- **定时/周期性任务**：delayable work 在 handler 中重新 `k_work_schedule` 自己实现周期执行
- **隔离耗时任务**：自定义队列专门执行耗时 work，不影响系统队列的响应
- **异步 I/O 完成处理**：I/O 事件触发后，将数据后处理提交到 work queue
- **GUI / 事件驱动的应用层逻辑**：将事件响应拆分为 work 项串行执行

### 优点

- **ISR 卸载的核心工具**：避免在 ISR 中执行长耗时操作
- 系统队列 `system_work_q` 自动创建，开箱即用
- Delayable work 天然支持周期性任务
- 自定义队列隔离——耗时任务不阻塞系统队列
- 线程栈大小可定制（FIFO/LIFO 无此能力）

### 缺点

- 非严格的 IPC 机制——更像**任务调度器**而非进程间通信
- 系统 work queue 线程优先级可能影响实时性（可调，但要注意）
- Delayable work 的精度受 tick 频率影响（`CONFIG_SYS_CLOCK_TICKS_PER_SEC`）
- Work 项本身无参数传递——通常依赖全局变量或容器宏（`CONTAINER_OF`）访问上下文
- 不适合需要**返回值**或**同步等待**的场景（work 执行完不会通知提交者）

### Work Queue vs 其他 IPC

Work Queue 是**任务调度**，不是**数据通信**。它不传递数据，而是在后台执行函数。将其与 MSGQ 配合使用是常见模式：

> ISR 通过 MSGQ 发送数据 → 工作线程从 MSGQ 取数据 → 提交 work 到队列处理

---

## 附录 A：数据传递能力速查

```
无数据传递（仅同步/通知）:
  Semaphore     —— 计数信号
  Mutex         —— 互斥保护
  Event         —— 32位标志
  Work Queue    —— 任务调度

携带简单值:
  Poll Signal   —— 1 个 int result

传递结构化数据（复制）:
  MSGQ          —— 定长消息复制
  Pipe          —— 字节流复制

传递结构化数据（引用/指针）:
  FIFO / LIFO   —— 传指针，不复制
  Mailbox       —— tx_data 指针 + info 元数据
  ZBus          —— Channel 消息指针 + 校验

ISR 安全:
  除 Work Queue（无可使用 ISR 场景）外，其余均可在 ISR 中发送端操作
```

## 附录 B：选型决策流程

```
需要多个线程互斥访问共享资源？
  └→ YES: Mutex

需要线程/ISR 通知另一个线程"事件发生"？
  ├→ 单一事件: Semaphore
  ├→ 多事件 OR 逻辑: Event (k_event_wait)
  └→ 多事件 AND 逻辑: Event (k_event_wait_all)

需要传递数据？
  ├→ 小数据 + 复制 + 发送即忘: MSGQ
  ├→ 大数据 + 零拷贝: FIFO
  ├→ 需要发送确认 + 同步等结果: Mailbox
  ├→ 字节流 / 变长数据: Pipe
  └→ 一对多广播 / 松耦合多模块: ZBus

需要紧急消息插队？
  └→ LIFO

需要 ISR 中触发后台处理？
  ├→ 简单通知: Semaphore / Event
  ├→ 带数据: MSGQ / FIFO / Pipe
  └→ 复杂处理: Work Queue (ISR只提交work)
```
