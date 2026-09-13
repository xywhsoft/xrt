# IMAP 数据层

`imap_data` 在线路解析和客户端之间提供独立可裁剪的零分配数据层。所有结果借用输入，
不会创建邮箱、搜索结果或邮件对象树；调用方可以逐项消费 LIST、STATUS、SEARCH、ESEARCH、
FETCH 和 FLAGS 数据。

FETCH literal 只返回长度和标记。正文仍由 `imap_client` 流式读取，完成后通过
`xrtImapFetchCursorContinue` 接入下一行片段，因此消息大小不会转化为客户端固定缓冲或临时拼接。

## 通用值

`xrtImapDataNext` 区分 atom、number、quoted string、NIL、完整括号 list 和行尾 literal。
`Source` 保留线路表示，`Value` 去除引号或括号但不隐式分配；`xrtImapStringWrite` 负责按需解码
quoted string，也支持先查询精确输出长度。

## 专用视图

- `xrtImapListParse` 保留未知 LIST 扩展，并允许邮箱名使用 literal。
- `xrtImapStatusNext` 返回开放的名称和值对，未知 STATUS 项不会丢失。
- `xrtImapSearchNext` 增量返回 ID 和 MODSEQ；`ESEARCH ALL` 保持 sequence-set，不展开数组。
- `xrtImapFetchNext` 支持 section 名内的字段列表、嵌套值和多个 literal 续段。
- `xrtImapFlagNext` 同时适用于 LIST 属性、系统 flag 和用户关键字。
