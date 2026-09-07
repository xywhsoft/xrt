---
num: 48
slug: signal
title: 跨平台信号
volume: 卷五 系统服务
type: practice
lead: 信号观察者、一次性语义与自动清理——Ctrl+C 到 SIGTERM 的跨平台优雅处理。
api: signal
---

## 导读

signal 模块把 POSIX 信号与 Windows 控制台事件统一成**信号观察者**模型：`xsignalwatch` 句柄 + `xrtSignalOn/Once/OnOwned/OnceOwned` 注册族订阅信号，事件以**异步通知**送达（不是信号处理函数的受限环境）；**一次性语义**（once）适配 SIGINT 这类"只处理一次"的信号；自动清理避免退出路径的悬空触发。三个高频信号——SIGINT（Ctrl+C）、SIGTERM（编排系统的停止指令）、SIGHUP（配置重载的传统信号）——是服务生命周期管理的三大入口。

## 引入

裸写信号处理函数是 C 门的著名雷区：**信号处理函数里只有一小撮函数是信号安全的**（printf/malloc 都不在内——在处理函数里打日志或分配内存是未定义行为）；跨平台差异（Windows 没有真正的 POSIX 信号，Ctrl+C 是控制台事件）；处理函数与主循环的通信只能靠 `volatile sig_atomic_t` 这类受限机制。每一条都是踩过的坑。

XRT 的解法是**观察者 + 异步投递**：注册观察者时声明关心的信号；信号发生时，模块在安全上下文（不是原始处理函数）把事件投递给你的回调——回调里可以正常使用全库 API；`xsignalevent` 携带信号名与计数（同一信号多次到达合并计数）。受限环境的问题被结构性绕开，而不是靠程序员背信号安全函数清单。

## 概念

### 观察者模型

```diagram flow
- 注册：xrtSignalOn(信号, 回调, 数据) 常驻 / Once 一次性 / OnOwned·OnceOwned 绑定拥有者
- 到达：信号发生 → 模块捕获 → 安全上下文投递 xsignalevent（名字+计数）
- 回调：正常环境执行——printf/日志/容器全部可用
- 清理：xrtSignalOff/Free 注销或进程退出自动清理——悬空触发被防止
```

计数语义值得注意：回调可能合并多次到达（事件带 `count`）——"Ctrl+C 按了三下"是一个 count=3 的事件而不是三次回调，快速连按下的用户意图（强制退出）不会被逐次处理的延迟吞掉。

### 一次性语义与 owned-on

signal_tour 范例展开两组进阶语义：**once**——观察者触发一次后自动注销（SIGINT 的"首次 Ctrl+C 优雅退出"用例：第二次 Ctrl+C 想直接杀进程就不要再拦了）；**owned-on**——观察者生命周期绑定到拥有者（自动销毁计数验证 `destroyed=1`），退出路径不悬空。两个语义合起来覆盖"信号处理的收尾"这个传统难点。

### 三大入口信号

| 信号 | 触发 | 标准响应 |
| --- | --- | --- |
| SIGINT | Ctrl+C（交互） | 优雅退出：停止接新、收尾存量、限时强退 |
| SIGTERM | 编排系统停止（k8s/docker stop） | 同上——容器环境的主要退出通道 |
| SIGHUP | 终端断开/传统重载信号 | 配置重载：重读配置、原子切换（第 45 章） |

三大入口共同指向**优雅退出/重载**模式：收到信号 → 置停止/重载标志 → 主循环检查标志 → 走收尾路径（Flush 日志、关闭句柄、等待在途请求）——信号回调只做标志位，重活留给主循环（与第 47 章回调轻活纪律同源）。

### 与第 50 章进程的分工

本章的 signal 模块管**接收**（我的进程怎么响应信号）；第 50 章的 process 模块管**发送**（我怎么向子进程发信号——超时杀进程的 SIGTERM→SIGKILL 阶梯）。收发两端共用信号名（`xrtSignalName`），但 API 各归各——与文件族的章节分工逻辑一致。

## 示例

### 完整程序：信号观察与计数

来自仓库范例 `examples/process/signal/main.c`：

```embed path="examples/process/signal/main.c" title="examples/process/signal/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/process/signal/main.c -lws2_32 -liphlpapi
$ ./a.exe
signal=INT count=1 total=1
```

