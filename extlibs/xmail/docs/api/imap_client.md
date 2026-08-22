# IMAP 客户端 API

`imap_client` 在无网络依赖的 IMAP 协议原语和 `mail_net` 上提供同步、流式会话状态机。
它不建立邮箱对象或聚合 FETCH 结果，调用方既可以使用顺序命令，也可以通过显式 tag
进行流水线发送并自行关联响应。

## 打开和 TLS

`xrtImapClientOpen` 建立 TCP 连接、读取 greeting、获取 CAPABILITY，并在配置要求时完成
隐式 TLS 或 STARTTLS。STARTTLS 的 tagged `OK` 被完整消费后才开始握手，握手成功后重新
获取 CAPABILITY；升级前的能力快照不会进入安全会话。TLS 连接必须提供 Verifier。

Client 借用调用方 Engine、Resolver、TLS Context 和 Verifier；销毁 Client 不销毁这些共享
对象。同步函数不能从 Client 所属 Engine 的 Worker 回调中调用。除显式流水线发送与接收
模型外，单个 Client 不支持并发调用。

## 两级命令模型

`xrtImapClientSend`、`SendParts`、`Write`、`Continue`、`Receive` 和 `ReadLiteral` 是最低层
线路入口。显式 tag 允许同时发出多个命令，`Receive` 按服务器到达顺序返回事件，调用方按
tag 关联 completion。`SendParts` 在各参数片之间插入空格并直接写入线路，不构造整条临时
命令。

`xrtImapClientBegin`、`BeginParts` 和 `Next` 是顺序便利层。Begin 自动生成唯一 tag；Next
把未请求响应和目标命令的 tagged completion 区分为 `XMAIL_NEXT_ITEM` 与
`XMAIL_NEXT_END`。活动顺序命令结束前不能开始另一条顺序命令。

## Literal 与视图

Receive 返回的响应、fragment 和 literal 描述均借用 Client 的线路缓冲；下一次 Receive、
Next 或 ReadLiteral 后失效。literal 正文必须使用调用方缓冲分段读取，未读完前不能继续
读取后续响应，因此消息大小不会变成固定内存上限。

CAPABILITY、APPENDLIMIT、状态和最近 completion 都由 Client 保存。未知 capability 与未知
响应仍可通过原始视图处理，不需要等待 xmail 增加专用 API。

## 关闭和错误

`xrtImapClientLogout` 完整消费 BYE 与 tagged completion 后正常关闭；`Close` 跳过 LOGOUT，
但仍等待传输正常关闭；`Abort` 无网络等待地提交异常中止，并且可以重复调用。原有 FAILED
状态和最近 completion 会保留到 `Destroy`，销毁未终止会话时也会补做异常中止。网络、取消、
超时、协议错序或解析失败会使线路进入 `XIMAP_CLIENT_FAILED`，因为此时无法保证下一字节仍
位于命令边界。服务器对完整命令返回 NO 或 BAD 时，客户端保留可恢复状态并返回结构化错误。

## 示例

见 `examples/imap/client/main.c`。
