# 字符串 API

## 设计契约

字符串基础层只处理字节，不隐式验证、解码或修改 UTF-8。这样同一套 API 可以安全处理 UTF-8、协议字段和包含 `\0` 的二进制片段，也避免字节偏移与字符偏移混用。Unicode 验证、码点遍历、大小写折叠和规范化属于字符集模块。

`xstrview` 是 core 提供的零分配借用类型，`xstrbuf` 是字符串模块提供的可增长独占构建器，返回 `str` 的函数创建独立零结尾字符串。调用方可以按性能和使用手感选择层级，不需要先构造重量级字符串对象。

启用方式：

- `XRT_FEATURE_STRING`：视图构造、查找、变换、独立字符串和构建器；`xstrview` 类型与 `XRT_STR_LITERAL` 本身始终可用。
- `XRT_FEATURE_STRING_SPLIT`：拆分与行迭代器，依赖 `XRT_FEATURE_STRING`。
- `XRT_FEATURE_STRING_FORMAT`：`printf` 格式化，依赖 `XRT_FEATURE_STRING`。
- `XRT_FEATURE_STRING_GLOB`：严格 UTF-8 通配匹配，依赖 `XRT_FEATURE_STRING` 与 `XRT_FEATURE_UNICODE`。

## 类型与所有权

### `xstrview`

```c
typedef struct xstrview {
	cstr Data;
	size_t Size;
} xstrview;
```

视图借用 `[Data, Data + Size)`，不拥有内存，也不保证 `Data[Size]` 为零。`Size == 0` 时允许 `Data == NULL`；`Size != 0` 时 `Data == NULL` 是无效视图并产生 `XERR_ARGUMENT`。只要视图仍在使用，源数据就必须保持有效且不得移动。

`XRT_STR_LITERAL("text")` 在编译期创建不含末尾零字节的视图。`XRT_NPOS` 表示未找到，也可以作为“直到结尾”的长度传给 `xrtStrSlice`。

### `xstrbuf`

```c
typedef struct xstrbuf {
	str Data;
	size_t Size;
	size_t Capacity;
} xstrbuf;
```

构建器拥有 `Data`。有效状态始终满足 `Size <= Capacity`；`Data != NULL` 时始终满足 `Data[Size] == 0`。内容仍可包含内嵌零字节，真实长度以 `Size` 为准。

构建器必须先由 `xrtStrBufInit` 初始化。增长可能移动 `Data`，外部借用视图随之失效；追加当前构建器有效内容中的子视图是受支持的。失败不会改变逻辑内容，调用方可以检查错误后重试或释放构建器。

### `xstrsplit`、`xstrlines`、`xstrfields` 与 `xstrlist`

`xstrsplit`、`xstrlines` 和 `xstrfields` 是零分配迭代器，返回的片段借用输入数据。迭代器必须通过对应的 `Init` 函数初始化，公开字段只用于栈上存储，不应由调用方修改。`xstrfields` 跳过连续 ASCII 空白且不返回空字段。`xstrlist` 是便捷结果；结构、视图数组和所有零结尾片段位于同一个分配块中，只需调用一次 `xrtStrListFree`。

## 视图函数

### `xrtStrView`

从零结尾字符串创建借用视图，空指针视为空字符串。

```c
xstrview xrtStrView(cstr sText)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sText` | 输入 | 允许空 | 零结尾字符串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 借用视图；空指针为空视图 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[path/basic](../../examples/path/basic/main.c) · 视图创建

```c
	if ( !xrtPathIterInit(&Iterator, xrtStrView(sJoined), XPATH_NATIVE) ) {
```

### `xrtStrViewN`

从明确长度创建借用视图。

```c
xstrview xrtStrViewN(cstr sText, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sText` | 输入 | — | 字符串起点 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 借用视图 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[text/regex](../../examples/text/regex/main.c) · 定长视图

```c
	pRegex = xrtRegexCompile(xrtStrViewN(sLiteral, iLiteralSize));
```

### `xrtStrEmpty`

判断字符串视图是否为空。

```c
bool xrtStrEmpty(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否为空 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/compare](../../examples/string/compare/main.c) · 空判断

```c
		xrtStrEmpty(SV("")) ? 1 : 0);
```

### `xrtStrBlank`

判断字符串是否只包含 ASCII 空白。

```c
bool xrtStrBlank(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否全空白 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/compare](../../examples/string/compare/main.c) · 空白判断

```c
		xrtStrBlank(SV("  \t ")) ? 1 : 0,
```

### `xrtStrEqual`

判断两个字符串视图是否完全相等。

```c
bool xrtStrEqual(xstrview Left, xstrview Right)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 左视图 |
| `Right` | 输入 | 借用 | 右视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否相等 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[compare](../../examples/string/compare/main.c) · 相等判断

```c
		xrtStrEqual(Hello, HelloUp) ? 1 : 0,
```

### `xrtStrCaseEqual`

按 ASCII 大小写不敏感规则判断相等。

```c
bool xrtStrCaseEqual(xstrview Left, xstrview Right)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 左视图 |
| `Right` | 输入 | 借用 | 右视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否相等 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/compare](../../examples/string/compare/main.c) · 大小写不敏感相等

```c
		xrtStrCaseEqual(Hello, HelloUp) ? 1 : 0);
```

### `xrtStrCompare`

按无符号字节进行词典序比较。

```c
int xrtStrCompare(xstrview Left, xstrview Right)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 左视图 |
| `Right` | 输入 | 借用 | 右视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `< 0` / `0` / `> 0` | 比较结果 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[compare](../../examples/string/compare/main.c) · 词典序比较

```c
	printf("cmp=%d\n", xrtStrCompare(Hello, HelloUp) > 0 ? 1 : -1);