**刚才发生了什么。** ① 观察者注册关心的信号集与回调——`xsignalwatch` 句柄保存这个订阅。② 信号到达（Ctrl+C）后事件投递到回调——`xsignalevent` 携带 `signal=INT`（名字）与 `count=1`（本次合并次数）；`total` 是范例自己维护的累计计数。③ 回调在正常环境执行——printf 直接用（原始信号处理函数里这是未定义行为，观察者模型把它变合法）。④ 事件还带 `Total`（累计总数）与 `Name`（字符串名）——`signal=INT count=1 total=1` 三字段的来源。程序在收到信号后有序退出，观察者清理自动完成。

### 完整程序：全接口巡礼

来自 `examples/process/signal_tour/main.c`——支持性查询、owned-on、once 与计数管理：

```embed path="examples/process/signal_tour/main.c" title="examples/process/signal_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/process/signal_tour/main.c -lws2_32 -liphlpapi
signal: supported=1/1 name=INT healthy=1
signal: owned-on fired=1 destroyed=1
signal: once fired=2 destroyed=1
signal: count/received/clear ok
signal: ignore/restore/all ok
```

**刚才发生了什么。** ① 支持性查询（`supported=1/1`）与名字转换（`name=INT`）——信号能力按平台可探测（写可移植代码时先问再注册）。② owned-on：拥有者作用域内触发（`fired=1`）、拥有者销毁时观察者自动清理（`destroyed=1`）——生命周期绑定防悬空。③ once：两次触发（`fired=2`）、一次性观察者随后自毁（`destroyed=1`）——"首次拦截、后续放行"的机制基础。④ 计数、收取与清除（`count/received/clear ok`）——轮询式消费的入口。⑤ 忽略、恢复与批量清理（`ignore/restore/all ok`）——信号掩码管理：临时忽略某个信号再恢复原状，批量注销收尾。

## 契约

- **安全环境**：回调在正常上下文执行——全库 API 可用；原始处理函数的受限性由模块内部消化。
- **合并计数**：同一信号多次到达合并为一个事件（count 字段）；快速重复触发不丢用户意图。
- **once/owned**：一次性语义（触发后自毁）与拥有者绑定（随拥有者清理）——收尾路径不悬空。
- **回调轻活**：回调只置标志/入队，重活留给主循环（第 47 章纪律的信号版）。
- **收发分工**：本章管接收；向子进程发信号在第 50 章进程模块。
- **能力探测**：信号支持按平台可查询——可移植代码先探测再注册。

### 从示例到工程：信号处理的三个宿主

**服务生命周期**（三大入口的主场）：启动时注册 SIGINT/SIGTERM 的 once 观察者 → 首次信号置停止标志 → 主循环收尾（停接入、等存量限时、Flush 日志、关句柄）→ 退出；once 注销后的第二次信号走默认终止——强制退出的出口。**配置热重载**：SIGHUP 触发重载流水线（第 42 章配置 → diff → 原子切换）——信号是触发器，重活全在主循环的既定路径上。**子进程监管**：与第 50 章联动——父进程的 SIGCHLD 类事件经观察者进入 reaper 流程（回收子进程资源），发信号端在进程模块。三宿主的信号回调都是同一个形状：**置标志或入队，几行收工**——第 47 章轻活纪律的完全一致。

### 一个历史包袱的化解：为什么信号 API 长这样

理解本章 API 的设计，需要知道它在化解什么。POSIX 信号处理函数运行在"被抢占的任意上下文"——你的主线程可能正拿着 malloc 的内部锁时被信号打断，处理函数里再调 malloc 就是同一把锁重入——死锁。所以传统信号处理只允许异步信号安全函数（一个很短的清单），连 printf 都不行。XRT 的观察者模型在底层处理函数里只做最小捕获，把投递搬到安全上下文——你写的回调因此"看起来是普通函数"。这不是把复杂度藏起来，是把复杂度移到该在的层：**受限环境的纪律由模块承担，业务回调保持正常代码的直观**。同样的思路你在第 47 章（系统回调→任务池）与第 58 章预演（IO 完成通知）都会再见——"边界处最小化、业务处正常化"是 XRT 异步设计的通用哲学。

### 信号与优雅退出的完整清单

