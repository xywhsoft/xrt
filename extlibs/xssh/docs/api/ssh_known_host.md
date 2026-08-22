# SSH known_hosts

`ssh_known_host` 是 OpenSSH known_hosts 的无分配明文格式层。它解析可选 marker、host pattern-list、
算法、Base64 key 和注释，并复用 `ssh_key_text` 解码公钥 blob。文件读取、追加、锁和首次信任策略
不属于格式层，由上层使用 XRT 文件 API 组合。

`xrtSshKnownHostLineKeyMatch` 可直接比较握手得到的完整 host-key blob，不需要调用方为每条记录
分配解码缓冲。多行数据库扫描与 `MATCH / CHANGED / REVOKED / CA` 策略由独立
`ssh_known_host_db` 层提供。

## 匹配契约

- 普通主机名按 ASCII 大小写不敏感规则匹配。
- `*` 匹配任意长度，`?` 匹配一个字节；实现不递归，也不构造临时主机字符串。
- `!pattern` 命中时优先返回 `XSSH_KNOWN_HOST_NEGATED`。
- 端口 22 直接匹配 `host`；其他端口虚拟匹配 `[host]:port`。
- `Host` 参数始终传未加方括号的主机名或 IP 地址。
- hashed host 由独立裁剪层处理，明文 matcher 返回 `XSSH_ERROR_UNSUPPORTED`。

`@cert-authority` 和 `@revoked` 映射为明确枚举。未来 marker 仍作为原始视图保留并标记为
`XSSH_KNOWN_HOST_MARKER_UNKNOWN`，格式层不会替上层静默信任。

## 示例

```c
xsshknownhostline Line;
xsshknownhostmatch Match;

if ( (xrtSshKnownHostLineRead(Text, &Line) == XSSH_OK) &&
	(xrtSshKnownHostLineMatch(
		&Line, XRT_STR_LITERAL("host.example.com"), 22u, &Match
	) == XSSH_OK) && (Match == XSSH_KNOWN_HOST_MATCH) ) {
	/* 再按 MarkerKind、Algorithm 和 key blob 执行信任策略。 */
}
```
