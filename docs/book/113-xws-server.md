---
num: 113
slug: xws-server
title: xws：服务端路由与会话
volume: 卷十一 其他扩展库
type: practice
lead: 一条注册同时服务明文与 TLS 的固定端点、Origin 策略与授权回调、升级的分阶段底层 API、协程会话形态——xws 收官。
api: xws-websocket_server_router, xws-websocket_http, xhttp-http_server_router
---

## 导读

xws 四章收官。前面三章给了连接（109）、发送（110）、分组（111）；本章把它们接到 HTTP 服务端：**固定 WebSocket 端点**（`xrtWsServerRoute`——一条注册挂在第 105 章的 HTTP Router 上，`ws` 与 `wss` 由启动方式决定、**不需要第二套注册**）；**安全默认**（Origin 策略——浏览器跨域攻击的第一道闸；Authorize 回调——业务授权点）；**升级底层**（`xrtWsServerCheck/Reply/Reject/Upgrade` 分阶段 API——固定端点不够用时直接在 HTTP 路由回调里组装）；**会话形态**（`session_channel_coroutine`——连接+Channel+协程把每连接业务写成顺序代码，第 57 章与本章的汇合点）。端点的自动响应（405/403/400/426/500）覆盖协议错误的全部分类。

## 引入

WebSocket 服务端的“入口工程”比看起来多：请求到达时先过 HTTP 层（方法必须 GET、不得有正文声明）；然后 Origin 校验（浏览器同源策略的服务端执行——缺失/伪造 Origin 的跨域连接是 CSRF 的 WebSocket 变体）；然后业务授权（这个用户能进这个频道吗）；然后子协议协商（第 94 章）；然后升级握手；最后才是你的 Open 回调拿到 `xwsconn`。固定端点把这条流水线标准化——**默认安全**（同源策略开箱即用）、**错误自动**（每类失败有规范响应码）、**生命周期清晰**（Open 借用连接、要保存必须 `xrtWsConnRef`——第 112 章 Add 持引用的上游）。

何时下沉到底层 API？契约的清单很实际：需要鉴权自定义响应（403 之外的形式）、异步授权、按路径参数创建每连接状态、自定义握手字段——这些“框架型”需求固定端点不做（避免第二套协议状态机），用 `ServerCheck/Reply/Reject/Upgrade` 在普通 HTTP 路由回调里分阶段组装。

## 概念

### 固定端点：注册与自动行为

```diagram flow
- 注册：WsServerRouteConfigInit（默认限制+空事件表+同源策略）
  → 填 Protocols/Events/Open/Data/Release → WsServerRoute(Router, 路径, &配置)
- 请求到达：Header 阶段拒方法错与正文声明（不读非法正文）
- 校验：Origin 策略 → Authorize 回调（可选）→ 握手协议/版本
- 自动响应：非 GET→405+Allow / 授权败→403 / 协议错→400 / 版本错→426+13 / 内部→500
- 升级成功：Open 借用 xwsconn——要保存必须 ConnRef；回调返回后适配器释放交付引用
```

**注册即复制**：`WsServerRoute` 只在注册前读一次固定配置，复制配置、事件与子协议列表——注册后调用方修改/释放配置与字符串不影响端点（第 99 章冻结语义的注册版）。**明密一体**：Router 经 `StartTls` 启动时同一条路由自动接管 TLS 流建 `wss` 端点——注册代码零改动。

### Origin 策略：三档默认

`XWS_SERVER_ORIGIN_SAME_HOST_OR_ABSENT`（默认）：原生客户端可省略 Origin，浏览器提供时必须与当前请求的 scheme/host/有效端口一致——**兼容原生、严管浏览器**的务实默认。`SAME_HOST`：连缺失也拒——只信浏览器的严格场景。`ANY`：完全放开——**只适合已在外层完成来源校验的组合**（网关已验、内网隔离）。Origin 是 WebSocket 版 CSRF 防线——第 85 章“默认安全”的端点层兑现。

### 授权与生命周期

`Authorize` 在协议握手与 Origin 通过后运行：读完整请求、路由参数与协商结果——返回 false 由路由器统一回 403（自定义拒绝形态走底层 API）。生命周期三回调：`Open`（借用连接——保存需 `ConnRef`）、`Error`（只观察同步握手与异步升级错误，**不应在此再提交响应**——它已是“响应之后”的阶段）、`Release`（`Data` 的唯一释放点：Router **和全部已升级连接**都不再使用后恰好执行一次——不是 Router 销毁时！在途连接延长 Data 寿命）。错误域用 `XWS_SERVER_ROUTER_ERROR_AUTHORIZATION` 区分 Origin 拒绝与业务授权拒绝——审计能分清“谁拒的”。

