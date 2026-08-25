# Mail 字符集

`mail_charset` 是可独立裁剪的小型邮件字符集层。它内置 UTF-8、US-ASCII、
ISO-8859-1/Latin-1 和 Windows-1252 的常见别名，不携带大型字符集映射表。

`xrtMailCharsetSupported` 无分配查询字符集是否内置。`xrtMailCharsetToUtf8Write` 先完整
校验和计量，再把明确长度字节转换成 UTF-8；查询模式使用空输出，实际容量包含末尾零字节，
失败不会发布部分文本。`xrtMailCharsetToUtf8` 返回由 `xrtFree` 释放的便捷结果。

```c
static const unsigned char text[] = { 'c', 'a', 'f', 0xE9u };
str utf8 = xrtMailCharsetToUtf8(
	XRT_STR_LITERAL("latin1"),
	(xbytesview) { text, sizeof(text) },
	NULL
);
```

GB18030、Big5、Shift_JIS 等大型字符集应由应用选择的平台转换器或扩展库处理。RFC 2047
和 RFC 2231 的低层 API 都公开原始字符集名称与正文边界，因此扩展转换时不需要重新解析
邮件语法。
