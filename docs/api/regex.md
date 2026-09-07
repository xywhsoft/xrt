# Regex

正则模块使用非回溯线性时间引擎。编译对象 `xregex` 不可变、引用计数且可以跨线程共享；`xregexmatcher` 保存可变执行缓存，只能由一个执行流使用，但可以反复匹配以避免热路径分配。

## 类型与常量

### `xregexflag`

编译标志与表达式内的 (?i)、(?m)、(?s)、(?U) 语义一致。

```c
typedef enum xregexflag {
	XREGEX_IGNORE_CASE = UINT32_C(0x00000001),
	XREGEX_MULTILINE = UINT32_C(0x00000002),
	XREGEX_DOT_ALL = UINT32_C(0x00000004),
	XREGEX_UNGREEDY = UINT32_C(0x00000008)
} xregexflag;
```

| 值 | 语义 |
|---|---|
| `XREGEX_IGNORE_CASE` | IGNORECASE |
| `XREGEX_MULTILINE` | MULTILINE |
| `XREGEX_DOT_ALL` | DOT全部 |
| `XREGEX_UNGREEDY` | （见枚举语义） |

### `xregexresult`

所有匹配入口使用同一三态结果，未匹配不属于错误。

```c
typedef enum xregexresult {
	XREGEX_ERROR = -1,
	XREGEX_NONE = 0,
	XREGEX_MATCH = 1
} xregexresult;
```

| 值 | 语义 |
|---|---|
| `XREGEX_ERROR` | 失败 |
| `XREGEX_NONE` | 无 |
| `XREGEX_MATCH` | （见枚举语义） |

### `xregexerror`

正则模块错误代码在 xrt.regex 域内保持稳定。

```c
typedef enum xregexerror {
	XREGEX_ERROR_CONFIG = 1501,
	XREGEX_ERROR_PATTERN,
	XREGEX_ERROR_LIMIT,
	XREGEX_ERROR_EXECUTE,
	XREGEX_ERROR_REPLACEMENT,
	XREGEX_ERROR_CALLBACK
} xregexerror;
```

| 值 | 语义 |
|---|---|
| `XREGEX_ERROR_CONFIG` | 配置非法 |
| `XREGEX_ERROR_PATTERN` | 失败 |
| `XREGEX_ERROR_LIMIT` | 超限 |
| `XREGEX_ERROR_EXECUTE` | 失败 |
| `XREGEX_ERROR_REPLACEMENT` | 失败 |
| `XREGEX_ERROR_CALLBACK` | （见枚举语义） |

### `xregexconfig`

编译配置同时控制语义标志与可由调用方收紧的资源预算。

