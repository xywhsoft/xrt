---
num: 93
slug: http-decode
title: 正文解码：Content-Encoding 流式还原
volume: 卷九 Web 协议核心
type: practice
lead: 从响应头建流式解码器、逐片段喂入推回调、Done 校验收尾——gzip/deflate 的网络流式姿势与内容协商。
api: http_decode, http_encoding, http1
---

## 导读

第 91 章解决了"正文的边界"；本章解决"正文的内容编码"。`Content-Encoding: gzip` 的响应在分帧层读出的是**压缩字节**——业务要的是明文。`xrtHttpDecodeCreate` 由响应头字段直接构建流式解码器（自动串接多级编码——`gzip, br` 这样的链式声明），`xrtHttpDecodeWrite` 逐片段喂入、解出的明文推给回调，`xrtHttpDecodeDone` 校验结尾（gzip 的 CRC 与长度 trailer——防静默损坏）。设计姿态与第 91 章一致：**片段随到达随解、明文直推消费方**——全程不攒完整响应，内存占用与正文大小无关。配合发送侧的内容协商（Accept-Encoding 的 q 值）构成压缩的完整闭环，卷九 HTTP 核心部分到此收官。

## 引入

下载一个 50 MB 的 gzip 压缩 JSON：天真实现是"收完整个响应→整块解压→得到 80 MB 明文"——峰值内存 130 MB，且要等最后一字节到齐才开始处理。流式实现是"每收到一段压缩片段→立刻解出一段明文→推给解析器"——内存恒定（一个滑动窗口量级）、首字节延迟约等于网络首段。差别不在 gzip 算法（第 29 章的解压器已是流式），而在**接线**：谁来把"HTTP 分帧层吐出的片段"接到"解压器的输入"上、谁来处理多级编码的串接、谁来校验结尾？

`http_decode` 模块就是这根线：输入端是第 91 章 `BodyRead` 吐出的 Data 视图，输出端是你给的回调，中间自动串接 Content-Encoding 声明的每一级。头字段声明什么就解什么——包括 `identity`（不解）与多级链；声明了本构建没编入的编码（如 br）创建时明确失败——不静默跳过（静默=把压缩数据当明文给业务，后面全错）。

## 概念

### 由头构建：Create 的契约

`xrtHttpDecodeCreate(字段数组, 数量, 可选配置)` 扫描 `Content-Encoding` 字段（多条按出现顺序串接）构建解码器：`gzip`（第 29 章 Deflate 家族的 gzip 容器）、`deflate`（zlib 包装）、`identity`（直通）、多级链（`gzip` 之后再 `gzip` 的病态但合法的声明也正确处理）。没有 `Content-Encoding` 字段 = 直通。**失败语义**：未知编码或未编入的编码创建返回 `NULL`——调用方决定降级（丢弃响应）还是报错；绝不"猜一个相近的解一下"。

配置（可选 `NULL` 取默认）区分 **compat 与 safe 两种模式**：历史服务器（特别是老 deflate 实现）有一些不规范的输出，compat 模式按宽容窗口接受（浏览器同款兼容行为），safe 模式严格拒绝——默认取安全侧，与遗留后端对接时显式放宽。这是"严格解析"纪律面对历史包袱的标准折衷：默认严格、放宽必须显式。

### 流式喂入：Write 与回调

```diagram flow
- 到达：BodyRead 吐出一段压缩 Data 视图（第 91 章）
- 喂入：DecodeWrite(解码器, Data, 是否最后一段, 回调, 上下文)
- 推送：解出的明文片段立即进回调（零攒批）——业务随到随处理
- 收尾：DecodeDone 校验 gzip CRC 与长度 trailer——防静默损坏
```

`bFinal` 参数标记"这是最后一段"——gzip 流的结尾块（含 CRC32 与原始长度）在此校验；`Done` 也可在 Write 之后单独调用（有些传输"喂完了"这件事与"最后一段"分离）。**回调形态**的意义：解码器不为你分配缓冲——明文推给你，缓冲策略（直接解析？写入管道？落盘？）完全归你。与第 29 章解压回调、第 91 章 Body 视图一脉相承。

### 与内容协商的闭环

解码是响应侧；请求侧要声明"我能解什么"：`Accept-Encoding: gzip;q=1.0, deflate`（`xrtHttpAcceptEncodingQuality` 查 q 值——千分位整数模型，第 91/92 章同款）。服务端从请求头选双方都可接受的编码写进 `Content-Encoding`。客户端的纪律：**只声明你真的能解的**——把 `br` 写进 Accept-Encoding 却没编 brotli 解码器，服务端选了 br 你的 Create 就失败。`http_encoding` 模块提供请求侧的解析与构造。

