# SSH Client Ed25519 Auth API

`ssh_client_auth_ed25519` 把已经解析的 `xsshed25519identity` 适配为客户端认证 provider。它直接发送
带签名的 RFC 4252 `publickey` 请求，不增加一次无签名 probe 往返，也不建立第二套认证状态机。

## 使用

把 `xrtSshClientEd25519Auth` 赋给 `xsshclientcoreconfig.Authenticate`，把在认证期间持续有效的
`xsshed25519identity` 地址赋给 `AuthenticateData`。身份通常由
`xrtSshPrivateKeyPemRead` 和 `xrtSshPrivateKeyEd25519Read` 从未加密 OpenSSH 私钥解析得到。

provider 借用身份、公钥和 seed，不复制私钥。客户端 core 的动态敏感输出先暂存签名原文，固定长度
签名在栈上生成并清零，随后同一输出缓冲被原子改写为最终请求。容量不足返回 `XSSH_ERROR_SPACE`，由
客户端 core 的有界增长循环重试；未协商 `publickey` 返回 `XSSH_ERROR_AUTHENTICATION`。

文件读取、口令解密、agent、HSM 和硬件签名不绑进该 helper。调用方仍可通过通用
`xsshclientauthproc` 实现这些策略，底层签名原文与 publickey writer 均保持公开。