优雅退出是信号处理的集大成场景，一份完整清单收尾本章：**注册**——SIGINT/SIGTERM once 观察者（首次拦截）；**标志**——原子停止位，回调唯一动作；**收尾顺序**——停接新（监听器关闭）→ 等存量（限时 deadline）→ Flush（日志/文件）→ 关句柄（反向依赖序）→ 退出码（有意区分优雅与超时）；**超时出口**——限期到或二次信号即强退，不无限等；**测试**——信号路径要演练（CI 里向自己发信号验证收尾完整）。六项清单就是练习二挑战的评分标准——也是每个上线服务的"停止动作"验收单。

## 避坑

### 坑 1：信号回调里做重活

症状：信号处理期间偶发死锁或崩溃——日志系统在回调里 Flush 撞上主线程的同一 Sink；分配器在回调里加锁撞上被信号打断的同把锁。

原因：信号随时到达——回调与主线程的任意代码并发；回调里碰主线程正在用的非重入资源（日志、分配器、容器）就是竞争。

```c bad
static void onSignal(const xsignalevent* pEvent, ptr pData)
{
	StopAllWorkers();     /* 重活：停线程池、Flush 日志——并发竞争集合 */
	ExitProcess(0);
}
```

```c good
static volatile bool gStop = false;   /* 标志位（或原子） */
static void onSignal(const xsignalevent* pEvent, ptr pData)
{
	gStop = true;        /* 轻活：只置标志 */
}
/* 主循环检查 gStop → 走收尾路径（停池/Flush/关句柄——安全上下文） */
```

### 坑 2：忽略第二次信号的用户意图

症状：服务卡在收尾路径不动——用户连按 Ctrl+C 毫无反应，最后只能 kill -9；收尾代码里的某个等待永不超时。

原因：首次信号触发了优雅退出，但收尾路径没有限时；后续 Ctrl+C 被同一个观察者继续"优雅吞掉"——用户的"强制退出"意图被忽略。

```c bad
xrtSignalOn(SIGINT, gracefulSignal, NULL);   /* 常驻：每次信号都优雅——没有出口 */
static void gracefulSignal(...) { gStop = true; }
/* 主循环： while(!gStop) ... 收尾 WaitAll() 可能永远等 */
```

```c good
xrtSignalOnce(SIGINT, firstSignal, NULL);    /* once 语义：只拦首次 */
static void firstSignal(...) { gStop = true; }
/* 收尾限时（第 41 章 deadline）；超时或二信号 → 直接退出 */
/* once 注销后第二次 Ctrl+C 走默认行为（终止）——强制退出的出口 */
```

## 练习

### 基础：三大信号注册

注册 SIGINT/SIGTERM/SIGHUP 三个观察者，分别置停止与重载标志；主循环每秒检查并打印状态变化（模拟优雅退出与热重载两条路径）。

### 进阶：优雅退出限时器

实现完整的退出流程：首次信号 → 停止接新 → 等待存量（限时 5 秒）→ Flush → 退出；限时内完成输出"graceful"、超时强退输出"forced"。用两个线程模拟在途工作验证等待语义。

### 挑战：配置热重载器

SIGHUP 触发配置重载：第 42 章三层配置重读 → 新旧值树 diff（第 31 章遍历）→ 变更字段打印 + 应用（日志级别立即生效、监听地址拒绝并回滚）。验收标准：重载期间服务不中断；非法配置被拒且旧配置继续服务；变更清单完整可审计。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 注册族 | `xrtSignalOn/Once/OnOwned/OnceOwned`；注销 `xrtSignalOff`/`xrtSignalFree` |
| 安全环境 | 回调在正常上下文——printf/日志/容器可用（受限性被模块消化） |
| 事件字段 | Name 名字 / Count 本次合并 / Total 累计；轮询消费有 count/received/clear |
| once/owned | 一次触发后自毁 / 随拥有者清理——收尾不悬空 |
| 三大入口 | SIGINT 交互退出 / SIGTERM 编排停止 / SIGHUP 配置重载 |
| 轻活纪律 | 回调置标志、主循环收尾——信号版异步边界律 |
| 掩码管理 | 忽略/恢复/批量注销——临时静音某信号再还原；收发分工：发信号在第 50 章 |
