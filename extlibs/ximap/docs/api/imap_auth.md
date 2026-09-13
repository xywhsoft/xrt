# IMAP 认证 API

`imap_auth` 是独立可裁剪层，在 `imap_client` 上提供 LOGIN、PLAIN、XOAUTH2 和
OAUTHBEARER。LOGIN 使用传统命令，其余机制使用 SASL continuation，并在服务器声明
SASL-IR 时支持 initial response。

## 安全默认值

`xrtImapAuthConfigInit` 默认选择 PLAIN、开启 initial response，并禁止在明文连接上传送
凭据。只有调用方明确设置 `AllowPlaintext` 才能越过安全门；生产网络应选择
`imap_client_tls`，使用隐式 TLS 或 STARTTLS。

Username、Secret 和可选 AuthorizationId 只在 `xrtImapClientAuth` 调用期间借用。PLAIN
原文、OAuth bearer 原文、Base64 文本和命令副本在使用后立即清零释放。配置校验拒绝机制
不接受的协议分隔字符，避免把调用方数据变成额外的 SASL 字段。

## 状态与扩展

认证只允许在 `XIMAP_CLIENT_NOT_AUTHENTICATED` 状态执行，并在发送前检查当前 CAPABILITY
是否声明所选机制。成功后状态进入 AUTHENTICATED；服务器拒绝认证时仍可选择另一机制，
线路、取消、超时或解析失败则遵循 Client 的失败终态。

未内建的 SASL 机制仍可用 Client 的 Send、Continue、Receive 和 ReadLiteral 原语实现，
无需修改网络层或 IMAP 解析层。
