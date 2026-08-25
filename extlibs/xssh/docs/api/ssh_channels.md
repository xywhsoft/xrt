# SSH Channels API

`ssh_channels` 是 `ssh_connection_session` 与经典应用场景之间的动态所有权层。它按本端 channel id
保存稳定地址的 `xsshchannel`，每项组合 `xsshchannelcore`、可选动态 I/O 和回复 FIFO，并可直接作为
`xsshchannelresolveproc` 使用。集合不创建 socket、Engine、future、任务或事件队列。

## 内存与上限

`xrtSshChannelsInit` 创建空整数映射，不预分配 channel、报文缓冲或回复 token。`Open` 和 `Accept`
只在出现真实 channel 时创建一个映射节点；DATA、stderr 和发送队列仍由 `xnetbuf` 按实际内容申请。
回复 token 由 `xrtSshChannelReplyReserve` 在发送 `want-reply` 请求前按需扩展，迁移时保持已有 FIFO
顺序。这样空连接没有旧版固定 channel 数组，每个空 channel 也没有固定 8 KiB/16 KiB 数据区。

配置中的 `MaxChannels`、`ReplyLimit`、接收窗口、最大 packet 和 I/O 预算都是硬边界。默认最多
1024 个活动 channel、每 channel 64 个待回复请求、2 MiB 接收窗口、32 KiB packet，以及收发各
2 MiB 动态数据预算。应用应按负载模型缩小或放大它们，不能依赖无限增长。

## 生命周期

`xrtSshChannelsOpen` 创建本端发起、等待 confirmation 的 channel；调用方随后可直接使用公开
`Core.Local` 构建 `CHANNEL_OPEN`。`xrtSshChannelsAccept` 从已解析的 peer open 创建等待本端接受或
拒绝的 channel。返回地址在对应项删除前稳定，因此 connection session、异步等待和应用状态可以
安全借用，但不得在 `Remove`、`Discard` 或集合清理后继续保存。

`xrtSshChannelsRemove` 只删除已经 FAILED/CLOSED、没有未决事务、未消费数据、待发送数据或回复的
channel，避免便利层静默丢失应用可观察状态。连接关闭或策略拒绝需要放弃数据时使用显式
`xrtSshChannelsDiscard`。迭代期间不得创建或删除 channel。

组合客户端或服务器可通过 `xrtSshChannelsOnRemoved` 安装唯一的
`xsshchannelsremovedproc` 删除观察器。观察器只在 `Remove` 或 `Discard` 已成功后同步收到失效的
本端 id，不会收到初始化回滚或集合整体清理通知。该钩子用于关闭外层 future、任务或索引，
不向底层集合引入这些模块的依赖。回调不得继续访问已经删除的 channel；需要保留的信息必须在
删除前复制到外层状态。

## 组合

```c
xsshchannels Channels;
xsshconnectionsession Session;
xsshreplyqueue GlobalReplies;
xsshchannel* pChannel;

xrtSshReplyQueueInit(&GlobalReplies, NULL, 0u);
xrtSshChannelsInit(&Channels, pPool, NULL);
xrtSshConnectionSessionInit(
	&Session,
	XSSH_ROLE_CLIENT,
	xrtSshChannelsResolve,
	&Channels,
	&GlobalReplies
);

xrtSshChannelsOpen(&Channels, &pChannel);
/* 使用 pChannel->Core.Local 构建 session CHANNEL_OPEN。 */
xrtSshChannelReplyReserve(pChannel, 1u);
```

协议驱动仍可完全跳过本模块，把自有数组、哈希表、slab 或代理对象接到公开 resolver；动态集合是
经典客户端和服务器的标准组合层，不是底层协议的强制存储模型。
