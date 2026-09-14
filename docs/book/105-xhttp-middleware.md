---
num: 105
slug: xhttp-middleware
title: 服务端（中）：中间件与静态文件
volume: 卷十 扩展库：xhttp
type: practice
lead: 注册顺序进、逆序退的洋葱模型、Next 的同步栈语义、静态文件与目录服务——横切关注点的标准装配。
api: xhttp-http_server_middleware, xhttp-http_server_static, xhttp-http_server_router
---

## 导读

业务路由之外的东西——日志、计时、鉴权、CORS、压缩——每个请求都要过一遍。`xrtHttpServerUse` 把它们注册为**中间件**：按注册顺序进入、按逆序退出（洋葱模型），`xrtHttpServerNext` 同步进入下一层、返回后可执行后置逻辑（计时/日志收尾）；**不调用 Next 就是短路**——中间件可以直接响应或保留 Connection 异步处理。契约的关键细节：`Next` 只在当前同步回调栈内有效（不能存进线程/Future/协程）、最多调一次（重复=错误路径）——**异步中间件应短路并自己保留 Connection，而不是延迟 Next**。本章后半讲静态文件层：单文件响应、目录映射与沙箱约束（`http_server_static`——第 47 章 dir 沙箱思想的服务端落地）。

## 引入

洋葱模型的价值在"前置+后置"成对：日志中间件前置记起点、调 Next、后置记终点与状态——一层代码覆盖请求的全生命周期。但异步打破了朴素洋葱：如果中间件要"查个缓存再决定放行"（异步查询），Next 什么时候调？契约的答案是**别调**——短路并保留 Connection，异步完成后自己响应或手动分派到最终路由。这条纪律来自一个技术约束：`Next` 是同步栈帧的句柄——保存到别处再调就是重入状态机（第 86 章 Select 回调同款禁令）。

静态文件是另一类横切：几乎所有服务都要"发个文件"——favicon、前端构建产物、下载归档。手写"读文件+猜类型+回 Range"是每个 C 服务员的传统节目，`http_server_static` 把它标准化：单文件、目录前缀映射、Range 协商（第 91 章 Range 的服务端形态）一次到位。

## 概念

### 注册与洋葱

```diagram flow
- 注册：Use(中间件A) → Use(B) → Get(路由)（先中间件后路由）→ RouterFreeze 冻结
- 请求：A 前置 → B 前置 → 路由处理器 → B 后置 → A 后置（逆序退出）
- Next：同步进入下一层；返回=下一层分派已返回（非 I/O 完成）
- 短路：不调 Next——中间件直接响应或保留 Connection 异步
```

两种注册：`Use`（数据调用方管理）与 `UseOwned`（注册成功后 `Release(Data)` 责任移交 Router——最后一个引用销毁时按注册逆序释放）。**回调返回值**：`true`=当前层接受请求；`false`=不可恢复应用错误——运行时尽力提交固定 500，响应已提交则异常关闭。中间件数量受 Router 的 `MaxRoutes` 限额约束；注册扩容原子（失败不改可见数量、不转移清理责任）。

### Next 的精确语义

三个容易误解的点。① **Next 返回 ≠ 响应写完**：返回只表示下一层分派返回——同步直答通常已进 `XHTTP_CONN_RESPONSE` 状态，异步路由可能仍在等待；统计完整响应耗时要用 Connection 的响应完成路径。② **栈内有效**：`xhttpservernext` 只在当前同步回调栈内有效——保存到其他线程/Future/协程再调是未定义行为；重复调用设 `XHTTP_SERVER_MIDDLEWARE_ERROR_NEXT` 进失败路径。③ **参数透传**：匹配路由时中间件收到的 `Params/Count` 与最终路由完全相同（未命中为空）——中间件能看到路由参数（日志里带 id）。

### 与 404/405/OPTIONS 的协作

统一日志和错误处理中间件要能看见 404/405 与自动 OPTIONS——启用至少一层中间件时，**未命中请求的正文先丢弃再进完整请求链**（日志层看到的是完整请求形态）；未启用中间件时未命中在 Header 阶段直接响应并拒绝正文（省事的快速路径）。启用与否改变的是"未命中的处理深度"——这正是"中间件接管横切"的兑现。

### 静态文件层

`http_server_static` 提供文件到路由的映射：

