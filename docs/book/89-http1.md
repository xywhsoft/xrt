---
num: 89
slug: http1
title: HTTP/1 消息：解析与封包
volume: 卷九 Web 协议核心
type: practice
lead: 请求/响应头的一次解析、状态行封包与查找遍历——绑定调用方数组的零逐请求分配核心路径；解析器与网络/TLS 流的绑定薄层收尾补齐。
api: http1, http, http1_net, http1_tls
---

## 导读

第 88 章的地基之上，本章组装完整的 **HTTP/1 消息层**：`xrtHttp1RequestParse`/`xrtHttp1ResponseParse` 把线上的请求/响应头解析为"起始行 + 字段数组"的结构（三态返回、借用视图），`xrtHttp1ResponseWrite` 把状态行与字段一次封包，字段检索直接用地基层的 `xrtHttpFieldFind`/`FieldNext`（对解析出的字段数组做大小写不敏感查找）。设计的关键词是**绑定数组**：`xhttp1head` 绑定调用方提供的字段数组——一个线程一个 Head 结构反复复用，零逐请求分配，这是 xhttp 运行时与自研网关的地基路径。正文（chunked 等）是第 90 章的分帧主题；本章只到"头块结束的空行"为止。

## 引入

一个 HTTP/1 服务端每秒处理几万请求，每个请求头约 1 KB、十几行字段。如果每次解析都分配 一个字段数组、解析完 free——分配器的锁与碎片先成为瓶颈，OOM 后的恢复路径再补一刀。XRT 的答案在 API 形状上就能看出来：`xrtHttp1HeadInit(&Head, Fields, 8)` 把**调用方的栈数组**绑给 Head——解析只是往数组里填视图；下一个请求复用同一个 Head 与数组，分配次数为零。

第二个设计决定是**三态返回**：`XHTTP1_READY`（完整头已到）、`XHTTP1_MORE`（输入不足，等更多数据）、`XHTTP1_ERROR`（结构非法）。配合"失败不推进"的约定，流式服务端可以无脑地"收一段、试解析、NEXT 就再收"——第 71 章分帧循环的 HTTP 版。

## 概念

### 解析：从字节到结构

`xhttp1head` 解析后的字段：`Method`/`Target`（请求）或 `Status`/`Reason`（响应）、`Version`、字段数组（`Fields`/`FieldCount`——绑定数组的填充区）、`Bytes`（头块总字节数——含终止空行，正文从这里开始）。三态返回模型：

```diagram state
输入不足 -> XHTTP1_MORE: 等待更多数据（不消费、不报错）
结构完整 -> XHTTP1_READY: 视图发布，Bytes 给出正文偏移
结构非法 -> XHTTP1_ERROR: 字段数超限/起始行非法/头不完整语义
```

请求与响应共用 Head 结构（角色由调用入口决定：`RequestParse`/`ResponseParse`）。`ResponseParse` 带方法参数——因为响应语义依赖请求方法（HEAD 的响应没有正文；这个上下文在第 90 章的 Plan 里变成硬规则）。

### 解析器的传输绑定（http1_net / http1_tls）

`RequestParse`/`ResponseParse` 的输入是**连续内存**——测试向量与完整缓冲的形态。真实收包路径拿到的是引擎的**缓冲链**（头可能被切在两块里，第 64 章），TLS 连接拿到的是**跨记录的明文流**（第 86 章）。绑定层把解析器接到这两种真实来源上：`xrtHttp1RequestParseBuffer`/`xrtHttp1ResponseParseBuffer` 消费 `xnetbuf`（Head 的视图借用 Buffer，跨块时才按需分配连续前缀——"函数不消费输入，调用方处理完成后消费 Head.Bytes"）；`xrtHttp1RequestParseTls`/`xrtHttp1ResponseParseTls` 直接吃 `xtlsstream`（不消费明文，跨记录只合并 Header，不分配连接级固定缓冲）。四个函数只有两个，不对——每个绑定两个，但签名与三态语义和内存版完全一致：**绑定层薄是因为解析器从设计起就把"字节从哪来"留给了入参**——上限防御（limits）、三态返回、零分配纪律原样有效。第 103 章的 xhttp 服务端与第 107 章的流式服务，底层走的就是这两个绑定。

