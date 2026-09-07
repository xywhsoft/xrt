# Codec

Codec 提供 Base64、HEX 与 Percent 三种字节文本编码的缓冲版、流式与分配型入口；全部入口为无共享状态的纯函数。

## 类型与常量

### `xhexflag`

HEX 编码可选大写字母，解码可选忽略 ASCII 空白。

```c
typedef enum xhexflag {
	XHEX_UPPER = UINT32_C(0x00000001),
	XHEX_IGNORE_SPACE = UINT32_C(0x00000002)
} xhexflag;
```

| 值 | 语义 |
|---|---|
| `XHEX_UPPER` | XHEX大写 |
| `XHEX_IGNORE_SPACE` | 忽略空白 |

### `xbase64flag`

Base64 配置标志；默认使用标准字母表、规范填充并严格拒绝空白。

```c
typedef enum xbase64flag {
	XBASE64_URL = UINT32_C(0x00000001),
	XBASE64_NO_PADDING = UINT32_C(0x00000002),
	XBASE64_IGNORE_SPACE = UINT32_C(0x00000004),
	XBASE64_OPTIONAL_PADDING = UINT32_C(0x00000008)
} xbase64flag;
```

| 值 | 语义 |
|---|---|
| `XBASE64_URL` | URL |
| `XBASE64_NO_PADDING` | NOPADDING |
| `XBASE64_IGNORE_SPACE` | IGNORESPACE |
| `XBASE64_OPTIONAL_PADDING` | 允许省略填充 |

### `xbase64config`

自定义字母表必须是 64 个互不重复的可见 ASCII 字符；空指针表示使用内置字母表。

```c
typedef struct xbase64config {
	cstr Alphabet;
	uint32 Flags;
} xbase64config;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Alphabet` | `cstr` | 字母表 |
| `Flags` | `uint32` | 标志位 |

### `xcodecerror`

Codec 模块稳定错误码；各编码族使用独立编号区间。

```c
typedef enum xcodecerror {
	#if defined(XRT_FEATURE_CODEC_HEX)
	XCODEC_ERROR_HEX_CONFIG = 901,
	XCODEC_ERROR_HEX_FORMAT = 902,
	#endif

	#if defined(XRT_FEATURE_CODEC_BASE64)
	XCODEC_ERROR_BASE64_CONFIG = 1001,
	XCODEC_ERROR_BASE64_FORMAT = 1002,
	#endif

	#if defined(XRT_FEATURE_CODEC_PERCENT)
	XCODEC_ERROR_PERCENT_CONFIG = 1101,
	XCODEC_ERROR_PERCENT_FORMAT = 1102,
	#endif
} xcodecerror;
```

| 值 | 语义 |
|---|---|
| `XCODEC_ERROR_HEX_CONFIG` | HEX配置非法 |
| `XCODEC_ERROR_HEX_FORMAT` | 格式非法 |
| `XCODEC_ERROR_BASE64_CONFIG` | BASE64配置非法 |
| `XCODEC_ERROR_BASE64_FORMAT` | 格式非法 |
| `XCODEC_ERROR_PERCENT_CONFIG` | PERCENT配置非法 |
| `XCODEC_ERROR_PERCENT_FORMAT` | 格式非法 |

### `xpercentnext`

逐字节 percent 解码明确区分非法转义、输入结束和一个有效字节。

```c
typedef enum xpercentnext {
	XPERCENT_NEXT_ERROR = -1,
	XPERCENT_NEXT_END = 0,
	XPERCENT_NEXT_BYTE = 1
} xpercentnext;
```

| 值 | 语义 |
|---|---|
| `XPERCENT_NEXT_ERROR` | 失败 |
| `XPERCENT_NEXT_END` | END |
| `XPERCENT_NEXT_BYTE` | 已产出字节 |

### `xpercentmap`

预编译的 ASCII 安全字符集合。 该结构可按值复制，供大量字段编码时复用，避免反复构建字符位图。

```c
typedef struct xpercentmap {
	uint64 Bits[2];
} xpercentmap;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Bits` | `uint64[2]` | 256 位字符位图（每字节一位） |

