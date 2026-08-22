# SSH Publickey Auth API

`ssh_auth_publickey` 在公共 USERAUTH 与 host-key 格式层上实现 RFC 4252 publickey 方法，不绑定
具体密钥算法、私钥存储或 signer。`xrtSshAuthPublicKeyWrite` 构建免签名 probe，
`xrtSshAuthPublicKeySignedWrite` 构建带签名请求，`xrtSshAuthPublicKeyRead` 严格读取两种形式。

`xrtSshAuthPublicKeySignDataWrite` 直接生成 `string(session_id) || USERAUTH_REQUEST` 签名原文，
调用方可以把连续结果交给硬件密钥、agent、XRT 密码算法或自定义 signer。
`xrtSshAuthPublicKeyOkWrite/Read` 实现服务端 probe 接受消息。

公钥 blob 的内部算法允许与请求算法不同，例如 RSA SHA-2 请求仍使用 `ssh-rsa` 公钥格式；
带签名请求则严格要求请求算法与 signature blob 的算法相同。所有结果借用原始 payload，构建器
无分配并保持失败原子性。

示例见 `examples/auth_publickey/main.c`。
