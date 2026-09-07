---
num: 97
slug: xhttp-easy
title: xhttp easy：一行请求的完整力量
volume: 卷十 扩展库：xhttp
type: practice
lead: GetSync 一行的背后没有第二套实现——便利层如何透明继承运行时的超时、缓存、TLS 与错误链。
api: xhttp-http_client, xhttp-http_client_easy, net
---

## 导读

卷九给了你 HTTP 的全部零件（解析、分帧、字段、解码）；卷十的 xhttp 把它们组装成**高层客户端**——本章从最薄的一层开始：**easy 便利层**。`xrtHttpClientGetSync(客户端, URL, NULL)` 一行完成请求-响应；关键是这一行的背后**没有隐藏 Engine、Client 或第二套请求实现**——它只创建临时请求交给 `xrtHttpClientDo()` 冻结，超时、取消、重定向、Cookie、缓存、代理、TLS、解压、诊断与错误链和底层入口完全一致。学完本章你得到的是"能用的最短路径"，以及一个判断力：什么时候该停在 easy 层、什么时候需要下沉到第 98 章的请求构建器。

## 引入

写一个"取一个 URL 打印状态码"的工具，curl 时代是一行命令；C 库时代通常是三十行装配（Engine、Client、请求、回调、等待、清理）。easy 层把这段压缩到三行核心——但设计问题在"压缩"之外：多数库的便利层是**另一条代码路径**，行为与底层入口悄悄分叉（easy 不支持重定向、错误码不同、诊断缺失）。xhttp 的立场是便利层只做语法糖：`http_client_easy` 在调用期间创建临时 `xhttprequest`、交给 `Do` 冻结、返回前销毁——**你永远可以把 easy 调用机械地改写成构建器形式，行为不变**。

这层的存在让 xhttp 的学习曲线变成三级台阶：本章（一行调用）→ 第 98 章（构建器与运行时全貌）→ 第 103 章（服务端）。

## 概念

### 三组便利入口与三种完成形态

| 入口 | 语义 |
| --- | --- |
| `xrtHttpClientGet(客户端, URL, 选项, 回调, 上下文)` | 无正文 GET（**不人为创建空正文**） |
| `xrtHttpClientPost(客户端, URL, 字节, 类型, 选项, 回调, 上下文)` | 复制固定字节正文 |
| `xrtHttpClientSendBytes(...)` | PUT/PATCH/任意方法携固定字节正文 |

回调形态是基线；启用 `http_client_easy_future` 后同名入口加 `Async`（返回 `xfuture`）与 `Sync`（阻塞取 `xhttpresult*`）后缀。**同步入口禁止在网络 Worker 上阻塞**——它在调用线程等待终态， Worker 上调用会死锁，这是与第 81 章 Future 同步等待一致的纪律。

### GET 无正文与 POST 空正文的语义分界

便利层故意区分两种"没有内容"：GET **没有正文**（不生成 Content-Length/Content-Type）；POST/SendBytes 传**空字节视图表示显式零长度正文**（生成 `Content-Length: 0`）。这个区分来自协议语义——GET 的正文本就未定义，而 POST 空 body 是合法且明确的表态。同理 `ContentType` 为空时不生成 Content-Type——不撒谎。

### 便利层的边界：它故意不接受什么

Header、流式正文、表单、认证、Upload——便利入口**故意不接受**这些。需要它们时就下沉：`xrtHttpRequestCreate()` + `xrtHttpRequestSetBody()` + `xrtHttpClientDo*()`——底层入口始终公开且与便利层并列（第 98 章）。这个"故意"是设计：便利层的参数表保持一行放得下；复杂需求用显式构建器表达，没有"便利层的隐藏配置结构"这种中间态。

### 结果对象：只读快照

