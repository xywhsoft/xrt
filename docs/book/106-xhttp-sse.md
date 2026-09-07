---
num: 106
slug: xhttp-sse
title: SSE：服务器推送
volume: 卷十 扩展库：xhttp
type: practice
lead: 不建专用状态机的 EventSource：SSE Reply 借道唯一响应状态机、结构化事件封包、客户端解析器与 Last-Event-ID 续传。
api: xhttp-http_sse, xhttp-http_server, xhttp-http_client
---

## 导读

服务器到客户端的单向推送，Web 世界有两个选择：WebSocket（第 94–96 章，双向、独立协议）与 **SSE**（Server-Sent Events——HTTP 响应流上的事件协议）。xhttp 的实现哲学一句话：**不建 SSE 专用连接、线程、队列或网络状态机**——`xrtHttpSseReplyCreate` 创建的是一个状态 200、`Content-Type: text/event-stream`、未知长度一次性 Body 的普通 `xhttpreply`，实际发送由 HTTP Server 的**唯一**响应状态机、TCP/TLS 背压与写时限驱动；返回的 `xhttpbodystream` 是独立生产端，交给后台任务或任意线程写入。客户端侧是同镜像：连接层 + 事件解析器（`text/event-stream` 的增量语法机），`Last-Event-ID` 断线续传内建。选择法则：只要单向推送就用 SSE——协议简单、走 80 端口、自动重连是浏览器内建。

## 引入

进度推送、行情流、日志尾随、AI 生成逐 token 输出——"服务端持续说话、客户端只听"的场景占推送需求的大半。为这类需求上 WebSocket 是高射炮打蚊子：双向协议的单向使用，多了握手升级、心跳、关闭协议全套负担。SSE 的答案是"别换协议"：还是 HTTP 响应，只是**不结束**——`Content-Type: text/event-stream` 的 chunked 流上，一行行 `event:/data:/id:/retry:` 字段组成事件、空行分隔。

工程难点在两侧。服务端：响应流要能与后台生产者协作——事件从任务线程来、连接在 Worker 上写、背压怎么传？客户端：事件语法是**增量**的（一个 data 字段可拆多个 data: 行、事件跨 chunk 边界）——解析器要流式。xhttp 两端都给了结构化答案：服务端 `SseSendEvent`（一次一条完整事件、先验证后编码、失败不提交半条）+ `xhttpbodystream` 生产端（AGAIN 背压 + `WaitWritable` 恢复）；客户端解析器按限额约束逐字段推进。

## 概念

### 服务端：借道唯一状态机

```diagram flow
- 创建：SseReplyCreate(配置, &流) → 普通 Reply（200/text-event-stream/未知长 Body）
- 定制：提交前用普通 Header API 加 Cache-Control/CORS/X-Accel-Buffering（部署层职责）
- 提交：HttpConnRespond 后 Server 冻结并保留正文来源——Reply 可销毁
- 生产：流交给任务线程 → SseSendEvent/SendComment/Write 族写入
- 背压：AGAIN 不保留输入引用 → 等待 WaitWritable 重试
- 收尾：末引用销毁=排队事件后正常 EOF；Close 幂等预关；Fail 丢弃未交付+稳定 Cause
```

三条设计纪律。① **协议必需与应用自决分界**：`ReplyCreate` 只设 Content-Type；`Cache-Control: no-cache`、认证、CORS、代理缓冲策略（`X-Accel-Buffering: no`）由应用在提交前用普通 Header API 设置——SSE 层不替你猜部署环境。② **原子性**：每次 Send 先完整验证并计量、再直接编码进一个 Body Stream 节点——**不建临时事件字符串、失败不提交半条事件**；字节与 Chunk 硬预算覆盖并发预留、排队与活动租约。③ **生命周期解耦**：Respond 提交后 Reply 可销毁，Stream 独立存活——"响应对象"与"生产端"是两个生命周期。

### 事件结构与封包

`xhttpsseevent` 四要素：`Type`（event: 行——事件类型名）、`Data`（data: 行——可多行）、`Id`（id: 行——续传游标）、`Retry`（retry: 行——建议客户端重连毫秒数）；`Flags` 声明哪些要素存在。`SseSendEvent` 一个调用封包完整事件；`SseSendComment`（`: heartbeat` 注释行——保活）；`xrtHttpBodyStreamWrite/WriteRef/WriteTake` 直写字节（预编码事件、代理转发、自定义扩展——结构化 Writer 不是强制路径）。

### 背压与等待

生产端写入遇预算满返回 `AGAIN`——**没有保留任何输入引用**，调用方原样重试；`xrtHttpBodyStreamWaitWritable` 返回共享 Future 表示下一代可写性——**单个等待者取消会影响其他等待者**，因此不应直接取消（第 57 章 Future 的共享语义）。这套"AGAIN+WaitWritable"与第 95 章 WebSocket Stream 同款词汇。

### 客户端：连接与解析器

