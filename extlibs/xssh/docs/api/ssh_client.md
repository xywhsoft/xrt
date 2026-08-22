# SSH Client API

`ssh_client` 在调用方提供的 `xnetstream` 上组合 `ssh_client_core`、`ssh_session_stream` 和动态
`ssh_channels`。它不创建隐藏 Engine，不阻塞 Worker，不预分配 channel 或报文缓冲，也不引入第二套
SSH 状态机。

## 连接

调用 `xrtSshClientConfigInit` 和 `xrtSshClientInit` 后，把
`xrtSshClientNetEvents()` 与 `xrtSshClientNetData()` 直接交给 `xrtNetStreamConnect`。对于应用已经建立
的 Stream，在所属 Worker 调用 `xrtSshClientAttach`。连接打开后，channel 与控制报文 scratch 才绑定
Worker 缓冲池。

需要主机名解析、Happy Eyeballs、统一连接超时与取消时，使用独立可裁剪的
`xrtSshClientDial`。其完成回调只表示 TCP Dial 终态；SSH 可用状态仍只由 `Ready` 事件发布。代理、
隧道和自定义传输继续使用 `xrtSshClientAttach`，不会被 Dial helper 限制。

`xsshclientconfig.ReadyTimeout` 统一约束 TCP 打开后到 `Ready` 的 identification、KEX、主机信任和认证
阶段，单位为微秒，默认 30 秒，配置为零时禁用。超时通过 `XSSH_ERROR_TIMEOUT`、`XERR_TIMEOUT` 和
`xrt.ssh.client` 结构化错误同时到达 `Error`、`Close` 与全部未决 Future；TCP Dial 的 DNS/建连截止
时间仍由 `xnetdialconfig.Timeout` 独立控制。

主机密钥默认拒绝。`Core.HostKey` 返回 `DEFER` 后，应用通过 `HostKey` 事件取得决定时机，再调用
`xrtSshClientHostKeyAccept` 或 `xrtSshClientHostKeyReject`。认证 provider 返回 `NEED_MORE` 时触发
`Authenticate`，凭据就绪后调用 `xrtSshClientContinue`。

## 数据和控制报文

- `xrtSshClientSend` 是 payload 快速路径，只增加 SSH packet 编码和一次向 TCP 队列的所有权转移。
- `xrtSshClientBuild` 为可变控制报文提供从零增长、可复用且有硬上限的连续 scratch。
- `Packet` 在底层读事务提交前执行，保留未知协议扩展和自定义 channel 的完整处理能力。
- 标准 DATA 与 stderr 先进入 channel I/O staging；提交后触发 `Data`，应用可零复制检查或显式读取。
- OOM 不会提交半个 packet。内部 DATA 预留失败时连接 HOLD，释放内存后调用
  `xrtSshClientPacketRetry`。

TCP 队列返回 `AGAIN` 时，完整加密 packet 仍由 transport 唯一持有，同时暂停新的 SSH 输入。底层
LowWater/Drain 回调先重试该内部 packet；只有内部事务已经提交且 TCP 队列当前确实排空后，才发布
客户端 `Drain`。因此应用在 `Drain` 中提交的新报文不会越过先前保留的 packet。

`xrtSshClientGlobalReplies` 与 `xrtSshClientGlobalReplyReserve` 为 `tcpip-forward` 等全局
want-reply 请求提供动态、有界 FIFO；普通 exec/shell 裁剪路径不需要扩展这块存储。
`xsshclientevents.Global` 在 FIFO 与输入事务共同提交后发布 `REQUEST_SUCCESS` 或
`REQUEST_FAILURE`，并携带调用请求时提供的稳定 `ReplyToken`。回调不借用输入 packet，可直接发起
下一条全局请求或 channel 操作。

## 通道事务

`xrtSshClientChannelOpen` 是自定义 channel type 的基础入口。调用方直接向 writer 写入 type
专用字段，客户端只负责动态本地编号、窗口、回复关联、发送提交和失败回滚。经典 session 与
direct-tcpip helper 都建立在这个入口之上，不维护另一套 channel 状态。

- `xrtSshClientOwnsChannel` 检查 channel 是否属于当前客户端，避免把另一连接的稳定指针交给
  发送或回复队列。
- `xrtSshClientChannelFlush` 从 channel 发送队列构建一个受远端窗口、最大 packet 和 TCP
  背压共同限制的数据片段。
- `xrtSshClientChannelAdjust` 发布本地消费产生的窗口增量。
- `xrtSshClientChannelEof` 与 `xrtSshClientChannelClose` 分别表达发送半关闭和双向关闭事务。

`xsshclientevents.Channel` 只在对应线路事务提交后发布，通知本身不借用输入 packet：

- `OPENED / OPEN_FAILED`：本端 open 已由 peer 决定；主动接受的 peer channel 在 confirmation
  进入 TCP 队列后以 `Incoming = true` 发布 `OPENED`。
- `WRITABLE`：peer 的窗口增量已经提交，可以继续 flush 发送队列。
- `REQUEST_SUCCESS / REQUEST_FAILURE`：回复 FIFO 已提交弹出，`ReplyToken` 可稳定关联原请求。
- `EOF / CLOSED`：远端半关闭或 close 已进入 channel core；应用可在回调中发送本端 close，或在
  双向关闭且缓冲排空后移除 channel。

这些事件用于常规应用状态机；`Packet` 仍保留提交前的完整报文和未知扩展字段，二者不是重复解析
路径。事件回调允许直接发送下一条控制报文，驱动会把重入请求折叠到同一 Worker 的下一轮推进。

对端主动发送 `CHANNEL_OPEN` 时，`Packet` 回调可检查借用的 `xsshchannelopen`，再调用
`xrtSshClientChannelAccept` 或 `xrtSshClientChannelReject` 暂存决定。客户端先提交输入 packet，
随后发送 confirmation/failure；只有回复可靠提交后才发布或销毁对应动态 channel。调用方返回
`HOLD` 时可异步取得策略结果，再通过 `xrtSshClientPacketAccept` 或
`xrtSshClientPacketRetry` 恢复同一事务。未处理的未知类型默认返回
`XSSH_CHANNEL_OPEN_UNKNOWN_CHANNEL_TYPE`，不会留下半开放 channel。

`xrtSshClientPacketAccept` 与 `xrtSshClientPacketRetry` 把底层“提交完成并继续等待输入”的
`XSSH_NEED_MORE` 收敛为 `XSSH_OK`。应用只需要区分事务成功、再次 HOLD 和真正失败，不依赖
SessionStream 的内部推进状态。

所有操作都在 Stream 所属 Worker 串行执行。跨线程调用应使用 XRT Engine Post/Future；本模块不复制
同步、Future 或协程状态机。
