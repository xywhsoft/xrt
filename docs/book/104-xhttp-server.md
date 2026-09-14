---
num: 104
slug: xhttp-server
title: 服务端（上）：启动、连接与路由
volume: 卷十 扩展库：xhttp
type: practice
lead: 五段超时防护、Headers/Body/Request 事件链、Reply 直答路径与 Drain 收尾——建立在 TCP 聚合上的 HTTP/1 服务运行时。
api: xhttp-http_server, xhttp-http_server_router, net
---

## 导读

客户端六章（98–103）之后，视角翻转：现在你是服务端。`xrtHttpServerStart` 一步把 Engine、聚合 TCP Server、Stream 与无 I/O 协议状态机组装成明文 HTTP/1 服务——不通过函数表隐藏依赖（裁剪诚实）。本章讲三件事：**配置面**——五段超时（Header/Body/Request/Idle/Write——每段独立保护一个阶段，零值关闭）；**事件链**——`Open → Headers → Body → Request → Error/Close`，其中 `Headers` 在读正文前做策略决策（缓冲/流式/丢弃/拒绝/直接响应），`Body` 只收流式片段且回调 false 即 500；**响应三路**——`Reply`（固定字节的直答路径，零临时容器）、`ReplyBody`（正文来源版）、`Respond`（动态字段与 trailer 的完整构建器）。路由与中间件是第 105 章，SSE/流式在第 107/108 章。协议状态机解析请求头的底层入口正是第 90 章解析器的网络绑定（http1_net——缓冲链直接进解析，无手工拼接）。

## 引入

服务端与客户端最大的范式差异在**控制权反转**：客户端"发起并等待"，服务端"被事件驱动"——请求什么时候来、来多少、带不带正文、是不是恶意慢速，全部由对端决定。运行时的设计围绕这个反转展开：五段超时各防一种"慢速攻击"（慢头发 HeaderTimeout、慢正文 BodyTimeout、应用卡死 RequestTimeout、挂着不用 IdleTimeout、响应不动 WriteTimeout）；正文策略在 Headers 阶段决定——路由还没走你就得说清"这个请求的正文怎么办"；应用背压（`PauseRequestBody`）让你在异步消费者跟上之前**不读对端的正文**——TCP 窗口自然收缩，内存不涨。

`Reply` 直答路径值得单独一句：固定 JSON/文本响应**不创建临时 Reply 或 Header 容器**、不要求构造字典或 JSON 对象——一个调用进冻结内核。健康检查、短错误、微服务间固定应答这些"每个服务都有"的路径，拿到的是零开销形态。

## 概念

### 启动与配置

`xrtHttpServerStart(Engine, 配置, 事件表)` 创建并立即启动；**启动失败不留部分可用的 Server**——全部逻辑端点要么都绑定成功要么整体失败（多端点配置见 Network 节）。配置要点：

- **五段超时**（微秒）：`HeaderTimeout`（读头阶段）、`BodyTimeout`（读正文）、`RequestTimeout`（应用处理时长）、`IdleTimeout`（keep-alive 空闲）、`WriteTimeout`（响应无进展）——零值关闭对应保护。慢速攻击的每一段都有闸。
- **连接与限额**：`MaxConnections`（零=不加应用层上限）、`MaxInformations`（单请求排队的信息响应数）、`WriteSize`（一次零复制发送租约——**不为每连接预留固定发送缓冲**）；接收内存由 TCP 的按需 `xnetbuf` 与硬限额管理。
- **Network**：完整暴露 TCP Server 的多端点、共享动态端口、accept 队列、reuse-port——`xrtHttpServerLocal` 取真实端点（监听端口填 0 的标准姿势，与第 67 章同款）；`xrtHttpServerNetwork` 借出底层 `xnetserver` 引用（特殊场景用低层统计，用完 `xrtNetServerDestroy`）。

### 事件链与正文策略

```diagram flow
- Open：地址、Worker、底层 TCP 连接事实
- Headers：读正文前的决策点——缓冲 / 流式 / 丢弃 / 拒绝 / 直接响应
  （无正文请求继续；有正文请求停在头边界等策略）
- Body：流式模式才触发——借用正文片段；返回 false=回调错误+500
  （已提交响应则响应优先；PauseRequestBody 暂停交付）
- Request：完整请求到达——同步响应或保留 Connection 稍后投递
- Error → Close：每连接至多一次稳定错误、恰一次终态；Shutdown=全部回收完成
```

三处关键语义。① **Headers 的策略权**：按路由调 `xrtHttpConnSetRequestBodyLimit`（当前 Exchange 的唯一硬上限）——上传限额是路由级决策不是全局常数。② **Body 的背压**：`PauseRequestBody` 只能在 Body 回调内调——当前片段回调返回即失效（要留自己复制）；异步消费者完成后**任意线程** `ResumeRequestBody`（Connection 内嵌命令恢复，不分配任务节点）；暂停期间 BodyTimeout 继续计时——失联消费者不能无限占住连接。③ **响应唯一性**：一个请求恰好一次最终提交（Respond/Reply/ReplyBody 三选一）；`xrtHttpConnInform` 可先发 1xx 信息响应（101 除外）。

