# SMTP 客户端 API

`smtp_client` 在 `smtp` 协议原语和 `mail_net` 共享传输之上提供同步会话状态机。它不创建
隐藏 Engine、DNS 线程或固定连接缓冲，也不负责构造 MIME 消息。调用方可以直接发送已经
生成的完整消息，也可以使用 xmail 内容模块构建消息。

## 所有权和线程

`xsmtpclientconfig` 只在 `xrtSmtpClientOpen` 期间借用。Client 持有自己的传输和最后响应
文本，但借用调用方的 `xnetengine`、`xnetresolver`、TLS Context 与 Verifier。销毁 Client
不会销毁这些共享对象。所有阻塞操作都接受同一个绝对 `xdeadline` 和可选 `xcancel`，且
不能从 Client 所属 Engine 的 Worker 回调中调用。单个 Client 不支持并发命令。

## 会话状态

Open 验证 `220` banner，然后发送 EHLO；只有服务器明确返回 `500`、`502` 或 `504` 时，
才可按配置回退 HELO。`READY -> MAIL -> RECIPIENT -> READY` 对应一笔 DATA envelope 事务；
CHUNKING 路径在首块后进入 `XSMTP_CLIENT_CHUNK`，LAST 成功后回到 READY。
协议拒绝保留服务器响应并允许调用方决定 RSET、重试或关闭；传输和解析失败进入
`XSMTP_CLIENT_FAILED`，不能继续复用。

`Quit` 发送协议命令后正常关闭，`Close` 跳过 QUIT 但仍等待传输正常关闭。`Abort` 从任意
非空 Client 状态立即提交异常中止，不等待网络完成；重复中止或对已关闭 Client 中止成功。
如果 Client 已经是 `FAILED`，Abort 保留该状态和最后响应，便于销毁前诊断。`Destroy` 是最终
所有权操作，会为尚未终止的连接补做异常中止。

`xrtSmtpClientSend`、`Receive` 和 `Command` 是扩展命令的兜底入口。它们公开完整 SMTP
线路能力，不限制调用方只能使用内置命令，但活动 DATA、尚未写完的 BDAT 块和被拒绝后
尚未 RSET 的 CHUNKING 事务会拒绝普通命令，避免命令字节被服务器当作消息正文。

## DATA 快速路径

`xrtSmtpClientDataBegin`、`xrtSmtpClientDataWrite` 和 `xrtSmtpClientDataEnd` 接受任意输入分块，
跨片段保留 CRLF 与行首状态，并直接发送 dot-transparent 片段，不创建整报文副本。
`xrtSmtpClientData` 是连续输入便利入口，发送前先完整验证 CRLF，再委托给同一增量状态机。
调用方仍负责 MIME 字段、传输编码和服务器 SIZE 限制。

## CHUNKING 快速路径

服务器声明 `XSMTP_CAP_CHUNKING` 后，`xrtSmtpClientBdatBegin`、`BdatWrite` 和 `BdatEnd`
按声明字节数直接发送原始片段，不扫描内容、不做 dot transparency、不追加 CRLF，也不创建
整块副本。`xrtSmtpClientBdat` 是连续块便利入口，零长度 LAST 合法。每块都同步读取 250；
4xx/5xx 后禁止继续发送块，调用方必须 RSET、关闭或中止。声明长度与实际长度不一致时，
End 会拒绝读取响应，调用方可继续补足字节；无法补足时必须中止连接。

`XSMTP_CAP_BINARYMIME` 只表示服务器支持二进制 MIME；调用方仍通过通用
`xrtSmtpClientMail(..., "BODY=BINARYMIME", ...)` 声明消息类型，并使用 BDAT 发送。这样
保留任意 MAIL 参数组合能力，不为 BINARYMIME 复制 envelope API。文本消息即使通过 BDAT
发送，也必须由调用方提供规范 CRLF 格式。

## TLS

基础 `smtp_client` 只支持明文 TCP。选择 `smtp_client_tls` 后，`XMAIL_SECURITY_TLS` 在 banner
之前完成隐式 TLS，`XMAIL_SECURITY_STARTTLS` 则要求 EHLO 明确声明 STARTTLS，验证 `220`
切换响应，在原 TCP Stream 上接管 TLS，并在握手后重新发送 EHLO。升级不会沿用升级前的
能力快照。TLS 配置必须提供验证器，不会静默跳过证书验证。

## 响应视图

`xrtSmtpClientLastReply` 和命令返回的 `xsmtpreply.Text` 是最终响应行文本，不包含状态码和
CRLF。视图借用 Client，只稳定到下一次 Receive 或 Client 销毁。`Lines` 给出完整多行响应
的行数，`Code` 给出统一验证后的状态码。

## 示例

见 `examples/smtp/client/main.c`。
