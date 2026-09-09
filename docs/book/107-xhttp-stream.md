---
num: 107
slug: xhttp-stream
title: 流式正文：上传、下载与异步文件
volume: 卷十 扩展库：xhttp
type: practice
lead: xhttpbody 的五来源与可重放语义、Reader 的租约式消费、有界生产流与异步文件正文——正文内存与内容大小解耦。
api: xhttp-http_body, xhttp-http_body_stream, xhttp-http_body_file
---

## 导读

HTTP 的正文是"内容"与"内存"的战场：2 GB 上传、流式下载、按需生成的响应——**正文大小不应等于内存占用**是全部设计的出发点。`xhttpbody` 统一五种正文来源：固定族（`Empty/Copy/Borrow/Take/Reference`——可重放，已知长度）、**有界生产流**（`HttpBodyStreamCreate`——任务线程生产、预算受限、SSE 的底座）、**文件正文**（`http_body_file`——异步文件 IO 的正文化）、自定义来源（生产者回调）与变换正文。消费侧 `xhttpbodyreader` 单消费者流式读取，`xhttpbodychunk` 是**独立数据租约**（`ChunkRelease` 归还、可晚于 Reader 释放——TCP 引用发送的完美搭档）。第 103 章服务端的 Body 回调、第 106 章 SSE 的生产端，在本章拿到完整的地基叙述。

## 引入

三个正文场景的内存形态要求。上传 2 GB：服务端不能攒完再处理——第 103 章的流式 Body 回调已解决接收侧；但**客户端发送侧**呢——把 2 GB 读进内存再 POST？文件正文让"边读边发"。下载流式生成：响应长度未知、内容按需生产（数据库游标、实时转码）——生产流正文让"边生成边发"，预算封顶。重放需求：第 99 章 307/308 重定向要求正文可重放——固定族天然可重放，生产流呢？`HttpBodyStreamCreate` 产出的 Body **消费一次**——重定向要重发的场景用固定族或文件正文。

租约是本章最重要的新概念：`Next` 吐出的 Chunk **拥有独立数据租约**——数据不复制，但你必须 `ChunkRelease` 归还；Chunk 可以活得比 Reader 久（引用发送的队列里排着，正文对象已销毁——引用计数管理）。这与第 95 章 WebSocket 引用发送（写完才释放）是同一思想在正文层的落点。

## 概念

### 五来源与可重放语义

| 来源 | 创建入口 | 可重放 | 典型场景 |
| --- | --- | --- | --- |
| 固定族 Empty/Copy/Borrow/Take/Reference | `xrtHttpBodyCopy` 等 | 是（已知长度=输入字节） | JSON/短正文/307 重发 |
| 有界生产流 | `xrtHttpBodyStreamCreate(配置, &流)` | 否（一次消费） | SSE/流式生成/进度输出 |
| 文件正文 | `http_body_file` 族 | 是（可重开） | 大文件上传/静态下载 |
| 自定义来源 | 生产者回调 | 取决实现 | 数据库游标/加密管道 |
| 变换正文 | 组合层 | 取决实现 | 压缩响应 |

固定族细节：`Copy` 把描述符与副本放**一个按实际长度的紧凑分配**（不为同一份正文建第二个分配）；`Borrow` 不延长外部内存生命周期（发送期间原缓冲必须活着）；`Reference` 必须提供释放过程（只借用就用 Borrow）。`xrtHttpBodyView` 借固定正文的连续字节——不保证连续的来源返回 `false`（文件/流式/变换可能不连续）。

### Reader：单消费者与租约

```diagram flow
- 打开：Reader 打开正文（单消费者——不允许并发调用）
- 读取：Next(上限, &Chunk) → DATA（租约块）/ AGAIN（暂无，Wait 等 Future）/ EOF
- 租约：Chunk 独立数据租约——用完 ChunkRelease；可晚于 Reader/Body 释放
- 重放：固定族可再次打开 Reader 从头读
```

别名纪律（契约原文级）：输出的 Chunk 描述符/缓冲/长度槽**不得覆盖** Reader、Body 或固定正文的底层字节——别名错误不推进 Reader、不清空输出长度，固定 Body 在重放与并发打开时始终保持不可变。这套防御让"正文对象被多个 Call 共享"（第 98 章冻结语义的正文引用）安全。

### 有界生产流：预算与收尾

`xhttpbodystreamconfig` 的 `MaxBytes/MaxChunks` 是生产端预算（预留+排队+活动租约全覆盖）——SSE 的 AGAIN 阈值即来自这里。收尾三路：最后一个流引用销毁=排队内容后**正常 EOF**；`Close` 幂等预关输入；`Fail` 丢弃未交付内容并把稳定 Cause 传给消费侧。`Write/WriteRef/WriteTake` 三写入形态对应复制/引用/接管——与第 67 章发送五档同族。

