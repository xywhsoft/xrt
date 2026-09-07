# Pattern

Pattern 是面向大量结构化字节模式的编译式匹配器。它只负责完整字符串
匹配、顺序捕获和模式选择，不包含 HTTP、路由处理函数或正则表达式语义。

## 类型与常量

### `xpatternresult`

未命中不是错误，全部匹配入口使用同一三态结果。

```c
typedef enum xpatternresult {
	XPATTERN_ERROR = -1,
	XPATTERN_NONE = 0,
	XPATTERN_MATCH = 1
} xpatternresult;
```

| 值 | 语义 |
|---|---|
| `XPATTERN_ERROR` | 失败 |
| `XPATTERN_NONE` | 无 |
| `XPATTERN_MATCH` | 命中 |

### `xpatternerror`

pattern 模块错误代码在 xrt.pattern 域内保持稳定。

```c
typedef enum xpatternerror {
	XPATTERN_ERROR_CONFIG = 1601,
	XPATTERN_ERROR_PATTERN,
	XPATTERN_ERROR_LIMIT,
	XPATTERN_ERROR_CONFLICT,
	XPATTERN_ERROR_CAPACITY
} xpatternerror;
```

| 值 | 语义 |
|---|---|
| `XPATTERN_ERROR_CONFIG` | 配置非法 |
| `XPATTERN_ERROR_PATTERN` | 失败 |
| `XPATTERN_ERROR_LIMIT` | 超限 |
| `XPATTERN_ERROR_CONFLICT` | 冲突 |
| `XPATTERN_ERROR_CAPACITY` | 捕获容量不足 |

### `xpatternconfig`

分隔符是字节集合：模式中的分隔字节仍要求精确匹配，捕获不能吞掉 集合中的任意字节。全部模式共享一份配置，以便编译成单一确定性程序。

```c
typedef struct xpatternconfig {
	uint32 Flags;
	xstrview Separators;
	size_t MaxPatternBytes;
	size_t MaxPatterns;
	size_t MaxCaptures;
	size_t MaxStates;
	size_t MaxCompiledBytes;
	uint32 Reserved[4];
} xpatternconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |
| `Separators` | `xstrview` | 分隔符集合 |
| `MaxPatternBytes` | `size_t` | MaxPatternBytes |
| `MaxPatterns` | `size_t` | MaxPatterns |
| `MaxCaptures` | `size_t` | MaxCaptures |
| `MaxStates` | `size_t` | MaxStates |
| `MaxCompiledBytes` | `size_t` | MaxCompiledBytes |

### `xpatternspec`

Value 仅作为借用值随命中返回，XRT 不获取或释放其所有权。

```c
typedef struct xpatternspec {
	xstrview Pattern;
	ptr Value;
	int32 Priority;
	uint32 Flags;
} xpatternspec;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Pattern` | `xstrview` | 模式串 |
| `Value` | `ptr` | 值 |
| `Priority` | `int32` | 优先级 |
| `Flags` | `uint32` | 标志位 |

### `xpatternmatch`

{name} 捕获非空字段，也可写成 prefix{name}suffix；一个字段至多一个普通 捕获，前后缀至少一侧非空。{*name} 仍独占最终字段并可捕获空尾部。 捕获由调用方数组按模式中的出现顺序保存，PatternIndex 属于当前快照。

```c
typedef struct xpatternmatch {
	xpatternid Id;
	size_t PatternIndex;
	ptr Value;
	size_t CaptureCount;
} xpatternmatch;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Id` | `xpatternid` | 标识 |
| `PatternIndex` | `size_t` | PatternIndex |
| `Value` | `ptr` | 值 |
| `CaptureCount` | `size_t` | CaptureCount |

### `xpatternid`

零值永远不是有效的 Builder 条目句柄。

```c
typedef uint64 xpatternid;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xpattern`

编译对象不可变、可跨线程共享并通过引用计数管理。

```c
typedef struct xpattern xpattern;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xpatternbuilder`

Builder 可变且不保证并发安全；成功编译不会清空其中的模式。

```c
typedef struct xpatternbuilder xpatternbuilder;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XPATTERN_PATTERN_DEFAULT` | `(1024u * 1024u)` | 默认预算适用于大量路由，也阻止不可信模式造成无界编译。 |
| `XPATTERN_PATTERNS_DEFAULT` | `100000u` | PATTERNS默认值 |
| `XPATTERN_CAPTURES_DEFAULT` | `256u` | CAPTURES默认值 |
| `XPATTERN_STATES_DEFAULT` | `1000000u` | STATES默认值 |
| `XPATTERN_COMPILED_DEFAULT` | `(512u * 1024u * 1024u)` | COMPILED默认值 |
| `XPATTERN_ID_INVALID` | `((xpatternid)0)` | 标识无效 |

## 模式语法

- 字面字段按字节精确匹配。
- `{name}` 捕获一个非空字段，不能跨越配置中的分隔字节。
- `prefix{name}`、`{name}suffix` 与 `prefix{name}suffix` 捕获同一字段中
  前后缀之间的非空字节；前后缀按字节精确匹配。
- `{*name}` 捕获剩余文本，只能位于模式末尾，并允许空值。
- `{{` 和 `}}` 在字面字段中分别表示 `{` 和 `}`。
- 捕获名必须是 ASCII 标识符，同一模式内不能重名。
- 一个字段至多包含一个普通捕获；`{a}-{b}` 当前是编译错误，尾捕获不能
  与任何字面量混合。
- 匹配始终锚定完整输入，不执行路径规范化、URL 解码或大小写折叠。

默认分隔符为 `/`。`xpatternconfig.Separators` 是分隔字节集合；模式中的
分隔符仍要求精确匹配。设置为 `/.` 后，`/file/{name}.{ext}` 可以分别捕获
文件名和扩展名，但 `/` 与 `.` 不会互相替代。

## 一次性提取

```c
xstrview Captures[2];
size_t iCount;

