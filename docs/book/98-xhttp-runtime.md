---
num: 98
slug: xhttp-runtime
title: xhttp 运行时：构建器、调用与连接池
volume: 卷十 扩展库：xhttp
type: practice
lead: 请求快照的冻结语义、两类截止时间与取消、诊断快照，以及按 origin 分片的连接池——运行时的全部齿轮。
api: xhttp-http_client, xhttp-http_client_runtime, net
---

## 导读

easy 层（第 97 章）折叠了什么？本章展开给你看。**请求构建器**（`xhttprequest`）：方法/URL/Header/正文/认证的显式装配，`Do` 提交时冻结快照——调用方可立即修改或销毁原请求；**调用层**：`xhttpcalloptions` 的两类截止时间（总超时覆盖全链、空闲超时只看无进展时长）、取消令牌、响应体限额；**诊断**：`xhttpcallresult.Info` 的状态机与时间戳（单调时钟微秒）——`RequestWireBytes`/`ResponseWireBytes` 累计整条重定向链；**连接池**：按 origin 分片（最多 32 片）、每片独立等待 FIFO 与空闲 LRU、全局原子配额——复用是性能的根。四个齿轮合起来就是"运行时"——easy 的每一行都在这上面跑。

## 引入

三个场景逼你从 easy 下沉。场景一：要发 JSON POST 带 `Authorization` 头——构建器三行的事，easy 做不到（故意的）。场景二：批量请求同一服务——每个请求新建 TCP+TLS 握手的开销是吞吐天花板；连接池的复用把第二次起的成本降到零握手。场景三：慢源站——总超时管"整个请求最多等多久"，但一个"每小时推一个字节"的源站在总超时内永远不完；**空闲超时**专治这种"连接活着但没进展"的拖拽。

第三个场景值得多想一步：为什么需要两种超时？总超时是资源上限（这个请求最多占我 30 秒）；空闲超时是**进展检测**（传输在动就继续等，卡住 10 秒就放弃）。收到请求字节、消费响应字节都刷新空闲计时——"慢但在动"与"快但卡死"由此可分。xhttp 把两者做成了正交配置：`Timeout` 与 `IdleTimeout` 各自独立、零值继承 Client、`XHTTP_CLIENT_TIMEOUT_NONE` 显式关闭。

## 概念

### 请求构建器与冻结语义

`xrtHttpRequestCreate(方法, URL)` 建立可修改的请求；`SetHeader`/`SetBytes`（固定字节正文，复制一次）/`SetBody`（引用正文——文件、生产者流）/`SetAuth` 族（认证）逐项装配。**冻结语义**：`xrtHttpClientDo` 返回成功前复制请求与值语义配置、保留正文引用——**提交后原请求可立即修改或销毁**（异步执行的独立性由快照保证）。`Clone` 的方法/URL/Header/Trailer 完全独立、正文只加引用——"改一份发一份"的模板成本是 O(差量)。

### 调用层：截止时间、取消与限额

```diagram flow
- 提交：Do 冻结快照 → 排队 → Worker 执行 → DNS/拨号/TLS/发送/接收
- 总超时：从提交起覆盖全链（排队/DNS/TCP/代理/TLS/收发）
- 空闲超时：从执行起计"无进展"时长——传输/收发字节都刷新
- 取消：xrtHttpCallCancel 任意线程；提交前已取消直接进取消终态
- 限额：ResponseBodyLimit 限表示正文（解码前）；Decompress.MaxBody 限明文
```

两条限额的层次要分清：`ResponseBodyLimit` 作用在**去掉 HTTP/1 分帧后、Content-Encoding 解码前**的表示正文——网络响应、缓存命中、本地组合的 Range 响应同预算；解码后的明文另由 `Decompress.MaxBody` 限制——**高压缩比内容绕不过内存预算**（第 96 章"炸弹防御"的客户端版）。SSE 这类长连接选无界表示正文、靠 Parser 的结构化限额约束——"限什么"由内容结构决定，不是一刀切。

### 诊断快照：Info 的全部字段

