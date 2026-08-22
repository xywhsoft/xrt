# MIME

`XHTTP_MODULE_MIME` 提供媒体类型的严格解析、参数读取、规范写出和可压缩性判断；
`XHTTP_MODULE_MIME_TYPES` 独立提供按扩展名或路径查询静态媒体类型表。

```c
xmediatype Type;

if ( xrtHttpMediaTypeParse(
	XRT_STR_LITERAL("application/json; charset=utf-8"),
	&Type
) ) {
	/* Type、Subtype 和 Parameters 都借用原输入。 */
}
```

解析路径不分配内存。需要保存结果时由调用方复制原文本，避免协议底层强制拥有状态。