### 组合全景：一次完整响应的消费管线

```diagram flow
- 头：ResponseParse（第 90 章）→ 字段数组
- 分帧：ResponsePlan + Body 流式读（第 91 章）→ 压缩片段 Data 视图
- 解码：DecodeCreate（由头）→ Write 逐片段 → 明文回调
- 消费：回调里做业务（JSON 增量解析/写盘/转发）
- 校验：DecodeDone + BodyDone —— 内容与边界双确认
```

四层各司其职、层间只有视图与回调——这是卷九前五章搭出的完整骨架，第 98 章的 xhttp 运行时把它封装成高层 API。

## 示例

### 第一个完整程序：从响应头到明文回调

下面的程序来自 `examples/http/decode/main.c`——42 字节的 gzip 流完整解出：

```embed path="examples/http/decode/main.c" title="examples/http/decode/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/http/decode/main.c -lws2_32 -liphlpapi
hello compressed world
```

**刚才发生了什么。** ① 输入是内嵌的 42 字节 gzip 流（`1F 8B` 魔数开头）与一条 `Content-Encoding: gzip` 字段——模拟第 91 章已经吐出的"头数组 + 压缩正文"。② `DecodeCreate` 扫字段建解码器：一级 gzip。③ `DecodeWrite` 一次喂入（`bFinal=true`——本例单段到达；流式场景每段 false、最后 true），解出的明文**直接推给 `printBody` 回调**——网络形态下这里换 JSON 解析器或管道写入，内存不随正文涨。④ `DecodeDone` 校验结尾：gzip 流尾部的 CRC32 与长度字段对不上会在这里失败——**压缩数据在传输中被改坏，宁可报错不出错数据**（静默损坏是压缩链路最阴的故障）。⑤ Destroy 释放——解码器是本模块唯一的拥有式对象。

### 第二个完整程序：解码器全特性巡检

第二个程序来自 `examples/http/decode_tour/main.c`——三行覆盖模式、内容模式与重置：

```embed path="examples/http/decode_tour/main.c" title="examples/http/decode_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/http/decode_tour/main.c -lws2_32 -liphlpapi
decode: config compat vs safe + create ok
decode: gzip content mode + input/output sizes ok
decode: reset to identity passthrough ok
```

**刚才发生了什么。** ① 第 1 行对比配置两种模式并验证创建——compat/safe 的分野在第 91 章讲过的"默认严格、放宽显式"。② 第 2 行验证 gzip 内容模式与输入输出尺寸统计——`input/output sizes` 给压缩比计算与监控指标直接可用的数据。③ 第 3 行 **reset 到 identity 直通**——同一连接上的下一响应可能不压缩：解码器状态归零、模式切换，对象复用不重建（高连接数服务的每响应成本再降一档）。

## 契约

- **由头构建**：`DecodeCreate` 扫 Content-Encoding（多条按序串接）；无字段=直通；未知/未编入编码创建失败——不猜测不静默跳过。
- **模式**：safe 默认（严格）；compat 显式放宽（历史服务器的宽容窗口）；配置 `NULL` 取安全默认。
- **流式**：Write 逐片段（bFinal 标记末段）；明文即时推回调；回调不成立则该片段处理失败——解码器不替你缓冲。
- **收尾校验**：Done 校验 gzip CRC 与长度 trailer；失败=内容损坏，正确动作是丢弃整响应而非使用部分。
- **重置**：Reset 归零并切 identity——同连接多响应复用同一对象。
- **内容协商**：Accept-Encoding 声明须与解码能力一致；`http_encoding` 模块（AcceptEncodingParse/Quality/Select）提供请求侧解析/构造；q 千分位整数。
- **对象模型**：Create/Destroy 拥有式（本模块唯一分配点）；Write/Done 无额外分配。
- **裁剪**：`XRT_MODULE_HTTP_DECODE` 依赖 COMPRESS 与 HTTP 地基——不解压的客户端零成本裁掉。
- **组合边界**：上游接 Body 的 Data 视图（第 91 章）；下游接你的回调——层间零拷贝贯穿。

## 避坑

### 坑 1：把 bFinal 恒置 true（或恒 false）

症状：恒 true——多段流从第二段起全错（解码器把每段当完整流收尾校验）；恒 false——Done 永远报"流不完整"，CRC 校验形同虚设。

原因：`bFinal` 是"传输层知道这是最后一段"的信号，来自你对第 91 章 Body DONE 状态的判断——不是随便填的。

```c bad
while ( read_chunk(&Data) ) {
	DecodeWrite(pDec, Data, true, cb, ctx);  /* 每段都当末段 */
}
```