### 查找与遍历

解析后的常见操作：对 `Head.Fields` 数组直接用第 88 章的 `xrtHttpFieldFind`/`FieldNext`（大小写不敏感——HTTP 字段名语义大小写不敏感；同名字段多值按序遍历，Set-Cookie 类）、`xrtHttpFieldGet`/`FieldGetUnique`（取值视图与唯一性语义）。这些与第 88 章的 token 迭代器组合，覆盖"查 Connection 是否含 upgrade"这类复合判断——第 93 章的 HTTP 升级（WebSocket 握手）就用这个组合。

### 封包：状态行 + 字段一次写

`xrtHttp1ResponseWrite(版本, 状态码, 短语, 字段数组, 数量, 输出, 容量, &长度)`——状态行与字段块**一次**写出："HTTP/1.1 200 OK\r\nContent-Type: ...\r\n\r\n"。容量原子性同第 88 章：不够就零写入。请求侧对应 `xrtHttp1RequestWrite`。**为什么不用 printf 拼**：拼接路径的每一步都可能中途失败（半报文）、格式细节（状态行空格、字段终止）散落各处——一次封包把这些收进一个经过变异测试的入口。

### 上限与防御

`xhttp1limits`（可选传入 Parse）控制：字段数上限（绑定数组容量即天然上限）、头块总长上限、方法/目标长度上限。**为什么必须设**：恶意客户端可以发送 2 GB 的头块——没有上限的解析器是内存放大器。默认值保守、代理场景按需放宽——上限是部署决策不是全局常数。

### Message 与 Body 的分层预告

本章 API 到"头块结束"为止。第 90 章的 `MessageParse` 组合"头 + 正文"一次到位（适合小消息），`Body` 族流式读正文（适合代理边收边转）——两层共享本章的头解析。

## 示例

### 第一个完整程序：请求解析与响应封包

下面的程序来自 `examples/http/http1/main.c`——最短路径的"收请求、发响应"：

```embed path="examples/http/http1/main.c" title="examples/http/http1/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http/http1/main.c -lws2_32 -liphlpapi
GET /health
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 11
```

**刚才发生了什么。** ① 输入是完整请求字节（静态数组模拟一次到达）；`HeadInit` 绑定 8 元素的栈数组——**解析零分配**。② `RequestParse` 返回 `READY`：`Head.Method`/`Target` 是借用视图（指向输入内部），打印 `GET /health`。③ `ResponseWrite` 一次封包：状态行（版本/状态/短语）+ 两个字段 + 空行；`iSize` 是完整字节数——随后正文 `{"ok":true}` 直接跟在后面写。**这就是服务端回包的全部**：没有逐行 printf、没有手工拼 CRLF。真实服务里输入来自第 66 章的接收缓冲（NEXT→继续收），输出交给发送路径（第 67 章写预算）。

### 第二个完整程序：Head 全特性巡检

第二个程序来自 `examples/http1/head_tour/main.c`——五行为你覆盖上限、查找、迭代、chunk 行与 Message：

```embed path="examples/http1/head_tour/main.c" title="examples/http1/head_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http1/head_tour/main.c -lws2_32 -liphlpapi
http1: limits + target valid +/- ok
http1: field lookup + transfer-coding iteration ok
http1: request body plan fixed/chunked ok
http1: chunk line 5 -> "5\r\n" (+ext) ok
http1: message parse -> borrowed body "ok" ok
http1: body trailers rebind + trailer parse ok
```

