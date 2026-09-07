# HTTP 内容编码协商

`<xrt/http_encoding.h>` 提供与网络、正文和服务端 Reply 解耦的
`Accept-Encoding` 解析与选择。模块不分配内存，适合协议库、原始封包路径和
高层服务器共同复用。

## 类型与常量

### `xhttpcoding`

内置编码值同时可作为可用编码位掩码，NONE 表示没有可接受表示。

```c
typedef enum xhttpcoding {
	XHTTP_CODING_NONE = 0,
	XHTTP_CODING_IDENTITY = UINT32_C(0x00000001),
	XHTTP_CODING_GZIP = UINT32_C(0x00000002),
	XHTTP_CODING_DEFLATE = UINT32_C(0x00000004)
} xhttpcoding;
```

| 值 | 语义 |
|---|---|
| `XHTTP_CODING_NONE` | 无 |
| `XHTTP_CODING_IDENTITY` | 无编码 |
| `XHTTP_CODING_GZIP` | gzip 包装 |
| `XHTTP_CODING_DEFLATE` | deflate |

### `xhttpacceptencodingflag`

解析标志区分 Header 缺失与各个显式编码成员。

```c
typedef enum xhttpacceptencodingflag {
	XHTTP_ACCEPT_ENCODING_NONE = 0,
	XHTTP_ACCEPT_ENCODING_PRESENT = UINT32_C(0x00000001),
	XHTTP_ACCEPT_ENCODING_GZIP = UINT32_C(0x00000002),
	XHTTP_ACCEPT_ENCODING_DEFLATE = UINT32_C(0x00000004),
	XHTTP_ACCEPT_ENCODING_IDENTITY = UINT32_C(0x00000008),
	XHTTP_ACCEPT_ENCODING_WILDCARD = UINT32_C(0x00000010)
} xhttpacceptencodingflag;
```

| 值 | 语义 |
|---|---|
| `XHTTP_ACCEPT_ENCODING_NONE` | 无 |
| `XHTTP_ACCEPT_ENCODING_PRESENT` | 请求携带 Accept-Encoding |
| `XHTTP_ACCEPT_ENCODING_GZIP` | gzip 包装 |
| `XHTTP_ACCEPT_ENCODING_DEFLATE` | deflate 包装 |
| `XHTTP_ACCEPT_ENCODING_IDENTITY` | 允许 identity |
| `XHTTP_ACCEPT_ENCODING_WILDCARD` | * 通配 |

### `xhttpacceptencoding`

质量值使用 0 到 1000 的定点表示。 同一编码重复出现时保留最高质量，Flags 记录是否显式出现。

```c
typedef struct xhttpacceptencoding {
	uint16 Gzip;
	uint16 Deflate;
	uint16 Identity;
	uint16 Wildcard;
	uint32 Flags;
} xhttpacceptencoding;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Gzip` | `uint16` | Gzip |
| `Deflate` | `uint16` | Deflate |
| `Identity` | `uint16` | Identity |
| `Wildcard` | `uint16` | Wildcard |
| `Flags` | `uint32` | 标志位 |

### `xhttpcontentencodingflag`

Content-Encoding 计划保留字段存在性、容错层和未知扩展。

```c
typedef enum xhttpcontentencodingflag {
	XHTTP_CONTENT_ENCODING_NONE = 0,
	XHTTP_CONTENT_ENCODING_PRESENT = UINT32_C(0x00000001),
	XHTTP_CONTENT_ENCODING_IDENTITY = UINT32_C(0x00000002),
	XHTTP_CONTENT_ENCODING_UNKNOWN = UINT32_C(0x00000004),
	XHTTP_CONTENT_ENCODING_LEGACY = UINT32_C(0x00000008)
} xhttpcontentencodingflag;
```

| 值 | 语义 |
|---|---|
| `XHTTP_CONTENT_ENCODING_NONE` | 无 |
| `XHTTP_CONTENT_ENCODING_PRESENT` | 响应携带 Content-Encoding |
| `XHTTP_CONTENT_ENCODING_IDENTITY` | 内容未编码 |
| `XHTTP_CONTENT_ENCODING_UNKNOWN` | 未知 |
| `XHTTP_CONTENT_ENCODING_LEGACY` | 传统别名（x-gzip） |

### `xhttpcontentencodingcursor`

游标可在重复 Content-Encoding 字段之间无分配前向迭代。

```c
typedef struct xhttpcontentencodingcursor {
	size_t Field;
	size_t Offset;
} xhttpcontentencodingcursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Field` | `size_t` | Field |
| `Offset` | `size_t` | 偏移量 |

