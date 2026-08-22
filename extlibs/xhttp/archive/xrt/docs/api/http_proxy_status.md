# HTTP Proxy-Status

`http_proxy_status` 实现 RFC 9209 的 `Proxy-Status` 响应字段，并识别 RFC 9532 的 `next-hop-aliases` 参数。它只表达中间节点处理和错误诊断，不绑定代理对象、HTTP 客户端或服务器。

## 裁剪与依赖

```c
#define XRT_FEATURE_HTTP_PROXY_STATUS
```

解析层依赖 `http_structured`，借用输入且不分配。生产端写出由独立的 `XRT_FEATURE_HTTP_PROXY_STATUS_WRITE` 控制，并依赖 `http_structured_write`。

RFC 9532 别名列表位于独立的 `XRT_FEATURE_HTTP_PROXY_ALIAS` 层，依赖
`http_proxy_status` 与 `codec_percent`；生产端由
`XRT_FEATURE_HTTP_PROXY_ALIAS_WRITE` 控制。未启用该层时，Proxy-Status 仍可
识别并保留 `next-hop-aliases` String，不会携带百分号编解码实现。

`Proxy-Status` 与 `Cache-Status` 的完整 List 预校验、重复字段组合、游标、别名和失败原子性共用 Structured Fields 内部状态遍历器，没有维护两份协议框架代码。

## 解析

```c
xhttpproxystatusfieldcursor Cursor;
xhttpproxystatus Status;

xrtHttpProxyStatusFieldCursorInit(&Cursor);
while ( xrtHttpProxyStatusFieldNext(
	Fields, FieldCount, &Cursor, &Status
) == XHTTP_NEXT_ITEM ) {
	/* 从靠近源站到靠近用户端处理 Status。 */
}
```

`xrtHttpProxyStatusNext` 处理一个字段值，`xrtHttpProxyStatusFieldNext` 跨越重复字段行。首次迭代先验证完整 Structured List 和全部代理标识，因此后续成员格式错误不会在发布前缀后才出现。代理标识必须是 String 或 Token。

游标在首次成功调用时绑定字段值地址和长度或字段数组地址和数量。重新初始化之前，来源的地址、大小和内容都必须保持不变；等长文本或等数量的另一组字段也不能继续使用原游标。

## 已知参数

`xhttpproxystatus` 类型化公开：

- `error`：Token；
- `next-hop`：String 或 Token；
- `next-protocol`：ALPN ID 长度必须为 `1..XHTTP_PROXY_ALPN_MAX` 字节；能表示为 ASCII Token 时必须使用 Token，只有不能表示为 Token 时才使用 Byte Sequence；
- `received-status`：`100..599` 的 Integer；
- `details`：String；
- `next-hop-aliases`：RFC 9532 String。

`Flags` 表示最后值类型和范围正确，`InvalidFlags` 使用相同位报告最后值无效。未知参数与代理错误的附加参数保留在 `Parameters`，可继续交给 `xrtHttpStructuredParameterNext`。String 和 Byte Sequence 分别使用通用 `xrtHttpStructuredStringDecode`、`xrtHttpStructuredBytesDecode` 解码。

代理错误类型来自可扩展 IANA registry，库返回原始 Token，不发布容易过期的封闭枚举。应用可以识别当前错误集合，同时可靠地记录和转发未来扩展。

`next-hop-aliases` 的 Structured String 正文是 RFC 9532 规定的逗号分隔、
百分号编码 DNS 名称列表。Proxy-Status 核心只保留完整内容，不把 DNS 名称策略
绑定到代理字段解析器；需要类型化处理时使用下面的独立别名层。

## 下一跳别名

```c
xhttpproxyaliascursor Cursor;
xstrview Alias;

xrtHttpProxyAliasCursorInit(&Cursor);
while ( xrtHttpProxyAliasNext(
	Status.NextHopAliases.Encoded, &Cursor, &Alias
) == XHTTP_NEXT_ITEM ) {
	/* Alias 是借用的百分号编码名称。 */
}
```

`xrtHttpProxyAliasesValid` 验证完整列表，`xrtHttpProxyAliasNext` 首次调用也会先
完成相同预校验，所以畸形后缀不会在已经发布前缀后才失败。空 String 合法地表示
DNS 解析没有遇到 CNAME；非空列表拒绝空项、空白、直接出现的保留字符、畸形
percent triplet，以及 percent 解码后不是 `\.` 或 `\\` 的反斜杠用法。

别名游标同样绑定首次列表的地址和长度，且初始化状态必须是全零。列表在迭代结束或游标重新初始化前不得移动、替换或修改。

`xrtHttpProxyAliasRead` 百分号解码一项，但有意保留 RFC 9532 的 DNS 展示形式
反斜杠：`dot%5C.label` 得到 `dot\.label`，从而不会把标签内句点误当成标签
分隔符；`backslash%5C%5Cname` 得到 `backslash\\name`。输出可以是二进制，
不附加零字符。

生产端 `xrtHttpProxyAliasWrite` 写一个名称，`xrtHttpProxyAliasesWrite` 写名称数组，
输入使用上述 DNS 展示形式；所有非 URI unreserved 字节都会规范百分号编码。
`xrtHttpProxyAliasesBuild` 是返回零结尾文本的常用便捷路径，结果由 `xrtFree`
释放。直接写出支持精确长度
查询、容量不足时的所需长度发布和完整输出原子性。

## 写出

`xrtHttpProxyStatusWrite` 接受 `xhttpstructureditemvalue`，写出一个合法的单成员字段值，适合中间节点新增一条 `Proxy-Status` 字段行。专用层先验证参数描述符、代理标识、已知参数类型和 ALPN 规范形式，通用 Structured Item 写出器负责扩展参数、唯一 key、String 转义、Byte Sequence 和输出原子性。

多个成员可由 `xrtHttpStructuredListWrite` 一次构建；固定错误响应也可以直接提交已拼好的字段内容，不要求创建协议对象。

## 内存契约

- 解析结果全部借用原字段值；
- 游标和输出允许未对齐存储，但不得覆盖输入、字段数组或彼此；游标绑定期间来源必须保持不可变；
- 首次完整验证失败不推进游标、不修改输出；
- 参数类型错误通过 `InvalidFlags` 报告，不隐藏代理标识和未知扩展；
- 解析与写出不分配堆内存，写出不附加零字节。

## 示例与测试

- `examples/http/proxy_status/main.c`
- `examples/http/proxy_status_write/main.c`
- `examples/http/proxy_alias/main.c`
- `examples/http/proxy_alias_write/main.c`
- `tests/http/test_http_proxy_status.c`
- `tests/http/test_http_proxy_status_noalloc.c`
- `tests/http/test_http_proxy_status_write.c`
- `tests/http/test_http_proxy_status_write_noalloc.c`
- `tests/http/test_http_proxy_alias.c`
- `tests/http/test_http_proxy_alias_noalloc.c`
- `tests/http/test_http_proxy_alias_write.c`
- `tests/http/test_http_proxy_alias_write_noalloc.c`
- `tests/http/test_http_proxy_alias_write_oom.c`
- `tests/single/test_single_http_proxy_status.c`
- `tests/single/test_single_http_proxy_status_write.c`
- `tests/single/test_single_http_proxy_alias.c`
- `tests/single/test_single_http_proxy_alias_write.c`

实现遵循 [RFC 9209](https://www.rfc-editor.org/rfc/rfc9209.html)、[RFC 9532](https://www.rfc-editor.org/rfc/rfc9532.html) 和 [RFC 9651](https://www.rfc-editor.org/rfc/rfc9651.html)。
