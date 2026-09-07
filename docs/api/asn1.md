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

解析器不分配、不修改输入，也不在对象内保存外部状态。输入必须在所有借用视图使用完之前保持有效。所有对象都是普通值：没有内部锁或共享可变状态，任意线程可并发使用各自持有的游标与值；读取视图与写入缓冲互不干扰。

## 游标 API

`Read`、`Peek` 和 `Expect` 只在成功时发布输出；`Read` 和 `Expect` 也只在成功时推进游标——失败调用对游标与输出都是空操作。正常读完使用 `XDER_DONE`，不会伪装成错误。

`xrtDerExpect` 适合协议结构中标签固定的路径；`xrtDerRead` 和 `xrtDerIs` 适合 CHOICE、OPTIONAL 与扩展字段。`xrtDerEnter` 允许逐层进入 SEQUENCE、SET 和显式上下文标签，而不复制内容。

### `xrtDerInit`

初始化一个借用输入的 DER 游标。只绑定指针与长度，不解析任何字节；内容合法性由后续读取决定。

```c
bool xrtDerInit(xdercursor* pCursor, const void* pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输出 | 非空 | 接收游标；`Offset` 置零 |
| `pData` | 输入 | 借用 | 输入字节；`iSize == 0` 时允许为空 |
| `iSize` | 输入 | — | 输入字节数；零表示空文档（首次读取得 `XDER_DONE`） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 游标已绑定输入 | — |
| `false` | 参数非法 | `*pCursor` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pCursor` 为空，或 `pData` 为空而 `iSize` 非零

#### 范例

[asn1/der · 解析链](../../examples/asn1/der/main.c) · 整体校验后建立根游标

```c
if ( !xrtDerValidate(Document, sizeof(Document)) ||
	!xrtDerInit(&Root, Document, sizeof(Document)) ||
```

### `xrtDerRead`

读取下一项并推进游标；失败时游标和输出保持不变。

```c
xderresult xrtDerRead(xdercursor* pCursor, xdervalue* pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入/输出 | 非空 | 游标；仅成功时 `Offset` 前移一个 TLV |
| `pValue` | 输出 | 非空 | 接收值视图（`Raw`/`Value` 均借用输入） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XDER_VALUE` | 取到下一项；`*pValue` 已发布、游标已推进 | — |
| `XDER_DONE` | 恰好消费完全部内容；不设置错误 | — |
| `XDER_ERROR` | 参数非法或结构非法 | 游标与 `*pValue` 均不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或游标偏移越界
- `XERR_PROTOCOL` + `XASN1_ERROR_TAG/LENGTH/VALUE` — 标签、长度或常用类型值非规范（详见"严格 DER"）
- `XERR_RANGE` + `XASN1_ERROR_TAG/LENGTH` — 高标签号或长度字节超出可表示范围

#### 范例

[asn1/der · 读取链](../../examples/asn1/der/main.c) · 三态返回串联两个 INTEGER

```c
(xrtDerRead(&Items, &Value) != XDER_VALUE) ||
!xrtDerUInt64(&Value, &iLeft) ||
(xrtDerRead(&Items, &Value) != XDER_VALUE) ||
!xrtDerUInt64(&Value, &iRight) ||
```

### `xrtDerPeek`

查看下一项但不推进游标；失败时输出保持不变。

```c
xderresult xrtDerPeek(const xdercursor* pCursor, xdervalue* pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入 | 非空 | 游标；本调用不改变它 |
| `pValue` | 输出 | 非空 | 接收值视图；随后仍可用 `Read`/`Expect` 消费同一项 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XDER_VALUE` | 取到下一项；`*pValue` 已发布，游标不变 | — |
| `XDER_DONE` | 已到末尾；不设置错误 | — |
| `XDER_ERROR` | 参数非法或结构非法 | `*pValue` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtDerRead`（`ARGUMENT`；`PROTOCOL`/`RANGE` + `XASN1_ERROR_*`）

#### 范例

[asn1/decode_tour · 窥视](../../examples/asn1/decode_tour/main.c) · Peek 后 Remaining 不变

```c
(void)xrtDerPeek(&Cursor, &Value);
printf("peek-tag=%u remaining-after-peek=%zu",
	(unsigned)Value.Tag.Number, xrtDerRemaining(&Cursor));
