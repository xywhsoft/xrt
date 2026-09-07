# HTTP TE

`<xrt/http_te.h>` 实现 RFC 9110 `TE` 请求字段的传输无关协议层。直接路径不分配内存，
也不绑定客户端、服务器或网络对象。

## 类型与常量

### `xhttptecodingflag`

单个 TE 成员标志区分 trailers、传输参数和显式权重。

```c
typedef enum xhttptecodingflag {
	XHTTP_TE_CODING_NONE = 0,
	XHTTP_TE_CODING_TRAILERS = UINT32_C(0x00000001),
	XHTTP_TE_CODING_HAS_PARAMETERS = UINT32_C(0x00000002),
	XHTTP_TE_CODING_HAS_WEIGHT = UINT32_C(0x00000004)
} xhttptecodingflag;
```

| 值 | 语义 |
|---|---|
| `XHTTP_TE_CODING_NONE` | 无 |
| `XHTTP_TE_CODING_TRAILERS` | 支持 trailer |
| `XHTTP_TE_CODING_HAS_PARAMETERS` | 带参数 |
| `XHTTP_TE_CODING_HAS_WEIGHT` | （见枚举语义） |

### `xhttptecoding`

TE 成员借用完整元素、编码名称和不含 q 权重的传输参数。

```c
typedef struct xhttptecoding {
	xstrview Element;
	xstrview Coding;
	xstrview Parameters;
	size_t ParameterCount;
	uint16 Quality;
	uint32 Flags;
} xhttptecoding;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Element` | `xstrview` | Element |
| `Coding` | `xstrview` | Coding |
| `Parameters` | `xstrview` | Parameters |
| `ParameterCount` | `size_t` | ParameterCount |
| `Quality` | `uint16` | Quality |
| `Flags` | `uint32` | 标志位 |

### `xhttptecursor`

单字段游标由初始化函数建立，调用方不得直接修改。

```c
typedef struct xhttptecursor {
	size_t Offset;
	uint8 Validated;
} xhttptecursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Offset` | `size_t` | 偏移量 |
| `Validated` | `uint8` | 是否已校验 |

### `xhttptefieldcursor`

重复字段游标同时记录当前字段和字段内位置。

```c
typedef struct xhttptefieldcursor {
	size_t Field;
	size_t Offset;
	uint8 Validated;
} xhttptefieldcursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Field` | `size_t` | Field |
| `Offset` | `size_t` | 偏移量 |
| `Validated` | `uint8` | 是否已校验 |

### `xhttpteflag`

TE 汇总标志明确区分字段缺失、空字段和 trailers 能力。

```c
typedef enum xhttpteflag {
	XHTTP_TE_NONE = 0,
	XHTTP_TE_PRESENT = UINT32_C(0x00000001),
	XHTTP_TE_ACCEPTS_TRAILERS = UINT32_C(0x00000002),
	XHTTP_TE_HAS_TRANSFER_CODINGS = UINT32_C(0x00000004)
} xhttpteflag;
```

| 值 | 语义 |
|---|---|
| `XHTTP_TE_NONE` | 无 |
| `XHTTP_TE_PRESENT` | 请求携带 TE |
| `XHTTP_TE_ACCEPTS_TRAILERS` | 接受 trailer |
| `XHTTP_TE_HAS_TRANSFER_CODINGS` | （见枚举语义） |

### `xhttpteinfo`

TE 汇总保留字段、总成员和实际传输编码数量。

```c
typedef struct xhttpteinfo {
	size_t FieldCount;
	size_t CodingCount;
	size_t TransferCodingCount;
	uint32 Flags;
} xhttpteinfo;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `FieldCount` | `size_t` | FieldCount |
| `CodingCount` | `size_t` | CodingCount |
| `TransferCodingCount` | `size_t` | TransferCodingCount |
| `Flags` | `uint32` | 标志位 |

## 成员

`xhttptecoding` 借用完整 `Element`、大小写不敏感的 `Coding`，以及不包含最终 `q`
权重的原始 `Parameters`。`Quality` 使用 0 到 1000 的定点值，缺省为 1000；
`ParameterCount` 只统计传输编码参数。

`trailers` 是独立能力标记，表示客户端不会丢弃响应 Trailer。它不能携带参数或权重。
其他成员遵循 `transfer-coding [ weight ]`：参数必须有值，最终 `q` 只能出现一次、不能加
引号，并严格使用 `q=` 形式。参数可用 `xrtHttpParamNext` 继续迭代。

## 列表与重复字段

`xrtHttpTeNext` 迭代一个字段值，`xrtHttpTeFieldNext` 按线路顺序跨越全部重复 `TE`
字段。两种游标必须由初始化函数建立，并在发布第一项前完成输入全集校验；畸形后缀不会
留下半份结果。quoted-string 内的逗号不会被当作列表分隔符，HTTP `#list` 空成员被
忽略。

`xrtHttpTeValid` 与 `xrtHttpTeCount` 处理单字段值。`xrtHttpTeParse` 汇总字段数量、成员
数量、传输编码数量、字段是否存在和 `trailers` 能力。描述符、游标和输出均支持未对齐
存储，输入在迭代期间必须保持不变。

`xrtHttpTeQuality` 返回同一传输编码所有声明中的最高权重；字段缺失或没有匹配时返回零，
语法错误也返回零但会设置错误。`xrtHttpTeAcceptsTrailers` 使用 `ERROR`、`END`、`ITEM`
区分错误、不接受和接受。

## HTTP/1.1 组合约束

RFC 9110 要求发送 `TE` 的发起端同时在 `Connection` 中声明 `TE`，使中间节点知道该
字段逐跳生效。核心层不构建拥有型请求，因此调用方应使用 `xrtHttpTeParse` 验证 `TE`，
使用 `xrtHttpConnectionParse` 验证连接选项，再把两条字段一并交给 HTTP/1 写出函数。
代理或自定义协议代码仍可按自身策略组合这些公开原语。