`xhttpresult` 是完成后交给调用方的只读快照：`xrtHttpResultResponse(结果)` 借出 `xhttpresponse`（版本/状态/理由/Header/trailer/**最终有效 URL**——重定向后的落点，第 99 章）、`xrtHttpResponseBody(响应)` 连续正文视图、`xrtHttpResultDestroy` 释放。正文按需增长不预留固定缓冲；流式执行（第 98 章）则不分配正文缓冲只记字节数。**结果用完必 Destroy**——它是本章唯一需要你管理的拥有式对象。

### 装配最小集

easy 调用前的固定三步：`xrtNetEngineConfigInit/Create/Start`（Engine）→ `xrtHttpClientConfigInit` + `xrtHttpClientCreate(引擎, 配置)`（Client）→ 便利调用。Client 创建即验证 Dial/Stream/Exchange/私有 Resolver 的静态配置——无效策略当场失败，不拖到首个请求（第 82 章身份"启动期验证"的同款哲学）。清理顺序与第 66 章一致：先 Client 后 Engine，Engine 的 Destroy 可能要等 Worker 排空（自旋重试是标准姿势——本章示例的收尾就是模板）。

### easy 在 xhttp 分层中的位置

```diagram flow
- easy 便利层（本章）：Get/Post/SendBytes 一行——临时请求交 Do 冻结
- 运行时（第 98 章）：请求构建器 + Client + 调用选项 + 连接池/超时/取消
- 自动行为层（第 99/100 章）：重定向/重试/Cookie/解压/缓存——配置开关
- 协议底座（卷九）：解析/分帧/字段/解码——零件
```

## 示例

### 第一个完整程序：一行 GET

下面的程序来自 `examples/http/client_easy`——三行核心的完整工具：

```embed path="extlibs/xhttp/examples/http/client_easy/main.c" title="extlibs/xhttp/examples/http/client_easy/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_easy/main.c -lws2_32 -liphlpapi
usage: client_easy <http-url>
```

**刚才发生了什么。** ① 装配三步后，`xrtHttpClientGetSync(pClient, URL视图, NULL)` 一行提交并等待——第三个参数 `NULL` 取默认调用选项（超时/重定向/缓存等继承 Client 配置）。② 成功路径三件套：`HttpResultResponse` 借响应 → `HttpResponseStatus` 状态码 + `HttpResponseBody` 正文视图（直接 fwrite，零复制）→ `HttpResultDestroy` 释放。③ 失败时 `GetSync` 返回 `NULL`、详情在线程错误槽（第 4 章模型贯穿到这里）。④ 收尾的自旋 `while (!xrtNetEngineDestroy(...))` 值得注意——Engine 销毁可能因在途收尾返回失败，清错重试直到成功，这就是"最后销毁 Engine"在 xhttp 语境的标准写法。

### 第二个完整程序：Future 形态

第二个程序来自 `examples/http/client_future`——同一套调用的异步面：

```embed path="extlibs/xhttp/examples/http/client_future/main.c" title="extlibs/xhttp/examples/http/client_future/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_future/main.c -lws2_32 -liphlpapi
usage: client_future <http-url>
```

**刚才发生了什么。** ① 这里用的是构建器路径提交（`HttpRequestCreate` + `HttpClientDo`），因为演示目标包括请求对象的显式生命周期——对照 easy：`Get` 系列就是这个三行的折叠。② Future 完成后 `xrtFutureValue` 取结果、`HttpResultResponse` 走同样的只读快照路径——**Sync 与 Future 共享同一结果对象**，便利层只是"谁来等"的差别（第 81 章双形态的 xhttp 版）。③ 把两个示例并排读：同样的装配、同样的收尾、同样的错误模型——差异只在提交与等待的两行。这就是"没有第二套实现"的可验证形态。

## 契约

- **无第二实现**：easy 只创建临时请求交 `Do` 冻结；一切行为（超时/取消/重定向/Cookie/缓存/代理/TLS/解压/诊断/错误链）与底层一致；可机械改写为构建器形式。
- **三入口三形态**：Get/Post/SendBytes × 回调/Async/Sync；Sync 禁止在 Worker 上阻塞。
- **正文语义**：GET 无正文（不造空正文）；空字节视图=显式零长度；ContentType 空=不生成该字段。
- **故意不接受**：Header/流式正文/表单/认证——下沉到构建器（第 98 章），无中间态。
- **结果对象**：只读快照；`ResultResponse` 借响应、`ResponseBody` 借正文；最终有效 URL 含重定向落点；用完 `ResultDestroy`。
- **启动期验证**：Client 创建即验静态配置，无效策略不延迟到首请求。
- **裁剪**：`XHTTP_MODULE_HTTP_CLIENT_EASY`（回调）与 `_EASY_FUTURE`（Future/同步）独立。
- **装配纪律**：Engine 先启动；清理先 Client 后 Engine；Engine Destroy 自旋重试。

## 避坑

### 坑 1：在 Worker 回调里调 Sync 入口

症状：死锁——完成回调在 Worker 上执行，Sync 在同一线程等待完成。

原因：Sync 的实现是"调用线程等终态"；Worker 等自己完成的工作等于永久互等。回调里要么转发给别的线程，要么直接用回调形态本身。

```c bad
static void on_done(xhttpcall* pCall, ..., ptr pCtx) {
	/* 在 Worker 上：*/
	pResult = xrtHttpClientGetSync(pClient, OtherUrl, NULL); /* 死锁 */
}
```

```c good
static void on_done(xhttpcall* pCall, ..., ptr pCtx) {
	/* 回调里就用回调形态提交下一个请求 */
	xrtHttpClientGet(pClient, OtherUrl, NULL, on_next, pCtx);
}
/* 确要串行阻塞等待：在非 Worker 线程用 Sync/Future */
```

### 坑 2：把 easy 当能力边界，绕路造配置

症状：需要加一个 Header 就放弃了 easy——自己拼 Engine+DNS+TCP+TLS 重写请求路径；或反过来，往 easy 参数里硬塞全局配置结构。

原因：没意识到边界是刻意的：easy 之上没有隐藏配置；需要的不是"更聪明的 easy"而是**下沉一层**——构建器一行 `HttpRequestSetHeader` 就够，其余装配全部复用。

```c bad
/* 为了一个 Header 丢掉整个 xhttp 客户端 */
hand_roll_engine_dns_tls_http();  /* 300 行 */
```

```c good
xhttprequest* pReq = xrtHttpRequestCreate();
xrtHttpRequestSetMethod(pReq, ...);
xrtHttpRequestSetUrl(pReq, ...);
xrtHttpRequestSetHeader(pReq, XRT_STR_LITERAL("Authorization"), Token);
xrtHttpRequestSetBytes(pReq, Body, iSize, Type);
pCall = xrtHttpClientDo(pClient, pReq, NULL, on_done, pCtx);
xrtHttpRequestDestroy(pReq);   /* Do 冻结快照后原请求可立即销毁 */
```

### 坑 3：结果对象忘了 Destroy（或提前 Destroy 响应）

症状：每次请求泄漏一个结果对象；或正文视图用到一半响应被释放——崩溃。

原因：`xhttpresult` 是拥有式快照（响应/正文归它管）；`ResultResponse`/`ResponseBody` 只是借用。生命周期规则：**视图活在结果里，结果活在你的 Destroy 前**。

```c bad
const xhttpresponse* pR = xrtHttpResultResponse(pResult);
xrtHttpResultDestroy(pResult);      /* 先毁了家 */
printf("%.*s\n", Body from pR);     /* 视图悬空 */
```

```c good
const xhttpresponse* pR = xrtHttpResultResponse(pResult);
xbytesview Body = xrtHttpResponseBody(pR);
consume(Body);                       /* 先消费 */
xrtHttpResultDestroy(pResult);       /* 后释放 */
```

## 练习

### 基础：三个入口各跑一遍

对同一测试服务（本地起第 103 章的 server 示例或任何 httpbin 类服务）分别用 Get/Post/SendBytes 取回结果，打印状态码与正文长度。验收标准：三种入口的装配代码完全一致；GET 响应无 Content-Type 请求头生成。

### 进阶：Sync 与 Future 对拍

同一 URL 分别用 `GetSync` 与 `Get`+Future 完成，用 `xhttpcallresult.Info` 的 `FirstByte`/`RequestWireBytes` 对比两次调用的诊断字段。验收标准：两路径诊断字段语义一致；能解释 `WireBytes` 与 `BodyBytes` 的差别（线上字节 vs 交付正文——解压后不同）。

### 挑战：easy→构建器机械改写

把一个 `Post`（带类型）调用逐行改写为 `HttpRequestCreate/SetMethodUrl/SetBytes + Do` 形式，两版本对拍输出与诊断。再改写 `Get`。验收标准：行为逐项一致（状态/正文/最终 URL/错误类别）；改写过程不需要查任何"easy 专属"文档——因为它不存在。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 设计立场 | 无第二实现：临时请求交 Do 冻结；行为与底层完全一致 |
| 三入口 | Get（无正文）/ Post（复制字节）/ SendBytes（任意方法+字节） |
| 三形态 | 回调基线；`_EASY_FUTURE` 加 Async（xfuture）/ Sync（阻塞） |
| 正文语义 | GET 无正文；空视图=显式零长；类型空=不生成字段 |
| 边界 | 故意不接受 Header/流式/表单/认证——下沉构建器 |
| 结果对象 | 只读快照；Response/Body 借用；最终 URL 含重定向落点；必 Destroy |
| Sync 纪律 | 禁止 Worker 上阻塞——回调里用回调形态 |
| 启动验证 | Client 创建即验静态配置 |
| 装配 | Engine 启动 → Client 创建 → 调用；清理反向，Engine 自旋销毁 |
| 裁剪 | EASY 与 EASY_FUTURE 独立宏 |