**刚才发生了什么。** ① 第 1 行验证上限模型：target 长度在限内通过、超限拒绝——防御参数的正负两侧。② 第 2 行是查找组合拳：按名找字段 + transfer-coding 的 token 迭代（第 88 章 TokenNext 在 Head 上的应用）。③ 第 3 行给第 90 章打样：请求正文的 Plan（定长/chunked 判定）在头解析后立即可得。④ 第 4 行验证 chunk size 行生成（`5\r\n`——可带扩展）。⑤ 第 5 行是 Message 层：一次解析"头+正文"，正文借用视图 `ok`——小消息的最省路径。⑥ 第 6 行验证 trailer 数组重绑与 trailer 块解析——第 90 章预告。六行合起来是本章+下一章的 API 面。

### 完整程序：ParseBuffer——缓冲链上的请求/响应解析

来自仓库范例 `examples/http1/parse_buffer/main.c`：

```embed path="examples/http1/parse_buffer/main.c" title="examples/http1/parse_buffer/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/http1/parse_buffer/main.c -lws2_32 -liphlpapi
request: GET /x
response: 200
```

**刚才发生了什么。** ① 请求字节**刻意分两块追加**进缓冲链——头跨块的最小化模拟，ParseBuffer 证明链上解析无需调用方拼接。② 解析结果与内存版同构：`Head.Method`/`Target` 借用视图直接打印。③ 响应侧同走 `ResponseParseBuffer`——请求/响应两个入口、一套 Head。④ `ParseTls` 版本无独立示例：它与 Buffer 版只差"流来源"这个参数（`xtlsstream` 替代 `xnetbuf`），契约行里说清即可，不硬造示例。

## 契约

- **绑定数组**：Head 绑定调用方字段数组；字段数天然受数组容量限制；复用 Head 前无需清理（每次 Parse 重建）。
- **三态返回**：`READY`/`NEXT`/`ERROR`；`NEXT` 不消费不报错；失败不推进（重试语义同第 71 章分帧）。
- **视图借用**：Method/Target/Status/Reason/字段全部借用输入；`Bytes` 是含终止空行的头块长度（正文起点）。
- **查找语义**：字段名大小写不敏感；同名字段多值按序遍历；找不到返回明确失败非 NULL 值。
- **封包原子性**：状态行+字段一次写出；容量不足零写入；成功发布总长。
- **上限防御**：limits 控制头块总长/方法/目标长度；无上限的解析是内存放大器；默认保守。
- **方法语义**：响应解析携带请求方法上下文（HEAD/204 无正文的规则在第 90 章由 Plan 执行）。
- **零逐请求分配**：解析与封包路径无堆分配（Head/数组/缓冲全由调用方提供）。
- **裁剪**：`HTTP1_HEAD`/`HTTP1_BODY`/`HTTP1_MESSAGE` 独立特性宏，依赖 HTTP 地基。

## 避坑

### 坑 1：NEXT 之后重新 Parse 全量（或不推进偏移）

症状：流式服务端"收一段试一段"写成死循环或重复解析同一批字节——CPU 空转或字段重复。

原因：`NEXT` 的语义是"这些输入不够"——正确动作是把新数据**追加**到同一缓冲再 Parse（输入从头开始重试，解析器保证 NEXT 路径无副作用）；或者配合返回的消费偏移推进。把它当"错误"丢缓冲重来，就回到拼接地狱。

```c bad
while ( recv(buf) > 0 ) {
	if ( Parse(buf) == XHTTP1_MORE ) {
		continue;   /* 只试新收的字节：永远不完整 */
	}
	break;
}
```

```c good
/* 追加式缓冲（第 64 章）：收到的都攒在同一段里 */
while ( recv_append(&In) > 0 ) {
	if ( xrtHttp1RequestParse(view(&In), &Head, NULL, NULL) ==
			XHTTP1_READY ) {
		break;   /* Bytes 之后的字节是正文/下一请求 */
	}
}
```

### 坑 2：字段数组按"恰好够"开

症状：稍长的请求（合法但字段多两条）解析失败——服务端对一部分客户端"随机"拒绝。

原因：绑定数组容量是硬上限。健康浏览器的请求头 15-25 个字段很常见；代理链还会追加 X-Forwarded-* 系列。

