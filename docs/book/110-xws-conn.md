---
num: 110
slug: xws-conn
title: xws（上）：连接管理与全链巡检
volume: 卷十一 其他扩展库
type: practice
lead: xwsconn 接管已握手连接、七行全链巡检、Future/协程桥、pause/resume 流控与协议化关闭——扩展库 WebSocket 的连接层。
api: xws-websocket_runtime, xws-websocket_http, net
---

## 导读

卷九第 95–97 章讲了 WebSocket 的**协议核心**（帧/消息/流）；xws 扩展库在其上补齐**工程连接层**：`xwsconn` 接管已经完成握手的 TCP 或 TLS 流——不负责 URL、DNS、HTTP 请求、证书策略或重定向（那些归第 94 章升级与卷八 TLS），只管连接上的消息事件、背压、关闭与自省。本章三块：**事件与视图纪律**（消息开始/数据/结束/Ping/Pong/Close 事件，数据视图仅回调内有效）；**流控与发送契约**（pause/resume 接收流控、高/低水位与 AGAIN 结果——继承网络流的全部词汇）；**全链巡检**（`connection_tour` 的七行自检覆盖离线构建、实连接对打、九种发送、异步族、自动 Pong、暂停恢复、干净关闭——本章的可执行契约表）。Future/协程桥与关闭协议收尾。

## 引入

xws 的分工边界值得先画清楚：**核心库**（卷九）提供帧、消息、握手、扩展协商、permessage-deflate——可组合的协议零件；**xws 扩展层**（本章起）把这些零件接到真实连接上，并补齐工程设施（连接事件、引用生命周期、Future 等待、连接组、服务端路由）。契约文档明确“HTTP 客户端/服务器适配、路由和框架型便利对象已经退出核心”——扩展层的定位是**高级能力的重建区**，不能让对象模型反向污染帧与直接收发路径。这个“核心轻、扩展厚”的分层与 xhttp（核心协议在卷九、应用层在卷十）完全同构。

连接层的第一个设计决定是**接管而非创建**：`xwsconn` 接手“已完成握手的流”——谁完成握手（第 94 章升级流程、xws 服务端路由（第 113 章））不重要，连接层只面对“已就绪的传输”。这让它可以平等地站在 TCP 与 TLS 之上（第 97 章 TLS 接管路径）。

## 概念

### 事件模型与视图纪律

连接事件：消息开始、数据片段、消息结束、Ping、Pong、Close、错误、最终关闭——比第 96 章 Stream 层多了 Ping/Pong 的**独立事件**（控制帧不只自动回，也可见）。**视图纪律**与全库一致：回调中的数据视图只在回调期间有效；跨回调保存必须复制或接管拥有型缓冲。角色规则继承核心层：服务端要求客户端帧有掩码、客户端要求服务端帧无掩码——角色在连接建立时固定，不按单帧猜测。

### 流控：pause/resume 与水位

```diagram flow
- 接收：消息事件流 → 应用消费
- 慢消费：pause 暂停接收 → 对端受 TCP 背压 → 内存恒定
- 恢复：resume 继续——不允许靠无限增长的应用队列解决慢消费者
- 发送：继承网络流高/低水位、最大排队字节与 AGAIN 结果契约
```

这是“停消费=停接收”纪律（第 62 章骨架经济学）的连接层实现。发送路径的 AGAIN 与水位词汇与第 96 章完全一致——学过 Stream 层这里零新词。

### 发送所有权三态（发送路径预告）

copy（返回前复制——短控制帧/短消息）、ref（队列释放时调一次 release——静态或共享大块）、take/buffer（移交已分配内存或网络缓冲链）——第 111 章展开 writer 形态，本章先立“未受理时所有权仍归调用方，受理后只由连接释放”的原子规则。

### Future 与协程桥

Future 只包装**已建立连接上的**发送、关闭、drain 和终态等待——不创建隐藏 HTTP 客户端。两条纪律：**取消等待≠销毁连接**——调用方按具体操作契约决定继续用、正常关闭还是 abort；协程通过通用 Future/Wait 体系等待（第 58 章）——**WebSocket 内不维护第二套调度器**。`connection_future` 示例族演示等待形态。

