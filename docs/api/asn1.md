# ASN.1 DER

`asn1_der` 是 X.509、PKCS、TLS 和其他 ASN.1 协议共用的零分配底层。旧版把 DER 解析器隐藏在 `nettls.h` 中，导致证书、密钥和其他协议无法复用；新版先公开严格游标层，再由 X.509 提供上层对象与验证策略。

## 裁剪

```c
#define XRT_FEATURE_ASN1_DER
```

该模块只依赖 `core`，不依赖加密、文件或网络。

## 数据模型

- `xasn1tag`：标签类别、构造位和完整的 32 位高标签号。
- `xdervalue`：`Raw` 包含完整 TLV，`Value` 只包含内容，二者都借用原输入。
- `xdercursor`：保存输入、总长度和下一项偏移，可按值复制后独立遍历。
- `xderresult`：明确区分 `XDER_VALUE`、`XDER_DONE` 和 `XDER_ERROR`。

解析器不分配、不修改输入，也不在对象内保存外部状态。输入必须在所有借用视图使用完之前保持有效。

## 游标 API

```c
bool xrtDerInit(xdercursor* cursor, const void* data, size_t size);
xderresult xrtDerRead(xdercursor* cursor, xdervalue* value);
xderresult xrtDerPeek(const xdercursor* cursor, xdervalue* value);

bool xrtDerExpect(
	xdercursor* cursor,
	xasn1class tagClass, uint32_t tagNumber, bool constructed,
	xdervalue* value
);

bool xrtDerEnter(const xdervalue* value, xdercursor* child);
bool xrtDerDone(const xdercursor* cursor);
size_t xrtDerRemaining(const xdercursor* cursor);
```

`Read`、`Peek` 和 `Expect` 只在成功时发布输出；`Read` 和 `Expect` 也只在成功时推进游标。正常读完使用 `XDER_DONE`，不会伪装成错误。

`xrtDerExpect` 适合协议结构中标签固定的路径；`xrtDerRead` 和 `xrtDerIs` 适合 CHOICE、OPTIONAL 与扩展字段。`xrtDerEnter` 允许逐层进入 SEQUENCE、SET 和显式上下文标签，而不复制内容。

## 严格 DER

每次读取都会拒绝：

- BER 无限长度和截断内容；
- 非最短、前导零或溢出的长度；
- 非最短、截断或超过 32 位的高标签号；
- primitive/constructed 形式错误的常用 Universal 类型；
- 非规范 BOOLEAN、INTEGER、ENUMERATED、BIT STRING、NULL 和 OID。

```c
bool xrtDerValidate(const void* data, size_t size);
```

`xrtDerValidate` 进一步验证整个构造树，要求输入恰好包含一个顶层值、SET 成员按完整 DER 编码排序，并限制最大嵌套为 64 层。游标解析适合受协议结构约束的高性能路径；完整验证适合接收证书、密钥和其他不可信独立 DER 文档的入口。

## 类型辅助层

```c
bool xrtDerBoolean(const xdervalue* value, bool* result);
bool xrtDerUnsigned(const xdervalue* value, xbytesview* result);
bool xrtDerUInt64(const xdervalue* value, uint64_t* result);
bool xrtDerBitString(
	const xdervalue* value, xbytesview* result, uint8_t* unusedBits
);
bool xrtDerOctets(const xdervalue* value, xbytesview* result);
bool xrtDerOid(const xdervalue* value, xbytesview* result);
bool xrtDerOidEqual(
	const xdervalue* value, const void* oid, size_t oidSize
);
```

这些函数仍返回借用视图。`xrtDerUnsigned` 拒绝负整数并去除正数为避免符号歧义而使用的单个前导零；`xrtDerUInt64` 额外检查范围；`xrtDerBitString` 单独返回未使用位数。

## 错误

DER 错误使用 `xrt.asn1` 域和 `XASN1_ERROR_*` 稳定代码。错误对象的 `Data` 在适用时包含 `offset=<字节偏移>`，便于上层错误映射和 C 日志定位输入。类型、协议、范围与参数错误分别使用统一的 `XERR_TYPE`、`XERR_PROTOCOL`、`XERR_RANGE` 和 `XERR_ARGUMENT` 类别。

## 示例与测试

- `examples/asn1/der/main.c`
- `tests/asn1/test_der.c`
- `tests/single/test_single_asn1_der.c`

测试覆盖高标签号、长长度、嵌套、SET 排序、尾随值、64 层深度门槛、常用 Universal 类型、失败原子性，以及旧解析器未拒绝的 BER 和非规范值。
