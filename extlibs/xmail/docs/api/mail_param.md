# MIME 参数

`mail_param` 是独立于 HTTP 的 MIME 字段值语法层，依赖 `mail_core` 和 XRT Unicode
校验。它覆盖 Content-Type、Content-Disposition、quoted-string、RFC 2231 扩展值
和连续段。

`xmailmediatypeview` 由 `Source`、`Type`、`Subtype`、`Parameters` 组成，
`xrtMailMediaTypeParse` 会验证完整 `type/subtype` 和参数块。
`xmaildispositionview` 保存 `Source`、`Type`、`Parameters`，由
`xrtMailDispositionParse` 完整解析。

底层路径使用 `xmailparamcursor`、`xmailparamview`、`xrtMailParamCursorInit` 和
`xrtMailParamNext`。视图同时提供 `RawName`、基础 `Name`、`RawValue`、`Value`、
`Section`、`Extended`、`Continued`、`Quoted`。没有 section 时 `Section` 为
`XMAIL_PARAM_SECTION_NONE`；组合 helper 最多接受 `XMAIL_PARAM_SECTIONS_MAX` 个连续段。

`xrtMailParamDecodeWrite` 解码一个 section，并可返回借用的字符集和语言视图。
`xrtMailParamFindWrite` 按大小写不敏感名称查找，优先连续扩展表示，再优先
`name*=`，合并全部 section 后返回 `XMAIL_NEXT_ITEM`；缺失返回 `XMAIL_NEXT_END`，
语法或容量错误返回 `XMAIL_NEXT_ERROR`。`xmailparaminfo` 保存 `Charset`、`Language`、
`Sections`、`Extended` 和 `Continued`。`xrtMailParamFind` 是由 `xrtFree` 释放的
一行式入口。

构建端使用 `xrtMailParamWrite` 和 `xrtMailParam`。输出总是包含可直接附加到
Content-Type 或 Content-Disposition 后面的 `; ` 前缀。`XMAIL_PARAM_ENCODING_AUTO`
优先 token，其次 quoted-string，非 ASCII 值使用带 UTF-8 校验的 RFC 2231 扩展参数；
也可通过 `TOKEN`、`QUOTED` 或 `UTF8` 强制指定。编码值超过
`XMAIL_PARAM_SECTION_SIZE` 时，写入端自动生成 `name*0`、`name*1` 形式的连续段；最多生成
`XMAIL_PARAM_SECTIONS_MAX` 段。连续段不会拆开 quoted-pair、百分号转义单元或 UTF-8 字符，并可由
`xrtMailParamFindWrite` 无损合并。写入接口支持长度查询，容量不足、输入非法或超过段数上限时
不会产生部分参数。

```c
xmaildispositionview disposition;
str filename;

xrtMailDispositionParse(value, &disposition);
filename = xrtMailParamFind(
	disposition.Parameters,
	XRT_STR_LITERAL("filename"),
	NULL,
	NULL
);
```