### 响应三路与冻结内核

- `xrtHttpConnReply(连接, 状态, 类型, 字节)`——固定响应直答：复制为紧凑 Body 直进 HTTP/1 冻结内核，零临时容器。健康检查的标准形态。
- `xrtHttpConnReplyBody(连接, ..., 可选 xhttpbody 引用)`——同一直答路径的正文来源版：Borrow/Take/Reference/文件/生产者避免复制；未知长度按 chunked（1.1）或关闭分帧（1.0）。调用返回后调用方即可销毁自己的 body 引用。
- `xrtHttpConnRespond`——动态字段、trailer 的完整构建器路径（第 99 章请求构建器的镜像）。

### 生命周期：Drain 与 Shutdown

`xrtHttpServerDrain` 优雅排空：不再接受新连接、在途请求完成、连接依次关闭；`Shutdown` 事件在**全部**协议对象、传输所有权、运行时退出且已受理的 Upgrade 交接回调返回后发布——"没有东西还在跑"是事件保证的，不是猜的。自旋等 `XHTTP_SERVER_CLOSED` 的收尾模板与第 67 章 TCP 一致。

## 示例

### 第一个完整程序：健康检查服务

下面的程序来自 `examples/http/server`——最小可用的完整服务：

```embed path="extlibs/xhttp/examples/http/server/main.c" title="extlibs/xhttp/examples/http/server/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/server/main.c -lws2_32 -liphlpapi
listening on http://127.0.0.1:52173/health
press Enter to drain
```

**刚才发生了什么。** ① 装配三件：Engine 启动、`HttpServerConfigInit`（五段超时取默认——保护全开的保守缺省）、`HttpServerEventsInit` + 两个回调（Request/Error）。② 监听地址 `NetAddrLoopback` + **端口 0**（系统分配）——`xrtHttpServerLocal` 取回真实端点拼成 URL 打印——与第 67 章 Listener 同款姿势在 xhttp 的再现。③ **Request 回调就是业务全部**：Target 等于 `/health` 就 `HttpConnReply` 直答 JSON（一个调用、零容器）；否则 404 直答——两条路径都是直答形态。④ 回车触发 `Drain`：优雅排空 → 自旋等 `XHTTP_SERVER_CLOSED` → 销毁——**服务的完整生命周期在一个 main 里走完**，这就是教学样本的姿态。

### 第二个完整程序：路由器装配

第二个程序来自 `examples/http/server_router`——路由与直答的组合：

```embed path="extlibs/xhttp/examples/http/server_router/main.c" title="extlibs/xhttp/examples/http/server_router/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/server_router/main.c -lws2_32 -liphlpapi
（输出路由装配计数并正常退出）
```

**刚才发生了什么。** ① `xrtHttpServerRouterCreate` 建路由器；`xrtHttpServerGet(路由器, 路径, 处理器, 数据)` 注册方法+路径的处理器——PUT/POST/通配同族；`RouterFreeze` 冻结为只读共享表（与第 99 章"提交即冻结"同一思想：热路径分派不碰可变结构）。② 冻结后的路由表由 Server 的事件链分派：请求到达 → 匹配 → 处理器收到 `xhttprouteparam` 参数数组（路径参数如 `/items/{id}` 的 id）；未匹配走 404 自动路径。③ 路由级回调与本章 Request 回调同一形态——`xhttpserverrequest` 快照 + `HttpConnReply/Respond` 响应；中间件（`xrtHttpServerUse`）叠在路由外层——第 105 章展开。

## 契约

- **启动原子性**：全部逻辑端点同时绑定成功，失败不留部分 Server；地址范围打开前验证；限额静态无分配验证。
- **五段超时**：Header/Body/Request/Idle/Write 各护一段，微秒单位，零值关闭；慢速攻击每段有闸。
- **事件串行**：全部应用事件在连接所属 Worker 串行执行；Error 至多一次（稳定错误+cause 链）、Close 恰一次；传输失败固定 Error→Close 顺序。
- **Headers 策略**：缓冲/流式/丢弃/拒绝/直接响应五选；正文限额路由级（SetRequestBodyLimit 改当前 Exchange 唯一硬上限）。
- **Body 背压**：Pause 仅 Body 回调内；片段回调返回即失效；Resume 任意线程（内嵌命令零分配）；暂停期 BodyTimeout 计时。
- **响应唯一**：一次最终提交；Inform 可发 1xx（101 除外）；信息与最终响应同背压线路。
- **直答路径**：Reply/ReplyBody 进冻结内核零临时容器；body 引用提交后调用方可销毁；未知长度 chunked/关闭分帧。
- **生命周期**：Drain 优雅排空；Shutdown 在全部回收（含 Upgrade 交接）后发布；Closed 终态自旋等待。
- **裁剪**：`XHTTP_FEATURE_HTTP_SERVER` 依赖 Exchange/Response/TCP Server；流式出站、文件、TLS、Upgrade 是独立组合层——核心无第二状态机。