if ( xrtPatternExtract(
	XRT_STR_LITERAL("/repo/{owner}/{name}"),
	Text,
	Captures,
	2u,
	&iCount
) == XPATTERN_MATCH ) {
	/* Captures[0] 和 Captures[1] 借用 Text。 */
}
```

`xrtPatternExtract` 在模式、参数和捕获容量有效时不分配内存，适合偶发
匹配；错误报告本身仍可能创建 `xerror`。同一模式被重复使用时，应通过
`xrtPatternCompile` 编译一次。

### `xrtPatternConfigInit`

初始化默认的 `"/"` 分隔符、严格语义和有限资源预算。

```c
void xrtPatternConfigInit(xpatternconfig* pConfig)
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

[pattern_tour](../../examples/text/pattern_tour/main.c) · 默认配置

```c
	xrtPatternConfigInit(&Config);
```

### `xrtPatternExtract`

直接解析并完整匹配一条模式；模式、参数和捕获容量有效时不分配内存，捕获视图借用 `Text`。

```c
xpatternresult xrtPatternExtract(
	xstrview Pattern,
	xstrview Text,
	xstrview* arrCapture,
	size_t iCapacity,
	size_t* pCaptureCount
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Pattern` | 输入 | 借用 | 模式表达式 |
| `Text` | 输入 | 借用 | 待匹配文本 |
| `arrCapture` | 输出 | 容量足够时非空 | 捕获视图数组 |
| `iCapacity` | 输入 | — | 捕获容量 |
| `pCaptureCount` | 输出 | 非空 | 接收所需/实际数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XPATTERN_MATCH` | 命中一条模式 | — |
| `XPATTERN_NONE` | 没有模式匹配 | 不设错误 |
| `XPATTERN_ERROR` | 参数、容量或预算失败 | `xrt.pattern` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.pattern` / `XPATTERN_ERROR_PATTERN`（`XERR_VALUE`） — 模式语法非法；字节位置可由 `xrtPatternErrorOffset` 读取
- `xrt.pattern` / `XPATTERN_ERROR_CAPACITY`（`XERR_RANGE`） — 捕获输出容量不足；`pCaptureCount` 写入所需数量，不产生部分捕获

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 一次性匹配

```c
	if ( (xrtPatternExtract(SV("{user}/{host}"),
			SV("jane/example.org"), Captures, 4u,
			&iNeeded) != XPATTERN_MATCH) ||
		(iNeeded != 2u) ||
		(Captures[0].Size != 4u) ||
		(memcmp(Captures[0].Data, "jane", 4u) != 0) ||
		(Captures[1].Size != 11u) ||
		(memcmp(Captures[1].Data, "example.org", 11u) != 0) ) {
```

### `xrtPatternExtractConfig`

使用自定义分隔符和预算执行一次性匹配。

