# Multipart 协议层

`multipart.h` 提供与网络、HTTP 客户端和服务器对象无关的 MIME multipart 整包解析底座。实现遵守 [RFC 2046 第 5.1 节](https://www.rfc-editor.org/rfc/rfc2046.html#section-5.1) 的公共 multipart 线缆语法，并为 [RFC 7578](https://www.rfc-editor.org/rfc/rfc7578.html) 的 `multipart/form-data` 提供常用校验和字段读取。

## 裁剪与依赖

`XHTTP_FEATURE_MULTIPART` 依赖 `XHTTP_FEATURE_HTTP_CONTENT_DISPOSITION`，由该层明确拉入媒体类型、HTTP 参数、RFC 8187 扩展值和 percent codec。只启用 `XHTTP_FEATURE_MIME` 时不会携带 multipart 或扩展文件名能力。

当前模块只负责边界、完整正文迭代、调用方数组解析和 form-data Part 元数据。流式 Reader、写出器、随机边界和拥有型表单容器采用独立裁剪宏建设，不会强迫整包解析用户引入额外状态机或随机源。

## Boundary

`xmultipartboundary` 持有解码后的 boundary，最大 70 字节，并额外保留零结尾。固定 70 字节来自协议上限，不是随连接分配的工作缓冲。

- `xrtMultipartBoundaryParse` 校验已经解码的 boundary。
- `xrtMultipartBoundaryFromContentType` 严格解析 `multipart/*` 媒体类型，查找唯一 `boundary` 参数并处理 quoted-string 转义。
- `xrtMultipartBoundaryView` 返回借用结构的视图。

允许的字符是 RFC 2046 的 `bchars`，内部可含普通空格，但最后一个字符不能是空格。解析器不接受控制字符、`@` 等集合外字符，也不会把畸形 quoted-string 当作原始 boundary 使用。

Boundary、limits、迭代游标、Part、计数和错误位置都支持完整但未对齐的固定存储，
便于 C FFI 和线缆映射调用。入口先验证地址范围和别名，再在对齐局部对象中解析，
最后一次性发布结果；地址回绕和非法输出重叠不会触碰输入或其他输出。

## 完整正文迭代

`xrtMultipartNext` 使用 `XHTTP_NEXT_ITEM`、`XHTTP_NEXT_END`、`XHTTP_NEXT_ERROR` 区分 Part、完整关闭和失败。`Offset` 初始为零；解析器忽略 preamble 和 epilogue，但要求至少一个 Part 和关闭边界。

```c
xmultipartboundary boundary;
xmultipartpart part;
xmultiparterrorinfo error;
size_t offset = 0;

xrtMultipartBoundaryFromContentType(contentType, &boundary);
while ( xrtMultipartNext(
	body, &boundary, &offset, &part, &error
) == XHTTP_NEXT_ITEM ) {
	/* 立即处理借用的 part.Headers、part.Body 和元数据。 */
}
```

边界只能出现在行首。Part 正文前用于引入 delimiter 的 CRLF 不属于正文，因此 `part.Body` 可以精确表示不以换行结尾的二进制数据。接收端接受 delimiter 后的空格或制表符 transport padding；普通 delimiter 必须以 CRLF 结束，关闭 delimiter 可直接结束正文，也可用 CRLF 引出应忽略的 epilogue。

近似文本如 `\r\n--boundary-extra` 不会被误识别为边界。缺少最终关闭边界、裸 LF、非法 Header、重复 `Content-Disposition`、重复 `Content-Type` 和重复 `Content-Transfer-Encoding` 都返回错误。

## Part

`xmultipartpart` 的所有视图都借用原始完整正文：

- `Headers`：不含分隔正文的空行，可用 `xrtHttpFieldNext` 读取任意扩展字段。
- `Body`：不含属于下一条 delimiter 的 CRLF。
- `Disposition`：出现 `Content-Disposition` 时由共享 MIME 层严格解析。
- `ContentType`：出现 `Content-Type` 时由共享媒体类型层严格解析。
- `TransferEncoding`：保留旧 MIME 兼容信息；RFC 7578 已不建议 HTTP form-data 发送该字段。

`xrtMultipartFormPartValid` 要求 disposition 类型为 `form-data` 且包含 `name`。同名字段和多个文件保持原始顺序，解析器不会错误地把它们折叠为字典。

`xrtMultipartPartNameWrite` 解码 `name` 参数，`xrtMultipartPartFileNameWrite` 读取文件名并复用 Content-Disposition 的字符集回退规则。文件名仍是不可信协议数据；落盘前必须由应用执行路径分量、保留名、长度和权限检查。

## 限制与数组解析

`xrtMultipartLimitsInit` 默认限制 1024 个 Part、每个 Part 64 个 Header、64 KiB Header 字节。Part 正文和完整正文默认不替应用决定上限，使用 `SIZE_MAX`；HTTP 客户端和服务器应按上传路由或响应策略显式设置。

`xrtMultipartValidate` 校验整个正文及限制。`xrtMultipartParse` 把结果写入调用方数组：

- `pParts == NULL` 且容量为零时只返回所需数量。
- 容量不足时返回所需数量，不写出部分结果。
- 成功结果仍借用原始正文，不产生每 Part 分配。

整包接口适合已经完整驻留内存的小表单和 MIME 对象。大文件上传应使用后续独立的流式 Reader，以 `Consumed + DATA view` 直接处理网络接收缓冲，而不是先缓存完整正文。

## 范例

`examples/http/multipart/main.c` 展示 boundary 初始化、Part 迭代、字段名读取和二进制正文访问。



## Writer

启用 `XHTTP_FEATURE_MULTIPART_WRITE` 会引入完整正文解析依赖。Writer 不持有状态、不分配内存，所有接口都支持 `pOutput == NULL && iCapacity == 0` 的精确容量查询，并保证容量不足时不产生部分结果。

常见表单直接使用：

```c
xrtMultipartFieldWrite(&boundary, XRT_STR_LITERAL("name"),
	(xbytesview){ (const uint8*)"value", 5 },
	output, capacity, &size);

xrtMultipartFileWrite(&boundary, XRT_STR_LITERAL("file"),
	XRT_STR_LITERAL("a.txt"), XRT_STR_LITERAL("text/plain"),
	body, output, capacity, &size);
```

大型正文或向量发送使用 `xrtMultipartPartHeadWrite()`、直接发送正文、`xrtMultipartPartEndWrite()` 三段组合，最后调用 `xrtMultipartCloseWrite()`。这条路径不会为了构建协议包复制文件正文。

`xrtMultipartPartWrite()` 是原始字段数组的完整 Part 便利层。它只约束字段行语法，不强制特定 MIME 字段集合，因此可以承载自定义 multipart 子类型。

Writer 直接生成 `filename="..."`，不会生成 RFC 7578 不建议用于 form-data 的 `filename*`；解析器仍接受 `filename*`，用于兼容已有客户端。

`examples/http/multipart_write/main.c` 展示普通字段、文件字段和关闭 boundary 的组合。



`xrtMultipartFormHeadWrite()` 是标准 form-data 的流式头部层：它负责 `Content-Disposition: form-data`、可选 `filename`、可选 `Content-Type` 与终止空行，但不写正文。字段名允许为空；`Filename == NULL` 表示参数缺席，而空视图表示参数存在但值为空。FormData、文件上传和自定义流式正文由此共享同一套协议写出规则。

启用独立的 `XHTTP_FEATURE_MULTIPART_RANDOM` 后，`xrtMultipartBoundaryRandom()` 使用系统安全随机源生成带 `xrt-form` 标识的 128 位随机 boundary。结果固定为 13 字节前缀加 32 个小写十六进制字符，共 45 字节；函数支持未对齐输出，并只在随机源和完整编码成功后原子发布。显式 boundary 的 parser、writer 和流 Reader 不依赖随机模块；需要一步构建表单正文时可直接使用 `xrtFormDataBodyRandom()`。


## Stream Reader

启用 `XHTTP_FEATURE_MULTIPART_STREAM` 会加入无分配 Reader。`xmultipartreader` 只拥有 boundary、限制、计数和状态，不拥有网络输入，也没有每连接固定正文缓冲。

Reader 是会被原地更新的有状态对象，必须使用自然对齐的存储；初始化会立即快照可未对齐的
boundary 与 limits。每次 Read 的 Consumed、Part、Data 和 Error 是固定输出描述符，
可以未对齐。入口还会验证公开计数没有超过硬限制，调用方意外破坏 Reader 时直接进入
稳定参数错误，不会因无符号预算下溢而放宽限制。

`xrtMultipartReaderRead()` 每次返回一个状态：

- `XMULTIPART_READ_MORE`：保留 `Input[Consumed..]`，追加更多输入后重试。
- `XMULTIPART_READ_PART`：`Part` 借用本次输入，调用方应立即读取名称、文件名和 Content-Type。
- `XMULTIPART_READ_DATA`：`Data` 借用本次输入，可以直接写文件、计算摘要或交给上层流消费者。
- `XMULTIPART_READ_PART_END`：当前 Part 完成。
- `XMULTIPART_READ_DONE`：关闭 boundary 和 epilogue 已全部消费。
- `XMULTIPART_READ_ERROR`：Reader 进入稳定错误终态，后续调用仍返回原错误。

`bEnd` 只能在调用方确定 multipart 正文已经没有后续字节时设置。对 HTTP/1 来说，这通常来自 Content-Length 或 chunked reader 的消息结束，而不是 TCP 临时无数据。

Reader 在 boundary 或 Header 被网络分块截断时返回 `MORE` 且保留必要尾部。调用方决定接收缓冲区的分配、增长和复用策略；协议层不会把上传正文复制到对象私有缓冲。

`xmultipartlimits` 同时约束完整解析器和流 Reader：

- `MaxParts`、`MaxHeaders`
- `MaxDelimiterBytes`，包含 delimiter 行及其 transport padding
- `MaxHeaderBytes`
- `MaxPartBytes`
- `MaxBodyBytes`，表示完整 multipart 线缆正文上限

`xrtMultipartReaderReset()` 保留 boundary 和限制，清空进度以复用对象。

`examples/http/multipart_stream/main.c` 展示从借用输入直接消费正文数据。
