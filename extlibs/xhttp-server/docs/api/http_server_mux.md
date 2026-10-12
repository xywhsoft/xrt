# HTTP Server Mux

`http_server_mux` 是冻结 `xhttpserverrouter` 之上的可选虚拟主机与热替换层。它不会改变基础 Router 的只读并发契约；不需要 Host 路由或热更新的服务应继续直接使用 `xrtHttpServerRouterStart`。

## 分层

- `xhttprouter`：不拥有处理器的纯路径索引。
- `xhttpserverrouter`：拥有服务端回调的冻结路径 Router，匹配阶段不加锁。
- `xhttpservermux`：用有效 HTTP Host 选择一个冻结 Router，允许运行时替换和移除 Host。
- `xrtHttpServerMuxStart` / `xrtHttpServerMuxStartTls`：把 Mux 组合到明文或 TLS HTTP/1 Server。

TLS SNI 身份选择由 `xtlsserverconfig.Select` 负责。HTTP Host 路由与证书选择是两个不同协议阶段，不在 Mux 中重复实现证书表。

## 生命周期

`xrtHttpServerMuxHost` 和 `xrtHttpServerMuxDefault` 保留 Router 引用，调用方仍保留自己的引用。运行中的 Server 也保留 Mux 引用，因此调用方可在启动后销毁自己的 Mux 与 Router 引用。

每个请求在 Header 完整后固定一次 Router 引用。替换 Host 只影响之后开始的请求；流式正文复用 Header 阶段按需保存的路径匹配，不会按 Body 片段重新查 Host 或 Router。异步响应和 Upgrade 继续使用原 Router，直到连接开始下一请求或关闭。

Mux 配置会在创建时立即快照，配置、Match 的 Router 输出和 Stats 输出都可以使用完整
但未对齐的固定存储。所有地址范围在加锁或分配前验证；Stats 在读锁内形成局部快照，
解锁后一次性发布，调用方不会观察到半更新统计。
Match 输出不能覆盖 Mux 对象或仍需读取的 Host，Stats 输出不能覆盖 Mux 对象；
别名错误不会先清空输出或破坏 Mux 状态。

## Host 契约

注册 Host 使用 HTTP Host 语法，不得包含端口。DNS 名称和 IPv6 十六进制比较忽略 ASCII 大小写；IPv6 字面量注册时使用方括号。请求端口不参与虚拟主机选择。

未知 Host 优先交给调用方提供的 Server 回退事件；没有回退时直接响应 `421 Misdirected Request`。Mux 不强迫应用构建 Reply、字典或 JSON 对象。

## 常用流程

```c
xhttpserverrouter* pDefault = xrtHttpServerRouterCreate(NULL);
xhttpserverrouter* pApi = xrtHttpServerRouterCreate(NULL);
xhttpservermux* pMux = xrtHttpServerMuxCreate(NULL);

xrtHttpServerGet(pDefault, XRT_STR_LITERAL("/"), home, NULL);
xrtHttpServerGet(pApi, XRT_STR_LITERAL("/v1"), api, NULL);
xrtHttpServerRouterFreeze(pDefault);
xrtHttpServerRouterFreeze(pApi);

xrtHttpServerMuxDefault(pMux, pDefault);
xrtHttpServerMuxHost(
	pMux, XRT_STR_LITERAL("api.example.test"), pApi
);
pServer = xrtHttpServerMuxStart(pEngine, &Config, pMux, NULL);
```

热替换只需构建并冻结新 Router，再调用一次：

```c
xrtHttpServerMuxHost(
	pMux, XRT_STR_LITERAL("api.example.test"), pNext
);
```

## 裁剪

- `XRT_MODULE_HTTP_SERVER_MUX`：Host 表、热替换和明文 Server 组合。
- `XRT_MODULE_HTTP_SERVER_MUX_TLS`：额外加入 TLS Server 入口。

Mux 只为启用该层的 HTTP Connection 分配一个小型 Router 固定上下文。固定 Router 仅在流式请求活动期间按需分配匹配缓存，基础 Server 和普通请求不承担固定内存与锁开销。
