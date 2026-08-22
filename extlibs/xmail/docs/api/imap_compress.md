# IMAP COMPRESS

`imap_compress` 为 IMAP 客户端增加 RFC 4978 `COMPRESS=DEFLATE`。模块使用持续的 raw-DEFLATE 双向流，不改变 `ximapclient` 的命令、事件和 literal 接口。

## 裁剪

- 模块宏：`XMAIL_MODULE_IMAP_COMPRESS`
- 功能宏：`XMAIL_FEATURE_IMAP_COMPRESS`
- 直接依赖：`imap_client`、`mail_net_deflate`
- 底层依赖：XRT `deflate`、`inflate`
- 不会隐式引入 TLS、认证、常用命令或 APPEND helper

## 配置

```c
ximapcompressconfig Config;

xrtImapCompressConfigInit(&Config);
Config.Level = XDEFLATE_LEVEL_DEFAULT;
Config.Strategy = XDEFLATE_STRATEGY_DEFAULT;
Config.WindowBits = XDEFLATE_WINDOW_MAX;

if ( !xrtImapCompressConfigValid(&Config) ) {
	return false;
}
```

`WindowBits` 同时约束发送和接收方向。该模块固定使用 raw-DEFLATE，调用方不能改成 zlib 或 gzip 封装。

## 协商

```c
if ( (xrtImapClientCapabilities(Client) &
	 XIMAP_CAP_COMPRESS_DEFLATE) != 0 ) {
	if ( !xrtImapClientCompress(Client, &Config, Deadline, Cancel) ) {
		return false;
	}
}
```

`xrtImapClientCompress` 仅允许在已认证或已选择邮箱的会话中调用。函数先以当前传输发送 `COMPRESS DEFLATE`，完整读取 tagged completion，并且只在 tagged `OK` 后原子切换收发两侧。

服务端拒绝命令时，连接仍保持原传输状态；成功后不能再次协商。可用 `xrtImapClientCompressed` 查询状态。

## 传输边界

- tagged `OK` 的 CRLF 仍是未压缩数据。
- CRLF 后的首字节立即按 raw-DEFLATE 解释。
- 同一 TCP/TLS 读取中预取到的压缩字节会被保留，不会丢失或误当明文。
- 每条完整 IMAP 命令使用同步刷新边界，参数分片和 APPEND 正文不会逐片强制刷新。
- 解压输入按有界小块推进，避免高压缩比输入一次产生无界的临时输出。

压缩位于 IMAP 协议层与 TCP/TLS 传输层之间。因此显式 TLS 和 STARTTLS 会话都可以在认证后继续启用 COMPRESS，线路顺序为 `IMAP -> DEFLATE -> TLS -> TCP`。

## 错误与状态

- 配置无效：参数或范围错误。
- 会话未认证、正在执行命令或已压缩：状态错误。
- 服务端未声明能力或返回非 `OK`：不支持错误。
- 压缩、网络或协议失败：客户端进入失败状态，不能继续复用。

所有输入配置均由调用方持有；协商函数在返回前已复制必要配置并创建内部状态。