```c bad
xhttpfield Fields[4];   /* 拒绝大多数真实请求 */
xrtHttp1HeadInit(&Head, Fields, 4);
```

```c good
xhttpfield Fields[32];   /* 经验值：真实请求 25 + 转发追加余量 */
xrtHttp1HeadInit(&Head, Fields, 32);
/* 真有超长场景：ERROR 里区分"字段数超限"，
   代理按需放大或显式拒绝（这本身是防滥用决策） */
```

### 坑 3：响应封包忘了 Content-Length 之外的定界

症状：自写的响应没带 Content-Length 也没声明 chunked——客户端不知道正文何时结束，挂着等到超时或连接关闭。

原因：HTTP/1 的正文必须有定界声明（Content-Length、chunked、或"连接关闭即结束"——最后一种只对 HTTP/1.0 语义可靠）。封包 API 帮你写字段，但**定界字段是你必须提供的语义**。

```c bad
xrtHttp1ResponseWrite(XHTTP_VERSION_1_1, 200, OK,
	NoFields, 0, Out, sizeof(Out), &iSize);
send(Out, iSize);
send(Body, BodySize);   /* 无定界：客户端不知道何时结束 */
```

```c good
static const xhttpfield F[] = {
	{ XRT_STR_INIT("Content-Length"), XRT_STR_INIT("11") }
};
xrtHttp1ResponseWrite(XHTTP_VERSION_1_1, 200,
	XRT_STR_LITERAL("OK"), F, 1, Out, sizeof(Out), &iSize);
send(Out, iSize);       /* 头声明 11 字节 */
send(Body, 11);         /* 正文恰好 11：定界完整 */
```

## 练习

### 基础：两种方法对照

把 http1 示例的请求换成 POST（带 `Content-Length: 5` 与 5 字节正文），解析后打印 `Head.Bytes` 并验证"正文从 Bytes 开始"。验收标准：Bytes 与手数一致；正文视图与原字节对应。

### 进阶：分片到达压力测试

把 41 字节请求按 1..N 字节随机分片"到达"（模拟 TCP 分段），每片追加后 Parse——统计从第几片开始 READY。再故意在中间插入畸形（两个连续冒号的字段名）验证 ERROR 且此前状态未污染。验收标准：任意分片顺序最终 READY；错误输入在含畸形的分片到达时报 ERROR。

### 挑战：管道化请求解析器

一段缓冲里粘着两个完整请求（HTTP/1.1 pipelining）：解析请求一（Bytes 定位边界）→ 消费 → 在剩余缓冲上解析请求二。实现 `parse_pipelined(输入, 回调)` 循环直到 NEXT。验收标准：两个请求各自正确解析；第二个不完整时停在 NEXT 且缓冲状态可继续；全程一个 Head/一个数组零分配。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 核心入口 | RequestParse / ResponseParse（三态）；ResponseWrite / RequestWrite（一次封包） |
| 绑定数组 | HeadInit 绑调用方字段数组；容量即字段上限；复用零分配 |
| 三态 | READY（Bytes=正文起点）/ MORE（等数据，无副作用）/ ERROR（不推进） |
| 查找 | HeadFind 大小写不敏感；多值字段按序遍历；配 TokenNext 做列表判断 |
| 封包 | 状态行+字段一次写；容量不足零写入；定界字段（CL/chunked）是调用方语义责任 |
| 上限 | limits 控头块/方法/目标长度；默认保守；防内存放大 |
| 方法上下文 | 响应解析带请求方法；HEAD/204 无正文规则由 Body Plan 执行 |
| Message 层 | 头+正文一次到位（小消息）；Body 层流式（第 90 章） |
| 零分配 | 解析/封包路径无堆分配；Head/数组/缓冲全由调用方出 |
| 传输绑定 | ParseBuffer 吃缓冲链 / ParseTls 吃 TLS 流——签名与三态同内存版；跨块/跨记录才合并 |