```c
xpatternresult xrtPatternExtractConfig(
	xstrview Pattern,
	xstrview Text,
	const xpatternconfig* pConfig,
	xstrview* arrCapture,
	size_t iCapacity,
	size_t* pCaptureCount
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Pattern` | 输入 | 借用 | 模式表达式 |
| `Text` | 输入 | 借用 | 待匹配文本 |
| `pConfig` | 输入 | 非空 | 分隔符与预算配置 |
| `arrCapture` | 输出 | 容量足够时非空 | 捕获视图数组 |
| `iCapacity` | 输入 | — | 捕获容量 |
| `pCaptureCount` | 输出 | 非空 | 接收所需/实际数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XPATTERN_MATCH` | 命中一条模式 | — |
| `XPATTERN_NONE` | 没有模式匹配 | 不设错误 |
| `XPATTERN_ERROR` | 参数、容量或预算失败 | `xrt.pattern` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.pattern` / `XPATTERN_ERROR_CONFIG`（`XERR_ARGUMENT`） — 配置字段非法、保留字段非零或花括号被配置为分隔符
- `xrt.pattern` / `XPATTERN_ERROR_PATTERN`（`XERR_VALUE`） — 模式语法非法；字节位置可由 `xrtPatternErrorOffset` 读取
- `xrt.pattern` / `XPATTERN_ERROR_CAPACITY`（`XERR_RANGE`） — 捕获输出容量不足；`pCaptureCount` 写入所需数量，不产生部分捕获

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 一次性匹配（自定义配置）

```c
	if ( xrtPatternExtractConfig(SV("{u}@{h}"),
			SV("bob@x.io"), &Config, Captures, 4u,
			&iNeeded) != XPATTERN_MATCH ) {
```

### `xrtPatternErrorOffset`

从 `xrt.pattern` 错误的机器数据中读取模式内字节位置。

