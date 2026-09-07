# HTML 文本转义

`html_escape` 是独立于 Template、HTTP 和网络的轻量文本原语。它只依赖
`unicode`，可以被模板引擎、HTTP 响应、日志查看器或任意自定义渲染器复用。

## 类型与常量

### `xhtmlescapemode`

HTML 转义上下文；属性模式只适用于由引号包围的属性值。

```c
typedef enum xhtmlescapemode {
	XHTML_ESCAPE_TEXT = 0,
	XHTML_ESCAPE_ATTRIBUTE
} xhtmlescapemode;
```

| 值 | 语义 |
|---|---|
| `XHTML_ESCAPE_TEXT` | XHTMLESCAPE文本 |
| `XHTML_ESCAPE_ATTRIBUTE` | 属性上下文转义 |

### `xhtmlerror`

HTML 文本原语的稳定错误代码。

```c
typedef enum xhtmlerror {
	XHTML_ERROR_MODE = 1,
	XHTML_ERROR_UTF8
} xhtmlerror;
```

| 值 | 语义 |
|---|---|
| `XHTML_ERROR_MODE` | XHTML失败MODE |
| `XHTML_ERROR_UTF8` | 输入不是合法 UTF-8 |

## 裁剪

```c
#define XRT_MODULE_HTML_ESCAPE
#define XRT_IMPLEMENTATION
#include "xrt.h"
```

模块宏会自动启用 `XRT_FEATURE_HTML_ESCAPE` 及其 `unicode` 依赖。未选择模块时，
类型、函数与实现都不会进入发布物。

## 上下文

`xhtmlescapemode` 明确区分两个输出位置：

- `XHTML_ESCAPE_TEXT`：转义 `&`、`<`、`>`，用于普通 HTML 文本节点；
- `XHTML_ESCAPE_ATTRIBUTE`：额外转义 `"` 和 `'`，用于由单引号或双引号包围的
  属性值。

属性模式不支持无引号属性。调用方仍须为结果写入配对引号。

## API 分层

`xrtHtmlEscapeSize` 严格校验 UTF-8，并返回不含末尾零的精确字节数。

`xrtHtmlEscapeWrite` 写入调用方缓冲区。容量必须比结果长度多一个末尾零字节；
输出为 `NULL` 且容量为零时只查询长度。输入和输出可以从同一地址开始，其他部分
重叠会被拒绝。容量不足时不修改输出，通过 `pOutputSize` 返回所需长度。

`xrtHtmlEscape` 创建零结尾结果，返回值由 `xrtFree` 释放。长度输出可以为
`NULL`，空输入仍返回独立、可释放的空字符串。

```c
str text = xrtHtmlEscape(
	XRT_STR_LITERAL("<ready> & running"),
	XHTML_ESCAPE_TEXT,
	NULL
);
```

## 文本契约

输入使用 `xstrview`，因此允许嵌入零并始终按显式长度处理。模块严格拒绝非法
UTF-8，但不会删除或正规化其他 Unicode 字符，也不会替换 ASCII 控制字节。
输出长度不能使用 `strlen` 代替显式结果长度，除非调用方已经确定输入不含零。

## 安全边界

该模块是上下文转义器，不是 HTML 清洗器。它不解析标签，不移除危险元素或属性，
也不处理 JavaScript、CSS、URL、JSON 或 SQL 上下文。用户提供的整段 HTML 需要独立
的白名单清洗方案；脚本、样式和 URL 属性需要各自的编码与校验规则。

Template 保持通用文本模板语义，不会隐式调用本模块。模板变量进入 HTML 文本或
属性时，应在对应上下文显式转义，避免模板层替调用方猜测输出格式。

## 模块契约：线程

转义/解析 API 为无共享状态的纯函数，可任意线程并发调用。

## 错误

`xhtmlerror` 枚举定义 `xrt.html` 错误域中的稳定模块代码：

- `XHTML_ERROR_MODE`：转义上下文无效；
- `XHTML_ERROR_UTF8`：输入不是严格 UTF-8。

空指针、地址回绕和非法重叠使用 `XERR_ARGUMENT`，短缓冲使用 `XERR_RANGE`，长度
溢出使用统一范围错误，分配失败使用 `XERR_MEMORY`。

## 示例与测试

- `examples/text/html_escape/main.c`
- `tests/text/test_html_escape.c`
- `tests/text/test_html_escape_noalloc.c`
- `tests/text/test_html_escape_oom.c`
- `tests/single/test_single_html_escape.c`
## API

### `xrtHtmlEscapeSize`

严格校验 UTF-8 并返回转义后的精确字节数（不含末尾零）。

```c
bool xrtHtmlEscapeSize(
	xstrview Text,
	xhtmlescapemode Mode,
	size_t* pOutputSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用、严格 UTF-8 | 允许嵌入零 |
| `Mode` | 输入 | 枚举 | TEXT 只转 `& < >`；ATTRIBUTE 额外转 `" '` |
| `pOutputSize` | 输出 | 非空 | 接收精确字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 长度已写出 | — |
| `false` | 输入非法 UTF-8 或参数错误 | `*pOutputSize` 不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `xrt.charset` 域错误 — 非法 UTF-8（严格模式）

#### 范例

[html/variants · 查询](../../examples/html/variants/main.c) · 观察

```c
	if ( !xrtHtmlEscapeSize(XRT_STR_LITERAL("<a>"),
		XHTML_ESCAPE_TEXT, &iSize) ) {
```


### `xrtHtmlEscapeWrite`

转义到调用方缓冲区；容量须含末尾零，空输出可只查询长度，同址扩张允许、部分重叠拒绝。

```c
bool xrtHtmlEscapeWrite(
	xstrview Text,
	xhtmlescapemode Mode,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用、严格 UTF-8 | 输入视图 |
| `Mode` | 输入 | 枚举 | 转义模式 |
| `sOutput` | 输出 | 可空 | 空+零容量 = 只查长度；可与输入同址 |
| `iCapacity` | 输入 | — | 容量（含末尾零） |
| `pOutputSize` | 输出 | 可空 | 实际长度（不含零） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出零结尾结果 | — |
| `false` | 容量不足、重叠非法或输入非法 | 输出不变；`*pOutputSize` 给出所需长度 |

#### 错误

- `XERR_RANGE` — 容量不足（不修改输出）
- `XERR_ARGUMENT` — 部分重叠或指针非法
- `xrt.charset` 域错误 — 非法 UTF-8

#### 范例

[html/variants · 写入](../../examples/html/variants/main.c) · 观察

```c
	if ( !xrtHtmlEscapeWrite(XRT_STR_LITERAL("<a>"),
		XHTML_ESCAPE_TEXT, Buffer, sizeof(Buffer), &iSize) ) {
```


### `xrtHtmlEscape`

创建由 `xrtFree` 释放的零结尾转义文本；空输入仍返回独立可释放的空字符串。

```c
str xrtHtmlEscape(
	xstrview Text,
	xhtmlescapemode Mode,
	size_t* pOutputSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用、严格 UTF-8 | 输入视图 |
| `Mode` | 输入 | 枚举 | 转义模式 |
| `pOutputSize` | 输出 | 可空 | 接收长度（不含零） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 输入非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.charset` 域错误 — 非法 UTF-8
- `XERR_MEMORY` — 分配失败

#### 范例

[text/html_escape · 双模式](../../examples/text/html_escape/main.c) · 观察

```c
	sText = xrtHtmlEscape(
		XRT_STR_LITERAL("状态：<ready> & 可用"),
		XHTML_ESCAPE_TEXT,
		NULL
	);
```