### 关闭协议与错误域

正常关闭顺序：发送或响应 Close → 停止新数据发送 → 等对端 Close 或截止时间 → 关闭写方向 → 等传输结束。协议破坏、超限、底层失败可直接 abort——但**必须释放所有已受理引用恰好一次**（引用所有权的终态保证）。错误模型：协议错误用稳定 WebSocket 域错误码并尽可能映射 RFC Close code；网络/TLS/应用回调错误**保留各自错误域**——不被压成一个布尔原因（第 4 章原因链在协议层的兑现）。

## 示例

### 第一个完整程序：七行全链巡检

下面的程序来自 `examples/websocket/connection_tour`——xhttp 服务器+客户端回环对接后的连接层全特性自检：

```embed path="extlibs/xws/examples/websocket/connection_tour/main.c" title="extlibs/xws/examples/websocket/connection_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/connection_tour/main.c -lws2_32 -liphlpapi
conn-tour: offline request builders ok
conn-tour: live pair upgraded, introspection ok
conn-tour: sync send x9 + writer-take delivered ok
conn-tour: async family + wait barrier ok
conn-tour: pong observed via auto-pong ok
conn-tour: pause main/callback held, resume delivered ok
conn-tour: close handshake clean on both peers ok
```

**刚才发生了什么。** 七行即七类契约。① **离线构建**：不建连接也能构造请求/应答材料（握手层的纯函数面——第 94 章）。② **实连接对**：xhttp 服务器与客户端经环回完成升级，自省（角色/协议/状态）在**连接所属 Worker 回调内**执行——同步调用的 Worker 归属纪律。③ **九种同步发送 + writer-take**：copy/ref/take × text/binary 加 writer 路径（第 111 章）全部投递成功——Take 负载的释放回调**恰好一次**（所有权终态验证）。④ **异步族 + 等待栅栏**：Future 发送与等待（取消不销毁连接）。⑤ **自动 Pong**：Ping 观察到自动应答——控制帧自动化。⑥ **暂停/恢复**：主回调暂停接收、数据保持、恢复后投递——流控语义。⑦ **干净关闭**：双方 Close 握手完成。一行一约、七行全绿——本章的契约表就是这张输出。

### 第二个完整程序：引用发送的释放契约

第二个程序来自 `examples/websocket/connection_ref`——ref 路径的所有权模板：

```embed path="extlibs/xws/examples/websocket/connection_ref/main.c" title="extlibs/xws/examples/websocket/connection_ref/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/connection_ref/main.c -lws2_32 -liphlpapi
WebSocket Connection reference example is ready
```

**刚才发生了什么。** ① `xnetref` 四元组{指针, 长度, `releasePayload` 回调, 上下文}——负载由 `xrtMalloc` 独立分配（不借用调用方缓冲）。② `xrtWsConnBinaryRef(连接, &Ref)` 引用入队——**排队期间零复制、写完 socket 后 release 恰好一次**；发送失败（未受理）时所有权归还——示例的失败分支自己 `xrtFree`（契约：未受理归调用方）。③ 本示例是“由已连接 Worker 回调调用”的函数模板（main 只验证就绪）——真实使用点在消息/路由回调里。第 96 章讲过核心库的 ref 发送；这里是连接层的同一语义——词汇完全通用。

## 契约

- **接管边界**：xwsconn 接管已握手 TCP/TLS 流；不管 URL/DNS/HTTP/证书/重定向——上下两层各归其位。
- **事件模型**：消息开始/数据/结束 + Ping/Pong/Close/错误/终态；数据视图仅回调内有效；角色建立时固定。
- **流控**：pause/resume；不允许无界应用队列；发送继承水位与 AGAIN 契约。
- **所有权原子**：copy/ref/take 三态；未受理归调用方、受理后连接释放恰好一次。
- **Future 桥**：只包已建立连接的操作；取消等待≠销毁连接；协程走通用 Future/Wait——无第二调度器。
- **关闭顺序**：Close → 停新数据 → 等对端/截止 → 关写方向 → 等传输；abort 也保证引用释放。
- **错误域**：协议错误映射 WS 域码+RFC Close code；网络/TLS/应用错误保留各自域——不压扁。
- **裁剪**：连接/所有权/异步/TLS/分组分表独立（帧层不带入网络——核心裁剪表的延续）。

