# SSH Client Future API

`ssh_client_future` 把已经提交的客户端和 channel 条件桥接到 XRT `xfuture`。它不复制 SSH 状态机，
不创建线程或 Engine，也不把取消转换为连接中止。

## 执行上下文

`xrtSshClientWaitAsync`、`xrtSshClientChannelWaitAsync`、`xrtSshClientChannelReadAsync`、
`xrtSshClientChannelReplyAsync` 和 `xrtSshClientGlobalReplyAsync` 必须在客户端所属网络 Worker 上
创建。返回后，Future 可由普通线程等待，也可直接交给 `xrtFutureAwait` 挂起协程。

Future 不拥有 caller-provided `xsshclient` 或借用的 `xsshchannel`。应用必须让客户端至少存活到
Future 终态；有未决 channel waiter 时不得提前移除对应 channel。客户端 `Close` 或显式 `Clear`
会终结全部未决等待，不留下指向调用方存储的后台操作。

## 客户端条件

- `READY`：identification、KEX、主机信任和认证已经完成。
- `DRAIN`：底层 TCP 发送队列当前为空，不代表所有 channel 队列都已被远端窗口接收。
- `CLOSE`：连接进入唯一关闭终态；正常关闭成功完成，异常关闭保留底层结构化错误。

这些条件按水平语义检查。创建 Future 时条件已经满足会立即完成，不要求应用提前猜测事件时序。

## Channel 条件

`ChannelWaitAsync` 覆盖 open、可写预算、远端 EOF 和远端 close。open failure 与 request failure
进入 Future 失败终态；错误域分别保留 channel open、channel request 或 global request 语义。

`ChannelReadAsync` 只等待指定 DATA/stderr 流至少有一个字节，不消费缓冲。若同一客户端同时使用
`Events.Data`，回调先获得消费机会；已经被回调全部消费的数据不会伪造可读 Future。
缓冲为空时收到远端 EOF 或 close，未决的读取 Future 进入 `XFUTURE_CLOSED`，不会一直等待到整个
SSH 连接关闭。未决的 channel 写入和请求回复等待也会在该 channel close 时关闭；EOF 只关闭读取，
不错误地关闭仍可发送的半双工写入。

channel/global request Future 通过调用请求时进入 FIFO 的 `ReplyToken` 精确关联回复。应在发送
want-reply 请求的同一 Worker 回合立即创建等待，避免把一次性回复事件当作可重复查询的状态。

Future 取消只取消本次 waiter。是否发送 channel EOF/CLOSE 或中止连接仍由应用显式决定，保证线程、
同步和协程入口共享相同副作用契约。

Future 内部使用 channel 指针和稳定本地 ID 共同关联事件。应用可以在 close 回调中移除旧 channel 并
立即创建新 channel；即使分配器复用地址，旧终态也不会命中新对象的 waiter。