### `xhttpcontentencodingitem`

每个成员保留原 token，并把内置编码映射到统一枚举。

```c
typedef struct xhttpcontentencodingitem {
	xstrview Token;
	xhttpcoding Coding;
} xhttpcontentencodingitem;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Token` | `xstrview` | Token |
| `Coding` | `xhttpcoding` | Coding |

### `xhttpcontentencodingplan`

计划只保存解析事实，不绑定具体解码算法或未知编码策略。

```c
typedef struct xhttpcontentencodingplan {
	size_t FieldCount;
	size_t CodingCount;
	size_t DecoderCount;
	size_t UnknownCount;
	size_t JoinedSize;
	uint32 Flags;
} xhttpcontentencodingplan;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `FieldCount` | `size_t` | FieldCount |
| `CodingCount` | `size_t` | CodingCount |
| `DecoderCount` | `size_t` | DecoderCount |
| `UnknownCount` | `size_t` | UnknownCount |
| `JoinedSize` | `size_t` | JoinedSize |
| `Flags` | `uint32` | 标志位 |

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XHTTP_CONTENT_CODINGS_DEFAULT` | `4u` | Content-Encoding 解析与通用 Body 解码共享的安全层数边界。 |
| `XHTTP_CONTENT_CODINGS_MAX` | `16u` | 上限 |

## 分层

`xrtHttpQualityParse` 与 `xrtHttpWeightedTokenNext` 位于
`<xrt/http.h>`，可解析任意 `token [ weight ]` 字段。需要支持 Brotli、Zstd
或应用私有编码时，可以直接使用这一层，不受内置编码集合限制。

`xrtHttpAcceptEncodingInit`、`Add` 和 `Parse` 在此基础上维护 gzip、deflate、
identity 与 wildcard 的有效质量。重复成员保留最高 qvalue，显式成员覆盖
wildcard。

`xrtHttpAcceptEncodingValid` 可验证由调用方保存或修改的公开协商状态；它是
零分配纯查询，不改变线程原有错误。

`xrtHttpAcceptEncodingSelect` 只在调用方给出的可用编码中选择。最高质量相同
时先使用 `Preferred`，再按 gzip、deflate、identity 排序。

## Content-Encoding

同一模块也提供接收方 `Content-Encoding` 计划，但不绑定 Inflate 或 Body。
`xrtHttpCodingParse` 把 identity、gzip、兼容别名 x-gzip 和 deflate 映射到
内置枚举；未知合法 token 返回 `XHTTP_CODING_NONE`，调用方仍可通过原 token
接入 Brotli、Zstd 或应用私有解码器。

`xrtHttpContentEncodingNext` 按字段出现顺序遍历全部重复字段与列表成员，忽略
RFC 列表中的空成员。`xrtHttpContentEncodingPlan` 一次汇总字段数、总层数、
内置解码器数、未知层数和原字段值合并后的精确大小。计划不分配内存，也不把
未知层当作错误，因此代理可以保留原始表示，高级客户端则可以选择自动解码、
原样交付或拒绝。

解码顺序必须与字段中的应用顺序相反。例如 `gzip, deflate` 表示先 gzip、
再 deflate，接收端必须先解 deflate、再解 gzip。`identity` 按无变换层容错，
但发送方不应在 Content-Encoding 中生成它。

`xrtHttpContentEncodingWrite` 以 `", "` 连接重复字段的原始值，适合删除
Content-Encoding 后保留诊断元数据。它不附加零字符，支持空输出查询精确大小，
容量不足时不会写出部分结果。游标、计划、大小槽和输出区都不能覆盖字段描述符或
借用文本，协议层会在写入前拒绝重叠参数。

## 缺失与空值

- 没有 `Accept-Encoding` Header：RFC 9110 规定任意内容编码都可接受。
- 存在空 Header：客户端不希望响应使用内容编码，只选择 identity。
- 未显式列出 identity：默认可接受；只有 `identity;q=0`，或没有更具体
  identity 时的 `*;q=0`，才排除 identity。
- 已知和 wildcard 质量均为零且 identity 也被排除：选择结果为
  `XHTTP_CODING_NONE`，高层服务器通常应返回 406。

协议接受能力不等于服务器策略。为兼容不完整的旧客户端，高层自动压缩默认可
选择在 Header 缺失时仍发送 identity；纯协商层仍完整保留 RFC 语义。

