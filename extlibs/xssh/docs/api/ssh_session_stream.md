# SSH callback Stream 驱动

## 定位

`ssh_session_stream` 把公共 `xnetstream` 事件与 `xsshsessiontcp`、`xsshsessionreader` 的事务边界组合成
一个最小 callback 驱动。它拥有 SSH TCP 会话和动态 Reader，借用 Stream、所属 Worker 的缓冲池、
配置外部对象和用户数据；不会创建或持有 Engine、任务、future、协程、凭据、信任数据库、channel
表或后台线程。

驱动只负责三件事：增量消费 Stream 接收链、把应用已经 Prepare 的完整输出交给有界 TCP 队列、在
HOLD、内存重试和写背压期间暂停新读取。KEX 算法、主机信任、认证方法、channel 存储与服务策略仍
通过底层会话公开 API 由应用选择，不会被 callback 便利层固化。

## 建立连接

先分别初始化 `xsshsessiontcpconfig` 与 `xsshsessionstream`。客户端把
`xrtSshSessionStreamNetEvents()` 和适配器地址直接传给 `xrtNetStreamConnect`；服务端在 Listener 的
`Accept` 回调中调用 `xrtSshSessionStreamAttach`。Attach 必须发生在 Stream 所属 Worker 上，并要求
当前接收链尚无积压字节。

初始化只复制配置和事件表，不占用网络或动态缓冲。`Open` 发布前，客户端适配器尚未附着 Stream；
服务端 Attach 可以在网络层正式 Open 事件之前完成，驱动会把这两个阶段合并为一次用户 Open 通知。

## 动作与输出

`Action` 在会话动作变化时发布。应用可以通过 `xrtSshSessionStreamSession` 下钻到底层会话，并调用
identification、KEX、认证或 connection 的 Prepare API。动作变为 `WRITE_PENDING` 后，驱动自动调用
`xrtSshSessionTcpWriteSubmit`；成功入队才提交 SSH 状态，`XNET_RESULT_AGAIN` 则暂停读取并在 LowWater
或 Drain 后重试同一完整事务。

驱动不会代替应用构造消息，也不会建立属性对象后重新拼包。所有高性能路径仍直接写入 Worker 动态
链，并沿 `Prepare -> SendBuffer -> Commit` 事务只移动一次所有权。

## 输入决策

Identification 与 Packet 回调返回三种决策：

- `ACCEPT` 立即提交当前借用输入并继续解析同一接收链；
- `HOLD` 暂停新读取，保留唯一未决事务，等待显式 `xrtSshSessionStreamAccept` 或 Reject；
- `ABORT` 回滚未决事务并请求异常关闭。

HOLD 期间可通过 `xrtSshSessionStreamVersion` 或 `xrtSshSessionStreamPacket` 访问借用视图。视图只存活到
接受、拒绝或关闭，不得跨该边界保存。普通明文 packet 仍直接借用 Stream 输入；只有密码解包或稳定
主机公钥需要 Reader 按实际长度申请动态块。

## 内存失败与 EOF

Reader 的普通分配失败使驱动进入 `RETRY`，保留同一未消费输入并暂停新读取。应用释放 Worker 内存后
在所属 Worker 调用 `xrtSshSessionStreamDrive` 即可重试；也可以 Abort。错误回调只通知本次失败，不
自动把可恢复的空间不足改成协议关闭。

EOF 到达时，驱动先发布 End，再推进已到达的完整消息。接收链为空表示正常 peer 结束；只有仍残留
无法组成完整 identification 或 packet 的字节才作为截断协议错误关闭。EOF 若发生在 HOLD 或 RETRY
期间，驱动保留当前事务，等待应用决定或内存恢复；若完整输出尚受 TCP 背压，则先在 LowWater/Drain
中提交该输出，再请求排空关闭。

## 线程与所有权

所有用户事件、Drive、Accept、Reject 和 Abort 都必须在绑定 Stream 的 Worker 上执行。其他线程应
通过 `xrtNetPost` 或 `xrtNetEnginePost` 投递操作。Pause/Resume、HOLD 和写重试始终由同一驱动串行化，
不会从 callback 之外并发修改 SSH 会话。

Close 回调期间 Stream、Session 和 Reader 仍可只读检查；回调返回后驱动释放全部池化动态块，状态变为
`CLOSED`。随后调用 `xrtSshSessionStreamClear` 清除适配器。活动连接不得直接 Clear。
