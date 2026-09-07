---
num: 95
slug: ws-stream
title: WebSocket Stream：接管与事件流
volume: 卷九 Web 协议核心
type: practice
lead: Attach 接管升级后的传输、MessageBegin/Data/End 事件、引用发送与统一背压——帧层之上的连接对象。
api: websocket, websocket_stream, net
---

## 导读

第 94 章的帧与消息是**状态机**（无传输的库层）；本章的 Stream（`xwsstream`）把它们接上真实传输：`xrtWsStreamAttach`/`AttachTls` 接管**已经完成升级**的 TCP/TLS 调用方引用（含"缓冲里还剩帧数据"的精确衔接），事件按 `MessageBegin → MessageData×N → MessageEnd` 发布——正文视图只在当前回调有效，Stream **不聚合完整消息、不为每连接预分配固定缓冲**，TCP/TLS 的现有块链直接进帧状态机。发送侧提供 copy 与 **ref 引用**两种路径（引用在真正写完 socket 后才释放——组播场景同一引用挂 N 连接计数释放）；`SendLimit` 是 WebSocket 与底层传输待发量的**统一硬边界**，`Backpressure/Writable/Drain` 给恢复边沿。Ping 自动回复、Close 唯一发送保证——连接对象的全部生命周期。

## 引入

第 94 章末的练习让你手搭帧循环——你会发现 80% 的代码在处理"传输接线"：升级后的缓冲里可能已经带着帧数据、TLS 帧头可能跨 record、写背压要同时看两层、Ping 要自动回、Close 只能发一次……这些每个实现都要重写的管线，Stream 层收口成 `Attach` 一次调用 + 四个事件回调。

最值得先懂的是**接管衔接**：升级应答刚发完，接收缓冲里可能已经有客户端抢发的第一帧（浏览器经常这样）。`iPrefix` 参数告诉 Stream"已验证但尚未消费的 HTTP 头长度"——接管时先复制协商出的子协议、精确消费头部、**同一缓冲中的帧余量直接进帧状态机**，一个字节都不浪费、不错位。失败不会部分接管（传输引用完好归还）。

## 概念

### 接管：Attach 的精确衔接

```diagram flow
- 前置：升级完成（第 93 章），持有 TCP/TLS 流的调用方引用
- Attach(传输, 配置, 事件, iPrefix=已验证头长)：复制子协议 → 精确消费头 → 帧余量直接进状态机
- 事件：MessageBegin（类型/元信息）→ MessageData×N（借用视图）→ MessageEnd
- 发送：Text/Binary/Send（copy）或 TextRef/SendRef（引用）；到达 SendLimit 返回 AGAIN
- 终态：Close（唯一发送+超时+对端快照）或 Abort
```

TLS 的特有处理：帧头可能跨 TLS record 边界——Stream 通过受 `PlainLimit` 限制的 `ReadMore` 路径累积最多 14 字节的完整帧头（最大帧头尺寸），"不因保留一个不完整前缀而停住"——第 86 章 TLS 流与本章帧状态机的衔接细节已经调好。

### 事件模型：视图即用即弃

`MessageData` 的正文视图**只在当前同步回调内有效**——Stream 不聚合完整消息（大消息不占内存）、不拷贝（块链直通）。你的消费逻辑必须"回调内完成"（解析、转发、写盘）；需要保留就自己复制。这与第 86 章 TLS 流的明文消费、第 90 章 Body 的 Data 视图同一纪律——**借用视图的世界里，保留是显式动作**。

### 发送三形态与统一背压

- **copy**（`Text/Binary/Send`）：负载复制入队——小消息的简单路径。
- **ref**（`TextRef/BinaryTake/SendRef` 等）：传 `xnetref`{数据, 长度, 释放回调, 上下文}——**队列排队期间零复制，真正写完 socket 后才调释放回调**。组播场景同一引用挂 N 个连接（计数释放）——一处分配、N 处发送。
- **压缩发送**（`TextCompressed` 等）：按需创建 Deflater，permessage-deflate 协商后的路径（第 96 章讲协商）。

背压是**统一水位**：`SendLimit` 同时计 WebSocket 待发与底层传输待发——达到上限 `XNET_RESULT_AGAIN`；`Backpressure`/`Writable`/`Drain` 事件给出恢复边沿（进入背压、可写恢复、排空）。与第 67 章 TCP 写预算、第 86 章 TLS SendLimit 一脉——每层一个水位、语义相同。

### 控制帧与关闭协议

