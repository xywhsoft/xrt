# SMTP 协议 API

`xrtSmtpPathValid` 验证不含尖括号、空白或控制分隔符的 reverse-path/forward-path 内容；
是否允许空路径由调用方明确指定。它只验证尖括号内部的路径，不构造 MAIL 或 RCPT 命令。

`smtp` 先公开不依赖网络的响应、能力与命令原语。官方 SMTP 客户端和自定义状态机使用
同一实现；选择协议底层 API 不会被迫创建客户端对象。

## 响应

`xrtSmtpReplyLineParse` 解析三位状态码和多行分隔符。`xsmtpreplyparser` 验证同一响应
中的状态码保持一致、行数不超限且只有一个终止行。输入来自增量网络缓冲时，先使用
`xrtMailLineRead` 取得不含 CRLF 的行，再交给 `xrtSmtpReplyRead`。

## EHLO

`xrtSmtpCapabilityParse` 返回借用的名称与参数。`xrtSmtpCapabilityAdd` 合并常用扩展、
AUTH PLAIN/LOGIN/XOAUTH2 和 64 位 SIZE 上限；未知扩展保持可解析但不占用内置位，
调用方仍可直接检查 `xsmtpcapabilityview`。

## 命令

`xrtSmtpCommandWrite` 输出 `Verb [Arguments]\r\n`，拒绝控制字符、CR/LF 注入以及超过
512 字节的命令。它是扩展命令的兜底入口，不限制用户只能调用固定的高层命令集合。
输出容量包含末尾零字节；分配式 `xrtSmtpCommand` 由 `xrtFree` 释放。

## 示例

见 `examples/smtp/protocol/main.c`。