## HEX Codec

`codec_hex` 是任意字节与十六进制文本之间的最小编码层，只依赖 `core`。启用：

```c
#define XRT_FEATURE_CODEC_HEX
```

基础 API `xrtHexEncode` / `xrtHexDecode` 采用统一的查询与写入契约：`output == NULL && capacity == 0` 只校验并返回所需长度；编码目标容量必须额外包含末尾零，解码目标是任意二进制，不要求哨兵。两者都不分配内存，允许输出与输入从同一地址开始，其他部分重叠被拒绝。所有显式指针范围会在读取前检查地址回绕；容量不足时，别名检查只覆盖调用方声明的输出容量，因此缓冲区之后的独立长度字段仍能接收精确需求值。

编码默认使用小写 `a-f`，`XHEX_UPPER` 改用大写。解码同时接受大小写，默认严格拒绝任何非 HEX 字符和奇数个数字；`XHEX_IGNORE_SPACE` 可显式忽略 SP、HT、VT、FF、CR、LF，适合读取分行或分组后的展示文本。编码器拒绝解码专用标志，解码器也拒绝编码专用标志。

旧版 `lib/string.h` 的 HEX 资产已迁移到该层：显式零长度不再触发 `strlen`，编码长度先检查溢出，解码先完整验证再写入，失败不会发布半个结果。

### `xrtHexEncode`

把任意字节编码为 HEX 文本；输出为空且容量为零时只查询文本长度。

