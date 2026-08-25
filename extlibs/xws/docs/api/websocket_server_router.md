# WebSocket Server Router

`websocket_server_router` 是 `websocket_server` 与 `http_server_router` 之上的可裁剪常用端点层。底层 `xrtWsServerCheck`、`xrtWsServerReply`、`xrtWsServerReject` 和 `xrtWsUpgrade` 仍可独立组合；需要鉴权、按路径参数创建每连接状态或自定义响应时，应直接在普通 HTTP 路由回调中使用这些底层 API。

`xwsserverroutererror` 额外用 `XWS_SERVER_ROUTER_ERROR_AUTHORIZATION` 区分 Origin 和业务授权拒绝，其他值分别覆盖参数、配置、分配、路由注册、响应与运行时状态。

端点生命周期回调类型为 `xwsserverrouteopenproc`、`xwsserverrouteerrorproc` 和 `xwsserverrouterreleaseproc`。Open 借用已建立 Connection，Error 借用当前稳定错误，Release 在 Router 和全部已升级连接不再使用 Data 后恰好执行一次。

## 固定端点

`xrtWsServerRouteConfigInit` 初始化服务端默认限制、空事件表和 `XWS_SERVER_ORIGIN_SAME_HOST_OR_ABSENT` 策略。该策略允许原生客户端省略 Origin，但浏览器提供 Origin 时必须与当前 HTTP 请求的 scheme、host 和有效端口一致。`XWS_SERVER_ORIGIN_SAME_HOST` 还会拒绝缺失 Origin 的客户端；`XWS_SERVER_ORIGIN_ANY` 只适合已在外层完成来源校验的组合。

可选 `Authorize` 回调在协议握手和 Origin 校验通过后运行，可读取完整请求、路由参数和协商结果。返回 `false` 由路由器统一回复 `403`；需要自定义拒绝响应或异步授权时，使用普通 HTTP 路由与底层分阶段 Upgrade API。

固定配置允许存放在未对齐地址，但整个结构必须处于完整且不回绕的地址区间。`xrtWsServerRoute` 在注册前只读取固定配置一次，并复制配置、事件和子协议列表；调用返回后，修改或释放调用方的固定配置与子协议字符串都不会改变已注册端点。路由在 Header 阶段拒绝方法错误和任何正文声明，不读取非法请求正文；无正文请求在完整请求阶段提交 Upgrade。

固定端点自动处理：

- 非 `GET` 返回 `405` 和 `Allow: GET`；不伪造 CORS/OPTIONS 授权。
- Origin 或业务授权失败返回 `403`。
- 握手协议错误返回 `400`，版本错误返回带 `Sec-WebSocket-Version: 13` 的 `426`。
- 服务端配置、分配或内部错误返回 `500`，响应无法提交时异常关闭连接。

`Open` 借用 Upgrade 产生的 `xwsconn`，回调返回后适配器自动释放交付引用。需要保存连接时必须调用 `xrtWsConnRef`。连接事件收到配置中的原始 `Data`，不暴露内部适配对象。

固定端点不区分明文与 TLS。HTTP Router 通过 `xrtHttpServerRouterStart` 启动时建立 `ws` 端点，通过 `xrtHttpServerRouterStartTls` 启动时，同一条路由自动接管 TLS Stream 并建立 `wss` 端点，不需要第二套注册 API。

## 生命周期

注册成功后 Router 接管一次 `Release(Data)`；注册失败时不接管。Router 销毁不会提前释放仍被 WebSocket 连接使用的数据，只有 Router 和全部已升级连接都退出后才调用一次 `Release`。`Error` 只观察同步握手与异步 Upgrade 错误，不应在该回调中再次提交响应。

```c
xwsserverrouteconfig Ws;

xrtWsServerRouteConfigInit(&Ws);
Ws.Server.Protocols = XRT_STR_LITERAL("chat.v2, chat.v1");
Ws.Events.MessageData = onMessage;
Ws.Open = onOpen;
Ws.Data = State;
Ws.Release = releaseState;

xrtWsServerRoute(
	Router,
	XRT_STR_LITERAL("/chat"),
	&Ws
);
```

可编译的最小注册示例位于 `examples/websocket/server_router/main.c`。明文和 TLS 真实回环、消息收发、HTTP Server 先退出、活动连接延迟释放及单头文件门禁分别位于 `tests/websocket/test_server_router.c` 与 `tests/websocket/test_server_router_tls.c`；逐分配点回滚位于 `tests/websocket/test_server_router_oom.c`。