```c
bool xrtPatternErrorOffset(
	const xerror* pError,
	size_t* pOffset
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 非空、`xrt.pattern` 域 | 编译/匹配错误 |
| `pOffset` | 输出 | 非空 | 接收模式内字节偏移 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出偏移 | — |
| `false` | 错误不含位置数据 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 无位置数据时返回 `false` 且不设置错误

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 错误定位

```c
			!xrtPatternErrorOffset(pError, &iOffset) ) {
```

### `xrtPatternErrorPattern`

从批量编译错误的机器数据中读取失败模式索引。

```c
bool xrtPatternErrorPattern(
	const xerror* pError,
	size_t* pPatternIndex
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 非空、`xrt.pattern` 域 | 批量编译错误 |
| `pPatternIndex` | 输出 | 非空 | 接收失败模式索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出索引 | — |
| `false` | 错误不含模式索引 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 无模式索引时返回 `false` 且不设置错误

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 失败模式定位

```c
			!xrtPatternErrorPattern(pError, &iBadIndex) ||
```

## 批量编译与匹配

`xrtPatternCompileMany` 将多条模式编译成不可变对象。`xrtPatternLookup`
只返回获胜模式；`xrtPatternMatch` 还按出现顺序写入捕获视图。编译对象没有
可变 matcher 缓存，可由多个线程同时查询。

模式按第一个不同字段决定特异度：字面字段优先于混合捕获，混合捕获优先于
整字段捕获，整字段捕获优先于尾捕获。多个混合捕获同时命中时，固定字节
总数更多者优先；总数相同时，前缀更长者优先。结构无法区分且优先级相同的模式会产生
`XPATTERN_ERROR_CONFLICT`；不同优先级可以显式替换这种同结构模式。优先级
不会令参数模式反超可同时命中的字面模式。

### `xrtPatternCompile`

使用默认配置编译一条可重复匹配的模式。

```c
xpattern* xrtPatternCompile(xstrview Pattern)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Pattern` | 输入 | 借用 | 模式表达式 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 不可变编译对象（引用 1） | — |
| `NULL` | 编译失败 | `xrt.pattern` 域错误 |

#### 错误

- `xrt.pattern` / `XPATTERN_ERROR_PATTERN`（`XERR_VALUE`） — 模式语法非法；字节位置可由 `xrtPatternErrorOffset` 读取
- `xrt.pattern` / `XPATTERN_ERROR_LIMIT`（`XERR_RANGE`） — 超出捕获、状态或编译字节数预算
- `XERR_MEMORY` — 编译或扩容分配失败

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 单条编译

```c
		xpattern* pBad = xrtPatternCompile(SV("{a}{b}"));
```

### `xrtPatternCompileConfig`

使用高级配置编译一条可重复匹配的模式。

```c
xpattern* xrtPatternCompileConfig(
	xstrview Pattern,
	const xpatternconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Pattern` | 输入 | 借用 | 模式表达式 |
| `pConfig` | 输入 | 非空 | 分隔符与预算配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 不可变编译对象（引用 1） | — |
| `NULL` | 编译失败 | `xrt.pattern` 域错误 |

#### 错误

- `xrt.pattern` / `XPATTERN_ERROR_CONFIG`（`XERR_ARGUMENT`） — 配置字段非法、保留字段非零或花括号被配置为分隔符
- `xrt.pattern` / `XPATTERN_ERROR_PATTERN`（`XERR_VALUE`） — 模式语法非法；字节位置可由 `xrtPatternErrorOffset` 读取
- `xrt.pattern` / `XPATTERN_ERROR_LIMIT`（`XERR_RANGE`） — 超出捕获、状态或编译字节数预算
- `XERR_MEMORY` — 编译或扩容分配失败

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 单条编译（自定义配置）

```c
	pSingle = xrtPatternCompileConfig(SV("{user}@{host}"), &Config);
```

### `xrtPatternCompileMany`

使用默认配置把多条模式编译成一个不可变匹配程序。

```c
xpattern* xrtPatternCompileMany(
	const xpatternspec* arrSpec,
	size_t iCount
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `arrSpec` | 输入 | 非空数组 | 模式规格 |
| `iCount` | 输入 | > 0 | 模式数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 不可变编译对象（引用 1） | — |
| `NULL` | 编译失败 | `xrt.pattern` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.pattern` / `XPATTERN_ERROR_PATTERN`（`XERR_VALUE`） — 模式语法非法；字节位置可由 `xrtPatternErrorOffset` 读取
- `xrt.pattern` / `XPATTERN_ERROR_LIMIT`（`XERR_RANGE`） — 超出捕获、状态或编译字节数预算
- `xrt.pattern` / `XPATTERN_ERROR_CONFLICT`（`XERR_EXISTS`） — 不可区分模式具有相同优先级
- `XERR_MEMORY` — 编译或扩容分配失败

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 批量编译

```c
		xpattern* pBadMany = xrtPatternCompileMany(BadSpecs, 2u);
```

### `xrtPatternCompileManyConfig`

使用高级配置把多条模式编译成一个不可变匹配程序。

```c
xpattern* xrtPatternCompileManyConfig(
	const xpatternspec* arrSpec,
	size_t iCount,
	const xpatternconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `arrSpec` | 输入 | 非空数组 | 模式规格 |
| `iCount` | 输入 | > 0 | 模式数量 |
| `pConfig` | 输入 | 非空 | 分隔符与预算配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 不可变编译对象（引用 1） | — |
| `NULL` | 编译失败 | `xrt.pattern` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.pattern` / `XPATTERN_ERROR_CONFIG`（`XERR_ARGUMENT`） — 配置字段非法、保留字段非零或花括号被配置为分隔符
- `xrt.pattern` / `XPATTERN_ERROR_PATTERN`（`XERR_VALUE`） — 模式语法非法；字节位置可由 `xrtPatternErrorOffset` 读取
- `xrt.pattern` / `XPATTERN_ERROR_LIMIT`（`XERR_RANGE`） — 超出捕获、状态或编译字节数预算
- `xrt.pattern` / `XPATTERN_ERROR_CONFLICT`（`XERR_EXISTS`） — 不可区分模式具有相同优先级
- `XERR_MEMORY` — 编译或扩容分配失败

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 批量编译（自定义配置）

```c
	pMulti = xrtPatternCompileManyConfig(Specs, 2u, &Config);
```

### `xrtPatternRef`

增加不可变编译对象引用并返回原指针。

```c
xpattern* xrtPatternRef(xpattern* pPattern)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 共享引用

```c
		(xrtPatternRef(pSingle) != pSingle) ||
```

### `xrtPatternRelease`

释放不可变编译对象引用；归零时释放单块存储。

```c
void xrtPatternRelease(xpattern* pPattern)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[pattern](../../examples/text/pattern/main.c) · 释放引用

```c
	xrtPatternRelease(pPattern);
```

### `xrtPatternCount`

返回编译对象中的模式数量。

```c
size_t xrtPatternCount(const xpattern* pPattern)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 模式数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 模式数量

```c
		(xrtPatternCount(pSingle) != 1u) ||
```

### `xrtPatternCompiledBytes`

返回编译对象实际占用的单块存储字节数。

```c
size_t xrtPatternCompiledBytes(const xpattern* pPattern)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 单块存储字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 存储占用

```c
		(xrtPatternCompiledBytes(pSingle) == 0u) ||
```

### `xrtPatternSeparators`

返回编译对象复制并归一化后的分隔符集合。

```c
xstrview xrtPatternSeparators(const xpattern* pPattern)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 分隔符借用（对象存活期有效） | — |
| 空视图 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 分隔符集合

```c
		(xrtPatternSeparators(pSingle).Size != 2u) ||
```

### `xrtPatternSource`

返回指定模式的原始表达式视图。

```c
xstrview xrtPatternSource(
	const xpattern* pPattern,
	size_t iPattern
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |
| `iPattern` | 输入 | < 模式数量 | 模式索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 表达式借用 | — |
| 空视图 | 索引越界 | `XERR_RANGE` |

#### 错误

- `XERR_RANGE` — 模式索引越界；空句柄 `XERR_ARGUMENT`

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 原始表达式

```c
		(xrtPatternSource(pSingle, 0u).Size != 13u) ||
```

### `xrtPatternId`

返回指定模式的稳定 Builder ID；直接批量编译时 ID 也保持非零。

```c
xpatternid xrtPatternId(
	const xpattern* pPattern,
	size_t iPattern
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |
| `iPattern` | 输入 | < 模式数量 | 模式索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非零 | 稳定 ID | — |
| `0` | 索引越界或句柄为空 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `XERR_RANGE` — 模式索引越界

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 稳定 ID

```c
		(xrtPatternId(pSingle, 0u) == XPATTERN_ID_INVALID) ||
```

### `xrtPatternValue`

返回指定模式携带的借用值。

```c
ptr xrtPatternValue(
	const xpattern* pPattern,
	size_t iPattern
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |
| `iPattern` | 输入 | < 模式数量 | 模式索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 值 | `xpatternspec.Value` 原样返回 | — |
| `NULL` | 未携带或索引越界 | `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `XERR_RANGE` — 模式索引越界

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 携带值

```c
		(xrtPatternValue(pMulti, 0u) != (ptr)"Jane") ||
```

### `xrtPatternCaptureCount`

返回指定模式的捕获数量。

```c
size_t xrtPatternCaptureCount(
	const xpattern* pPattern,
	size_t iPattern
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |
| `iPattern` | 输入 | < 模式数量 | 模式索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 该模式捕获数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 捕获数量

```c
		(xrtPatternCaptureCount(pSingle, 0u) != 2u) ||
```

### `xrtPatternMaxCaptureCount`

返回整个编译对象中单条模式所需的最大捕获数量。

```c
size_t xrtPatternMaxCaptureCount(const xpattern* pPattern)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 最大捕获数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 最大捕获数

```c
		(xrtPatternMaxCaptureCount(pSingle) != 2u) ||
```

### `xrtPatternCaptureName`

返回指定捕获的借用名称。

```c
bool xrtPatternCaptureName(
	const xpattern* pPattern,
	size_t iPattern,
	size_t iCapture,
	xstrview* pName
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |
| `iPattern` | 输入 | < 模式数量 | 模式索引 |
| `iCapture` | 输入 | < 该模式捕获数 | 捕获索引 |
| `pName` | 输出 | 非空 | 接收名称视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出名称 | — |
| `false` | 无名称或索引越界 | 不设错 |

#### 错误

- 无名称或索引越界返回 `false` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 捕获名称

```c
		!xrtPatternCaptureName(pSingle, 0u, 0u, &Captures[2]) ||
```

### `xrtPatternCaptureIndex`

按名称查找捕获索引，未找到时返回 `XRT_NPOS`。

```c
size_t xrtPatternCaptureIndex(
	const xpattern* pPattern,
	size_t iPattern,
	xstrview Name
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |
| `iPattern` | 输入 | < 模式数量 | 模式索引 |
| `Name` | 输入 | 借用 | 捕获名称 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 索引 | 命中的捕获索引 | — |
| `XRT_NPOS` | 未找到 | 不设错 |

#### 错误

- 未找到返回 `XRT_NPOS` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 按名查找捕获

```c
		(xrtPatternCaptureIndex(pSingle, 0u, SV("host")) != 1u) ) {
```

### `xrtPatternLookup`

只选择最优模式，不记录或输出捕获。

```c
xpatternresult xrtPatternLookup(
	const xpattern* pPattern,
	xstrview Text,
	xpatternmatch* pMatch
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |
| `Text` | 输入 | 借用 | 待匹配文本 |
| `pMatch` | 输出 | 允许空 | 接收命中模式信息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XPATTERN_MATCH` | 命中一条模式 | — |
| `XPATTERN_NONE` | 没有模式匹配 | 不设错误 |
| `XPATTERN_ERROR` | 参数、容量或预算失败 | `xrt.pattern` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 最优选择

```c
	if ( (xrtPatternLookup(pMulti, SV("mail/hello"),
			&Match) != XPATTERN_MATCH) ||
		(Match.PatternIndex != 1u) ||
		(Match.Value != (ptr)"Mailbox") ||
		(xrtPatternLookup(pMulti, SV("nothing"),
			&Match) != XPATTERN_NONE) ) {
```

### `xrtPatternMatch`

选择最优模式并按出现顺序输出借用 `Text` 的捕获视图。

```c
xpatternresult xrtPatternMatch(
	const xpattern* pPattern,
	xstrview Text,
	xstrview* arrCapture,
	size_t iCapacity,
	xpatternmatch* pMatch
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |
| `Text` | 输入 | 借用 | 待匹配文本 |
| `arrCapture` | 输出 | 容量足够时非空 | 捕获视图数组 |
| `iCapacity` | 输入 | — | 捕获容量 |
| `pMatch` | 输出 | 允许空 | 接收命中模式信息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XPATTERN_MATCH` | 命中一条模式 | — |
| `XPATTERN_NONE` | 没有模式匹配 | 不设错误 |
| `XPATTERN_ERROR` | 参数、容量或预算失败 | `xrt.pattern` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.pattern` / `XPATTERN_ERROR_CAPACITY`（`XERR_RANGE`） — 捕获输出容量不足；`pCaptureCount` 写入所需数量，不产生部分捕获

#### 范例

[pattern](../../examples/text/pattern/main.c) · 选择并捕获

```c
	if ( xrtPatternMatch(
		pPattern,
		XRT_STR_LITERAL("/users/admin/item-42.json"),
		arrCapture,
		2u,
		&Match
	) == XPATTERN_MATCH ) {
```

### `xrtPatternTest`

只判断是否有模式匹配，不返回模式或捕获。

```c
xpatternresult xrtPatternTest(
	const xpattern* pPattern,
	xstrview Text
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPattern` | 输入 | 非空 | 目标编译对象 |
| `Text` | 输入 | 借用 | 待匹配文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XPATTERN_MATCH` | 命中一条模式 | — |
| `XPATTERN_NONE` | 没有模式匹配 | 不设错误 |
| `XPATTERN_ERROR` | 参数、容量或预算失败 | `xrt.pattern` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 存在性判断

```c
	if ( (xrtPatternTest(pMulti, SV("a@b")) != XPATTERN_MATCH) ||
		(xrtPatternTest(pMulti, SV("-")) != XPATTERN_NONE) ) {
```

## 动态模式

`xpatternbuilder` 复制并缓存模式解析结果。`Add`、`Set`、`Remove` 只修改
Builder；`xrtPatternBuilderCompile` 生成新的不可变快照。旧快照不会观察到
后续修改，因而可以由上层使用 generation 或 RCU 风格生命周期安全发布。

Builder 不保证并发安全。未修改的 Builder 再次编译会返回缓存快照的新
引用；批量追加具有事务性。

### `xrtPatternBuilderCreate`

使用默认配置创建空 Builder。

```c
xpatternbuilder* xrtPatternBuilderCreate(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Builder | — |
| `NULL` | 分配失败 | `XERR_MEMORY` |

#### 错误

- `XERR_MEMORY` — 编译或扩容分配失败

#### 范例

[pattern](../../examples/text/pattern/main.c) · 创建 Builder

```c
	xpatternbuilder* pBuilder = xrtPatternBuilderCreate();
```

### `xrtPatternBuilderCreateConfig`

使用高级配置创建空 Builder；分隔符会立即被复制。

```c
xpatternbuilder* xrtPatternBuilderCreateConfig(
	const xpatternconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空 | 分隔符与预算配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Builder | — |
| `NULL` | 失败 | `xrt.pattern` 域错误 |

#### 错误

- `xrt.pattern` / `XPATTERN_ERROR_CONFIG`（`XERR_ARGUMENT`） — 配置字段非法、保留字段非零或花括号被配置为分隔符
- `XERR_MEMORY` — 编译或扩容分配失败

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 创建 Builder（自定义配置）

```c
	pBuilder = xrtPatternBuilderCreateConfig(&Config);
```

### `xrtPatternBuilderFree`

释放 Builder、其中复制的模式以及内部缓存的编译快照。

```c
void xrtPatternBuilderFree(xpatternbuilder* pBuilder)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuilder` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[pattern](../../examples/text/pattern/main.c) · 释放 Builder

```c
		xrtPatternBuilderFree(pBuilder);
```

### `xrtPatternBuilderClear`

清空全部条目并使已有 ID 失效，同时保留已分配槽容量。

```c
void xrtPatternBuilderClear(xpatternbuilder* pBuilder)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuilder` | 输入 | 非空 | 目标 Builder |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已清空，版本递增 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 清空条目

```c
	xrtPatternBuilderClear(pBuilder);
```

### `xrtPatternBuilderReserve`

保证 Builder 至少可保存指定数量的活动条目。

```c
bool xrtPatternBuilderReserve(
	xpatternbuilder* pBuilder,
	size_t iCapacity
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuilder` | 输入 | 非空 | 目标 Builder |
| `iCapacity` | 输入 | — | 期望容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 容量已保证 | — |
| `false` | 扩容失败 | `XERR_MEMORY` / `XERR_OVERFLOW` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 槽数组扩容失败
- `XERR_OVERFLOW` — 容量引起尺寸溢出

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 预留容量

```c
		!xrtPatternBuilderReserve(pBuilder, 8u) ||
```

### `xrtPatternBuilderCount`

返回 Builder 中的活动条目数量。

```c
size_t xrtPatternBuilderCount(const xpatternbuilder* pBuilder)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuilder` | 输入 | 非空 | 目标 Builder |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 活动条目数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 条目数量

```c
		(xrtPatternBuilderCount(pBuilder) != 1u) ||
```

### `xrtPatternBuilderVersion`

返回每次成功结构修改后递增的版本。

```c
uint64 xrtPatternBuilderVersion(const xpatternbuilder* pBuilder)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuilder` | 输入 | 非空 | 目标 Builder |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 版本号 | 单调递增的结构版本 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 结构版本

```c
	iVersion = xrtPatternBuilderVersion(pBuilder);
```

### `xrtPatternBuilderDirty`

判断 Builder 是否存在尚未成功编译的结构修改。

```c
bool xrtPatternBuilderDirty(const xpatternbuilder* pBuilder)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuilder` | 输入 | 非空 | 目标 Builder |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否有未编译修改 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 未编译修改

```c
		!xrtPatternBuilderDirty(pBuilder) ) {
```

### `xrtPatternBuilderAdd`

复制并追加一条模式，返回稳定代际 ID。

```c
xpatternid xrtPatternBuilderAdd(
	xpatternbuilder* pBuilder,
	const xpatternspec* pSpec
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuilder` | 输入/输出 | 非空 | 目标 Builder |
| `pSpec` | 输入 | 非空 | 模式规格 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非零 | 稳定代际 ID | — |
| `0` | 追加失败 | `xrt.pattern` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.pattern` / `XPATTERN_ERROR_PATTERN`（`XERR_VALUE`） — 模式语法非法；字节位置可由 `xrtPatternErrorOffset` 读取
- `xrt.pattern` / `XPATTERN_ERROR_LIMIT`（`XERR_RANGE`） — 超出捕获、状态或编译字节数预算
- `XERR_MEMORY` — 编译或扩容分配失败

#### 范例

[pattern](../../examples/text/pattern/main.c) · 追加模式

```c
		 (xrtPatternBuilderAdd(pBuilder, &Spec) == XPATTERN_ID_INVALID) ) {
```

### `xrtPatternBuilderAddMany`

原子追加一批模式；任意一条失败时 Builder 保持不变。

```c
bool xrtPatternBuilderAddMany(
	xpatternbuilder* pBuilder,
	const xpatternspec* arrSpec,
	size_t iCount,
	xpatternid* arrId
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuilder` | 输入/输出 | 非空 | 目标 Builder |
| `arrSpec` | 输入 | 非空数组 | 模式规格数组 |
| `iCount` | 输入 | > 0 | 追加数量 |
| `arrId` | 输出 | 允许空 | 接收各条 ID |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 全部追加成功 | — |
| `false` | 任一条失败，Builder 保持不变 | `xrt.pattern` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.pattern` / `XPATTERN_ERROR_PATTERN`（`XERR_VALUE`） — 模式语法非法；字节位置可由 `xrtPatternErrorOffset` 读取
- `xrt.pattern` / `XPATTERN_ERROR_LIMIT`（`XERR_RANGE`） — 超出捕获、状态或编译字节数预算
- `XERR_MEMORY` — 编译或扩容分配失败

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 批量追加

```c
		if ( !xrtPatternBuilderAddMany(pBuilder, More, 2u,
				Ids) ||
			(Ids[0] == XPATTERN_ID_INVALID) ||
			(Ids[1] == XPATTERN_ID_INVALID) ||
			(xrtPatternBuilderCount(pBuilder) != 3u) ) {
```

### `xrtPatternBuilderSet`

替换有效 ID 的模式和值，同时保留 ID 与原始注册顺序。

```c
bool xrtPatternBuilderSet(
	xpatternbuilder* pBuilder,
	xpatternid Id,
	const xpatternspec* pSpec
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuilder` | 输入/输出 | 非空 | 目标 Builder |
| `Id` | 输入 | 有效 ID | 目标条目 |
| `pSpec` | 输入 | 非空 | 新模式规格 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已替换，版本递增 | — |
| `false` | ID 陈旧或规格失败 | `xrt.pattern` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- ID 不存在或已因清空失效返回 `false` 且不设置错误
- `xrt.pattern` / `XPATTERN_ERROR_PATTERN`（`XERR_VALUE`） — 模式语法非法；字节位置可由 `xrtPatternErrorOffset` 读取
- `xrt.pattern` / `XPATTERN_ERROR_LIMIT`（`XERR_RANGE`） — 超出捕获、状态或编译字节数预算
- `XERR_MEMORY` — 编译或扩容分配失败

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 替换模式

```c
	if ( !xrtPatternBuilderSet(pBuilder, IdUser, &Extra) ||
		!xrtPatternBuilderRemove(pBuilder, Ids[1]) ||
		(xrtPatternBuilderCount(pBuilder) != 2u) ||
		xrtPatternBuilderRemove(pBuilder, Ids[1]) ) {
```

### `xrtPatternBuilderRemove`

删除有效 ID；不存在或陈旧 ID 返回 `false`，但不属于执行错误。

```c
bool xrtPatternBuilderRemove(
	xpatternbuilder* pBuilder,
	xpatternid Id
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuilder` | 输入/输出 | 非空 | 目标 Builder |
| `Id` | 输入 | 有效 ID | 目标条目 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除，版本递增 | — |
| `false` | ID 不存在或陈旧 | 不设错误 |

#### 错误

- 不存在或陈旧 ID 返回 `false` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[pattern_tour](../../examples/text/pattern_tour/main.c) · 删除模式

```c
		!xrtPatternBuilderRemove(pBuilder, Ids[1]) ||
```

### `xrtPatternBuilderCompile`

编译当前版本并返回新引用；无修改时复用缓存，失败不影响上一个快照，也不会撤销等待修正的修改。

```c
xpattern* xrtPatternBuilderCompile(xpatternbuilder* pBuilder)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuilder` | 输入 | 非空 | 目标 Builder |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 当前版本的编译快照（新引用） | — |
| `NULL` | 编译失败 | `xrt.pattern` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.pattern` / `XPATTERN_ERROR_PATTERN`（`XERR_VALUE`） — 模式语法非法；字节位置可由 `xrtPatternErrorOffset` 读取
- `xrt.pattern` / `XPATTERN_ERROR_LIMIT`（`XERR_RANGE`） — 超出捕获、状态或编译字节数预算
- `xrt.pattern` / `XPATTERN_ERROR_CONFLICT`（`XERR_EXISTS`） — 不可区分模式具有相同优先级
- `XERR_MEMORY` — 编译或扩容分配失败

#### 范例

[pattern](../../examples/text/pattern/main.c) · 编译快照

```c
	pPattern = xrtPatternBuilderCompile(pBuilder);
```

## 所有权

- 编译对象通过 `xrtPatternRef`、`xrtPatternRelease` 管理。
- Builder 通过 `xrtPatternBuilderFree` 释放。
- 模式和捕获名由 Builder/编译对象复制。
- `xpatternspec.Value` 不转移所有权。
- 捕获 `xstrview` 只在输入文本仍然有效时有效。

完整函数列表见 [Pattern API reference](pattern-reference.md)。
