# POP3 认证

`pop3_auth` 在 `pop3_client` 上提供传统 USER/PASS，以及 RFC 5034 SASL PLAIN、
XOAUTH2 和 OAUTHBEARER。认证配置只借用凭据；Base64 和明文临时缓冲会在调用结束前清零。

## 配置

`xrtPop3AuthConfigInit` 默认选择 PLAIN、启用初始响应并禁止明文传输。`Username` 与
`Secret` 必填；PLAIN 和 OAUTHBEARER 可设置 `AuthorizationId`。bearer 机制始终要求
TLS，口令机制只有在 TLS 下或显式设置 `AllowPlaintext` 时才允许发送。

`xrtPop3ClientLogin` 是 USER/PASS 的直接便利入口；需要 SASL 时使用
`xrtPop3ClientAuth`。

## 能力与交换

客户端读取 CAPA 时会保存 SASL 参数中的已知机制。`xrtPop3ClientSaslMechanisms`
返回 `XPOP3_SASL_PLAIN`、`XPOP3_SASL_XOAUTH2` 和 `XPOP3_SASL_OAUTHBEARER` 位集。
SASL 高层入口只尝试服务器明确公布的机制；未知机制仍可通过
`xrtPop3ClientAuthLine` 与 `xrtPop3ClientLine` 实现。

初始响应严格遵守 255 字节 AUTH 命令预算；超过预算时自动改为 challenge 往返。
普通 POP3 命令与 SASL 响应使用独立长度上限，认证扩展不会放宽其他命令的输入边界。
