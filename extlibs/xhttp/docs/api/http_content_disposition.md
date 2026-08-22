# Content-Disposition

`http_content_disposition` 实现 RFC 6266 字段值以及 RFC 8187 扩展参数。它提供
零分配严格解析、任意参数查询、文件名字节读取、调用方缓冲写出和一次分配 Build。
协议层不依赖 HTTP Client、Server、multipart 或网络运行时，两端都可以直接使用。

## 裁剪与依赖

- `XHTTP_FEATURE_HTTP_CONTENT_DISPOSITION`：解析、参数查询、文件名读取和字段写出；
  依赖 `mime`、`http_ext_value` 与 `unicode`。

解析、查询、文件名直接读取和字段直接写出均不分配。名称含 `Build` 的函数返回由
`xrtFree` 释放的零结尾缓冲。公开头文件只包含 MIME 类型定义；Unicode 是实现依赖，
不会把无关类型引入调用方源码。

## 解析与视图

```c
xcontentdisposition Disposition;
xhttpparam Param;

if ( xrtHttpContentDispositionParse(Value, &Disposition) &&
	(xrtHttpContentDispositionParam(
		&Disposition, XRT_STR_LITERAL("creation-date"), &Param
	) == XHTTP_NEXT_ITEM) ) {
	/* Param.Name 和 Param.Value 借用 Value。 */
}
```

`xrtHttpContentDispositionParse` 去除字段两端 OWS，严格验证 disposition type 和参数
列表，按 ASCII 大小写不敏感规则拒绝重复参数名称。任何名称以 `*` 结尾的扩展参数都
必须是未加引号的合法 RFC 8187 `ext-value`；该规则不只适用于 `filename*`。失败时
输出结构清零。

`Type` 和 `Parameters` 是 `xcontentdisposition` 的权威借用视图。`Name`、
`FileName`、`FileNameExt` 与 `Flags` 只是常用参数缓存。查询、文件名读取和写出都会
从权威视图重新验证并重建缓存，因此调用方可以只填写 `Type` 和 `Parameters`，也不能
通过篡改缓存绕过语法检查。输入文本必须活到最后一次使用结构为止。

## 文件名

```c
char FileName[256];
size_t Size;

if ( xrtHttpContentDispositionFileNameWrite(
	&Disposition, FileName, sizeof(FileName), &Size
) ) {
	/* FileName[0..Size) 是协议解码后的文件名字节，不自动追加零字符。 */
}
```

读取规则如下：

- 存在合法 UTF-8 `filename*` 时，优先 percent 解码该值。
- `filename*` 的 charset 不受支持或解码后不是合法 UTF-8 时，忽略它并回退普通
  `filename`，且不覆盖进入函数前已经存在的线程错误。
- 普通 `filename` 按 quoted-string 规则解码，但其字符编码没有可靠协议保证；API
  返回原始语义字节，不把它误标为 UTF-8。
- 两种参数都不可用时返回值错误，不发布部分输出或长度。

UTF-8 校验以流式状态跨越原始和 percent 字节进行，不分配完整解码副本。先以
`pOutput == NULL` 查询精确长度；容量不足时只发布所需长度，不修改缓冲。输入视图、
结构、输出和长度位置不能以会破坏借用数据的方式重叠。

线路文件名始终是不可信数据。应用在写入文件系统前必须按目标平台移除目录语义，
拒绝零字节、`.`、`..`、设备名和保留名，限制长度，并执行覆盖、权限及符号链接策略。
xrt 不在协议层固化某一种平台或存储策略。

## 写出

`xrtHttpContentDispositionWrite` 规范写出 `Type`，存在参数时追加 `; ` 和原始
`Parameters`。它会重新验证权威视图，但不会强迫调用方构建响应对象或逐参数重建
内容；已经准备好的合法参数串可以直接借用。长度查询、短缓冲和输入输出重叠遵守与
文件名 Writer 相同的原子契约。

`xrtHttpContentDispositionBuild` 与 `xrtHttpContentDispositionFileNameBuild` 只为
常用拥有型路径提供一次分配便利；性能敏感路径应优先使用 Writer。

## Multipart 区别

RFC 6266 定义 HTTP 响应字段，RFC 7578 定义 multipart/form-data part 中的
`Content-Disposition`。后者的生产端不应生成 `filename*`，因此 multipart writer
只写普通 `filename`；接收端仍复用本模块并容忍实际客户端发送的 `filename*`，让
应用可以按相同回退规则读取，而不是复制另一套参数解析器。

## 示例与测试

- `examples/http/content_disposition/main.c`
- `tests/http/test_http_content_disposition.c`
- `tests/http/test_http_content_disposition_noalloc.c`
- `tests/http/test_http_content_disposition_oom.c`
- `tests/single/test_single_http_content_disposition.c`

实现遵循 [RFC 6266](https://www.rfc-editor.org/rfc/rfc6266.html)、
[RFC 8187](https://www.rfc-editor.org/rfc/rfc8187.html) 与
[RFC 7578](https://www.rfc-editor.org/rfc/rfc7578.html)。