### 分阶段底层：Check/Reply/Reject/Upgrade

固定端点之下是四个独立入口：`xrtWsServerCheck`（校验请求可升级性——Origin/版本/协议）、`Reply`（构造 101 应答）、`Reject`（构造拒绝响应——可自定义形态）、`Upgrade`（执行升级产连接）。它们可在**普通 HTTP 路由回调**里自由组合——鉴权查库（异步）后Upgrade、按路径参数给每连接状态、自定义 426 应答……第 105 章洋葱模型的 WebSocket 特化层。

### 会话形态：Channel+协程

`session_channel_coroutine` 示例展示每连接业务的**顺序化写法**：连接事件投递到 Channel（第 57 章）、消费者协程从 Channel 取事件顺序处理——回调地狱变成直线代码。收尾顺序是模板：关 Channel → 关调度器 → Run 排空 → 销毁协程/调度器 → `ChannelDrain` 清残留——第 60 章结构化并发的 WebSocket 会话版。

## 示例

### 第一个完整程序：一条注册的固定端点

下面的程序来自 `examples/websocket/server_router`——最小可编译注册：

```embed path="extlibs/xws/examples/websocket/server_router/main.c" title="extlibs/xws/examples/websocket/server_router/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/server_router/main.c -lws2_32 -liphlpapi
WebSocket route /chat is ready
```

**刚才发生了什么。** ① 第 105 章的 `xhttpserverrouter` 是载体——WebSocket 端点是挂在它上面的**路径端点**：与 HTTP 路由共存（一个 Router 既有 `/api` HTTP 端点又有 `/chat` WS 端点是常态）。② `WsServerRouteConfigInit` 取默认（限制+同源策略）→ 填 `Protocols`（"chat.v2, chat.v1"——第 94 章协商）、`Open`、（示例省略 Data/Release）→ `WsServerRoute` 注册——注册即复制，栈上的 Config 用完即弃。③ `RouterFreeze` 冻结后端点就绪——**Open 回调拿到连接的一行注释**（第 112 章 group 的 Add 就写在这里）。真实回环、TLS 变体、OOM 回滚各有专门测试——示例聚焦注册形态。

### 第二个完整程序：升级回调的完整生命周期

第二个程序来自 `examples/websocket/http_server`——Upgrade 完成回调的标准形态：

```embed path="extlibs/xws/examples/websocket/http_server/main.c" title="extlibs/xws/examples/websocket/http_server/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/http_server/main.c -lws2_32 -liphlpapi
（升级回调演示完成：连接发送-关闭-销毁路径执行后正常退出）
```

**刚才发生了什么。** ① 升级回调的**三步收尾模板**：`ConnText`（欢迎消息——第 111 章 copy 形态）→ `ConnClose`（NORMAL 码正常关闭）→ `ConnDestroy`（释放自己的引用）——本例立即关闭所以三步连写；长连接则 Ref 保存（进组/进会话）稍后走同一路径。② 头注释点明架构：“HTTP Request 事件可以先完成路由和鉴权，再把当前连接升级”——这就是分阶段底层 API 的使用场景（鉴权在 HTTP 层完成后 Upgrade）。③ `onRequest` 的形态与第 104/105 章完全一致——WebSocket 升级是 HTTP 请求处理的一种“响应”。

## 契约

- **注册即复制**：配置/事件/子协议注册时复制一次；之后调用方改动不影响端点；未对齐地址允许但须完整不回绕。
- **明密一体**：同一条路由在 `RouterStart` 建 `ws`、`RouterStartTls` 建 `wss`——无第二套注册。
- **Origin 三档**：SAME_HOST_OR_ABSENT 默认（原生可省/浏览器必符）/SAME_HOST 严格/ANY 仅外层已校验时。
- **自动响应**：非 GET→405+Allow；Origin/授权→403；协议→400；版本→426+`Sec-WebSocket-Version: 13`；内部→500（无法提交则异常关闭）。
- **Header 期拒绝**：方法错与正文声明在 Header 阶段拒绝、不读非法正文；无正文请求在完整请求阶段升级。
- **Authorize 时机**：握手与 Origin 通过后；可读请求/参数/协商结果；false 统一 403；自定义/异步走底层。
- **Open 借用**：回调返回后适配器释放交付引用——保存必须 `ConnRef`。
- **Release 语义**：Router 与全部已升级连接退出后恰好一次——在途连接延长 Data 寿命。
- **Error 边界**：只观察不响应——它是握手失败的通知者不是处理者。
- **底层四入口**：Check/Reply/Reject/Upgrade 独立组合——自定义授权/响应/状态的逃生口。