- **单文件**：`xrtHttpConnFile` 族——一个路径对一个文件（favicon、robots 这类），Range 变体 `xrtHttpConnFileRange` 同层。
- **目录映射**：URL 前缀 ↔ 磁盘目录——`/static/*` 映射到构建产物目录；路径穿越防护（`../` 攻击在映射层拦截——第 47 章 dir 沙箱思想）。
- **协商**：Content-Type 按扩展名（MIME 数据库）、ETag/Last-Modified 与条件请求（304 路径）、**Range 请求**（`http_cache_range` 的服务端镜像——断点续传）。
- **响应路径**：文件响应走 `ReplyBody` 的文件正文来源（第 104 章直答路径）——零复制发送（SendFile 族在第 68 章讲的内核直传在传输层兑现）。

### 分派路径的统一

固定 Router 与 Host Mux（按 Host 头分流的复用器——一台服务多域名）使用**同一条分派路径**——中间件表与路由表一起冻结只读共享。热路径无锁分派（第 99 章连接池分片的同款性能哲学）。路由模式的 `{name}` 段捕获语法（`http_route.h`）与第 30 章表亲 pattern 的 `{name}` 捕获同源——冻结即"一次编译、热路径只查表"的那条纪律在路由层的落点。

## 示例

### 第一个完整程序：中间件 + 路由的完整装配

下面的程序来自 `examples/http/server_middleware`——日志中间件包裹健康路由：

```embed path="extlibs/xhttp/examples/http/server_middleware/main.c" title="extlibs/xhttp/examples/http/server_middleware/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/server_middleware/main.c -lws2_32 -liphlpapi
middleware: 1, routes: 1
```

**刚才发生了什么。** ① 装配顺序即洋葱顺序：`Use(日志中间件)` 先注册、`Get("/health", 路由)` 后注册、`RouterFreeze` 冻结——**中间件永远在路由外层**（注册顺序即包裹顺序）。② 日志中间件的形态模板：前置打印 `> 方法 目标` → `xrtHttpServerNext(pNext)` 进下一层（健康路由直答 JSON）→ 返回后打印 `< state`——`xrtHttpConnState` 显示响应已提交（同步直答路径）。③ 输出 `middleware: 1, routes: 1` 回显装配事实——`MiddlewareCount`/`RouterCount` 是配置自省（与第 99 章 Stats 同款的"配置可验证"姿态）。④ 真实服务里这个 Router 挂进 `xhttpserverconfig` 的分派配置——本章示例聚焦装配层所以不启网络。

### 第二个完整程序：静态文件服务

第二个程序来自 `examples/http/static_file`——文件响应的标准装配：

```embed path="extlibs/xhttp/examples/http/static_file/main.c" title="extlibs/xhttp/examples/http/static_file/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/static_file/main.c -lws2_32 -liphlpapi
（输出静态服务装配结果并正常退出）
```

**刚才发生了什么。** ① 静态层把"路径→文件"的映射装进路由体系——单文件注册（favicon 类）与目录前缀映射（`/static/*`）各有入口；扩展名到 MIME 的判定内建。② 文件响应走第 104 章 `ReplyBody` 的文件来源——不读进内存、传输层零复制（大文件服务的内存形态与文件大小无关）。③ 条件请求（If-None-Match/If-Modified-Since）与 Range 由层内协商——304 与 206 是自动路径；`examples/http/static_path`（目录映射与穿越防护）、`static_multipart_body`（本地组合的 multipart 响应）、`server_file`（运行中服务形态）覆盖静态族的其余面。

## 契约

- **洋葱顺序**：注册序进、逆序退；先中间件后路由；冻结后只读共享（固定 Router 与 Host Mux 同一分派路径）。
- **Next 语义**：同步栈内有效、至多一次；返回=分派返回≠I/O 完成；重复=错误路径。
- **短路**：不调 Next 即短路——直接响应或保留 Connection 异步；异步中间件必须短路而非延迟 Next。
- **回调返回**：true=接受；false=不可恢复错误→尽力 500（已提交则异常关闭）。
- **参数透传**：中间件见到的 Params 与最终路由一致；描述符与 Next 仅借当前栈。
- **未命中深度**：启用中间件时未命中正文先丢弃再进链（日志可见 404/405/OPTIONS）；未启用时 Header 期直接响应。
- **限额与原子性**：中间件数受 MaxRoutes 限；注册扩容原子；UseOwned 按注册逆序释放。
- **静态层**：单文件/目录映射/穿越防护/MIME/条件请求/Range 内建；文件响应走零复制路径。
- **裁剪**：`HTTP_SERVER_MIDDLEWARE`/`HTTP_SERVER_STATIC`/`HTTP_SERVER_FILE` 独立宏。

