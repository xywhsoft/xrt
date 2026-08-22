# SSH 密钥交换底层

密钥交换底层按职责拆成 ECDH 报文、SHA-256 transcript、Curve25519 原语和安全随机密钥对四层。
所有层只调用 XRT 公共 API，不包含私有密码实现或固定会话缓冲。

## ECDH 报文

- `xrtSshEcdhInitWrite` / `xrtSshEcdhInitRead`
- `xrtSshEcdhReplyWrite` / `xrtSshEcdhReplyRead`

报文模块只处理消息号和 SSH string，适用于 Curve25519 以及后续其他 ECDH 方法。解析结果借用原
payload，严格拒绝错误消息号、截断和尾随数据。

## Exchange Hash

`xrtSshKexHashSha256` 流式写入以下 transcript，不构造中间大缓冲：

`V_C || V_S || I_C || I_S || K_S || Q_C || Q_S || K`

前七项编码为 SSH string，共享秘密编码为规范非负 `mpint`。`SharedSecret` 接受大端 magnitude；
对于 Curve25519，应直接传 X25519 的 32 字节输出。RFC 8731 要求把这组字节重新解释为网络序
整数，不反转字节，再按 `mpint` 编码。

`xrtSshKexDeriveSha256` 实现 RFC 4253 的 A-F 密钥扩展，并支持超过 32 字节的输出。输出不得与
共享秘密、exchange hash 或 session id 重叠。共享秘密必须非零。

## Curve25519

- `xrtSshCurve25519Public`：从显式私钥导出公钥。
- `xrtSshCurve25519Shared`：计算共享秘密并拒绝低阶公钥产生的全零结果。
- `xrtSshCurve25519KeyPair`：使用 XRT 操作系统安全随机源生成临时密钥对。

纯 Curve25519 模块不依赖随机源，适合标准向量、确定性测试和自定义密钥管理；随机密钥对是独立
裁剪层。私钥在 transport 使用结束后必须调用 `xrtSecureZero` 清除。

## 主机密钥

ECDH reply 中的 `ServerHostKey` 与 `Signature` 可直接交给 `ssh_hostkey`。格式解析和
Ed25519 验证分别位于 `ssh_hostkey`、`ssh_hostkey_ed25519`，详见 `ssh_hostkey.md`。
签名验证完成后，客户端仍必须单独执行主机公钥信任判断。