```c
xhttpacceptencoding Accept;
xhttpcoding Coding;

xrtHttpAcceptEncodingInit(&Accept);
xrtHttpAcceptEncodingAdd(
	&Accept,
	XRT_STR_LITERAL("gzip;q=0.8, deflate;q=0.4")
);
Coding = xrtHttpAcceptEncodingSelect(
	&Accept,
	XHTTP_CODING_IDENTITY |
		XHTTP_CODING_GZIP |
		XHTTP_CODING_DEFLATE,
	XHTTP_CODING_GZIP
);
```

## API

### 编码枚举

### `xrtHttpCodingParse`

把编码 token 解析为内置枚举（identity/gzip/deflate，x-gzip 为 gzip 别名）；未知返回 NONE。

```c
xhttpcoding xrtHttpCodingParse(xstrview Token);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Token` | 输入 | 借用 | 编码 token（大小写不敏感） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_CODING_IDENTITY/GZIP/DEFLATE` | 内置编码 | — |
| `XHTTP_CODING_NONE` | 未知编码（如 zstd） | 不设错 |

#### 错误

- 无 — NONE 是分类结果

#### 范例

[http/small_fields · 编码枚举](../../examples/http/small_fields/main.c) · 观察

```c
		if ( (xrtHttpCodingParse(SV("gzip")) !=
				XHTTP_CODING_GZIP) ||
```

### `xrtHttpCodingName`

返回 identity、gzip 或 deflate 的静态小写 token；NONE 返回空视图。

```c
xstrview xrtHttpCodingName(xhttpcoding Coding);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Coding` | 输入 | — | 编码枚举 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 静态小写 token | — |
| 空视图 | `XHTTP_CODING_NONE` | 纯查询 |

#### 错误

- 无 — 纯查询

#### 范例

[http/encoding · 编码名](../../examples/http/encoding/main.c) · 观察

```c
	Name = xrtHttpCodingName(Coding);
```


### Accept-Encoding 协商

### `xrtHttpAcceptEncodingInit`

初始化为 Header 缺失状态；按 RFC 该状态接受任意内容编码。

```c
void xrtHttpAcceptEncodingInit(
	xhttpacceptencoding* pAccept
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAccept` | 输出 | 非空 | 协商状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无

#### 范例

[http/encoding · 协商](../../examples/http/encoding/main.c) · 观察

```c
	xrtHttpAcceptEncodingInit(&Accept);
```

### `xrtHttpAcceptEncodingValid`

判断公开协商状态字段是否自洽；纯查询不修改线程原有错误。

```c
bool xrtHttpAcceptEncodingValid(
	const xhttpacceptencoding* pAccept
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAccept` | 输入 | 非空 | 协商状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 自洽 | — |
| `false` | 不自洽 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/small_fields · 协商](../../examples/http/small_fields/main.c) · 观察

```c
			!xrtHttpAcceptEncodingValid(&Accept) ) {
```

### `xrtHttpAcceptEncodingAdd`

失败原子地合并一个 Accept-Encoding 字段值；空值只记录 Header 存在，未知编码语法有效但不进入内置集合。

```c
bool xrtHttpAcceptEncodingAdd(
	xhttpacceptencoding* pAccept,
	xstrview Value
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAccept` | 输入/输出 | 非空 | 协商状态 |
| `Value` | 输入 | 借用 | 单个字段值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已合并（失败原子） | — |
| `false` | 语法错误 | 状态不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/encoding · 协商](../../examples/http/encoding/main.c) · 观察

```c
	if ( !xrtHttpAcceptEncodingAdd(
		&Accept,
		XRT_STR_LITERAL(
			"gzip;q=0.8, deflate;q=0.4, identity;q=0.1"
		)
	) ) {
```

### `xrtHttpAcceptEncodingParse`

扫描全部同名字段并构建零分配协商状态；Fields 为空且 Count 为零表示没有任何字段。

```c
bool xrtHttpAcceptEncodingParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpacceptencoding* pAccept
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pAccept` | 输出 | 非空 | 接收协商状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 状态已构建 | — |
| `false` | 任一字段非法 | 输出不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · 协商](../../examples/http/small_fields/main.c) · 观察

```c
		if ( !xrtHttpAcceptEncodingParse(arrAe, 1u, &Accept) ||
			(xrtHttpAcceptEncodingQuality(&Accept,
				XHTTP_CODING_GZIP) != 900u) ||
			(xrtHttpAcceptEncodingQuality(&Accept,
				XHTTP_CODING_IDENTITY) != 1000u) ||
			!xrtHttpAcceptEncodingValid(&Accept) ) {
```

### `xrtHttpAcceptEncodingQuality`

返回指定内置编码的有效质量；参数错误返回零并设置错误。

```c
uint16 xrtHttpAcceptEncodingQuality(
	const xhttpacceptencoding* pAccept,
	xhttpcoding Coding
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAccept` | 输入 | 非空 | 协商状态 |
| `Coding` | 输入 | — | 内置编码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `0–1000` | 有效质量（缺失按 RFC 缺省 1000） | — |
| `0` | 参数错误 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[http/small_fields · 协商](../../examples/http/small_fields/main.c) · 观察

```c
			(xrtHttpAcceptEncodingQuality(&Accept,
				XHTTP_CODING_GZIP) != 900u) ||
			(xrtHttpAcceptEncodingQuality(&Accept,
```

### `xrtHttpAcceptEncodingSelect`

从 Available 位掩码中选择最高质量编码；等质量时先选 Preferred，再按 gzip、deflate、identity 顺序。

```c
xhttpcoding xrtHttpAcceptEncodingSelect(
	const xhttpacceptencoding* pAccept,
	uint32 iAvailable,
	xhttpcoding Preferred
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAccept` | 输入 | 非空 | 协商状态 |
| `iAvailable` | 输入 | `XHTTP_CODING_*` 位掩码 | 服务器可用集 |
| `Preferred` | 输入 | — | 等质量偏好 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 编码值 | 选中的编码 | — |
| `XHTTP_CODING_NONE` | 无可用匹配 | `XERR_ARGUMENT`（参数错误时） |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[http/encoding · 选择](../../examples/http/encoding/main.c) · 观察

```c
	Coding = xrtHttpAcceptEncodingSelect(
		&Accept,
		XHTTP_CODING_IDENTITY |
			XHTTP_CODING_GZIP |
			XHTTP_CODING_DEFLATE,
		XHTTP_CODING_GZIP
	);
```


### Content-Encoding 响应链

### `xrtHttpContentEncodingCursorInit`

初始化可重复使用的 Content-Encoding 前向游标。

```c
void xrtHttpContentEncodingCursorInit(
	xhttpcontentencodingcursor* pCursor
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输出 | 非空 | 调用方存储 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无

#### 范例

[http/small_fields · 响应链](../../examples/http/small_fields/main.c) · 观察

```c
			xrtHttpContentEncodingCursorInit(&Cursor);
```

### `xrtHttpContentEncodingNext`

按字段出现顺序迭代全部 Content-Encoding 成员；未知扩展仍返回 ITEM 且 Coding 为 NONE。

```c
xhttpnext xrtHttpContentEncodingNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcontentencodingcursor* pCursor,
	xhttpcontentencodingitem* pItem
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pCursor` | 输入/输出 | 已初始化 | 游标 |
| `pItem` | 输出 | 非空 | 接收编码条目 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态（未知扩展 Coding=NONE） | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · 响应链](../../examples/http/small_fields/main.c) · 观察

```c
			if ( xrtHttpContentEncodingNext(arrCe, 1u,
					&Cursor, &Item) != XHTTP_NEXT_ITEM ||
				(Item.Token.Size != 4u) ||
				(Item.Coding != XHTTP_CODING_GZIP) ) {
```

### `xrtHttpContentEncodingPlan`

无分配构建完整 Content-Encoding 计划；DecoderCount 只统计 gzip/deflate。

```c
bool xrtHttpContentEncodingPlan(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcontentencodingplan* pPlan
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pPlan` | 输出 | 非空 | 接收解码器数/连接串大小 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计划已发布 | — |
| `false` | 字段非法 | 输出不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/encoding · 响应链](../../examples/http/encoding/main.c) · 观察

```c
	if ( !xrtHttpContentEncodingPlan(
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		&Plan
	) ) {
```

### `xrtHttpContentEncodingWrite`

按字段出现顺序写出以逗号空格连接的原始值；空输出可查大小，容量不足不写部分结果。

```c
bool xrtHttpContentEncodingWrite(
	const xhttpfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pOutput` | 输出 | 可空 | 空+零容量 = 查长度 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出（不附加零） | — |
| `false` | 容量不足 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE`

#### 范例

[http/small_fields · 响应链](../../examples/http/small_fields/main.c) · 观察

```c
		if ( !xrtHttpContentEncodingWrite(arrCe, 1u, Buffer,
				sizeof(Buffer), &iCount) ||
			(iCount != 8u) ||
			(memcmp(Buffer, "gzip, br", 8u) != 0) ) {
```

