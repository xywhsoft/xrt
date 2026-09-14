---
num: 91
slug: http-framing
title: 正文分帧图解：定长、chunked 与边界
volume: 卷九 Web 协议核心
type: practice
lead: Plan 先行、Body 流式、Chunk 写出与 trailer 收集——HTTP/1 正文的三种定界与一次读懂的分帧状态机。
api: http1, http_te, http
---

## 导读

第 90 章停在头块结束的空行；正文从那里开始——而"正文怎么结束"是 HTTP/1 最容易出错的问题。本章图解三种定界：**定长**（Content-Length——读够即止）、**chunked**（Transfer-Encoding: chunked——逐块长度行，0 块终止）、**关闭定界**（无声明、连接关闭即结束——HTTP/1.0 语义）。核心设计是 **Plan 先行**：`xrtHttp1RequestBodyPlan`/`ResponseBodyPlan` 先由头推导"怎么读"（含方法语义——HEAD/204 无正文），再按 Plan 初始化流式读取器 `xrtHttp1BodyInit/Read/Done`；写侧对称提供 `ChunkWrite/ChunkEndWrite`。trailer（chunked 尾部的字段块）与 TE 协商（客户端能力声明）也在本章。这是第 72 章通用分帧在 HTTP 语境的完全特化——状态机更细，但"增量吸收粘包半包"的骨架不变。

## 引入

代理转发一个 2 GB 的响应：上游 chunked 到达、下游客户端只支持 HTTP/1.0？流式读入逐块转发，还是先攒完 2 GB？服务器动态生成内容（发送前不知道总长）怎么声明定界？客户端怎么在 chunked 尾部收 trailer（跨代理的完整性摘要）？——这些问题的答案全部系于**分帧层**：它决定"边到边转"是否可能（内存与正文大小无关）、"动态长度"怎么表达（chunked）、"元数据后置"怎么收（trailer）。

错误使用分帧的代价也不抽象：**定界歧义就是请求走私**（request smuggling）——同一连接上前后两个请求对边界理解不同，攻击者把第二个请求藏进第一个的"正文"里穿过代理。CL 与 TE 同时出现怎么办（必须拒绝）、CL 值有多个怎么办（必须拒绝）——本章契约区把这些规则写死。

## 概念

### 三种定界与 Plan 推导

```diagram flow
- 定长：Content-Length: N —— 读 N 字节即完（最简单；发送前必须已知总长）
- chunked：Transfer-Encoding: chunked —— "16 进制长度\r\n数据\r\n"逐块，"0\r\n"终止，尾随 trailer 块
- 关闭定界：两者皆无 —— 连接关闭即结束（HTTP/1.0 语义；1.1 服务端响应不允许、1.1 客户端读到关闭）
- Plan：由头字段 + 方法推导 —— 请求/响应各自的规则（响应侧 HEAD/204/304 无正文）
```

`xrtHttp1ResponseBodyPlan(&Head, 方法, &Plan)` 与 `RequestBodyPlan(&Head, &Plan)` 的产出是"怎么读"的完整决策：定界方式、总长（若定长）、是否允许 trailer。**方法参与语义**：HEAD 响应、204/304 响应声明了 CL 也无正文——Plan 按角色清零。**歧义拒绝**：CL 与 TE 并存、多个不一致 CL——Plan 返回失败（走私防御的第一道闸在推导层）。

### 流式读取：Body 状态机

```diagram state
XHTTP1_BODY_DATA -> XHTTP1_BODY_DATA: 消费一段正文（Data 视图 + Consumed 字节）
XHTTP1_BODY_DATA -> XHTTP1_BODY_DONE: 定界满足（定长读够 / 0 块到达）
任意 -> XHTTP1_BODY_ERROR: 超上限 / 非法块行 / trailer 块超限
```

`xrtHttp1BodyRead(&Body, 输入视图, 是否终结, &消费, &数据视图, NULL)` 是核心循环：喂入当前缓冲，返回"出数据"（Data 借用视图——零拷贝直读）、"完成"、"错误"三态之一，`Consumed` 报告本次消费了多少（剩余字节属于下一消息/正文外）。**上限防御**（`xhttp1bodylimits`）：MaxBody（正文总量）、块行长度、trailer 数量——恶意超大块在进入前被拦。trailer 通过绑定数组收集（`BodyInit` 传入 trailer 数组），`Done` 后可查——与第 90 章字段数组同一形态。

### 写侧：生成 chunk 与终止块

`xrtHttp1ChunkWrite(数据, 输出, 容量, &长度)` 生成"16 进制长度\r\n + 数据\r\n"完整块；`xrtHttp1ChunkEndWrite(trailer数组, 数量, ...)` 生成"0\r\n"终止与可选 trailer 块。两板斧组合出任意流：动态内容边生成边发（配合第 68 章 Vec 发送摊还系统调用）；大文件按固定块循环。定长正文更简单——头里写 CL、正文直发。**选择法则**：知道总长用 CL（省 6 字节/块的行开销与解析成本）；不知道或流式生成用 chunked。