Ping **自动回复** Pong（应用无感）；`Ping` 可主动发起。Close 的保证：**唯一发送**（重复 Close 不会上线）、超时机制（对端不回应答按超时处理）、对端终态快照（CloseInfo：码+原因，供应用读）。关闭码的语义域与写侧校验是第 96 章组合章的内容——本章记住"Close 调用一次，状态机管协议"。

### 生命周期与自省

`Ref/Destroy` 引用计数（与其他连接对象一致）；自省族：`State/Role/Protocol/Worker/Tcp/TcpRef/Tls/TlsRef/Deflate/Pending/Writable`——连接的全部公开事实；`Pause/Paused/Resume` 流控（暂停接收，配合下游限速）。

## 示例

### 第一个完整程序：引用发送——写完才释放

下面的程序来自 `examples/websocket/stream_ref/main.c`——零拷贝发送路径的标准样本：

```embed path="examples/websocket/stream_ref/main.c" title="examples/websocket/stream_ref/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/stream_ref/main.c -lws2_32 -liphlpapi
WebSocket Connection reference example is ready
```

**刚才发生了什么。** ① 业务负载由 `xrtMalloc` 分配（模拟来自上游的消息）；`xnetref` 把 {指针, 长度, `releasePayload` 回调, 上下文} 打包。② `xrtWsStreamBinaryRef(连接, 负载, Ref)` 以引用入队——**负载在 Worker 队列里排队期间不复制**；发送真正完成（写进 socket）后 `releasePayload` 才被调用、`xrtFree` 归还。③ 为什么这很重要：高吞吐场景 copy 路径每次发送都是一次分配+复制；ref 路径一次分配、零中转复制。**组播升级**（头注释指路）：同一引用挂到 N 个连接——内部计数释放，最后一个写完才真正 free。④ `Pending`/`Writable` 配合等待排空——统一背压的读法。

配套的 `examples/websocket/stream_tour` 是 Stream 层全特性巡检（回环双端）：配置校验、双端 Attach、64 字节回显、自动与手动 Pong、暂停恢复、压缩三形态、干净关闭（code=1000）——把它当本章的**可执行契约表**跑一遍。

### 第二个完整程序：扩展协商预览

第二个程序来自 `examples/websocket/extension/main.c`——升级后的扩展参数遍历，为第 96 章铺垫：

```embed path="examples/websocket/extension/main.c" title="examples/websocket/extension/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/extension/main.c -lws2_32 -liphlpapi
extension=permessage-deflate
  parameter=client_max_window_bits
extension=x-trace
```

**刚才发生了什么。** ① `Sec-WebSocket-Extensions` 的两层结构：外层按 `,` 切扩展、内层按 `;` 切参数——与第 91 章字段族语法同构，但扩展层有自己的迭代器（`x-trace` 这样的自定义扩展也用 `name` + 参数表达）。② `client_max_window_bits` 是 permessage-deflate 的协商参数之一——第 96 章完整讲协商与压缩链路；这里先建立"扩展是升级后连接的属性、协商决定 Stream 行为（压缩路径、RSV 位）"的认知。③ 扩展迭代是纯字段层操作——调试代理、合规检查器不用建连接就能审计协商结果。

## 契约

- **接管原子性**：Attach 失败不部分接管（传输引用完好）；`iPrefix` 精确消费已验证头部，同缓冲帧余量直通状态机。
- **事件视图**：`MessageData` 正文仅在当前回调有效；不聚合完整消息、无每连接固定缓冲——块链直通帧状态机。
- **TLS 跨 record**：帧头跨 TLS record 由 `ReadMore` 累积（≤14 字节、受 PlainLimit 限）——不停不丢。
- **发送三形态**：copy（复制入队）/ ref（引用+延迟释放回调）/ compressed（按需 Deflater）；容量与预算语义同全库。
- **统一背压**：`SendLimit` 同计 WS 与传输待发；AGAIN + Backpressure/Writable/Drain 边沿事件。
- **引用发送**：`xnetref`{数据,长度,释放,上下文}；写完 socket 才释放；同引用可挂 N 连接计数释放。
- **Ping/Pong**：收到 Ping 自动回 Pong；主动 Ping 可发；流控 Pause/Paused/Resume。
- **Close 保证**：唯一发送、超时机制、对端终态快照（CloseInfo）；Abort 立即中止。
- **自省族**：State/Role/Protocol/Worker/Tcp(+Ref)/Tls(+Ref)/Deflate/Pending/Writable。
- **裁剪**：`WEBSOCKET_STREAM` 独立于帧层；`_REF`/`_TLS`/`_DEFLATE` 子特性按需。

