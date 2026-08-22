# HTTP Server Router

`http_server_router` 是 `http_router` 与 HTTP Server 运行时之上的可裁剪易用层。底层 Router 仍可独立使用；该模块只负责复制服务端回调、按请求阶段分发，并提供常见默认响应。

可运行的最小注册示例见 `examples/http/server_router/main.c`。需要虚拟主机和运行时替换时，在冻结 Router 之上组合 [HTTP Server Mux](http_server_mux.md)。

## 分层

- `xrtHttpServerRouteEvents` 暴露 Header、流式 Body 和完整 Request 三个阶段，适合上传、按路由正文限额和异步处理。
- `xrtHttpServerRoute` 只注册完整请求回调，是常见业务接口。
- `xrtHttpServerGet/Post/Put/Patch/Delete/Any` 提供经典方法的一行注册。
- `xrtHttpServerRouterDispatch` 可嵌入用户自己的 `Events.Request`。
- `xrtHttpServerRouterStart` 负责完整事件适配；传入的原始 `Events.Request` 只处理未命中请求，其他事件保持原义。
- `xrtHttpServerRouterStartTls` 位于独立的 `http_server_router_tls` 裁剪模块，保持同一分发和生命周期契约。
- `xrtHttpServerUse/UseOwned` 位于独立的 `http_server_middleware` 裁剪模块，以无每请求分配的同步洋葱链包裹最终分派。
- `xrtWsServerRoute` 位于独立的 `websocket_server_router` 裁剪模块，用于固定 WebSocket 端点；鉴权、按路径参数构造状态或自定义拒绝响应仍直接组合底层路由与 `xrtWsUpgrade`。

## 契约

Router 在注册期复制方法、模板和回调记录。`Release` 为空时 `Data` 仍由调用方拥有；非空时只有注册成功才转移一次清理责任，最后一个 Router 引用释放时调用 `Release(Data)`。`Freeze` 后不能继续注册，但可以被多个 Server 和 Worker 无锁共享。Server 启动后持有 Router 引用，因此调用方可以立即释放自己的引用。

`xhttprouterconfig`、`xhttpserverrouteevents` 和启动时传入的 `xhttpserverevents` 都是固定描述符：接口支持其位于未对齐存储，并在返回前完整快照；调用方随后可以覆盖或释放原存储。任何描述符范围发生地址回绕时，操作都会在分配或状态变更前返回结构化参数错误。Router 本体是有生命周期的状态对象，不属于这一未对齐描述符承诺。

匹配参数描述符只在当前回调期间有效；参数名借用冻结 Router，参数值借用请求快照。需要跨回调保存时，应复制描述符和需要的文本，或保留请求引用。

Header 阶段选择 `XHTTP_SERVER_BODY_STREAM` 后，运行时只为当前活动请求按需保存一次结构匹配结果；后续每个 Body 片段和最终 Request 不再重复解析 Target 或匹配 Router。缓冲、丢弃和拒绝正文的请求不承担这项分配。

常见不超过 8 个参数的路由使用栈上描述符；更长模板按精确数量临时分配。默认情况下，未命中路由在 Header 阶段直接提交 404、405 或自动 OPTIONS，并拒绝继续读取正文；注册中间件后会丢弃未命中正文并让最终响应穿过中间件链。405 和自动 OPTIONS 都包含按注册顺序生成的 `Allow`，GET 自动公开 HEAD。

## 示例

```c
static void userGet(
	xhttpserver* Server,
	xhttpconn* Connection,
	const xhttpserverrequest* Request,
	const xhttprouteparam* Params,
	size_t Count,
	ptr Data
)
{
	(void)Server;
	(void)Request;
	(void)Data;
	(void)xrtHttpConnReply(
		Connection,
		200,
		XRT_STR_LITERAL("application/json; charset=utf-8"),
		XRT_BYTES_LITERAL("{\"code\":200}")
	);
	(void)Params;
	(void)Count;
}

xhttpserverrouter* Router = xrtHttpServerRouterCreate(NULL);

xrtHttpServerGet(
	Router,
	XRT_STR_LITERAL("/users/{id}"),
	userGet,
	NULL
);
xrtHttpServerRouterFreeze(Router);
Server = xrtHttpServerRouterStart(Engine, &Config, Router, &Events);
xrtHttpServerRouterDestroy(Router);
```

真实 Select TCP 回环、HEAD、405/OPTIONS、9 参数动态匹配、大 `Allow`、流式正文、未命中回调和生命周期门禁位于 `tests/http/test_http_server_router.c`。逐分配点回滚位于 `tests/http/test_http_server_router_oom.c`，TLS 1.3 回环位于 `tests/http/test_http_server_router_tls.c`。
