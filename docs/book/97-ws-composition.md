---
num: 97
slug: ws-composition
title: WebSocket 组合：双向通信全链路
volume: 卷九 Web 协议核心 · 卷九收官
type: practice
lead: 关闭码语义、permessage-deflate 协商与流式压缩、全链路串联——卷九九章积木的收官组合。
api: websocket, websocket_stream, http
---

## 导读

卷九收官章。前三章给了升级（93）、帧（94）、流（95）；本章补上双向通道的最后两块工程件并把全链路串起来。**关闭协议**：`xrtWsCloseWrite/CloseParse` 的往返、关闭码的语义域（1000 正常/1001 离开/1002 协议错误……）、写侧强制（原因必须合法 UTF-8、总长不超限、保留码不上线）——优雅关闭是"双方都知道结束了"的握手。**permessage-deflate**：WebSocket 的压缩扩展——协商层（offer/response 的严格区分、window bits、context takeover）、`xwsinflater`/`xwsdeflater` 流式对象（分片推进不聚合、双重上限、RSV1 只在首帧）、Stream 层的压缩发送路径。最后用"一条消息的完整旅程"把卷九九章从 HTTP 解析到压缩分片帧收尾——第 98 章起进入 xhttp 的高层世界。

## 引入

两个工程问题收尾卷九。其一：连接怎么"好好说再见"？直接断 TCP，对端无法区分"正常结束"与"网络故障/中间设备截断"——监控告警、重连策略全都失效。Close 控制帧带着状态码与原因，让关闭成为**双方确认的协议事件**：发起方发 Close、对端回 Close、各自进入终态——`CloseInfo` 快照告诉你对端"为什么走"。其二：JSON 消息重复率高（键名、结构），文本压缩能省 70-90% 带宽——但 HTTP 的 Content-Encoding 思路（第 93 章）在双向流上要重新设计：压缩按**消息**不按连接、字典跨消息复用（context takeover）、控制帧永不压缩——这就是 permessage-deflate，协商与流式处理都是协议层工程。

## 概念

### 关闭握手与码语义

```diagram flow
- 发起：CloseWrite(码, 原因) 产控制帧负载 → Close 发送（唯一保证）
- 对端：收到 Close → 状态机自动回 Close（CloseInfo 快照给应用）
- 终态：双方 Close 交换完成 → CLOSED；超时（对端不回应答）强制收尾
```

关闭码的语义域分三段。**1000-1003**：正常关闭族（1000 正常、1001 端点离开、1002 协议错误、1003 不接受数据类型）；**1008-1011**：错误族（1008 策略违反、1009 消息过大、1010 缺扩展、1011 内部错误）；**3000-4999**：应用自定义（库不解释）；**保留码**（1005"无码"、1006"异常关闭"、1015"TLS 握手失败"）**只作 API 表达、禁止上线**——它们描述"没有 Close 帧的情况"，写侧强制拒绝。负载规则：码 2 字节 + 原因（合法 UTF-8、总长 ≤123——`XWS_CLOSE_PAYLOAD_MAX`）——写侧越界直接失败，读侧拒绝畸形负载。

### permessage-deflate：协商层

扩展名 `permessage-deflate`，参数四个：`server_max_window_bits`/`client_max_window_bits`（压缩窗口 9-15——越小越省内存、压缩率略降）、`server_no_context_takeover`/`client_no_context_takeover`（消息边界重置字典——省内存、跨消息压缩率降）。协商规则由协商层**严格强制**：offer 与 response 分开校验、重复参数拒绝、非法 window bits 拒绝、response 不得引入 offer 未提的能力、context takeover 两端一致。应答原则是**最小合规子集**——只回必要参数（提议 `server_max_window_bits=10`，采纳就只回这个）。`xrtWsDeflateDirection` 把协商结果转成单方向运行参数——收发两向参数可以不同。

### 流式压缩对象

`xwsinflater`（解压）/`xwsdeflater`（压缩）是流式对象，与第 29 章解压/92 章解码同姿态：

- **分片推进**：输入输出都按片段、不聚合整条消息——大消息内存恒定。
- **回调输出**：解出的数据直进调用方缓冲或发送队列。
- **双重上限**：解压总量受消息上限与 Inflate 上限双约束——压缩炸弹防御（3 字节输入声称解出 4 GB？上限先拦）。
- **no-context-takeover**：协商了就在消息边界重置字典。
- **容量规划**：`xrtWsDeflaterBound` 写入前给出上界——SendLimit 预算的输入。
- **失败路径**：回调失败/OOM/数据错误有明确 reset/abort/destroy 语义。

**RSV1 规则**：压缩标志 RSV1 只允许出现在压缩消息的**首个数据帧**——continuation 不重复、控制帧永不压缩。这是第 95 章"RSV 非零=扩展协商"的兑现。

