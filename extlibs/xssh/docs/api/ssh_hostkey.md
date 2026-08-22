# SSH 主机密钥与签名

`ssh_hostkey` 是无分配的纯协议层，只依赖 `ssh_wire`。它公开通用公钥、通用签名以及
`ssh-ed25519` 固定格式的编解码，不包含密码实现、信任存储或网络状态。

## 通用格式

- `xrtSshPublicKeyRead` 读取算法名，并把算法名后的字段作为原始 `Parameters` 视图返回。
- `xrtSshSignatureRead` 严格读取 `string algorithm || string signature`，拒绝尾随数据。
- `xrtSshSignatureWrite` 直接写入调用方缓冲，不分配内存。

通用公钥参数保持原始编码，因此新增 RSA、ECDSA 或安全密钥算法时，不需要修改公共前缀解析器。

## Ed25519

- `xrtSshEd25519PublicKeyRead` / `xrtSshEd25519PublicKeyWrite`
- `xrtSshEd25519SignatureRead` / `xrtSshEd25519SignatureWrite`
- `xrtSshEd25519HostKeyVerify`

Ed25519 格式层严格要求 32 字节公钥和 64 字节签名。验证层单独依赖 XRT
`crypto_ed25519_verify`；签名不匹配返回 `XSSH_ERROR_AUTHENTICATION`，格式错误返回
`XSSH_ERROR_PROTOCOL`，不支持的算法返回 `XSSH_ERROR_UNSUPPORTED`。

验证签名只证明该 key blob 持有者签署了消息。客户端仍须通过 known-hosts、证书或应用回调判断
主机公钥本身是否可信；这一策略不会被隐藏在密码学验证函数中。
