---
num: 67
slug: net-tcp
title: TCP 流（上）：连接与读写
volume: 卷七 网络
type: practice
lead: Engine、Listener 与 Stream 三件套：从一次回环回显开始，掌握 TCP 的状态机与关闭语义。
api: tcp, tcp_server, net
---

## 导读

第 63～66 章解决了"地址从哪来"——解析、端口、缓冲与 DNS。本章开始真正传输数据：建立 Engine、监听端口、发起连接、收发字节，最后干净地关闭一切。TCP 模块是后面所有内容的地基——第 68 章的背压与服务端形态、卷八的 TLS、卷九的 HTTP，全部构建在 `xnetstream` 这一个对象之上。本章先掌握两个面：阻塞的同步面（工具程序的最少代码路径）与事件回调面（生产服务的骨架），并把"状态机"和"关闭时序"这两件最容易出错的事彻底讲清楚。

## 引入

想象你要写一个数据迁移小工具：从本机一个服务读一批数据，转发到另一个服务。这类工具对网络的需求朴素而完整——连得上、发得出、收得全、关得干净。用原始 socket 写过的人都知道，真正费事的从来不是那五个系统调用，而是收尾：连接卡在半开状态怎么办？缓冲里还有数据没发完能直接关吗？什么时候才能安全释放对象？

XRT 的 TCP 层把这些坑内建成了对象契约：流有明确的状态机，`CLOSED` 是唯一终态，看到它之前不释放引用；关闭分"排空"与"立即中止"两种语义；Engine 统一驱动所有连接的事件循环。本章用两个完整程序把这套模型走一遍。

## 概念

### 三个对象，一台发动机

- **`xnetengine`**：网络引擎。管理平台事件后端（IOCP / epoll / kqueue / io_uring）与一组 Worker 线程，所有 Listener 和 Stream 都挂在某个 Engine 上工作。配置里 `Workers` 指定线程数。Worker 数量的经验值：工具程序与单元测试 1～2 个即可——Engine 的线程只做事件分发与回调，不承载业务计算（要把小件任务直接排上 Worker 循环，用第 61 章的网络任务组 task_net）；服务端的容量规划（Worker 数、队列深度、背压水位）是第 68 章的主题。多个 Engine 可以共存，但本章与后续章节的惯例是一个进程一个 Engine。
- **`xnetlistener`**：监听器。绑定地址（端口填 0 表示让系统分配），接受接入连接。
- **`xnetstream`**：流。一条 TCP 连接的读写面，客户端与服务端各自持有一条。

Stream 与 Listener 的状态都只向前推进：

```diagram state
CONNECTING -> OPEN: 连接建立完成
OPEN -> CLOSING: Close / 对端关闭
CLOSING -> CLOSED: 收尾完成
```

```diagram state
OPEN -> CLOSING: Close 请求
CLOSING -> CLOSED: 在途操作终结
```

`CLOSED` 是唯一终态，而且它在发布前就保证：Socket、缓冲、在途操作和对 Engine 的活动占用都已终结。换句话说，**看到 `CLOSED` 你才可以释放调用方引用**。

### 同一面孔，两个调用面

`xnetstream` 同时提供两套调用约定：

- **同步面**：`xrtNetListenerAcceptWait` 阻塞等连接，`xrtNetStreamRecv` 阻塞收数据，返回拥有式的 `xnetbytes` 字节结果。工具类程序（探活、一次性迁移、简单客户端）用同步面，代码最短。
- **事件面**：注册回调表（`Accept`/`Read`/`Close` 等函数指针），Engine 的 Worker 线程在事件到来时调用。生产服务用事件面获得高并发与硬背压。

两套面共享同一批对象与状态机，本章各演示一个完整程序。

### 关闭的两种语义与引用计数

- `xrtNetStreamClose`：请求排空关闭——把在途数据发完、走完协议收尾再进入 `CLOSED`。
- `xrtNetStreamAbort`：立即中止——丢弃在途数据，尽快进入 `CLOSED`。错误路径与 `Cleanup` 段用它防止卡死。

