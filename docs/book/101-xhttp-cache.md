---
num: 101
slug: xhttp-cache
title: xhttp 自动缓存：查找、验证与失效
volume: 卷十 扩展库：xhttp
type: practice
lead: Store 挂进配置后的全自动 HTTP 缓存：新鲜度、条件验证、304 合并、Range 组合与 unsafe 失效。
api: xhttp-http_cache, xhttp-http_client
---

## 导读

第 100 章的三个自动行为之后，缓存是最大也最有味道的一个。`http_client_cache` 层把 Store（`xhttpcache`）挂进 Client 配置后，**查找、命中交付、新鲜度判断、条件验证、存储、部分响应组合与 unsafe 方法失效全自动**——你的代码一行不变，第二次请求同一 URL 就从本地出。协议侧完全遵循 RFC 9111：请求的 `Cache-Control`（no-store/no-cache/only-if-cached/年龄约束）与响应的可存储性统一由公共缓存策略解析器处理，客户端不另造近似语义。存储对象保持公开——应用、代理、自定义持久化后端直接用同一套 Store 契约。本章讲透配置、四种调用模式、存储契约与并发条件提交。

## 引入

为什么客户端要内置缓存而不是"业务自己存响应"？三个理由。**正确性**：缓存的可存储性判断（`no-store`/`Vary: *`/认证响应/共享私有规则）、新鲜度计算（max-age/Expires/年龄修正）、条件验证（ETag/Last-Modified 的 304 合并）每条都是 RFC 的精细规则——手写"存个 Map 按时间过期"在真实响应头面前错得五花八门。**组合性**：Range 请求的部分内容组合（本地已有的片段+回源补齐）需要与存储结构协同——不可能外挂。**失效**：unsafe 方法成功后同源相关表示的失效（`Location`/`Content-Location`）是缓存协议的一部分。xhttp 把这套规则实现一次，配置一个 Store 全体受益。

## 概念

### 装配与调用模式

```diagram flow
- 装配：xrtHttpCacheCreate → ClientConfig.Cache.Store → Create 后自己的引用可放
- 请求到达：模式判定（四模式）→ 缓存查找（主键=方法+有效 URI+分区+Vary 声明）
- 命中且新鲜：直接交付（不出网）
- 命中但过期：条件验证（ETag/If-Modified-Since）→ 304 则合并记录交付
- 未命中/禁用：回源 → 响应按可存储性决定是否入 Store
```

调用级四模式：`XHTTP_CLIENT_CACHE_DEFAULT`（遵守请求/响应/存储策略——正常路径）、`DISABLED`（本次绕过读写与失效）、`RELOAD`（强制回源验证或重取，成功响应仍可更新缓存——"刷新"按钮）、`ONLY`（禁网络；无记录时**合成 504**——离线模式）。四种模式与请求头语义叠加而不冲突——`only-if-cached` 请求头由同一策略解析器处理。

### 存储契约：什么进 Store

- **默认私有缓存**；`Shared` 显式切共享语义（字段与授权规则不同）。
- **不进 Store**：`no-store`、不可缓存状态码、`Vary: *`、禁止共享保存的认证/私有响应。
- **启发式寿命**：无显式新鲜度时可按 `Last-Modified` 启用受限启发式——默认 10%、最长一天、可整体关闭（保守缺省，隐私与正确性优先）。
- **按需增长**：编码正文只在真实到达后增长，不为每 Call 预留；`MaxBody`（默认 8 MiB）是单次回源捕获的硬上限——超限停止捕获但**默认 fail-open**（网络响应继续交付，只是不缓存）；小正文 256 B 起步倍增。
- **释放纪律**：存储或重放完成后，候选记录、字段快照与捕获正文在终态发布前释放——不随用户持有的结果引用驻留。

### 条件提交与并发

304 合并、HEAD 元数据更新、Range 片段组合都**不用普通 Put 覆盖**——它们用 Store 的 `Replace`/`RemoveRecord` 当前版本条件：304 提交冲突时（并发更新者赢了），当前请求仍交付自己已验证的快照、缓存保留并发更新者的结果——**不为保存自己的视图丢掉更新**。这套乐观并发与第 18 章容器的"失效规则"同一思想：读后写前别人改了，写条件不满足就各走各路。

### Range 组合与 unsafe 失效

