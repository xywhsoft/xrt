# SSH hashed known host

`ssh_known_host_hash` 实现 OpenSSH `|1|salt|hash` 兼容格式。该格式固定使用 20 字节 salt 与
HMAC-SHA1；SHA-1 只在显式选择本模块时进入闭包，普通 known_hosts 文本匹配不承担这项历史兼容
成本。

## API

- `xrtSshKnownHostHash` 输出原始 20 字节 HMAC，便于自定义数据库或测试。
- `xrtSshKnownHostHashWrite` 生成完整文本；先以空输出查询精确容量，实际容量包含末尾零字节。
- `xrtSshKnownHostHashMatch` 严格解析版本、salt 与 hash，并常量时间比较摘要。
- `xrtSshKnownHostLineHashMatch` 直接匹配 `xrtSshKnownHostLineRead` 得到的 hashed 行。

主机名在计算前执行 ASCII 小写折叠。端口 22 使用 `host`，其他端口使用 `[host]:port`；实现流式
更新 SHA-1，不为任意长度主机名分配或建立固定缓冲。salt 必须由密码学安全随机源生成；本模块
要求调用方显式提供 salt，确定性测试与生产随机策略因此保持分层。

## 示例

```c
unsigned char Salt[XSSH_KNOWN_HOST_HASH_SIZE];
char Text[80];
size_t Size;

xrtRandomSecure(Salt, sizeof(Salt));
if ( xrtSshKnownHostHashWrite(
	XRT_STR_LITERAL("host.example"), 22u,
	(xbytesview){ Salt, sizeof(Salt) }, Text, sizeof(Text), &Size
) == XSSH_OK ) {
	/* Text 可写入 known_hosts 的 hostnames 字段。 */
}
```