创建函数返回的对象自带**一个调用方引用**；运行时另持一个内部引用，直到唯一 `Close` 回调结束。所以正确的退出顺序是：先请求关闭（Close 或 Abort）→ 等状态到 `CLOSED` → `xrtNetStreamDestroy` 释放调用方引用。`Ref`/`Destroy` 可以跨线程使用，但 `Ref` 不能从零复活已终结的对象。

## 示例

### 第一个完整程序：同步面回显

下面的程序来自仓库范例 `examples/network/tcp_sync/main.c`：起一个回环监听，连上去发一句 `hello`，服务端收到并打印。这是"最少代码走完整生命周期"的标准样本：

```embed path="examples/network/tcp_sync/main.c" title="examples/network/tcp_sync/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/tcp_sync/main.c -lws2_32 -liphlpapi
received: hello
```

**刚才发生了什么。** 六个节点值得注意。① `Workers = 2`——工具程序给两个 Worker 就够，Engine 本身不做业务。② 监听地址用 `xrtNetAddrLoopback` 生成回环地址，**端口填 0**，随后 `xrtNetListenerLocal` 取回系统分配的真实端口——这是并发测试不互相冲突的标准姿势。③ 客户端 `xrtNetStreamConnect` 的第 3 个参数 `1` 是地址数量；同步面下连接由 Engine 在后台完成。④ `xrtNetListenerAcceptWait` 带一个 `xrtDeadlineAfter` 截止时间，到点没连接就返回 `NULL` 并在线程错误槽留下超时类别（第 4 章的模型在这里兑现）。⑤ `xrtNetStreamRecv` 返回**拥有式**的 `xnetbytes`，用 `xrtNetBytesView` 借出视图读取，读完 `xrtNetBytesDestroy` 释放。⑥ 收尾段先 `Abort` 两条流与监听器，**自旋等三者都到 `CLOSED`**，再逐个 `Destroy`，最后 `xrtNetEngineDestroy`。

### 事件面完整形态：回调驱动的回显服务

第二个程序来自 `examples/network/tcp/main.c`，是生产 TCP 服务的标准骨架——事件表驱动读写、服务端直移接收缓冲回显（零拷贝）、对端关闭后排空并正常关闭：

```embed path="examples/network/tcp/main.c" title="examples/network/tcp/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/tcp/main.c -lws2_32 -liphlpapi
listening on 127.0.0.1:52173
reply: hello TCP
```

**事件面的三个要点。** ① `Accept` 回调接管每一条新连接：调用 `xrtNetStreamSetData` 把业务上下文挂到流上，之后所有回调都能取回，不必自己维护"流→上下文"映射。② 服务端 `Read` 回调里直接 `xrtNetStreamSendBuffer` 把接收缓冲**引用移交**回去——数据从接收缓冲到发送队列零拷贝，这是回显类服务的正确写法；客户端 `Read` 则演示普通读取（`xrtNetBufRead` 拷贝到栈缓冲）。③ `Close` 回调是唯一终态通知：参数里的 `xnetresult` 与错误对象区分"正常关闭"与"出错关闭"，示例只对干净关闭计数。回调运行在 Engine 的 Worker 线程上，主线程用原子计数与截止时间观察进度——第 68 章会把这里的等待升级成正经的同步原语与背压控制。

## 契约

- **状态**：Stream 与 Listener 状态只向前推进；`CLOSED` 唯一终态；进入 `CLOSED` 前 Socket、缓冲与在途操作都已终结。
- **引用**：创建返回调用方引用；运行时持内部引用直到 `Close` 回调结束；先请求关闭、等 `CLOSED`、再 `Destroy`；`Ref` 可跨线程、不可复活。
- **关闭**：`Close` 排空，`Abort` 立即；错误路径一律 `Abort`，正常路径让数据走完。
- **线程**：事件回调在 Engine Worker 上执行；`Close` 回调可能仍在 Worker 上收尾，`xrtNetEngineDestroy` 会等待 Worker 正常退出。
- **错误**：失败经返回值宣告（`NULL` / 非 `XNET_RESULT_OK`），详情在线程错误槽，类别与域沿用第 4 章模型。

