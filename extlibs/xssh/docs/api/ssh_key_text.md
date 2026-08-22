# SSH 公钥文本

`ssh_key_text` 解析 OpenSSH public-key 与 `authorized_keys` 单行文本。它把 options、算法、Base64
和注释作为原输入的借用视图公开，Base64 公钥 blob 解码到调用方缓冲，不分配内存，也不保存
密钥数据。

## 分层

- `xrtSshPublicKeyLineRead` 处理文本字段、quoted option 边界，并校验文本算法与 Base64 blob 前缀。
- `xrtSshPublicKeyLineDecode` 复用 XRT Base64，随后复用 `xrtSshPublicKeyRead` 验证 blob。
- `xrtSshPublicKeyLineMatch` 逐组比较 Base64 与原始 blob，不创建完整编码或解码副本。
- 算法名不使用固定白名单；扩展算法只要符合 SSH name 规则即可进入格式层。
- options 只保留原文，不在底层解释 `command`、`from`、`restrict` 等授权策略。

`xsshopensshkeyline` 的四个文本视图都借用原始行，原始行必须至少保持到解码完成。
`BlobSize` 是精确的调用方缓冲需求。解码失败时 `pPublicKey` 不变；`pBlob` 是工作区，在 Base64
已成功但后续 blob 语义失败时可能已经写入，调用方不应把失败工作区当作有效密钥。

## 示例

```c
static const char sLine[] =
	"restrict ssh-ed25519 "
	"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
unsigned char arrBlob[64];
xsshopensshkeyline Line;
xsshpublickey Key;

if ( (xrtSshPublicKeyLineRead(XRT_STR_LITERAL(sLine), &Line) == XSSH_OK) &&
	(xrtSshPublicKeyLineDecode(
		&Line, arrBlob, sizeof(arrBlob), &Key
	) == XSSH_OK) ) {
	/* Key 与 Line 都借用调用方存储。 */
}
```