```

### `xrtStrCaseCompare`

按 ASCII 大小写不敏感规则进行词典序比较。

```c
int xrtStrCaseCompare(xstrview Left, xstrview Right)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 左视图 |
| `Right` | 输入 | 借用 | 右视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `< 0` / `0` / `> 0` | 比较结果 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/compare](../../examples/string/compare/main.c) · 大小写不敏感比较

```c
		xrtStrCaseCompare(Hello, HelloUp) == 0 ? 1 : -1);
```

### `xrtStrFind`

从指定字节位置查找子串，未找到返回 `XRT_NPOS`。

```c
size_t xrtStrFind(xstrview Text, xstrview Part, size_t iStart)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Part` | 输入 | 借用 | 子串 |
| `iStart` | 输入 | — | 起始字节位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字节位置 | 首个匹配位置 | — |
| `XRT_NPOS` | 未找到 | 不设错误 |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[find](../../examples/string/find/main.c) · 查找子串

```c
		(int)xrtStrFind(Path, SV("archive"), 0u),
```

### `xrtStrRFind`

从右侧查找最后一个子串，未找到返回 `XRT_NPOS`。

```c
size_t xrtStrRFind(xstrview Text, xstrview Part)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Part` | 输入 | 借用 | 子串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字节位置 | 首个匹配位置 | — |
| `XRT_NPOS` | 未找到 | 不设错误 |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[find](../../examples/string/find/main.c) · 右查找子串

```c
		(int)xrtStrRFind(Path, SV(".")),
```

### `xrtStrCaseFind`

按 ASCII 大小写不敏感规则查找子串。

```c
size_t xrtStrCaseFind(xstrview Text, xstrview Part, size_t iStart)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Part` | 输入 | 借用 | 子串 |
| `iStart` | 输入 | — | 起始字节位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字节位置 | 首个匹配位置 | — |
| `XRT_NPOS` | 未找到 | 不设错误 |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/find](../../examples/string/find/main.c) · 大小写不敏感查找

```c
		(int)xrtStrCaseFind(SV("Xrt-Core"), SV("xrt"), 0u),
```

### `xrtStrCaseRFind`

按 ASCII 大小写不敏感规则从右侧查找最后一个子串。

```c
size_t xrtStrCaseRFind(xstrview Text, xstrview Part)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Part` | 输入 | 借用 | 子串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字节位置 | 首个匹配位置 | — |
| `XRT_NPOS` | 未找到 | 不设错误 |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/find](../../examples/string/find/main.c) · 大小写不敏感右查找

```c
	(int)xrtStrCaseRFind(SV("a.Tar.GZ"), SV(".gz")),
```

### `xrtStrFindByte`

从指定字节位置查找单个字节。

```c
size_t xrtStrFindByte(xstrview Text, unsigned char iByte, size_t iStart)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `iByte` | 输入 | — | 目标字节 |
| `iStart` | 输入 | — | 起始字节位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字节位置 | 首个匹配位置 | — |
| `XRT_NPOS` | 未找到 | 不设错误 |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[find](../../examples/string/find/main.c) · 查找字节

```c
		(int)xrtStrFindByte(SV("a/b/c"), '/', 2u),
```

### `xrtStrFindAny`

从指定字节位置查找属于集合的首个字节。

```c
size_t xrtStrFindAny(xstrview Text, xstrview Set, size_t iStart)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Set` | 输入 | 借用 | 字节集合 |
| `iStart` | 输入 | — | 起始字节位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字节位置 | 首个匹配位置 | — |
| `XRT_NPOS` | 未找到 | 不设错误 |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[find](../../examples/string/find/main.c) · 查找集合字节

```c
		(int)xrtStrFindAny(SV("host:8080"), SV(": /"), 0u));
```

### `xrtStrContains`

判断字符串是否包含指定子串。

```c
bool xrtStrContains(xstrview Text, xstrview Part)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Part` | 输入 | 借用 | 子串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否包含 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/compare](../../examples/string/compare/main.c) · 包含判断

```c
		xrtStrContains(Hello, SV("lo X")) ? 1 : 0,
```

### `xrtStrCaseContains`

按 ASCII 大小写不敏感规则判断是否包含子串。

```c
bool xrtStrCaseContains(xstrview Text, xstrview Part)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Part` | 输入 | 借用 | 子串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否包含 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/compare](../../examples/string/compare/main.c) · 大小写不敏感包含

```c
		xrtStrCaseContains(SV("Config"), SV("FIG")) ? 1 : 0,
```

### `xrtStrContainsAny`

判断字符串是否包含集合中的任意字节。

```c
bool xrtStrContainsAny(xstrview Text, xstrview Set)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Set` | 输入 | 借用 | 字节集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否包含 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/compare](../../examples/string/compare/main.c) · 包含集合字节

```c
		xrtStrContainsAny(SV("host:8080"), SV(";:/")) ? 1 : 0);
```

### `xrtStrCount`

统计不重叠子串数量，空子串返回零。

```c
size_t xrtStrCount(xstrview Text, xstrview Part)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Part` | 输入 | 借用 | 子串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 不重叠出现次数 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/compare](../../examples/string/compare/main.c) · 计数

```c
		(int)xrtStrCount(SV("ab aB ab"), SV("ab")),
```

### `xrtStrCaseCount`

按 ASCII 大小写不敏感规则统计不重叠子串数量。

```c
size_t xrtStrCaseCount(xstrview Text, xstrview Part)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Part` | 输入 | 借用 | 子串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 不重叠出现次数 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/compare](../../examples/string/compare/main.c) · 大小写不敏感计数

```c
		(int)xrtStrCaseCount(SV("Ab aB ab"), SV("ab")));