**Range 本地组合**：请求 `Range: bytes=0-99` 而本地已有 `0-199` 的记录——直接从缓存切出交付（不出网）；本地只有片段时，"本地片段+回源补缺"的组合由 `MaxRanges`（默认 16）限制复杂度——排序工作、临时数组与 multipart 放大都有界。**unsafe 失效**：POST/PUT/DELETE 成功响应使目标 URI 失效，连同响应允许失效的同源 `Location`/`Content-Location` 表示——写后读的一致性由协议保证，不是应用提醒。

### 分区键

`PartitionKey` 提交时复制，用于按用户/站点/租户隔离同一 URI——主键含分区。**为什么不能只靠 `Vary`**：`Vary` 靠请求头区分（如 `Vary: Authorization`），但"哪些头该区分"由响应方声明——响应方没声明就共享了；分区键是**调用方主动的隔离**（多租户客户端的底线）。私有数据的隔离归调用方决策，协议工具只是工具。

## 示例

### 第一个完整程序：缓存装配与容量配置

下面的程序来自 `examples/http/client_cache`——Store 挂载与限额：

```embed path="extlibs/xhttp/examples/http/client_cache/main.c" title="extlibs/xhttp/examples/http/client_cache/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_cache/main.c -lws2_32 -liphlpapi
（示例校验缓存装配与回源-命中路径后正常退出）
```

**刚才发生了什么。** ① `xrtHttpCacheCreate(NULL)` 建内存 Store（`NULL` 取默认配置——容量与逐出策略在 `xhttpcacheconfig`）；放入 `ClientConfig.Cache.Store`——Client 创建即持引用。② `Cache.MaxBody = 16 MiB` 覆盖默认 8 MiB——**单次回源捕获上限**是容量规划的锚：它决定"多大的响应值得缓存"，与 Store 总容量（另一个配置）配合。③ 之后的请求代码与无缓存完全一致——命中不出网、过期自动验证、`Info` 的 Wire 字节数为 0 即命中（诊断面识别缓存命中的方式）。④ `HttpClientCache(pClient)` 可借回同一句柄（统计、显式逐出）；不配 Store 时该层完全不进热路径。

### 第二个完整程序：缓存策略巡检

第二个程序来自 `examples/http/cache_policy`——策略解析与判定的协议层：

```embed path="extlibs/xhttp/examples/http/cache_policy/main.c" title="extlibs/xhttp/examples/http/cache_policy/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/cache_policy/main.c -lws2_32 -liphlpapi
（输出策略解析与可存储性判定的自检结果）
```

**刚才发生了什么。** ① 缓存策略是**独立协议模块**（`http_cache_policy`）：请求的 Cache-Control 指令、响应的可存储性、新鲜度与年龄——客户端缓存层只是它的使用者（与第 91 章 Body 复用 Plan、第 97 章压缩复用协商层同一模式）。② 本示例离线验证策略判定：给定请求/响应头，可存储吗？新鲜多久？需要验证吗？——**缓存行为的每一步都是可单测的纯函数**，这是"自动行为不失控"的工程保证。③ 巡检族还包括 `cache_control`（指令解析）、`cache_time`（年龄计算）、`cache_validate`（验证器选择）、`cache_range`（Range 组合）、`cache_store`（存储契约）、`cache_status`（状态头生成）——六个协议模块撑起自动缓存，全部独立可裁剪。

## 契约

- **装配**：Store 进 Client 配置即全自动（查找/交付/验证/存储/组合/失效）；Client 持引用；不配该层不进热路径。
- **四模式**：DEFAULT/Disabled/Reload/Only（无记录合成 504）；与请求头语义由同一解析器统一，不另造近似。
- **主键**：方法+有效 URI+分区键+记录实际声明的 Vary 字段。
- **不存储**：no-store/不可缓存状态/Vary:*/共享禁止的认证私有响应。
- **启发式**：默认 10%·Last-Modified·最长一天·可关闭。
- **限额**：MaxBody 单次回源捕获硬上限（默认 8 MiB）超限 fail-open；MaxRanges 默认 16 限组合复杂度；按需增长零预留。
- **条件提交**：304/HEAD/Range 用版本条件 Replace——冲突时交付自己快照、保留并发更新者。
- **失效**：unsafe 成功失效目标 URI+响应允许的同源 Location/Content-Location。
- **分区**：PartitionKey 提交时复制；私有隔离靠分区不靠 Vary。
- **裁剪**：`HTTP_CLIENT_CACHE` 依赖公共缓存协议族（policy/control/time/validate/range/store/status 独立）。

## 避坑

### 坑 1：把 MaxBody 当 Store 总容量

症状：设了 `MaxBody = 16 MiB` 就以为缓存最多占 16 MiB——运行几小时内存涨到几百 MiB。