### 一条消息的完整旅程（卷九全景）

```diagram flow
- 请求到达：TCP/TLS（67/87 章）→ RequestParse（89）→ 升级判定+握手（93）
- 101 之后：StreamAttach 接管（95）——帧状态机上线（94）
- 消息到达：FrameParse 三态 → 解掩码（分段相位）→ 消息重组（增量 UTF-8）
- 压缩消息：RSV1 → inflater 流式解压（双重上限）→ MessageData 视图
- 应用处理：回调内消费（转发/解析/回显）
- 回程：Text/Ref/Compressed 发送（统一 SendLimit）→ 分片帧 + 掩码（客户端向）
- 告别：Close(码,原因) → 对端回 Close → 双方终态快照
```

九个章节的 API 在一条消息里各就各位——这就是"积木结构"的验收形态。

## 示例

### 第一个完整程序：关闭负载往返

下面的程序来自 `examples/websocket/close/main.c`——码与原因的写出/解析往返：

```embed path="examples/websocket/close/main.c" title="examples/websocket/close/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/close/main.c -lws2_32 -liphlpapi
code=1000 reason=shutdown
```

**刚才发生了什么。** ① `CloseWrite(1000, "shutdown")` 产 9 字节负载（码 2 + 原因 8）——写侧三重强制在入口：原因 UTF-8 合法、总长 ≤123、码不在保留区间。② `CloseParse` 往返解析出 `xwsclose`{Code, Reason 视图}——码 1000 语义"正常关闭"，原因给人看（日志/审计）。③ 保留码实验：试图写 1005/1006/1015 会直接失败——它们是 API 层的"表达性码"（Stream 的 CloseInfo 用 1006 表示"异常关闭没收到 Close"），不是可发送的值。这条"写侧拒绝"挡住了实现者最常犯的协议错误。④ Stream 层（第 96 章）的 `Close` 内部就是这对函数——层间关系：本章是负载层，95 章是连接层。

### 第二个完整程序：压缩协商应答

第二个程序来自 `examples/websocket/deflate/main.c`——offer 到 response 的最小合规应答：

```embed path="examples/websocket/deflate/main.c" title="examples/websocket/deflate/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/deflate/main.c -lws2_32 -liphlpapi
response=permessage-deflate; server_max_window_bits=10
```

**刚才发生了什么。** ① offer 提议 `server_max_window_bits=10`（服务端压缩窗口 1KB）并通知客户端可带 `client_max_window_bits`。② 应答**采纳 10 并按最小合规子集回写**——只回必要参数：`permessage-deflate; server_max_window_bits=10`，不回显 client 侧的"能力通知"。**为什么最小**：response 回显的每个参数都是**承诺**——回了 `client_max_window_bits` 就承诺了窗口上限，多回=多承诺=少回旋。协商层的严格校验（重复参数/非法值/未提议能力）全部内建。③ 应答串放进 `Sec-WebSocket-Extensions` 响应字段——与第 94 章握手应答组合。配套的 `deflater/inflater` 示例演示流式对象的双向运转，`extension_tour` 是扩展层的完整巡检。

## 契约

- **关闭负载**：码 2 字节（网络序）+ 可选原因（UTF-8）；总长 ≤ `XWS_CLOSE_PAYLOAD_MAX`（125）；写侧强制三规则、读侧拒绝畸形。
- **码语义域**：1000-1003 正常族 / 1008-1011 错误族 / 3000-4999 应用自定义 / 1005/1006/1015 保留（仅 API 表达，禁止上线）。
- **关闭握手**：Close 唯一发送、对端自动应答、超时强制收尾、CloseInfo 终态快照（95 章连接层保证）。
- **压缩协商**：offer/response 分别严格校验；重复/非法/未提议即拒绝；应答最小合规子集；`DeflateDirection` 转单方向参数。
- **压缩参数**：window bits 9-15；context takeover 两端一致；no-takeover 在消息边界重置字典。
- **流式对象**：分片推进不聚合；回调输出；解压受消息+Inflate 双重上限（炸弹防御）；`DeflaterBound` 容量规划；明确 reset/abort/destroy。
- **RSV1 规则**：压缩标志只在首个数据帧；continuation 不重复；控制帧永不压缩。
- **发送路径**：`TextCompressed` 族按需创建 Deflater（95 章）；预算含压缩输出上界。
- **裁剪**：`WEBSOCKET_CLOSE`/`_DEFLATE`/`_DEFLATER`/`_INFLATER`/`_EXTENSION` 独立宏——不压缩的连接零压缩代码。

## 避坑

### 坑 1：给 Close 塞保留码或超长原因

症状：`CloseWrite(1006, ...)` 直接失败；或塞了 200 字节的"详细说明"当原因——同样失败。

