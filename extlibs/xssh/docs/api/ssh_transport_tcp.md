# SSH TCP Transport

`ssh_transport_tcp` 把 `ssh_transport_core` 的协议事务映射到 XRT TCP 的有界发送队列和
`xnetbuf` 接收链。它不创建 Engine、不连接地址、不持有 Stream，也不在每个会话中嵌入
8 KiB 或 35 KiB 数组。

## 初始化

`xrtSshTransportTcpConfigInit` 建立默认 packet、identification 和 rekey 预算。
真实 Stream 应在其 Worker 回调内初始化，并传入
`xrtNetWorkerBufPool(xrtNetStreamWorker(stream))`。这样输出 packet 直接写入 worker 缓冲块，
随后可由 `xrtNetStreamSendBuffer` 零复制接管。传入空池适合纯协议测试，但不能把该缓冲跨到
某个 Stream Worker。

适配对象只应由 Stream 所属 Worker 的单个执行流推进。`xrtSshTransportTcpCore` 暴露同一个
core，KEX 驱动可直接完成算法配置、NEWKEYS 密钥切换和 rekey 查询。

## 写入

1. `xrtSshTransportTcpIdentificationPrepare` 准备本端 identification；核心 packet 路径使用
   可注入 padding 的 `xrtSshTransportTcpWritePrepareWithPadding`。独立的
   `ssh_transport_tcp_random` 提供系统安全随机便利函数 `xrtSshTransportTcpWritePrepare`。
2. `xrtSshTransportTcpWriteSubmit` 把动态输出链交给 TCP。
3. 返回 `XNET_RESULT_AGAIN` 或 `ERROR` 时，输出块、密文、sequence、nonce 和协议状态均保持
   不变；调用方可在 writable/drain 后提交同一包，或调用 `WriteAbort`。
4. 返回 `OK` 表示 TCP 已可靠接管完整链，适配层已经提交对应 SSH 状态。之后取消上层任务
   不能回滚该包。

输出缓冲按 `xrtSshPacketCodecWriteMeasure` 的精确线长一次预留。适配对象固定部分只有 core、
缓冲链描述和事务元数据，实际 packet 大小只影响池化动态块。

## 读取

identification 与 packet 都采用 ReadPrepare / ReadCommit / ReadAbort。Prepare 返回的 banner、
payload 和 padding 都是借用视图，必须在当前 Worker 回调内解析并提交或拒绝。

`xrtSshTransportTcpReadInspect` 只从块链复制四字节公开长度头。完整 packet 到达后，
`ReadPrepare` 仅对当前 packet 调用 `xrtNetBufPullup`，不会把后续 packet 一起拼接。plain 模式
直接借用该前缀；AES-GCM 模式所需的精确解密区由 `Need.PlainSize` 给出并由调用方提供。
`ReadCommit` 成功后只消费当前线路包，尾随 packet 留在原链中。

```c
xsshpacketneed need;
xsshpacketview packet;
xsshrekeydecision rekey;

if ( xrtSshTransportTcpReadInspect(&transport, input, &need) == XSSH_OK ) {
	/* plain 按 need.PlainSize 准备；零表示 packet 可直接借用 input。 */
	if ( xrtSshTransportTcpReadPrepare(
		&transport, input, &packet, plain, plain_size, now_ms
	) == XSSH_OK ) {
		/* 同步解析 packet.Payload。 */
		xrtSshTransportTcpReadCommit(&transport, now_ms, &rekey);
	}
}
```