## 范例

参见 `examples/http/te/main.c`。

## API

### 单值解析

### `xrtHttpTeCursorInit`

初始化单个 TE 字段值游标。

```c
void xrtHttpTeCursorInit(xhttptecursor* pCursor);
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

- 无 — 初始化不失败

#### 范例

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	xrtHttpTeCursorInit(&TeCursor);
```

### `xrtHttpTeCodingParse`

严格解析一个不含列表分隔逗号的 TE 成员。

```c
bool xrtHttpTeCodingParse(
	xstrview Element,
	xhttptecoding* pCoding
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Element` | 输入 | 借用 | 单个成员文本 |
| `pCoding` | 输出 | 非空 | 接收名称/权重 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已解析 | — |
| `false` | 语法错误 | 输出不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	if ( !xrtHttpTeCodingParse(SV("trailers"), &TeCoding) ||
		(TeCoding.Quality != 1000u) ||
		!xrtHttpTeCodingParse(SV("gzip;q=0.8"), &TeCoding) ||
		(TeCoding.Quality != 800u) ) {
```

### `xrtHttpTeValid`

完整验证一个 TE 字段值；HTTP 列表空成员会被忽略。

```c
bool xrtHttpTeValid(xstrview Value);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 合法 | — |
| `false` | 非法 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	if ( !xrtHttpTeValid(SV("gzip, deflate")) ||
		!xrtHttpTeValid(SV("gzip,, ,")) ||
		xrtHttpTeValid(SV("gzip;;")) ||
		!xrtHttpTeCount(SV("gzip, deflate, br"), &iCount) ||
		(iCount != 3u) ) {
```

### `xrtHttpTeCount`

完整验证并统计一个 TE 字段值中的非空成员。

```c
bool xrtHttpTeCount(
	xstrview Value,
	size_t* pCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |
| `pCount` | 输出 | 非空 | 接收计数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计数已写出 | — |
| `false` | 语法错误 | 计数不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
		!xrtHttpTeCount(SV("gzip, deflate, br"), &iCount) ||
		(iCount != 3u) ) {
```

### `xrtHttpTeNext`

按线路顺序迭代一个完整 TE 字段值。

```c
xhttpnext xrtHttpTeNext(
	xstrview Value,
	xhttptecursor* pCursor,
	xhttptecoding* pCoding
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |
| `pCursor` | 输入/输出 | 已初始化 | 游标 |
| `pCoding` | 输出 | 非空 | 接收成员 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	while ( xrtHttpTeNext(SV("gzip, trailers"), &TeCursor,
			&TeCoding) == XHTTP_NEXT_ITEM ) {
```


### 跨字段与汇总

### `xrtHttpTeFieldCursorInit`

初始化跨重复 TE 字段游标。

```c
void xrtHttpTeFieldCursorInit(
	xhttptefieldcursor* pCursor
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

- 无 — 初始化不失败

#### 范例

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	xrtHttpTeFieldCursorInit(&TeFieldCursor);
```

### `xrtHttpTeFieldNext`

跨重复 TE 字段行按线路顺序迭代全部成员。

```c
xhttpnext xrtHttpTeFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttptefieldcursor* pCursor,
	xhttptecoding* pCoding
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pCursor` | 输入/输出 | 已初始化 | 游标 |
| `pCoding` | 输出 | 非空 | 接收成员 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM/END/ERROR` | 三态 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	while ( xrtHttpTeFieldNext(arrTeFields, 2u, &TeFieldCursor,
			&TeCoding) == XHTTP_NEXT_ITEM ) {
```

### `xrtHttpTeParse`

完整解析全部重复 TE 字段并发布零分配汇总。

```c
bool xrtHttpTeParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpteinfo* pInfo
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pInfo` | 输出 | 非空 | 接收汇总（chunked/编码集等） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 汇总已发布 | — |
| `false` | 任一字段非法 | 输出不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/te · 汇总](../../examples/http/te/main.c) · 观察

```c
	if ( !xrtHttpTeParse(Fields, 2u, &Info) ) {
```

### `xrtHttpTeQuality`

返回指定传输编码的最高有效权重；缺失或不匹配返回零。

```c
uint16 xrtHttpTeQuality(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Coding
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | TE 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `Coding` | 输入 | 借用 | 编码名 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `0–1000` | 最高有效权重 | — |
| `0` | 缺失或不匹配（或参数错误） | `XERR_ARGUMENT`（非法时） |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `xrt.http` 域错误 — 字段值非法

#### 范例

[http/te · 权重](../../examples/http/te/main.c) · 观察

```c
		(unsigned int)xrtHttpTeQuality(
			Fields, 2u, XRT_STR_LITERAL("gzip")
		)
```

### `xrtHttpTeAcceptsTrailers`

完整验证并判断客户端是否声明不会丢弃 Trailer。

```c
xhttpnext xrtHttpTeAcceptsTrailers(
	const xhttpfield* pFields,
	size_t iCount
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用数组 | 字段数组 |
| `iCount` | 输入 | — | 条目数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM` | 声明 trailers（`TE: trailers`） | — |
| `XHTTP_NEXT_END` | 未声明 | 不设错 |
| `XHTTP_NEXT_ERROR` | 字段非法 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/small_fields · TE](../../examples/http/small_fields/main.c) · 观察

```c
	if ( xrtHttpTeAcceptsTrailers(arrTeFields, 2u) !=
		XHTTP_NEXT_ITEM ) {
```

## 模块契约：线程

传输编码解析/编码 API 为无共享状态的纯函数，可任意线程并发调用。