### trailer：正文之后的元数据

chunked 允许终止块后携带字段块——`Digest`（内容完整性）、跨代理的 tracing ID 等后置元数据的标准位置。两条纪律：**读侧**——trailer 数组容量超限即错误（与字段上限同理）；**发侧协商**——客户端 `TE: trailers` 才表示认识 trailer（下一节），服务端应在确认后使用；不支持代理剥掉 trailer 是合法行为。

### TE 协商：传输编码能力声明

`xrtHttpTeParse(字段数组, 数量, &Info)` 汇总（可重复的）TE 字段：接受的传输编码列表、`XHTTP_TE_ACCEPTS_TRAILERS` 标志（客户端声明认识 trailer）、各编码的 q 值。`xrtHttpTeQuality(字段, 数量, 编码名)` 返回**千分位整数**（`gzip;q=0.5` → 500）——避免浮点比较的精度与平台差异。服务端据此选双方都可接受的编码；q=0 表示明确不要。这套"整数 q 值 + 标志位"的协商模型与其他字段族一致。

## 示例

### 第一个完整程序：Plan 流式读 + 分块写出

下面的程序来自 `examples/http/http1_body/main.c`——读侧与写侧的完整闭环：

```embed path="examples/http/http1_body/main.c" title="examples/http/http1_body/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http/http1_body/main.c -lws2_32 -liphlpapi
chunked body
5
hello
0
```

**刚才发生了什么。** **读侧**（前 4 行输出的第一行）：① 响应头声明 `Transfer-Encoding: chunked`，`ResponseParse` 后 `ResponseBodyPlan`（带方法 GET）推导出 chunked 计划；`BodyLimitsInit` 设 MaxBody=1024 后 `BodyInit` 绑定 trailer 数组。② 输入是"`7\r\nchunked\r\n5\r\n body\r\n0\r\nDigest: ok\r\n\r\n`"——`BodyRead` 循环逐段吐出 `chunked` 与 ` body` 两段（拼接打印为 `chunked body`），`0` 块到达后 trailer `Digest: ok` 落入绑定数组、状态 DONE。零拷贝：Data 视图直接指向原缓冲。**写侧**（后三行输出）：③ `ChunkWrite("hello")` 产出 `5\r\nhello\r\n`（打印为 `5`、`hello` 两行），`ChunkEndWrite(带 Digest trailer)` 产出 `0\r\nDigest: sha-256=:demo:\r\n\r\n`（首行 `0`）——**写侧生成的字节序列与读侧消费的格式完全一致**，这就是第 72 章"读侧与写侧共用同一分帧状态机"的 HTTP 版。

### 第二个完整程序：TE 协商读取

第二个程序来自 `examples/http/te/main.c`——客户端能力的汇总查询：

```embed path="examples/http/te/main.c" title="examples/http/te/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/http/te/main.c -lws2_32 -liphlpapi
codings=1 trailers=yes gzip=500
```

**刚才发生了什么。** ① 两条 TE 字段（HTTP 允许字段重复）被 `TeParse` 汇总成一份能力：传输编码 1 种（gzip——`trailers` 是能力标志不算编码）、接受 trailer。② `TeQuality("gzip")` 返回 500——`q=0.5` 的千分位整数形态；浮点协商的精度陷阱（0.1+0.2≠0.3 一族）从表示层消灭。③ 服务端决策链：`trailers=yes` → 允许在 chunked 尾部发 Digest；`gzip=500` → gzip 可用但低优先。配套的 `examples/http1/variants` 与 `examples/http/trailer` 分别覆盖定界变体与 trailer 细节——本章练习会用到。

## 契约

- **Plan 先行**：读正文前必经 Plan（头推导）；方法参与语义（响应侧 HEAD/204/304 无正文）；歧义定界（CL+TE 并存、不一致多 CL）在 Plan 层拒绝——走私防御第一闸。
- **Body 三态**：DATA（Data 借用视图 + Consumed）/ DONE（定界满足）/ ERROR；循环喂入直到 DONE；错误不继续。
- **上限防御**：MaxBody/块行长/trailer 数三线上限；超限在消耗内存前拒绝——上限是部署决策。
- **trailer 收集**：绑定数组收集；容量超限即错误；Done 后可查；读侧不校验"客户端是否声明 trailers"（那是发侧协商）。
- **写侧对称**：ChunkWrite（块）+ ChunkEndWrite（终止+可选 trailer）；与读侧共用格式状态机；容量原子性同第 89 章。
- **定界选择**：已知总长用 CL；流式生成用 chunked；1.1 服务端不得裸关闭定界。
- **TE 协商**：可重复字段汇总；q 千分位整数（无浮点）；`ACCEPTS_TRAILERS` 标志；未提及编码 q=0。
- **零拷贝**：Data 视图借用输入；正文不过中转缓冲——代理"边收边转"内存与正文大小无关。
- **Message 层**：`MessageParse` 组合头+正文一次到位（小消息便利路径），流式场景用 Body 族。

## 避坑

### 坑 1：CL 与 TE 并存时"挑一个信"