### 文件正文：异步 IO 的正文化

`http_body_file` 族把第 71 章的完成端口文件 IO 包成正文档：`xrtHttpBodyFileConfigInit` + 文件正文创建——**读取走异步文件路径**（Engine 的 Worker 驱动），不阻塞、不整读；`xrtHttpBodyFileFuture`/`FileRangeFuture` 提供"等正文就绪"的 Future 形态。Range 变体天然支撑断点上传/下载。文件正文**可重放**（重开文件重读）——大文件 + 重定向重发的安全组合。

### 上传与下载的完整形态

- **上传**（客户端）：文件正文 + 第 98 章构建器 `SetBody`——2 GB 文件上传的客户端内存 ≈ 一个读块；服务端第 103 章流式 Body 回调接收。
- **下载**（客户端）：第 98 章 `ResponseBodyLimit` 管预算、流式消费（Body 回调或 Reader）——内存与下载量解耦；第 92 章解码器串联在消费链上。
- **生成响应**（服务端）：生产流正文 + `Respond`——数据库游标逐步产出；预算封顶后 AGAIN 让生产者等待消费侧。TLS 服务（第 85 章）上跑这一整套时，请求头解析走的就是第 89 章解析器的 TLS 绑定（http1_tls）——跨记录不分配连接级缓冲，流式纪律一路贯通。

## 示例

### 第一个完整程序：生产流写出与读回

下面的程序来自 `examples/http/body_stream`——有界生产流的最小闭环：

```embed path="extlibs/xhttp/examples/http/body_stream/main.c" title="extlibs/xhttp/examples/http/body_stream/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/body_stream/main.c -lws2_32 -liphlpapi
first chunk
second chunk
```

**刚才发生了什么。** ① `Config.MaxBytes=1024/MaxChunks=16` 定预算 → `HttpBodyStreamCreate` 一call得**双对象**：`xhttpbody`（交给请求/响应）与 `xhttpbodystream`（生产端持有）。② 两段 `Write` 后销毁流——**最后引用销毁=正常 EOF**：排队内容先交付、EOF 随后。③ 读回循环（与第 106 章客户端同款骨架）：`Next(8, &Chunk)` 小上限分段、DATA 打印+`ChunkRelease`、AGAIN 走 `xrtHttpBodyReaderWait` 的 Future 等待、EOF 收尾。④ 两行输出即"生产端写入的字节流完整到达消费端"——**正文对象是生产与消费的解耦点**：两端可在不同线程、不同时刻。真实部署里生产端是任务线程（SSE 事件/游标产出），消费端是 HTTP 传输。

### 第二个完整程序：文件正文上传

第二个程序来自 `examples/http/body_file`——大文件的上传形态：

```embed path="extlibs/xhttp/examples/http/body_file/main.c" title="extlibs/xhttp/examples/http/body_file/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/body_file/main.c -lws2_32 -liphlpapi
（示例校验文件正文创建与读回后正常退出）
```

**刚才发生了什么。** ① `xrtHttpBodyFileConfigInit` 配置路径与选项 → 创建文件正文——**长度来自文件元数据、内容按需异步读**：客户端内存与文件大小无关（对比 `Copy` 整读——2 GB 就是 2 GB）。② 文件正文可重放（重新打开重读）——第 99 章 307/308 重定向的重发要求在这里天然满足；固定流式生产则不行（一次消费）。③ `xrtHttpBodyFileFuture` 的 Future 形态让"等文件就绪/读完"进入第 57 章的等待体系。④ 配套 `examples/http/body`（固定族五来源）、`body_compose`（组合正文）、`body_inflate/deflate`（变换正文——与第 92 章解码对应的发送侧压缩）、`server_body_async`（服务端异步正文响应）——正文族的完整巡检面。

## 契约

- **五来源**：固定族（可重放·已知长度）、生产流（一次消费·有界）、文件（可重放·异步读）、自定义、变换——`xhttpbody` 统一形态。
- **固定族纪律**：Copy 单分配（描述符+副本紧凑块）；Borrow 不延长外部生命；Reference 必须有释放过程；View 只对保证连续的来源成功。
- **生产流**：MaxBytes/MaxChunks 预算覆盖预留+排队+租约；EOF/Close/Fail 三收尾；Write 三形态（复制/引用/接管）。
- **Reader**：单消费者；DATA（租约）/AGAIN（Wait Future）/EOF 三态；Chunk 独立租约（ChunkRelease；可晚于 Reader/Body 释放——引用发送友好）。
- **别名防御**：输出不得覆盖正文底层字节；别名错误不推进不破坏——固定 Body 重放与并发打开恒不可变。
- **文件正文**：异步读不阻塞不整读；可重放；Range 变体；Future 就绪形态。
- **重放矩阵**：307/308 重发→固定族/文件；一次生成→生产流。
- **预算归属**：客户端 ResponseBodyLimit（第 98 章）限表示正文；生产流预算限生产端——两层各自有界。
- **裁剪**：`HTTP_BODY`/`HTTP_BODY_STREAM`/`HTTP_BODY_FILE` 独立宏。

