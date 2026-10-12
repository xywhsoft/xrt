# HTTP Server Middleware

`http_server_middleware` 是 `http_server_router` 的独立可裁剪扩展。它在完整请求阶段用同步洋葱链包裹路由、未命中回退和 Router 标准响应，不改变 Header、流式 Body、异步响应、协议升级或 Host Mux 的底层路径。Mux 固定的是完整 Router 引用，因此 Host 热替换期间，中间件、Body 和最终路由始终属于同一个请求快照。

## 分层

- `xrtHttpServerUse` 注册由调用方管理数据的常用中间件。
- `xrtHttpServerUseOwned` 在注册成功后把一次 `Release(Data)` 责任交给 Router。
- `xrtHttpServerNext` 同步进入下一层或最终路由，返回后可执行日志、计时等后置逻辑。
- 不调用 `Next` 即为短路；中间件可以直接响应，也可以保留 Connection 后异步响应。
- Router 冻结后，中间件表与路由表一起只读共享；固定 Router 和 Host Mux 使用同一条分派路径。

## 契约

中间件按注册顺序进入，按逆序退出。回调返回 `true` 表示当前层已经接受请求；返回 `false` 表示不可恢复的应用错误，运行时会尽力提交固定 500，若响应已经提交则异常关闭连接。

`Next` 返回表示下一层分派已经返回，不表示响应字节已经写入网络。同步直接响应通常已经进入 `XHTTP_CONN_RESPONSE`；异步路由则可能仍处于等待状态，外层后置逻辑也会立即继续。需要统计完整响应耗时或发送结果时，应使用 Connection 的响应完成路径，而不是把 `Next` 返回时刻当作 I/O 完成。

`xhttpservernext` 只在当前同步回调栈内有效，不能保存到其他线程、Future 或协程中。每个 `Next` 最多调用一次；重复调用会设置 `XHTTP_SERVER_MIDDLEWARE_ERROR_NEXT` 并使请求进入失败路径。异步中间件应在当前回调内短路并自行保留 Connection，而不是延迟调用 `Next`。

中间件只在完整请求阶段运行。路由级 `Headers` 和 `Body` 回调继续负责鉴权前置、按路由正文限额、流式上传和正文背压等早期决策。为了让统一日志和错误处理中间件看见 404、405 与自动 OPTIONS，启用至少一层中间件时，未命中请求的正文会被丢弃后再进入完整请求链；未启用该模块时仍在 Header 阶段直接响应并拒绝正文。

匹配路由时，`Params` 与 `Count` 和最终路由收到的参数完全相同；未命中时 `Params` 为空。参数描述符和 `Next` 都只借用当前调用栈。

中间件数量受 Router 的 `MaxRoutes` 限额约束。注册扩容是原子的：失败不会改变可见数量，也不会转移拥有型数据的清理责任。最后一个 Router 引用销毁时，拥有型中间件按注册逆序释放。

## 示例

```c
static bool logRequest(
	xhttpserver* Server,
	xhttpconn* Connection,
	const xhttpserverrequest* Request,
	const xhttprouteparam* Params,
	size_t Count,
	xhttpservernext* Next,
	ptr Data
)
{
	bool Result;

	printf("request begin\n");
	Result = xrtHttpServerNext(Next);
	printf("request end\n");
	return Result;
}

xhttpserverrouter* Router = xrtHttpServerRouterCreate(NULL);

xrtHttpServerUse(Router, logRequest, NULL);
xrtHttpServerGet(Router, XRT_STR_LITERAL("/health"), health, NULL);
xrtHttpServerRouterFreeze(Router);
Server = xrtHttpServerRouterStart(Engine, &Config, Router, NULL);
```

完整示例位于 `examples/http/server_middleware/main.c`。真实 Select TCP 回环、短路、404 包裹、重复 `Next` 和生命周期测试位于 `tests/http/test_http_server_middleware.c`；逐分配点回滚位于 `tests/http/test_http_server_middleware_oom.c`；Mux 热替换组合门槛位于 `tests/http/test_http_server_mux_runtime.c`。
