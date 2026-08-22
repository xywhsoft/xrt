# SSH Connection Session API

`ssh_connection_session` 在已认证的 `ssh_transport_core` 上编排 RFC 4254 全局消息和 channel 消息。
它不拥有网络、packet 缓冲、channel 表、data 队列或任务对象，也不分配内存。调用方可按连接规模
选择数组、哈希表、slot map 或 slab 存储 `xsshchannelcore`，会话仅通过本地 recipient 解析回调取得
当前对象。

## 开始会话

`xrtSshConnectionSessionInit` 固定本端角色、channel resolver 和可选的全局 `xsshreplyqueue`。
`xrtSshConnectionSessionBegin` 只接受首轮 KEX 已完成、server `USERAUTH_SUCCESS` 已按正确方向提交且
当前没有 packet 事务的 transport。连接层不依赖 `ssh_auth_session`，因此应用可以使用相同的标准
认证编排，也可以提供满足 transport 契约的自定义认证实现。

全局回复 FIFO 在初始化和开始会话时都必须为空，不能携带前一个 transport 的协议位置。每个
channel 必须使用自己的 FIFO，不能与全局 FIFO 混用；不同 channel 也不应共享 FIFO，因为它们的
响应顺序彼此独立。

resolver 按本地 channel id 返回 `xsshchannelcore` 和该 channel 的可选回复 FIFO。回调只是存储选择，
不是用函数表隐藏 SSH 依赖。单个连接会话、它引用的 channel 和回复队列必须由同一个执行流串行推进；
跨线程调度应在调用这些函数之前完成。

## 写事务

最终 payload 先传给 `xrtSshConnectionSessionWritePrepare`。函数严格重读消息并在内部副本上验证
channel 状态；channel request 设置 `want-reply` 时，调用方同时传入该 channel 的 FIFO 和任意
`uint64` token。全局 request 使用初始化时配置的全局 FIFO。随后依次执行：

1. `xrtSshTransportCoreWritePrepareWithPadding`
2. 网络队列可靠接管完整线路 packet
3. `xrtSshTransportCoreWriteCommit`
4. `xrtSshConnectionSessionWriteCommit`

只有最后一步才提交 channel 窗口、EOF/CLOSE 和 FIFO token。网络背压前可调用
`xrtSshTransportCoreWriteAbort` 与 `xrtSshConnectionSessionWriteAbort` 放弃两个候选，外部状态不变。

## 读事务

transport 完成认证并返回 payload 后，调用 `xrtSshConnectionSessionReadPrepare` 得到
`xsshconnectionpacket`。字符串、字段和 data 都借用当前 transport read 事务，只能在 read commit
或 abort 前访问。随后先提交 transport，再调用 `xrtSshConnectionSessionReadCommit`；会话此时原子
提交 channel 和回复 FIFO 状态。

收到 `CHANNEL_OPEN` 时还没有本地 recipient，因此不会调用 resolver。策略层应在借用期内检查类型
和字段，复制自身需要长期保留的数据，并保存 `Sender`、`Window`、`MaxPacket` 三个值；transport 与
connection read 提交后，再选择本地 id 并用 `xrtSshChannelCoreAcceptInit` 建立调用方对象。拒绝路径
无需建立持久 channel core，可直接构建 open failure。

收到 channel 或 global success/failure 时，`ReplyToken` 与 `HasReplyToken` 返回对应 FIFO 队首位置。
取消某个等待者不能删除中间 token；应用应保留协议位置，收到响应后再决定是否发布结果。

## 路由与失败

会话只识别 RFC 4254 的消息 80..100。KEX、transport 控制、认证扩展和未来上层消息返回
`XSSH_ERROR_UNSUPPORTED`，外层驱动可在同一 transport read 事务中交给其他模块；会话不会因此失败。
已识别但格式错误、recipient 不存在、窗口越界或无待处理回复属于连接协议错误，会话立即失败，调用方
必须用 `xrtSshTransportCoreReadAbort` 关闭承载连接。

`xrtSshConnectionSessionReadAbort` 用于应用拒绝一个已经认证但无法接受的 connection packet，它同样
终止会话。`xrtSshConnectionSessionFail` 可在外部存储或策略故障时显式关闭编排状态。

完整双端事务范例见 `tests/connection/test_ssh_connection_session.c`。
