# SSH 客户端 Session

`ssh_client_session` 在 `ssh_client` 的动态 channel、事务提交和网络背压之上，提供 RFC 4254 的经典 session 应用路径。该层不拥有 Engine、TCP Stream、线程、Future 或协程。

## 打开与请求

- `xrtSshClientSessionOpen` 创建动态 channel，并发送 `session` 类型的 `CHANNEL_OPEN`。
- `xrtSshClientSessionRequest` 保留未知 request 类型的扩展入口。
- `xrtSshClientSessionEnv`、`xrtSshClientSessionShell`、`xrtSshClientSessionExec` 和 `xrtSshClientSessionSubsystem` 覆盖经典进程启动路径。
- `xrtSshClientSessionSignal` 和 `xrtSshClientSessionBreak` 发送进程控制请求。

所有函数必须在客户端 TCP Stream 所属 worker 中调用。需要回复的请求先按 channel 动态扩展 token FIFO；peer 的 `CHANNEL_SUCCESS` 或 `CHANNEL_FAILURE` 通过 `xsshconnectionpacket.ReplyToken` 返回。请求构建失败不会把 token 推入 FIFO。

## 数据与关闭

数据所有权由 `xrtSshChannelIoWrite`、`WriteBorrow`、`WriteTake`、`WriteRef` 和 `WriteBuffer` 明确选择。随后调用 `xrtSshClientChannelFlush`；每次只发送当前窗口、peer 最大包和 TCP 背压允许的一条消息。读取后调用 `xrtSshClientChannelAdjust` 返还窗口，结束写方向使用 `xrtSshClientChannelEof`，双向结束使用 `xrtSshClientChannelClose`。

PTY 与窗口尺寸属于独立可裁剪层，不会被普通 exec 强制带入。