```c
typedef struct xregexconfig {
	uint32 Flags;
	size_t MaxPatternBytes;
	size_t MaxCaptures;
	uint32 Reserved[4];
} xregexconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |
| `MaxPatternBytes` | `size_t` | MaxPatternBytes |
| `MaxCaptures` | `size_t` | MaxCaptures |

### `xregexspan`

匹配范围使用零基半开字节区间 [Begin, End)。

```c
typedef struct xregexspan {
	size_t Begin;
	size_t End;
} xregexspan;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Begin` | `size_t` | Begin |
| `End` | `size_t` | 结束 |

### `xregexcapture`

捕获记录区分未参与匹配与合法的空匹配。

```c
typedef struct xregexcapture {
	bool Matched;
	xregexspan Span;
	xstrview Text;
} xregexcapture;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Matched` | `bool` | Matched |
| `Span` | `xregexspan` | Span |
| `Text` | `xstrview` | 文本视图 |

### `xregexsplitflag`

拆分标志控制捕获输出和空项过滤。

```c
typedef enum xregexsplitflag {
	XREGEX_SPLIT_CAPTURES = UINT32_C(0x00000001),
	XREGEX_SPLIT_SKIP_EMPTY = UINT32_C(0x00000002)
} xregexsplitflag;
```

| 值 | 语义 |
|---|---|
| `XREGEX_SPLIT_CAPTURES` | XREGEX按级别分流CAPTURES |
| `XREGEX_SPLIT_SKIP_EMPTY` | （见枚举语义） |

### `xregexsplitconfig`

Limit 是最多使用的分隔匹配数，SIZE_MAX 表示不限制。

```c
typedef struct xregexsplitconfig {
	size_t Limit;
	uint32 Flags;
	uint32 Reserved[4];
} xregexsplitconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Limit` | `size_t` | 上限 |
| `Flags` | `uint32` | 标志位 |

### `xregexsplitpart`

Capture 为 XRT_NPOS 时是普通字段，否则是捕获索引。

```c
typedef struct xregexsplitpart {
	xstrview Text;
	size_t Capture;
	bool Matched;
} xregexsplitpart;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Text` | `xstrview` | 文本视图 |
| `Capture` | `size_t` | Capture |
| `Matched` | `bool` | Matched |

### `xregex`

编译对象不可变、可跨线程共享并通过引用计数管理。

```c
typedef struct xregex xregex;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xregexmatcher`

matcher 独占可变执行缓存，捕获视图在下一次匹配前有效。

```c
typedef struct xregexmatcher xregexmatcher;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xregexsplitter`

流式拆分器借用输入，并独占一个可重用 matcher。

```c
typedef struct xregexsplitter xregexsplitter;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xregexset`

编译集合不可变并持有各模式引用。

```c
typedef struct xregexset xregexset;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xregexsetmatcher`

集合 matcher 独占执行缓存和本轮命中索引。

```c
typedef struct xregexsetmatcher xregexsetmatcher;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xregexreplacefn`

自定义替换器只能向输出尾部追加内容，返回 false 表示终止并报告错误。

```c
typedef bool (*xregexreplacefn)(
	const xregexmatcher* pMatcher,
	xstrbuf* pOutput,
	ptr pUserData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XREGEX_PATTERN_DEFAULT` | `(1024u * 1024u)` | 默认限制面向不可信表达式，防止编译阶段无界消耗资源。 |
| `XREGEX_CAPTURES_DEFAULT` | `4096u` | CAPTURES默认值 |

## 模块

- `XRT_MODULE_REGEX`：启用完整正则能力。
- `XRT_MODULE_REGEX_CORE`：字面量转义、编译、元数据、资源预算和结构化错误。
- `XRT_MODULE_REGEX_MATCH`：搜索、指定位置、完整匹配、遍历和捕获；依赖 Unicode 原语处理空匹配推进。
- `XRT_MODULE_REGEX_REPLACE`：模板替换、回调替换和事务式字符串构建；依赖 matcher 与字符串构建器。
- `XRT_MODULE_REGEX_SPLIT`：流式正则拆分与单块复制结果；依赖 matcher 和字符串拆分结果。
- `XRT_MODULE_REGEX_SET`：把多个编译对象合并为一个执行程序，一次返回全部命中的模式索引。

### `xrtRegexEscapeSize`

返回字面量文本转义为正则表达式后的精确字节数，不包含末尾零。

```c
bool xrtRegexEscapeSize(xstrview Text, size_t* pOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 字面量文本 |
| `pOutputSize` | 输出 | 非空 | 接收字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出长度 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 转义查询

```c
	if ( !xrtRegexEscapeSize(SV("a.b*c"), &iSize) ||
		(iSize != 7u) ||
		/* 长度出参必填（与 Escape 的可空不同），容量含末尾零。 */
		!xrtRegexEscapeWrite(SV("a.b*c"), Escape,
			sizeof(Escape), &iSize) ||
		(strcmp(Escape, "a\\.b\\*c") != 0) ) {
```

### `xrtRegexEscapeWrite`

将字面量文本转义到调用方缓冲区；容量必须包含末尾零。

```c
bool xrtRegexEscapeWrite(
	xstrview Text,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 字面量文本 |
| `sOutput` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | 足够 | 容量，须含末尾零 |
| `pOutputSize` | 输出 | 允许空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入并补零 | — |
| `false` | 容量不足或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 转义写入

```c
		!xrtRegexEscapeWrite(SV("a.b*c"), Escape,
			sizeof(Escape), &iSize) ||
```

### `xrtRegexEscape`

创建由 `xrtFree` 释放的零结尾正则字面量，长度输出可以为空。

```c
str xrtRegexEscape(xstrview Text, size_t* pOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 字面量文本 |
| `pOutputSize` | 输出 | 允许空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾转义文本，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[regex](../../examples/text/regex/main.c) · 分配转义

```c
	sLiteral = xrtRegexEscape(XRT_STR_LITERAL("file[1].txt"), &iLiteralSize);
```

### `xrtRegexConfigInit`

初始化默认编译标志和有限资源预算。

```c
void xrtRegexConfigInit(xregexconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 默认配置

```c
	xrtRegexConfigInit(&Config);
```

### `xrtRegexCompile`

使用默认配置编译明确长度的 UTF-8 正则表达式。

```c
xregex* xrtRegexCompile(xstrview Pattern)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Pattern` | 输入 | 借用 | 表达式 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 编译对象（引用 1） | — |
| `NULL` | 编译失败 | `xrt.regex` 域错误 |

#### 错误

- `xrt.regex` / `XREGEX_ERROR_PATTERN`（`XERR_VALUE` / `XERR_RANGE`） — 表达式语法非法；字节位置可由 `xrtRegexErrorOffset` 读取
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `XERR_MEMORY` — 分配失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 默认编译

```c
	pBad = xrtRegexCompile(SV("a(b"));
	if ( (pBad != NULL) ||
		((pError = xrtTakeError()) == NULL) ||
		!xrtRegexErrorOffset(pError, &iOffset) ||
		(iOffset != 3u) ) {
		goto Cleanup;
	}
	xrtErrorFree(pError);

	/* ---- 转义两段式：先量尺寸再写入 ---- */
	if ( !xrtRegexEscapeSize(SV("a.b*c"), &iSize) ||
		(iSize != 7u) ||
```

### `xrtRegexCompileConfig`

使用高级配置编译正则表达式。

```c
xregex* xrtRegexCompileConfig(
	xstrview Pattern,
	const xregexconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Pattern` | 输入 | 借用 | 表达式 |
| `pConfig` | 输入 | 非空 | 编译配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 编译对象（引用 1） | — |
| `NULL` | 编译失败 | `xrt.regex` 域错误 |

#### 错误

- `xrt.regex` / `XREGEX_ERROR_CONFIG`（`XERR_ARGUMENT` / `XERR_VALUE`） — 配置字段非法
- `xrt.regex` / `XREGEX_ERROR_PATTERN`（`XERR_VALUE` / `XERR_RANGE`） — 表达式语法非法；字节位置可由 `xrtRegexErrorOffset` 读取
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `XERR_MEMORY` — 分配失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 高级编译

```c
	pDate = xrtRegexCompileConfig(SV("(?<year>\\d+)-(?<month>\\d+)"),
		&Config);
```

### `xrtRegexValid`

验证表达式能否使用默认配置完成编译。

```c
bool xrtRegexValid(xstrview Pattern)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Pattern` | 输入 | 借用 | 表达式 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 可编译 | — |
| `false` | 不可编译 | `xrt.regex` 域错误 |

#### 错误

- `xrt.regex` / `XREGEX_ERROR_PATTERN`（`XERR_VALUE` / `XERR_RANGE`） — 表达式语法非法；字节位置可由 `xrtRegexErrorOffset` 读取
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 可编译校验

```c
		!xrtRegexValid(SV("\\d+")) ||
```

### `xrtRegexRef`

增加编译对象引用并返回原指针。

```c
xregex* xrtRegexRef(xregex* pRegex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 共享引用

```c
		(xrtRegexRef(pDate) != pDate) ||
```

### `xrtRegexRelease`

释放编译对象引用。

```c
void xrtRegexRelease(xregex* pRegex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 释放引用

```c
		xrtRegexRelease(pComma);
```

### `xrtRegexPattern`

返回编译对象持有的原始表达式视图。

```c
xstrview xrtRegexPattern(const xregex* pRegex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 表达式借用 | — |
| 空视图 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 原始表达式

```c
		(xrtRegexPattern(pDate).Size == 0u) ||
```

### `xrtRegexFlags`

返回编译时使用的标志。

```c
uint32 xrtRegexFlags(const xregex* pRegex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 标志位 | 编译标志 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 编译标志

```c
		(xrtRegexFlags(pDate) != XREGEX_IGNORE_CASE) ||
```

### `xrtRegexCaptureCount`

返回包含组 0 在内的捕获数量。

```c
size_t xrtRegexCaptureCount(const xregex* pRegex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 含组 0 的捕获数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 捕获数量

```c
		(xrtRegexCaptureCount(pDate) != 3u) ) {
```

### `xrtRegexCaptureName`

返回指定捕获的借用名称；未命名捕获返回空视图。

```c
bool xrtRegexCaptureName(
	const xregex* pRegex,
	size_t iIndex,
	xstrview* pName
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |
| `iIndex` | 输入 | < 捕获数量 | 捕获索引 |
| `pName` | 输出 | 非空 | 接收名称视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出（可为空视图） | — |
| `false` | 索引越界 | `XERR_RANGE` |

#### 错误

- `XERR_RANGE` — 捕获索引越界；句柄为空 `XERR_ARGUMENT`

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 捕获名称

```c
		if ( !xrtRegexCaptureName(pDate, 1u, &Name) ||
			(Name.Size != 4u) ||
			(memcmp(Name.Data, "year", 4u) != 0) ||
			(xrtRegexCaptureIndex(pDate, SV("year")) != 1u) ||
			(xrtRegexCaptureIndex(pDate, SV("nope")) !=
				XRT_NPOS) ) {
```

### `xrtRegexCaptureIndex`

按名称查找捕获索引，未找到时返回 `XRT_NPOS`。

```c
size_t xrtRegexCaptureIndex(
	const xregex* pRegex,
	xstrview Name
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |
| `Name` | 输入 | 借用 | 捕获名称 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 索引 | 命中的捕获索引 | — |
| `XRT_NPOS` | 未找到 | 不设错误 |

#### 错误

- 未找到返回 `XRT_NPOS` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 按名查找捕获

```c
			(xrtRegexCaptureIndex(pDate, SV("year")) != 1u) ||
```

### `xrtRegexErrorOffset`

从 `xrt.regex` 错误的机器数据中读取字节位置。

```c
bool xrtRegexErrorOffset(
	const xerror* pError,
	size_t* pOffset
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 非空、`xrt.regex` 域 | 编译/执行错误 |
| `pOffset` | 输出 | 非空 | 接收表达式内字节偏移 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出偏移 | — |
| `false` | 错误不含位置数据 | 不设错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 无位置数据时返回 `false` 且不设置错误

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 错误定位

```c
		!xrtRegexErrorOffset(pError, &iOffset) ||
```

## 编译

`xrtRegexCompile` 使用默认 1 MiB 模式预算和 4096 个捕获预算。`xrtRegexCompileConfig` 允许收紧预算并设置忽略大小写、多行、点匹配换行和非贪婪标志。模式和输入都是明确长度视图，不要求零结尾，并允许嵌入零字节；命名捕获名称是例外，名称不能为空、重复或包含零字节。

编译对象通过 `xrtRegexRef` 和 `xrtRegexRelease` 管理。模块不公开底层 builder、分配器或 clone；共享编译对象不需要克隆。

动态文本不能直接拼入表达式。`xrtRegexEscapeSize` 返回转义后的精确字节数，`xrtRegexEscapeWrite` 写入调用方缓冲区并支持输入、输出同址，`xrtRegexEscape` 返回由 `xrtFree` 释放的独立字符串。转义按字节保留普通内容，只为具有语法含义的 `\\.+*?()|[]{}^$` 增加反斜杠，因此明确长度的二进制文本不会被截断。部分重叠的输入和输出会被拒绝。

```c
size_t iSize;
str sPattern = xrtRegexEscape(XRT_STR_LITERAL("file[1].txt"), &iSize);
xregex* pRegex = sPattern != NULL
	? xrtRegexCompile(xrtStrViewN(sPattern, iSize)) : NULL;

xrtFree(sPattern);
```

## 匹配

所有匹配函数返回 `xregexresult`：

| 值 | 含义 |
| --- | --- |
| `XREGEX_ERROR` | 执行失败，读取 `xrtGetError()` |
| `XREGEX_NONE` | 正常完成但未匹配 |
| `XREGEX_MATCH` | 匹配成功 |

`xrtRegexMatcherFind` 从给定字节位置搜索，`xrtRegexMatcherAt` 要求匹配从该位置开始，`xrtRegexMatcherFull` 要求覆盖完整输入。`xrtRegexMatcherNext` 遍历后续匹配；空匹配按一个合法 UTF-8 标量推进，无效序列按一个字节推进。

捕获范围统一使用半开字节区间 `[Begin, End)`。`xregexcapture.Matched` 区分“捕获未参与”与“捕获了空字符串”。捕获文本和 matcher 输入均为借用视图，在下一次匹配或 matcher 释放前有效。

## 替换

`xrtRegexReplaceTo` 是基础模板入口，把结果追加到已有 `xstrbuf`。`iLimit` 是最大替换次数，`0` 表示不替换，`SIZE_MAX` 表示全部替换。成功时可通过 `pCount` 取得实际次数；失败时，本次调用追加的内容全部撤销，进入函数前的构建器内容保持不变。输入和替换模板允许借用构建器当前有效内容，内部会在首次增长前稳定这些视图。

模板语法刻意保持精简：

| 写法 | 含义 |
| --- | --- |
| `$0` | 整体匹配 |
| `$1`、`$2` | 数字捕获 |
| `${name}` | 命名捕获 |
| `$$` | 字面量美元符号 |

未参与匹配的可选捕获展开为空文本。无效索引、未知名称和不完整美元令牌在开始匹配前报告 `XREGEX_ERROR_REPLACEMENT`，`xrtRegexErrorOffset` 返回模板字节位置。

`xrtRegexReplaceFuncTo` 让回调读取当前 matcher 的全部捕获并直接向输出尾部追加内容，适合大小写转换、编码或数据驱动重写。回调只能追加，不得清空、收缩或接管构建器。回调失败同样撤销本次替换调用的全部输出。

`xrtRegexReplace` 与 `xrtRegexReplaceFirst` 是常见路径的一行式入口，返回由调用方使用 `xrtFree` 释放的零结尾字符串。需要保留嵌入零字节后的精确长度时使用 `xrtRegexReplaceTo` 并读取 `xstrbuf.Size`。

## 拆分

`xrtRegexSplitterCreate` 创建借用输入的流式拆分器，`xrtRegexSplitterNext` 以 `XREGEX_MATCH / XREGEX_NONE / XREGEX_ERROR` 返回字段。普通字段的 `xregexsplitpart.Capture` 为 `XRT_NPOS`；启用 `XREGEX_SPLIT_CAPTURES` 后，每次分隔匹配的捕获按组号紧随字段返回。捕获项用 `Matched` 区分未参与捕获和合法空捕获。

`xregexsplitconfig.Limit` 表示最多使用多少次分隔匹配：`0` 返回完整输入，`SIZE_MAX` 不限制。默认保留首尾及相邻分隔符产生的空字段；`XREGEX_SPLIT_SKIP_EMPTY` 会同时跳过空字段和空捕获。空分隔匹配沿用 matcher 的 UTF-8 标量推进规则，不会在同一字节位置循环。

`xrtRegexSplit` 是默认配置的便捷入口。它复用同一个 splitter 执行计数和写入两遍，返回与 `xrtStrSplit` 相同布局的 `xstrlist`：视图数组、各项零结尾副本和哨兵都位于一个分配块中，使用 `xrtStrListFree` 释放。需要限制、过滤或捕获元数据时使用流式接口。

## 集合匹配

`xrtRegexSetCreate` 接受已有 `xregex`，因此同一集合中的模式可以使用不同编译标志。集合会增加每个模式的引用；创建成功后，调用方可以立即释放原引用。`xrtRegexSetCompile` 和 `xrtRegexSetCompileConfig` 是共享配置的批量编译入口。空集合合法且始终返回 `XREGEX_NONE`。

`xregexset` 与 `xregex` 一样不可变并可跨线程共享。每个执行流使用自己的 `xregexsetmatcher`。`xrtRegexSetMatcherMatch` 从明确字节位置开始搜索，并以升序返回所有能够在剩余输入中命中的模式索引。`xrtRegexSetMatcherCount`、`xrtRegexSetMatcherIndex`、`xrtRegexSetMatcherMatched` 和 `xrtRegexSetMatcherFirst` 只读取最近一轮成功执行的结果，不复制输入或匹配文本。

集合适合规则分类、路由预筛和日志扫描。需要捕获内容时，先用集合取得候选模式，再使用 `xregexmatcher` 执行对应编译对象；这样集合热路径只维护模式索引，不为每个模式保存捕获数组。

### `xrtRegexMatcherCreate`

为一个不可变编译对象创建可复用 matcher。

```c
xregexmatcher* xrtRegexMatcherCreate(xregex* pRegex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | matcher（持有编译对象引用） | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[regex](../../examples/text/regex/main.c) · 创建 matcher

```c
	pMatcher = xrtRegexMatcherCreate(pRegex);
```

### `xrtRegexMatcherFree`

释放 matcher、执行缓存和持有的编译对象引用。

```c
void xrtRegexMatcherFree(xregexmatcher* pMatcher)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[regex](../../examples/text/regex/main.c) · 释放 matcher

```c
			xrtRegexMatcherFree(pMatcher);
```

### `xrtRegexMatcherFind`

从字节位置开始搜索首个匹配。

```c
xregexresult xrtRegexMatcherFind(
	xregexmatcher* pMatcher,
	xstrview Text,
	size_t iStart
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入/输出 | 非空 | 目标 matcher |
| `Text` | 输入 | 借用 | 输入文本 |
| `iStart` | 输入 | <= `Text.Size` | 起始字节偏移 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XREGEX_MATCH` | 命中 | — |
| `XREGEX_NONE` | 无匹配或遍历结束 | 不设错误 |
| `XREGEX_ERROR` | 参数、配置或预算失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败

#### 范例

[regex](../../examples/text/regex/main.c) · 搜索匹配

```c
	for ( xregexresult Result = xrtRegexMatcherFind(pMatcher, Text, 0);
		 Result == XREGEX_MATCH;
		 Result = xrtRegexMatcherNext(pMatcher) ) {
```

### `xrtRegexMatcherAt`

要求首个匹配恰好从指定字节位置开始。

```c
xregexresult xrtRegexMatcherAt(
	xregexmatcher* pMatcher,
	xstrview Text,
	size_t iStart
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入/输出 | 非空 | 目标 matcher |
| `Text` | 输入 | 借用 | 输入文本 |
| `iStart` | 输入 | <= `Text.Size` | 锚定字节偏移 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XREGEX_MATCH` | 命中 | — |
| `XREGEX_NONE` | 无匹配或遍历结束 | 不设错误 |
| `XREGEX_ERROR` | 参数、配置或预算失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 锚定匹配

```c
		xrtRegexMatcherAt(pMatcher, SV("2024-05!"), 0u) ==
```

### `xrtRegexMatcherFull`

要求表达式覆盖完整输入。

```c
xregexresult xrtRegexMatcherFull(
	xregexmatcher* pMatcher,
	xstrview Text
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入/输出 | 非空 | 目标 matcher |
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XREGEX_MATCH` | 命中 | — |
| `XREGEX_NONE` | 无匹配或遍历结束 | 不设错误 |
| `XREGEX_ERROR` | 参数、配置或预算失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 完整匹配

```c
		xrtRegexMatcherFull(pMatcher, SV("2024-05")) ==
```

### `xrtRegexMatcherNext`

继续查找下一项，空匹配会按一个 UTF-8 标量向前推进。

```c
xregexresult xrtRegexMatcherNext(xregexmatcher* pMatcher)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入/输出 | 非空 | 已持有匹配的 matcher |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XREGEX_MATCH` | 命中 | — |
| `XREGEX_NONE` | 无匹配或遍历结束 | 不设错误 |
| `XREGEX_ERROR` | 参数、配置或预算失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败

#### 范例

[regex](../../examples/text/regex/main.c) · 继续查找

```c
		 Result = xrtRegexMatcherNext(pMatcher) ) {
```

### `xrtRegexMatcherMatched`

返回 matcher 当前是否持有一次成功匹配。

```c
bool xrtRegexMatcherMatched(const xregexmatcher* pMatcher)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入 | 非空 | 目标 matcher |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否持有匹配 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 命中查询

```c
		!xrtRegexMatcherMatched(pMatcher) ||
```

### `xrtRegexMatcherText`

返回当前输入的借用视图。

```c
xstrview xrtRegexMatcherText(const xregexmatcher* pMatcher)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入 | 非空 | 目标 matcher |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 当前输入借用 | — |
| 空视图 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 输入视图

```c
		(xrtRegexMatcherText(pMatcher).Size != 13u) ||
```

### `xrtRegexMatcherCapture`

返回指定捕获的参与状态、绝对字节范围和借用文本。

```c
bool xrtRegexMatcherCapture(
	const xregexmatcher* pMatcher,
	size_t iIndex,
	xregexcapture* pCapture
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入 | 非空 | 目标 matcher |
| `iIndex` | 输入 | < 捕获数量 | 捕获索引 |
| `pCapture` | 输出 | 非空 | 接收捕获描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出（未参与时区间为空） | — |
| `false` | 无匹配或索引越界 | `XERR_STATE` / `XERR_RANGE` |

#### 错误

- `XERR_STATE` — 当前无成功匹配
- `XERR_RANGE` — 捕获索引越界
- `XERR_ARGUMENT` — 指针为空

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 按索引捕获

```c
	if ( !xrtRegexMatcherCapture(pMatcher, 0u, &Capture) ) {
```

### `xrtRegexMatcherCaptureNamed`

按名称返回当前捕获。

```c
bool xrtRegexMatcherCaptureNamed(
	const xregexmatcher* pMatcher,
	xstrview Name,
	xregexcapture* pCapture
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入 | 非空 | 目标 matcher |
| `Name` | 输入 | 借用 | 捕获名称 |
| `pCapture` | 输出 | 非空 | 接收捕获描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 无匹配、未命名或不存在 | `XERR_STATE` 或不设错 |

#### 错误

- `XERR_STATE` — 当前无成功匹配；名称不存在返回 `false` 且不设置错误

#### 范例

[regex](../../examples/text/regex/main.c) · 按名捕获

```c
		if ( !xrtRegexMatcherCaptureNamed(pMatcher, XRT_STR_LITERAL("name"), &Name) ||
			 !xrtRegexMatcherCaptureNamed(pMatcher, XRT_STR_LITERAL("value"), &Value) ) {
```

### `xrtRegexTest`

使用临时 matcher 搜索编译表达式。

```c
xregexresult xrtRegexTest(
	xregex* pRegex,
	xstrview Text
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XREGEX_MATCH` | 命中 | — |
| `XREGEX_NONE` | 无匹配或遍历结束 | 不设错误 |
| `XREGEX_ERROR` | 参数、配置或预算失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 临时搜索

```c
	if ( (xrtRegexTest(pDate, SV("say 2024-05")) !=
			XREGEX_MATCH) ||
		(xrtRegexTest(pDate, SV("no digits")) !=
			XREGEX_NONE) ) {
```

### `xrtRegexFullTest`

使用临时 matcher 检查编译表达式是否覆盖完整输入。

```c
xregexresult xrtRegexFullTest(
	xregex* pRegex,
	xstrview Text
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XREGEX_MATCH` | 命中 | — |
| `XREGEX_NONE` | 无匹配或遍历结束 | 不设错误 |
| `XREGEX_ERROR` | 参数、配置或预算失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败

#### 范例

[regex](../../examples/text/regex/main.c) · 临时完整匹配

```c
	if ( xrtRegexFullTest(pRegex, XRT_STR_LITERAL("file[1].txt")) != XREGEX_MATCH ) {
```

### `xrtRegexMatch`

编译默认表达式并执行一次搜索。

```c
xregexresult xrtRegexMatch(
	xstrview Pattern,
	xstrview Text
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Pattern` | 输入 | 借用 | 表达式 |
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XREGEX_MATCH` | 命中 | — |
| `XREGEX_NONE` | 无匹配或遍历结束 | 不设错误 |
| `XREGEX_ERROR` | 参数、配置或预算失败 | `xrt.regex` 域错误 |

#### 错误

- `xrt.regex` / `XREGEX_ERROR_PATTERN`（`XERR_VALUE` / `XERR_RANGE`） — 表达式语法非法；字节位置可由 `xrtRegexErrorOffset` 读取
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `XERR_MEMORY` — 分配失败
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 一次搜索

```c
	if ( (xrtRegexMatch(SV("\\d+"), SV("abc123")) !=
			XREGEX_MATCH) ||
		(xrtRegexMatch(SV("\\d+"), SV("abc")) != XREGEX_NONE) ||
		(xrtRegexFullMatch(SV("\\d+"), SV("123")) !=
			XREGEX_MATCH) ||
		(xrtRegexFullMatch(SV("\\d+"), SV("a123")) !=
			XREGEX_NONE) ) {
```

### `xrtRegexFullMatch`

编译默认表达式并执行一次完整匹配。

```c
xregexresult xrtRegexFullMatch(
	xstrview Pattern,
	xstrview Text
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Pattern` | 输入 | 借用 | 表达式 |
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XREGEX_MATCH` | 命中 | — |
| `XREGEX_NONE` | 无匹配或遍历结束 | 不设错误 |
| `XREGEX_ERROR` | 参数、配置或预算失败 | `xrt.regex` 域错误 |

#### 错误

- `xrt.regex` / `XREGEX_ERROR_PATTERN`（`XERR_VALUE` / `XERR_RANGE`） — 表达式语法非法；字节位置可由 `xrtRegexErrorOffset` 读取
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `XERR_MEMORY` — 分配失败
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 一次完整匹配

```c
		(xrtRegexFullMatch(SV("\\d+"), SV("123")) !=
			XREGEX_MATCH) ||
```

### `xrtRegexReplaceTo`

按模板替换至构建器，`SIZE_MAX` 表示不限制替换次数。

```c
bool xrtRegexReplaceTo(
	xregex* pRegex,
	xstrview Text,
	xstrview Replacement,
	size_t iLimit,
	xstrbuf* pOutput,
	size_t* pCount
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |
| `Text` | 输入 | 借用 | 输入文本 |
| `Replacement` | 输入 | 借用 | 替换模板 |
| `iLimit` | 输入 | `SIZE_MAX` = 全部 | 次数上限 |
| `pOutput` | 输出 | 非空 | 字符串构建器 |
| `pCount` | 输出 | 允许空 | 接收替换次数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已完成追加 | — |
| `false` | 失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_REPLACEMENT`（`XERR_VALUE`） — 模板引用了不存在的捕获
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 模板替换至构建器

```c
	if ( !xrtRegexReplaceTo(pDate, SV("2024-01 2025-02"),
			SV("[y]"), 1u, &Output, &iCount) ||
		(iCount != 1u) ||
		(xrtStrBufView(&Output).Size != 11u) ||
		(memcmp(xrtStrBufView(&Output).Data, "[y] 2025-02",
			11u) != 0) ) {
```

### `xrtRegexReplaceFuncTo`

由回调生成每次替换内容，失败时撤销本次调用追加的全部数据。

```c
bool xrtRegexReplaceFuncTo(
	xregex* pRegex,
	xstrview Text,
	size_t iLimit,
	xregexreplacefn pReplace,
	ptr pUserData,
	xstrbuf* pOutput,
	size_t* pCount
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |
| `Text` | 输入 | 借用 | 输入文本 |
| `iLimit` | 输入 | `SIZE_MAX` = 全部 | 次数上限 |
| `pReplace` | 输入 | 非空 | 替换回调 |
| `pUserData` | 输入 | 任意值 | 回调数据 |
| `pOutput` | 输出 | 非空 | 字符串构建器 |
| `pCount` | 输出 | 允许空 | 接收替换次数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已完成追加 | — |
| `false` | 失败，本次追加已撤销 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_CALLBACK` — 回调失败
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 回调替换至构建器

```c
		!xrtRegexReplaceFuncTo(pWord, SV("aa bb"),
			SIZE_MAX,
			exampleUpperReplace, &iHits, &Output, &iCount) ||
```

### `xrtRegexReplace`

替换全部匹配并返回零结尾独立字符串。

```c
str xrtRegexReplace(
	xregex* pRegex,
	xstrview Text,
	xstrview Replacement
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |
| `Text` | 输入 | 借用 | 输入文本 |
| `Replacement` | 输入 | 借用 | 替换模板 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_REPLACEMENT`（`XERR_VALUE`） — 模板引用了不存在的捕获
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败
- `XERR_MEMORY` — 分配失败

#### 范例

[regex_replace](../../examples/text/regex_replace/main.c) · 替换全部

```c
	sResult = xrtRegexReplace(
		pRegex,
		XRT_STR_LITERAL("width=128 height=72"),
		XRT_STR_LITERAL("${name}: $2")
	);
```

### `xrtRegexReplaceFirst`

只替换第一个匹配并返回零结尾独立字符串。

```c
str xrtRegexReplaceFirst(
	xregex* pRegex,
	xstrview Text,
	xstrview Replacement
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |
| `Text` | 输入 | 借用 | 输入文本 |
| `Replacement` | 输入 | 借用 | 替换模板 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_REPLACEMENT`（`XERR_VALUE`） — 模板引用了不存在的捕获
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败
- `XERR_MEMORY` — 分配失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 替换首个

```c
	sResult = xrtRegexReplaceFirst(pDate, SV("2024-01 2025-02"),
		SV("[x]"));
```

### `xrtRegexSplitConfigInit`

初始化不限制分隔次数且保留空字段的拆分配置。

```c
void xrtRegexSplitConfigInit(xregexsplitconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[regex_split](../../examples/text/regex_split/main.c) · 拆分配置

```c
	xrtRegexSplitConfigInit(&Config);
```

### `xrtRegexSplitterCreate`

创建借用输入的流式正则拆分器。

```c
xregexsplitter* xrtRegexSplitterCreate(
	xregex* pRegex,
	xstrview Text,
	const xregexsplitconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |
| `Text` | 输入 | 借用 | 输入文本 |
| `pConfig` | 输入 | 允许空 | 空 = 默认配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拆分器（持有编译对象引用） | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[regex_split](../../examples/text/regex_split/main.c) · 创建拆分器

```c
	pSplitter = xrtRegexSplitterCreate(
		pRegex,
		XRT_STR_LITERAL("alpha, beta; gamma"),
		&Config
	);
```

### `xrtRegexSplitterFree`

释放拆分器、matcher 和持有的正则引用。

```c
void xrtRegexSplitterFree(xregexsplitter* pSplitter)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSplitter` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[regex_split](../../examples/text/regex_split/main.c) · 释放拆分器

```c
	xrtRegexSplitterFree(pSplitter);
```

### `xrtRegexSplitterNext`

返回下一字段或捕获，`XREGEX_NONE` 表示遍历结束。

```c
xregexresult xrtRegexSplitterNext(
	xregexsplitter* pSplitter,
	xregexsplitpart* pPart
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSplitter` | 输入/输出 | 非空 | 目标拆分器 |
| `pPart` | 输出 | 非空 | 接收字段视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XREGEX_MATCH` | 命中 | — |
| `XREGEX_NONE` | 无匹配或遍历结束 | 不设错误 |
| `XREGEX_ERROR` | 参数、配置或预算失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败

#### 范例

[regex_split](../../examples/text/regex_split/main.c) · 下一字段

```c
	while ( (Result = xrtRegexSplitterNext(pSplitter, &Part)) == XREGEX_MATCH ) {
```

### `xrtRegexSplit`

使用默认配置拆分并返回一个分配块内的零结尾字段。

```c
xstrlist* xrtRegexSplit(
	xregex* pRegex,
	xstrview Text
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegex` | 输入 | 非空 | 目标编译对象 |
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 字段列表，整体一次 `xrtFree` 释放 | — |
| `NULL` | 失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败
- `XERR_MEMORY` — 分配失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 分配拆分

```c
			((pParts = xrtRegexSplit(pComma,
				SV("ab,cd,ef"))) == NULL) ) {
```

### `xrtRegexSetCreate`

从已有编译对象创建集合，各模式可以使用不同标志。

```c
xregexset* xrtRegexSetCreate(
	xregex* const* arrRegex,
	size_t iCount
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `arrRegex` | 输入 | 非空数组 | 编译对象指针数组 |
| `iCount` | 输入 | > 0 | 模式数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 集合（引用 1） | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 从编译对象创建集合

```c
	pSet = xrtRegexSetCreate(arrRegex, 2u);
```

### `xrtRegexSetCompile`

使用默认配置批量编译模式并创建集合。

```c
xregexset* xrtRegexSetCompile(
	const xstrview* arrPattern,
	size_t iCount
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `arrPattern` | 输入 | 非空数组 | 表达式数组 |
| `iCount` | 输入 | > 0 | 模式数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 集合（引用 1） | — |
| `NULL` | 创建失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_PATTERN`（`XERR_VALUE` / `XERR_RANGE`） — 表达式语法非法；字节位置可由 `xrtRegexErrorOffset` 读取
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `XERR_MEMORY` — 分配失败

#### 范例

[regex_set](../../examples/text/regex_set/main.c) · 批量编译集合

```c
	xregexset* pSet = xrtRegexSetCompile(arrPattern, 3u);
```

### `xrtRegexSetCompileConfig`

使用同一高级配置批量编译模式并创建集合。

```c
xregexset* xrtRegexSetCompileConfig(
	const xstrview* arrPattern,
	size_t iCount,
	const xregexconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `arrPattern` | 输入 | 非空数组 | 表达式数组 |
| `iCount` | 输入 | > 0 | 模式数量 |
| `pConfig` | 输入 | 非空 | 编译配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 集合（引用 1） | — |
| `NULL` | 创建失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_CONFIG`（`XERR_ARGUMENT` / `XERR_VALUE`） — 配置字段非法
- `xrt.regex` / `XREGEX_ERROR_PATTERN`（`XERR_VALUE` / `XERR_RANGE`） — 表达式语法非法；字节位置可由 `xrtRegexErrorOffset` 读取
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `XERR_MEMORY` — 分配失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 批量编译集合（高级配置）

```c
		pBroken = xrtRegexSetCompileConfig(Patterns, 2u, &Config);
```

### `xrtRegexSetRef`

增加集合引用并返回原指针。

```c
xregexset* xrtRegexSetRef(xregexset* pSet)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空 | 目标集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 共享引用

```c
		(xrtRegexSetRef(pSet) != pSet) ||
```

### `xrtRegexSetRelease`

释放集合引用。

```c
void xrtRegexSetRelease(xregexset* pSet)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[regex_set](../../examples/text/regex_set/main.c) · 释放引用

```c
	xrtRegexSetRelease(pSet);
```

### `xrtRegexSetCount`

返回集合中的模式数量。

```c
size_t xrtRegexSetCount(const xregexset* pSet)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空 | 目标集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 模式数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 模式数量

```c
		(xrtRegexSetCount(pSet) != 2u) ||
```

### `xrtRegexSetRegex`

返回集合借用的指定编译对象。

```c
const xregex* xrtRegexSetRegex(
	const xregexset* pSet,
	size_t iIndex
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空 | 目标集合 |
| `iIndex` | 输入 | < 模式数量 | 模式索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 编译对象借用 | — |
| `NULL` | 索引越界 | `XERR_RANGE` |

#### 错误

- `XERR_RANGE` — 模式索引越界；句柄为空 `XERR_ARGUMENT`

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 成员借用

```c
		(xrtRegexSetRegex(pSet, 1u) != pWord) ||
```

### `xrtRegexSetErrorIndex`

从批量编译错误中读取失败的模式索引。

```c
bool xrtRegexSetErrorIndex(
	const xerror* pError,
	size_t* pIndex
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 非空、`xrt.regex` 域 | 批量编译错误 |
| `pIndex` | 输出 | 非空 | 接收失败模式索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出索引 | — |
| `false` | 错误不含模式索引 | 不设错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 无模式索引时返回 `false` 且不设置错误

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 失败模式定位

```c
			!xrtRegexSetErrorIndex(pError, &iErrorIndex) ||
```

### `xrtRegexSetMatcherCreate`

为不可变集合创建可复用 matcher。

```c
xregexsetmatcher* xrtRegexSetMatcherCreate(xregexset* pSet)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空 | 目标集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 集合 matcher（持有集合引用） | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[regex_set](../../examples/text/regex_set/main.c) · 创建集合 matcher

```c
	pMatcher = xrtRegexSetMatcherCreate(pSet);
```

### `xrtRegexSetMatcherFree`

释放集合 matcher 及其命中索引。

```c
void xrtRegexSetMatcherFree(xregexsetmatcher* pMatcher)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[regex_set](../../examples/text/regex_set/main.c) · 释放集合 matcher

```c
	xrtRegexSetMatcherFree(pMatcher);
```

### `xrtRegexSetMatcherMatch`

从指定字节位置开始计算所有命中的模式。

```c
xregexresult xrtRegexSetMatcherMatch(
	xregexsetmatcher* pMatcher,
	xstrview Text,
	size_t iStart
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入/输出 | 非空 | 目标集合 matcher |
| `Text` | 输入 | 借用 | 输入文本 |
| `iStart` | 输入 | <= `Text.Size` | 起始字节偏移 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XREGEX_MATCH` | 命中 | — |
| `XREGEX_NONE` | 无匹配或遍历结束 | 不设错误 |
| `XREGEX_ERROR` | 参数、配置或预算失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败

#### 范例

[regex_set](../../examples/text/regex_set/main.c) · 集合匹配

```c
	Result = xrtRegexSetMatcherMatch(
		pMatcher,
		XRT_STR_LITERAL("disk timeout"),
		0
	);
```

### `xrtRegexSetMatcherCount`

返回本轮命中的模式数量。

```c
size_t xrtRegexSetMatcherCount(const xregexsetmatcher* pMatcher)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入 | 非空 | 目标集合 matcher |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 本轮命中数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[regex_set](../../examples/text/regex_set/main.c) · 命中数量

```c
		for ( size_t i = 0; i < xrtRegexSetMatcherCount(pMatcher); i++ ) {
```

### `xrtRegexSetMatcherIndex`

返回本轮第 `iIndex` 个命中的模式索引。

```c
size_t xrtRegexSetMatcherIndex(
	const xregexsetmatcher* pMatcher,
	size_t iIndex
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入 | 非空 | 目标集合 matcher |
| `iIndex` | 输入 | < 命中数量 | 序号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 命中的模式索引 | — |
| `XRT_NPOS` | 序号越界 | 不设错误 |

#### 错误

- 序号越界返回 `XRT_NPOS` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[regex_set](../../examples/text/regex_set/main.c) · 命中索引

```c
			printf("matched rule %zu\n", xrtRegexSetMatcherIndex(pMatcher, i));
```

### `xrtRegexSetMatcherMatched`

判断指定模式是否在本轮命中。

```c
bool xrtRegexSetMatcherMatched(
	const xregexsetmatcher* pMatcher,
	size_t iPattern
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入 | 非空 | 目标集合 matcher |
| `iPattern` | 输入 | < 模式数量 | 模式索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否命中 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 指定模式命中查询

```c
		!xrtRegexSetMatcherMatched(pSetMatcher, 0u) ||
```

### `xrtRegexSetMatcherFirst`

返回本轮最小的命中模式索引，未命中时返回 `XRT_NPOS`。

```c
size_t xrtRegexSetMatcherFirst(const xregexsetmatcher* pMatcher)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMatcher` | 输入 | 非空 | 目标集合 matcher |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 最小命中索引 | — |
| `XRT_NPOS` | 本轮未命中 | 不设错误 |

#### 错误

- 未命中返回 `XRT_NPOS` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 最小命中索引

```c
		(xrtRegexSetMatcherFirst(pSetMatcher) > 1u) ) {
```

### `xrtRegexSetTest`

使用临时 matcher 检查集合中是否有模式命中。

```c
xregexresult xrtRegexSetTest(
	xregexset* pSet,
	xstrview Text
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空 | 目标集合 |
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XREGEX_MATCH` | 命中 | — |
| `XREGEX_NONE` | 无匹配或遍历结束 | 不设错误 |
| `XREGEX_ERROR` | 参数、配置或预算失败 | `xrt.regex` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.regex` / `XREGEX_ERROR_LIMIT`（`XERR_RANGE`） — 超出编译或执行预算
- `xrt.regex` / `XREGEX_ERROR_EXECUTE` — 执行失败

#### 范例

[regex_tour](../../examples/text/regex_tour/main.c) · 临时集合匹配

```c
		(xrtRegexSetTest(pSet, SV("hello 2024-01 world")) !=
			XREGEX_MATCH) ) {
```

## 语法与保证

支持 RE2 风格字符类、Unicode 属性、贪婪与非贪婪量词、命名捕获、行首尾和文本首尾断言，以及表达式内标志。表达式内标志为 `i`、`m`、`s` 和 `U`；引擎始终按 Unicode 模式工作，因此不接受会暗示切换 Unicode 模式的 `u`。字符类末尾的 `-` 按字面量处理，逆序范围属于语法错误。`\b` 和 `\B` 使用 ASCII 单词字符定义。

模块有意不支持反向引用和前后向环视。这些结构无法维持线性时间保证；需要这类语义时，应把正则用于候选定位，再由调用方执行二次检查。

## 错误

模块错误域为 `xrt.regex`。语法错误使用 `XREGEX_ERROR_PATTERN`，资源预算使用 `XREGEX_ERROR_LIMIT`。`xrtRegexErrorOffset` 可读取语法错误的模式字节位置。批量编译的语法、配置和预算错误会保留原始错误作为 cause，`xrtRegexSetErrorIndex` 返回失败模式的零基索引。内存不足不再尝试分配包装错误，而是原样传播稳定的 `XERR_MEMORY`。

## 示例

普通匹配示例位于 `examples/text/regex/main.c`，替换示例位于 `examples/text/regex_replace/main.c`，拆分示例位于 `examples/text/regex_split/main.c`，集合分类示例位于 `examples/text/regex_set/main.c`。
