# 邮件线路 API

`mail_wire` 是 SMTP、POP3 和 IMAP 可共同使用的最小线路原语，不创建网络连接，也不
隐藏 XRT TCP/TLS/Future。协议客户端可以把这些函数直接组合进自己的状态机。

## 增量行

`xrtMailLineRead` 从当前输入前缀读取一条严格 CRLF 行。返回 `XMAIL_NEXT_ITEM` 时，
`pLine` 借用不含 CRLF 的内容，`pConsumed` 表示可从接收缓冲消费的字节；返回
`XMAIL_NEXT_END` 时需要继续接收，不能消费现有前缀。零限制使用 64 KiB，`SIZE_MAX`
明确取消限制。

## Dot Transparency

`xrtMailDotLine` 是零分配接收路径：单点行表示多行响应结束，其他点开头的线路去除一
个转义点。`xmaildotwriter`、`xrtMailDotWriterInit`、`xrtMailDotWriterWrite` 和
`xrtMailDotWriterFinish` 组成发送侧增量状态机；它保留跨片段的 CRLF 与行首状态，可以
直接连接 Compose sink 和网络写入，不建立完整 dot-stuffed 副本。

`xrtMailDotWrite` 和 `xrtMailDotDecodeWrite` 提供连续输入的长度查询与调用方缓冲，分配式
入口返回由 `xrtFree` 释放的字节。

启用 `Terminate` 时，编码器会在需要时补一个 CRLF，再追加 `.\r\n`。这适合 SMTP
DATA；不启用时只做逐行点转义。解码器可以要求终止行，适合 POP3 多行响应。所有已
完成线路必须使用 CRLF，裸 CR/LF 会作为协议错误返回。

## 示例

见 `examples/mail/wire/main.c`。
