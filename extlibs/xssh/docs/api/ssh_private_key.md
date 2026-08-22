# SSH OpenSSH 私钥

私钥能力分成三个独立裁剪层，避免把文本、算法和秘密所有权塞进一个会话对象：

- `ssh_private_key` 解析二进制 `openssh-key-v1` 容器，保留 cipher、KDF、公开公钥序列和
  `PrivateList` 借用视图。未知加密算法不会在格式层被白名单拒绝。
- `ssh_private_key_pem` 复用 XRT PEM，把 `OPENSSH PRIVATE KEY` 文本解码到调用方缓冲。
- `ssh_private_key_ed25519` 严格解释未加密、单密钥 Ed25519 字段，并提供原始签名和最终 SSH
  signature blob 两条路径。

## 所有权与安全

所有结构都借用输入。PEM 查询模式先返回精确二进制长度，实际解码由调用方提供缓冲；该缓冲同时
承载 seed，使用结束后必须调用 `xrtSecureZero` 再释放。库不读取文件、不保存口令，也不把私钥复制
进 session。解析失败时结构化输出不变，但 PEM 二进制工作区在成功解码后可能已经写入。

容器层会预验证所有公开公钥 blob，并要求 `cipher=none` 与 `kdf=none` 成对出现。加密容器返回
元数据和密文，具体 cipher/KDF 层可在未来独立增加；当前 Ed25519 解析器对加密容器明确返回
`XSSH_ERROR_UNSUPPORTED`。

## 示例

```c
size_t BinarySize;
xsshopensshprivatekey PrivateKey;
xsshed25519identity Identity;

if ( xrtSshPrivateKeyPemRead(
	PemText, NULL, 0u, &BinarySize, NULL
) == XSSH_OK ) {
	/* 分配或复用 BinarySize 字节，再执行实际解码。 */
}

if ( (xrtSshPrivateKeyRead(BinaryBlob, &PrivateKey) == XSSH_OK) &&
	(xrtSshPrivateKeyEd25519Read(&PrivateKey, &Identity) == XSSH_OK) ) {
	/* Identity.Seed 仍借用 BinaryBlob。 */
}
```