## 避坑

### 坑 1：不等 CLOSED 就销毁

症状：偶发崩溃或断言，尤其是服务端高并发退出时；有时表现为 `xrtNetEngineDestroy` 卡住。

原因：`CLOSED` 发布前对象仍被在途操作与 Worker 使用，提前 `Destroy` 释放的是仍被引用的内存。

```c bad
xrtNetStreamClose(pStream);
xrtNetStreamDestroy(pStream);   /* 状态可能还在 CLOSING */
```

```c good
xrtNetStreamClose(pStream);
while ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
	xrtThreadYield();
}
xrtNetStreamDestroy(pStream);
```

### 坑 2：Recv 结果忘记释放

症状：长时间运行的程序内存持续增长；分配统计里 `xnetbytes` 只增不减。

原因：`xrtNetStreamRecv` 返回的是**拥有式**字节结果，与第 15 章缓冲不同，它的生命周期完全归调用方。

```c bad
xnetbytes* pBytes = xrtNetStreamRecv(pStream, 0, deadline, NULL);
printf("%.*s\n", (int)xrtNetBytesView(pBytes).Size,
	(cstr)xrtNetBytesView(pBytes).Data);
/* 少了 Destroy：每收一次泄漏一个结果对象 */
```

```c good
xnetbytes* pBytes = xrtNetStreamRecv(pStream, 0, deadline, NULL);
if ( pBytes != NULL ) {
	printf("%.*s\n", (int)xrtNetBytesView(pBytes).Size,
		(cstr)xrtNetBytesView(pBytes).Data);
	xrtNetBytesDestroy(pBytes);
}
```

### 坑 3：写死监听端口

症状：并行跑测试或同事同时启动服务时 bind 失败，或更糟——连到了别人进程的端口上。

原因：固定端口把"哪个进程用哪个端口"变成了全局协调问题。

正确做法：监听地址端口填 `0` 让系统分配，随后 `xrtNetListenerLocal` 取回真实端点再交给客户端——两个范例用的都是这个姿势。

## 练习

### 基础：改回显内容

把同步面程序改成"收到什么就回大写什么"：服务端收到后转换为大写再发回，客户端打印结果。只改 `main.c` 里的收发逻辑，生命周期代码保持不动。

### 进阶：给接收加超时类别判断

用更短的截止时间调用 `xrtNetListenerAcceptWait`，在超时分支用 `xrtErrorIs(xrtGetError(), XERR_TIMEOUT)` 判断并打印 `accept timeout`。提示：先确认函数宣告失败（返回 `NULL`）再读线程错误槽——这是第 4 章"坑 3"的实操。

### 挑战：写一个端口探测工具

对 `127.0.0.1` 的一段端口（如 51000–51050）逐个 `xrtNetStreamConnect`，输出开放端口列表；区分"拒绝连接"与"等待超时"两种失败（用错误类别判断）。验收标准：先启动本章的回显服务，探测工具能准确列出其端口；对明确拒绝与被防火墙吞掉的端口给出不同的标注；全部探测完成后 Engine 正常销毁、无泄漏。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 对象三件套 | Engine（线程池+事件后端）/ Listener（监听）/ Stream（连接） |
| Stream 状态 | `CONNECTING → OPEN → CLOSING → CLOSED`，只进不退 |
| 退出顺序 | 请求关闭（Close/Abort）→ 等 `CLOSED` → `Destroy` → 最后销毁 Engine |
| 同步面 | `AcceptWait` / `Send` / `Recv`（拥有式 `xnetbytes`，用完 Destroy） |
| 事件面 | 回调表 + `SetData` 挂上下文；`SendBuffer` 零拷贝直移；`Close` 是终态通知 |
| 端口分配 | 监听端口 0 + `ListenerLocal` 取真实端点，并发不冲突 |
| 错误 | 返回值宣告失败；`XNET_RESULT_OK` 之外查线程错误槽 |
