# FormData

`form_data.h` 提供拥有型、有序、区分大小写且允许重复名称的 `multipart/form-data` 容器。它不绑定 HTTP 客户端或服务器对象：容器、multipart 编码、完整正文解析分别裁剪，低层 multipart parser/writer 仍可独立使用。

## 所有权

- 名称、文件名和 Content-Type 在追加或设置时复制，每个 Part 只使用一个元数据分配。
- 正文使用 `xhttpbody` 独立引用，可以是固定字节、文件、变换器或应用自定义流。
- `xformdatapart` 借用容器；任何容器修改都会使旧视图失效。
- `Clone` 深复制元数据，但共享不可变正文对象。
- 容器本身不执行内部同步；并发读写或并发修改必须由调用方串行化。

配置、可选文件名描述符和 `At/Find` 输出结构支持合法的未对齐存储。非空结构体与
文本视图必须覆盖完整且不会地址回绕的连续范围；容器会立即快照输入描述符，并在
验证完成后一次性发布输出，失败不会改变容器或发布部分结果。

基础 `form_data` 只依赖 MIME 与 `xhttpbody`，不会引入 multipart parser、writer
或正文组合层。编码、解析和安全随机 boundary 是三个独立裁剪层；只需要保存和修改
字段的程序无需承担线缆协议实现。

## 修改语义

- `AppendText`、`AppendBytes` 和 `AppendBody` 保持插入顺序与重复项。
- `Set*` 在第一个同名位置提交新值并删除后续同名项；失败时原容器不变。
- `Remove` 删除全部同名项。
- `Get` 一步读取首个同名 Part，`Find` 顺序迭代全部同名 Part。
- 空名称有效；文件名指针为空表示 `filename` 参数缺席，空文件名视图表示参数存在但值为空。

## 正文与限制

`xrtFormDataBody()` 使用 multipart writer 构造协议元数据，再用正文组合层交错原 Part 正文。文件和流内容不会为了封包被合并复制。全部子正文长度已知时结果长度精确；任一未知时结果保持未知。只有全部子正文可重放时结果才可重放，异步 `AGAIN/Wait` 也由组合层透明传递。

选择 `form_data_random` 后，`xrtFormDataBodyRandom()` 使用系统安全随机源生成 128 位 boundary 并一步返回正文；随机功能独立裁剪，显式 boundary 的基础编码不依赖随机模块。

`MaxPartBytes` 和 `MaxBodyBytes` 使用 `XHTTP_BODY_UNKNOWN` 表示无限。有限限制不能接收未知长度正文，因为容器无法证明它满足配额。

## 解析

`xrtFormDataParse()` 和 `xrtFormDataParseContentType()` 适合已经完整驻留内存的小表单。它们先执行严格 multipart 校验，再复制名称、文件名、媒体类型和 Part 正文。大文件上传应直接使用 `xmultipartreader` 流式消费，避免完整缓存。

拥有型解析接受零 Part 的关闭 boundary，以便空 FormData 往返；通用 `xrtMultipartValidate()` 仍维持至少一个 Part 的严格 MIME 策略。需要实际变换的历史 `Content-Transfer-Encoding` 会被拒绝，不能静默把编码文本当原始文件字节。

正文、boundary、配置和 multipart 限制会在解析开始时验证并快照。结构体描述符与
`xmultiparterrorinfo` 输出支持合法的未对齐存储；回绕范围或与任一输入重叠的错误
输出会在读取正文前拒绝。零 Part 正文仍执行 `MaxBodyBytes` 与
`MaxDelimiterBytes`，不会借空表单快速路径绕过协议限额。

`xmultiparterrorinfo` 只表达线缆协议及 multipart 限额错误。本地内存不足、`xformdataconfig` 容器配额失败时，其 `Code` 保持 `XMULTIPART_ERROR_NONE`，详细原因由当前执行上下文的结构化 xrt 错误提供；两类错误不会伪装成 `Content-Disposition` 语法错误。

解析入口把参数错误、协议错误和 multipart 限额分别映射为 `XERR_ARGUMENT`、
`XERR_PROTOCOL` 和 `XERR_RANGE`，并统一发布 `form.data` 域的
`XFORM_DATA_ERROR_MULTIPART`。应用可以用结构化错误决定控制流，再用
`xmultiparterrorinfo` 定位具体线缆字段。

```c
xformdata* form = xrtFormDataCreate(NULL);
xmultipartboundary boundary;
xhttpbody* body;

xrtMultipartBoundaryParse(XRT_STR_LITERAL("----request"), &boundary);
xrtFormDataAppendText(
	form, XRT_STR_LITERAL("name"), XRT_STR_LITERAL("xrt")
);
body = xrtFormDataBody(form, &boundary);
```
