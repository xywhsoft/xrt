# SSH Transport Core

`ssh_transport_core` 是 packet codec、协议顺序和 rekey 预算之间的无缓冲组合层。它不创建 socket、
Engine、future、协程或系统时钟，也不保存 KEX payload、密钥原文和应用消息。同步、事件回调、future
与协程客户端可以驱动同一个状态契约。

## 发送事务

`xrtSshTransportCoreWritePrepareWithPadding` 直接把最终线路包写入调用方缓冲，同时自动识别 KEXINIT、
NEWKEYS 和 USERAUTH_SUCCESS。准备成功后 sequence、GCM nonce、协议状态和 rekey 预算均未推进：

- XRT 发送队列成功接收包后调用 `xrtSshTransportCoreWriteCommit`。
- 返回 `XRT_NET_AGAIN` 时保留同一线路包，等待 writable 后重试入队，不得重新准备。
- 决定不发送时调用 `xrtSshTransportCoreWriteAbort`，丢弃本次 writer 新增字节。

任一时刻每个方向最多一包待提交。该约束避免背压期间复用 sequence/nonce，也不会为队列再复制一份包。

## 接收事务

先用 `xrtSshTransportCoreInspect` 读取四字节长度头，按 `WireSize` 聚合完整输入并按 `PlainSize` 准备
可复用解密工作区。`xrtSshTransportCoreReadPrepare` 完成认证、消息分类和状态检查，随后上层解析
借用的 payload：

- 解析和策略接受后调用 `xrtSshTransportCoreReadCommit`。
- 上层拒绝已认证包时调用 `xrtSshTransportCoreReadAbort`；codec 已经消费该包，因此 transport 会关闭，
  不会伪装回滚序列号继续通信。

完整网络包发生认证失败、非法 padding、超限或 codec 状态错误时，`ReadPrepare` 会直接关闭 core。
`NEED_MORE`、工作区不足和调用参数错误不会关闭，调用方修正输入条件后可以重试。

## NEWKEYS

本端 NEWKEYS 可靠入队后，写方向保持关闭，直到 `xrtSshTransportCoreSetWriteAesGcm` 成功。对端
NEWKEYS 认证后同理由 `xrtSshTransportCoreSetReadAesGcm` 激活。strict-kex 序列重置、方向性 rekey
计数清零和 cipher 切换在同一个调用内完成。只有两个方向的新密钥都实际生效后，
`xrtSshTransportCoreKexComplete` 才返回真。

Core 当前组合 `ssh_packet_codec` 已公开的 plain 与 OpenSSH AES-GCM 模式。未来增加 cipher 时扩展
packet codec 和对应激活函数，不改变网络驱动、缓冲所有权或 Prepare/Commit 契约。

## 网络适配

Core 的发送缓冲可直接交给 `xrtNetStreamSend`、`xrtNetStreamSendRef` 或自定义队列；接收端可借用
`xrtNetStreamBuffer` 的连续区间。网络适配层只负责背压、生命周期、deadline 和 cancel，不重复实现
SSH packet、KEX 顺序或 rekey 规则。
