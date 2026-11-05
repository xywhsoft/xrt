# IMAP 常用命令 API

`imap_command` 是建立在 `imap_client` 顺序命令模型上的可裁剪便利层。它覆盖邮箱管理、
选择、查询、消息操作和 IDLE，但不把响应强制转换为重量级对象；返回数据的命令继续通过
`xrtImapClientNext` 与 `xrtImapClientReadLiteral` 流式消费。

## 邮箱状态

`xrtImapClientSelect` 和 `xrtImapClientExamine` 返回零分配 `ximapmailboxinfo` 摘要。
`Present` 位区分服务器未返回字段与字段值为零；`ReadOnly` 保留最终 completion 给出的访问
模式。`xrtImapMailboxInfoUpdate` 也可以把后续未请求 EXISTS、RECENT 和响应码合并到同一
摘要。

已知 UIDNEXT、UIDVALIDITY 和 UNSEEN 响应码接受 1..UINT32_MAX；UNSEEN 响应码是首个
未读消息的序号，与 STATUS UNSEEN 的可为零计数不同。EXISTS、RECENT 接受
0..UINT32_MAX，EXPUNGE 序号接受 1..UINT32_MAX；这三种响应不接受额外数据。
`xrtImapMailboxInfoUpdate` 在拒绝一条已知更新时保持整个摘要及 Present 位不变；
零序号或额外数据返回 PROTOCOL，超出已知数值域返回 RANGE，错误域/code 保持邮件协议
分类。未知扩展继续交给调用方处理，泛型数字和既有 MODSEQ 仍支持完整 uint64，
不把它们统一收紧为上述字段的范围。

SELECT/EXAMINE 在成功开始命令后才初始化输出摘要。本地非法邮箱、线路超限或引号
分配失败等开始命令失败不会清空调用方输出，并可继续复用会话。命令开始后的远程错误
可以留下此前合法响应形成的部分摘要；它不是成功选择结果，调用方必须先判断返回值
和 Client 状态。已知非法数值使正在消费的选择命令返回协议错误并进入 FAILED，
包括未请求返回摘要的调用；原始完整响应仍可通过 LastResponse 查询。

数值域依据 [RFC 3501 第 9 节](https://www.rfc-editor.org/rfc/rfc3501.html#section-9)
与 [RFC 9051 第 9 节](https://www.rfc-editor.org/rfc/rfc9051.html#section-9) 的已知字段。

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

未请求 FETCH 事件可能携带 literal。必须先读完 literal 正文及其后续 fragment，
再调用 EndIdle；提前调用返回 `XERR_STATE`，不记录 DONE，读完后可重试同一 IDLE。

小于 `-1` 的非法超时在 Begin/End 的命令或状态改变前拒绝，可以更正参数后继续同一
IDLE。有效超时、取消或传输失败不会伪造命令边界，线路按 Client 的不可恢复规则处理。
