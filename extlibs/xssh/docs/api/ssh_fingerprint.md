# SSH 主机密钥指纹

`ssh_fingerprint` 对完整 SSH host-key blob 计算 SHA-256，并输出 OpenSSH 风格
`SHA256:<base64-no-padding>` 文本。实现只复用 XRT SHA-256 与 Base64，不解析算法、不分配内存，
因此同样适用于未来公钥算法和证书 blob。

`xrtSshHostKeyDigestSha256` 公开固定 32 字节原始摘要；
`xrtSshHostKeyFingerprintSha256` 支持空输出查询容量，实际输出容量必须额外包含末尾零字节。
输入、文本输出和长度输出不得重叠，任何失败都不会发布部分结果。

MD5 指纹只具有历史展示价值，不进入默认模块。需要兼容旧界面时，上层可以使用 XRT MD5 与
hex codec 组合，不应让主机信任路径默认携带 MD5。

```c
char Text[64];
size_t Size;

if ( xrtSshHostKeyFingerprintSha256(
	HostKeyBlob, Text, sizeof(Text), &Size
) == XSSH_OK ) {
	/* Text 可用于首次信任提示和审计日志。 */
}
```
