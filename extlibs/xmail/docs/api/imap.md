# IMAP 协议 API

`imap` 公开不依赖网络的响应、literal、capability 和命令原语。它只解析协议线路和
借用视图，不替用户构造邮箱、搜索条件或 FETCH 数据模型，因此未知扩展和复杂响应仍可
由调用方按原始文本处理。

## 响应与 literal

`xrtImapResponseParse` 区分 tagged、untagged 和 continuation 响应，并识别稳定的
`OK`、`NO`、`BAD`、`PREAUTH`、`BYE` 状态。所有视图借用输入，不产生分配。

`xrtImapLiteralParse` 识别行尾同步 literal、`LITERAL+` 非同步 literal 和 binary
literal，只返回长度与标记视图；literal 数据仍由网络状态机按明确长度读取，避免复制和
隐式缓存。

## Capability 与命令

`xrtImapAtomCursorInit` 和 `xrtImapAtomNext` 遍历空白分隔的简单 atom 列表；
`xrtImapCapability` 只为常用能力返回稳定标记，未知扩展保持可见且不占内建位。

`xrtImapQuoteWrite` 写出转义 quoted string，控制数据必须改走 literal。通用
`xrtImapCommandWrite` 写出 `Tag Command [Arguments]\r\n`，拒绝 atom 错误、控制字符、
线路注入和超过调用方上限的命令。输出容量包含末尾零字节。

## 示例

见 `examples/imap/protocol/main.c`。