`xhttpcallresult.Info`（完成回调携带，也可任意线程复制运行中快照）：`State`（排队/执行/**不可变终态**——终态发布后所有字段冻结，迟到的取消改写不了已发布结果）、`Phase`（实际结束阶段保留）、微秒时间戳（`TransportReady`/`RequestSent`/`FirstByte`/`Headers`——单调时钟，未到为零）、字节统计（`RequestWireBytes`/`ResponseWireBytes` **累计整条重定向链**；`ResponseBodyBytes` 是最终交付正文——自动解压时记明文）、`ReusedConnection`（任一跳复用过）、`Secure`（当前/最终跳加密）。这套字段就是"慢在哪一跳"的体检表——第 136 章性能分析会回来用它。

### 连接池：origin 分片与公平等待

启用 `http_client_pool` 后按 origin（scheme+大小写不敏感 host+port+代理身份）管理连接：

- **分片**：按 Worker 数建最多 32 个固定 origin 分片，每片独立索引/等待 FIFO/空闲 LRU/清扫 Timer——**不为每条连接建 Timer**；跨片只用原子配额协调，无 Client 级热锁。
- **公平**：同片等待按"最早可运行"FIFO；连接归还时同 origin 等待者直接接管；全局额度耗尽时跨片轮转分发——没有"某片永远休眠"。
- **回池条件**：消息边界完整、无 `Connection: close`、无升级、传输健康——四条全过才复用；**不用 HTTP/1 pipelining**（一条连接同时一个事务——第 89 章走私防御的工程兑现）。
- **默认值**：活动不限、全局空闲 128、每 origin 空闲 8、90 秒清扫；空闲上限零=关闭复用。
- **运维口**：`xrtHttpClientCloseIdle`（只摘空闲不打断活动）、`xrtHttpClientStats`（可并发读的数量与单调计数）。

### 提交即验证、失败即冻结

Client 创建验静态配置；`Do` 提交验配置范围（地址回绕同步拒绝、不发布回调）；终态一经发布不可改写——**迟到的取消/超时/传输失败不会覆盖已发布结果**。这三条把"配置错误"与"运行失败"都钉在明确的时点，调试时错误不再漂移。

## 示例

### 第一个完整程序：构建器装配与 URL 内省

下面的程序来自 `examples/http/client_request`——JSON POST 的显式装配：

```embed path="extlibs/xhttp/examples/http/client_request/main.c" title="extlibs/xhttp/examples/http/client_request/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_request/main.c -lws2_32 -liphlpapi
POST api.example.test:443
```

**刚才发生了什么。** ① `HttpRequestCreate(方法, URL)` 一步建立请求——URL 是完整形态（scheme+host+query），内部由第 101 章的 `xurl` 解析。② `SetHeader` 加 Accept、`SetBytes` 复制 14 字节 JSON 正文并声明类型——构建器的两个常用 Set。③ `xrtHttpRequestUrl` 借出已解析的 URL 结构，`xrtUrlPort` 取数值端口（443——HTTPS 默认值由此补齐，第 101 章 `XURL_PORT_VALUE` 语义）；打印证明**构建器内省能力**：方法、主机、端口都是可读事实。④ Destroy 收尾——请求不提交也能当"URL/方法的结构化持有者"用。真实提交：`xrtHttpClientDo(pClient, pRequest, NULL, on_done, pCtx)` 后**立即 Destroy 原请求**——冻结语义让你不必跟踪"异步还在用吗"。

### 第二个完整程序：连接池统计与空闲清理

第二个程序来自 `examples/http/client_pool`——运行时的运维面：

```embed path="extlibs/xhttp/examples/http/client_pool/main.c" title="extlibs/xhttp/examples/http/client_pool/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_pool/main.c -lws2_32 -liphlpapi
（示例校验连接池统计与空闲清理路径后正常退出）
```

**刚才发生了什么。** ① `HttpClientStats(pClient, &Stats)` 读取连接池当前量与单调生命周期计数——**各字段不承诺同一全局时刻**（并发读的松一致）；`ActiveConnections` 含拨号/使用/保留配额的连接。② `HttpClientCloseIdle` 只摘空闲——不打断活动调用；生命周期终态还会等异步 Close 与分片 Timer 取消回调（"从 LRU 摘除"≠"完全关闭"——两个时点的区分是这个 API 的诚实之处）。③ 配置面：`xhttpclientpoolconfig` 的空闲/等待/活动上限是部署参数——每 origin 8 条空闲对多数 API 客户端够用；上游连接数受限的服务调低全局上限。统计→调整→再统计是容量循环。

## 契约

- **冻结语义**：`Do` 成功返回前完成复制；提交后原请求可改可毁；`Clone` 差量复制（正文加引用）。
- **两类截止**：`Timeout` 提交起全链；`IdleTimeout` 执行起无进展时长（收发刷新）；零值继承 Client、`TIMEOUT_NONE` 显式关；同刻到达稳定报 TOTAL。
- **取消**：`CallCancel` 任意线程；提交前已取消不装 Timer 不启 DNS 直接终态；终态发布后不可改写。
- **双限额**：`ResponseBodyLimit`（表示正文、解码前、缓存同预算）+ `Decompress.MaxBody`（明文）——高压缩比绕不过预算；SSE 用结构化限额替代下载限额。
- **Info 冻结**：终态字段冻结；时间单调时钟微秒、未到为零；Wire 字段累计重定向链；Body 记最终明文。
- **池分片**：≤32 片固定、片内 FIFO/LRU/Timer、跨片原子配额；回池四条件；无 pipelining；默认 128/8/90s。
- **池运维**：CloseIdle 不打断活动；Stats 松一致可并发；Drain/Abort 走同一路径并等异步收尾。
- **提交验证**：配置范围回绕同步拒绝不发布回调；创建期验 Dial/Stream/Exchange 静态配置。
- **裁剪**：`HTTP_CLIENT_POOL`/`_FUTURE`/`_PREPARE`/`_STREAM` 独立——按用到的面裁剪。

## 避坑

### 坑 1：提交后继续改原请求，以为影响在途调用

症状：`Do` 之后又 `SetHeader`——在途请求"没带上"新头，以为库有 bug。

原因：冻结语义——`Do` 返回成功那一刻快照已定，后续修改属于"下一次提交"。这是特性不是缺陷：否则每次提交都要等调用方宣布"改完了"。

```c bad
pCall = xrtHttpClientDo(pClient, pReq, NULL, on_done, pCtx);
xrtHttpRequestSetHeader(pReq, Extra, Value);  /* 改不动在途的 */
```

```c good
pCall = xrtHttpClientDo(pClient, pReq, NULL, on_done, pCtx);
xrtHttpRequestDestroy(pReq);   /* 原请求使命结束 */
/* 追加需求？新建请求再提交一次——快照语义让并发互不影响 */
```

### 坑 2：只设总超时，慢速拖拽连接拖满额度

症状：总超时 30 秒、源站每 20 秒推一个字节——每个请求都"合法地"耗满 30 秒，吞吐被这类源站拖垮。

原因：总超时只看时长不看进展。空闲超时才是进展检测——"连接活着但没动"10 秒就该放弃。

```c bad
xrtHttpClientConfigInit(&Config);
Config.Timeout = 30000000ull;   /* 只有总超时：一字节/20s 的源站拖满全程 */
```

```c good
xrtHttpClientConfigInit(&Config);
Config.Timeout = 30000000ull;        /* 总预算 30s */
Config.IdleTimeout = 10000000ull;    /* 无进展 10s 即弃 */
/* 传输/收发都刷新空闲——"慢但在动"不受影响，"卡住"快速止损 */
```

### 坑 3：把 Stats 当精确快照做容量决策

症状：用 `ActiveConnections + IdleConnections` 与瞬时 QPS 做除法，结论漂移不定。

原因：Stats 的字段**不承诺同一时刻**——并发读取下各字段来自不同瞬间。容量决策要用**单调生命周期计数**（累计创建/销毁/复用次数）算趋势，而不是瞬时量算比例。

```c bad
double load = (double)Stats.ActiveConnections /
	(Stats.ActiveConnections + Stats.IdleConnections);  /* 瞬时混拍：漂移 */
