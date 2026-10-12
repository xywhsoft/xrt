# SSE 客户端

编码、解析器与 HTTP 内容描述在 [xhttp](../../../xhttp/docs/api/http_sse.md)。

## EventSource 客户端

`http_sse_client` 提供 `CONNECTING`、`OPEN`、`CLOSED` 三态会话。最短路径只需要
一个 HTTP Client、URL 和消息回调：

```c
xhttpsseclientevents Events;
xhttpsseclient* sse;

memset(&Events, 0, sizeof(Events));
Events.Message = on_message;
Events.Close = on_close;
Events.Data = context;

sse = xrtHttpSseConnect(
	client,
	XRT_STR_LITERAL("https://example.test/events"),
	NULL,
	&Events
);
```

`Message` 是唯一必需回调。需要认证、自定义 Header 或 Cookie 时，先建立无正文、
无 fragment 的 GET `xhttprequest`，再调用 `xrtHttpSseConnectRequest`。会话克隆请求，
因此入口返回后可以立即修改或销毁原请求；正文、非 GET 方法和 fragment 会在任何
网络操作前失败。每次尝试都会唯一设置 `Accept: text/event-stream`，按 Parser 的
持久 ID 设置或删除 `Last-Event-ID`，并清除与 GET 冲突的旧正文分帧字段。

### 配置与重连

```c
typedef struct xhttpsseclientconfig {
	xhttpsseparserconfig Parser;
	xhttpcalloptions Http;
	size_t MaxReconnects;
	uint64 RetryMin;
	uint64 RetryMax;
} xhttpsseclientconfig;
```

`xrtHttpSseClientConfigInit` 默认使用动态 Parser 限额、三秒初始 retry、100..300000
毫秒本地重连范围和无限重连次数。`MaxReconnects` 只计算首次请求之后实际安排的
重连；零表示首次尝试结束后立即以 `RECONNECT_LIMIT` 关闭。`RetryMin`、`RetryMax`
和服务端 `retry` 字段的单位都是毫秒；`Http.Timeout` 与 `Http.IdleTimeout` 仍使用
HTTP Client 的毫秒单位。

配置和事件表都是调用期间立即复制的固定值，允许位于完整但未对齐的存储中；入口
返回后可以立即修改或释放。Cookie 与 Cache 分区键会深复制，取消令牌、代理和 HTTP
Client 会增加引用，因此重连不会借用调用方栈上的选项。

```c
#define XHTTP_SSE_RECONNECT_MAX_DEFAULT SIZE_MAX
#define XHTTP_SSE_RETRY_MIN_DEFAULT UINT64_C(100)
#define XHTTP_SSE_RETRY_MAX_DEFAULT UINT64_C(300000)
```

默认 HTTP 策略适合长流：关闭总超时和空闲超时，响应正文上限设为 `UINT64_MAX`，
强制 origin-form，跟随重定向，关闭通用 HTTP 自动重试与缓存。若对应模块存在，
代理、Cookie、TLS 和自动内容解码仍沿用 HTTP Client；调用选项中的取消令牌、代理
和分区文本由会话跨重连深持有。SSE 自己负责重连，并占用 `Http.Events` 的
Informational、Headers 和 Body 回调完成会话驱动，调用方不能同时安装这些通用事件。

状态处理保持 EventSource 语义：

- `200` 加唯一合法 `text/event-stream` 进入 `OPEN`，并发布一次 Open 回调。
- `204` 正常停止，不再重连，Close 原因为 `STOP` 且错误为空。
- 其他状态、缺失或错误媒体类型是永久 `REJECTED`，不会循环请求错误端点。
- 正常 EOF、拨号、连接池、代理、传输和超时失败按当前 retry 延迟重连。
- 解析错误、用户回调拒绝、取消和其他 HTTP 错误直接进入唯一终态。

每次成功解析的 `id` 会跨响应保留，并在下次请求中发送。服务端 `retry` 会先写入
Parser，再裁剪到本地范围。重定向完成后，会话克隆最终有效请求作为后续模板；例如
旧入口 307 到新 URL 后，断线重连会直接访问新 URL，不重复访问旧入口。新的相对
重定向继续由 HTTP Client 的唯一 URL 解析和凭据过滤规则处理。

### 回调与背压

Open、Message、Comment、Retry 和 Retrying 都在当前 HTTP Call 的网络 Worker 上
同步执行。Message、Comment、响应和 Retrying Error 只借用到本次回调返回；跨回调
保存必须复制。Open、Message、Comment 或 Retry 返回 `false` 会永久关闭会话；回调
可以先设置结构化错误，该错误会作为 SSE `CALLBACK` Cause 保留。

在项目回调中调用 `xrtHttpSseClientPause` 会同时暂停 Parser 交付和底层 HTTP/TCP
读取。已经进入当前输入回调但尚未解析的尾段按实际长度动态保留，不存在每会话固定
8 KiB 缓冲；内存不足进入 `INTERNAL` 终态，不会丢字节后继续。`Resume` 可以从任意
线程调用，它把尾段排空后才恢复传输读取；重复恢复或在非暂停状态恢复返回状态错误。
Pause 已经生效时重复调用是幂等成功。

暂停尾段排空后会立即释放临时 Buffer 容量，重连开始时也不会保留上一次暂停产生的
峰值存储。Parser 的行、数据、类型和 ID 缓冲仍按协议限额复用，不存在每连接固定
正文块；会话销毁时统一释放。

回调可以重入调用 `Close`。一旦关闭门成功，当前输入不再发布后续项目，活动 Call
或重连 Timer 被取消。`Close`、外部取消和终态错误通过同一门收敛，Close 回调全生命
周期至多一次；Retrying 回调只报告暂态断开，不是终态。

### 所有权与查询

会话持有 HTTP Client、请求模板、Parser、取消监听器和活动 Call/Timer 的运行时引用。
调用方持有 `xrtHttpSseConnect*` 返回的引用，使用 `xrtHttpSseClientRef` 跨作用域保留，
最后用 `xrtHttpSseClientDestroy` 释放。Destroy 不隐式关闭活动订阅；需要停止时先调用
可从任意线程执行的 `xrtHttpSseClientClose`。

`xrtHttpSseClientState`、`Paused` 和 `Info` 是并发快照。`Info` 提供最后状态码、当前
retry、消息、注释、retry 更新和重连计数，不暴露 Parser 借用内存。只有唯一 Close
已经发布后才能调用 `xrtHttpSseClientError`；主动关闭和 204 停止返回空，其余异常
终态返回由会话拥有的稳定错误。SSE 顶层错误域是 `xrt.http.sse.client`，底层 HTTP、
解析、内存或系统错误保留在 Cause 链中，便于 C 分类和上层宿主映射。
`Info` 支持完整但未对齐的输出存储，并在所有原子字段读取完成后一次性发布，不会让
调用方看到只更新一半的结构。