## 避坑

### 坑 1：Open 里保存了连接没 Ref

症状：Open 后的某个时刻访问连接崩溃——适配器在回调返回后释放了交付引用。

原因：Open 是**借用**语义。要长期持有（进组、进会话表）必须在回调内 `xrtWsConnRef` 增持——第 112 章 Add 内部就是这么做的。

```c bad
static void onOpen(xwsconn* pConn, ptr pData) {
	g_Session = pConn;   /* 存指针没 Ref：回调返回即悬空 */
}
```

```c good
static void onOpen(xwsconn* pConn, ptr pData) {
	if ( xrtWsConnRef(pConn) != NULL ) {
		g_Session = pConn;      /* 增持后保存 */
		xrtWsGroupAdd(g_Group, pConn);  /* 组再持一份 */
	}
}
```

### 坑 2：Origin 策略图省事用 ANY

症状：第三方页面用用户的浏览器凭证建立 WebSocket 连接——跨站 WebSocket 劫持（CSRF 的 WS 版）。

原因：浏览器自动带 Cookie/凭证；`ORIGIN_ANY` 不校验来源——除非外层（网关/内网隔离）已验，否则 ANY 是开门。默认档已兼顾原生（可省略）与浏览器（必符）——没有理由降到 ANY。

```c bad
Config.Origin = XWS_SERVER_ORIGIN_ANY;  /* "先跑通再说" */
```

```c good
/* 保持默认 SAME_HOST_OR_ABSENT；严格场景升 SAME_HOST */
Config.Origin = XWS_SERVER_ORIGIN_SAME_HOST;
Config.Authorize = check_session;       /* 业务授权另加一道 */
```

### 坑 3：以为 Release 在 Router 销毁时执行

症状：Router Destroy 后 Data 还被在途连接使用——提前释放引发崩溃；或反向：等不到 Release 泄漏。

原因：Release 的契约是“Router **和全部已升级连接**都不再使用后恰好一次”——在途连接延长 Data 寿命。清理顺序：Router 销毁 → 连接逐个关闭 → 最后的 Release。

```c bad
xrtHttpServerRouterDestroy(pRouter);
free(my_data);   /* 在途连接还在用：过早 */
```

```c good
/* Release 回调里释放——时点由契约保证 */
static void releaseState(ptr pData) {
	free(pData);   /* Router 与全部连接退出后恰好执行 */
}
Ws.Release = releaseState;
/* 销毁顺序：Drain server → 连接收尾 → Router Destroy → Release 自动到来 */
```

## 练习

### 基础：注册与自动响应矩阵

跑通 server_router；用 curl 对 `/chat` 发 POST（405）、无 Origin 带错版本（426）——对照自动响应表。验收标准：五类自动响应全部复现；`Allow: GET` 头存在。

### 进阶：授权端点

加 `Authorize` 回调：查令牌（模拟）决定放行/403。浏览器形态测试：带匹配 Origin 放行、带他域 Origin 拒绝（403 且错误域为 AUTHORIZATION）。验收标准：Origin 拒与业务拒在错误域可区分；合法连接正常升级。

### 挑战：频道服务全栈

第 112 章挑战的落地：Router 挂 `/chat/{channel}` 端点（路径参数选组）、Open 时 Ref+入组、消息广播到频道组、kick 与停机流程（Seal→排空→逐频道关闭→销毁→Release 收尾）。验收标准：多频道并发压测零错乱；停机后 Release 恰好执行一次（计数验证）；全程零泄漏。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 固定端点 | WsServerRoute 挂 HTTP Router；注册即复制；明密一体 |
| Origin 三档 | SAME_HOST_OR_ABSENT 默认 / SAME_HOST 严格 / ANY 仅外层已验 |
| 自动响应 | 405+Allow / 403 / 400 / 426+13 / 500——五类全覆盖 |
| Header 期拒绝 | 方法/正文声明不读正文即拒；升级在完整请求阶段 |
| Authorize | 握手与 Origin 后运行；false→统一 403；自定义走底层 |
| Open 借用 | 保存必须 ConnRef；返回后适配器释放交付引用 |
| Release | Router 与全部连接退出后恰好一次——在途连接延长 Data |
| 底层四入口 | Check/Reply/Reject/Upgrade——自定义授权/响应/状态的逃生口 |
| 会话形态 | Channel+协程顺序化每连接业务；收尾关-排空-销毁有序 |
| 协作章 | 109 连接/110 发送/111 分组/112 入口——四拼图合体 |
