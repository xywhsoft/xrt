# Unicode 与字符集 API

## 设计契约

XRT 的内部文本主线是 UTF-8。`unicode` 模块只处理 Unicode 标量值和 UTF-8/16/32 编码形式；`charset` 模块处理带明确字节序的编码方案与 BOM；`charset_detect` 提供不承诺绝对正确的启发式检测。三层可以独立裁剪：

- `XRT_FEATURE_UNICODE`：标量原语、严格校验、流式 UTF-8 校验、缓冲区转换和分配型转换。
- `XRT_FEATURE_UNICODE_TEXT`：调用方缓冲区与分配型 UTF-8 文本变换，依赖 `unicode`。
- `XRT_FEATURE_UNICODE_DISTANCE`：按 Unicode 标量计算编辑距离与相似度，依赖 `unicode`。
- `XRT_FEATURE_CHARSET`：UTF-8、UTF-16 LE/BE、UTF-32 LE/BE 字节流、BOM 和通用转码，依赖 `unicode`。
- `XRT_FEATURE_CHARSET_DETECT`：带置信度的编码猜测，依赖 `charset`。

模块遵循以下不变式：

1. UTF-8 只接受 RFC 3629 的 1 至 4 字节形式；过长形式、代理项、5/6 字节形式和大于 U+10FFFF 的值无效。
2. UTF-16 只接受配对代理项；UTF-32 码元必须直接是 Unicode 标量值。
3. `XUTF_STRICT` 在首个错误处停止并设置 `xrt.unicode` 结构化错误。
4. `XUTF_REPLACE` 按 Unicode “最大子部件”规则写入 U+FFFD，保证继续前进，同时不吞掉后面的合法序列。
5. 明确长度视图允许嵌入 U+0000；零结尾便捷函数在第一个零码元处结束。
6. 所有分配型函数，包括空结果，都返回由调用方使用 `xrtFree` 释放的独立内存。
7. BOM 只在调用方明确要求时写入；通用转码不会隐式删除输入中的 U+FEFF。

“字节”“码元”“标量”不能互换：UTF-8 视图的 `Size` 是字节数，UTF-16/32 视图的 `Size` 是码元数，`xrtUtf8Count` 和 `xrtUtf16Count` 返回 Unicode 标量数。用户可见的字素簇数量不属于本模块。

## 类型

### `xbytesview` 与 `xstrview`

两个类型由 core 提供，分别借用任意字节和文本字节。它们不拥有数据，也不要求末尾补零：

```c
typedef struct xbytesview {
	cbytes Data;
	size_t Size;
} xbytesview;

typedef struct xstrview {
	cstr Data;
	size_t Size;
} xstrview;
```

`XRT_BYTES_LITERAL` 和 `XRT_STR_LITERAL` 在编译期创建不含末尾零字节的视图。

### `xutf16view` 与 `xutf32view`

`xutf16view.Size` 是 16 位码元数，`xutf32view.Size` 是 32 位码元数。`xrtUtf16View` 和 `xrtUtf32View` 只构造借用视图，不校验内容、不分配内存。

### `xutfpolicy`

- `XUTF_STRICT`：任何不完整或非法输入都失败。
- `XUTF_REPLACE`：每个最大子部件替换为一个 U+FFFD。

协议字段、标识符、源代码和安全边界通常应使用严格模式。展示来源不可靠的普通文本时可以明确选择替换模式。

### `xutfstatus` 与 `xutfresult`

`xutfstatus` 包含：

- `XUTF_OK`：输入已成功处理。
- `XUTF_MORE`：单标量或流式输入还缺少后续码元。
- `XUTF_INVALID`：输入或参数无效。
- `XUTF_NO_SPACE`：调用方目标缓冲区不足。
- `XUTF_OVERFLOW`：结果长度无法由 `size_t` 表示。

缓冲区转换返回：

```c
typedef struct xutfresult {
	xutfstatus Status;
	size_t Read;
	size_t Written;
	size_t Error;
} xutfresult;
```

`Read` 和 `Written` 使用各自视图的码元单位。`Error` 是源视图中的首个错误位置，成功时为 `XRT_NPOS`。目标空间不足时，`Read` 停在尚未写入的完整标量前，因此调用方可以更换缓冲区后继续。

### `xencoding`

编码方案包含 `XENCODING_UTF8`、`XENCODING_UTF16_LE`、`XENCODING_UTF16_BE`、`XENCODING_UTF32_LE`、`XENCODING_UTF32_BE` 和无法判断时使用的 `XENCODING_UNKNOWN`。

这里没有含义随平台改变的 “OEM” 编码。Windows 代码页、GBK、Shift-JIS 等传统编码属于可选的平台/外部编解码边界，不能在非 Windows 平台静默等价为 UTF-8。

### `xutf16view`

UTF-16 视图的 Size 表示 16 位码元数。

```c
typedef struct xutf16view {
	const uint16* Data;
	size_t Size;
} xutf16view;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `const uint16*` | 数据 |
| `Size` | `size_t` | 字节数 |

### `xutf32view`

UTF-32 视图的 Size 表示 32 位码元数。

```c
typedef struct xutf32view {
	const uint32* Data;
	size_t Size;
} xutf32view;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `const uint32*` | 数据 |
| `Size` | `size_t` | 字节数 |

### `xutfstatus`

UTF 原语和转换缓冲区共同使用的状态。

```c
typedef enum xutfstatus {
	XUTF_OK = 0,
	XUTF_MORE,
	XUTF_INVALID,
	XUTF_NO_SPACE,
	XUTF_OVERFLOW
} xutfstatus;
```

| 值 | 语义 |
|---|---|
| `XUTF_OK` | 成功 |
| `XUTF_MORE` | 需要更多输入 |
| `XUTF_INVALID` | 无效 |
| `XUTF_NO_SPACE` | NOSPACE |

### `xutfresult`

转换结果明确区分读取量、写入量和首个错误位置。

```c
typedef struct xutfresult {
	xutfstatus Status;
	size_t Read;
	size_t Written;
	size_t Error;
} xutfresult;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Status` | `xutfstatus` | 状态输出 |
| `Read` | `size_t` | Read |
| `Written` | `size_t` | Written |
| `Error` | `size_t` | 错误输出 |

### `xutf8state`

流式 UTF-8 校验器最多保留一个未完成标量的前缀。

```c
typedef struct xutf8state {
	unsigned char Pending[4];
	size_t Total;
	size_t PendingOffset;
	size_t Error;
	uint8 PendingSize;
	bool Failed;
} xutf8state;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Total` | `size_t` | 总量 |
| `PendingOffset` | `size_t` | PendingOffset |
| `Error` | `size_t` | 错误输出 |
| `PendingSize` | `uint8` | PendingSize |
| `Failed` | `bool` | Failed |

### `xutferror`

Unicode 模块的稳定错误代码。

```c
typedef enum xutferror {
	XUTF_ERROR_INVALID = 1,
	XUTF_ERROR_OVERFLOW
} xutferror;
```

| 值 | 语义 |
|---|---|
| `XUTF_ERROR_INVALID` | XUTF失败无效 |

### `xencodingguess`

检测结果明确表达猜测强度，零表示没有可靠结论。

```c
typedef struct xencodingguess {
	xencoding Encoding;
	size_t BomSize;
	uint8 Confidence;
} xencodingguess;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Encoding` | `xencoding` | Encoding |
| `BomSize` | `size_t` | BomSize |
| `Confidence` | `uint8` | Confidence |

## 视图与宽字符串

### `xrtUtf16View`

从明确码元数创建 UTF-16 借用视图；不校验内容、不分配内存。

```c
xutf16view xrtUtf16View(const uint16* pText, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pText` | 输入 | 借用 | 码元数组；`iSize == 0` 时允许为空 |
| `iSize` | 输入 | — | 码元数（不是字节数） |

#### 返回值

| 返回 | 含义 |
|---|---|
| 视图 | 纯构造，不失败；视图借用输入，存活期由调用方保证 |

#### 范例

[charset/unicode · 严格往返](../../examples/charset/unicode/main.c) · 与 DupView 组装平台边界缓冲

```c
pUtf16Copy = xrtUtf16DupView(xrtUtf16View(pUtf16, iUnits));
```

### `xrtUtf32View`

从明确码元数创建 UTF-32 借用视图。

```c
xutf32view xrtUtf32View(const uint32* pText, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pText` | 输入 | 借用 | 码元数组 |
| `iSize` | 输入 | — | 码元数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 视图 | 纯构造，不失败 |

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 视图族统一入口形态

