# 邮件网络配置 API

`mail_net` 是 SMTP、POP3 和 IMAP 客户端共享的 XRT 网络接入层。它不公开另一套 socket
或 stream 抽象，只统一三种协议确实相同的 TCP 拨号、截止时间、取消、动态线路缓冲和
关闭语义。`mail_net_tls` 是独立可裁剪层，只在需要 TLS 时加入安全传输。

## 所有权

`xmailnetconfig` 借用调用方的 `xnetengine`、`xnetresolver`、主机名和 TLS 共享对象。
客户端不会创建或销毁这些对象，因此多个协议客户端可以共享同一高性能 Engine 和 DNS
缓存。同步客户端函数不能从该 Engine 的 Worker 回调中调用。

## 安全模式

默认配置使用 `XMAIL_SECURITY_PLAIN`，因此基础模块不依赖任何 TLS、X.509 或密码模块。
`XMAIL_SECURITY_PLAIN` 建立明文 TCP；`XMAIL_SECURITY_TLS` 完成隐式 TLS；
`XMAIL_SECURITY_STARTTLS` 先建立明文连接，再由具体协议状态机完成能力检查、升级命令和
TLS 接管。后两种模式要求启用 `mail_net_tls`；TLS 模式必须提供 `Tls.Verifier`，不会
静默关闭证书验证。空 SNI/验证名称会从拨号主机补齐；数字 IP 只补验证名称。

## 缓冲边界

默认线路上限为 64 KiB，接收按 4 KiB 动态增长，不为每个连接固定保留 8 KiB。发送按
16 KiB 分片，并同时受 TCP 写入硬上限和 TLS Future 未完成字节上限约束。协议客户端可
调整这些值，但所有增长都保持有界并接受统一 `xdeadline` 与 `xcancel`。