```

### `xrtDerExpect`

读取并要求下一项具有指定标签；标签不符或缺失时不推进游标。

```c
bool xrtDerExpect(
	xdercursor* pCursor,
	xasn1class Class,
	uint32 iNumber,
	bool bConstructed,
	xdervalue* pValue
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入/输出 | 非空 | 游标；仅成功时推进一个完整 TLV |
| `Class` | 输入 | 枚举范围 | 期望的标签类别 |
| `iNumber` | 输入 | — | 期望的标签号 |
| `bConstructed` | 输入 | — | 期望的构造位 |
| `pValue` | 输出 | 非空 | 接收匹配项的值视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 标签匹配；`*pValue` 已发布、游标已推进 | — |
| `false` | 参数非法、结构非法、到尾或标签不符 | 游标与 `*pValue` 均不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或 `Class` 超出枚举范围
- `XERR_PROTOCOL` + `XASN1_ERROR_END` — 游标已到尾，期望的值缺失
- `XERR_TYPE` + `XASN1_ERROR_TYPE` — 下一项存在但标签与期望不符
- 其余同 `xrtDerPeek`（结构非法时透传解析错误）

#### 范例

[asn1/der · 解析链](../../examples/asn1/der/main.c) · 根元素必须是 SEQUENCE

```c
!xrtDerExpect(
	&Root, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true, &Value
) || !xrtDerEnter(&Value, &Items) ||
```

### `xrtDerEnter`

从一个构造值的内容初始化子游标；不复制内容。

```c
bool xrtDerEnter(const xdervalue* pValue, xdercursor* pCursor);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 构造类型的值视图（来自 `Read`/`Peek`/`Expect`） |
| `pCursor` | 输出 | 非空 | 接收子游标，范围为 `pValue` 的内容字节 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 子游标已建立 | — |
| `false` | 参数非法或值不是构造类型 | `*pCursor` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空
- `XERR_TYPE` + `XASN1_ERROR_TYPE` — 尝试进入 primitive 值

#### 范例

[asn1/der · 解析链](../../examples/asn1/der/main.c) · 进入 SEQUENCE 内部

```c
!xrtDerExpect(
	&Root, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true, &Value
) || !xrtDerEnter(&Value, &Items) ||
```

### `xrtDerDone`

返回游标是否已经恰好消费完全部内容。

```c
bool xrtDerDone(const xdercursor* pCursor);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入 | 非空 | 待检查的游标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `Offset == Size`：恰好消费完 | — |
| `false` | 还有剩余或游标非法 | 纯查询，不设置错误 |

#### 错误

- 无 — 判定结果即答案；与 `Read` 返回 `XDER_DONE` 后联用可拒绝尾随垃圾

#### 范例

[asn1/der · 收尾断言](../../examples/asn1/der/main.c) · 多一个字节都算结构非法

```c
/* Done 断言恰好消费完：多一个字节都算结构非法。 */
!xrtDerDone(&Items) ) {
```

### `xrtDerRemaining`

返回游标尚未消费的字节数；非法游标返回零并设置参数错误。

```c
size_t xrtDerRemaining(const xdercursor* pCursor);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入 | 非空 | 待查询的游标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | `Size - Offset`，尚未消费的字节数 | — |
| `0` | 游标非法（空指针/偏移越界） | `XERR_ARGUMENT`（非法时设置） |

#### 错误

- `XERR_ARGUMENT` — `pCursor` 为空、数据为空而长度非零、或偏移大于长度

#### 范例

[asn1/decode_tour · 窥视](../../examples/asn1/decode_tour/main.c) · Peek 不消费剩余量

```c
(void)xrtDerPeek(&Cursor, &Value);
printf("peek-tag=%u remaining-after-peek=%zu",
	(unsigned)Value.Tag.Number, xrtDerRemaining(&Cursor));
```

### `xrtDerIs`

仅判断值的标签，不修改错误状态。

```c
bool xrtDerIs(
	const xdervalue* pValue,
	xasn1class Class,
	uint32 iNumber,
	bool bConstructed
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 允许空 | 待判定的值视图 |
| `Class` | 输入 | — | 期望类别 |
| `iNumber` | 输入 | — | 期望标签号 |
| `bConstructed` | 输入 | — | 期望构造位 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 三项全部匹配 | — |
| `false` | 任一不匹配或 `pValue` 为空 | 纯判定，不设置错误 |

#### 错误

- 无 — CHOICE/OPTIONAL 分支判定用，否定不是错误

#### 范例

[asn1/decode_tour · 判定](../../examples/asn1/decode_tour/main.c) · 与 Peek 配合分流

```c
printf(" is-bool=%d\n",
	xrtDerIs(&Value, XASN1_UNIVERSAL, (uint32)XASN1_BOOLEAN, false) ? 1 : 0);
```

## 严格 DER

每次读取都会拒绝：

- BER 无限长度和截断内容；
- 非最短、前导零或溢出的长度；
- 非最短、截断或超过 32 位的高标签号；
- primitive/constructed 形式错误的常用 Universal 类型；
- 非规范 BOOLEAN、INTEGER、ENUMERATED、BIT STRING、NULL 和 OID。

游标解析适合受协议结构约束的高性能路径；完整验证适合接收证书、密钥和其他不可信独立 DER 文档的入口。

### `xrtDerValidate`

验证输入恰好包含一个完整、规范且最大嵌套 64 层的 DER 值。

```c
bool xrtDerValidate(const void* pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用、非空 | 完整 DER 文档 |
| `iSize` | 输入 | `> 0` | 文档字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 整棵构造树规范：单一顶层值、SET 按完整编码排序、嵌套 ≤ 64 层 | — |
| `false` | 参数非法或发现违规 | 错误经 `xrtGetError()` 报告，`Data` 含 `offset=` |

#### 错误

- `XERR_ARGUMENT` — `pData` 为空或 `iSize` 为零
- `XERR_PROTOCOL` + `XASN1_ERROR_TRAILING` — 顶层值多于一个（尾随字节）
- `XERR_PROTOCOL` + `XASN1_ERROR_ORDER` — SET 成员未按 DER 字节字典序排列
- `XERR_RANGE` + `XASN1_ERROR_DEPTH` — 嵌套超过 64 层
- 其余同游标解析（`TAG`/`LENGTH`/`VALUE` 族）

#### 范例

[asn1/der · 解析链](../../examples/asn1/der/main.c) · 入口先整体校验一遍

```c
if ( !xrtDerValidate(Document, sizeof(Document)) ||
	!xrtDerInit(&Root, Document, sizeof(Document)) ||
```

## 类型辅助层

这些函数把 `xdervalue` 转换为具体类型的值或借用视图，全部要求 primitive Universal 类型且内容规范。`xrtDerUnsigned` 拒绝负整数并去除正数为避免符号歧义而使用的单个前导零；`xrtDerUInt64`/`xrtDerInt64` 额外检查范围；`xrtDerBitString` 单独返回未使用位数。

### `xrtDerBoolean`

读取规范 DER BOOLEAN。

```c
bool xrtDerBoolean(const xdervalue* pValue, bool* pResult);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 期望 `(UNIVERSAL, BOOLEAN, primitive)` |
| `pResult` | 输出 | 非空 | 接收 `0xFF → true`、`0x00 → false` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `*pResult` 已发布 | — |
| `false` | 参数非法、类型不符或值非规范 | `*pResult` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pValue`/`pResult` 为空
- `XERR_TYPE` + `XASN1_ERROR_TYPE` — 值不是 BOOLEAN primitive
- `XERR_PROTOCOL` + `XASN1_ERROR_VALUE` — 内容不是单字节 `00` 或 `FF`

#### 范例

[asn1/decode_tour · 读取器族](../../examples/asn1/decode_tour/main.c) · Read 后按类型转换

```c
if ( xrtDerRead(&Cursor, &Value) == XDER_VALUE ) {
	(void)xrtDerBoolean(&Value, &bBool);
	printf("bool=%s", bBool ? "true" : "false");
}
```

### `xrtDerUnsigned`

读取非负 DER INTEGER，并返回去除可选符号零后的借用字节。

```c
bool xrtDerUnsigned(const xdervalue* pValue, xbytesview* pResult);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 期望 `(UNIVERSAL, INTEGER, primitive)` |
| `pResult` | 输出 | 非空 | 接收大端字节视图；正数的前导符号零已去除 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `*pResult` 已发布（借用原输入） | — |
| `false` | 参数非法、类型不符或整数为负 | `*pResult` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pValue`/`pResult` 为空
- `XERR_TYPE` + `XASN1_ERROR_TYPE` — 值不是 INTEGER primitive
- `XERR_VALUE` + `XASN1_ERROR_VALUE` — 首字节符号位为 1（负整数）

#### 范例

[asn1/decode_tour · 读取器族](../../examples/asn1/decode_tour/main.c) · 任意精度大整数取原始字节

```c
if ( xrtDerRead(&Cursor, &Value) == XDER_VALUE ) {
	(void)xrtDerInt64(&Value, &iInt);
	printf(" int=%lld", (long long)iInt);
	(void)xrtDerUnsigned(&Value, &Bytes);
	printf(" unsigned-len=%zu", Bytes.Size);
}
```

### `xrtDerUInt64`

读取不超过 64 位的非负 DER INTEGER。

```c
bool xrtDerUInt64(const xdervalue* pValue, uint64* pResult);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 期望非负 INTEGER primitive |
| `pResult` | 输出 | 非空 | 接收大端转换后的值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `*pResult` 已发布 | — |
| `false` | 参数非法、类型不符、为负或超出 64 位 | `*pResult` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtDerUnsigned`（`ARGUMENT`/`TYPE`/负值 `XERR_VALUE`）
- `XERR_RANGE` + `XASN1_ERROR_RANGE` — 去符号零后长度超过 8 字节

#### 范例

[asn1/der · 读取链](../../examples/asn1/der/main.c) · 两个 INTEGER 求和

```c
(xrtDerRead(&Items, &Value) != XDER_VALUE) ||
!xrtDerUInt64(&Value, &iLeft) ||
(xrtDerRead(&Items, &Value) != XDER_VALUE) ||
!xrtDerUInt64(&Value, &iRight) ||
```

### `xrtDerInt64`

读取不超过 64 位的有符号 DER INTEGER。

```c
bool xrtDerInt64(const xdervalue* pValue, int64* pResult);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 期望 `(UNIVERSAL, INTEGER, primitive)` |
| `pResult` | 输出 | 非空 | 接收补码解释后的值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `*pResult` 已发布 | — |
| `false` | 参数非法、类型不符或超出 64 位 | `*pResult` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pValue`/`pResult` 为空
- `XERR_TYPE` + `XASN1_ERROR_TYPE` — 值不是 INTEGER primitive
- `XERR_RANGE` + `XASN1_ERROR_RANGE` — 内容超过 8 字节

#### 范例

[asn1/decode_tour · 读取器族](../../examples/asn1/decode_tour/main.c) · 有符号转换

```c
if ( xrtDerRead(&Cursor, &Value) == XDER_VALUE ) {
	(void)xrtDerInt64(&Value, &iInt);
	printf(" int=%lld", (long long)iInt);
```

### `xrtDerBitString`

读取 DER BIT STRING，返回数据和末字节未使用位数。

```c
bool xrtDerBitString(
	const xdervalue* pValue,
	xbytesview* pResult,
	uint8* pUnusedBits
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 期望 `(UNIVERSAL, BIT STRING, primitive)` |
| `pResult` | 输出 | 非空 | 接收数据字节视图（不含未用位数首字节） |
| `pUnusedBits` | 输出 | 非空 | 接收末字节未使用的低位数（0–7） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 两个输出已发布 | — |
| `false` | 参数非法、类型不符或值非规范 | 输出不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空
- `XERR_TYPE` + `XASN1_ERROR_TYPE` — 值不是 BIT STRING primitive
- `XERR_PROTOCOL` + `XASN1_ERROR_VALUE` — 缺少未用位数字节、计数大于 7、或未用位非零

#### 范例

[asn1/decode_tour · 读取器族](../../examples/asn1/decode_tour/main.c) · 数据与未用位数一并取出

```c
if ( xrtDerRead(&Cursor, &Value) == XDER_VALUE ) {
	(void)xrtDerBitString(&Value, &Bytes, &iUnused);
	printf(" bitstring=%02X unused=%u\n",
		Bytes.Size ? (unsigned)Bytes.Data[0] : 0u, (unsigned)iUnused);
}
```

### `xrtDerOctets`

读取 DER OCTET STRING 的借用内容。

```c
bool xrtDerOctets(const xdervalue* pValue, xbytesview* pResult);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 期望 `(UNIVERSAL, OCTET STRING, primitive)` |
| `pResult` | 输出 | 非空 | 接收内容字节视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `*pResult` 已发布（借用原输入） | — |
| `false` | 参数非法或类型不符 | `*pResult` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pValue`/`pResult` 为空
- `XERR_TYPE` + `XASN1_ERROR_TYPE` — 值不是 OCTET STRING primitive

#### 范例

[asn1/decode_tour · 读取器族](../../examples/asn1/decode_tour/main.c) · 零拷贝取内容

```c
if ( xrtDerRead(&Cursor, &Value) == XDER_VALUE ) {
	(void)xrtDerOctets(&Value, &Bytes);
	printf(" octets=%.*s", (int)Bytes.Size, (const char*)Bytes.Data);
}
```

### `xrtDerOid`

读取规范 OBJECT IDENTIFIER 的借用内容。

```c
bool xrtDerOid(const xdervalue* pValue, xbytesview* pResult);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 非空 | 期望 `(UNIVERSAL, OID, primitive)` |
| `pResult` | 输出 | 非空 | 接收内容八位组（base-128 子标识符序列） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `*pResult` 已发布（借用原输入） | — |
| `false` | 参数非法、类型不符或内容非规范 | `*pResult` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pValue`/`pResult` 为空
- `XERR_TYPE` + `XASN1_ERROR_TYPE` — 值不是 OID primitive
- `XERR_PROTOCOL` + `XASN1_ERROR_VALUE` — 内容为空、截断或子标识符非最短

#### 范例

[asn1/encode_tour · OID 工具](../../examples/asn1/encode_tour/main.c) · 游标遍历中识别 OID

```c
if ( xrtDerOid(&Value, &OidContent) ) {
	printf("der-oid-content=%zu\n", OidContent.Size);
```

### `xrtDerOidEqual`

比较 DER OBJECT IDENTIFIER 与调用方提供的内容八位组。

```c
bool xrtDerOidEqual(
	const xdervalue* pValue,
	const void* pOid,
	size_t iOidSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pValue` | 输入 | 允许空 | 待比较的值视图 |
| `pOid` | 输入 | 借用 | 期望的内容八位组；`iOidSize == 0` 时允许为空 |
| `iOidSize` | 输入 | — | 期望内容字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 值是 OID primitive 且内容逐字节相等 | — |
| `false` | 类型不符、长度不等或内容不同 | 纯比较，不设置错误 |

#### 错误

- 无 — 不匹配是比较结果而非错误；证书策略匹配的热路径用它

#### 范例

[asn1/encode_tour · OID 工具](../../examples/asn1/encode_tour/main.c) · 与编码产物比对

```c
printf(" equal=%d",
	xrtDerOidEqual(
		&(xdervalue){ 0 },
		OidBytes, iOidSize) ? 0 : 0); /* 结构体零值仅演示签名 */
```

### `xasn1class`

ASN.1 标签类别使用 X.690 的两位稳定值。

```c
typedef enum xasn1class {
	XASN1_UNIVERSAL = 0,
	XASN1_APPLICATION,
	XASN1_CONTEXT,
	XASN1_PRIVATE
} xasn1class;
```

| 值 | 语义 |
|---|---|
| `XASN1_UNIVERSAL` | universal 类 |
| `XASN1_APPLICATION` | application 类 |
| `XASN1_CONTEXT` | context 类 |
| `XASN1_PRIVATE` | （见枚举语义） |

### `xasn1universal`

X.509、PKCS 与 TLS 常用的 ASN.1 Universal 标签号。

```c
typedef enum xasn1universal {
	XASN1_BOOLEAN = 1,
	XASN1_INTEGER = 2,
	XASN1_BIT_STRING = 3,
	XASN1_OCTET_STRING = 4,
	XASN1_NULL = 5,
	XASN1_OBJECT_IDENTIFIER = 6,
	XASN1_ENUMERATED = 10,
	XASN1_UTF8_STRING = 12,
	XASN1_RELATIVE_OID = 13,
	XASN1_SEQUENCE = 16,
	XASN1_SET = 17,
	XASN1_NUMERIC_STRING = 18,
	XASN1_PRINTABLE_STRING = 19,
	XASN1_TELETEX_STRING = 20,
	XASN1_IA5_STRING = 22,
	XASN1_UTC_TIME = 23,
	XASN1_GENERALIZED_TIME = 24,
	XASN1_VISIBLE_STRING = 26,
	XASN1_GENERAL_STRING = 27,
	XASN1_UNIVERSAL_STRING = 28,
	XASN1_BMP_STRING = 30
} xasn1universal;
```

| 值 | 语义 |
|---|---|
| `XASN1_BOOLEAN` | BOOLEAN |
| `XASN1_INTEGER` | INTEGER |
| `XASN1_BIT_STRING` | BIT字符串 |
| `XASN1_OCTET_STRING` | OCTET字符串 |
| `XASN1_NULL` | 空值 |
| `XASN1_OBJECT_IDENTIFIER` | 对象形态IDENTIFIER |
| `XASN1_ENUMERATED` | ENUMERATED |
| `XASN1_UTF8_STRING` | UTF-8字符串 |
| `XASN1_RELATIVE_OID` | RELATIVEOID |
| `XASN1_SEQUENCE` | SEQUENCE |
| `XASN1_SET` | 集合形态 |
| `XASN1_NUMERIC_STRING` | NUMERIC字符串 |
| `XASN1_PRINTABLE_STRING` | PRINTABLE字符串 |
| `XASN1_TELETEX_STRING` | TELETEX字符串 |
| `XASN1_IA5_STRING` | IA5字符串 |
| `XASN1_UTC_TIME` | UTC时间 |
| `XASN1_GENERALIZED_TIME` | GENERALIZED时间 |
| `XASN1_VISIBLE_STRING` | VISIBLE字符串 |
| `XASN1_GENERAL_STRING` | GENERAL字符串 |
| `XASN1_UNIVERSAL_STRING` | UNIVERSAL字符串 |
| `XASN1_BMP_STRING` | BMPString（UTF-16） |

### `xasn1tag`

DER 标签保留类别、构造位和完整的高标签号。

```c
typedef struct xasn1tag {
	xasn1class Class;
	uint32 Number;
	bool Constructed;
} xasn1tag;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Class` | `xasn1class` | Class |
| `Number` | `uint32` | Number |
| `Constructed` | `bool` | Constructed |

### `xdervalue`

DER 值中的所有视图都借用原输入，不分配也不复制。

```c
typedef struct xdervalue {
	xasn1tag Tag;
	xbytesview Raw;
	xbytesview Value;
	size_t HeaderSize;
} xdervalue;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Tag` | `xasn1tag` | 标签名 |
| `Raw` | `xbytesview` | Raw |
| `Value` | `xbytesview` | 值 |
| `HeaderSize` | `size_t` | HeaderSize |

### `xdercursor`

DER 游标保存不可变输入和下一项偏移，可安全复制后独立遍历。

```c
typedef struct xdercursor {
	cbytes Data;
	size_t Size;
	size_t Offset;
} xdercursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `cbytes` | 数据 |
| `Size` | `size_t` | 字节数 |
| `Offset` | `size_t` | 偏移量 |

### `xderresult`

Read/Peek 把正常结束与协议错误分开表达。

```c
typedef enum xderresult {
	XDER_ERROR = -1,
	XDER_DONE = 0,
	XDER_VALUE = 1
} xderresult;
```

| 值 | 语义 |
|---|---|
| `XDER_ERROR` | 失败 |
| `XDER_DONE` | 完成 |
| `XDER_VALUE` | 已产出值 |

### `xasn1error`

ASN.1/DER 模块稳定错误码。

```c
typedef enum xasn1error {
	XASN1_ERROR_TAG = 1,
	XASN1_ERROR_LENGTH,
	XASN1_ERROR_VALUE,
	XASN1_ERROR_TYPE,
	XASN1_ERROR_END,
	XASN1_ERROR_TRAILING,
	XASN1_ERROR_ORDER,
	XASN1_ERROR_DEPTH,
	XASN1_ERROR_RANGE
} xasn1error;
```

| 值 | 语义 |
|---|---|
| `XASN1_ERROR_TAG` | 失败 |
| `XASN1_ERROR_LENGTH` | 失败 |
| `XASN1_ERROR_VALUE` | 值非法 |
| `XASN1_ERROR_TYPE` | 类型 |
| `XASN1_ERROR_END` | 失败 |
| `XASN1_ERROR_TRAILING` | 失败 |
| `XASN1_ERROR_ORDER` | 失败 |
| `XASN1_ERROR_DEPTH` | 深度超限 |
| `XASN1_ERROR_RANGE` | （见枚举语义） |

### `xbuffer`

DER 写入接口只需要缓冲的不透明指针。

```c
typedef struct xbuffer xbuffer;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

## 编码器

编码器族向 `xbuffer` 尾部追加规范 DER：失败时不修改对外可见长度，不发布半个 TLV。`Content` 一律借用——追加的是编码后的字节，不是调用方对象；构造类型（SEQUENCE/SET/显式标签）的子项由调用方预先编码后作为 `Content` 传入。

### `xrtDerAppend`

向缓冲尾部追加一个 DER TLV。Content 是借用的原始内容；对于构造类型，调用方负责提供已经规范编码的子项。

```c
bool xrtDerAppend(
	xbuffer* pOutput,
	xasn1class Class,
	uint32 iNumber,
	bool bConstructed,
	xbytesview Content
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOutput` | 输入/输出 | 非空 | 目标缓冲；成功后长度增加完整 TLV |
| `Class` | 输入 | 枚举范围 | 标签类别 |
| `iNumber` | 输入 | — | 标签号；`>= 31` 自动使用高标签号形式 |
| `bConstructed` | 输入 | — | 构造位 |
| `Content` | 输入 | 借用 | 原始内容字节；构造类型传已编码子项 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 完整 TLV 已追加（含最短长度形式） | — |
| `false` | 参数非法或缓冲扩展失败 | 对外可见长度不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pOutput` 为空、`Class` 超出枚举范围、或 `Content.Data` 为空而 `Size` 非零
- 内存分配失败 — 缓冲扩展失败（由缓冲层设置）

#### 范例

[asn1/encode_tour · 任意 TLV](../../examples/asn1/encode_tour/main.c) · 空 SEQUENCE 构造

```c
(void)xrtDerAppend(&Buf, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE,
	true, (xbytesview){ NULL, 0u });
```

### `xrtDerAppendBoolean`

追加规范 DER BOOLEAN（`true → FF`、`false → 00`）。

```c
bool xrtDerAppendBoolean(xbuffer* pOutput, bool bValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOutput` | 输入/输出 | 非空 | 目标缓冲 |
| `bValue` | 输入 | — | 布尔值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 3 字节 TLV 已追加 | — |
| `false` | 参数非法或缓冲扩展失败 | 长度不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pOutput` 为空
- 内存分配失败 — 缓冲扩展失败

#### 范例

[asn1/encode_tour · 编码器族](../../examples/asn1/encode_tour/main.c) · TRUE 追加 3 字节

```c
(void)xrtDerAppendBoolean(&Buf, true);
```

### `xrtDerAppendUInt64`

追加非负 DER INTEGER（最短补码形式，必要时补符号零）。

```c
bool xrtDerAppendUInt64(xbuffer* pOutput, uint64 iValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOutput` | 输入/输出 | 非空 | 目标缓冲 |
| `iValue` | 输入 | — | 非负整数值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 最短 INTEGER TLV 已追加 | — |
| `false` | 参数非法或缓冲扩展失败 | 长度不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pOutput` 为空
- 内存分配失败 — 缓冲扩展失败

#### 范例

[asn1/encode_tour · 编码器族](../../examples/asn1/encode_tour/main.c) · 超过 32 位需 5 字节值

```c
(void)xrtDerAppendInt64(&Buf, 12345);
(void)xrtDerAppendUInt64(&Buf, UINT64_C(4294967296));
```

### `xrtDerAppendInt64`

追加有符号 DER INTEGER（最短补码形式）。

```c
bool xrtDerAppendInt64(xbuffer* pOutput, int64 iValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOutput` | 输入/输出 | 非空 | 目标缓冲 |
| `iValue` | 输入 | — | 有符号整数值，负数合法 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 最短 INTEGER TLV 已追加 | — |
| `false` | 参数非法或缓冲扩展失败 | 长度不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pOutput` 为空
- 内存分配失败 — 缓冲扩展失败

#### 范例

[asn1/encode_tour · 编码器族](../../examples/asn1/encode_tour/main.c) · 与无符号形态并列

```c
(void)xrtDerAppendInt64(&Buf, 12345);
(void)xrtDerAppendUInt64(&Buf, UINT64_C(4294967296));
```

### `xrtDerAppendOctets`

追加 DER OCTET STRING。

```c
bool xrtDerAppendOctets(xbuffer* pOutput, xbytesview Content);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOutput` | 输入/输出 | 非空 | 目标缓冲 |
| `Content` | 输入 | 借用 | 内容字节；零长度合法 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | TLV 已追加 | — |
| `false` | 参数非法或缓冲扩展失败 | 长度不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pOutput` 为空、或 `Content.Data` 为空而 `Size` 非零
- 内存分配失败 — 缓冲扩展失败

#### 范例

[asn1/encode_tour · 编码器族](../../examples/asn1/encode_tour/main.c) · 字面量内容

```c
(void)xrtDerAppendOctets(&Buf, XRT_BYTES_LITERAL("hello"));
```

### `xrtDerAppendNull`

追加 DER NULL（2 字节：tag + 零长度）。

```c
bool xrtDerAppendNull(xbuffer* pOutput);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOutput` | 输入/输出 | 非空 | 目标缓冲 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 2 字节 TLV 已追加 | — |
| `false` | 参数非法或缓冲扩展失败 | 长度不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pOutput` 为空
- 内存分配失败 — 缓冲扩展失败

#### 范例

[asn1/encode_tour · 编码器族](../../examples/asn1/encode_tour/main.c) · 长度核对用

```c
(void)xrtDerAppendNull(&Buf);
```

### `xrtDerAppendBitString`

追加 DER BIT STRING，含未用位数首字节。

```c
bool xrtDerAppendBitString(
	xbuffer* pOutput,
	xbytesview Content,
	uint8 iUnusedBits
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOutput` | 输入/输出 | 非空 | 目标缓冲 |
| `Content` | 输入 | 借用 | 数据字节；零长度时未用位数必须为零 |
| `iUnusedBits` | 输入 | `<= 7` | 末字节未使用的低位数，且这些位必须为零 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | TLV 已追加（首字节为未用位数） | — |
| `false` | 参数非法或缓冲扩展失败 | 长度不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `iUnusedBits > 7`、空内容配非零未用位、未用位实际非零、或指针非法
- 内存分配失败 — 缓冲扩展失败

#### 范例

[asn1/encode_tour · 编码器族](../../examples/asn1/encode_tour/main.c) · `0xA0` 高 4 位有效

```c
(void)xrtDerAppendBitString(&Buf, XRT_BYTES_LITERAL("\xA0"), 4u);
```

### `xrtDerAppendOid`

把点分文本 OID 编码为内容八位组并追加为 OBJECT IDENTIFIER TLV。

```c
bool xrtDerAppendOid(xbuffer* pOutput, xstrview Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOutput` | 输入/输出 | 非空 | 目标缓冲 |
| `Text` | 输入 | 借用 | 形如 `"2.5.4.3"` 的点分文本，至少两段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | OID TLV 已追加 | — |
| `false` | 文本非法或缓冲扩展失败 | 长度不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pOutput` 为空或文本不是合法点分 OID（非数字、前导零、首段 > 2、首段 < 2 时次段 > 39）
- `XERR_RANGE` — 某一段超过 `uint64` 可表示范围
- 内存分配失败 — 内部编码缓冲或输出扩展失败

#### 范例

[asn1/encode_tour · 编码器族](../../examples/asn1/encode_tour/main.c) · CN 的 OID

```c
(void)xrtDerAppendOid(&Buf, XRT_STR_LITERAL("2.5.4.3"));
```

### `xrtDerOidEncode`

把点分文本 OID 转换为内容八位组（base-128 子标识符），追加到 Output 尾部；失败时不修改 Output。

```c
bool xrtDerOidEncode(xstrview Text, xbuffer* pOutput);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 点分文本，至少两段；首两段按 `40 * first + second` 合并 |
| `pOutput` | 输入/输出 | 非空 | 接收内容八位组（不含 TLV 头） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 内容八位组已追加 | — |
| `false` | 文本非法或缓冲扩展失败 | Output 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pOutput` 为空或文本非法（同 `xrtDerAppendOid`）
- `XERR_RANGE` — 某一段超过 `uint64`
- 内存分配失败 — 缓冲扩展失败

#### 范例

[asn1/encode_tour · OID 工具](../../examples/asn1/encode_tour/main.c) · 只取内容八位组

```c
if ( !xrtDerOidEncode(XRT_STR_LITERAL("2.5.4.3"), &OidBuf) ) {
	return 1;
}
```

### `xrtDerOidDecode`

把内容八位组解码为点分文本，追加到 Output 尾部；失败时不修改 Output。

```c
bool xrtDerOidDecode(xbytesview Oid, xbuffer* pOutput);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Oid` | 输入 | 借用 | base-128 子标识符序列（`xrtDerOid` 的产物） |
| `pOutput` | 输入/输出 | 非空 | 接收形如 `"2.5.4.3"` 的文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 点分文本已追加（首两段由 `40 * first + second` 拆开） | — |
| `false` | 内容非法或缓冲扩展失败 | Output 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pOutput` 为空、内容为空/截断、或子标识符以 `0x80` 开头（非最短）
- `XERR_RANGE` — 子标识符超过 `uint64`
- 内存分配失败 — 缓冲扩展失败

#### 范例

[asn1/encode_tour · OID 工具](../../examples/asn1/encode_tour/main.c) · 空内容被拒（不打印）

```c
if ( xrtDerOidDecode((xbytesview){ Buf.Data, 0u }, &OidBuf) ) {
	printf(" decode=ok");
}
```

## 错误

DER 错误使用 `xrt.asn1` 域和 `XASN1_ERROR_*` 稳定代码。错误对象的 `Data` 在适用时包含 `offset=<字节偏移>`，便于上层错误映射和 C 日志定位输入。类型、协议、范围与参数错误分别使用统一的 `XERR_TYPE`、`XERR_PROTOCOL`、`XERR_RANGE` 和 `XERR_ARGUMENT` 类别。

| 代码 | 含义 |
|---|---|
| `XASN1_ERROR_TAG` | 标签非法：保留标签号、高标签号非最短/截断/超 32 位、primitive/constructed 形式错误 |
| `XASN1_ERROR_LENGTH` | 长度非法：BER 无限长度、截断、前导零、非最短长形式、内容越过输入 |
| `XASN1_ERROR_VALUE` | 值非规范：BOOLEAN 非 00/FF、INTEGER 非最短、BIT STRING 未用位非法、NULL 非空、OID 非最短 |
| `XASN1_ERROR_TYPE` | 类型辅助层/Expect 的标签不匹配 |
| `XASN1_ERROR_END` | 输入在标签前截断，或 Expect 期望的值缺失 |
| `XASN1_ERROR_TRAILING` | Validate 发现顶层值之后还有字节 |
| `XASN1_ERROR_ORDER` | Validate 发现 SET 成员未按 DER 编码排序 |
| `XASN1_ERROR_DEPTH` | Validate 发现嵌套超过 64 层 |
| `XASN1_ERROR_RANGE` | 数值超出目标范围（标签号、长度、64 位整数、OID 分量） |

## 示例与测试

- `examples/asn1/der/main.c`
- `tests/asn1/test_der.c`
- `tests/single/test_single_asn1_der.c`

测试覆盖高标签号、长长度、嵌套、SET 排序、尾随值、64 层深度门槛、常用 Universal 类型、失败原子性，以及旧解析器未拒绝的 BER 和非规范值。
