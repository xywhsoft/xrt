# HTTP Extended Value

`XHTTP_MODULE_HTTP_EXT_VALUE` 实现 RFC 8187 `charset'language'value` 语法。
解析结果借用输入，读取函数只执行严格百分号解码，不隐式转换字符集。

```c
xhttpextvalue Value;

if ( xrtHttpExtValueParse(
	XRT_STR_LITERAL("UTF-8'zh-CN'file%20name.txt"),
	&Value
) ) {
	/* 调用 xrtHttpExtValueRead 解码 Value。 */
}
```