```c
bool xrtHexEncode(
	const void* pData,
	size_t iSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	uint32 iFlags
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 任意字节；可含零 |
| `iSize` | 输入 | — | 字节数 |
| `sOutput` | 输出 | 可空 | 目标缓冲；空 + 零容量 = 只查询 |
| `iCapacity` | 输入 | — | 容量；实际写入要求额外包含末尾零 |
| `pOutputSize` | 输出 | 可空 | 接收文本长度（不含末尾零） |
| `iFlags` | 输入 | — | `XHEX_UPPER` 大写；不得含解码专用标志 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入（或长度已发布） | — |
| `false` | 参数非法、标志错误或容量不足 | 输出不变；容量不足时 `*pOutputSize` 给精确需求 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或部分重叠
- `XERR_VALUE` + `XCODEC_ERROR_HEX_CONFIG` — 标志组合非法
- `XERR_RANGE` — 编码长度溢出

#### 范例

[codec/tour · HEX](../../examples/codec/tour/main.c) · 两段式：先查询再写入

```c
if ( !xrtHexEncode("AB", 2u, NULL, 0u, &iSize, 0u) ||
	(iSize != 4u) ||
	!xrtHexEncode("AB", 2u, Text, sizeof(Text), &iSize,
		0u) ||
	(strcmp(Text, "4142") != 0) ) {
```

### `xrtHexDecode`

严格解码 HEX 文本；输出为空且容量为零时只验证并查询字节数。输出可以与输入从同一地址开始，从而原地解码。

```c
bool xrtHexDecode(
	xstrview Text,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	uint32 iFlags
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | HEX 文本；大小写均接受 |
| `pOutput` | 输出 | 可空 | 二进制目标；不要求哨兵 |
| `iCapacity` | 输入 | — | 字节容量 |
| `pOutputSize` | 输出 | 可空 | 接收解码字节数 |
| `iFlags` | 输入 | — | `XHEX_IGNORE_SPACE` 忽略空白；不得含编码专用标志 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已解码写入 | — |
| `false` | 格式非法、参数非法或容量不足 | 输出不变；容量不足时给出精确需求 |

#### 错误

- `XERR_PROTOCOL` + `XCODEC_ERROR_HEX_FORMAT` — 非 HEX 字符或奇数个数字
- `XERR_VALUE` + `XCODEC_ERROR_HEX_CONFIG` — 标志非法
- `XERR_ARGUMENT` / `XERR_RANGE` — 参数/别名/溢出

#### 范例

[codec/tour · HEX](../../examples/codec/tour/main.c) · 输入 4 字节原地解码为 2 字节

```c
if ( !xrtHexDecode(SV("4142"), Text, 4u, &iSize, 0u) ||
	(iSize != 2u) ||
	(memcmp(Text, "AB", 2u) != 0) ) {
```

### `xrtHexEncodeNew`

编码并返回由 `xrtFree` 释放的末尾补零文本；空输入仍返回独立可释放结果。

```c
str xrtHexEncodeNew(
	const void* pData,
	size_t iSize,
	uint32 iFlags
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 任意字节 |
| `iSize` | 输入 | — | 字节数 |
| `iFlags` | 输入 | — | 同 `xrtHexEncode` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾文本 | — |
| `NULL` | 标志错误、溢出或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` + `XCODEC_ERROR_HEX_CONFIG` — 标志非法
- `XERR_RANGE` — 长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[codec/hex · 二进制安全](../../examples/codec/hex/main.c) · 首字节 0x00 不被截断

```c
str sText = xrtHexEncodeNew(arrData, sizeof(arrData), (uint32)XHEX_UPPER);
```

### `xrtHexDecodeNew`

解码并返回由 `xrtFree` 释放的字节；额外末尾零字节不计入结果长度。

```c
bytes xrtHexDecodeNew(
	xstrview Text,
	size_t* pOutputSize,
	uint32 iFlags
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | HEX 文本 |
| `pOutputSize` | 输出 | 可空 | 接收解码字节数 |
| `iFlags` | 输入 | — | 同 `xrtHexDecode` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 字节缓冲 + 零哨兵 | — |
| `NULL` | 格式错误或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_PROTOCOL` + `XCODEC_ERROR_HEX_FORMAT` — 格式非法
- `XERR_MEMORY` — 分配失败

#### 范例

[codec/hex · 二进制安全](../../examples/codec/hex/main.c) · 视图显式传长度往返

```c
pData = xrtHexDecodeNew((xstrview){ sText, sizeof(arrData) * 2u }, &iSize, 0);
```


## Base64 Codec

`codec_base64` 是字符串、PEM、WebSocket、HTTP 认证、XID 与序列化模块共用的基础编码层。它只依赖 `core`，不与字符串对象或网络对象绑定。启用：

```c
#define XRT_FEATURE_CODEC_BASE64
```

`xbase64config` 的零初始化值表示 RFC 4648 标准字母表、规范 `=` 填充和严格输入：

```c
xbase64config Config = { 0 };
```

- `XBASE64_URL`：使用 URL-safe 的 `-`、`_` 字母表。
- `XBASE64_NO_PADDING`：编码不写 `=`，解码严格拒绝 `=`。
- `XBASE64_IGNORE_SPACE`：解码时忽略 RFC 7468 定义的 SP、HT、VT、FF、CR 和 LF，供 PEM/MIME 使用；编码器拒绝该标志。
- `XBASE64_OPTIONAL_PADDING`：编码仍写规范填充，解码同时接受完整、部分或缺失的末尾填充；不能与 `XBASE64_NO_PADDING` 同时使用。
- `Alphabet`：可选的 64 字符自定义可见 ASCII 字母表。字符必须唯一且不能包含 `=`；不能与 `XBASE64_URL` 同时使用。

解码器严格拒绝：非末尾填充、超过两个填充或不匹配的填充数量；非规范的末尾残余位；不属于字母表的字符；默认模式下的空白或无填充文本；无填充模式下长度余一的文本。

### `xrtBase64Encode`

把字节编码为 Base64 文本；输出为空且容量为零时只查询文本长度。

```c
bool xrtBase64Encode(
	const void* pData,
	size_t iSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	const xbase64config* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 任意字节 |
| `iSize` | 输入 | — | 字节数 |
| `sOutput` | 输出 | 可空 | 目标缓冲；实际写入要求容量额外包含末尾零 |
| `iCapacity` | 输入 | — | 容量 |
| `pOutputSize` | 输出 | 可空 | 接收文本长度（4 × ceil(n/3) 含填充，不含零结尾） |
| `pConfig` | 输入 | 允许空 | 配置；空指针用零值默认（标准字母表 + 规范填充） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入；可原地扩张（输出与输入同址） | — |
| `false` | 参数、配置或容量失败 | 输出不变；容量不足时给出精确需求 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或部分重叠
- `XERR_VALUE` + `XCODEC_ERROR_BASE64_CONFIG` — 标志或字母表非法
- `XERR_RANGE` — 长度溢出

#### 范例

[codec/base64 · 缓冲版](../../examples/codec/base64/main.c) · 容量不足不写半个结果

```c
if ( !xrtBase64Encode(
	Message, sizeof(Message) - 1u, Encoded, sizeof(Encoded),
	&iEncodedSize, NULL
) ) {
	return 1;
}
```

### `xrtBase64Decode`

严格解码 Base64 文本；输出为空且容量为零时只验证并查询字节数。

```c
bool xrtBase64Decode(
	cstr sText,
	size_t iTextSize,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	const xbase64config* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sText` | 输入 | 借用 | Base64 文本 |
| `iTextSize` | 输入 | — | 文本长度 |
| `pOutput` | 输出 | 可空 | 二进制目标；可与输入同址原地解码 |
| `iCapacity` | 输入 | — | 字节容量 |
| `pOutputSize` | 输出 | 可空 | 接收解码字节数 |
| `pConfig` | 输入 | 允许空 | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已解码写入 | — |
| `false` | 格式、配置、参数或容量失败 | 输出不变；容量不足时给出精确需求 |

#### 错误

- `XERR_PROTOCOL` + `XCODEC_ERROR_BASE64_FORMAT` — 非法填充、残余位、字母表外字符或空白违规
- `XERR_VALUE` + `XCODEC_ERROR_BASE64_CONFIG` — 配置非法
- `XERR_ARGUMENT` / `XERR_RANGE`

#### 范例

[codec/tour · Base64](../../examples/codec/tour/main.c) · 查询 + 写入两段式

```c
if ( !xrtBase64Decode("QUJD", 4u, NULL, 0u, &iSize, NULL) ||
	(iSize != 3u) ||
	!xrtBase64Decode("QUJD", 4u, Raw, sizeof(Raw), &iSize,
		NULL) ||
```

### `xrtBase64EncodeNew`

编码并返回由 `xrtFree` 释放的末尾补零文本。

```c
str xrtBase64EncodeNew(
	const void* pData,
	size_t iSize,
	const xbase64config* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 任意字节 |
| `iSize` | 输入 | — | 字节数 |
| `pConfig` | 输入 | 允许空 | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾文本 | — |
| `NULL` | 配置错误或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` + `XCODEC_ERROR_BASE64_CONFIG`
- `XERR_MEMORY`

#### 范例

[codec/tour · Base64](../../examples/codec/tour/main.c) · 分配版编码

```c
sEncoded = xrtBase64EncodeNew("ABC", 3u, NULL);
```

### `xrtBase64DecodeNew`

解码并返回由 `xrtFree` 释放的字节；额外的末尾零字节不计入结果长度。

```c
bytes xrtBase64DecodeNew(
	cstr sText,
	size_t iTextSize,
	size_t* pOutputSize,
	const xbase64config* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sText` | 输入 | 借用 | Base64 文本 |
| `iTextSize` | 输入 | — | 文本长度 |
| `pOutputSize` | 输出 | 可空 | 接收解码字节数 |
| `pConfig` | 输入 | 允许空 | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 字节缓冲 + 零哨兵；二进制仍须用返回长度 | — |
| `NULL` | 格式/配置错误或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_PROTOCOL` + `XCODEC_ERROR_BASE64_FORMAT`
- `XERR_VALUE` + `XCODEC_ERROR_BASE64_CONFIG`
- `XERR_MEMORY`

#### 范例

[codec/base64 · 往返](../../examples/codec/base64/main.c) · 分配型解码核对无损

```c
pDecoded = xrtBase64DecodeNew(
	Encoded, iEncodedSize, &iDecodedSize, NULL
);
```


## Percent Codec

`codec_percent` 是 URI 字节转义的纯编解码层。它不解析 URL，不拆 Query，也不实现 `application/x-www-form-urlencoded` 的加号规则，因此这些上层协议可以独立裁剪并共用同一个基础实现。启用：

```c
#define XRT_FEATURE_CODEC_PERCENT
```

编码器始终直接保留 RFC 3986 unreserved 字节 `ALPHA / DIGIT / "-" / "." / "_" / "~"`，其他字节使用大写 `%HH`。`extraSafe` 可按 URL 组件额外保留 reserved 字符：path segment 用空集合；完整 path 可传 `/`；已知结构安全的 authority 子层可传 `:@`。`%`、空白、控制字符、反斜杠和非 ASCII 字节不能声明为安全字符。

解码器接受大小写十六进制数字，并严格要求每个 `%` 后紧跟两个十六进制数字。它只负责 percent 语法，不把 `+` 转换为空格；后者只属于 form-urlencoded。

### `xrtPercentMapInit`

构建可复用的 ASCII 安全字符集合；原子更新，拒绝控制字符、非 ASCII 和范围别名。

```c
bool xrtPercentMapInit(
	xpercentmap* pMap,
	xstrview Safe,
	bool bIncludeUnreserved
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输出 | 非空 | 接收位图；构建一次后可重复测量/写出多个字段 |
| `Safe` | 输入 | 借用 | 追加的可见 ASCII 字符；重复项无副作用 |
| `bIncludeUnreserved` | 输入 | — | 先加入 RFC 3986 unreserved 字符 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 位图已构建 | — |
| `false` | 集合含非法字符或参数非法 | `*pMap` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或别名
- `XERR_VALUE` + `XCODEC_ERROR_PERCENT_CONFIG` — `Safe` 含控制字符/非 ASCII 等不能直接写入 URI 的字符

#### 范例

[codec/tour · Percent](../../examples/codec/tour/main.c) · 空集合 + 手动字符，构建一次三段复用

```c
if ( !xrtPercentMapInit(&Map, SV(""), false) ||
	!xrtPercentMeasure("A /", 3u, &Map, false, &iSize) ||
```

### `xrtPercentMeasure`

计算指定字符集合和空格规则下的精确编码长度。

```c
bool xrtPercentMeasure(
	const void* pData,
	size_t iSize,
	const xpercentmap* pMap,
	bool bSpaceAsPlus,
	size_t* pOutputSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 任意字节 |
| `iSize` | 输入 | — | 字节数 |
| `pMap` | 输入 | 非空 | 已构建的安全集合 |
| `bSpaceAsPlus` | 输入 | — | 空格写为 `+`（表单语义）或 `%20` |
| `pOutputSize` | 输出 | 非空 | 接收精确编码长度（不含零结尾） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 长度已发布 | — |
| `false` | 参数非法 | `*pOutputSize` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法

#### 范例

[codec/tour · Percent](../../examples/codec/tour/main.c) · 三个非安全字符各 3 字节 → 9

```c
if ( !xrtPercentMapInit(&Map, SV(""), false) ||
	!xrtPercentMeasure("A /", 3u, &Map, false, &iSize) ||
	(iSize != 9u) ) {  /* 每个非安全字符 3 字节 */
```

### `xrtPercentWriteMeasured`

把已经由 `xrtPercentMeasure` 预检的输入顺序写入不重叠输出；不再重复容量与格式检查。

```c
size_t xrtPercentWriteMeasured(
	const void* pData,
	size_t iSize,
	const xpercentmap* pMap,
	bool bSpaceAsPlus,
	char* sOutput
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 与测量时相同的输入 |
| `iSize` | 输入 | — | 字节数 |
| `pMap` | 输入 | 非空 | 与测量时相同的位图 |
| `bSpaceAsPlus` | 输入 | — | 与测量时相同的模式 |
| `sOutput` | 输出 | 非空、不重叠 | 至少测量结果大小的缓冲 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 实际写出字节数 | 调用方保证空间；不写终止零 | 无检查路径，不设置错误 |

#### 错误

- 无 — 预检快速路径；前置条件由 `Measure` 保证

#### 范例

[codec/tour · Percent](../../examples/codec/tour/main.c) · 互不重叠片段的写出

```c
size_t iWritten = xrtPercentWriteMeasured("A /", 3u,
	&Map, false, Text);

if ( (iWritten != 9u) ||
	(memcmp(Text, "%41%20%2F", 9u) != 0) ) {
```

### `xrtPercentEncodeMeasured`

把已经测量的输入编码到可同址扩张的输出，并可补写终止零。

```c
void xrtPercentEncodeMeasured(
	const void* pData,
	size_t iSize,
	const xpercentmap* pMap,
	bool bSpaceAsPlus,
	char* sOutput,
	size_t iOutputSize,
	bool bTerminate
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 与测量时相同的输入 |
| `iSize` | 输入 | — | 字节数 |
| `pMap` | 输入 | 非空 | 位图 |
| `bSpaceAsPlus` | 输入 | — | 模式 |
| `sOutput` | 输出 | 非空、同址可扩张 | 输出容量由调用方保证 |
| `iOutputSize` | 输入 | = 测量结果 | 精确长度 |
| `bTerminate` | 输入 | — | 为真时补写终止零（额外 1 字节容量） |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 预检路径；从后向前扩张支持输入输出同址 |

#### 范例

[codec/tour · Percent](../../examples/codec/tour/main.c) · 同址扩张 + 终止零

```c
xrtPercentEncodeMeasured("A /", 3u, &Map, false, Text,
	9u, true);
if ( (memcmp(Text, "%41%20%2F", 9u) != 0) ||
	(Text[9] != '\0') ) {
```

### `xrtPercentDecodeMeasure`

严格验证全部 percent 转义并计算解码字节数。

```c
bool xrtPercentDecodeMeasure(
	xstrview Text,
	bool bPlusAsSpace,
	size_t* pOutputSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 待解码文本 |
| `bPlusAsSpace` | 输入 | — | 表单语义才为真；URI 路径必须为假 |
| `pOutputSize` | 输出 | 非空 | 接收解码字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 格式合法且长度已发布 | — |
| `false` | 转义非法或参数非法 | `*pOutputSize` 不变 |

#### 错误

- `XERR_PROTOCOL` + `XCODEC_ERROR_PERCENT_FORMAT` — `%` 后未紧跟两个十六进制数字
- `XERR_ARGUMENT` — 指针非法

#### 范例

[codec/tour · Percent](../../examples/codec/tour/main.c) · 两段式解码的预检

```c
if ( !xrtPercentDecodeMeasure(SV("%41%20%2F"), false,
		&iSize) ||
	(iSize != 3u) ) {
```

### `xrtPercentDecodeMeasured`

把已经由 `xrtPercentDecodeMeasure` 预检的文本顺序解码到输出；输出可与输入同址。

```c
size_t xrtPercentDecodeMeasured(
	xstrview Text,
	bool bPlusAsSpace,
	void* pOutput
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 与测量时相同的文本 |
| `bPlusAsSpace` | 输入 | — | 与测量时相同 |
| `pOutput` | 输出 | 非空、同址可收缩 | 至少测量结果大小 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 实际解码字节数 | 从前向后收缩支持原地 | 无检查路径 |

#### 错误

- 无 — 预检快速路径

#### 范例

[codec/tour · Percent](../../examples/codec/tour/main.c) · 预检后一次解码

```c
size_t iDecoded = xrtPercentDecodeMeasured(
	SV("%41%20%2F"), false, Raw);

if ( (iDecoded != 3u) ||
	(memcmp(Raw, "A /", 3u) != 0) ) {
```

### `xrtPercentNext`

无分配读取一个原始或 percent 转义字节；`Offset` 从零开始，成功读取时才推进。

```c
xpercentnext xrtPercentNext(
	xstrview Text,
	bool bPlusAsSpace,
	size_t* pOffset,
	uint8* pValue
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `bPlusAsSpace` | 输入 | — | 表单语义为真 |
| `pOffset` | 输入/输出 | 非空 | 游标；初始为零，成功才推进 |
| `pValue` | 输出 | 非空、独立 | 接收字节值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XPERCENT_NEXT_BYTE` | 读取一个字节；游标已推进 | — |
| `XPERCENT_NEXT_END` | 游标到尾（正常结果） | 不设置错误 |
| `XPERCENT_NEXT_ERROR` | 转义非法、回绕或输出别名 | 不推进游标、不改输出、不改线程错误 |

#### 错误

- 无 — 错误路径刻意不修改线程错误，便于解析器组合

#### 范例

[codec/tour · Percent](../../examples/codec/tour/main.c) · 游标逐字节到 END

```c
if ( (xrtPercentNext(SV("%41"), false, &iOffset, &uValue) !=
		XPERCENT_NEXT_BYTE) ||
	(uValue != 0x41u) ||
	(xrtPercentNext(SV("%41"), false, &iOffset,
		&uValue) != XPERCENT_NEXT_END) ) {
```

### `xrtPercentEncode`

按 RFC 3986 对字节进行百分号编码；编码文本包含零结尾，返回长度不计零结尾。

```c
bool xrtPercentEncode(
	const void* pData,
	size_t iSize,
	xstrview ExtraSafe,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 任意字节 |
| `iSize` | 输入 | — | 字节数 |
| `ExtraSafe` | 输入 | 借用 | 额外保留字符；空集合 = 除 unreserved 外全部转义 |
| `sOutput` | 输出 | 可空 | 目标；空 + 零容量 = 只查询 |
| `iCapacity` | 输入 | — | 容量须含末尾零 |
| `pOutputSize` | 输出 | 可空 | 编码长度（不含零结尾） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入；支持同址向后扩张 | — |
| `false` | 参数、配置或容量失败 | 输出不变；容量不足时给出精确需求 |

#### 错误

- `XERR_VALUE` + `XCODEC_ERROR_PERCENT_CONFIG` — `ExtraSafe` 含 `%`、空白、控制字符等
- `XERR_ARGUMENT` / `XERR_RANGE`

#### 范例

[codec/percent · path segment](../../examples/codec/percent/main.c) · 空白集合把 `/` 与空格都转义

```c
if ( !xrtPercentEncode(
	Segment, sizeof(Segment) - 1u, XRT_STR_LITERAL(""),
	Encoded, sizeof(Encoded), &iEncodedSize
) ) {
```

### `xrtPercentWrite`

按 RFC 3986 写出不带零结尾的编码片段；容量恰好等于返回长度即可。

```c
bool xrtPercentWrite(
	const void* pData,
	size_t iSize,
	xstrview ExtraSafe,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 任意字节 |
| `iSize` | 输入 | — | 字节数 |
| `ExtraSafe` | 输入 | 借用 | 额外保留字符 |
| `sOutput` | 输出 | 可空 | 片段目标；不写终止零 |
| `iCapacity` | 输入 | — | 容量 |
| `pOutputSize` | 输出 | 可空 | 片段长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 同 `xrtPercentEncode` 的失败条件 | 输出不变 |

#### 错误

- 同 `xrtPercentEncode`（`CONFIG` / `ARGUMENT` / `RANGE`）

#### 范例

[codec/tour · Percent](../../examples/codec/tour/main.c) · `/` → `%2F` 无终止零

```c
if ( !xrtPercentWrite("/", 1u, SV(""), Text, sizeof(Text),
		&iSize) ||
	(iSize != 3u) ||
	(memcmp(Text, "%2F", 3u) != 0) ||
```

### `xrtPercentDecode`

严格解码百分号转义；加号保持不变，输出可以与输入从同一地址开始。

```c
bool xrtPercentDecode(
	xstrview Text,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 待解码文本 |
| `pOutput` | 输出 | 可空 | 二进制目标；可含零 |
| `iCapacity` | 输入 | — | 字节容量 |
| `pOutputSize` | 输出 | 可空 | 解码字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已解码（或仅查询长度） | — |
| `false` | 格式、参数或容量失败 | 输出不变；容量不足时给出精确需求 |

#### 错误

- `XERR_PROTOCOL` + `XCODEC_ERROR_PERCENT_FORMAT` — 不完整或非法 `%HH`
- `XERR_ARGUMENT` / `XERR_RANGE`

#### 范例

[codec/tour · Percent](../../examples/codec/tour/main.c) · 空输出仅查询长度

```c
!xrtPercentDecode(SV("%2F"), NULL, 0u, &iSize) ) {
	goto Cleanup;
}
```

### `xrtPercentEncodeNew`

编码并返回由 `xrtFree` 释放的零结尾文本；长度输出可以为空。

```c
str xrtPercentEncodeNew(
	const void* pData,
	size_t iSize,
	xstrview ExtraSafe,
	size_t* pOutputSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 任意字节 |
| `iSize` | 输入 | — | 字节数 |
| `ExtraSafe` | 输入 | 借用 | 额外保留字符 |
| `pOutputSize` | 输出 | 可空 | 接收长度（不含零结尾） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾编码文本 | — |
| `NULL` | 配置错误或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` + `XCODEC_ERROR_PERCENT_CONFIG`
- `XERR_MEMORY`

#### 范例

[codec/tour · Percent](../../examples/codec/tour/main.c) · 分配版含终止零

```c
sEncoded = xrtPercentEncodeNew("/", 1u, SV(""),
	&iNew);
if ( (sEncoded == NULL) || (iNew != 3u) ||
	(strcmp(sEncoded, "%2F") != 0) ) {
```

### `xrtPercentDecodeNew`

解码并返回由 `xrtFree` 释放的字节；末尾哨兵零不计入返回长度。

```c
bytes xrtPercentDecodeNew(
	xstrview Text,
	size_t* pOutputSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 待解码文本 |
| `pOutputSize` | 输出 | 非空 | 接收解码字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 字节缓冲 + 零哨兵 | — |
| `NULL` | 格式错误或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_PROTOCOL` + `XCODEC_ERROR_PERCENT_FORMAT`
- `XERR_MEMORY`

#### 范例

[codec/percent · path segment](../../examples/codec/percent/main.c) · 非法转义（如 %G0）失败返回 NULL

```c
pDecoded = xrtPercentDecodeNew(
	(xstrview){ Encoded, iEncodedSize }, &iDecodedSize
);
```


## 模块契约：所有权

两段式缓冲 API 的输出写入调用方缓冲；`*New` 族返回由 `xrtFree` 释放的独立结果；流式游标借用输入视图，不拥有数据。

## 错误总表

codec 三族共用 `xrt.codec` 域：

| 代码 | 族 | 触发条件 |
|---|---|---|
| `XCODEC_ERROR_HEX_CONFIG` | HEX | 标志组合非法（编码器收解码标志或反之） |
| `XCODEC_ERROR_HEX_FORMAT` | HEX | 非 HEX 字符、奇数长度 |
| `XCODEC_ERROR_BASE64_CONFIG` | Base64 | 标志/字母表非法、互斥标志并用 |
| `XCODEC_ERROR_BASE64_FORMAT` | Base64 | 填充、残余位、字母表外字符、空白违规 |
| `XCODEC_ERROR_PERCENT_CONFIG` | Percent | 安全集合含不可直接写入 URI 的字符 |
| `XCODEC_ERROR_PERCENT_FORMAT` | Percent | 不完整或非法 `%HH` |

空指针、非法重叠、容量不足、长度溢出和 OOM 使用统一的 `XERR_ARGUMENT`、`XERR_RANGE` 与 `XERR_MEMORY` 类别。

## 示例与测试

- `examples/codec/hex/main.c` / `tests/codec/test_hex*.c`
- `examples/codec/base64/main.c` / `tests/codec/test_base64*.c`
- `examples/codec/percent/main.c` / `tests/codec/test_percent*.c`
- `examples/codec/tour/main.c` —— 三族缓冲版与流式族巡览

测试覆盖 RFC 4648 向量、URL-safe、自定义字母表、PEM 空白、无填充、原地编解码、格式原子性、长度溢出、6000 组确定性变异、无分配路径与 OOM。