## 避坑

### 坑 1：把 MessageData 的视图存起来异步处理

症状：异步队列里处理到一半崩溃或读到垃圾——视图在回调返回后失效。

原因：Stream 不聚合不复制——视图指向接收块链的当前位置，下个事件就可能覆盖。异步处理必须**在回调内复制数据**，队列里放副本。

```c bad
onMessageData(pStream, Data, ...) {
	queue_push(g_Queue, Data);   /* 存视图：返回即悬空 */
}
```

```c good
onMessageData(pStream, Data, ...) {
	bytes pCopy = (bytes)xrtMalloc(Data.Size);
	memcpy(pCopy, Data.Data, Data.Size);
	queue_push(g_Queue, pCopy);   /* 副本可异步 */
}
/* 更优：同步完成消费（解析/转发），不引入队列 */
```

### 坑 2：ref 发送后提前释放负载

症状：偶发对端收到错乱帧——负载在队列还没写进 socket 就被 free 了。

原因：ref 路径的契约是"释放回调由 Stream 调"——调用方入队后**不再拥有**负载。提前 free 或重复 free 都是对所有权的破坏。

```c bad
xrtWsStreamBinaryRef(pStream, PayloadView, &Ref);
xrtFree(pBuffer);   /* 入队就释放：队列里的引用悬空 */
```

```c good
/* 所有权随引用移交；releasePayload 回调里释放 */
xrtWsStreamBinaryRef(pStream, PayloadView, &Ref);
/* 此后 pBuffer 归 Stream 管——不再碰它 */
```

### 坑 3：把 SendLimit 当 WS 层水位单独看

症状：自己统计"WS 待发量"判断背压，与实际 AGAIN 时机对不上——TLS/TCP 层的待发也占预算。

原因：`SendLimit` 是**统一水位**（WS + 底层传输）。分层统计会漏掉传输侧的排队；直接用 `Pending`/`Writable` 事件就是全部事实。

```c bad
if ( my_ws_queue_size() < MY_LIMIT ) {
	send_more();   /* 传输层可能早已超限 */
}
```

```c good
if ( xrtWsStreamSend(pStream, ...) == XNET_RESULT_AGAIN ) {
	/* 等 Writable/Drain 事件——统一水位的恢复边沿 */
	return;
}
```

## 练习

### 基础：巡检跑通

跑通 stream_tour 与 stream_ref；对照 stream_tour 头注释逐项勾选验证过的行为（回显/Pong/暂停/压缩/关闭）。验收标准：能指出每项行为对应的事件或 API。

### 进阶：回显服务的双形态

基于 Stream 事件实现回显：copy 形态（MessageEnd 后 Text 回发）与 ref 形态（MessageData 的视图复制成 ref 回发）各写一版。压测对比两者的分配计数（第 6 章）。验收标准：语义一致；ref 版每消息分配次数显著更少；背压下（慢客户端）两版都稳定。

### 挑战：广播器

实现 `broadcast(连接数组, 消息)`：一条消息引用发送到 N 个连接——单一 `xrtMalloc` 负载、N 个 `xnetref` 计数释放；处理部分连接背压（AGAIN 的连接跳过或缓存重试，策略自选并注释理由）。验收标准：8 连接广播零额外复制（统计验证）；背压连接不影响其他连接的发送；全部写完后负载恰好释放一次。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 接管 | Attach/AttachTls 接管升级后的传输；iPrefix 精确衔接缓冲余量；失败不部分接管 |
| 事件流 | MessageBegin → MessageData×N（视图仅回调内有效）→ MessageEnd；不聚合零预分配 |
| TLS 衔接 | 帧头跨 record 由 ReadMore 累积（≤14B、PlainLimit 限） |
| 发送三形态 | copy（复制）/ ref（引用延迟释放）/ compressed（按需 Deflater） |
| 统一背压 | SendLimit 同计 WS+传输；AGAIN + Backpressure/Writable/Drain |
| 引用发送 | xnetref{数据,长度,释放,上下文}；写完才释放；可 N 连接计数 |
| 控制帧 | Ping 自动回 Pong；Close 唯一发送+超时+对端快照 |
| 流控与自省 | Pause/Resume；State/Role/Protocol/Pending/Writable 族 |
| 扩展属性 | 扩展协商结果可遍历（第 96 章 permessage-deflate） |
| 裁剪 | STREAM 独立于帧层；REF/TLS/DEFLATE 子特性 |