```

### `xrtStrStarts`

判断字符串是否以指定子串开始。

```c
bool xrtStrStarts(xstrview Text, xstrview Part)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Part` | 输入 | 借用 | 子串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否前缀 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/compare](../../examples/string/compare/main.c) · 前缀判断

```c
		xrtStrStarts(HelloUp, SV("HELLO")) ? 1 : 0,
```

### `xrtStrEnds`

判断字符串是否以指定子串结束。

```c
bool xrtStrEnds(xstrview Text, xstrview Part)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Part` | 输入 | 借用 | 子串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否后缀 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/compare](../../examples/string/compare/main.c) · 后缀判断

```c
		xrtStrEnds(Hello, SV("XRT")) ? 1 : 0);
```

### `xrtStrCaseStarts`

按 ASCII 大小写不敏感规则判断是否以子串开始。

```c
bool xrtStrCaseStarts(xstrview Text, xstrview Part)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Part` | 输入 | 借用 | 子串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否前缀 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/compare](../../examples/string/compare/main.c) · 大小写不敏感前缀

```c
		xrtStrCaseStarts(Hello, SV("hello")) ? 1 : 0,
```

### `xrtStrCaseEnds`

按 ASCII 大小写不敏感规则判断是否以子串结束。

```c
bool xrtStrCaseEnds(xstrview Text, xstrview Part)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Part` | 输入 | 借用 | 子串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否后缀 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/compare](../../examples/string/compare/main.c) · 大小写不敏感后缀

```c
		xrtStrCaseEnds(Hello, SV("xrt")) ? 1 : 0,
