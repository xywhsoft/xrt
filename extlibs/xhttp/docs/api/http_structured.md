# HTTP Structured Fields

`http_structured` 实现 RFC 9651（替代并向后兼容 RFC 8941）的 Structured Field Values。它是 Priority、Cache-Status、Proxy-Status 等现代 HTTP 字段的公共协议底座，不绑定客户端、服务器或网络对象。

## 裁剪与依赖

```c
#define XHTTP_FEATURE_HTTP_STRUCTURED
```

该层依赖 `http`、`codec_base64` 和 `unicode`。Base64 用于 Byte Sequence，Unicode 只用于 Display String 的严格 UTF-8 校验；不依赖字符串对象、容器、Buffer 或网络，因此全部解析、查找和调用方缓冲解码都不分配堆内存。

规范序列化是独立裁剪层：

```c
#define XHTTP_FEATURE_HTTP_STRUCTURED_WRITE
```

它依赖解析层，但只需要读取字段的程序不会携带序列化实现。

## 数据模型

顶层类型是 Item、List 和 Dictionary。List 与 Dictionary 成员可以是 Item 或 Inner List；Item 和 Inner List 均可带参数。裸值支持：

- `XHTTP_STRUCTURED_INTEGER`：范围为正负 `999999999999999`；
- `XHTTP_STRUCTURED_DECIMAL`：`Number` 使用千分之一为单位，例如 `-12.34` 是 `-12340`；
- `XHTTP_STRUCTURED_STRING`：`Encoded` 借用不含引号但仍含反斜杠转义的线路内容；
- `XHTTP_STRUCTURED_TOKEN`：`Encoded` 直接借用 token；
- `XHTTP_STRUCTURED_BYTES`：`Encoded` 借用不含冒号的 Base64；
- `XHTTP_STRUCTURED_BOOLEAN`：`Number` 为零或一；
- `XHTTP_STRUCTURED_DATE`：`Number` 是 Unix epoch 秒；
- `XHTTP_STRUCTURED_DISPLAY`：`Encoded` 借用不含 `%"` 与末尾引号的百分号线路内容。

解析结构保留线路表示，避免零分配 API 把转义切片误报成已解码抽象值。String、Byte Sequence 和 Display String 分别通过 `xrtHttpStructuredStringDecode`、`xrtHttpStructuredBytesDecode`、`xrtHttpStructuredDisplayDecode` 解码；输出为 `NULL` 且容量为零时只验证并查询长度，不写零结尾。
这三类描述符的 `Number` 必须为零，手工构造的类型与载荷不一致描述符会以
`XERR_VALUE` 拒绝。

## 解析层次

```c
xhttpnext xrtHttpStructuredBareNext(...);
bool xrtHttpStructuredItemParse(...);
xhttpnext xrtHttpStructuredParameterNext(...);
xhttpnext xrtHttpStructuredInnerNext(...);
xhttpnext xrtHttpStructuredListNext(...);
xhttpnext xrtHttpStructuredDictionaryNext(...);
xhttpnext xrtHttpStructuredDictionaryMapNext(...);
```

`BareNext` 是最低层增量入口。`ItemParse` 要求消耗完整顶层值。List、Dictionary 和 Inner List 使用调用方持有的 `size_t` 游标；游标必须从零开始，首次调用会线性验证完整值后才发布第一个成员，迭代期间输入必须保持不变。`XHTTP_NEXT_ITEM`、`XHTTP_NEXT_END`、`XHTTP_NEXT_ERROR` 明确区分成员、正常结束和格式失败。

所有顶层解析都执行 RFC 9651 的严格语义：顶层只把 SP 当作前后空白，List/Dictionary 的逗号处允许 OWS，Inner List 只允许 SP 分隔项目，尾随逗号、空成员、非法转义、非 ASCII 线路字节和剩余内容都会使完整值失败。

## 有序 Map

参数与 Dictionary 都是有序 map。`DictionaryNext` 是线路迭代器，会暴露每次
出现，便于协议诊断；`DictionaryMapNext` 和随机访问 API 表达抽象 map：key 的
位置按第一次出现确定，值取最后一次出现。

```c
size_t xrtHttpStructuredParameterCount(...);
xhttpnext xrtHttpStructuredParameterAt(...);
xhttpnext xrtHttpStructuredParameterFind(...);

size_t xrtHttpStructuredDictionaryCount(...);
xrtHttpStructuredMapCursorInit(&Cursor);
xhttpnext xrtHttpStructuredDictionaryMapNext(...);
xhttpnext xrtHttpStructuredDictionaryAt(...);
xhttpnext xrtHttpStructuredDictionaryFind(...);
```

