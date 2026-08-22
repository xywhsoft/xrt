# SSH 安全 Padding

`ssh_packet_random` 把 XRT 的操作系统密码学安全随机源接到 SSH packet padding 契约。它只依赖
`ssh_packet` 与 `random_secure`，不会引入密码算法或网络。

## API

- `xrtSshSecurePadding`：可传给 plain packet 或加密 packet 的标准 padding 回调。
- `xrtSshPacketWriteSecure`：用系统安全随机源直接构建 plain packet。

系统随机调用失败时，packet writer、序列号及已提交长度保持不变。高吞吐 transport 可以使用
`xrtSshPacketWrite` 并注入经过安全播种的会话级 PRNG，避免每包进入操作系统；随机源的安全性由
transport 负责。

```c
unsigned char wire[64];
xsshwriter writer;

xrtSshWriterInit(&writer, wire, sizeof(wire));
xrtSshPacketWriteSecure(
	&writer, XRT_BYTES_LITERAL("payload"), 8u, NULL
);
```
