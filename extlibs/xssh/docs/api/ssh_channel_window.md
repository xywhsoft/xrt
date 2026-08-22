# SSH Channel Window API

`ssh_channel_window` 是无分配、无网络所有权的 RFC 4254 双向流控状态。它不保存 payload，也不
设置固定接收缓冲；调用方可以把视图直接交给消费方、环形缓冲或零拷贝队列。

## 发送方向

`xrtSshChannelSendLimit` 返回远端窗口与远端 `max-packet` 的较小值。数据成功进入可靠发送队列后，
调用 `xrtSshChannelSendCommit` 扣减额度；队列失败时不应提交。收到 WINDOW_ADJUST 后调用
`xrtSshChannelSendAdjust`，任何令 uint32 窗口回绕的消息都返回协议错误。

## 接收方向

收到普通或 extended data 后，先调用 `xrtSshChannelReceiveCommit` 校验本地窗口和 `max-packet`。
应用真正消费数据时调用 `xrtSshChannelReceiveConsume`，额度进入 `ReceivePending`，但尚未重新通告
给远端。达到 `AdjustThreshold` 或窗口耗尽时，`xrtSshChannelReceiveAdjustReady` 返回 true。

`xrtSshChannelReceiveAdjustLimit` 给出一条 WINDOW_ADJUST 可安全携带的额度；消息可靠排队后再调用
`xrtSshChannelReceiveAdjustCommit`。这套两阶段契约避免发送失败却提前扩大本地窗口。
`xrtSshChannelReceiveGrantCommit` 用于调用方新增加接收容量，支持零窗口启动和动态内存预算；它不
消耗已消费额度，且同样应在对应 WINDOW_ADJUST 已可靠排队后调用。

状态使用 `uint64` 统计未消费与待返还字节，长连接不会因累计超过 4 GiB 回绕；线路窗口仍严格保持
RFC 要求的 `uint32`。示例见 `examples/channel_window/main.c`。
