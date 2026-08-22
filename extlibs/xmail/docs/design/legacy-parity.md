# 旧版邮件扩展迁移对照

## 结论

旧版 `xmail_mime`、`xsmtp`、`xpop3` 和 `ximap` 已由现代 `xmail` 体系取代。现代实现保留或
增强协议能力，但不兼容旧 API，也不恢复“每个异步操作创建一个线程”的包装模型。调用方
需要异步执行完整业务流程时，应把同步会话提交到显式 XRT task pool；协议客户端本身继续
使用统一 deadline、cancel 和底层异步网络状态机。

## MIME 内容

旧版的 CRLF、Header、RFC 2047、Quoted-Printable、Base64、地址、日期、Message-ID、
multipart、附件、inline 资源、`message/rfc822` 和递归 part tree，分别由 `mail_core`、
`mail_codec`、`mail_header`、`mail_word`、`mail_address`、`mail_date`、`mail_id`、
`mail_param`、`mail_multipart`、`mail_message`、`mail_tree`、`mail_build` 和 `mail_compose`
覆盖。低层解析使用借用视图和游标，高层拥有型树是独立可裁剪层。

## SMTP

旧版的明文、隐式 TLS、STARTTLS、EHLO、SIZE、8BITMIME、SMTPUTF8、DSN、AUTH PLAIN、
LOGIN、XOAUTH2、envelope、Bcc 隐藏、MIME 提交与失败恢复，映射到 `smtp`、`smtp_client`、
`smtp_auth` 和 `smtp_submit`。现代客户端额外支持 OAUTHBEARER、流式 DATA、CHUNKING/BDAT
和 BINARYMIME 原始字节路径。DSN 的 RET、ENVID、NOTIFY、ORCPT 继续通过通用 MAIL/RCPT
参数表达，不复制专用 envelope API。

## POP3

旧版的 USER/PASS、AUTH PLAIN、CAPA、STAT、LIST、UIDL、RETR、TOP、DELE、NOOP、RSET、
QUIT、dot transparency、POP3S、STLS 和 MIME 解析，映射到 `pop3`、`pop3_client`、
`pop3_auth` 与 `pop3_message`。现代认证额外支持 XOAUTH2/OAUTHBEARER、能力机制快照、
独立 SASL 行长和凭据传输策略。

## IMAP

旧版的 IMAPS、STARTTLS、CAPABILITY、LOGIN/XOAUTH2、SELECT/EXAMINE、LIST、STATUS、
SEARCH、FETCH、STORE、EXPUNGE、BODY section、BODYSTRUCTURE、IDLE 与 MIME 解析，映射到
`imap`、`imap_data`、`imap_client`、`imap_auth`、`imap_command`、`imap_message` 和
`imap_body`。现代实现额外提供显式 tag 流水线、APPEND、LITERAL+/LITERAL-/binary literal、
SASL-IR、OAUTHBEARER、UIDPLUS 结果、ESEARCH、MOVE、COMPRESS=DEFLATE，以及未知扩展的
低层 Begin/Next/ReadLiteral 组合路径。

## 不迁移项

- 旧 API 名称和多版本兼容层。
- 每项操作独占线程的 Future/AsyncWait 复制实现。
- 隐藏 Engine、DNS worker、TLS 或 socket 生命周期。
- 为 DSN、BINARYMIME 等可由通用参数完整表达的扩展重复建立对象体系。
- IDLE 自动重连、业务心跳和永久后台循环；这些属于应用策略，可在公开 IDLE 原语和 task
  pool 上实现。

