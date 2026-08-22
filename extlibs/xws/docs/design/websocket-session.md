# WebSocket 会话业务交接

本文说明 WebSocket Connection、Channel、Future 与 Coroutine 的应用层组合边界。
协议运行时不内置业务消息队列；业务层也不应重新实现 WebSocket 的连接等待、背压或
关闭状态机。


## 主线

```text
网络 Worker
	MessageBegin / MessageData / MessageEnd
		|
		| 复制借用分块，移交 Connection 强引用
		v
有界 Channel
		|
		v
业务 Coroutine / Task
		|
		| xrtWsConn*Async + Future Await
		v
Connection 所属 Worker
```

这条链的职责是：

- Connection 负责帧、UTF-8、压缩、Ping/Pong、背压和关闭。
- 回调只完成最小业务校验、借用数据复制和所有权移交。
- Channel 保存正式消息对象，并用固定容量表达业务过载。
- Coroutine 或 Task 执行顺序业务，不占用网络 Worker。
- Future 把跨线程发送重新串行化到 Connection 所属 Worker。


## 接收所有权

`MessageData` 的 `xbytesview` 只在当前同步回调期间有效。需要在回调后处理时必须复制，
不能把 `Data.Data` 直接放入 Queue、Channel、Task 或用户对象。

Connection 按流交付消息，不为每条连接建立固定大接收缓冲。业务确实需要完整消息时，
应按收到的实际字节增长存储，并同时设置应用上限；不应为每条会话预分配上限大小。
只需要流式解析的业务可以在 `MessageData` 中直接推进自己的增量解析器，避免重组。

消息要跨线程保留发送目标时，在 Worker 回调中调用 `xrtWsConnRef`。最终消费者必须调用
`xrtWsConnDestroy`。Connection 强引用只保证对象寿命，不替应用定义消息顺序或过载策略。


## 业务背压

Channel 容量与 Connection `SendLimit` 是两层独立门禁：

- Channel 满表示业务消费者落后；应用可以回复 `busy`、丢弃允许丢失的事件、暂停接收，
  或按策略关闭会话。普通 `busy` 文本也可能受出站背压；发送失败时应使用 Close 控制
  预算提交 1013，不能假设业务 ACK 必然可写。
- WebSocket 异步发送达到 `AsyncBytesLimit`/`AsyncCountLimit`，或 Connection 当前不可写，
  表示出站路径受阻；应等待返回 Future 或 `XWS_CONN_WAIT_WRITE/DRAIN`，不能忙重试。

不要用一个无限业务队列掩盖网络背压，也不要把 Connection 的发送预算误当成业务队列。
多人广播使用 `xwsgroup` 稳定快照和 Group Future；它不会替应用提供房间、主题或权限模型。


## 协程与任务

Channel 的 `RecvAwait` 会挂起当前协程，不阻塞调度线程，也不需要额外 `xcoevent`。收到
业务消息后可以直接创建 `xrtWsConnTextAsync` 等 Future，并用 `xrtFutureAwait*` 等待受理。
真正阻塞的数据库、文件或第三方 SDK 调用应提交到 Task/线程池，再由协程等待 Task Future。

Open、Write、Drain 和 Close 已有 WebSocket Future，不应再用共享布尔值和轮询模拟。
取消 Channel Await、业务 Task 或发送 Future，不等价于关闭会话；需要终止协议时显式发送
Close，等待关闭终态，并为失联对端保留 `CloseTimeout`。


## 停服顺序

1. 停止接收新 HTTP 请求和 WebSocket Upgrade。
2. 封闭连接组，阻止新会话进入广播集合。
3. 关闭业务 Channel 的发送端，让消费者排空已经接纳的消息。
4. 对现有 Connection 发 Close，并等待协议终态或截止时间。
5. 取消仍未完成的慢任务，释放消息持有的 Connection 引用。
6. 销毁 WebSocket/HTTP 状态，最后销毁 NetEngine 和业务调度器。

协议 Close 与业务 Channel drain 是两个终态，不能以其中一个替代另一个。


## 示例

`examples/websocket/session_channel_coroutine/main.c` 给出可编译的正式骨架，覆盖：

- 按实际消息大小增长、具有 64 KiB 硬上限的接收存储；
- Text-only 策略、1003/1009/1011 Close；
- Channel 满时的 `busy` 或 1013 过载路径；
- Connection 强引用跨线程移交；
- Coroutine 消费与 WebSocket Future 回发；
- Channel 关闭后的剩余消息析构。

HTTP Upgrade、客户端连接、代理、TLS、连接组和 Future 的独立可运行示例位于同级
`examples/websocket` 目录。组合层复用这些公开 API，不增加第二套 WebSocket 客户端或服务端。
