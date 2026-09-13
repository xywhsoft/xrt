# xsmtp

xsmtp 是构建在 xmail 邮件基座（MIME 内容层与传输层）之上的 SMTP 客户端扩展库：协议解析、同步客户端、STARTTLS/隐式 TLS、SASL 认证与从 xmailmessage 派生的流式提交。通过 `XSMTP_MODULE_*` 宏裁剪，单头形态为 `single/xsmtp.h`。
