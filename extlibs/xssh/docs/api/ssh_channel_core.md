# SSH Channel Core API

`ssh_channel_core` 把单个 channel 的 open、窗口和 EOF/CLOSE 状态组合在一起。对象不拥有数据
缓冲、不分配内存、不保存 channel type 字段，也不接触 transport；应用可把任意数量的
`xsshchannelcore` 放入数组、哈希表或自定义 slab，并按本地 recipient 做 O(1) 路由。

本端 open 使用 `xrtSshChannelCoreOpenInit`，在线路 open 可靠提交后等待 peer 响应；收到并解析
confirmation 或 failure 后分别调用 `xrtSshChannelCoreConfirmationCommit` 或
`xrtSshChannelCoreFailureCommit`。peer open 使用 `xrtSshChannelCoreAcceptInit`，策略层复制所需
类型字段并构建响应；confirmation 或 failure 可靠排队后分别调用
`xrtSshChannelCoreAcceptCommit` 或 `xrtSshChannelCoreRejectCommit`。

只有 `XSSH_CHANNEL_CORE_OPEN` 阶段开放数据面。发送长度先受
`xrtSshChannelCoreSendLimit` 限制，packet 可靠排队后再调用
`xrtSshChannelCoreDataSendCommit`；接收 data 在应用接管视图前调用
`xrtSshChannelCoreDataReceiveCommit`。应用消费数据后调用 `xrtSshChannelCoreDataConsume`，并通过
`xrtSshChannelCoreAdjustReady`、`xrtSshChannelCoreAdjustLimit` 和
`xrtSshChannelCoreAdjustSendCommit` 事务性返还窗口。peer 窗口更新由
`xrtSshChannelCoreAdjustReceiveCommit` 应用。

`xrtSshChannelCoreEofSendCommit` 和 `xrtSshChannelCoreEofReceiveCommit` 只关闭对应数据方向；request
能力由 `xrtSshChannelCoreCanSendRequest` 与 `xrtSshChannelCoreCanReceiveRequest` 独立查询。
`xrtSshChannelCoreCloseSendCommit` 和 `xrtSshChannelCoreCloseReceiveCommit` 完成双向 close 后，阶段
进入 `XSSH_CHANNEL_CORE_CLOSED`。此前已经交给应用的数据仍可消费，但不会再产生 WINDOW_ADJUST。

channel core 不内置 request token。每个 channel 可独立组合 `xsshreplyqueue`，因此容量和存储位置
由应用按实际并发设置，空闲 channel 不承担固定 FIFO 成本。完整状态测试见
`tests/connection/test_ssh_channel_core.c`。
