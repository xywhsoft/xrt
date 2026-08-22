# SSH 客户端 Forwarding

`ssh_client_forward` 只组合 SSH forwarding 协议事务，不创建本地 TCP listener、隐藏 Engine、线程或同步等待。

- `xrtSshClientDirectTcpipOpen` 创建 `direct-tcpip` channel，用于本地端口转发或应用自定义隧道。
- `xrtSshClientForwardedTcpipAccept` 在 `Packet` 回调或 HOLD 期间解析服务端主动打开的 `forwarded-tcpip`，读事务提交后由客户端自动发送 confirmation。
- `xrtSshClientTcpipForward` 发送要求回复的 `tcpip-forward` 全局请求；端口为零时，成功报文可通过 `xrtSshTcpipForwardSuccessRead` 取得服务端分配端口。
- `xrtSshClientTcpipForwardCancel` 取消同一 remote forwarding 地址。

全局请求 token 使用客户端动态、有界 FIFO。FIFO 出队和输入事务提交后，
`xsshclientevents.Global` 发布 `REQUEST_SUCCESS`/`REQUEST_FAILURE` 及稳定 token；需要读取动态端口等
类型专用字段的应用仍可在提交前 `Packet` 回调中解析并复制。Direct channel 的数据、背压、窗口与
关闭统一使用 `xrtSshChannelIo*` 和 `xrtSshClientChannel*`。

拒绝未知或不允许的 peer channel 使用 `xrtSshClientChannelReject`。未作决定且直接接受 Packet 时，客户端默认回复 `UNKNOWN_CHANNEL_TYPE`，不会让 peer 永久等待。应用不应在 Packet 回调中直接发送 confirmation；客户端会严格按“提交读事务、发送响应、提交 channel 状态”的顺序完成。