客户端侧两层：**连接层**（`http_sse_client`——发起 GET、维护重连与 Last-Event-ID 头）与**解析器**（`text/event-stream` 增量语法机——字段行、事件边界、多行 data 拼接）。解析器配置约束单行、事件数据、类型与 ID 的限额——**结构性内存上限**（第 98 章"限什么由内容结构决定"的 SSE 版：表示正文选无界、结构化内存有界）。断线续传：服务端发的 `id:` 被客户端记录，重连时以 `Last-Event-ID` 头带回——服务端从游标后续传。

### SSE vs WebSocket 的选型

| 维度 | SSE | WebSocket（第 94–96 章） |
| --- | --- | --- |
| 方向 | 单向（服务端→客户端） | 双向 |
| 协议 | 纯 HTTP 响应流 | 升级后的独立协议 |
| 基础设施 | 任何 HTTP 中间件透明 | 需 Upgrade 支持 |
| 重连 | 浏览器/客户端内建+游标续传 | 应用自建 |
| 适用 | 推送/流式输出/进度 | 聊天/协作/双向遥测 |

## 示例

### 第一个完整程序：事件、心跳与直写三路

下面的程序来自 `examples/http/sse_server`——结构化事件发送的完整面：

```embed path="extlibs/xhttp/examples/http/sse_server/main.c" title="extlibs/xhttp/examples/http/sse_server/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/sse_server/main.c -lws2_32 -liphlpapi
（读回自检通过后正常退出）
```

**刚才发生了什么。** ① `BodyStreamConfigInit` 设 MaxBytes/MaxChunks（生产端预算——AGAIN 的阈值来源）→ `SseReplyCreate` 得到 Reply 与 Stream 双对象。② 三种写入形态齐发：`SseSendEvent`（四要素全开的 progress 事件——type/data/id/retry 一次封包）、`SseSendComment`（heartbeat 注释行——保活不产生事件）、`BodyStreamWrite`（预封装的 `data: ready\n\n` 字节直写——代理/静态事件的路径）。③ **读回验证**（示例的 `exampleReadReply`）：用第 107 章的 Body 读取器把 Reply 的正文流式读出、逐块打印——**服务端封包的字节就是客户端解析器要吃的语法**，一个示例完成两端对拍。④ 双对象生命周期：Stream 用完 Destroy（EOF 发布）、Reply 随后 Destroy——与"提交后 Reply 可销毁、Stream 独立"的契约一致（本例未提交给连接，直接本地读回）。

### 第二个完整程序：客户端流读取的等待形态

第二个程序来自 `examples/http/sse_client`——客户端消费侧的标准循环：

```embed path="extlibs/xhttp/examples/http/sse_client/main.c" title="extlibs/xhttp/examples/http/sse_client/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/sse_client/main.c -lws2_32 -liphlpapi
（示例校验客户端事件消费路径后正常退出）
```

**刚才发生了什么。** ① Body 读取循环的三态：`XHTTP_BODY_DATA`（取块消费+`ChunkRelease`）、`XHTTP_BODY_AGAIN`（暂无数据——`xrtHttpBodyReaderWait` 拿 Future 等待到达）、`EOF`（流正常结束）。**AGAIN+Wait** 的组合就是流式消费的通用骨架（与服务端 AGAIN+WaitWritable 对称）。② SSE 客户端的连接层在此之上维护：断线按 Retry 建议重连、`Last-Event-ID` 游标带回；事件解析器把字节流转成结构化事件回调。③ 配套 `examples/http/sse_http`（无客户端层的裸 HTTP 形态——响应流直接消费）覆盖"只要流不要重连"的场景。

## 契约

- **无专用状态机**：SSE Reply 是普通 Reply（200/text-event-stream/未知长）；发送走唯一响应状态机与既有背压/写时限。
- **协议/应用分界**：ReplyCreate 只设 Content-Type；Cache-Control/CORS/缓冲策略提交前自设。
- **生命周期**：Respond 后 Reply 可销毁、Stream 独立（任务线程/发布器持有）；末引用销毁=排队事件后 EOF；Close 幂等；Fail 丢弃未交付+稳定 Cause。
- **写入原子**：先验证计量再编码一个节点；不建临时事件串；失败不提交半条；字节/Chunk 预算覆盖预留+排队+租约。
- **背压**：AGAIN 不保留输入；WaitWritable 共享 Future（不可单方取消）；Write/WriteRef/WriteTake 直写字节可选。
- **事件四要素**：event/data/id/retry + Flags；注释行保活；id 即续传游标。
- **客户端解析**：增量语法机；单行/数据/类型/ID 四限额——结构化内存有界；Last-Event-ID 续传。
- **消费循环**：DATA（块+Release）/ AGAIN（Wait Future）/ EOF 三态骨架。
- **裁剪**：`HTTP_SSE` 服务端/客户端独立；解析器可单独用于自定义传输。

## 避坑

### 坑 1：SSE 事件里塞裸换行的 data

症状：客户端收到的"一个事件"被拆成多个或解析错乱——data 里的换行改变了事件边界。