## 避坑

### 坑 1：把 Next 存起来异步再调

症状：偶发状态机错乱或 `MIDDLEWARE_ERROR_NEXT`——异步完成回调里调了早前保存的 Next。

原因：`xhttpservernext` 是同步栈帧的句柄。异步的正确形态是短路+自己管连接——不是把同步句柄带到别处。

```c bad
static bool onMiddleware(..., xhttpservernext* pNext, ...) {
	/* 想异步查缓存后继续 */
	g_SavedNext = pNext;               /* 存句柄：栈帧早已消亡 */
	async_lookup(later_call_next);     /* 之后调：未定义行为 */
}
```

```c good
static bool onMiddleware(xhttpserver* pS, xhttpconn* pC,
		const xhttpserverrequest* pReq, const xhttprouteparam* pP,
		size_t iP, xhttpservernext* pNext, ptr pD) {
	/* 短路：不调 Next；保留连接异步决策 */
	async_cache_lookup(pC, pReq);
	return true;   /* 本层接受——响应由异步路径提交 */
}
```

### 坑 2：把 Next 返回当响应完成计时

症状：日志里的耗时全是亚毫秒——因为 Next 返回时异步路由还在等待，"耗时"量的只是分派栈。

原因：Next 返回=下一层分派返回；响应字节何时写完是另一回事。完整计时要用 Connection 的响应完成路径（Close/响应排空事件）。

```c bad
uint64_t t0 = now();
Next(pNext);
log_duration(now() - t0);   /* 量的是栈时间不是请求时间 */
```

```c good
/* 前置记起点（按请求标识存）；响应完成回调里配对计时 */
on_request_start(pConn);    /* 记录 */
Next(pNext);
/* 完成时刻在响应完成路径（如 Close 前的最终状态）对账 */
```

### 坑 3：静态目录映射忘了穿越防护的边界

症状：`/static/../../etc/passwd` 类请求真的读到了系统文件——自写映射用字符串拼接路径，没做规范化校验。

原因：路径拼接前必须做"规范化后仍在根内"的校验——这正是 `http_server_static` 目录映射内建的防护；绕开层自己拼路径就把防护也绕开了。

```c bad
snprintf(Path, "%s/%s", RootDir, UrlPath);   /* 无校验拼接 */
send_file(Path);                              /* ../ 穿越直达 */
```

```c good
/* 用静态层的目录映射：穿越防护/规范化内建 */
/* 或自管时：先规范化（UrlPathPathNormalize，第 102 章）再验前缀 */
if ( !path_inside_root(RootDir, Normalized) ) {
	reply_404();
}
```

## 练习

### 基础：三层洋葱验证

注册日志、计时、CORS 三个中间件包裹一个路由——在每层前后打印标记，观察进出顺序。验收标准：输出序列严格符合"注册序进、逆序退"；任一层短路后内层不再执行。

### 进阶：鉴权中间件

实现 `auth_required` 中间件：查 `Authorization` 头，无/无效则直接回 401（短路）；有效解析令牌放入请求上下文传递给路由。对照第 106 章验证方案族。验收标准：受保护路由无令牌 401、有令牌可达；中间件短路时路由不执行。

### 挑战：带缓存的静态服务

目录映射 + 中间件协商：ETag 中间件（首响生成、条件请求比对 304）+ 静态层 Range。用 curl 验证三种路径（首次 200、再请求 304、断点续传 206）。验收标准：三类响应状态与字节正确；304 无正文；Range 片段与文件偏移一致。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 洋葱模型 | 注册序进、逆序退；先中间件后路由；Freeze 只读共享 |
| Next 语义 | 同步栈内有效、至多一次；返回=分派返回≠I/O 完成 |
| 短路 | 不调 Next——直接响应或保留连接异步；异步中间件必须短路 |
| UseOwned | Release 责任移交 Router；末引用销毁按注册逆序释放 |
| 回调返回 | true 接受 / false 不可恢复→尽力 500 |
| 参数透传 | 中间件与路由见同一 Params；描述符仅借当前栈 |
| 未命中深度 | 有中间件：正文先丢弃再进链；无中间件：Header 期直答 |
| 静态层 | 单文件/目录映射/穿越防护/MIME/条件请求/Range；零复制响应 |
| 分派统一 | 固定 Router 与 Host Mux 同一路径；热路径无锁 |
| 裁剪 | MIDDLEWARE/STATIC/FILE 独立宏 |