这些入口会先校验完整值，不会因为目标 key 位于畸形前缀之前而接受整个字段。无成员返回 `END`，格式错误返回 `ERROR`，Count 格式错误返回 `XRT_NPOS`。
需要顺序读取多个成员时应使用 `DictionaryMapNext`，不要在循环中调用 `At`。
在不分配索引的前提下，重复 key 的“首次位置、最后值”语义使完整顺序迭代的
最坏复杂度为 O(n^2)；实现不会退化为逐项 `Count + At` 造成的 O(n^3)。

## 重复字段行

RFC 9651 要求同名 HTTP 字段行按逗号逻辑组合后再解析：

```c
xrtHttpStructuredFieldCursorInit(&Cursor);
while ( xrtHttpStructuredListFieldNext(
	Fields, FieldCount, XRT_STR_LITERAL("Example"),
	&Cursor, &Member
) == XHTTP_NEXT_ITEM ) {
	/* 使用 Member。 */
}
```

`xrtHttpStructuredListFieldNext` 和 `xrtHttpStructuredDictionaryFieldNext` 会跨越全部大小写不敏感的同名字段，并在发布首个成员前验证所有字段行。多个同名行中的空值等价于组合后的空成员，因此会被拒绝。游标在第一次成功调用后绑定字段数组、字段名和 List/Dictionary 类型；切换任一项都会以 `XERR_ARGUMENT` 原子失败。Item 不能通过逗号组合，`xrtHttpStructuredItemField` 只接受唯一同名字段；缺失返回 `END`，重复或畸形返回 `ERROR`。

跨字段 Dictionary 同样提供 `xrtHttpStructuredDictionaryMapFieldNext`、
`xrtHttpStructuredDictionaryFieldCount`、`xrtHttpStructuredDictionaryFieldAt` 和
`xrtHttpStructuredDictionaryFieldFind`。重复 key 即使分布在不同字段行，也按
首次出现位置保序并采用最后一次值。

迭代期间字段数组、字段名和借用内容必须保持不变。游标副本只可继续读取同一
来源，不能复制到另一组字段或另一种迭代操作继续使用。

## 规范序列化

写出层使用调用方提供的轻量描述符：`xhttpstructuredvalue`、`xhttpstructureditemvalue`、`xhttpstructuredmembervalue`、`xhttpstructureddictionaryentry`。String、Token 和 Display 的 `Data` 是未编码文本；Bytes 的 `Data` 是任意二进制字节。四个入口覆盖从底层到完整字段：

```c
bool xrtHttpStructuredBareWrite(...);
bool xrtHttpStructuredItemWrite(...);
bool xrtHttpStructuredListWrite(...);
bool xrtHttpStructuredDictionaryWrite(...);
```

输出严格采用 RFC 9651 规范形式：Decimal 最多三位小数且至少一位，String 只转义引号和反斜杠，Byte Sequence 使用标准填充 Base64，Display String 使用小写百分号十六进制，参数和 Dictionary 中的 Boolean true 省略值，成员之间使用逗号加一个 SP。空 List/Dictionary 产生长度为零的省略值。

写出 API 先遍历完整描述符树，验证类型、范围、UTF-8、唯一 key、未使用字段和全部借用范围，再检查容量和目标重叠，最后才写入。因此格式、重叠和容量失败不会留下部分线路内容。输出不附加零字节；传入空输出和零容量可查询精确长度。

## 内存契约

- 解析结果全部借用输入，输入生命周期必须覆盖结果使用期；
- 固定描述符允许未对齐存储，实现通过 `memcpy` 发布；
- 线路字段游标和有序 Map 游标同样允许未对齐存储，并绑定首次来源；
- 解码与规范序列化的长度输出也允许未对齐存储；
- 游标、输出描述符和借用输入不得互相覆盖；
- String、Bytes 和 Display 解码允许输出与 `Encoded.Data` 从同一地址开始，其他部分重叠被拒绝；
- 规范序列化拒绝输出与任一描述符、key 或数据视图重叠；
- 格式失败不推进游标，也不发布部分描述符；容量不足只通过长度输出报告所需空间。

## 示例与测试

- `examples/http/structured/main.c`
- `tests/http/test_http_structured.c`
- `tests/http/test_http_structured_mutation.c`
- `tests/http/test_http_structured_noalloc.c`
- `tests/http/test_http_structured_write.c`
- `tests/http/test_http_structured_property.c`
- `tests/http/test_http_structured_write_noalloc.c`
- `tests/single/test_single_http_structured.c`
- `tests/single/test_single_http_structured_write.c`

实现遵循 [RFC 9651](https://www.rfc-editor.org/rfc/rfc9651.html)，覆盖八种裸值、数值边界、参数、Inner List、线路与有序 Map 迭代、重复 key、重复字段行、来源绑定、Base64 可选填充、Display String UTF-8、1024 成员规模、未对齐描述符与长度输出、失败原子性、无分配和单头发布。