```c
(void)xrtUtf32View(A32, 2u);
printf(" u32len=%zu", xrtUtf32Len(A32));
```

### `xrtUtf16Len`

返回零结尾 UTF-16 字符串在第一个零码元前的码元数。空指针返回零。

```c
size_t xrtUtf16Len(const uint16* pText);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pText` | 输入 | 零结尾 | 宽字符串；空指针返回 0 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `>= 0` | 首个零码元前的码元数；需要保留嵌入零必须用明确长度视图 |

#### 错误

- 无 — 纯长度查询，空指针是合法的空串

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 零结尾形态的长度

```c
printf("u16len=%zu", xrtUtf16Len(A16));
```

### `xrtUtf32Len`

返回零结尾 UTF-32 字符串在第一个零码元前的码元数。空指针返回零。

```c
size_t xrtUtf32Len(const uint32* pText);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pText` | 输入 | 零结尾 | 宽字符串；空指针返回 0 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `>= 0` | 首个零码元前的码元数 |

#### 错误

- 无 — 纯长度查询

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 与视图形态对照

```c
(void)xrtUtf32View(A32, 2u);
printf(" u32len=%zu", xrtUtf32Len(A32));
```

### `xrtUtf16Dup`

复制零结尾 UTF-16 字符串并返回独立的零结尾内存。空指针按空字符串处理。

```c
uint16* xrtUtf16Dup(const uint16* pText);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pText` | 输入 | 零结尾 | 要复制的宽字符串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 独立零结尾副本，`xrtFree` 释放（含末尾零码元） | — |
| `NULL` | 内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 零结尾复制三件套之一

```c
uint16* pDup = xrtUtf16Dup(A16);
uint32* pDup32 = xrtUtf32Dup(A32);
uint32* pDup32V = xrtUtf32DupView((xutf32view){ A32, 2u });
```

### `xrtUtf32Dup`

复制零结尾 UTF-32 字符串并返回独立的零结尾内存。

```c
uint32* xrtUtf32Dup(const uint32* pText);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pText` | 输入 | 零结尾 | 要复制的宽字符串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾副本，`xrtFree` 释放 | — |
| `NULL` | 内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 与 16 位形态并列

```c
uint16* pDup = xrtUtf16Dup(A16);
uint32* pDup32 = xrtUtf32Dup(A32);
```

### `xrtUtf16DupView`

复制明确长度 UTF-16 视图并追加一个零码元；只复制码元，不校验 Unicode 合法性。

```c
uint16* xrtUtf16DupView(xutf16view Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 要复制的视图；嵌入零原样保留 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立副本 + 零码元结尾，`xrtFree` 释放 | — |
| `NULL` | 溢出或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` + `XUTF_ERROR_OVERFLOW` — 大小计算溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[charset/unicode · 严格往返](../../examples/charset/unicode/main.c) · 平台 API 边界的独立缓冲

```c
pUtf16Copy = xrtUtf16DupView(xrtUtf16View(pUtf16, iUnits));
if ( pUtf16Copy == NULL ) {
	xrtFree(pUtf16);
	return 1;
}
```

### `xrtUtf32DupView`

复制明确长度 UTF-32 视图并追加一个零码元。

```c
uint32* xrtUtf32DupView(xutf32view Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 要复制的视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立副本 + 零码元结尾 | — |
| `NULL` | 溢出或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` + `XUTF_ERROR_OVERFLOW` — 大小计算溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 明确长度复制

```c
uint32* pDup32V = xrtUtf32DupView((xutf32view){ A32, 2u });
```

## 标量原语

### `xrtUnicodeScalar`

判断数值是否在 U+0000 至 U+10FFFF 之间且不属于代理项区间 U+D800 至 U+DFFF。非字符码点仍是合法标量。

```c
bool xrtUnicodeScalar(uint32 iScalar);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iScalar` | 输入 | — | 待判定的数值 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 可编码的 Unicode 标量 |
| `false` | 代理项或超出 U+10FFFF；纯判定不设置错误 |

#### 错误

- 无 — 编码合法性判定，否定不是错误

#### 范例

[charset/transcode_tour · 编解码往返](../../examples/charset/transcode_tour/main.c) · 编码前先验证标量

```c
printf(" scalar=%d\n", xrtUnicodeScalar(0x4F60) ? 1 : 0);
```

### `xrtUtf8Decode`

从视图开头严格解码一个标量。成功返回 `XUTF_OK`；合法前缀被截断返回 `XUTF_MORE`；非法形式返回 `XUTF_INVALID`。

```c
xutfstatus xrtUtf8Decode(xstrview Text, uint32* pScalar, size_t* pRead);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 至少包含一个标量的起始 |
| `pScalar` | 输出 | 非空 | 接收解码标量；失败时不变 |
| `pRead` | 输出 | 可空 | 成功时为消费字节数；`INVALID` 时为容错应消费的最大子部件长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XUTF_OK` | `*pScalar` 与 `*pRead` 已发布 | — |
| `XUTF_MORE` | 合法前缀截断 | 不设置执行上下文错误 |
| `XUTF_INVALID` | 非法形式或参数无效 | `XERR_VALUE` + `xrt.unicode` 结构化错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `XERR_VALUE` + `XUTF_ERROR_INVALID` — 首字节即为非法序列（`Data` 含 offset）

#### 范例

[charset/transcode_tour · 编解码往返](../../examples/charset/transcode_tour/main.c) · 解码"你"再编码

```c
(void)xrtUtf8Decode(SV("你"), &iScalar, &iRead);
printf("decode=U+%X", iScalar);
iWrote = xrtUtf8Encode(iScalar, Out);
```

### `xrtUtf16Decode`

从 UTF-16 视图开头严格解码一个标量；代理项必须成对。

```c
xutfstatus xrtUtf16Decode(xutf16view Text, uint32* pScalar, size_t* pRead);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | UTF-16 码元视图 |
| `pScalar` | 输出 | 非空 | 接收解码标量 |
| `pRead` | 输出 | 可空 | 消费码元数（1 或 2） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XUTF_OK` | 已解码 | — |
| `XUTF_MORE` | 高代理项后缺少低代理项 | 不设置错误 |
| `XUTF_INVALID` | 孤立代理项或非法序列 | `XERR_VALUE` + `xrt.unicode` |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `XERR_VALUE` + `XUTF_ERROR_INVALID` — 非法码元序列

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 编码后再解码核对

```c
size_t iWrote = xrtUtf16Encode(0x4F60, Out);

printf("encode=%zu", iWrote);
(void)xrtUtf16Decode((xutf16view){ Out, iWrote }, &iScalar, &iRead);
printf(" decode=U+%X\n", iScalar);
```

### `xrtUtf8Encode`

把单个标量写入调用方缓冲区，返回写入字节数。缓冲区至少 4 字节。

```c
size_t xrtUtf8Encode(uint32 iScalar, char arrOutput[4]);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iScalar` | 输入 | 合法标量 | 要编码的数值 |
| `arrOutput` | 输出 | 非空、至少 4 字节 | 接收 UTF-8 字节序列 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 1–4 | 实际写入字节数 | — |
| `0` | 标量无效或目标为空 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 目标为空
- `XERR_VALUE` + `XUTF_ERROR_INVALID` — `iScalar` 不是合法标量

#### 范例

[charset/transcode_tour · 编解码往返](../../examples/charset/transcode_tour/main.c) · 单标量往返验证

```c
iWrote = xrtUtf8Encode(iScalar, Out);
printf(" encode=%zu bytes", iWrote);
```

### `xrtUtf16Encode`

把单个标量写入调用方缓冲区，返回写入码元数。缓冲区至少 2 个码元。

```c
size_t xrtUtf16Encode(uint32 iScalar, uint16 arrOutput[2]);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iScalar` | 输入 | 合法标量 | 要编码的数值 |
| `arrOutput` | 输出 | 非空、至少 2 码元 | 接收 UTF-16 码元（BMP 为 1，增补平面为一对代理项） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 1–2 | 实际写入码元数 | — |
| `0` | 标量无效或目标为空 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 目标为空
- `XERR_VALUE` — 标量非法

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · U+4F60 编码 1 码元

```c
size_t iWrote = xrtUtf16Encode(0x4F60, Out);
```

## 校验与计数

### `xrtUtf8Valid`

严格校验完整 UTF-8 视图，不分配内存。内容无效不设置执行上下文错误，便于探测输入。