症状：自研代理对同时带 Content-Length 与 Transfer-Encoding 的请求按 CL 处理，后端按 TE——同一连接的下一请求被"藏"进正文：请求走私。

原因：两种定界并存是歧义输入——前后端理解不一致正是走私攻击的构造方式。唯一安全动作是**拒绝**。

```c bad
if ( has_content_length(Head) ) {
	plan_as_fixed(...);      /* 我按 CL 读 */
} else {
	plan_as_chunked(...);
/* 下游若相反理解：边界错位 → 走私窗口 */
}
```

```c good
/* Plan 层已内置拒绝：让推导结果说话 */
if ( !xrtHttp1RequestBodyPlan(&Head, &Plan) ) {
	return reject("ambiguous framing");  /* 歧义定界：唯一安全响应 */
}
```

### 坑 2：BodyRead 的 Consumed 没接上消费偏移

症状：流式代理丢字节或重复转发——每轮喂入的起点错了。

原因：`BodyRead` 的输入是"当前缓冲从哪开始由你决定"——它消费了多少（`iConsumed`）必须累进下一轮的偏移；缓冲里没消费的尾部可能属于下一消息（pipelining）。

```c bad
size_t iOff = Head.Bytes;
while ( !Done ) {
	BodyRead(&Body, view(In), false, &iUsed, &Data, NULL);
	forward(Data);
	/* iUsed 丢了：下一轮又从原偏移喂 */
}
```

```c good
size_t iOff = Head.Bytes;
while ( !xrtHttp1BodyDone(&Body) ) {
	xhttp1bodystatus St = xrtHttp1BodyRead(&Body,
		(xbytesview){ In.Data + iOff, In.Size - iOff },
		bFin, &iUsed, &Data, NULL);
	iOff += iUsed;                 /* 消费接上偏移 */
	if ( St == XHTTP1_BODY_DATA ) { forward(Data); }
	else if ( St != XHTTP1_BODY_DONE ) { return false; }
}
/* In 中 iOff 之后的数据属于下一消息 */
```

### 坑 3：服务端动态内容"先攒完再定长"

症状：为了写 Content-Length，把整个动态响应攒在内存——2 GB 的导出接口把服务攒 OOM。

原因：CL 要求发送前已知总长。内容流式生成时唯一正确解是 chunked——每块生成即发，内存与总量无关。

```c bad
buffer_whole_response(&Body);   /* 攒完 2 GB */
set_content_length(Body.Size);
send_all(Body);
```

```c good
send_header_chunked();
while ( next_chunk(&Data) ) {
	xrtHttp1ChunkWrite(Data, Out, sizeof(Out), &iSize);
	send(Out, iSize);            /* 边生成边发 */
}
xrtHttp1ChunkEndWrite(Trailers, 1, Out, sizeof(Out), &iSize);
send(Out, iSize);               /* 0 块 + trailer 收尾 */
```

## 练习

### 基础：三种定界各走一遍

构造三个响应字节流（CL=11、chunked 两块、无声明），分别 Plan+Body 流式读到 DONE，打印各自动作序列（消费轮数/数据段数）。验收标准：三组结论与手推一致；无声明组的"关闭定界"在传 bFin=true（对端关闭）时 DONE。

### 进阶：走私样本实验

构造四个恶意样本：CL+TE 并存、双 CL 不一致、超 MaxBody、块行超长——逐一验证 Plan/Body 层的拒绝。把每个样本的拒绝层次写进表格。验收标准：四个样本全部被拒；能指出每道闸在解析管线的哪一层。

### 挑战：流式转码代理核心

上游 chunked 响应逐块到达：Body 流式读 → 每段数据做"大写转换" → ChunkWrite 重新分块（固定 16 字节块）→ 下游转发；trailer 原样转发（ Digest 重算可选）。验收标准：内存占用与上游总量无关（第 6 章统计对照）；上下游块边界不同（7+5 进、16×N 出）仍语义正确；上游中断时下游收到干净错误而非半帧。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 三种定界 | 定长 CL / chunked（0 块终止）/ 关闭定界（1.0 语义） |
| Plan 先行 | 由头+方法推导；HEAD/204 无正文；歧义定界（CL+TE、不一致 CL）拒绝 |
| Body 三态 | DATA（视图+Consumed）/ DONE / ERROR；消费偏移必须累进 |
| 上限三线 | MaxBody / 块行长 / trailer 数；超限前拒绝；部署决策 |
| trailer | 终止块后字段块；绑定数组收集；发侧须客户端声明 TE trailers |
| 写侧两板斧 | ChunkWrite（长度行+数据）/ ChunkEndWrite（0 块+trailer）；与读侧同格式机 |
| 定界选择 | 已知总长→CL；流式→chunked；1.1 服务端禁裸关闭 |
| TE 协商 | 重复字段汇总；q 千分位整数；ACCEPTS_TRAILERS 标志；未提及 q=0 |
| 零拷贝 | Data 借用输入；边收边转内存与正文大小无关 |
| Message 层 | 头+正文一次到位（小消息）；Body 族是流式原语 |
