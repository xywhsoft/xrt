# SSH Channel State API

`ssh_channel_state` 是无网络、无缓冲的 RFC 4254 EOF/CLOSE 生命周期。它与窗口状态分离，同一套
状态可由同步、future、协程或事件循环驱动。

EOF 只表示发送方不再发送 data 或 extended-data，不会关闭反方向数据，也不会禁止 channel
request。`xrtSshChannelLocalEofCommit` 只能在 EOF 已可靠排队后调用；远端重复 EOF 或 close 后
EOF 作为协议错误返回。

CLOSE 终止新增数据和 request。收到远端 CLOSE 后，`xrtSshChannelCloseReplyNeeded` 指示是否需要
回复；只有 `xrtSshChannelClosed` 返回 true 时才能回收 channel id。为正确处理双向在途消息，本端
先发送 CLOSE 后，接收方向仍可处理远端在 CLOSE 之前已发送的数据，直到远端 EOF 或 CLOSE 到达。

该模块只管理协议生命周期，不决定是否丢弃应用不再需要的在途数据。示例见
`examples/channel_state/main.c`。
