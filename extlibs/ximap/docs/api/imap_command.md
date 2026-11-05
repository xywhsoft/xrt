# IMAP 常用命令 API

`imap_command` 是建立在 `imap_client` 顺序命令模型上的可裁剪便利层。它覆盖邮箱管理、
选择、查询、消息操作和 IDLE，但不把响应强制转换为重量级对象；返回数据的命令继续通过
`xrtImapClientNext` 与 `xrtImapClientReadLiteral` 流式消费。

## 邮箱状态

`xrtImapClientSelect` 和 `xrtImapClientExamine` 返回零分配 `ximapmailboxinfo` 摘要。
`Present` 位区分服务器未返回字段与字段值为零；`ReadOnly` 保留最终 completion 给出的访问
模式。`xrtImapMailboxInfoUpdate` 也可以把后续未请求 EXISTS、RECENT 和响应码合并到同一
摘要。

CREATE、DELETE、RENAME、SUBSCRIBE、UNSUBSCRIBE、CHECK、UNSELECT 与 CLOSE 是完整消费
响应的同步命令。输入邮箱名会按 IMAP string 规则校验和引用，不能通过 CRLF 注入额外命令。

## 流式结果

LIST、STATUS、SEARCH、FETCH、STORE、COPY、MOVE 与 EXPUNGE 只负责安全构造并开始命令。
调用方循环调用 Next，按 `ximapresponseview` 或 `imap_data` 原语读取返回项；遇到 literal 时
分段读取正文。UID 形式由显式参数选择，不复制消息集合、搜索条件或 FETCH 项列表。

这种接口保留未知扩展、复杂 FETCH 数据和超大消息的处理能力。需要领域对象的上层框架可
在流式入口上构建缓存或映射，而不会改变底层传输成本。

## IDLE

`xrtImapClientBeginIdle` 开始 IDLE 命令，调用方通过 Next 接收服务器 continuation，
之后继续用 Next 接收未请求事件。`xrtImapClientEndIdle` 只发送 DONE，调用方仍须用 Next
消费匹配 tag 的 completion，直到返回 `XMAIL_NEXT_END`。

小于 `-1` 的非法超时在 Begin/End 的命令或状态改变前拒绝，可以更正参数后继续同一
IDLE。有效超时、取消或传输失败不会伪造命令边界，线路按 Client 的不可恢复规则处理。
