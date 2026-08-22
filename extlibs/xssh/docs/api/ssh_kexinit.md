# SSH KEXINIT

KEXINIT 层负责算法清单构建、严格解析、RFC 4253 协商以及
`first_kex_packet_follows` 猜测处理。它不执行密钥交换，不依赖密码算法或网络。

## 默认算法

当前默认值只公布已经具备底层闭环的现代算法：

- KEX：`curve25519-sha256,curve25519-sha256@libssh.org`
- host key：`ssh-ed25519`
- cipher：`aes128-gcm@openssh.com,aes256-gcm@openssh.com`
- MAC：`hmac-sha2-256,hmac-sha2-512`
- compression：`none`

首次 KEX 根据角色在算法清单末尾增加 `ext-info-c/s`、标准 `kex-strict-c/s` 和预标准
`kex-strict-c/s-v00@openssh.com`。只有双方使用同一套 strict 名称时才启用 strict-kex，标准名与
预标准名不会交叉匹配。这些名称只用于能力协商，不会被选为真实 KEX 算法；本端重协商配置自动
回到不含标记的 `XSSH_KEX_DEFAULT`，对端在重协商中重复发送的首次标记按协议忽略。
`first_kex_packet_follows` 判断同样会跳过位于清单任意位置的扩展标记。

默认 MAC 仍写入 KEXINIT 以满足协议字段要求；选中 AEAD cipher 后，协商结果不会消费 MAC。
调用方可以替换任一列表，纯协商层不会限制扩展算法。

## 构建

- `xrtSshKexInitConfigInit`：按 client/server 角色和 initial/rekey 阶段初始化安全默认清单。
- `xrtSshKexInitWrite`：使用调用方提供的 16 字节 cookie 构建 payload。
- `xrtSshKexInitWriteSecure`：使用 XRT 操作系统 CSPRNG 生成 cookie。

角色配置不可省略，避免低层默认值发送错误方向或在重协商中重复发送首次标记。纯协议 API 不提供
零 cookie 默认值；测试、确定性复现或自定义随机源必须显式传 cookie，生产常规路径直接使用
`xrtSshKexInitWriteSecure`。

## 解析与协商

- `xrtSshKexInitRead`：返回借用 payload 的 `xsshkexinit`，严格拒绝非法 name-list、非零保留字段和尾随数据。
- `xrtSshKexNegotiate`：始终按客户端列表顺序选择服务端支持的第一项。
- `xrtSshKexFeatures`：判定本端是否接受/发送 EXT_INFO，以及双方是否协商 strict-kex。
- `xrtSshCipherIsAead`：判断当前已知的 AEAD cipher。
- `xrtSshKexGuessSkip`：只在 peer 声明 follows 且 KEX/host-key 任一首项猜错时要求丢弃下一包。

所有失败都保持 writer 和输出结构不变。解析结果以及协商结果中的字符串视图依赖原 KEXINIT payload
保持有效。

```c
unsigned char payload[512];
xsshwriter writer;
xsshkexinit parsed;
xsshkexinitconfig config;

xrtSshKexInitConfigInit(&config, XSSH_ROLE_CLIENT, true);
xrtSshWriterInit(&writer, payload, sizeof(payload));
xrtSshKexInitWriteSecure(&writer, &config);
xrtSshKexInitRead(
	(xbytesview){ payload, writer.Size },
	&parsed
);
```
