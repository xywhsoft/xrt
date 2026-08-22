# POP3 消息 API

`pop3_message` 在 `pop3_client` 的逐行多行响应之上提供三层可选能力，同时保留底层
`xrtPop3ClientRetr`、`Top` 和 `Next`。

## 流式路径

`xrtPop3ClientRetrWrite` 和 `xrtPop3ClientTopWrite` 逐行去除 dot transparency、恢复 CRLF，
随后直接调用 `xmailwriteproc`。它们不建立完整消息副本，适合写文件、哈希、增量解析或自定义
存储。`iMaxBytes` 是恢复 CRLF 后的硬上限，零值使用 `XPOP3_MESSAGE_BYTES_DEFAULT`，
`SIZE_MAX` 明确取消限制。

## 连续字节与 MIME 树

`xrtPop3ClientRetrBytes` 和 `xrtPop3ClientTopBytes` 返回由 `xrtFree` 释放的连续字节，并附加
不计入长度的零字节。`xrtPop3ClientRetrTree` 使用 `xmailtreelimits` 收集和解析完整邮件，
返回由 `xrtMailTreeFree` 释放的拥有型 MIME 树。

TOP 可能只包含 MIME 实体前缀，因此不提供强制树解析便利入口；调用方可以直接读取字节，
按自己的宽松策略处理预览内容。

## 失败状态

命令在服务器拒绝前不会进入多行状态。多行读取已经开始后，预算超限、sink 失败、取消或线路
错误都会关闭连接并保留最初错误，避免残余正文被误解析为下一条 POP3 状态响应。成功后 Client
回到 `XPOP3_CLIENT_TRANSACTION`，可以继续执行下一条命令。

## 示例

见 `examples/pop3/message/main.c`。