## 避坑

### 坑 1：Body 回调里把片段视图存起来异步处理

症状：异步消费者读到的片段数据错乱或崩溃——与第 96 章同款的悬空视图。

原因：Body 片段是**借用**——回调返回即失效。异步消费要么复制，要么用 Pause/Resume 让运行时等你。

```c bad
static bool onBody(..., xbytesview Chunk, ...) {
	queue_push(g_Queue, Chunk);      /* 存视图：返回即悬空 */
}
```

```c good
static bool onBody(xhttpconn* pConn, ..., xbytesview Chunk, ...) {
	if ( !consumer_ready() ) {
		copy_and_queue(Chunk);                 /* 留就复制 */
		xrtHttpConnPauseRequestBody(pConn);    /* 暂停后续交付 */
		return true;
	}
	consume_now(Chunk);                          /* 同步消费零复制 */
	return true;
}
/* 异步消费者完成后任意线程：xrtHttpConnResumeRequestBody(pConn) */
```

### 坑 2：慢客户端没设 WriteTimeout

症状：一个下载到一半断网（不发 RST）的客户端把连接与缓冲占住数小时——连接数被这类僵尸耗尽。

原因：WriteTimeout 默认值可能未覆盖你的部署形态（零值=关闭保护）。响应阶段的"无进展"必须有时限。

```c bad
xrtHttpServerConfigInit(&Config);
/* WriteTimeout 用默认/零——僵尸连接永久占额 */
```

```c good
xrtHttpServerConfigInit(&Config);
Config.WriteTimeout = UINT64_C(30000000);   /* 响应 30s 无进展即弃 */
Config.IdleTimeout = UINT64_C(75000000);    /* keep-alive 75s */
/* 五段全显式——保护是部署契约不是运气 */
```

### 坑 3：在 Request 回调里做长阻塞操作

症状：Worker 被阻塞——同 Worker 的其他连接全部卡住（第 64 章 Worker 经济学的服务端代价）。

原因：事件在 Worker 串行执行；回调里的阻塞（同步文件 IO、外部调用）就是拿整条 Worker 做抵押。RequestTimeout 只是止损不是解法。

```c bad
static void onRequest(..., const xhttpserverrequest* pReq, ...) {
	char* p = blocking_external_call(...);   /* 阻塞 2s：Worker 全停 */
	reply(...);
}
```

```c good
static void onRequest(xhttpserver* pS, xhttpconn* pC,
		const xhttpserverrequest* pReq, ptr pD) {
	submit_to_pool(pC, pReq);   /* 转交执行池（第 59 章），保留 Connection */
	/* 池内完成后投递回所属 Worker 再 Respond——RequestTimeout 兜底 */
}
```

## 练习

### 基础：五段超时实验

基于 server 示例逐项验证：慢头发（每秒 1 字节的头）、慢正文、不回读客户端——分别观察对应超时的触发与连接关闭。验收标准：三段各自的拒绝时间与配置一致；错误回调收到结构化错误与阶段信息。

### 进阶：路由与参数

用 Router 实现 `/items/{id}` 与 `/items/{id}/parts/{part}` 两级路径参数，Request 打印参数值；加 PUT 处理器。验收标准：路径参数按序提取；未匹配 404；方法不匹配 405。

### 挑战：上传限额网关

Headers 回调按路由设置正文限额（`/avatar` 2 MB、`/video` 200 MB、其他 1 MB）；Body 流式接收并统计字节；超限请求在正文阶段拒绝（413）且连接干净关闭。验收标准：三种限额各自生效；超限连接不占内存（限额在消耗前拦截）；正常上传完整接收。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 启动 | Start 一步建明文服务；失败不留部分 Server；Local 取动态端口端点 |
| 五段超时 | Header/Body/Request/Idle/Write 各护一段；微秒；零值关 |
| 事件链 | Open→Headers（策略点）→Body（流式片段）→Request→Error/Close；Worker 串行 |
| Headers 五策 | 缓冲/流式/丢弃/拒绝/直接响应；限额路由级 |
| Body 背压 | Pause 仅回调内、片段即失效；Resume 任意线程零分配；暂停期超时计 |
| 响应唯一 | 一次最终提交；Inform 发 1xx；三路：Reply/ReplyBody/Respond |
| 直答路径 | Reply 零容器直进冻结内核；body 引用提交后可销毁 |
| 生命周期 | Drain 排空；Shutdown 在全部回收后发布；Closed 自旋等 |
| Network | 多端点/动态端口/reuse-port 全暴露；Network 借出底层引用 |
| 裁剪 | Server=Exchange+Response+TCP Server；流式/文件/TLS/Upgrade 独立层 |
