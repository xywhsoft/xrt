# SMTP 认证 API

`smtp_auth` 是独立可裁剪层，只为 `smtp_client` 增加 Base64 和 PLAIN、LOGIN、XOAUTH2
认证流程，不强制依赖 TLS。实际应用通常同时选择 `smtp_client_tls`，但裁剪关系保持正交。

## 安全默认值

`xrtSmtpAuthConfigInit` 默认使用 PLAIN 和 SASL initial response，并禁止在明文连接发送凭据。
只有显式设置 `AllowPlaintext` 才能越过这一安全门。认证前还会检查 EHLO 是否声明对应机制，
避免向不支持的服务器泄露凭据。

Username、Secret 和可选 AuthorizationId 都只在 `xrtSmtpClientAuth` 调用期间借用，不保存到
Client。PLAIN 原文、XOAUTH2 bearer 原文、Base64 文本和发送行副本在使用后立即清零释放。
AuthorizationId 只适用于 PLAIN；XOAUTH2 字段拒绝其协议分隔字节。

## 机制

- `XSMTP_AUTH_PLAIN` 支持 SASL-IR，也兼容服务器返回空 challenge 后再次提交响应。
- `XSMTP_AUTH_LOGIN` 完成 username 与 password 两阶段 challenge。
- `XSMTP_AUTH_XOAUTH2` 使用 bearer 格式；服务器以 `334` 返回错误详情时，客户端发送空响应
  正常结束 SASL 交换，再读取最终拒绝状态。

服务器拒绝认证时 Client 仍保持 READY，调用方可以检查 `xrtSmtpClientLastReply` 后选择其他
机制；网络、取消、超时或响应解析失败仍遵循 SMTP Client 的不可恢复传输状态。