```

### `xrtStrCut`

围绕首个分隔符切分借用视图，未找到时 Before 返回完整输入。

```c
bool xrtStrCut(xstrview Text, xstrview Separator, xstrview* pBefore, xstrview* pAfter)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Separator` | 输入 | 借用 | 分隔符 |
| `pBefore` | 输出 | 非空 | 接收借用视图（分隔符前） |
| `pAfter` | 输出 | 非空 | 接收借用视图（分隔符后） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 找到分隔符并已切分 | — |
| `false` | 未找到，Before = 完整输入 | 不设错误 |

#### 错误

- 未找到分隔符返回 `false` 且不设置错误；指针为空 `XERR_ARGUMENT`

#### 范例

[string/basic](../../examples/string/basic/main.c) · 首个分隔切分

```c
	if ( !xrtStrCut(Text, XRT_STR_LITERAL("/"), &Name, NULL) ||
		 !xrtStrFilterTo(Name, XRT_STR_LITERAL("_-"), arrName,
			sizeof(arrName), &iNameSize) ) {
```

### `xrtStrRCut`

围绕最后一个分隔符切分借用视图，未找到时 Before 返回完整输入。

```c
bool xrtStrRCut(xstrview Text, xstrview Separator, xstrview* pBefore, xstrview* pAfter)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Separator` | 输入 | 借用 | 分隔符 |
| `pBefore` | 输出 | 非空 | 接收借用视图（分隔符前） |
| `pAfter` | 输出 | 非空 | 接收借用视图（分隔符后） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 找到分隔符并已切分 | — |
| `false` | 未找到，Before = 完整输入 | 不设错误 |

#### 错误

- 未找到分隔符返回 `false` 且不设置错误；指针为空 `XERR_ARGUMENT`

#### 范例

[string/find](../../examples/string/find/main.c) · 最后分隔切分

```c
	(void)xrtStrRCut(SV("report.final.txt"), SV("."), NULL, &Name);
```

### `xrtStrCutPrefix`

删除匹配的前缀并返回剩余借用视图。

```c
bool xrtStrCutPrefix(xstrview Text, xstrview Prefix, xstrview* pRest)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Prefix` | 输入 | 借用 | 前缀 |
| `pRest` | 输出 | 非空 | 接收借用视图（剩余） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 前缀匹配且已写出剩余 | — |
| `false` | 前缀不匹配 | 不设错误 |

#### 错误

- 前缀不匹配返回 `false` 且不设置错误；指针为空 `XERR_ARGUMENT`

#### 范例

[string/find](../../examples/string/find/main.c) · 删前缀

```c
	(void)xrtStrCutPrefix(Path, SV("/tmp/"), &Rest);
```

### `xrtStrCutSuffix`

删除匹配的后缀并返回剩余借用视图。

```c
bool xrtStrCutSuffix(xstrview Text, xstrview Suffix, xstrview* pRest)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Suffix` | 输入 | 借用 | 后缀 |
| `pRest` | 输出 | 非空 | 接收借用视图（剩余） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 后缀匹配且已写出剩余 | — |
| `false` | 后缀不匹配 | 不设错误 |

#### 错误

- 后缀不匹配返回 `false` 且不设置错误；指针为空 `XERR_ARGUMENT`

#### 范例

[string/find](../../examples/string/find/main.c) · 删后缀

```c
	(void)xrtStrCutSuffix(Rest, SV(".tar.gz"), &Rest);
```

### `xrtStrSlice`

按字节截取借用视图，范围会钳制到源字符串。

```c
xstrview xrtStrSlice(xstrview Text, size_t iStart, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `iStart` | 输入 | — | 起始字节位置 |
| `iCount` | 输入 | — | 截取字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 截取结果（范围已钳制） | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/edit](../../examples/string/edit/main.c) · 截取

```c
	xstrview Part = xrtStrSlice(SV("abcd"), 1u, 2u);
```

### `xrtStrTrimLeft`

删除左侧 ASCII 空白并返回借用视图。

```c
xstrview xrtStrTrimLeft(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 去除左侧空白后的借用 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[pad_trim](../../examples/string/pad_trim/main.c) · 删左空白

```c
	xstrview Trimmed = xrtStrTrimRight(xrtStrTrimLeft(SV("  mid  ")));
```

### `xrtStrTrimRight`

删除右侧 ASCII 空白并返回借用视图。

```c
xstrview xrtStrTrimRight(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 去除右侧空白后的借用 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[pad_trim](../../examples/string/pad_trim/main.c) · 删右空白

```c
	xstrview Trimmed = xrtStrTrimRight(xrtStrTrimLeft(SV("  mid  ")));
```

### `xrtStrTrim`

删除两侧 ASCII 空白并返回借用视图。

```c
xstrview xrtStrTrim(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 去除两侧空白后的借用 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[string/basic](../../examples/string/basic/main.c) · 删两侧空白

```c
	xstrview Text = xrtStrTrim(XRT_STR_LITERAL("  alpha/beta  "));
```

### `xrtStrTrimLeftSet`

删除左侧属于指定字节集合的内容。

```c
xstrview xrtStrTrimLeftSet(xstrview Text, xstrview Set)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Set` | 输入 | 借用 | 字节集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 去除后的借用 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[pad_trim](../../examples/string/pad_trim/main.c) · 删左集合

```c
	xstrview Hex = xrtStrTrimLeftSet(SV("0xFF00"), SV("0x"));
```

### `xrtStrTrimRightSet`

删除右侧属于指定字节集合的内容。

```c
xstrview xrtStrTrimRightSet(xstrview Text, xstrview Set)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Set` | 输入 | 借用 | 字节集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 去除后的借用 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[pad_trim](../../examples/string/pad_trim/main.c) · 删右集合

```c
	xstrview Num = xrtStrTrimRightSet(SV("42 ms"), SV(" ms"));
```

### `xrtStrTrimSet`

删除两侧属于指定字节集合的内容。

```c
xstrview xrtStrTrimSet(xstrview Text, xstrview Set)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Set` | 输入 | 借用 | 字节集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 去除后的借用 | — |

#### 错误

- 无 — 纯借用操作，不设置错误

#### 范例

[pad_trim](../../examples/string/pad_trim/main.c) · 删两侧集合

```c
	xstrview Value = xrtStrTrimSet(SV("{ \"value\" }"), SV("{}\" "));
```

## 独立字符串

### `xrtStrDup`

复制零结尾字符串，返回值始终由 `xrtFree` 释放。

```c
str xrtStrDup(cstr sText)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sText` | 输入 | 允许空 | 零结尾字符串（空 = 空串） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[string/edit](../../examples/string/edit/main.c) · 复制字符串

```c
	str sDup = xrtStrDup("hello");
```

### `xrtStrDupN`

复制明确长度字符串并追加零结尾。

```c
str xrtStrDupN(cstr sText, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sText` | 输入 | — | 字符串起点 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[string/edit](../../examples/string/edit/main.c) · 定长复制

```c
	str sDupN = xrtStrDupN("world", 3u);
```

### `xrtStrDupView`

复制字符串视图并追加零结尾。

```c
str xrtStrDupView(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[string/edit](../../examples/string/edit/main.c) · 视图复制

```c
	str sDupView = xrtStrDupView(Part);
```

### `xrtStrConcat`

连接两个字符串视图。

```c
str xrtStrConcat(xstrview Left, xstrview Right)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 左视图 |
| `Right` | 输入 | 借用 | 右视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[string/basic](../../examples/string/basic/main.c) · 连接

```c
	sResult = xrtStrConcat((xstrview){ arrName, iNameSize },
		XRT_STR_LITERAL(".txt"));
```

### `xrtStrJoin`

使用分隔符连接一组字符串视图。

```c
str xrtStrJoin(xstrview Separator, const xstrview* arrText, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Separator` | 输入 | 借用 | 分隔符 |
| `arrText` | 输入 | 非空数组 | 视图数组 |
| `iCount` | 输入 | — | 视图数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[dup_join](../../examples/string/dup_join/main.c) · 分隔连接

```c
	str sJoin = xrtStrJoin(SV(", "), Parts, 3u);
```

### `xrtStrRepeat`

重复字符串指定次数。

```c
str xrtStrRepeat(xstrview Text, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `iCount` | 输入 | — | 重复次数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[dup_join](../../examples/string/dup_join/main.c) · 重复

```c
	str sRepeat = xrtStrRepeat(SV("ab-"), 3u);
```

### `xrtStrReplace`

替换所有不重叠子串。

```c
str xrtStrReplace(xstrview Text, xstrview Part, xstrview Replacement)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Part` | 输入 | 借用 | 子串 |
| `Replacement` | 输入 | 借用 | 替换内容 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[edit](../../examples/string/edit/main.c) · 替换

```c
	showOwned("replace", xrtStrReplace(SV("a.b.c"), SV("."), SV("-")));
```

### `xrtStrInsert`

按字节位置插入子串。

```c
str xrtStrInsert(xstrview Text, size_t iPosition, xstrview Part)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `iPosition` | 输入 | — | 插入字节位置 |
| `Part` | 输入 | 借用 | 插入内容 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[edit](../../examples/string/edit/main.c) · 插入

```c
	showOwned("insert", xrtStrInsert(SV("xrtcore"), 3u, SV("::")));
```

### `xrtStrRemove`

按字节范围删除内容。

```c
str xrtStrRemove(xstrview Text, size_t iPosition, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `iPosition` | 输入 | — | 删除起点 |
| `iCount` | 输入 | — | 删除字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[edit](../../examples/string/edit/main.c) · 删除

```c
	showOwned("remove", xrtStrRemove(SV("xrt::core"), 3u, 2u));
```

### `xrtStrReverseBytes`

按字节反转字符串。

```c
str xrtStrReverseBytes(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[edit](../../examples/string/edit/main.c) · 反转

```c
	showOwned("reverse", xrtStrReverseBytes(SV("abc")));
```

### `xrtStrReverseBytesTo`

按字节反转到调用方缓冲区并补零；允许输入和输出起点相同。

```c
bool xrtStrReverseBytesTo(xstrview Text, char* sOutput, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `sOutput` | 输出 | 非空 | 输出缓冲，须含末尾零 |
| `iCapacity` | 输入 | — | 容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入并补零 | — |
| `false` | 容量不足或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足（须含末尾零字节），不写半个结果

#### 范例

[edit](../../examples/string/edit/main.c) · 反转到缓冲

```c
	if ( xrtStrReverseBytesTo(SV("cba"), Buffer, sizeof(Buffer)) ) {
```

### `xrtStrLower`

复制字符串并把 ASCII 字母转换为小写。

```c
str xrtStrLower(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[case](../../examples/string/case/main.c) · 转小写

```c
	printOwned("lower", xrtStrLower(SV("HELLO XRT")));
```

### `xrtStrLowerTo`

把 ASCII 字母转换为小写并写入调用方缓冲区；允许原地转换。

```c
bool xrtStrLowerTo(xstrview Text, char* sOutput, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `sOutput` | 输出 | 非空 | 输出缓冲，须含末尾零 |
| `iCapacity` | 输入 | — | 容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入并补零 | — |
| `false` | 容量不足或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足（须含末尾零字节），不写半个结果

#### 范例

[case](../../examples/string/case/main.c) · 转小写到缓冲

```c
	if ( xrtStrLowerTo(SV("BUFFER-WAY"), Buffer, sizeof(Buffer)) ) {
```

### `xrtStrUpper`

复制字符串并把 ASCII 字母转换为大写。

```c
str xrtStrUpper(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[case](../../examples/string/case/main.c) · 转大写

```c
	printOwned("upper", xrtStrUpper(SV("hello xrt")));
```

### `xrtStrUpperTo`

把 ASCII 字母转换为大写并写入调用方缓冲区；允许原地转换。

```c
bool xrtStrUpperTo(xstrview Text, char* sOutput, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `sOutput` | 输出 | 非空 | 输出缓冲，须含末尾零 |
| `iCapacity` | 输入 | — | 容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入并补零 | — |
| `false` | 容量不足或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足（须含末尾零字节），不写半个结果

#### 范例

[case](../../examples/string/case/main.c) · 转大写到缓冲

```c
	if ( xrtStrUpperTo(SV("buffer-way"), Buffer, sizeof(Buffer)) ) {
```

### `xrtStrFilter`

删除集合中的全部字节并创建独立字符串。

```c
str xrtStrFilter(xstrview Text, xstrview Set)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Set` | 输入 | 借用 | 字节集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[string/dup_join](../../examples/string/dup_join/main.c) · 过滤

```c
	str sFilter = xrtStrFilter(SV("xrt::core//"), SV(":/"));
```

### `xrtStrFilterTo`

删除集合中的全部字节并写入调用方缓冲区。

```c
bool xrtStrFilterTo(xstrview Text, xstrview Set, char* sOutput, size_t iCapacity, size_t* pOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Set` | 输入 | 借用 | 字节集合 |
| `sOutput` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `pOutputSize` | 输出 | 允许空 | 接收输出长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入 | — |
| `false` | 容量不足或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足（须含末尾零字节），不写半个结果

#### 范例

[string/basic](../../examples/string/basic/main.c) · 过滤到缓冲

```c
		 !xrtStrFilterTo(Name, XRT_STR_LITERAL("_-"), arrName,
			sizeof(arrName), &iNameSize) ) {
```

### `xrtStrPadLeft`

按字节宽度在左侧重复填充字符串。

```c
str xrtStrPadLeft(xstrview Text, size_t iWidth, xstrview Fill)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `iWidth` | 输入 | — | 目标字节宽度 |
| `Fill` | 输入 | 借用、非空 | 填充内容 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[pad_trim](../../examples/string/pad_trim/main.c) · 左填充

```c
	showOwned("pad-left", xrtStrPadLeft(SV("id"), 5u, SV("*")));
```

### `xrtStrPadRight`

按字节宽度在右侧重复填充字符串。

```c
str xrtStrPadRight(xstrview Text, size_t iWidth, xstrview Fill)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `iWidth` | 输入 | — | 目标字节宽度 |
| `Fill` | 输入 | 借用、非空 | 填充内容 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[pad_trim](../../examples/string/pad_trim/main.c) · 右填充

```c
	showOwned("pad-right", xrtStrPadRight(SV("id"), 5u, SV("*")));
```

### `xrtStrPadCenter`

按字节宽度在两侧重复填充字符串。

```c
str xrtStrPadCenter(xstrview Text, size_t iWidth, xstrview Fill)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `iWidth` | 输入 | — | 目标字节宽度 |
| `Fill` | 输入 | 借用、非空 | 填充内容 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[pad_trim](../../examples/string/pad_trim/main.c) · 居中填充

```c
	showOwned("pad-center", xrtStrPadCenter(SV("id"), 5u, SV("*")));
```

## 字符串构建器

### `xrtStrBufInit`

初始化空字符串构建器。

```c
void xrtStrBufInit(xstrbuf* pBuffer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输出 | 非空 | 接收构建器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[builder](../../examples/string/builder/main.c) · 初始化

```c
	xrtStrBufInit(&tBuffer);
```

### `xrtStrBufValid`

检查字符串构建器的公开状态是否自洽。

```c
bool xrtStrBufValid(const xstrbuf* pBuffer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | 目标构建器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否自洽 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[builder_tour](../../examples/string/builder_tour/main.c) · 状态自检

```c
	if ( !xrtStrBufReserve(&Buffer, 64u) || !xrtStrBufValid(&Buffer) ) {
```

### `xrtStrBufFree`

释放字符串构建器持有的内存。

```c
void xrtStrBufFree(xstrbuf* pBuffer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[builder](../../examples/string/builder/main.c) · 释放

```c
		xrtStrBufFree(&tBuffer);
```

### `xrtStrBufClear`

清空字符串构建器但保留容量。

```c
void xrtStrBufClear(xstrbuf* pBuffer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标构建器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已清空 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[string/builder_tour](../../examples/string/builder_tour/main.c) · 清空

```c
	xrtStrBufClear(&Buffer);
```

### `xrtStrBufView`

返回字符串构建器当前内容的借用视图。

```c
xstrview xrtStrBufView(const xstrbuf* pBuffer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | 目标构建器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 当前内容借用 | — |
| 空视图 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[string/builder_tour](../../examples/string/builder_tour/main.c) · 内容视图

```c
	(void)xrtStrBufResize(&Buffer, xrtStrBufView(&Buffer).Size - 1u);
```

### `xrtStrBufAlias`

检查视图是否借用构建器当前内容，并返回原始字节偏移。

```c
bool xrtStrBufAlias(const xstrbuf* pBuffer, xstrview Text, bool* pAlias, size_t* pOffset)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | 目标构建器 |
| `Text` | 输入 | 借用 | 待检查视图 |
| `pAlias` | 输出 | 允许空 | 接收是否别名 |
| `pOffset` | 输出 | 允许空 | 接收原始字节偏移 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 视图是构建器内容的别名 | — |
| `false` | 非别名或参数非法 | 不设错误 |

#### 错误

- 非别名返回 `false` 且不设置错误；指针为空 `XERR_ARGUMENT`

#### 范例

[builder_tour](../../examples/string/builder_tour/main.c) · 别名检查

```c
	(void)xrtStrBufAlias(&Buffer, View, &bAlias, &iAliasOffset);
```

### `xrtStrBufReserve`

保证字符串构建器至少具有指定数据容量。

```c
bool xrtStrBufReserve(xstrbuf* pBuffer, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标构建器 |
| `iCapacity` | 输入 | — | 期望容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 容量已保证 | — |
| `false` | 扩容失败 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 容量溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[string/builder_tour](../../examples/string/builder_tour/main.c) · 预留容量

```c
	if ( !xrtStrBufReserve(&Buffer, 64u) || !xrtStrBufValid(&Buffer) ) {
```

### `xrtStrBufResize`

调整字符串构建器长度，扩展区域填零。

```c
bool xrtStrBufResize(xstrbuf* pBuffer, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标构建器 |
| `iSize` | 输入 | — | 新长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 长度已调整 | — |
| `false` | 调整失败 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[string/builder_tour](../../examples/string/builder_tour/main.c) · 调整长度

```c
	(void)xrtStrBufResize(&Buffer, xrtStrBufView(&Buffer).Size - 1u);
```

### `xrtStrBufAppend`

追加字符串视图，允许追加自身的有效子视图。

```c
bool xrtStrBufAppend(xstrbuf* pBuffer, xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标构建器 |
| `Text` | 输入 | 借用 | 追加内容 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已追加 | — |
| `false` | 扩容失败 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[builder](../../examples/string/builder/main.c) · 追加视图

```c
	if ( !xrtStrBufAppend(&tBuffer, XRT_STR_LITERAL("items=")) ||
		 !xrtStrBufAppendRepeat(&tBuffer, XRT_STR_LITERAL("ab"), 3) ) {
```

### `xrtStrBufAppendByte`

追加一个字节。

```c
bool xrtStrBufAppendByte(xstrbuf* pBuffer, char iByte)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标构建器 |
| `iByte` | 输入 | — | 追加的字节 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已追加 | — |
| `false` | 扩容失败 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[string/builder_tour](../../examples/string/builder_tour/main.c) · 追加字节

```c
	if ( !xrtStrBufAppendByte(pBuffer, ' ') ||
		!xrtStrBufAppend(pBuffer, (xstrview){ sTag, strlen(sTag) }) ) {
```

### `xrtStrBufAppendRepeat`

重复追加字符串视图。

```c
bool xrtStrBufAppendRepeat(xstrbuf* pBuffer, xstrview Text, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标构建器 |
| `Text` | 输入 | 借用 | 追加内容 |
| `iCount` | 输入 | — | 重复次数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已追加 | — |
| `false` | 扩容失败 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[string/builder](../../examples/string/builder/main.c) · 重复追加

```c
		 !xrtStrBufAppendRepeat(&tBuffer, XRT_STR_LITERAL("ab"), 3) ) {
```

### `xrtStrBufAppendFormat`

使用 printf 规则直接追加到构建器；拒绝 `%n`。

```c
bool xrtStrBufAppendFormat(xstrbuf* pBuffer, cstr sFormat, ...)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标构建器 |
| `sFormat` | 输入 | 非空 | printf 格式串 |
| `...` | 输入 | 与格式串匹配 | 变参 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已追加 | — |
| `false` | 格式化或扩容失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.str` 域错误 — 格式串非法（含 `%n`）或与实参不匹配
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 扩容失败

#### 范例

[string/format](../../examples/string/format/main.c) · 格式化追加

```c
		 !xrtStrBufAppendFormat(&tBuffer, "%08X / %.2f", 255u, 3.5) ) {
```

### `xrtStrBufAppendFormatV`

使用 printf 规则和已有参数列表直接追加到构建器。

```c
bool xrtStrBufAppendFormatV(xstrbuf* pBuffer, cstr sFormat, va_list Args)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标构建器 |
| `sFormat` | 输入 | 非空 | printf 格式串 |
| `Args` | 输入 | 已由 `va_start` 初始化 | 变参列表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已追加 | — |
| `false` | 格式化或扩容失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.str` 域错误 — 格式串非法（含 `%n`）或与实参不匹配
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 扩容失败

#### 范例

[string/builder_tour](../../examples/string/builder_tour/main.c) · 格式化追加（va_list）

```c
	bool bOk = xrtStrBufAppendFormatV(pBuffer, "=%d", Args);
```

### `xrtStrBufTake`

取走构建器内存并把构建器重置为空。

```c
str xrtStrBufTake(xstrbuf* pBuffer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标构建器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 构建器为空 | 不设错误 |

#### 错误

- 空构建器返回 `NULL` 且不设置错误；指针为空 `XERR_ARGUMENT`

#### 范例

[string/builder](../../examples/string/builder/main.c) · 取走内存

```c
	sResult = xrtStrBufTake(&tBuffer);
```

## 拆分与行处理

### `xrtStrSplitInit`

初始化不分配内存的字符串拆分迭代器。

```c
bool xrtStrSplitInit(xstrsplit* pSplit, xstrview Text, xstrview Separator)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSplit` | 输出 | 非空 | 接收迭代器 |
| `Text` | 输入 | 借用 | 输入文本 |
| `Separator` | 输入 | 借用、非空 | 分隔符 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[split](../../examples/string/split/main.c) · 初始化拆分

```c
	if ( !xrtStrSplitInit(&tSplit, XRT_STR_LITERAL("alpha,beta,,gamma"), XRT_STR_LITERAL(",")) ) {
```

### `xrtStrSplitNext`

返回下一个借用片段，结束时返回 `false`。

```c
bool xrtStrSplitNext(xstrsplit* pSplit, xstrview* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSplit` | 输入/输出 | 已初始化 | 目标迭代器 |
| `pItem` | 输出 | 非空 | 接收借用视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已产出片段 | — |
| `false` | 遍历结束 | 不设错误 |

#### 错误

- 遍历结束返回 `false` 且不设置错误；指针为空 `XERR_ARGUMENT`

#### 范例

[split](../../examples/string/split/main.c) · 下一片段

```c
	while ( xrtStrSplitNext(&tSplit, &Item) ) {
```

### `xrtStrFieldsInit`

初始化按连续 ASCII 空白拆分的零分配字段迭代器。

```c
bool xrtStrFieldsInit(xstrfields* pFields, xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输出 | 非空 | 接收迭代器 |
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[iterators](../../examples/string/iterators/main.c) · 初始化字段

```c
	if ( xrtStrFieldsInit(&Fields, SV("  user\tport  443 ")) ) {
```

### `xrtStrFieldsNext`

返回下一个非空借用字段，结束时返回 `false`。

```c
bool xrtStrFieldsNext(xstrfields* pFields, xstrview* pField)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入/输出 | 已初始化 | 目标迭代器 |
| `pField` | 输出 | 非空 | 接收借用视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已产出字段 | — |
| `false` | 遍历结束 | 不设错误 |

#### 错误

- 遍历结束返回 `false` 且不设置错误；指针为空 `XERR_ARGUMENT`

#### 范例

[iterators](../../examples/string/iterators/main.c) · 下一字段

```c
		while ( xrtStrFieldsNext(&Fields, &Field) ) {
```

### `xrtStrLinesInit`

初始化不分配内存的行迭代器。

```c
bool xrtStrLinesInit(xstrlines* pLines, xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLines` | 输出 | 非空 | 接收迭代器 |
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[iterators](../../examples/string/iterators/main.c) · 初始化行

```c
	if ( xrtStrLinesInit(&Lines, SV("first\nsecond\r\n")) ) {
```

### `xrtStrLinesNext`

返回下一行借用视图，结束时返回 `false`。

```c
bool xrtStrLinesNext(xstrlines* pLines, xstrview* pLine)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLines` | 输入/输出 | 已初始化 | 目标迭代器 |
| `pLine` | 输出 | 非空 | 接收借用视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已产出行 | — |
| `false` | 遍历结束 | 不设错误 |

#### 错误

- 遍历结束返回 `false` 且不设置错误；指针为空 `XERR_ARGUMENT`

#### 范例

[iterators](../../examples/string/iterators/main.c) · 下一行

```c
		while ( xrtStrLinesNext(&Lines, &Line) ) {
```

### `xrtStrSplit`

一次性拆分字符串并返回独立的零结尾片段。

```c
xstrlist* xrtStrSplit(xstrview Text, xstrview Separator)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Separator` | 输入 | 借用 | 分隔符 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 片段列表，整体一次 `xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[string/dup_join](../../examples/string/dup_join/main.c) · 一次性拆分

```c
	xstrlist* pList = xrtStrSplit(SV("alpha,beta,gamma"), SV(","));
```

### `xrtStrSplitLines`

一次性按行拆分字符串。

```c
xstrlist* xrtStrSplitLines(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 行列表，整体一次 `xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[iterators](../../examples/string/iterators/main.c) · 一次性拆行

```c
	xstrlist* pList = xrtStrSplitLines(SV("a\nb\nc"));
```

### `xrtStrFields`

一次性按连续 ASCII 空白拆分字符串。

```c
xstrlist* xrtStrFields(xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 字段列表，整体一次 `xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[iterators](../../examples/string/iterators/main.c) · 一次性拆字段

```c
	xstrlist* pFields = xrtStrFields(SV("one two  three"));
```

### `xrtStrListAlloc`

为指定片段数量和零结尾数据容量分配单块字符串列表。

```c
xstrlist* xrtStrListAlloc(size_t iCount, size_t iDataSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iCount` | 输入 | — | 片段数量 |
| `iDataSize` | 输入 | — | 数据区字节容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 单块列表，整体一次 `xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 列表尺寸溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[list](../../examples/string/list/main.c) · 分配列表

```c
	xstrlist* pList = xrtStrListAlloc(3u, iDataSize);
```

### `xrtStrListWrite`

将片段复制到列表数据区并推进字节偏移。

```c
bool xrtStrListWrite(xstrlist* pList, size_t iIndex, xstrview Item, size_t* pOffset)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输入/输出 | 非空 | 目标列表 |
| `iIndex` | 输入 | — | 片段索引 |
| `Item` | 输入 | 借用 | 片段内容 |
| `pOffset` | 输出 | 允许空 | 接收/推进数据区偏移 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制并推进 | — |
| `false` | 索引越界或容量不足 | `XERR_RANGE` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 索引越界或数据区剩余容量不足

#### 范例

[list](../../examples/string/list/main.c) · 写入片段

```c
		if ( !xrtStrListWrite(pList, i, Source[i], &iOffset) ) {
```

### `xrtStrListFree`

释放便捷拆分结果。

```c
void xrtStrListFree(xstrlist* pList)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[list](../../examples/string/list/main.c) · 释放列表

```c
			xrtStrListFree(pList);
```

## 格式化

### `xrtFormat`

使用 printf 规则创建字符串；拒绝 `%n`。

```c
str xrtFormat(cstr sFormat, ...)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sFormat` | 输入 | 非空 | printf 格式串 |
| `...` | 输入 | 与格式串匹配 | 变参 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.str` 域错误 — 格式串非法（含 `%n`）或与实参不匹配
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[string/format_tour](../../examples/string/format_tour/main.c) · 格式化创建

```c
	str sDirect = xrtFormat("id=%d ok=%s", 7, "true");
```

### `xrtFormatV`

使用 printf 规则和已有参数列表创建字符串。

```c
str xrtFormatV(cstr sFormat, va_list Args)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sFormat` | 输入 | 非空 | printf 格式串 |
| `Args` | 输入 | 已由 `va_start` 初始化 | 变参列表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.str` 域错误 — 格式串非法（含 `%n`）或与实参不匹配
- `XERR_OVERFLOW` — 结果长度溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[string/format_tour](../../examples/string/format_tour/main.c) · 格式化创建（va_list）

```c
	sBody = xrtFormatV("x=%s", Args);
```

## 通配匹配

### `xrtStrGlob`

使用严格 UTF-8 通配模式匹配完整字符串。

```c
bool xrtStrGlob(xstrview Text, xstrview Pattern, uint32 iFlags)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Pattern` | 输入 | 借用 | 通配模式 |
| `iFlags` | 输入 | — | 匹配标志 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否完整匹配 | 不匹配不设错 |

#### 错误

- 不匹配返回 `false` 且不设置错误；指针为空或模式非法 `XERR_ARGUMENT`

#### 范例

[glob](../../examples/string/glob/main.c) · 通配匹配

```c
	printf("%s\n", xrtStrGlob(Name, Pattern, XSTR_GLOB_CASE_ASCII) ?
		"matched" : "not matched");
```

## 模块契约：线程

视图、查找与变换等纯函数可任意线程并发调用；构建器、拆分迭代器等有状态对象由创建线程独占使用，不可跨线程共享。

## 错误

- 无效指针、无效视图或无效公开结构产生 `XERR_ARGUMENT` 或 `XERR_STATE`。
- 长度计算或容量增长溢出产生 `XERR_RANGE`。
- 分配失败产生 `XERR_MEMORY`。
- C 运行库拒绝格式串或格式串包含 `%n` 时产生 `XERR_VALUE`，错误域为 `xrt.string`，代码为 `XSTR_ERROR_FORMAT`。

返回布尔值的迭代 `Next` 函数以 `false` 同时表示正常结束和失败。调用方只需在初始化或参数可能无效时检查当前错误；正常结束不会创建新错误。成功调用不清除旧错误，遵循 XRT 通用错误契约。

## 范例

完整范例位于：

- `examples/string/basic/main.c`：视图、裁剪和独立字符串。
- `examples/string/builder/main.c`：增量构建和所有权转移。
- `examples/string/split/main.c`：零分配行迭代。
- `examples/string/format/main.c`：直接格式化到构建器。
- `examples/string/glob/main.c`：严格 UTF-8 文件名通配。
- `examples/string/distance/main.c`：带阈值的 Unicode 标量编辑距离与相似度。

```c
xstrbuf Buffer;
xstrview Line;
xstrlines Lines;
str sResult;

xrtStrBufInit(&Buffer);
xrtStrBufAppend(&Buffer, XRT_STR_LITERAL("count="));
xrtStrBufAppendFormat(&Buffer, "%u", 3u);
sResult = xrtStrBufTake(&Buffer);

xrtStrLinesInit(&Lines, xrtStrView("alpha\r\nbeta\n"));
while ( xrtStrLinesNext(&Lines, &Line) ) {
	/* Line 借用原字符串，可按 Size 直接处理。 */
}

xrtFree(sResult);
xrtStrBufFree(&Buffer);
```