```

```c good
/* 生命周期计数器单调：两次采样求速率/复用率——稳定可比较 */
double reuse_rate = (double)Stats.ReusedConnections /
	(double)(Stats.CreatedConnections + 1);
```

## 练习

### 基础：构建器全要素装配

用构建器发一条带 `Authorization` 头与 JSON 正文的 POST（对本地或 httpbin 服务），在完成回调打印状态与 `Info.Phase`。验收标准：提交后立即销毁原请求不影响结果；响应正文与 Header 完整可读。

### 进阶：双超时实验

起一个慢速服务（每秒推一字节）与一个卡死服务（接受连接不发数据），分别只设总超时、只设空闲、双设三种配置测量放弃时间。验收标准：三种配置的放弃时间与契约推导一致；能指出 `Info.Phase` 报告的结束阶段差异。

### 挑战：连接复用基准

对同一 origin 连发 100 个请求（并发 8），从 Stats 的生命周期计数统计复用率与握手次数；再换 `ClientConfig` 关闭池（空闲上限零）对比。验收标准：复用率、总耗时、`Info.ReusedConnection` 命中率三组数据自洽；能解释关闭池后时间差来自哪里（TCP+TLS 握手 × 次数）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 构建器 | Create(方法,URL) + SetHeader/SetBytes/SetBody/SetAuth 族；Do 冻结快照 |
| 冻结语义 | 提交后原请求可改可毁；Clone 差量（正文加引用） |
| 两类截止 | Timeout 全链（提交起）；IdleTimeout 无进展（收发刷新）；零继承/NONE 关 |
| 取消 | 任意线程；提交前取消直接终态；终态冻结不可改写 |
| 双限额 | 表示正文（解码前，缓存同预算）+ 明文（解压后）——炸弹两道闸 |
| Info | 终态冻结；微秒单调钟；Wire 累计重定向链；Body 记明文字节 |
| 池分片 | ≤32 固定片；片内 FIFO/LRU/Timer；跨片原子配额无热锁 |
| 回池条件 | 边界完整+无 close+无升级+健康；无 pipelining；默认 128/8/90s |
| 运维口 | CloseIdle 不打断活动；Stats 松一致——容量看单调计数 |
| 内省 | RequestUrl/Method 可读；UrlPort 补默认端口——请求也是结构化持有者 |
