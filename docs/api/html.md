# HTML 文本转义

`html_escape` 是独立于 Template、HTTP 和网络的轻量文本原语。它只依赖
`unicode`，可以被模板引擎、HTTP 响应、日志查看器或任意自定义渲染器复用。

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