原因：1005/1006/1015 是"表达没有 Close 帧情况"的 API 值，协议禁止上线；负载上限 125 字节（码 2 + 原因 123）。原因超长该走应用层消息，Close 原因只放摘要。

```c bad
xrtWsCloseWrite(1006, XRT_STR_LITERAL("abnormal"), ...);  /* 保留码：拒绝 */
xrtWsCloseWrite(1000, LongParagraph, ...);                 /* 超限：拒绝 */
```

```c good
xrtWsCloseWrite(1000, XRT_STR_LITERAL("policy: quota"), ...);
/* 码用语义域内的值；原因一句话；细节走应用消息或日志 */
```

### 坑 2：压缩应答回显了没承诺的参数

症状：自写协商应答把 offer 的参数全部回显——客户端按"服务端承诺了 client_max_window_bits"初始化，实际服务端没实现对应窗口，压缩流错乱。

原因：response 的每个参数都是承诺。最小合规子集是协议的安全姿态——只回你真要执行的。

```c bad
/* 把 offer 原样回回去 */
response = offer_string;   /* 回显了未承诺的能力 */
```

```c good
/* 用协商层应答：内建最小合规子集与全部校验 */
xrtWsDeflateInit(&Offer);
parse_offer(Fields, &Offer);
accept = deflate_accept(&Offer);   /* 只回必要参数 */
```

### 坑 3：不设 Inflate 上限，吃压缩炸弹

症状：恶意对端发 3 字节压缩片段声称解出 4 GB——解压器老老实实产出直到 OOM 被杀。

原因：压缩比是攻击面。消息上限（第 95 章配置）与 Inflate 上限**双重约束**是协议栈的标准防御——只设其一都有绕路（分片消息绕消息上限、多消息绕 Inflate 上限）。

```c bad
xrtWsInflaterConfigInit(&Config);    /* 不设上限直接建？危险 */
xrtWsInflaterCreate(&Config, NULL);
feed_everything(&Infl, PeerData);     /* 炸弹直达内存 */
```

```c good
/* 消息上限 + Inflate 上限都显式设置（量级按业务：如 1MB / 8MB） */
MessageConfig.MaxMessage = 1024 * 1024;
InflateLimit = 8 * 1024 * 1024;
/* 超限即协议错误：回 Close 1009（消息过大）断开 */
```

## 练习

### 基础：关闭码矩阵

对 `CloseWrite/Parse` 做五组实验：1000+短原因、1001+空原因、1005（保留）、原因含非法 UTF-8、130 字节原因——前三组验证写侧强制，后两组验证拒绝路径。验收标准：合法组往返一致；非法组错误信息能区分三类原因。

### 进阶：压缩链路手动装配

用 `deflater`/`inflater` 示例改造：发送侧把一条 10 KB JSON 压缩成 RSV1 帧（首帧 RSV1、分片、FIN），接收侧解压重组。统计压缩前后字节数与内存峰值（第 6 章）。验收标准：往返原文一致；分片边界在压缩数据中间也正确；内存峰值与消息大小无关（流式）。

### 挑战：回声服务的全特性版本

在第 96 章回声服务上加齐：压缩协商（offer 最小应答）、压缩收发（RSV1 规则）、关闭协议（正常 Close 1000 与策略拒绝 1008 两路）、消息上限与 Inflate 双上限、Ping/Pong（含背压期间的心跳）。与浏览器 WebSocket 对打（开发者控制台）。验收标准：大消息（1 MB 文本）压缩往返正确；恶意输入（超限消息/畸形帧/保留码 Close）全部按协议拒绝且连接干净关闭；全链零整消息缓冲。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 关闭负载 | 码（2B 网络序）+ UTF-8 原因；≤125 字节；写侧三强制、读侧拒畸形 |
| 码语义域 | 1000-1003 正常 / 1008-1011 错误 / 3000-4999 应用 / 1005·1006·1015 保留禁上线 |
| 关闭握手 | 唯一发送、自动应答、超时收尾、CloseInfo 快照 |
| 协商规则 | offer/response 分别严格校验；重复/非法/未提议即拒；最小合规子集应答 |
| 协商参数 | window bits 9-15；context takeover 一致；no-takeover 按消息重置 |
| 流式对象 | inflater/deflater 分片推进、回调输出、明确 reset/abort/destroy |
| 炸弹防御 | 消息上限 + Inflate 上限双重约束；超限回 1009 |
| RSV1 规则 | 压缩标志仅首数据帧；continuation 不重复；控制帧永不压缩 |
| 容量规划 | DeflaterBound 先算上界再进 SendLimit 预算 |
| 全景 | 一条消息的旅程串起 89→93→94→95→96 九章 API |
