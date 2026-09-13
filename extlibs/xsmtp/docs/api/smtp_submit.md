# SMTP 提交 API

`smtp_submit` 把高层 `xmailmessage` 与同步 SMTP 会话连接起来。它先完整验证消息和 envelope，
随后依次发送 MAIL、RCPT 和 DATA；MIME Compose 的片段直接进入增量 dot writer 和网络发送
路径，不创建整封临时报文。

## 两种入口

`xrtSmtpSubmit` 从消息的 `From`、`To`、`Cc` 和 `Bcc` 建立 envelope。Bcc 地址只用于 RCPT，
不会出现在消息字段中。

`xrtSmtpSubmitEnvelope` 接受独立的 `xsmtpenvelope`。它支持空 reverse-path、MAIL 参数以及每个
收件人独立的 RCPT 参数，适合 DSN、SMTPUTF8 和中继等扩展场景。所有视图和数组只在调用
期间借用。

## 状态与失败

调用前 Client 必须处于 `XSMTP_CLIENT_READY`。MAIL 或 RCPT 失败时，提交层在仍可复用的连接
上发送 RSET，并保留最初失败的结构化错误；DATA 已经开始后若 Compose、取消或传输失败，
连接会被关闭，避免半封消息被误提交。成功后 Client 回到 READY，可继续提交下一封消息。

提交层不负责打开连接、TLS 或认证。调用方应先使用 `smtp_client_tls` 和 `smtp_auth` 完成这些
步骤；不需要高层消息描述时，仍可直接使用 `xrtSmtpClientDataBegin`、`DataWrite` 和 `DataEnd`
保持最底层的流式发送能力。

## 示例

见 `examples/smtp/submit/main.c`。
