# 邮件消息 API

`mail_message` 在字段、编码和 multipart 原语之上提供轻量 RFC 消息视图。它不建立
对象树、不复制原始报文，也不要求调用方通过 builder 构造邮件。

## 解析

`xrtMailMessageParse` 要求字段区使用严格 CRLF，并且必须存在空行分隔符。零预算使用
`XMAIL_MESSAGE_HEADER_BYTES_DEFAULT` 和 `XMAIL_MESSAGE_HEADERS_DEFAULT`；传入
`SIZE_MAX` 可以显式取消对应限制。成功后 `xmailmessageview` 的全部视图都借用输入，
输入必须比视图存活更久。

`xrtMailMessageHeader` 按 ASCII 大小写不敏感名称和出现序号查找字段。它保留重复字段，
不会把 `Received` 等合法重复字段合并。

## 正文

`xrtMailMessageTransfer` 读取唯一的 `Content-Transfer-Encoding`；字段缺失时返回
`XMAIL_TRANSFER_7BIT`，重复或未知值返回错误。`xrtMailMessageBodyWrite` 支持调用方
缓冲和长度查询，`xrtMailMessageBody` 是由 `xrtFree` 释放的便捷入口。

`7bit`、`8bit` 和 `binary` 保持原字节；Quoted-Printable 与 Base64 复用 `mail_codec`
的唯一实现。此层不自动解释字符集、Content-Type 或 multipart，调用方可以继续组合
`mail_word`、`mail_param` 和 `mail_multipart`，高级对象树不会堵住原始报文路径。

## 示例

见 `examples/mail/message/main.c`。
