# SSH AES-GCM Packet

`ssh_packet_aes_gcm` 实现 `aes128-gcm@openssh.com` 与 `aes256-gcm@openssh.com` 共用的 packet
封装。四字节 `packet_length` 保持明文并作为 AAD，`padding_length | payload | padding` 原位加密，
末尾附加十六字节认证标签。

## 状态

- `xsshaesgcm` 是单向状态；读写方向必须各自初始化，不能并发推进同一状态。
- `xrtSshAesGcmInit` 接受 16 或 32 字节 AES 密钥和 12 字节 initial IV。
- initial IV 的前四字节是 fixed IV，后八字节是大端 invocation counter。
- counter 在每个成功 packet 后递增；达到 `UINT64_MAX` 前停止，绝不回绕复用 nonce。
- `xrtSshAesGcmClear` 清除密钥、IV 与 counter。

## Packet API

- `xrtSshAesGcmMeasure`：计算十六字节对齐的 `padding_length` 与 `packet_length`。
- `xrtSshAesGcmWrite`：直接在 writer 未提交区域构建并原位加密，不分配堆内存。
- `xrtSshAesGcmRead`：先认证再解密到调用方缓冲，成功后返回借用该缓冲的 packet view。
- `xrtSshAesGcmInvocation`：读取下一包使用的 invocation counter。

写入的 padding 来源由调用方决定。生产环境可传 `xrtSshSecurePadding`，高吞吐会话可传安全播种的
会话级 PRNG。容量不足、截断、padding 回调失败、认证失败或状态耗尽都不会推进 reader、writer、
sequence 或 invocation counter。认证失败也不会修改明文输出；认证成功但 packet 结构非法时会清零
已经解密的 packet body。

`pPlain` 不得与输入 packet 或 AES-GCM 状态重叠。成功后 `Payload` 与 `Padding` 只在调用方保持
`pPlain` 有效且未复用时有效。

写入时 `Payload` 不得与 writer 的本次输出区间重叠；这是为了维持无分配原位加密路径，并避免
四字节长度头覆盖尚未读取的 payload。

```c
xsshaesgcm write_state;
xsshwriter writer;

xrtSshAesGcmInit(&write_state, key, initial_iv);
xrtSshWriterInit(&writer, output, output_size);
xrtSshAesGcmWrite(
	&writer,
	payload,
	&write_state,
	&sequence,
	xrtSshSecurePadding,
	NULL
);
xrtSshAesGcmClear(&write_state);
```
