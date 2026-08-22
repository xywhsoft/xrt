# SSH Channel I/O API

`ssh_channel_io` 是 `ssh_channel_core` 之上的可选动态缓冲层。它替代旧运行时每个 channel 固定
16 KiB stdout、16 KiB stderr 和 16 KiB pending-write 数组，但不把缓冲重新塞回协议核心。
对象借用一个 `xsshchannelcore`，并用同一 `xnetbufpool` 管理普通数据、标准错误、两条发送队列和
单次接收预分配；空闲 channel 只保留五个空链头，不分配数据块。

## 预算

`xrtSshChannelIoConfigInit` 默认给接收和发送各 2 MiB 硬上限。接收上限是 DATA 与 STDERR 的共享
预算，并且初始化时必须不小于 channel 已经通告给 peer 的接收窗口；这样 peer 在协议允许范围内
发送时不会因为固定对象容量而产生功能死角。发送上限同样由两条流共享，达到上限后所有追加入口
返回 `XSSH_ERROR_SPACE`，调用方可以等待远端 `WINDOW_ADJUST` 和队列消费后重试。

发送数据可通过 `xrtSshChannelIoWrite` 复制，也可使用 `WriteBorrow`、`WriteTake`、`WriteRef` 或
`WriteBuffer` 选择借用、接管和整链移动。零长度操作成功但不转移所有权。`Readable`、`Queued`、
`Writable` 和 `SendLimit` 都是不分配的快照；`ReadBuffer` 借出只读 `xnetbuf`，应用可遍历 span 后
用 `Consume` 零复制消费。

## 收发事务

接收 data 时先调用 `xrtSshChannelIoReceivePrepare`。函数在临时链中完成复制和容量检查，并在 channel
副本上验证 recipient、最大包与接收窗口，不修改正式状态。随后外层提交 transport 和
`ssh_connection_session`；channel core 已更新后调用 `ReceiveCommit`，预分配块只做链移动，不再
分配。内存不足时可以保留 transport 读事务、暂停网络并重试；决定拒绝输入时先回滚或关闭外层，
再调用 `ReceiveAbort` 释放临时块。

发送时 `xrtSshChannelIoSendPrepare` 从指定队首借出一个连续 span，并同时受远端窗口、远端最大包和
writer 剩余空间约束，直接生成最终 `CHANNEL_DATA` 或 stderr `CHANNEL_EXTENDED_DATA` payload。
外层可靠提交 transport 与 connection/channel 状态后调用 `SendCommit` 消费相同队首；背压或取消
则在外层回滚后调用 `SendAbort`，队列和远端窗口均保持不变。

同一对象由一个执行流推进，且任一时刻只允许一个收发事务。这与 `ssh_connection_session` 的短事务
边界一致，避免窗口、队首和 packet ordinal 交错。未知 extended-data type 不进入 STDERR 便利缓冲，
调用方可直接使用 connection session 返回的借用视图处理，因此扩展能力没有被 type 1 固化。

## 示例

```c
xsshchannelioconfig Config;
xsshchannelio Io;
xsshwriter Writer;
xbytesview Payload;
unsigned char arrPayload[32768];

xrtSshChannelIoConfigInit(&Config);
Config.ReceiveLimit = 2u * 1024u * 1024u;
Config.SendLimit = 2u * 1024u * 1024u;
xrtSshChannelIoInit(&Io, pPool, &Channel, &Config);

xrtSshChannelIoWrite(&Io, XSSH_CHANNEL_IO_DATA, pData, iSize);
xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload));
if ( xrtSshChannelIoSendPrepare(
	&Io,
	XSSH_CHANNEL_IO_DATA,
	&Writer,
	&Payload
) == XSSH_OK ) {
	/* transport 与 connection 可靠提交 Payload 后： */
	xrtSshChannelIoSendCommit(&Io);
}
```

直接追求最低复制次数的协议驱动仍可跳过本模块：使用 `ssh_connection_session` 借出的 data view，
在 transport read commit 前完成同步消费，并显式推进 `xsshchannelcore`。动态缓冲层是便利组合，
不是强制数据路径。