原因：SSE 用空行分隔事件、data 的多行靠多个 `data:` 行表达——`SseSendEvent` 封包时会正确处理多行 data（拆成多个 data: 行），但绕过结构化入口直拼字符串时容易把换行裸写。

```c bad
xrtHttpBodyStreamWrite(pStream, XRT_BYTES_LITERAL(
	"data: line1\nline2\n\n"));   /* line2 没有 data: 前缀：语法错 */
```

```c good
xhttpsseevent Event = { 0 };
Event.Data = XRT_STR_LITERAL("line1\nline2");   /* 封包器拆多行 data: */
Event.Flags = XHTTP_SSE_EVENT_DATA;
xrtHttpSseSendEvent(pStream, &Event);
```

### 坑 2：忽略 AGAIN 直接重试（忙等）或直接丢弃

症状：生产任务全速写、预算满后自旋烧 CPU；或把 AGAIN 当错误丢事件——客户端缺事件。

原因：AGAIN 是背压信号（第 95 章同款语义）：正确动作是等待可写再发**同一条**——AGAIN 没保留输入，你手里的数据就是唯一副本。

```c bad
while ( xrtHttpSseSend(pStream, Data) == XHTTP_BODY_STREAM_AGAIN ) {
	/* 紧循环：烧 CPU 且可能永远挤不进 */
}
```

```c good
xhttpbodystreamresult R;
while ( (R = xrtHttpSseSend(pStream, Data)) ==
		XHTTP_BODY_STREAM_AGAIN ) {
	xfuture* pW = xrtHttpBodyStreamWaitWritable(pStream);
	if ( pW == NULL ) { break; }
	xrtFutureWaitFor(pW, Timeout);   /* 等待，不取消共享 Future */
	xrtFutureDestroy(pW);
}
/* R 为 OK/CLOSED 时退出循环分别处理 */
```

### 坑 3：忘设 X-Accel-Buffering，事件被代理攒住

症状：本地直连事件实时，上了 nginx 后客户端"半天收到一坨"——代理把响应流当可缓冲下载攒批。

原因：SSE 的实时性依赖中间设备**不缓冲**；`X-Accel-Buffering: no` 是告知代理的标准头——它属于部署环境知识，所以 xhttp 不自动加（协议/应用分界），你要加。

```c bad
pReply = xrtHttpSseReplyCreate(NULL, &pStream);
respond(pConn, pReply);   /* 经代理部署：事件被攒批 */
```

```c good
pReply = xrtHttpSseReplyCreate(NULL, &pStream);
xrtHttpReplySetHeader(pReply,
	XRT_STR_LITERAL("X-Accel-Buffering"),
	XRT_STR_LITERAL("no"));
xrtHttpReplySetHeader(pReply,
	XRT_STR_LITERAL("Cache-Control"),
	XRT_STR_LITERAL("no-cache"));
respond(pConn, pReply);
```

## 练习

### 基础：两端对拍

跑通 sse_server 与 sse_client；把服务端事件改成自定义类型+多行 data，客户端解析后打印结构。验收标准：多行 data 拼接正确；id 游标递增可见。

### 进阶：进度推送服务

第 103 章服务加 `/progress` 路由：任务提交后 SSE 推送进度（每 100ms 一条 percent 事件，heartbeat 每 15s）；生产任务在独立线程，经 Stream 写入。验收标准：慢客户端（限速消费）下服务端内存恒定（背压生效——观察 AGAIN 路径）；任务完成发带 id 的 done 事件后 EOF。

### 挑战：断线续传

客户端消费 N 条后断开；重连带 `Last-Event-ID`；服务端从游标后续传（游标存储自选：内存环形或第 18 章 Map）。验收标准：客户端事件流无缝续上（不重不漏）；服务端重启（游标丢失）时客户端从头收（记录这个行为差异）；心跳在空闲期维持连接。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 设计哲学 | 无专用状态机：普通 Reply 借道唯一响应状态机与既有背压 |
| 双对象 | Reply（可先销毁）+ Stream（独立生产端）；EOF/Close/Fail 三收尾 |
| 事件四要素 | event/data/id/retry + Flags；SseSendEvent 一次封包；id=续传游标 |
| 原子写入 | 先验证计量再编码一节点；失败无半条事件；预算覆盖预留/排队/租约 |
| 背压 | AGAIN 不留输入；WaitWritable 共享 Future 不可单方取消 |
| 直写路径 | Write/WriteRef/WriteTake——预编码/代理/扩展不必走结构化 Writer |
| 协议/应用分界 | Content-Type 归层；Cache-Control/CORS/X-Accel-Buffering 归你 |
| 客户端 | 连接层（重连+Last-Event-ID）+ 解析器（四限额结构化内存有界） |
| 消费循环 | DATA（块+Release）/AGAIN（Wait）/EOF 三态骨架 |
| 选型 | 单向推送用 SSE；双向才 WebSocket——不是越强越好 |