## 避坑

### 坑 1：pause 之后忘了 resume（或对端超时）

症状：客户端“莫名其妙收不到消息”——服务端 pause 后消费逻辑挂了，连接半死不活到超时。

原因：pause 是“暂时不收”——必须与一个明确的恢复路径配对（异步消费者完成、下游就绪）。挂起期间对端视角就是连接卡死。

```c bad
onMessage(...) {
	if ( busy() ) {
		xrtWsConnPause(pConn);   /* 暂停后无人恢复 */
	}
}
```

```c good
onMessage(...) {
	if ( busy() ) {
		xrtWsConnPause(pConn);
		defer_resume_on_ready(pConn);   /* 明确的恢复触发点 */
	}
}
```

### 坑 2：把 Future 取消当连接关闭

症状：等发送 Future 超时后直接 Destroy 连接——对端正在收的半条消息被腰斩，还可能双释放。

原因：取消等待只是“不等了”——连接和已受理数据的所有权完好。按操作契约决定：继续用（等下一个 Future）、正常 Close、或 abort。

```c bad
if ( wait_timeout(pSendFuture) ) {
	xrtWsConnDestroy(pConn);   /* 取消≠销毁：可能双释放/半消息 */
}
```

```c good
if ( wait_timeout(pSendFuture) ) {
	/* 数据是否已受理？连接状态查得到——再决定关闭方式 */
	if ( !xrtWsConnClose(pConn, Code, Reason) ) {
		xrtWsConnAbort(pConn);   /* 关闭不行才中止 */
	}
}
```

### 坑 3：在非 Worker 线程调同步发送

症状：偶发状态错乱——同步发送设计在连接所属 Worker 上执行（巡检第 2 行的注释明说）。

原因：连接对象非多线程共享的可变状态——跨线程操作走 Future/异步族（它们内部正确投递）。这与第 86 章会话层、第 104 章服务端事件的 Worker 串行纪律同源。

```c bad
/* 任意线程直接调同步发送 */
xrtWsConnText(pConn, Msg);   /* 非所属 Worker：竞态 */
```

```c good
/* 跨线程：走 Future/异步族（内部投递到连接 Worker） */
/* 形态见 websocket_http_future 模块：ConnectAsync 一族的等待语义 */
```

## 练习

### 基础：巡检逐行标注

跑通 connection_tour，给七行输出各写一句“验证了哪条契约”。验收标准：七条契约全部能指到本章契约区的对应条目。

### 进阶：暂停-恢复压力对

基于巡检的回环骨架：消费端随机 pause 0-50ms 后 resume，发送端全速发 1000 条——统计投递与丢失。验收标准：零丢失（暂停只是延迟）；内存恒定（第 6 章统计）；对端视角只是吞吐波动。

### 挑战：双通道代理

连接 A 收到的消息转发到连接 B（反向亦然）——双连接的 pause 联动：B 忙时 pause A。验收标准：转发零丢失；B 慢时 A 的接收同步暂停（内存恒定）；任一端关闭时另一端干净收尾（Close 传播）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 分工边界 | 核心库（95-97 章协议零件）+ xws（连接/组/路由工程层） |
| 接管形态 | 已握手 TCP/TLS 流；不管 URL/DNS/HTTP/证书 |
| 事件模型 | 消息三段 + Ping/Pong/Close/错误/终态；视图仅回调内有效 |
| 流控 | pause/resume；无界队列禁止；AGAIN 结果+水位同全库 |
| 所有权三态 | copy/ref/take；未受理归调用方；受理后释放恰好一次 |
| Future 桥 | 只包已建立连接操作；取消≠销毁；协程走通用体系 |
| 关闭顺序 | Close→停发→等对端→关写→等传输；abort 保引用释放 |
| 错误域 | WS 域码+RFC Close code；网络/TLS/应用各留各域 |
| 巡检 | connection_tour 七行=七类契约的可执行表 |
| 裁剪 | 连接/所有权/异步/TLS/分组独立分表 |
