# IMAP 消息 API

`imap_message` 是公开 `FETCH`、事件与 literal 流之上的可选便利层，不替代低层 IMAP
状态机。需要多个属性、多个消息、部分抓取、BINARY 或扩展数据项时，继续使用
`xrtImapClientBeginFetch`、`xrtImapClientNext` 和 `xrtImapClientReadLiteral`。

## 流式路径

`xrtImapClientBodyWrite` 请求单个序号或 UID 对应的 `BODY[section]`。空 `Section` 表示完整
RFC 消息；`bPeek` 选择 `BODY.PEEK`，避免隐式设置 `\\Seen`。section 允许标准 ASCII
section-spec，但拒绝控制字符和嵌套方括号，不能注入额外命令。

literal 直接以最多 16 KiB 的借用片段交给 `xmailwriteproc`，不创建完整消息副本。
`iMaxBytes` 是服务器声明 literal 长度的硬上限；零值使用
`XIMAP_MESSAGE_BYTES_DEFAULT`，`SIZE_MAX` 明确取消限制。

## 连续字节与 MIME 树

`xrtImapClientBodyBytes` 返回由 `xrtFree` 释放的连续字节，并附加不计入长度的零字节。
`xrtImapClientMessageTree` 固定读取完整 `BODY[]`，随后按 `xmailtreelimits` 解析并返回由
`xrtMailTreeFree` 释放的拥有型 MIME 树。

## 状态和失败

服务器正常完成但没有返回目标消息时，函数返回 `XERR_NOT_FOUND`，Client 保持可复用。
`NO` 映射为 `XERR_PERMISSION`，`BAD` 和非法 FETCH 结构映射为 `XERR_PROTOCOL`。

literal 已开始后发生预算超限、sink 失败、取消、线路错误、属性不匹配或重复 literal 时，
函数关闭当前 Client 并保留首个错误，避免残余字节成为下一条响应。成功后 Client 仍处于
选中态，可以继续执行命令。

## 示例

见 `examples/imap/message/main.c`。
