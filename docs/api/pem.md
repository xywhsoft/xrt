# PEM

`pem` 为 X.509、PKCS、TLS 和其他文本封装提供统一的 RFC 7468 底层。模块同时公开借用式遍历、精确标签查找、严格 Base64 解码和规范文本编码，协议层无需再私有实现边界扫描或 Base64 包装。

## 类型与常量

### `xpemblock`

PEM 块的标签、正文和完整消费区间都借用原始输入；Raw 包含存在的结束行换行。

```c
typedef struct xpemblock {
	xstrview Label;
	xstrview Body;
	xstrview Raw;
} xpemblock;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Label` | `xstrview` | 标签 |
| `Body` | `xstrview` | 主体 |
| `Raw` | `xstrview` | Raw |

### `xpemcursor`

PEM 游标允许输入前后存在说明文本，并按出现顺序遍历多个块。

```c
typedef struct xpemcursor {
	xstrview Text;
	size_t Offset;
} xpemcursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Text` | `xstrview` | 文本视图 |
| `Offset` | `size_t` | 偏移量 |

### `xpemresult`

PEM 读取结果把正常结束与格式错误分开。

```c
typedef enum xpemresult {
	XPEM_ERROR = -1,
	XPEM_DONE = 0,
	XPEM_BLOCK = 1
} xpemresult;
```

| 值 | 语义 |
|---|---|
| `XPEM_ERROR` | 失败 |
| `XPEM_DONE` | 完成 |

### `xpemerror`

PEM 模块稳定错误码。

```c
typedef enum xpemerror {
	XPEM_ERROR_BOUNDARY = 1,
	XPEM_ERROR_LABEL,
	XPEM_ERROR_BODY,
	XPEM_ERROR_NOT_FOUND
} xpemerror;
```

| 值 | 语义 |
|---|---|
| `XPEM_ERROR_BOUNDARY` | 失败 |
| `XPEM_ERROR_LABEL` | 失败 |
| `XPEM_ERROR_BODY` | 失败 |

## 裁剪

```c
#define XRT_FEATURE_PEM
#define XRT_FEATURE_CODEC_BASE64
```

`pem` 只依赖 `codec_base64`；不依赖 ASN.1、加密、文件或网络。只处理二进制与文本的封装关系，证书和密钥语义留给上层模块。

## 借用式遍历

```c
xpemcursor cursor;
xpemblock block;

xrtPemInit(&cursor, text, text_size);
while ( xrtPemRead(&cursor, &block) == XPEM_BLOCK ) {
	/* block.Label、block.Body 和 block.Raw 都借用 text。 */
}
```

`xrtPemRead` 允许块前后存在说明文本，并识别 LF、CRLF 和 CR。开始与结束标签必须精确匹配；嵌套开始边界、缺失结束边界、非法标签和边界尾部文本都会被拒绝。返回 `XPEM_DONE` 表示正常结束，不设置错误。

游标和块只在成功时更新。`Raw` 精确覆盖本次消费的块，从开始边界到结束边界的行尾；原输入必须在所有借用视图使用完之前保持有效，输入不要求以零字节结尾。

### `xrtPemInit`

初始化一个严格有界、借用输入的 PEM 游标。

