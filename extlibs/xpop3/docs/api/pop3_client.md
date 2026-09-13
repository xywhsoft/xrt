# POP3 客户端 API

`pop3_client` 在公共 POP3 解析原语和 `mail_net` 传输上提供同步状态机。它保留
AUTHORIZATION、TRANSACTION、MULTILINE、UPDATE 和失败终态，不创建隐藏网络对象，也不把
邮件内容聚合为固定上限字符串。

## 打开和 TLS

Open 验证 greeting，并默认使用 CAPA 建立能力快照。基础模块使用 110 明文端口；选择
`pop3_client_tls` 后可以使用隐式 TLS，或在 CAPA 明确声明 STLS 后原位升级连接。STLS 的
`+OK` 响应被完整消费后才开始握手，握手完成后重新执行 CAPA，升级前能力不会泄漏到安全
会话。TLS 必须提供 Verifier。

Client 借用调用方 Engine、Resolver、TLS Context 和 Verifier；销毁 Client 不销毁这些共享
对象。同步函数不能从 Client 所属 Engine 的 Worker 回调中调用，单个 Client 不支持并发命令。

`Quit` 提交 UPDATE 后正常关闭，`Close` 跳过 QUIT 但仍等待传输正常关闭。`Abort` 无网络等待，
立即禁止继续使用该会话；重复调用成功，原有 `FAILED` 状态和最后响应保留到 `Destroy`。销毁
尚未终止的 Client 时会自动异常中止。

## 分层入口

`xrtPop3ClientSend` 和 `xrtPop3ClientLine` 是最低层线路入口，可实现 SASL continuation 和
未知扩展。`Receive` 在其上解析 `+OK/-ERR`，`Command` 再组合命令构建与状态响应。
`Begin/Next` 处理任意标准多行命令，Next 逐行去除 dot transparency，遇到单点终止行后自动
恢复命令前状态。

STAT、单项 LIST/UIDL、全量 LIST/UIDL、RETR、TOP、DELE、RSET、NOOP 和 QUIT 都建立在这些
公开底层入口上。RETR/TOP 不分配整封邮件；每条返回行借用内部接收缓冲，只稳定到下一次
线路读取。调用方可以直接流向文件、MIME 增量解析器或自己的消息存储。

可选 `pop3_message` 在同一状态机上增加有界 `RetrWrite/TopWrite`、owned 字节和 MIME 树入口；
只需要底层逐行路径时不会携带 Buffer 或 MIME 树闭包。

## 认证

`pop3_auth` 提供常用 USER/PASS。用户名、密码和临时命令副本只在调用期间存在，发送后立即
清零释放。默认拒绝在明文传输发送凭据；`AllowPlaintext` 只用于调用方明确控制的兼容环境。
服务器拒绝凭据后仍停留在 AUTHORIZATION，线路错误则进入 FAILED。

其他 SASL 机制可以用 Send/Line 自定义；它们后续也可以作为独立裁剪模块增加，不需要改变
POP3 Client 或网络底座。

## 示例

见 `examples/pop3/client/main.c`。