```c good
while ( (St = xrtHttp1BodyRead(&Body, In, bEof,
		&iUsed, &Data, NULL)) != XHTTP1_BODY_DONE ) {
	if ( St != XHTTP1_BODY_DATA ) { return false; }
	/* 最后一段：Body 层即将 DONE（下一轮就是 DONE）*/
	DecodeWrite(pDec, Data, false /* 除真正末段外 */, cb, ctx);
}
xrtHttpDecodeDone(pDec);   /* 分帧层收尾后再校验解码层 */
```

### 坑 2：Done 失败后"用已经推出来的那部分数据"

症状：CRC 校验失败（传输损坏），但回调已经推了 90% 的明文给下游——下游把残缺数据当完整处理了。

原因：流式的天然张力——数据已经推出去才知道结尾坏了。解法是**下游也按事务处理**：CRC 失败时下游必须能作废（丢弃/回滚），或者你选择"全部缓冲完再确认"的保守模式（牺牲流式换安全）。

```c bad
/* 回调直接把明文写进数据库 */
static bool onPlain(xbytesview Data, ptr pCtx) {
	db_append((db*)pCtx, Data);   /* Done 失败时已写入：脏数据 */
	return true;
}
```

```c good
/* 回调写进可作废的暂存（文件/内存段），Done 通过后一次性提交 */
static bool onPlain(xbytesview Data, ptr pCtx) {
	staging_append((staging*)pCtx, Data);
	return true;
}
/* ... */
if ( !xrtHttpDecodeDone(pDec) ) {
	staging_discard(&Staging);   /* 整体作废：干净失败 */
} else {
	staging_commit(&Staging);
}
```

### 坑 3：Accept-Encoding 写了 br，构建没编 brotli

症状：服务端按协商选了 brotli 压缩，客户端 `DecodeCreate` 失败——下载功能对一部分站点"随机"不可用。

原因：内容协商声明的是**能力承诺**。写进 Accept-Encoding 的每种编码，你的构建必须真的能解。XRT 当前支持 gzip/deflate/identity——br 不要写。

```c bad
/* 从某处抄来的"标准头" */
send_header("Accept-Encoding: gzip, deflate, br, zstd");
/* 服务端选 br → Create 失败 */
```

```c good
/* 声明与能力一致：本构建支持 gzip/deflate */
send_header("Accept-Encoding: gzip, deflate");
/* 未声明 br → 服务端不会选它 */
```

## 练习

### 基础：分段喂入实验

把 decode 示例的 42 字节流按 7/13/22 字节三段喂入（前两段 bFinal=false），验证输出不变。再故意翻转末段一个字节验证 Done 失败。验收标准：分段与整段输出一致；CRC 失败被捕获且报告为解码错误。

### 进阶：完整管线串联

把第 90/91/93 章串成完整消费链：一段完整 chunked+gzip 响应字节（自备，可用 curl 生成）→ ResponseParse → BodyPlan/Body 流式读 → DecodeCreate/Write → 回调统计明文总长 → 双 Done 校验。验收标准：明文与 `curl --compressed` 结果一致；全链零整响应缓冲（第 6 章统计对照）；任一层失败时后续层不再执行。

### 挑战：条件协商客户端

实现 `fetch(Url, 支持的编码列表)`：请求侧按能力构造 Accept-Encoding（q：gzip 1000、deflate 900）；响应侧走完整管线；对 `Content-Encoding` 不在能力内的响应（服务端不理会协商）返回明确错误而不是错误解码。验收标准：对 gzip/deflate/无编码三种响应都正确；能力外编码报"unsupported content-encoding"；并发 8 路下载内存恒定。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 三入口 | Create（由头字段构建）/ Write（逐片段推回调）/ Done（CRC 收尾校验） |
| 编码支持 | gzip / deflate / identity / 多级链；未知或未编入创建失败 |
| 模式 | safe 默认严格；compat 显式放宽历史兼容；NULL 配置取安全默认 |
| bFinal | 末段标记——来自 Body 层 DONE 判断；恒真/恒假都是错 |
| 回调形态 | 明文即时推送；缓冲策略归你；解码器零分配 |
| 收尾语义 | Done 失败=整响应作废；下游须可作废（staging 模式） |
| 重置 | Reset 归零切 identity——同连接多响应复用 |
| 内容协商 | 只声明真能解的；q 千分位；encoding 模块构造请求侧 |
| 组合边界 | 上游 Body 的 Data 视图、下游你的回调——零拷贝贯穿 |
| 裁剪 | XRT_MODULE_HTTP_DECODE 依赖 COMPRESS；不解压零成本裁掉 |