```c
bool xrtUtf8Valid(xstrview Text, size_t* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 待校验视图 |
| `pError` | 输出 | 可空 | 接收首个错误字节位置；成功时为 `XRT_NPOS` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 整个视图是严格合法 UTF-8 | — |
| `false` | 内容非法（`*pError` 给出位置）或参数非法 | 内容无效不设错误；参数无效设 `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 参数组合无效（如 `pError` 与输入别名）

#### 范例

[charset/transcode_tour · 一次性校验](../../examples/charset/transcode_tour/main.c) · 失败给首错字节位置

```c
printf("valid=%d", xrtUtf8Valid(SV("你好"), &iError) ? 1 : 0);
```

### `xrtUtf16Valid`

严格校验完整 UTF-16 视图。

```c
bool xrtUtf16Valid(xutf16view Text, size_t* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 待校验视图 |
| `pError` | 输出 | 可空 | 首个错误码元位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 合法（代理项全部成对） | — |
| `false` | 内容非法或参数非法 | 内容无效不设错误 |

#### 错误

- `XERR_ARGUMENT` — 参数组合无效

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 转换产物立即校验

```c
printf(" valid=%d", xrtUtf16Valid((xutf16view){ p16, iSize },
	NULL) ? 1 : 0);
```

### `xrtUtf32Valid`

严格校验完整 UTF-32 视图；码元必须直接是标量。

```c
bool xrtUtf32Valid(xutf32view Text, size_t* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 待校验视图 |
| `pError` | 输出 | 可空 | 首个错误码元位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 全部码元都是合法标量 | — |
| `false` | 有代理项或越界值，或参数非法 | 内容无效不设错误 |

#### 错误

- `XERR_ARGUMENT` — 参数组合无效

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 视图级校验

```c
printf(" u32valid=%d", xrtUtf32Valid((xutf32view){ A32, 2u }, NULL) ? 1 : 0);
```

### `xrtUtf8Count`

返回视图中的 Unicode 标量数；不折叠组合字符或 emoji 序列。

```c
size_t xrtUtf8Count(xstrview Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 待计数视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 标量数 | — |
| `XRT_NPOS` | 输入非法 UTF-8 | `XERR_VALUE` + 带字节位置的 `xrt.unicode` 错误 |

#### 错误

- `XERR_VALUE` + `XUTF_ERROR_INVALID` — 遇到非法序列

#### 范例

[charset/utf8_search · 计数与换算](../../examples/charset/utf8_search/main.c) · "a你x你b" 共 5 个标量

```c
printf("count=%zu", xrtUtf8Count(Text));
printf(" offset(1)=%zu", xrtUtf8Offset(Text, 1u));
```

### `xrtUtf16Count`

返回 UTF-16 视图中的 Unicode 标量数。

```c
size_t xrtUtf16Count(xutf16view Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 待计数视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 标量数（代理对算一个） | — |
| `XRT_NPOS` | 输入非法 | `XERR_VALUE` + `xrt.unicode` 错误 |

#### 错误

- `XERR_VALUE` + `XUTF_ERROR_INVALID` — 孤立代理项等

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 码元数与标量数对照

```c
printf(" count=%zu\n", xrtUtf16Count((xutf16view){ p16, iSize }));
```

### `xrtUtf8Offset`

把标量索引转换为字节偏移；索引可以正好位于末端。

```c
size_t xrtUtf8Offset(xstrview Text, size_t iIndex);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `iIndex` | 输入 | `<= 标量数` | 标量索引；等于标量数返回字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 对应字节偏移 | — |
| `XRT_NPOS` | 索引越界或输入非法 | `XERR_RANGE`（越界）或 `XERR_VALUE`（非法 UTF-8） |

#### 错误

- `XERR_RANGE` — `iIndex` 超过标量数
- `XERR_VALUE` + `XUTF_ERROR_INVALID` — 前缀含非法序列

#### 范例

[charset/utf8_search · 计数与换算](../../examples/charset/utf8_search/main.c) · 标量 1 的字节偏移

```c
printf(" offset(1)=%zu", xrtUtf8Offset(Text, 1u));
```

### `xrtUtf8Index`

把字节偏移转换为标量索引；偏移必须位于标量边界。

```c
size_t xrtUtf8Index(xstrview Text, size_t iOffset);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `iOffset` | 输入 | 标量边界 | 字节偏移 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 对应标量索引 | — |
| `XRT_NPOS` | 落在多字节序列中间、越界或输入非法 | `XERR_RANGE` 或 `XERR_VALUE` |

#### 错误

- `XERR_RANGE` — 偏移越过末尾或落在标量中间
- `XERR_VALUE` + `XUTF_ERROR_INVALID` — 前缀非法

#### 范例

[charset/utf8_search · 计数与换算](../../examples/charset/utf8_search/main.c) · 字节偏移 4 → 标量索引 2

```c
printf("case-rfind=%zu", xrtUtf8CaseRFind(Text, SV("B")));
printf(" index(4)=%zu\n", xrtUtf8Index(Text, 4u));
```

### `xrtUtf8At`

读取指定标量索引的标量值。

```c
bool xrtUtf8At(xstrview Text, size_t iIndex, uint32* pScalar);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `iIndex` | 输入 | `< 标量数` | 标量索引 |
| `pScalar` | 输出 | 非空 | 接收标量值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `*pScalar` 已发布 | — |
| `false` | 索引在末端无可读标量、输入非法或参数无效 | `XERR_RANGE` / `XERR_VALUE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_RANGE` — `iIndex >= 标量数`
- `XERR_VALUE` — 前缀非法
- `XERR_ARGUMENT` — 指针为空

#### 范例

[charset/utf8_search · 计数与换算](../../examples/charset/utf8_search/main.c) · 取标量 1

```c
(void)xrtUtf8At(Text, 1u, &iScalar);
printf(" at(1)=U+%X\n", iScalar);
```

### `xrtUtf8Slice`

按标量索引返回借用切片；起点与末端钳制到输入末尾。

```c
bool xrtUtf8Slice(xstrview Text, size_t iStart, size_t iCount, xstrview* pSlice);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `iStart` | 输入 | 钳制到末尾 | 起点标量索引 |
| `iCount` | 输入 | — | 标量数；`XRT_NPOS` 表示到末尾 |
| `pSlice` | 输出 | 非空 | 接收借用视图；不分配、不补零 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `*pSlice` 已发布（空结果也是成功） | — |
| `false` | 参数无效或遍历前缀非法 | `XERR_ARGUMENT` / `XERR_VALUE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `XERR_VALUE` — 前缀含非法序列

#### 范例

[charset/unicode · 严格往返](../../examples/charset/unicode/main.c) · 从标量 4 起取 2 个，绝不切开多字节

```c
if ( !xrtUtf8Slice(Text, 4, 2, &Word) ) {
	xrtFree(sUtf8);
	xrtFree(pUtf16Copy);
	xrtFree(pUtf16);
	return 1;
}
```

## 文本区间

`unicode_text` 接口承接语言运行时常用的带符号索引语义：`iStart < 0` 从末尾按标量计数，`iCount < 0` 表示一直到末尾；超出两端的起点会钳制到边界，包括 `INT64_MIN`。空结果仍是成功结果。

### `xrtUtf8Range`

按 Unicode 标量解析带负索引的范围并返回借用视图。

```c
bool xrtUtf8Range(xstrview Text, int64 iStart, int64 iCount,
	xstrview* pRange);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `iStart` | 输入 | 可负 | 负值从末尾按标量计数；越界钳制到边界 |
| `iCount` | 输入 | 可负 | 负值表示一直到末尾 |
| `pRange` | 输出 | 非空 | 接收借用视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `*pRange` 已发布（空区间也是成功） | — |
| `false` | 参数无效或文本非法 | `XERR_ARGUMENT` / `XERR_VALUE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `XERR_VALUE` — 文本含非法序列

#### 范例

[charset/unicode_text · 区间](../../examples/charset/unicode_text/main.c) · -2 起取 1 个 → "😀"

```c
if ( !xrtUtf8Range(XRT_STR_LITERAL("A你😀B"), -2, 1, &Range) ) {
	return 3;
}
printf("%.*s\n", (int)Range.Size, Range.Data);
```

### `xrtUtf8Substr`

按 Unicode 标量复制带负索引的范围，返回独立零结尾字符串。

```c
str xrtUtf8Substr(xstrview Text, int64 iStart, int64 iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `iStart` | 输入 | 可负 | 负值从末尾计数，越界钳制 |
| `iCount` | 输入 | 可负 | 负值表示到末尾 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾副本，`xrtFree` 释放 | — |
| `NULL` | 文本非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` — 文本非法
- `XERR_MEMORY` — 分配失败

#### 范例

[charset/utf8_edit · 编辑族](../../examples/charset/utf8_edit/main.c) · 从标量 1 起取 2 个

```c
show("substr", xrtUtf8Substr(SV("a你x"), 1, 2));
```

## 搜索

四个搜索函数只接受严格 UTF-8，返回 Unicode 标量索引而不是字节偏移。未找到返回 `XRT_NPOS`，这是正常结果，不设置错误。`Case` 版本只折叠 ASCII 字母，行为确定且不依赖区域设置。

### `xrtUtf8Find`

按标量索引正向查找子串。

```c
size_t xrtUtf8Find(xstrview Text, xstrview Part, size_t iStart);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `Part` | 输入 | 借用 | 模式；空模式返回起点 |
| `iStart` | 输入 | 标量索引 | 搜索起点 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 首个命中的标量索引 | — |
| `XRT_NPOS` | 未找到（正常结果）或输入非法 | 未找到不设错；非法输入设 `XERR_VALUE` |

#### 错误

- `XERR_VALUE` — 文本或模式非法
- `XERR_RANGE` — `iStart` 超过文本标量数

#### 范例

[charset/utf8_search · 搜索族](../../examples/charset/utf8_search/main.c) · 返回标量下标 1

```c
printf("find=%zu", xrtUtf8Find(Text, SV("你"), 0u));
printf(" rfind=%zu", xrtUtf8RFind(Text, SV("你")));
```

### `xrtUtf8CaseFind`

按 ASCII 大小写不敏感规则正向查找。

```c
size_t xrtUtf8CaseFind(xstrview Text, xstrview Part, size_t iStart);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `Part` | 输入 | 借用 | 模式；只折叠 ASCII 字母 |
| `iStart` | 输入 | 标量索引 | 搜索起点 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 首个命中的标量索引 | — |
| `XRT_NPOS` | 未找到或输入非法 | 同 `xrtUtf8Find` |

#### 错误

- 同 `xrtUtf8Find`（`XERR_VALUE` / `XERR_RANGE`）

#### 范例

[charset/utf8_search · 搜索族](../../examples/charset/utf8_search/main.c) · 小写 x 命中大写 X

```c
printf(" case-find=%zu\n", xrtUtf8CaseFind(Text, SV("X"), 0u));
```

### `xrtUtf8RFind`

从右侧查找子串并返回标量索引。

```c
size_t xrtUtf8RFind(xstrview Text, xstrview Part);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `Part` | 输入 | 借用 | 模式；空模式返回文本标量数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 最后一个命中的标量索引 | — |
| `XRT_NPOS` | 未找到或输入非法 | 未找到不设错 |

#### 错误

- `XERR_VALUE` — 输入非法

#### 范例

[charset/utf8_search · 搜索族](../../examples/charset/utf8_search/main.c) · 第二个"你"在下标 3

```c
printf(" rfind=%zu", xrtUtf8RFind(Text, SV("你")));
```

### `xrtUtf8CaseRFind`

按 ASCII 大小写不敏感规则从右侧查找。

```c
size_t xrtUtf8CaseRFind(xstrview Text, xstrview Part);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `Part` | 输入 | 借用 | 模式 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 最后一个命中的标量索引 | — |
| `XRT_NPOS` | 未找到或输入非法 | 未找到不设错 |

#### 错误

- `XERR_VALUE` — 输入非法

#### 范例

[charset/utf8_search · 搜索族](../../examples/charset/utf8_search/main.c) · 大小写不敏感反向命中

```c
printf("case-rfind=%zu", xrtUtf8CaseRFind(Text, SV("B")));
```

### `xrtUtf8ContainsAny`

判断文本是否包含集合中的任意 Unicode 标量。

```c
bool xrtUtf8ContainsAny(xstrview Text, xstrview Set);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `Set` | 输入 | 借用 | 标量集合；严格 UTF-8 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 文本包含集合中至少一个标量 | — |
| `false` | 不包含（正常结果）或输入非法 | 不包含不设错；非法设 `XERR_VALUE` |

#### 错误

- `XERR_VALUE` — 文本或集合非法

#### 范例

[charset/utf8_search · 搜索族](../../examples/charset/utf8_search/main.c) · 标量粒度不会因共享字节误判

```c
printf("contains-any=%d\n",
	xrtUtf8ContainsAny(Text, SV("你x")) ? 1 : 0);
```

## 集合裁剪

三个裁剪函数删除两端属于指定标量集合的内容并返回借用视图，不分配内存，支持明确长度输入中的 U+0000。空集合保持原文本不变。这些接口不隐式定义“Unicode 空白”。

### `xrtUtf8TrimLeftSet`

删除左侧属于指定 Unicode 标量集合的内容并返回借用视图。

```c
bool xrtUtf8TrimLeftSet(xstrview Text, xstrview Set,
	xstrview* pResult);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `Set` | 输入 | 借用 | 要删除的标量集合 |
| `pResult` | 输出 | 非空 | 接收借用视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `*pResult` 已发布（可能为空视图） | — |
| `false` | 文本或集合非法 | `XERR_VALUE` |

#### 错误

- `XERR_VALUE` — 输入非法
- `XERR_ARGUMENT` — 指针为空

#### 范例

[charset/utf8_edit · 编辑族](../../examples/charset/utf8_edit/main.c) · 与双向、右侧形态并列

```c
(void)xrtUtf8TrimSet(SV(" 你x"), SV(" x"), &Trimmed);
printf("trim=[%.*s]\n", (int)Trimmed.Size, Trimmed.Data);
(void)xrtUtf8TrimLeftSet(SV(" 你x"), SV(" x"), &Trimmed);
(void)xrtUtf8TrimRightSet(SV(" 你x"), SV(" x"), &Trimmed);
```

### `xrtUtf8TrimRightSet`

删除右侧属于指定 Unicode 标量集合的内容并返回借用视图。

```c
bool xrtUtf8TrimRightSet(xstrview Text, xstrview Set,
	xstrview* pResult);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `Set` | 输入 | 借用 | 要删除的标量集合 |
| `pResult` | 输出 | 非空 | 接收借用视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `*pResult` 已发布 | — |
| `false` | 输入非法 | `XERR_VALUE` |

#### 错误

- `XERR_VALUE` / `XERR_ARGUMENT` — 同 `xrtUtf8TrimLeftSet`

#### 范例

[charset/utf8_edit · 编辑族](../../examples/charset/utf8_edit/main.c) · 三件套之一

```c
(void)xrtUtf8TrimLeftSet(SV(" 你x"), SV(" x"), &Trimmed);
(void)xrtUtf8TrimRightSet(SV(" 你x"), SV(" x"), &Trimmed);
```

### `xrtUtf8TrimSet`

删除两侧属于指定 Unicode 标量集合的内容并返回借用视图。

```c
bool xrtUtf8TrimSet(xstrview Text, xstrview Set,
	xstrview* pResult);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `Set` | 输入 | 借用 | 要删除的标量集合；空集合保持原文 |
| `pResult` | 输出 | 非空 | 接收借用视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `*pResult` 已发布 | — |
| `false` | 输入非法 | `XERR_VALUE` |

#### 错误

- `XERR_VALUE` / `XERR_ARGUMENT` — 同 `xrtUtf8TrimLeftSet`

#### 范例

[charset/utf8_edit · 编辑族](../../examples/charset/utf8_edit/main.c) · " 你x" 裁剪集合 " x" → "你"

```c
(void)xrtUtf8TrimSet(SV(" 你x"), SV(" x"), &Trimmed);
printf("trim=[%.*s]\n", (int)Trimmed.Size, Trimmed.Data);
```

## 编辑与填充

### `xrtUtf8Insert`

按 Unicode 标量位置插入严格 UTF-8 子串。

```c
str xrtUtf8Insert(xstrview Text, int64 iPosition, xstrview Part);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `iPosition` | 输入 | 可负 | 负值从末尾计数；越界钳制 |
| `Part` | 输入 | 借用 | 要插入的严格 UTF-8 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 结果的独立零结尾副本 | — |
| `NULL` | 输入非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` — 文本或子串非法
- `XERR_MEMORY` — 分配失败

#### 范例

[charset/utf8_edit · 编辑族](../../examples/charset/utf8_edit/main.c) · 标量位置 1 之后插入 X

```c
show("insert", xrtUtf8Insert(SV("a你x"), 1, SV("X")));
```

### `xrtUtf8Remove`

按 Unicode 标量范围删除内容，负数量表示一直删除到末尾。

```c
str xrtUtf8Remove(xstrview Text, int64 iStart, int64 iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `iStart` | 输入 | 可负 | 负值从末尾计数 |
| `iCount` | 输入 | 可负 | 负值表示到末尾 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 结果的独立零结尾副本 | — |
| `NULL` | 输入非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` — 文本非法
- `XERR_MEMORY` — 分配失败

#### 范例

[charset/utf8_edit · 编辑族](../../examples/charset/utf8_edit/main.c) · 从标量 1 起删 1 个

```c
show("remove", xrtUtf8Remove(SV("a你x"), 1, 1));
```

### `xrtUtf8PadLeft`

按 Unicode 标量宽度在左侧重复填充严格 UTF-8 文本；空填充使用 ASCII 空格。

```c
str xrtUtf8PadLeft(xstrview Text, size_t iWidth, xstrview Fill);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `iWidth` | 输入 | 标量数 | 目标宽度；不是终端列宽 |
| `Fill` | 输入 | 借用 | 填充模式；按标量循环，末次可在标量边界截断 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 填充结果的独立零结尾副本 | — |
| `NULL` | 输入非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` — 文本或填充非法
- `XERR_MEMORY` — 分配失败

#### 范例

[charset/utf8_edit · 编辑族](../../examples/charset/utf8_edit/main.c) · "*" 填充到宽 3

```c
show("pad", xrtUtf8PadLeft(SV("a"), 3u, SV("*")));
show("pad-r", xrtUtf8PadRight(SV("a"), 3u, SV("*")));
```

### `xrtUtf8PadRight`

按 Unicode 标量宽度在右侧重复填充。

```c
str xrtUtf8PadRight(xstrview Text, size_t iWidth, xstrview Fill);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `iWidth` | 输入 | 标量数 | 目标宽度 |
| `Fill` | 输入 | 借用 | 填充模式 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 填充结果的独立副本 | — |
| `NULL` | 输入非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtUtf8PadLeft`（`XERR_VALUE` / `XERR_MEMORY`）

#### 范例

[charset/utf8_edit · 编辑族](../../examples/charset/utf8_edit/main.c) · 右侧填充

```c
show("pad-r", xrtUtf8PadRight(SV("a"), 3u, SV("*")));
```

### `xrtUtf8PadCenter`

按 Unicode 标量宽度在两侧重复填充；奇数余量把多出的一个标量放在右侧。

```c
str xrtUtf8PadCenter(xstrview Text, size_t iWidth, xstrview Fill);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `iWidth` | 输入 | 标量数 | 目标宽度 |
| `Fill` | 输入 | 借用 | 填充模式；可为多标量串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 填充结果的独立副本 | — |
| `NULL` | 输入非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtUtf8PadLeft`

#### 范例

[charset/unicode_text · 居中填充](../../examples/charset/unicode_text/main.c) · "好😀" 模式填宽 7

```c
sPadded = xrtUtf8PadCenter(XRT_STR_LITERAL("XRT"), 7,
	XRT_STR_LITERAL("好😀"));
if ( sPadded == NULL ) {
	return 4;
}
printf("%s\n", sPadded);
```

## 反转与过滤

### `xrtUtf8ReverseTo`

按 Unicode 标量反转严格 UTF-8 文本到调用方缓冲区；支持输入与输出起点相同的原地路径，部分重叠被拒绝。容量必须包含末尾零。

```c
bool xrtUtf8ReverseTo(xstrview Text, char* sOutput, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `sOutput` | 输出 | 非空、独立或同起点 | 接收反转字节（不含零结尾） |
| `iCapacity` | 输入 | — | 缓冲容量；不足时不改动目标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写入反转结果 | — |
| `false` | 容量不足、重叠非法或输入非法 | 目标不变；`XERR_RANGE` / `XERR_ARGUMENT` / `XERR_VALUE` |

#### 错误

- `XERR_RANGE` — 容量不足
- `XERR_ARGUMENT` — 部分重叠
- `XERR_VALUE` — 输入非法

#### 范例

[charset/utf8_edit · 编辑族](../../examples/charset/utf8_edit/main.c) · 反转到调用方缓冲

```c
if ( xrtUtf8ReverseTo(SV("ab"), Buffer, sizeof(Buffer)) ) {
	Buffer[iSize] = 0;
	printf("reverse=%s\n", Buffer);
}
```

### `xrtUtf8Reverse`

按 Unicode 标量反转并返回独立零结尾字符串。不执行字素簇分段。

```c
str xrtUtf8Reverse(xstrview Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 反转结果的独立副本 | — |
| `NULL` | 输入非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` — 输入非法
- `XERR_MEMORY` — 分配失败

#### 范例

[charset/unicode_text · 反转](../../examples/charset/unicode_text/main.c) · 8 个标量整体倒序，emoji 完整

```c
sText = xrtUtf8Reverse(XRT_STR_LITERAL("XRT 你好 😀"));
if ( sText == NULL ) {
	return 1;
}
printf("%s\n", sText);
```

### `xrtUtf8FilterTo`

按 Unicode 标量集合过滤严格 UTF-8 文本到调用方缓冲区；支持长度查询与同起点原地过滤，容量不足或重叠非法时不改动目标。集合必须在写入期间独立于目标。

```c
bool xrtUtf8FilterTo(xstrview Text, xstrview Set,
	char* sOutput, size_t iCapacity, size_t* pOutputSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `Set` | 输入 | 借用 | 要删除的标量集合 |
| `sOutput` | 输出 | 可空 | 空指针 + 零容量 = 只查询所需长度 |
| `iCapacity` | 输入 | — | 缓冲容量 |
| `pOutputSize` | 输出 | 可空 | 接收结果字节数（不含零结尾） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已过滤写入（或长度已发布） | — |
| `false` | 容量不足、重叠非法或输入非法 | 目标不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — 容量不足
- `XERR_ARGUMENT` — 集合与目标缓冲重叠
- `XERR_VALUE` — 文本或集合非法

#### 范例

[charset/utf8_edit · 编辑族](../../examples/charset/utf8_edit/main.c) · 剔除"你"到缓冲

```c
if ( xrtUtf8FilterTo(Text, SV("你"), Buffer, sizeof(Buffer),
	&iSize) ) {
	Buffer[iSize] = 0;
	printf("filter=%s\n", Buffer);
}
```

### `xrtUtf8Filter`

按 Unicode 标量集合过滤并创建独立字符串。

```c
str xrtUtf8Filter(xstrview Text, xstrview Set);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 严格 UTF-8 |
| `Set` | 输入 | 借用 | 要删除的标量集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 过滤结果的独立零结尾副本 | — |
| `NULL` | 输入非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` — 输入非法
- `XERR_MEMORY` — 分配失败

#### 范例

[charset/unicode_text · 过滤](../../examples/charset/unicode_text/main.c) · 删除"，好"两个标量

```c
sFiltered = xrtUtf8Filter(XRT_STR_LITERAL("你好，XRT"),
	XRT_STR_LITERAL("，好"));
if ( sFiltered == NULL ) {
	return 2;
}
printf("%s\n", sFiltered);
```

## 编辑距离

### `xrtUtf8Distance`

按 Unicode 标量计算 Levenshtein 距离；`iLimit == XRT_NPOS` 计算精确距离，有限限制使用带状动态规划。

```c
size_t xrtUtf8Distance(xstrview Left, xstrview Right, size_t iLimit);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 严格 UTF-8 |
| `Right` | 输入 | 借用 | 严格 UTF-8 |
| `iLimit` | 输入 | — | 距离上限；`XRT_NPOS` 不限制 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 编辑距离（插入/删除/替换各计 1） | — |
| `XRT_NPOS` | 超过限制（正常阈值结果，不设错）或输入非法/内存不足 | 非法时设置 `XERR_VALUE` / `XERR_MEMORY` |

#### 错误

- `XERR_VALUE` — 输入非法
- `XERR_MEMORY` — 工作区分配失败

#### 范例

[string/distance · 编辑距离](../../examples/string/distance/main.c) · "网络客户端" vs "网络服务端" 距离 2

```c
size_t iDistance = xrtUtf8Distance(Left, Right, XRT_NPOS);
double fSimilarity = xrtUtf8Similarity(Left, Right);

if ( (iDistance == XRT_NPOS) || (fSimilarity < 0.0) ) {
	return 1;
}
```

### `xrtUtf8Similarity`

按 Unicode 标量返回 0.0 至 1.0 的相似度；两个空字符串为 1.0。

```c
double xrtUtf8Similarity(xstrview Left, xstrview Right);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 严格 UTF-8 |
| `Right` | 输入 | 借用 | 严格 UTF-8 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `0.0–1.0` | `1 - distance / max(left, right)` | — |
| `< 0.0` | 输入非法或内存不足 | 结构化错误保留 |

#### 错误

- `XERR_VALUE` — 输入非法
- `XERR_MEMORY` — 分配失败

#### 范例

[string/distance · 编辑距离](../../examples/string/distance/main.c) · 1 - 2/5 = 0.6

```c
double fSimilarity = xrtUtf8Similarity(Left, Right);

if ( (iDistance == XRT_NPOS) || (fSimilarity < 0.0) ) {
	return 1;
}
printf("distance=%llu similarity=%.3f\n",
	(unsigned long long)iDistance, fSimilarity);
```

## 流式 UTF-8 校验

`xrtUtf8StateFeed` 接受任意分块，最后一块把 `bFinal` 设为 `true`。分块末尾最多保留 3 个合法前缀字节，因此不需要每连接固定分配大缓冲区。返回 `XUTF_MORE` 表示当前分块结束在合法前缀中间，不是错误。状态失败后保持失败，重新使用前必须再次初始化。

### `xrtUtf8StateInit`

初始化流式校验状态。

```c
void xrtUtf8StateInit(xutf8state* pState);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输出 | 非空 | 状态对象；可嵌入连接结构复用 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯初始化，不失败 |

#### 范例

[charset/transcode_tour · 流式校验](../../examples/charset/transcode_tour/main.c) · 跨块汉字 2+1 分喂

```c
xrtUtf8StateInit(&State);
```

### `xrtUtf8StateFeed`

向状态喂入一块数据；最后一块置 `bFinal`。

```c
xutfstatus xrtUtf8StateFeed(xutf8state* pState, xstrview Text, bool bFinal);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输入/输出 | 非空 | 状态；失败后保持失败 |
| `Text` | 输入 | 借用 | 本块字节 |
| `bFinal` | 输入 | — | 末块标记；末块仍不完整返回 `XUTF_INVALID` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XUTF_OK` | 本块接受；末块后整流合法 | — |
| `XUTF_MORE` | 分块结束在合法前缀中间（非错误） | 不设置错误 |
| `XUTF_INVALID` | 非法序列或末块不完整 | `XERR_VALUE` + `xrt.unicode`；状态锁定失败 |

#### 错误

- `XERR_ARGUMENT` — 状态为空
- `XERR_VALUE` + `XUTF_ERROR_INVALID` — 非法序列

#### 范例

[charset/transcode_tour · 流式校验](../../examples/charset/transcode_tour/main.c) · "你"拆成 2+1 字节跨块

```c
(xrtUtf8StateFeed(&State, (xstrview){ A, 2u }, false) ==
XUTF_MORE &&
xrtUtf8StateFeed(&State, (xstrview){ B, 2u }, true) ==
XUTF_OK) ? "OK" : "FAIL");
```

### `xrtUtf8StateError`

返回从整个流开头计算的绝对错误字节位置；从未失败时为 `XRT_NPOS`。

```c
size_t xrtUtf8StateError(const xutf8state* pState);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输入 | 非空 | 状态对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 首个错误的绝对字节位置 | — |
| `XRT_NPOS` | 流从未失败 | 纯查询 |

#### 错误

- `XERR_ARGUMENT` — `pState` 为空

#### 范例

[charset/transcode_tour · 流式校验](../../examples/charset/transcode_tour/main.c) · 成功流的位置为 none

```c
size_t iErr = xrtUtf8StateError(&State);

if ( iErr == XRT_NPOS ) {
	printf(" error-pos=none\n");
}
```

## 缓冲区转换

六个函数覆盖所有 UTF 码元宽度方向。传入 `pTarget == NULL` 且 `iCapacity == 0` 时只校验并计算精确目标长度。传入目标时不写零终止码元。目标不足不会拆分一个标量；源与目标不得重叠，别名输入以 `XERR_ARGUMENT` 拒绝且不修改缓冲区。

### `xrtUtf8To16Buffer`

UTF-8 转 UTF-16；目标为空时只计算所需码元数。

```c
xutfresult xrtUtf8To16Buffer(xstrview Source, uint16* pTarget,
	size_t iCapacity, xutfpolicy Policy);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | 严格或按策略处理的 UTF-8 |
| `pTarget` | 输出 | 可空 | 目标码元缓冲；空 + 零容量 = 只计量 |
| `iCapacity` | 输入 | — | 目标码元容量 |
| `Policy` | 输入 | — | `XUTF_STRICT` 或 `XUTF_REPLACE` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Status == XUTF_OK` | 转换完成；`Read`/`Written` 为码元单位 | — |
| `Status == XUTF_NO_SPACE` | 目标不足；`Read` 停在完整标量前 | 不覆盖执行上下文错误 |
| `Status == XUTF_INVALID` | 严格模式遇错；`Error` 为源内首错位置 | `XERR_VALUE` + `xrt.unicode` |

#### 错误

- `XERR_ARGUMENT` — 参数非法或源目标重叠
- `XERR_VALUE` + `XUTF_ERROR_INVALID` — 严格模式首个错误（`Data` 含 offset）
- `XERR_RANGE` + `XUTF_ERROR_OVERFLOW` — 长度溢出

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 显式容量 + 策略，结果含读/写量

```c
xutfresult R = xrtUtf8To16Buffer(SV("a你"), A16, 8u, XUTF_REPLACE);
```

### `xrtUtf8To32Buffer`

UTF-8 转 UTF-32；目标为空时只计算所需码元数。错误契约同 `xrtUtf8To16Buffer`。

```c
xutfresult xrtUtf8To32Buffer(xstrview Source, uint32* pTarget,
	size_t iCapacity, xutfpolicy Policy);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | UTF-8 源 |
| `pTarget` | 输出 | 可空 | UTF-32 码元缓冲 |
| `iCapacity` | 输入 | — | 码元容量 |
| `Policy` | 输入 | — | 错误策略 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Status == XUTF_OK` | 转换完成 | — |
| `XUTF_NO_SPACE` / `XUTF_INVALID` | 同 `xrtUtf8To16Buffer` | 同左 |

#### 错误

- 同 `xrtUtf8To16Buffer`

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 32 位方向

```c
xutfresult R32 = xrtUtf8To32Buffer(SV("a你"), A32, 8u, XUTF_REPLACE);
```

### `xrtUtf16To8Buffer`

UTF-16 转 UTF-8；目标为空时只计算所需字节数。

```c
xutfresult xrtUtf16To8Buffer(xutf16view Source, char* pTarget,
	size_t iCapacity, xutfpolicy Policy);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | UTF-16 码元视图 |
| `pTarget` | 输出 | 可空 | 字节缓冲 |
| `iCapacity` | 输入 | — | 字节容量 |
| `Policy` | 输入 | — | 错误策略 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Status == XUTF_OK` | 转换完成；`Written` 为字节数 | — |
| 其余 | 同 `xrtUtf8To16Buffer` 的流控口径 | 同左 |

#### 错误

- 同 `xrtUtf8To16Buffer`

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 16→8 缓冲方向

```c
(void)xrtUtf16To8Buffer((xutf16view){ A16, 2u }, Back,
	sizeof(Back), XUTF_REPLACE);
```

### `xrtUtf16To32Buffer`

UTF-16 转 UTF-32；目标为空时只计算所需码元数。

```c
xutfresult xrtUtf16To32Buffer(xutf16view Source, uint32* pTarget,
	size_t iCapacity, xutfpolicy Policy);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | UTF-16 码元视图 |
| `pTarget` | 输出 | 可空 | UTF-32 码元缓冲 |
| `iCapacity` | 输入 | — | 码元容量 |
| `Policy` | 输入 | — | 错误策略 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Status == XUTF_OK` | 转换完成 | — |
| 其余 | 同缓冲族流控口径 | 同左 |

#### 错误

- 同 `xrtUtf8To16Buffer`

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 16→32 缓冲方向

```c
(void)xrtUtf16To32Buffer((xutf16view){ A16, 2u }, A32, 8u, XUTF_REPLACE);
```

### `xrtUtf32To8Buffer`

UTF-32 转 UTF-8；目标为空时只计算所需字节数。

```c
xutfresult xrtUtf32To8Buffer(xutf32view Source, char* pTarget,
	size_t iCapacity, xutfpolicy Policy);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | UTF-32 码元视图 |
| `pTarget` | 输出 | 可空 | 字节缓冲 |
| `iCapacity` | 输入 | — | 字节容量 |
| `Policy` | 输入 | — | 错误策略 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Status == XUTF_OK` | 转换完成 | — |
| 其余 | 同缓冲族流控口径 | 同左 |

#### 错误

- 同 `xrtUtf8To16Buffer`

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 32→8 缓冲方向

```c
(void)xrtUtf32To8Buffer((xutf32view){ A32, 2u }, Back,
	sizeof(Back), XUTF_REPLACE);
```

### `xrtUtf32To16Buffer`

UTF-32 转 UTF-16；目标为空时只计算所需码元数。

```c
xutfresult xrtUtf32To16Buffer(xutf32view Source, uint16* pTarget,
	size_t iCapacity, xutfpolicy Policy);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | UTF-32 码元视图 |
| `pTarget` | 输出 | 可空 | UTF-16 码元缓冲 |
| `iCapacity` | 输入 | — | 码元容量 |
| `Policy` | 输入 | — | 错误策略 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `Status == XUTF_OK` | 转换完成 | — |
| 其余 | 同缓冲族流控口径 | 同左 |

#### 错误

- 同 `xrtUtf8To16Buffer`

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 32→16 缓冲方向

```c
(void)xrtUtf32To16Buffer((xutf32view){ A32, 2u }, A16, 8u, XUTF_REPLACE);
```

## 分配型转换

### 零结尾便捷层

六个函数严格转换零结尾输入，最短名称保留给最常见的平台边界；空指针按空字符串处理，返回独立零结尾对象，`pSize` 返回目标码元数（不含末尾零）。

### `xrtUtf8To16`

严格转换零结尾 UTF-8 并分配零结尾 UTF-16 字符串。

```c
uint16* xrtUtf8To16(cstr sText, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sText` | 输入 | 零结尾 | UTF-8 源；空指针按空串处理 |
| `pSize` | 输出 | 可空 | 接收目标码元数（不含零码元） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾 UTF-16 数组 | — |
| `NULL` | 输入非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` + `XUTF_ERROR_INVALID` — 非法序列
- `XERR_MEMORY` — 分配失败

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 平台 API 边界最常用方向

```c
uint16* p16 = xrtUtf8To16("a你", &iSize);

printf("u8→16: len=%zu", iSize);
```

### `xrtUtf8To32`

严格转换零结尾 UTF-8 并分配零结尾 UTF-32 数组。

```c
uint32* xrtUtf8To32(cstr sText, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sText` | 输入 | 零结尾 | UTF-8 源 |
| `pSize` | 输出 | 可空 | 接收目标码元数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾 UTF-32 数组 | — |
| `NULL` | 输入非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtUtf8To16`（`XERR_VALUE` / `XERR_MEMORY`）

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 32 位方向

```c
uint32* p32 = xrtUtf8To32("a你", &iSize);

printf("u8→32: len=%zu", iSize);
```

### `xrtUtf16To8`

严格转换零结尾 UTF-16 并分配 UTF-8 字符串。

```c
str xrtUtf16To8(const uint16* pText, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pText` | 输入 | 零结尾 | UTF-16 源 |
| `pSize` | 输出 | 可空 | 接收目标字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾 UTF-8 | — |
| `NULL` | 输入非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtUtf8To16`

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 回转 UTF-8

```c
str s8 = xrtUtf16To8(A16, &iSize);

printf("16→8: [%.*s]", (int)iSize, s8 ? s8 : "?");
```

### `xrtUtf16To32`

严格转换零结尾 UTF-16 并分配 UTF-32 数组。

```c
uint32* xrtUtf16To32(const uint16* pText, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pText` | 输入 | 零结尾 | UTF-16 源 |
| `pSize` | 输出 | 可空 | 接收目标码元数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾 UTF-32 | — |
| `NULL` | 输入非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtUtf8To16`

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 16→32 分配方向

```c
uint32* p32 = xrtUtf16To32(A16, &iSize);

printf(" 16→32: %zu\n", iSize);
```

### `xrtUtf32To8`

严格转换零结尾 UTF-32 并分配 UTF-8 字符串。

```c
str xrtUtf32To8(const uint32* pText, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pText` | 输入 | 零结尾 | UTF-32 源 |
| `pSize` | 输出 | 可空 | 接收目标字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾 UTF-8 | — |
| `NULL` | 输入非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtUtf8To16`

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 32→8 分配方向

```c
str s8 = xrtUtf32To8(A32, &iSize);

printf("32→8: [%.*s]", (int)iSize, s8 ? s8 : "?");
```

### `xrtUtf32To16`

严格转换零结尾 UTF-32 并分配 UTF-16 数组。

```c
uint16* xrtUtf32To16(const uint32* pText, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pText` | 输入 | 零结尾 | UTF-32 源 |
| `pSize` | 输出 | 可空 | 接收目标码元数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾 UTF-16 | — |
| `NULL` | 输入非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtUtf8To16`

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 32→16 分配方向

```c
uint16* p16 = xrtUtf32To16(A32, &iSize);

printf(" 32→16: %zu\n", iSize);
```

### 明确长度与策略层

六个视图版函数保留嵌入零并允许选择错误策略；返回值均由 `xrtFree` 释放，`pSize` 的单位由目标编码决定。

### `xrtUtf8ViewTo16`

严格转换零结尾 UTF-8，并分配零结尾 UTF-16 字符串（明确长度 + 策略）。

```c
uint16* xrtUtf8ViewTo16(xstrview Source, xutfpolicy Policy, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | 明确长度 UTF-8；嵌入零保留 |
| `Policy` | 输入 | — | 严格或替换 |
| `pSize` | 输出 | 可空 | 接收目标码元数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾 UTF-16（含末尾零码元） | — |
| `NULL` | 严格遇错或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` + `XUTF_ERROR_INVALID` — 严格模式非法序列
- `XERR_MEMORY` — 分配失败

#### 范例

[charset/unicode · 严格往返](../../examples/charset/unicode/main.c) · 严格模式任何非法序列都失败

```c
pUtf16 = xrtUtf8ViewTo16(Text, XUTF_STRICT, &iUnits);
if ( pUtf16 == NULL ) {
	return 1;
}
```

### `xrtUtf8ViewTo32`

转换明确长度 UTF-8，可选择严格失败或替换错误输入。

```c
uint32* xrtUtf8ViewTo32(xstrview Source, xutfpolicy Policy, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | 明确长度 UTF-8 |
| `Policy` | 输入 | — | 错误策略 |
| `pSize` | 输出 | 可空 | 接收目标码元数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾 UTF-32 | — |
| `NULL` | 严格遇错或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtUtf8ViewTo16`

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 替换策略视图版

```c
uint32* p32 = xrtUtf8ViewTo32(SV("a你"), XUTF_REPLACE, &iSize);

printf(" u8view-to32=%zu\n", iSize);
```

### `xrtUtf16ViewTo8`

转换明确长度 UTF-16，可选择错误策略。

```c
str xrtUtf16ViewTo8(xutf16view Source, xutfpolicy Policy, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | 明确长度 UTF-16 |
| `Policy` | 输入 | — | 错误策略 |
| `pSize` | 输出 | 可空 | 接收目标字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾 UTF-8 | — |
| `NULL` | 严格遇错或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtUtf8ViewTo16`

#### 范例

[charset/unicode · 严格往返](../../examples/charset/unicode/main.c) · 长度出参不需要时传 NULL

```c
sUtf8 = xrtUtf16ViewTo8(xrtUtf16View(pUtf16, iUnits), XUTF_STRICT, NULL);
if ( sUtf8 == NULL ) {
	xrtFree(pUtf16Copy);
	xrtFree(pUtf16);
	return 1;
}
```

### `xrtUtf16ViewTo32`

转换明确长度 UTF-16，可选择错误策略。

```c
uint32* xrtUtf16ViewTo32(xutf16view Source, xutfpolicy Policy, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | 明确长度 UTF-16 |
| `Policy` | 输入 | — | 错误策略 |
| `pSize` | 输出 | 可空 | 接收目标码元数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾 UTF-32 | — |
| `NULL` | 严格遇错或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtUtf8ViewTo16`

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 视图版 16→32

```c
uint32* p32 = xrtUtf16ViewTo32((xutf16view){ A16, 2u },
	XUTF_REPLACE, &iSize);
```

### `xrtUtf32ViewTo8`

转换明确长度 UTF-32，可选择错误策略。

```c
str xrtUtf32ViewTo8(xutf32view Source, xutfpolicy Policy, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | 明确长度 UTF-32 |
| `Policy` | 输入 | — | 错误策略 |
| `pSize` | 输出 | 可空 | 接收目标字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾 UTF-8 | — |
| `NULL` | 严格遇错或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtUtf8ViewTo16`

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 视图版 32→8

```c
str s8 = xrtUtf32ViewTo8((xutf32view){ A32, 2u },
	XUTF_REPLACE, &iSize);
```

### `xrtUtf32ViewTo16`

转换明确长度 UTF-32，可选择错误策略。

```c
uint16* xrtUtf32ViewTo16(xutf32view Source, xutfpolicy Policy, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | 明确长度 UTF-32 |
| `Policy` | 输入 | — | 错误策略 |
| `pSize` | 输出 | 可空 | 接收目标码元数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立零结尾 UTF-16 | — |
| `NULL` | 严格遇错或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtUtf8ViewTo16`

#### 范例

[charset/utf16_32 · 全族](../../examples/charset/utf16_32/main.c) · 视图版 32→16

```c
uint16* p16 = xrtUtf32ViewTo16((xutf32view){ A32, 2u },
	XUTF_REPLACE, &iSize);
```

## BOM 与通用转码

### `xrtEncodingUnitSize`

返回编码方案的码元字节宽度：UTF-8 为 1，UTF-16 为 2，UTF-32 为 4。

```c
size_t xrtEncodingUnitSize(xencoding Encoding);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Encoding` | 输入 | — | 编码方案 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 1 / 2 / 4 | 对应码元宽度 |
| `0` | `XENCODING_UNKNOWN` 或非法值 |

#### 错误

- 无 — 纯查表；未知编码返回 0

#### 范例

[charset/transcode_tour · BOM 三件套](../../examples/charset/transcode_tour/main.c) · UTF-16 单元 2 字节

```c
printf(" unit=%zu\n", xrtEncodingUnitSize(XENCODING_UTF16_LE));
```

### `xrtEncodingBom`

只检查输入开头的 BOM。先检查 4 字节 UTF-32 BOM，再检查 UTF-8 和 UTF-16，避免把 `FF FE 00 00` 错认成 UTF-16 LE。

```c
xencoding xrtEncodingBom(xbytesview Data, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | 借用 | 输入字节 |
| `pSize` | 输出 | 可空、独立 | 接收 BOM 字节数；不得与输入重叠 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 编码值 | 识别到的 BOM 对应编码；`*pSize` 为其字节数 | — |
| `XENCODING_UNKNOWN` | 无 BOM（`*pSize` 为零，正常结果） | 无 BOM 不设错；参数非法设 `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — `pSize` 与输入字节重叠

#### 范例

[charset/transcode_tour · BOM 三件套](../../examples/charset/transcode_tour/main.c) · 写出后识别回读

```c
size_t iBom = 0;
xencoding Enc = xrtEncodingBom((xbytesview){ Bom, iSize }, &iBom);

printf(" bom-encoding=%d", (int)Enc);
```

### `xrtEncodingWriteBom`

写出指定编码的 BOM；目标为空时返回所需字节数。

```c
size_t xrtEncodingWriteBom(xencoding Encoding, bytes pTarget, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Encoding` | 输入 | 已知编码 | 要写的 BOM 编码 |
| `pTarget` | 输出 | 可空 | 目标缓冲；空指针 = 只查询所需大小 |
| `iCapacity` | 输入 | — | 缓冲容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 所需或实际写出字节数 | — |
| `0` | 容量不足、编码未知或参数非法 | 目标不变；`XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_RANGE` — 目标容量不足
- `XERR_ARGUMENT` — 编码未知

#### 范例

[charset/transcode_tour · BOM 三件套](../../examples/charset/transcode_tour/main.c) · 写 UTF-8 BOM 后回读

```c
iSize = xrtEncodingWriteBom(XENCODING_UTF8, Bom, sizeof(Bom));
printf("bom-size=%zu", iSize);
```

### `xrtTranscode`

在五种 Unicode 编码方案之间直接转码，返回字节缓冲区和字节长度。只使用一条“解码标量 -> 编码标量”管线，不创建中间字符串。

```c
bytes xrtTranscode(xbytesview Source, xencoding SourceEncoding,
	xencoding TargetEncoding, xutfpolicy Policy, bool bWriteBom, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | 源字节 |
| `SourceEncoding` | 输入 | — | 源编码方案 |
| `TargetEncoding` | 输入 | — | 目标编码方案 |
| `Policy` | 输入 | — | 严格或替换 |
| `bWriteBom` | 输入 | — | 只控制目标前缀；返回长度含 BOM |
| `pSize` | 输出 | 可空、独立 | 接收产物字节数；不得与源重叠 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立字节缓冲（含 BOM 若请求），`xrtFree` 释放 | — |
| `NULL` | 参数非法、严格遇错或内存不足 | 源与 `*pSize` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 编码未知或 `pSize` 与源重叠
- `XERR_VALUE` + `XUTF_ERROR_INVALID` — 严格模式非法序列
- `XERR_RANGE` + `XUTF_ERROR_OVERFLOW` — 长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[charset/transcode · 带 BOM 封包](../../examples/charset/transcode/main.c) · UTF-8 → UTF-16 LE 带 BOM

```c
pUtf16 = xrtTranscode(Source, XENCODING_UTF8, XENCODING_UTF16_LE,
	XUTF_STRICT, true, &iSize);
if ( pUtf16 == NULL ) {
	return 1;
}
```

## 编码检测

`xrtEncodingGuess` 返回：

```c
typedef struct xencodingguess {
	xencoding Encoding;
	size_t BomSize;
	uint8 Confidence;
} xencodingguess;
```

规则按可靠性排序：完整 BOM 置信度 100；零字节分布明显的合法 UTF-16/32 中高置信度；合法非 ASCII UTF-8 置信度 90；纯 ASCII 返回 UTF-8 兼容结果但置信度仅 40；无可靠结论返回 `XENCODING_UNKNOWN`、置信度 0。检测永远只是猜测：协议声明、文件元数据和调用方显式配置优先于启发式结果。

### `xrtEncodingGuess`

对字节序列做编码猜测，一次返回编码、BOM 长度与置信度三项结论。

```c
xencodingguess xrtEncodingGuess(xbytesview Data);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | 借用 | 输入字节 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 结构体 | 按值返回，零分配；`BomSize` 可用于解码前跳过签名 | — |

#### 错误

- 无 — 启发式查询；无结论时 `Encoding == XENCODING_UNKNOWN`、`Confidence == 0`

#### 范例

[charset/detect · 编码探测](../../examples/charset/detect/main.c) · UTF-8 BOM 定性置信 100

```c
xencodingguess Guess = xrtEncodingGuess(
	(xbytesview){ arrInput, sizeof(arrInput) });

printf("encoding=%d bom=%llu confidence=%u\n", (int)Guess.Encoding,
	(unsigned long long)Guess.BomSize, (unsigned int)Guess.Confidence);
```

## 模块契约：线程

转换 API 为无共享状态的纯函数；同一输入可被多线程并发转换。转换器上下文对象（如启用）由创建线程独占使用。

## 错误

严格转换失败时，执行上下文错误具有：

- `Kind = XERR_VALUE`
- `Domain = "xrt.unicode"`
- `Code = XUTF_ERROR_INVALID`
- `Operation` 指明转换方向或 `transcode`
- `Data` 包含源视图中的 `offset`

长度溢出使用 `XERR_RANGE` 和 `XUTF_ERROR_OVERFLOW`。内存不足沿用 core 的 `XERR_MEMORY`。缓冲区容量不足由 `XUTF_NO_SPACE` 直接表达，不覆盖执行上下文错误。

## 完整示例

- `examples/charset/unicode/main.c`：严格 UTF-8/16 往返。
- `examples/charset/unicode_text/main.c`：UTF-8 标量反转与过滤。
- `examples/string/distance/main.c`：Unicode 标量编辑距离与相似度。
- `examples/charset/transcode/main.c`：直接生成带 BOM 的 UTF-16 LE 字节封包。
- `examples/charset/detect/main.c`：读取编码、BOM 长度和置信度。
- `examples/charset/utf16_32/main.c`：UTF-16/32 全族巡览。
- `examples/charset/utf8_edit/main.c` / `utf8_search/main.c`：UTF-8 编辑与搜索族。
- `examples/charset/transcode_tour/main.c`：流式校验、标量编解码与 BOM 三件套。

## 标准依据

编码合法性、替换策略和 BOM 语义以 Unicode Standard 的 Unicode Encoding Forms 与 U+FFFD 最大子部件建议为依据：

- <https://www.unicode.org/versions/latest/core-spec/chapter-3/>
- <https://www.unicode.org/versions/latest/core-spec/chapter-5/>
- <https://www.unicode.org/faq/utf_bom.html>
