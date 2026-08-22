# HTTP Digest 字段

`http_digest` 实现 RFC 9530 的 `Content-Digest`、`Repr-Digest`、
`Want-Content-Digest` 和 `Want-Repr-Digest`。协议层只表达字段语法和摘要目标，
不把摘要强制绑定到 HTTP 客户端、服务器、缓存或正文对象。
这里的 Digest 是内容完整性字段，不是 HTTP Digest Authentication；认证能力位于
独立的 `http_auth_digest` 系列模块。

RFC 9530 已废弃 RFC 3230 的 `Digest` 与 `Want-Digest`。新代码应明确选择消息
内容摘要或完整选定表示摘要，避免旧字段中“内容”和“表示”长期混淆的问题。

## 裁剪与依赖

- `XRT_FEATURE_HTTP_DIGEST`：类型化解析、重复字段组合和 Byte Sequence 解码；
  只依赖 `http_structured`。
- `XRT_FEATURE_HTTP_DIGEST_WRITE`：单算法摘要和偏好的常用写出；依赖
  `http_structured_write`。
- `XRT_FEATURE_HTTP_DIGEST_SHA2`：连续数据的 SHA-256/SHA-512 一步生成和验证；
  依赖 `crypto_sha256` 与 `crypto_sha512`。

只转发、记录或使用自定义算法的程序不需要带入 SHA 实现。所有直接解析、解码、
写出和 SHA-2 便利路径均不分配堆内存。

## 摘要解析

```c
xhttpdigestcursor Cursor;
xhttpdigest Digest;

xrtHttpDigestCursorInit(&Cursor);
while ( xrtHttpDigestFieldNext(
	Fields, FieldCount, XHTTP_DIGEST_CONTENT,
	&Cursor, &Digest
) == XHTTP_NEXT_ITEM ) {
	/* Algorithm、Value 和 Parameters 全部借用字段。 */
}
```

`XHTTP_DIGEST_CONTENT` 选择 `Content-Digest`，摘要输入是移除 HTTP 传输分帧后、
仍包含 Content-Encoding 效果的实际消息内容。`XHTTP_DIGEST_REPRESENTATION` 选择
`Repr-Digest`，摘要输入是完整选定表示；范围响应中它与只覆盖当前范围内容的
Content-Digest 可以不同。

`xrtHttpDigestNext` 处理单个字段值，`xrtHttpDigestFieldNext` 按 HTTP 字段组合
规则跨重复字段行处理。首次迭代会验证完整 Structured Dictionary 和所有成员，
只接受 Byte Sequence 值。重复算法按首次出现位置输出，值取最后一次线路值。
未知算法保持原 key，不使用容易过期的封闭枚举。

游标通过 `xrtHttpDigestCursorInit` 创建，第一次成功迭代后绑定值来源或字段数组、
摘要/偏好用途以及 Content/Representation 目标。迭代期间输入不可变；切换来源、
目标或用途会以 `XERR_ARGUMENT` 原子失败。实现直接复用 Structured Fields 的有序
Map 游标，不再维护另一套重复 key 算法。无分配顺序迭代的最坏复杂度为 O(n^2)，
并有 1024 个唯一算法成员的退化测试。

`xrtHttpDigestRead` 严格 Base64 解码 Byte Sequence，空输出查询精确长度，不附加
零字节。它也会验证算法 key、描述符载荷一致性和完整参数区。成员参数保留在
`Parameters`，可交给 Structured Fields 参数迭代器处理；输出与编码文本完全同址
时允许原地收缩解码，其他部分重叠会被拒绝。

## 偏好解析

`xrtHttpDigestPreferenceNext` 和 `xrtHttpDigestPreferenceFieldNext` 处理
`Want-Content-Digest` 与 `Want-Repr-Digest`。每个算法必须是 0 到 10 的 Integer：
0 表示不可接受，1 是最低偏好，10 是最高偏好。字段只是提示，库不把算法选择
策略硬编码进解析器；调用方可以结合本地算法强度、成本和优先顺序选择。

## 写出与 SHA-2

```c
char Value[96];
size_t Size;

xrtHttpDigestSha256Write(
	Body, BodySize, Value, sizeof(Value), &Size
);
```

`xrtHttpDigestWrite` 接受算法 key 和已经计算的二进制摘要；
`xrtHttpDigestPreferenceWrite` 写出单算法偏好。多算法或带扩展参数的场景直接使用
`xrtHttpStructuredDictionaryWrite`，避免再维护一套重复的 Dictionary 构建模型。

`xrtHttpDigestSha256Write` 与 `xrtHttpDigestSha512Write` 适合连续内存常用路径。
流式正文使用 `xrtSha256Init/Update/Final` 或 `xrtSha512Init/Update/Final`，完成后
把摘要交给 `xrtHttpDigestWrite`，无需在协议层缓存整段正文。
连续内存便利层先完成哈希再写字段，因此输出可以复用正文起始地址；输出和长度
输出不能重叠，所有内存范围会在读取正文前完成验证。

`xrtHttpDigestSha2Verify` 返回 `ERROR`、`UNSUPPORTED`、`MISMATCH` 或 `OK`，并以
常量时间比较支持算法的摘要。当前 IANA registry 中 Active 算法只有 `sha-256`
和 `sha-512`；废弃算法仍可通过通用解析与写出路径保留，但 SHA-2 便利层不会默认
启用它们。合法但未知的算法直接返回 `UNSUPPORTED`，不会解码其算法专属载荷或
计算正文哈希；非法算法 key 返回 `ERROR`。

## 内存与错误契约

- 解析结果借用输入，调用期间字段与线路文本必须保持不变；
- 首次完整预校验失败不推进游标、不修改输出；
- 游标绑定首次来源、用途和摘要目标，复制后也只能继续同一不可变来源；
- 游标、固定结果和长度输出允许合法未对齐存储；
- 直接写出容量不足时发布所需长度，不写部分字段值；
- 语法或成员类型错误使用 `XERR_VALUE`，参数、范围和 OOM 保留各自类别；
- Integrity 字段只能检测所摘要字节的变化，不认证其他 HTTP 字段或发送者身份。

## 示例与测试

- `examples/http/digest/main.c`
- `examples/http/digest_sha2/main.c`
- `tests/http/test_http_digest.c`
- `tests/http/test_http_digest_noalloc.c`
- `tests/http/test_http_digest_write.c`
- `tests/http/test_http_digest_write_noalloc.c`
- `tests/http/test_http_digest_sha2.c`
- `tests/http/test_http_digest_sha2_noalloc.c`
- `tests/single/test_single_http_digest.c`
- `tests/single/test_single_http_digest_write.c`
- `tests/single/test_single_http_digest_sha2.c`

实现遵循 [RFC 9530](https://www.rfc-editor.org/rfc/rfc9530.html)、
[RFC 9651](https://www.rfc-editor.org/rfc/rfc9651.html) 和
[IANA Hash Algorithms for HTTP Digest Fields](https://www.iana.org/assignments/http-digest-hash-alg/)。
