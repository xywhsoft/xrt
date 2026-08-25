# Mail 编码词

## 低层视图

`xrtMailWordParse` 从输入开头读取一个 `xmailwordview`，不分配内存，也不解码正文。
`Source`、`Charset`、`Language` 和 `Encoded` 都借用输入，`Encoding` 用
`xmailwordencoding` 区分 `XMAIL_WORD_BASE64` 与 `XMAIL_WORD_Q`。RFC 2231 形式的
`charset*language` 会被拆成独立的 `Charset` 与 `Language`；没有语言子标签时
`Language.Size` 为零。调用方可以利用这层视图接入 xmail 尚未内置的字符集转换器，
不必重新实现 RFC 2047 边界解析。

解析器把普通文本报告为 `XMAIL_NEXT_END`，把有效编码词报告为 `XMAIL_NEXT_ITEM`，
并严格拒绝空字符集、非法编码标记、空正文、控制字节和超过 75 字节的编码词。

## 编码

`xrtMailWordEncodeWrite` 接受明确长度的 UTF-8 字段文本。安全纯 ASCII 会原样写出；
非 ASCII 或可能被误识别为编码词的 `=?` 会编码为一个或多个 `UTF-8` 编码词。长文本
只在 UTF-8 标量边界分片，每个词都不超过 75 字节，相邻词以一个空格分隔。

Base64 是长度与性能更稳定的默认选择。Q 编码只原样保留在 phrase 中也安全的 ASCII，
空格写成下划线，其余字节写成十六进制转义。`xrtMailWordEncode` 是由 `xrtFree` 释放的
便捷入口。

## 解码

`xrtMailWordDecodeWrite` 解码普通 UTF-8 与编码词混合的字段值，同时支持 B 和 Q。
相邻且可解码的编码词之间的空格、制表符或合法折叠会被忽略；其他折叠会规范为一个
空格。输出可与输入从同一地址开始。

`XMAIL_WORD_STRICT` 内置接受 UTF-8、US-ASCII、ISO-8859-1/Latin-1 和
Windows-1252 的常用别名，并拒绝非规范 Base64、错误 Q 转义、无效文本和可形成
字段注入的控制字节。每个临时缓冲都由 RFC 2047 的 75 字节硬上限推导，不依赖
字符集名称恰好足够长。语言子标签只做语法边界校验，不影响字符集转换和解码结果。设置
`XMAIL_WORD_RELAXED` 后，无法可靠解释的完整编码词保留原文，不会猜测未知字符集。
`xrtMailWordDecode` 返回由 `xrtFree` 释放的 UTF-8 文本。

GB18030、Big5、Shift_JIS 等需要大型映射表的字符集不进入默认构建。需要这些字符集时，
先用 `xrtMailWordParse` 取得零分配边界与原始正文，再接入应用选择的字符集转换器；
无需大型映射表的旧字符集转换由可独立使用的 `mail_charset` 层提供。调用方不需要重新
实现编码词解析。

所有写入入口先完整验证和计量。容量不足、格式错误、字符集错误或不允许的重叠都不
修改目标缓冲区，并通过 `XMAIL_ERROR_ENCODING`、`XMAIL_ERROR_CHARSET`、
`XMAIL_ERROR_HEADER` 或 `XMAIL_ERROR_LINE` 提供结构化错误。