原因：`MaxBody` 是**单次回源捕获**上限（这条响应多大还缓存）；Store 的总容量与逐出在 `xhttpcacheconfig`（创建 Store 时的配置）。两个旋钮管两件事。

```c bad
ClientConfig.Cache.MaxBody = 16u * 1024u * 1024u;   /* 以为这就是总容量 */
pCache = xrtHttpCacheCreate(NULL);                    /* 默认总容量没动 */
```

```c good
xhttpcacheconfig CacheConfig;
xrtHttpCacheConfigInit(&CacheConfig);
CacheConfig.MaxBytes = 64u * 1024u * 1024u;   /* Store 总容量+逐出 */
pCache = xrtHttpCacheCreate(&CacheConfig);
ClientConfig.Cache.Store = pCache;
ClientConfig.Cache.MaxBody = 16u * 1024u * 1024u;   /* 单次捕获上限 */
```

### 坑 2：多用户客户端共用无分区缓存

症状：用户 A 请求过的私有资源，用户 B 同 URL 请求直接命中——**跨用户数据泄漏**。

原因：缓存主键默认不含用户维度；`Vary: Authorization` 只有响应方声明了才生效——响应方没声明就共享了。多用户/多租户必须给分区键。

```c bad
/* 全体用户一个 Client 一个 Store，无分区 */
pClient = xrtHttpClientCreate(pEngine, &Config);   /* B 命中 A 的私有响应 */
```

```c good
/* 每用户（或每租户）一个分区键——主键含分区 */
xrtHttpCallOptionsInit(&Options);
Options.Cache.PartitionKey = user_partition(user);
pCall = xrtHttpClientDo(pClient, pReq, &Options, on_done, pCtx);
/* 私有隔离是调用方主动决策——工具不替你猜 */
```

### 坑 3：把 ONLY 模式的 504 当网络错误处理

症状：离线模式（`CACHE_ONLY`）下无记录返回 504——代码按"网关故障"走告警路径。

原因：504 是**合成的本地语义**："禁止出网且无缓存"——它不是网络错误，是离线策略的确定性结果。处理应走"离线无内容"分支（提示/降级 UI），不是重试或告警。

```c bad
if ( status == 504 ) {
	alert_gateway_down();   /* 离线命中被当网络故障 */
}
```

```c good
if ( offline_mode && status == 504 ) {
	show_offline_empty(url);   /* 本地合成 504：离线且无缓存 */
}
```

## 练习

### 基础：命中与回源对拍

对本地服务同一 URL 连发两次：对比第二次的 `Info.ResponseWireBytes`（应为 0——命中）与响应头里的年龄信息。再 `RELOAD` 模式第三次。验收标准：三段的 Wire 字节数与模式语义一致；命中响应与首源响应正文逐字节一致。

### 进阶：条件验证观察

服务端返回带 ETag 的响应；缓存过期后（或强制 RELOAD）再次请求——用抓包或服务端日志确认第二次请求携带条件头、响应为 304、客户端交付的是**合并后的完整表示**（状态码 200 不是 304）。验收标准：调用方永远看不到内部 304（契约验证）；命中正文与首源一致。

### 挑战：离线优先客户端

实现 `fetch_offline_fallback(Url)`：先 `CACHE_ONLY`（命中即用）；504 时回退正常模式出网并把结果写缓存。断网测试整个流程。验收标准：有缓存时断网可用；无缓存时明确离线错误（区分"断网"与"服务器 5xx"）；缓存写入遵守响应的可存储性（no-store 响应不入库——可从后续离线行为验证）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 装配 | Store 进 Client 配置即全自动；Client 持引用；不配零开销 |
| 四模式 | DEFAULT / Disabled / Reload（强源仍可更新）/ Only（无记录合成 504） |
| 主键 | 方法+有效 URI+分区+Vary 声明；分区是调用方隔离手段 |
| 不存储 | no-store/不可缓存状态/Vary:*/共享禁的认证私有 |
| 启发式 | Last-Modified 10%·上限一天·可关（保守缺省） |
| 限额两旋钮 | MaxBody=单次捕获（超限 fail-open）；Store 配置=总容量+逐出 |
| 条件提交 | 304/HEAD/Range 用版本条件 Replace；冲突保留并发更新者 |
| Range 组合 | 本地片段+回源补齐；MaxRanges 默认 16 限复杂度 |
| 失效 | unsafe 成功→目标 URI+同源 Location/Content-Location |
| 协议族 | policy/control/time/validate/range/store/status 独立可裁剪可单测 |
