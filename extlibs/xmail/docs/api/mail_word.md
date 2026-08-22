# Mail 编码词

## 低层视图

`xrtMailWordParse` 从输入开头读取一个 `xmailwordview`，不分配内存，也不解码正文。
`Source`、`Charset` 和 `Encoded` 都借用输入，`Encoding` 用 `xmailwordencoding` 区分
`XMAIL_WORD_BASE64` 与 `XMAIL_WORD_Q`。调用方可以利用这层视图接入 xmail 尚未内置
的字符集转换器，不必重新实现 RFC 2047 边界解析。

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

`XMAIL_WORD_STRICT` 只接受 `UTF-8`、`UTF8`、`US-ASCII` 和 `ASCII`，并拒绝非规范
Base64、错误 Q 转义、无效 UTF-8 和可形成字段注入的控制字节。设置
`XMAIL_WORD_RELAXED` 后，无法可靠解释的完整编码词保留原文，不会猜测未知字符集。
`xrtMailWordDecode` 返回由 `xrtFree` 释放的 UTF-8 文本。

所有写入入口先完整验证和计量。容量不足、格式错误、字符集错误或不允许的重叠都不
修改目标缓冲区，并通过 `XMAIL_ERROR_ENCODING`、`XMAIL_ERROR_CHARSET`、
`XMAIL_ERROR_HEADER` 或 `XMAIL_ERROR_LINE` 提供结构化错误。