```c
bool xrtPemInit(xpemcursor* pCursor, cstr sText, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输出 | 非空 | 接收游标 |
| `sText` | 输入 | 借用 | PEM 文本 |
| `iSize` | 输入 | — | 文本字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pem_tour](../../examples/asn1/pem_tour/main.c) · 初始化游标

```c
	if ( !xrtPemInit(&Cursor, sText, sizeof(sText) - 1u) ) {
```

### `xrtPemRead`

读取下一个 PEM 块；失败时游标和输出保持不变。

```c
xpemresult xrtPemRead(xpemcursor* pCursor, xpemblock* pBlock)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入/输出 | 已初始化 | 目标游标 |
| `pBlock` | 输出 | 非空 | 接收块视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XPEM_BLOCK` | 已读取一个块，`pBlock` 有效 | — |
| `XPEM_DONE` | 输入耗尽，无更多块 | 不设错误 |
| `XPEM_ERROR` | 参数或格式错误 | `XERR_ARGUMENT` / `xrt.pem` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或游标状态非法
- `xrt.pem` / `XPEM_ERROR_LABEL`（`XERR_PROTOCOL`） — 标签缺失或不完整
- `xrt.pem` / `XPEM_ERROR_BOUNDARY`（`XERR_PROTOCOL`） — 起止边界不匹配或残缺

#### 范例

[pem_tour](../../examples/asn1/pem_tour/main.c) · 读取下一块

```c
	while ( xrtPemRead(&Cursor, &Block) == XPEM_BLOCK ) {
```

## 查找与解码

```c
xpemblock block;
size_t size;
bytes data;

if ( xrtPemFind(text, text_size, "CERTIFICATE", &block) ) {
	data = xrtPemDecodeNew(&block, &size);
}
```

`xrtPemFind` 按出现顺序查找第一个标签完全相同的块。`xrtPemDecode` 支持查询长度和调用方缓冲；`xrtPemDecodeNew` 返回由 `xrtFree` 释放的字节，并额外保留一个不计入长度的末尾零字节。

正文使用严格、规范的 Base64 解码，只忽略 RFC 文本封装允许的 SP、HT、VT、FF、CR 和 LF。正文格式失败使用 `xrt.pem` 的 `XPEM_ERROR_BODY`，并通过 `xrtErrorCause` 保留原始 `xrt.codec` 错误。

### `xrtPemFind`

查找第一个标签完全匹配的 PEM 块。

```c
bool xrtPemFind(
	cstr sText,
	size_t iSize,
	cstr sLabel,
	xpemblock* pBlock
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sText` | 输入 | 借用 | PEM 文本 |
| `iSize` | 输入 | — | 文本字节数 |
| `sLabel` | 输入 | 非空、零结尾 | 目标标签 |
| `pBlock` | 输出 | 非空 | 接收块视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已找到，`pBlock` 有效 | — |
| `false` | 未找到或失败 | `XERR_NOT_FOUND` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.pem` / `XPEM_ERROR_LABEL`（`XERR_VALUE`） — 标签为空或非法
- `xrt.pem` / `XPEM_ERROR_NOT_FOUND`（`XERR_NOT_FOUND`） — 无匹配块
- `xrt.pem` 域错误 — 遍历中遇到的格式错误（LABEL/BOUNDARY）

#### 范例

[pem](../../examples/asn1/pem/main.c) · 查找块

```c
		!xrtPemFind(sText, strlen(sText), "XRT DATA", &Block) ) {
```

### `xrtPemDecode`

解码 PEM 块正文；输出为空且容量为零时只验证并查询长度。

```c
bool xrtPemDecode(
	const xpemblock* pBlock,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBlock` | 输入 | 非空、来自 Init/Read/Find | PEM 块 |
| `pOutput` | 输出 | 允许空 | 空 + 零容量 = 验证查询 |
| `iCapacity` | 输入 | — | 输出容量 |
| `pOutputSize` | 输出 | 非空 | 接收解码字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已解码（或已验证） | — |
| `false` | 正文或容量非法 | `xrt.pem` / 底座错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.pem` / `XPEM_ERROR_BODY`（`XERR_PROTOCOL`） — 正文不是规范 Base64
- `XERR_RANGE` — 容量不足，由 Base64 底座透传，不写半个结果

#### 范例

[pem_tour](../../examples/asn1/pem_tour/main.c) · 解码正文

```c
		if ( xrtPemDecode(&Block, Data, sizeof(Data), &iDataSize) ) {
```

### `xrtPemDecodeNew`

解码并返回由 `xrtFree` 释放的字节。

```c
bytes xrtPemDecodeNew(
	const xpemblock* pBlock,
	size_t* pOutputSize
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBlock` | 输入 | 非空、来自 Init/Read/Find | PEM 块 |
| `pOutputSize` | 输出 | 非空 | 接收解码字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 解码字节，`xrtFree` 释放 | — |
| `NULL` | 失败 | `xrt.pem` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.pem` / `XPEM_ERROR_BODY`（`XERR_PROTOCOL`） — 正文不是规范 Base64
- `XERR_MEMORY` — 分配失败
- `XERR_OVERFLOW` — 长度引起尺寸溢出

#### 范例

[pem](../../examples/asn1/pem/main.c) · 分配解码

```c
	pDecoded = xrtPemDecodeNew(&Block, &iDecodedSize);
```

## 规范编码

```c
size_t text_size;
str text = xrtPemEncodeNew("PUBLIC KEY", der, der_size);

xrtPemEncode(
	"PUBLIC KEY", der, der_size,
	buffer, buffer_capacity, &text_size
);
```

编码器统一输出五连字符边界、每行恰好最多 64 个 Base64 字符和 LF 换行。`xrtPemEncode` 在输出为空且容量为零时只查询长度；实际写入要求容量额外包含末尾零字节。容量和重叠失败不会修改输出缓冲。

空二进制正文生成相邻的开始行与结束行，不添加无意义空行。

### `xrtPemEncode`

生成 RFC 7468 文本，Base64 每行 64 字符并统一使用 LF；输出为空且容量为零时只查询文本长度，实际写入要求额外的末尾零字节。

```c
bool xrtPemEncode(
	cstr sLabel,
	const void* pData,
	size_t iSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sLabel` | 输入 | 非空、零结尾 | 块标签 |
| `pData` | 输入 | 非空 | 原始字节 |
| `iSize` | 输入 | — | 字节数 |
| `sOutput` | 输出 | 允许空 | 空 + 零容量 = 查询长度 |
| `iCapacity` | 输入 | — | 容量，须含末尾零字节 |
| `pOutputSize` | 输出 | 非空 | 接收不含零字节的长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入并补零 | — |
| `false` | 参数、标签或容量失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.pem` / `XPEM_ERROR_LABEL`（`XERR_VALUE`） — 标签为空或非法
- `XERR_RANGE` — 容量不足（须含末尾零字节），不写半个结果

#### 范例

[pem_tour](../../examples/asn1/pem_tour/main.c) · 规范编码

```c
	if ( !xrtPemEncode("DATA", Data, iDataSize, Text, sizeof(Text),
		&iTextSize) ) {
```

### `xrtPemEncodeNew`

生成并返回由 `xrtFree` 释放的 PEM 文本。

```c
str xrtPemEncodeNew(cstr sLabel, const void* pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sLabel` | 输入 | 非空、零结尾 | 块标签 |
| `pData` | 输入 | 非空 | 原始字节 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾 PEM 文本，`xrtFree` 释放 | — |
| `NULL` | 失败 | `xrt.pem` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.pem` / `XPEM_ERROR_LABEL`（`XERR_VALUE`） — 标签为空或非法
- `XERR_MEMORY` — 分配失败

#### 范例

[pem](../../examples/asn1/pem/main.c) · 分配编码

```c
	sText = xrtPemEncodeNew("XRT DATA", Data, sizeof(Data));
```

## 错误

PEM 错误使用 `xrt.pem` 域：

- `XPEM_ERROR_BOUNDARY`：边界缺失、嵌套或行格式错误；
- `XPEM_ERROR_LABEL`：标签非法或开始、结束标签不一致；
- `XPEM_ERROR_BODY`：正文不是规范 Base64；
- `XPEM_ERROR_NOT_FOUND`：目标标签不存在。

结构错误在适用时把 `offset=<字节偏移>` 写入错误数据，供上层错误映射和 C 日志定位。

## 示例与测试

- `examples/asn1/pem/main.c`
- `tests/asn1/test_pem.c`
- `tests/asn1/test_pem_oom.c`
- `tests/single/test_single_pem.c`

测试覆盖多块与说明文本、三种换行、非零结尾输入、空正文、64 字符换行、错误原因链、非法边界、容量与重叠原子性、OOM 和单头生成。