## 避坑

### 坑 1：Borrow 正文发送期间原缓冲被改/释放

症状：偶发发送错乱内容——栈上的缓冲、或发送前就 free 的堆缓冲配了 Borrow。

原因：Borrow 的契约是"不延长外部内存生命周期"——正文只记指针，发送（可能异步、可能重发）期间原数据必须原样存活。不确定就 Copy 或 Reference。

```c bad
uint8 Buf[256];
fill(Buf);
xrtHttpRequestSetBody(pReq, xrtHttpBodyBorrow(view(Buf)));  /* 栈缓冲 */
send_async(pReq);   /* 函数返回后栈帧消亡：发送读到垃圾 */
```

```c good
/* 要么 Copy（小正文），要么 Reference+释放回调（自有堆缓冲） */
xrtHttpRequestSetBody(pReq, xrtHttpBodyCopy(view(Buf)));
/* 或 xrtHttpBodyReference(view(HeapBuf), release_cb, ctx)——发送完自动释放 */
```

### 坑 2：生产流预算用满后丢数据继续产

症状：流式响应丢段——AGAIN 后生产者跳过这条继续下一条（"免得阻塞"）。

原因：AGAIN 是背压不是错误——**没有保留任何输入**，你手里的这条数据就是唯一副本；跳过=丢数据。正确动作：等 `WaitWritable` 再发同一条。

```c bad
for ( each_item ) {
	if ( send(stream, item) == AGAIN ) {
		continue;   /* 丢弃——客户端缺这段 */
	}
}
```

```c good
for ( each_item ) {
	while ( send(stream, item) == AGAIN ) {
		wait_writable(stream);   /* 第 106 章坑 2 的等待模板 */
	}
}
```

### 坑 3：Reader 的 Chunk 忘 Release（或当作视图长期持有不还）

症状：分配统计里租约只增不减——预算耗尽快全部 AGAIN。

原因：Chunk 是租约不是视图——占着生产端预算；Release 是归还动作。要长期持有就复制（引用发送除外——那正是租约的设计场景：发送完成时释放）。

```c bad
while ( next(&Reader, &Chunk) == DATA ) {
	queue_push(g_Q, Chunk);   /* 租约不还：预算耗尽 */
}
```

```c good
while ( next(&Reader, &Chunk) == DATA ) {
	if ( want_keep(Chunk) ) {
		queue_push(g_Q, copy_chunk(Chunk));  /* 留副本 */
	}
	xrtHttpBodyChunkRelease(&Chunk);          /* 还租约 */
}
```

## 练习

### 基础：五来源对照

同一份文本分别用 Copy/Borrow/Take/Reference/生产流创建正文并发送（对本地服务），对比分配统计与可重放性（读两次）。验收标准：分配次数与契约一致；固定族二次读取成功、生产流第二次打开失败（或按契约的行为）。

### 进阶：大文件上传管线

客户端文件正文 POST 2 GB 文件（生成临时文件）→ 服务端流式 Body 回调接收写盘 → 比对哈希。全程记录两端内存峰值。验收标准：两端峰值均与文件大小无关（MB 级）；哈希一致；中途限速（模拟慢盘）管线稳定。

### 挑战：变换正文（加密管道）

自定义生产者正文：读文件块 → AES-GCM 加密块（第 74 章）→ 编码进正文——形成"加密文件下载"服务。客户端用第 92 章解码思路反向解密消费。验收标准：往返原文一致；密钥只在两侧内存（SecureZero 收尾——第 76 章纪律）；内存峰值恒定。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 五来源 | 固定族（可重放）/生产流（一次）/文件（异步可重放）/自定义/变换 |
| 固定族 | Copy 单分配；Borrow 不延长生命；Reference 带释放；View 仅连续来源 |
| 生产流 | 双对象（Body+Stream）；MaxBytes/MaxChunks 预算；EOF/Close/Fail 三收尾 |
| Reader | 单消费者；DATA（租约）/AGAIN（Wait）/EOF；ChunkRelease 归还 |
| 租约 | Chunk 独立数据租约、可晚于 Reader 释放——引用发送的设计场景 |
| 别名防御 | 输出不覆盖正文底层；固定 Body 重放/并发打开恒不可变 |
| 文件正文 | 异步读不整读；可重放；Range 变体；Future 就绪 |
| 重放矩阵 | 重定向重发→固定/文件；一次生成→生产流 |
| 预算两层 | 客户端 ResponseBodyLimit（表示）+ 生产流预算（生产端） |
| 裁剪 | BODY/BODY_STREAM/BODY_FILE 独立宏 |
