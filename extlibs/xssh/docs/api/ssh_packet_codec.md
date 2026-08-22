# SSH Packet Codec

`ssh_packet_codec` 是 transport 共用的无分配 packet 状态层。它把初始 plain packet 与
OpenSSH AES-GCM packet 组合成同一套读写契约，但不拥有 socket、收发队列或工作缓冲。

## 状态

- `xsshpacketcodec` 分别保存读写模式、RFC 4253 序列号和 AES-GCM 状态。
- `xrtSshPacketCodecInit` 从双向 plain 模式开始；零上限使用 `XSSH_PACKET_MAX_DEFAULT`。
- `xrtSshPacketCodecSetWriteAesGcm` 在本端 NEWKEYS 已入队后切换写方向。
- `xrtSshPacketCodecSetReadAesGcm` 在 peer NEWKEYS 已认证后切换读方向。
- 普通 rekey 不重置序列号；协商 OpenSSH strict-kex 后，调用方在各自 NEWKEYS 边界调用对应的 sequence reset。
- `xrtSshPacketCodecClear` 清除两个方向的密钥、nonce 与计数器。

同一方向不能由多个线程并发推进。不同方向可以由调用方在外部保证生命周期后分别驱动。

## 缓冲

`xrtSshPacketCodecInspect` 只需要四字节 `packet_length`，即可返回完整线长和 AES-GCM
解密工作区大小。`xrtSshPacketCodecWriteMeasure` 按当前写方向精确返回 payload 的
`packet_length` 和最终线路长度。transport 因此可以按实际包长使用动态 buffer、池化 block
或调用方固定预算，不需要在每个 session 内嵌 8 KiB 数组。

plain 模式的 `Payload`/`Padding` 借用输入；AES-GCM 模式借用 `pPlain`。失败时 reader、
writer、packet view、序列号和 cipher invocation 都不会推进。认证失败不会修改明文输出。

## 写入

- `xrtSshPacketCodecWritePrepareWithPadding` 生成最终线路包，但不消费 sequence 或 GCM nonce。
- 网络层可靠接收该包后调用 `xrtSshPacketCodecWriteCommit`；`AGAIN` 时保留线路包并重试，未发送时调用 `xrtSshPacketCodecWriteAbort`。
- 准备未决期间禁止准备第二包、切换写密钥或重置写序列，避免复用同一 nonce。
- `xrtSshPacketCodecWriteWithPadding` 是无需背压重试时的准备加提交便利路径。
- 可选 `ssh_packet_codec_random` 的 `xrtSshPacketCodecWrite` 使用 XRT 系统安全随机源，适合默认生产路径。
- 同一随机便利层的 `xrtSshPacketCodecWritePrepare` 保留显式可靠入队边界。
- 只选择核心 codec 不会引入 `random_secure`；高吞吐 transport 可自行提供安全播种的会话级 PRNG。
- codec 的 `MaxPacketSize` 同时限制接收和发送，避免本端生成 peer 不应接受的大包。

```c
xsshpacketneed need;
xsshpacketcodec codec;
xsshreader reader;

xrtSshPacketCodecInit(&codec, 0);
xrtSshReaderInit(&reader, received);
if ( xrtSshPacketCodecInspect(&codec, &reader, &need) == XSSH_OK ) {
	/* 按 need.WireSize 聚合输入，并按 need.PlainSize 准备解密工作区。 */
}
xrtSshPacketCodecClear(&codec);
```
