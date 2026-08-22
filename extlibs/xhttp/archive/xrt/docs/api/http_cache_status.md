# HTTP Cache-Status

`http_cache_status` 实现 RFC 9211 的 `Cache-Status` 响应字段。它建立在 RFC 9651 Structured Fields 上，只表达缓存链路诊断，不绑定缓存存储、HTTP 客户端或服务器对象。

## 裁剪与依赖

```c
#define XRT_FEATURE_HTTP_CACHE_STATUS
```

解析层依赖 `http_structured`，全程借用字段值且不分配。生产端写出是独立裁剪层：

```c
#define XRT_FEATURE_HTTP_CACHE_STATUS_WRITE
```

写出层依赖 `http_cache_status` 和 `http_structured_write`。

## 解析层次

`xrtHttpCacheStatusNext` 迭代一个字段值，`xrtHttpCacheStatusFieldNext` 跨越全部重复字段行。两个游标分别由 `xrtHttpCacheStatusCursorInit` 和 `xrtHttpCacheStatusFieldCursorInit` 初始化。

游标在首次成功调用时绑定输入地址和长度或字段数组地址和数量。直到重新初始化游标，调用方必须保持该来源的地址、大小和内容不变；即使新来源长度或字段数量相同，也不能复用已经推进的游标。

```c
xhttpcachestatusfieldcursor Cursor;
xhttpcachestatus Status;

xrtHttpCacheStatusFieldCursorInit(&Cursor);
while ( xrtHttpCacheStatusFieldNext(
	Fields, FieldCount, &Cursor, &Status
) == XHTTP_NEXT_ITEM ) {
	/* 按从源站到用户侧的顺序处理 Status。 */
}
```

首次迭代会先验证完整 List、重复字段组合和全部缓存标识，后续成员畸形不会在已经发布前缀后才暴露。每个成员标识必须是 String 或 Token；`Cache` 保留 Structured Fields 裸值，可用通用 String 解码函数读取转义内容。

## 已知参数

`xhttpcachestatus` 类型化公开 `hit`、`fwd`、`fwd-status`、`ttl`、`stored`、`collapsed`、`key` 和 `detail`。`Flags` 表示最后一次出现且类型、范围正确的参数，`InvalidFlags` 使用相同位表示最后值无效。重复参数遵循 Structured Fields 的最后值规则。

- `fwd-status` 只接受 `100..599`；
- `ttl` 是可以为负数的 Integer；
- `key` 必须是 String；
- `detail` 可以是 String 或 Token；
- 未知扩展参数不会丢失，`Parameters` 可继续交给 `xrtHttpStructuredParameterNext`。

类型错误的已知参数不会让整个诊断链不可读，而是进入 `InvalidFlags`。字段结构、分隔符或成员标识错误仍返回 `XHTTP_NEXT_ERROR`。

`Issues` 暴露 RFC 中非致命但可疑的组合：同时出现 `hit` 与 `fwd`，以及没有 `fwd` 却出现 `fwd-status`、`stored` 或 `collapsed`。解析器不替应用删除这些线路信息。

## 写出

```c
bool xrtHttpCacheStatusWrite(
	const xhttpstructureditemvalue* status,
	void* output,
	size_t capacity,
	size_t* size
);
```

该入口写出一个完整成员，也就是一个合法的单成员字段值，适合中间缓存通过新增字段行追加自己的诊断。它先验证参数数组和借用视图，再读取已知 key、验证缓存标识与全部已知参数的生产类型，并让通用 Structured Item 写出器负责唯一 key、转义、规范 Boolean 和输出原子性。扩展参数直接放入 `Parameters` 数组。

需要一次构建多个成员时，使用 `xrtHttpStructuredListWrite`；需要固定响应时可以直接提交已经拼好的字段值，协议层不强制构建对象。

## 内存契约

- 解析结果中的标识、Token、String 编码区和参数区都借用输入；
- 游标和输出允许未对齐存储，但不得覆盖输入、字段数组或彼此；游标绑定期间来源必须保持不可变；
- 首次完整验证失败不推进游标、不修改输出；
- 写出不附加零字节，空输出与零容量用于查询长度；
- 解析与写出均不分配堆内存。

## 示例与测试

- `examples/http/cache_status/main.c`
- `examples/http/cache_status_write/main.c`
- `tests/http/test_http_cache_status.c`
- `tests/http/test_http_cache_status_noalloc.c`
- `tests/http/test_http_cache_status_write.c`
- `tests/http/test_http_cache_status_write_noalloc.c`
- `tests/single/test_single_http_cache_status.c`
- `tests/single/test_single_http_cache_status_write.c`

实现遵循 [RFC 9211](https://www.rfc-editor.org/rfc/rfc9211.html)，Structured Fields 线路格式遵循 [RFC 9651](https://www.rfc-editor.org/rfc/rfc9651.html)。
