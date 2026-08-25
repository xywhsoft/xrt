# SSH known_hosts 数据库策略

`ssh_known_host_db` 在单行格式层之上提供纯内存、无分配的数据库游标和常见主机信任判定。它同时
支持明文 pattern 与 OpenSSH `|1|` 哈希主机，但不拥有文件读取、追加、替换或锁；调用方可以直接
把映射文件、`xbuffer` 或其他持久化来源作为借用文本传入。

## 分层

- `xrtSshKnownHostDbInit` 初始化借用文本游标。
- `xrtSshKnownHostDbNext` 逐行返回完整记录，便于枚举多个 CA 或实现自定义 marker。
- `xrtSshKnownHostDbCheck` 直接比较握手得到的原始 host-key blob，不建立 Base64 解码副本。
- 非严格模式跳过坏行和未知 marker；严格模式将其返回为带行号的 `INVALID`。

常见判定的优先级为 `REVOKED > MATCH > CERT_AUTHORITY > CHANGED > NEW`。只要普通条目的主机模式
命中，但没有任何 host key 精确匹配，就返回 `CHANGED`；更换 RSA、ECDSA 或 Ed25519 算法不会降级
为 `NEW`。`CERT_AUTHORITY` 只说明存在匹配的 CA 候选，调用方必须继续验证主机证书，不能把该状态
直接当作认证成功。需要尝试多个 CA 或实施自定义算法迁移策略时，使用游标枚举相关记录。

## 示例

```c
xsshknownhostcheck Check;

if ( xrtSshKnownHostDbCheck(
	KnownHostsText,
	Host,
	Port,
	ServerHostKeyBlob,
	(uint32)XSSH_KNOWN_HOST_DB_STRICT,
	&Check
) == XSSH_OK ) {
	if ( Check.Trust == XSSH_KNOWN_HOST_TRUST_MATCH ) {
		/* 普通主机密钥精确匹配。 */
	}
}
```
