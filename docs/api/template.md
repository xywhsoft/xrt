# Template

模板模块把“编译一次、重复渲染”作为基本模型。`xtemplate` 是不可变引用对象，可以跨线程共享；每次渲染的作用域、预算、输出和外部解析状态互相独立。模板源、路径、参数和输出都使用明确长度，允许嵌入零字节。

## 类型与常量

### `xtemplateerror`

模板错误代码在 xrt.template 域内稳定标识失败阶段。

```c
typedef enum xtemplateerror {
	XTEMPLATE_ERROR_CONFIG = 1,
	XTEMPLATE_ERROR_SYNTAX,
	XTEMPLATE_ERROR_LIMIT,
	XTEMPLATE_ERROR_UNDEFINED,
	XTEMPLATE_ERROR_TYPE,
	XTEMPLATE_ERROR_FORMAT,
	XTEMPLATE_ERROR_ITERATE,
	XTEMPLATE_ERROR_WRITE,
	XTEMPLATE_ERROR_CALLBACK,
	XTEMPLATE_ERROR_INCLUDE,
	XTEMPLATE_ERROR_CYCLE
} xtemplateerror;
```

| 值 | 语义 |
|---|---|
| `XTEMPLATE_ERROR_CONFIG` | 配置非法 |
| `XTEMPLATE_ERROR_SYNTAX` | 语法非法 |
| `XTEMPLATE_ERROR_LIMIT` | 超限 |
| `XTEMPLATE_ERROR_UNDEFINED` | 失败 |
| `XTEMPLATE_ERROR_TYPE` | 类型 |
| `XTEMPLATE_ERROR_FORMAT` | 格式非法 |
| `XTEMPLATE_ERROR_ITERATE` | 失败 |
| `XTEMPLATE_ERROR_WRITE` | 写方向 |
| `XTEMPLATE_ERROR_CALLBACK` | 回调失败 |
| `XTEMPLATE_ERROR_INCLUDE` | 失败 |

### `xtemplatelocation`

源码位置使用 0 基字节偏移和 1 基行列。

```c
typedef struct xtemplatelocation {
	size_t Offset;
	size_t Size;
	size_t Line;
	size_t Column;
} xtemplatelocation;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Offset` | `size_t` | 偏移量 |
| `Size` | `size_t` | 字节数 |
| `Line` | `size_t` | 行号 |
| `Column` | `size_t` | 列号 |

### `xtemplatenodetype`

核心层节点区分原样文本和动态输出。

```c
typedef enum xtemplatenodetype {
	XTEMPLATE_NODE_TEXT = 1,
	XTEMPLATE_NODE_OUTPUT,
	XTEMPLATE_NODE_INLINE_IF,
	XTEMPLATE_NODE_IF,
	XTEMPLATE_NODE_FOR,
	XTEMPLATE_NODE_FOREACH,
	XTEMPLATE_NODE_BREAK,
	XTEMPLATE_NODE_CONTINUE,
	XTEMPLATE_NODE_DEFINE,
	XTEMPLATE_NODE_INCLUDE,
	XTEMPLATE_NODE_RAW,
	XTEMPLATE_NODE_EXTENSION
} xtemplatenodetype;
```

| 值 | 语义 |
|---|---|
| `XTEMPLATE_NODE_TEXT` | 文本 |
| `XTEMPLATE_NODE_OUTPUT` | 输出失败 |
| `XTEMPLATE_NODE_INLINE_IF` | 行内条件（{{...}}） |
| `XTEMPLATE_NODE_IF` | 条件块 |
| `XTEMPLATE_NODE_FOR` | 数值循环 |
| `XTEMPLATE_NODE_FOREACH` | 迭代循环 |
| `XTEMPLATE_NODE_BREAK` | break |
| `XTEMPLATE_NODE_CONTINUE` | CONTINUE（100 继续） |
| `XTEMPLATE_NODE_DEFINE` | 模板定义 |
| `XTEMPLATE_NODE_INCLUDE` | 包含 |
| `XTEMPLATE_NODE_RAW` | 裸格式 |

### `xtemplateoutputtype`

输出类型决定动态值允许的类型与格式化规则。

```c
typedef enum xtemplateoutputtype {
	XTEMPLATE_OUTPUT_TEXT = 1,
	XTEMPLATE_OUTPUT_NUMBER,
	XTEMPLATE_OUTPUT_TIME
} xtemplateoutputtype;
```

| 值 | 语义 |
|---|---|
| `XTEMPLATE_OUTPUT_TEXT` | 文本 |
| `XTEMPLATE_OUTPUT_NUMBER` | 输出失败 |

### `xtemplatenodeview`

节点视图中的字符串全部借用模板，模板释放后立即失效。

```c
typedef struct xtemplatenodeview {
	xtemplatenodetype Type;
	xtemplateoutputtype Output;
	xtemplatelocation Location;
	xstrview Source;
	xstrview Expression;
	xstrview Format;
	xstrview Name;
} xtemplatenodeview;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Type` | `xtemplatenodetype` | 类型 |
| `Output` | `xtemplateoutputtype` | 输出缓冲 |
| `Location` | `xtemplatelocation` | Location |
| `Source` | `xstrview` | 源视图 |
| `Expression` | `xstrview` | Expression |
| `Format` | `xstrview` | 格式 |
| `Name` | `xstrview` | 名称 |

### `xtemplateextensiontype`

扩展类型明确区分行内调用、解析主体和完全原样主体。

```c
typedef enum xtemplateextensiontype {
	XTEMPLATE_EXTENSION_FUNCTION = 1,
	XTEMPLATE_EXTENSION_STATEMENT,
	XTEMPLATE_EXTENSION_BLOCK,
	XTEMPLATE_EXTENSION_RAW_BLOCK
} xtemplateextensiontype;
```

| 值 | 语义 |
|---|---|
| `XTEMPLATE_EXTENSION_FUNCTION` | FUNCTION |
| `XTEMPLATE_EXTENSION_STATEMENT` | STATEMENT |
| `XTEMPLATE_EXTENSION_BLOCK` | 阻塞策略 |

### `xtemplateextension`

注册描述在创建期间借用，注册表成功创建后复制名称并接管用户数据。

```c
typedef struct xtemplateextension {
	xstrview Name;
	xtemplateextensiontype Type;
	size_t MinArguments;
	size_t MaxArguments;
	xtemplateextensionfn Call;
	ptr Data;
	xtemplateextensiondrop Drop;
} xtemplateextension;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Name` | `xstrview` | 名称 |
| `Type` | `xtemplateextensiontype` | 类型 |
| `MinArguments` | `size_t` | MinArguments |
| `MaxArguments` | `size_t` | MaxArguments |
| `Call` | `xtemplateextensionfn` | Call |
| `Data` | `ptr` | 数据 |
| `Drop` | `xtemplateextensiondrop` | Drop |

### `xtemplateargview`

参数视图借用模板源码，并以调用内相对索引作为稳定句柄。

```c
typedef struct xtemplateargview {
	size_t Index;
	xstrview Name;
	xstrview Source;
} xtemplateargview;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Index` | `size_t` | 索引 |
| `Name` | `xstrview` | 名称 |
| `Source` | `xstrview` | 源视图 |

### `xtemplatevalue`

求值结果借用输入值或模板文本，仅对应当前类型的字段有效。

```c
typedef struct xtemplatevalue {
	xvaluetype Type;
	const xvalue* Value;
	bool Bool;
	int64 Integer;
	uint64 Unsigned;
	double Float;
	xstrview Text;
	xtime Time;
} xtemplatevalue;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Type` | `xvaluetype` | 类型 |
| `Value` | `const xvalue*` | 值 |
| `Bool` | `bool` | Bool |
| `Integer` | `int64` | Integer |
| `Unsigned` | `uint64` | Unsigned |
| `Float` | `double` | Float |
| `Text` | `xstrview` | 文本视图 |
| `Time` | `xtime` | 时间戳（Unix 微秒） |

### `xtemplateconfig`

编译配置限制源码和编译产物规模，并允许替换成对标签括号。

```c
typedef struct xtemplateconfig {
	xstrview Open;
	xstrview Close;
	size_t MaxSourceBytes;
	size_t MaxNodes;
	size_t MaxPathSegments;
	size_t MaxPathDepth;
	size_t MaxExpressions;
	size_t MaxBlockDepth;
	size_t MaxExpressionDepth;
	const xtemplateregistry* Registry;
	size_t MaxArguments;
	size_t MaxCallArguments;
} xtemplateconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Open` | `xstrview` | Open |
| `Close` | `xstrview` | Close |
| `MaxSourceBytes` | `size_t` | MaxSourceBytes |
| `MaxNodes` | `size_t` | MaxNodes |
| `MaxPathSegments` | `size_t` | MaxPathSegments |
| `MaxPathDepth` | `size_t` | MaxPathDepth |
| `MaxExpressions` | `size_t` | MaxExpressions |
| `MaxBlockDepth` | `size_t` | MaxBlockDepth |
| `MaxExpressionDepth` | `size_t` | MaxExpressionDepth |
| `Registry` | `const xtemplateregistry*` | Registry |
| `MaxArguments` | `size_t` | MaxArguments |
| `MaxCallArguments` | `size_t` | MaxCallArguments |

### `xtemplaterenderflag`

渲染标志可组合；HTML 转义只作用于动态 {$path} 输出，不改写模板文本。

```c
typedef enum xtemplaterenderflag {
	XTEMPLATE_STRICT_UNDEFINED = 0x0001u,
	XTEMPLATE_ESCAPE_HTML_TEXT = 0x0002u
} xtemplaterenderflag;
```

| 值 | 语义 |
|---|---|
| `XTEMPLATE_STRICT_UNDEFINED` | XTEMPLATESTRICTUNDEFINED |

### `xtemplaterenderconfig`

每次渲染使用独立配置，因此同一模板可以并发执行。

```c
typedef struct xtemplaterenderconfig {
	const xvalue* Root;
	const xvalue* Current;
	const xvalue* Global;
	size_t MaxOutputBytes;
	size_t MaxSteps;
	size_t MaxDepth;
	size_t MaxLoopIterations;
	xtemplateresolvefn Resolve;
	ptr ResolveData;
	size_t MaxIncludeDepth;
	uint32 Flags;
} xtemplaterenderconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Root` | `const xvalue*` | Root |
| `Current` | `const xvalue*` | Current |
| `Global` | `const xvalue*` | Global |
| `MaxOutputBytes` | `size_t` | MaxOutputBytes |
| `MaxSteps` | `size_t` | MaxSteps |
| `MaxDepth` | `size_t` | MaxDepth |
| `MaxLoopIterations` | `size_t` | MaxLoopIterations |
| `Resolve` | `xtemplateresolvefn` | Resolve |
| `ResolveData` | `ptr` | ResolveData |
| `MaxIncludeDepth` | `size_t` | MaxIncludeDepth |
| `Flags` | `uint32` | 标志位 |

### `xtemplate`

编译模板是不可变且可跨线程共享的对象。

```c
typedef struct xtemplate xtemplate;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtemplateregistry`

扩展注册表（不透明）：不可变且可跨线程共享，持有扩展定义与被接管的用户数据。


```c
typedef struct xtemplateregistry xtemplateregistry;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtemplatecall`

扩展调用上下文（不透明）：渲染期间传给扩展回调，提供参数、作用域值与渲染出口。


```c
typedef struct xtemplatecall xtemplatecall;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtemplateextensionfn`

扩展回调返回 false 时直接传播模板错误；其他错误会保留为 cause 并补充调用位置。

```c
typedef bool (*xtemplateextensionfn)(xtemplatecall* pCall);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtemplateextensiondrop`

注册表释放时调用数据析构；每个描述项独立拥有自己的数据。

```c
typedef void (*xtemplateextensiondrop)(ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtemplateresolvefn`

输出初始为空，回调写入的任何非空模板引用都会由渲染器接管。

```c
typedef bool (*xtemplateresolvefn)(
	ptr pUserData,
	xstrview Name,
	xtemplate** pTemplate
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtemplatewritefn`

Writer 借用当前分片；返回 false 会停止渲染并保留回调设置的错误。

```c
typedef bool (*xtemplatewritefn)(ptr pUserData, xstrview Text);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

## 模块

- `XRT_MODULE_TEMPLATE_CORE`：文本、字符串值、数字和时间输出，编译对象、节点检查、流式与字符串渲染。
- `XRT_MODULE_TEMPLATE_CONTROL`：表达式、条件、范围循环、容器遍历和循环控制。
- `XRT_MODULE_TEMPLATE_COMPOSE`：本地定义、外部模板解析、包含、原样块和环检测；依赖 `map`。
- `XRT_MODULE_TEMPLATE_EXTENSION`：不可变扩展注册表、函数、语句、解析块和原样块；依赖 compose。
- `XRT_MODULE_TEMPLATE_FILE`：从文件读取完整模板并编译；只依赖 core 与 `file_whole`，不强制引入控制、组合或扩展层。

可以同时启用多个顶层模块。例如 `template_extension,template_file` 同时提供完整语言和文件入口。裁剪时依赖由清单展开，缺失直接依赖会在头文件或构建阶段失败，不会静默降级。

## 默认预算

所有默认值都是有限值，调用方可以通过配置进一步收紧：

| 宏 | 默认值 | 约束对象 |
| --- | ---: | --- |
| `XTEMPLATE_SOURCE_DEFAULT` | 16 MiB | 模板源字节数 |
| `XTEMPLATE_NODES_DEFAULT` | 1,000,000 | 编译节点数 |
| `XTEMPLATE_PATH_SEGMENTS_DEFAULT` | 1,000,000 | 全部路径段数 |
| `XTEMPLATE_PATH_DEPTH_DEFAULT` | 64 | 单条路径深度 |
| `XTEMPLATE_EXPRESSIONS_DEFAULT` | 1,000,000 | 表达式数 |
| `XTEMPLATE_BLOCK_DEPTH_DEFAULT` | 64 | 编译块嵌套深度 |
| `XTEMPLATE_EXPRESSION_DEPTH_DEFAULT` | 128 | 表达式嵌套深度 |
| `XTEMPLATE_ARGUMENTS_DEFAULT` | 1,000,000 | 全部扩展参数数 |
| `XTEMPLATE_CALL_ARGUMENTS_DEFAULT` | 256 | 单次扩展调用参数数 |
| `XTEMPLATE_OUTPUT_DEFAULT` | 16 MiB | 单次渲染输出字节数 |
| `XTEMPLATE_STEPS_DEFAULT` | 10,000,000 | 单次渲染执行步数 |
| `XTEMPLATE_RENDER_DEPTH_DEFAULT` | 64 | 运行时节点嵌套深度 |
| `XTEMPLATE_LOOP_DEFAULT` | 1,000,000 | 单次渲染循环次数 |
| `XTEMPLATE_INCLUDE_DEPTH_DEFAULT` | 32 | 外部包含深度 |

`xrtTemplateConfigInit` 初始化默认分隔符与编译预算，`xrtTemplateRenderConfigInit` 初始化默认作用域与渲染预算。配置结构必须先初始化，再覆盖所需字段；不要依赖全零结构的含义。

## 编译与所有权

`xrtTemplateCompile` 使用默认配置编译 `xstrview`。`xrtTemplateCompileConfig` 接受显式 `xtemplateconfig`：

- `Open`、`Close` 替换默认开始与结束标记。
- `MaxSourceBytes`、`MaxNodes`、`MaxPathSegments` 和 `MaxPathDepth` 约束核心编译产物。
- control 启用后，`MaxExpressions`、`MaxBlockDepth` 和 `MaxExpressionDepth` 约束表达式与块。
- extension 启用后，`Registry` 绑定不可变注册表，`MaxArguments` 和 `MaxCallArguments` 约束扩展参数。

编译会复制模板源及需要长期持有的元数据。调用返回后，输入 `xstrview` 与配置本身都可以失效。`xrtTemplateRef` 增加引用，`xrtTemplateRelease` 释放引用；最终释放时同时释放模板持有的注册表引用。

`xrtTemplateCompileFile` 使用默认配置读取并编译文件，`xrtTemplateCompileFileConfig` 使用配置中的 `MaxSourceBytes` 作为读取上限。文件内容按原始字节处理，不删除 BOM，也不做编码转换。读取失败保留 `xrt.file` 错误，编译失败使用 `xrt.template` 错误；临时文件缓冲在编译返回前释放。

### `xrtTemplateConfigInit`

初始化默认括号和有限编译预算。

```c
void xrtTemplateConfigInit(xtemplateconfig* pConfig)
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

[extension](../../examples/template/extension/main.c) · 编译配置

```c
	xrtTemplateConfigInit(&Config);
```

### `xrtTemplateCompile`

使用默认配置编译模板源码。

```c
xtemplate* xrtTemplateCompile(xstrview Source)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | 模板源码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 不可变模板（引用 1） | — |
| `NULL` | 编译失败 | `xrt.template` 域错误 |

#### 错误

- `xrt.template` / `XTEMPLATE_ERROR_CONFIG` — 配置字段非法
- `xrt.template` / `XTEMPLATE_ERROR_SYNTAX` — 源码语法非法，位置可由 `xrtTemplateErrorLocation` 读取
- `xrt.template` / `XTEMPLATE_ERROR_LIMIT` — 超出编译或渲染预算
- `XERR_MEMORY` — 分配失败

#### 范例

[compose](../../examples/template/compose/main.c) · 默认编译

```c
	pTemplate = xrtTemplateCompile(XRT_STR_LITERAL(
		"Users: {#foreach:users}{#include:'user'}"
		"{?loop.last::,}{#end}{#include:'suffix'}"
		"{#define:'user'}{$name}{#end}"
	));
```

### `xrtTemplateCompileConfig`

使用显式配置编译模板源码。

```c
xtemplate* xrtTemplateCompileConfig(xstrview Source, const xtemplateconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Source` | 输入 | 借用 | 模板源码 |
| `pConfig` | 输入 | 非空 | 编译配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 不可变模板（引用 1） | — |
| `NULL` | 编译失败 | `xrt.template` 域错误 |

#### 错误

- `xrt.template` / `XTEMPLATE_ERROR_CONFIG` — 配置字段非法
- `xrt.template` / `XTEMPLATE_ERROR_SYNTAX` — 源码语法非法，位置可由 `xrtTemplateErrorLocation` 读取
- `xrt.template` / `XTEMPLATE_ERROR_LIMIT` — 超出编译或渲染预算
- `XERR_MEMORY` — 分配失败

#### 范例

[extension](../../examples/template/extension/main.c) · 显式配置编译

```c
	pTemplate = xrtTemplateCompileConfig(
		XRT_STR_LITERAL("{#link:url}{$label}{#end}"),
		&Config
	);
```

### `xrtTemplateCompileFile`

使用默认模板配置读取并编译完整文件。

```c
xtemplate* xrtTemplateCompileFile(cstr sPath)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | UTF-8 文件路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 不可变模板（引用 1） | — |
| `NULL` | 读取或编译失败 | `xrt.template` / `xrt.file` 域错误 |

#### 错误

- `xrt.template` / `XTEMPLATE_ERROR_CONFIG` — 配置字段非法
- `xrt.template` / `XTEMPLATE_ERROR_SYNTAX` — 源码语法非法，位置可由 `xrtTemplateErrorLocation` 读取
- `xrt.template` / `XTEMPLATE_ERROR_LIMIT` — 超出编译或渲染预算
- `XERR_MEMORY` — 分配失败
- `xrt.file` 域错误 — 文件读取失败

#### 范例

[file](../../examples/template/file/main.c) · 默认文件编译

```c
	pTemplate = xrtTemplateCompileFile("examples/template/file/page.tpl");
```

### `xrtTemplateCompileFileConfig`

在配置源码上限内读取完整文件并编译，文件内容只在调用期间持有。

```c
xtemplate* xrtTemplateCompileFileConfig(cstr sPath, const xtemplateconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | UTF-8 文件路径 |
| `pConfig` | 输入 | 非空 | 编译配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 不可变模板（引用 1） | — |
| `NULL` | 读取或编译失败 | `xrt.template` / `xrt.file` 域错误 |

#### 错误

- `xrt.template` / `XTEMPLATE_ERROR_CONFIG` — 配置字段非法
- `xrt.template` / `XTEMPLATE_ERROR_SYNTAX` — 源码语法非法，位置可由 `xrtTemplateErrorLocation` 读取
- `xrt.template` / `XTEMPLATE_ERROR_LIMIT` — 超出编译或渲染预算
- `XERR_MEMORY` — 分配失败
- `xrt.file` 域错误 — 文件读取失败

#### 范例

[tour](../../examples/template/tour/main.c) · 显式配置文件编译

```c
	pTemplate = xrtTemplateCompileFileConfig(sFile, &Config);
```

### `xrtTemplateRef`

增加不可变模板引用并返回原指针。

```c
xtemplate* xrtTemplateRef(xtemplate* pTemplate)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTemplate` | 输入 | 非空 | 目标模板 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[compose](../../examples/template/compose/main.c) · 共享引用

```c
		*pTemplate = xrtTemplateRef(pExternal);
```

### `xrtTemplateRelease`

释放模板引用。

```c
void xrtTemplateRelease(xtemplate* pTemplate)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTemplate` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[compose](../../examples/template/compose/main.c) · 释放引用

```c
	xrtTemplateRelease(pExternal);
```

### `xrtTemplateSource`

返回模板持有的原始源码视图。

```c
xstrview xrtTemplateSource(const xtemplate* pTemplate)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTemplate` | 输入 | 非空 | 目标模板 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 源码借用 | — |
| 空视图 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tour](../../examples/template/tour/main.c) · 原始源码

```c
		(xrtTemplateSource(pTemplate).Size != 14u) ) {
```

### `xrtTemplateNodeCount`

返回模板中的全部编译节点数量，包括控制块内部节点。

```c
size_t xrtTemplateNodeCount(const xtemplate* pTemplate)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTemplate` | 输入 | 非空 | 目标模板 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 编译节点数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tour](../../examples/template/tour/main.c) · 节点数量

```c
	iNodes = xrtTemplateNodeCount(pTemplate);
```

### `xrtTemplateNode`

返回指定编译节点的只读视图。

```c
bool xrtTemplateNode(const xtemplate* pTemplate, size_t iIndex, xtemplatenodeview* pNode)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTemplate` | 输入 | 非空 | 目标模板 |
| `iIndex` | 输入 | < 节点数量 | 节点索引 |
| `pNode` | 输出 | 非空 | 接收节点视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 视图已写出 | — |
| `false` | 索引越界 | `XERR_RANGE` |

#### 错误

- `XERR_RANGE` — 节点索引越界；句柄为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/template/tour/main.c) · 节点视图

```c
			if ( !xrtTemplateNode(pTemplate, i, &Node) ) {
```

### `xrtTemplateRegistryCreate`

校验并复制全部扩展定义，成功后注册表接管每项用户数据。

```c
xtemplateregistry* xrtTemplateRegistryCreate(const xtemplateextension* pExtensions, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExtensions` | 输入 | 非空数组 | 扩展定义 |
| `iCount` | 输入 | > 0 | 扩展数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 不可变注册表（引用 1） | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.template` / `XTEMPLATE_ERROR_CONFIG` — 扩展定义非法
- `XERR_MEMORY` — 分配失败

#### 范例

[extension](../../examples/template/extension/main.c) · 创建注册表

```c
	xtemplateregistry* pRegistry = xrtTemplateRegistryCreate(&Extension, 1u);
```

### `xrtTemplateRegistryRef`

增加不可变注册表引用并返回原指针。

```c
xtemplateregistry* xrtTemplateRegistryRef(const xtemplateregistry* pRegistry)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegistry` | 输入 | 非空 | 目标注册表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tour](../../examples/template/tour/main.c) · 共享引用

```c
	pRegistryRef = xrtTemplateRegistryRef(pRegistry);
```

### `xrtTemplateRegistryRelease`

释放注册表引用及其最终拥有的扩展用户数据。

```c
void xrtTemplateRegistryRelease(const xtemplateregistry* pRegistry)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRegistry` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[extension](../../examples/template/extension/main.c) · 释放引用

```c
	xrtTemplateRegistryRelease(pRegistry);
```

## 基础语法

默认开始和结束标记是 `{` 与 `}`。重复开始标记输出一个字面开始标记，例如 `{{` 输出 `{`。

| 写法 | 含义 |
| --- | --- |
| `{$path}` | 输出字符串或可直接表示为文本的标量 |
| `{%path}` | 输出数字 |
| `{%path:04d}` | 使用 XRT 数字格式输出数字 |
| `{&path:%F}` | 使用 XRT 时间格式输出时间 |

路径默认从 `Current` 开始，支持对象键、点号、数组索引和负索引。`this`、`root`、`global` 是显式根；control 还提供 `loop`。默认查找不会从 Current 隐式回退到 Root 或 Global，跨作用域访问必须写出根名，避免同名字段改变含义。

Template 是通用文本引擎，不知道输出将进入 HTML 文本、HTML 属性、JavaScript、CSS、
Shell、SQL 还是其他上下文。`{$path}` 按原字节输出，不自动执行 HTML 或其他上下文
转义。只有已经由应用验证或转义的数据才能直接进入这些敏感位置；需要统一策略时，
应通过只读扩展函数集中完成，并为每种输出上下文使用不同扩展。JSON 序列化和 SQL
参数绑定不能用模板转义代替。

HTML 文本与带引号属性可以按需组合独立的 `html_escape` 模块。应用可以在写入
`xvalue` 前调用 `xrtHtmlEscape`，也可以注册只读模板扩展并在扩展内部调用它；
Template 本身不依赖该模块，也不会替调用方猜测输出上下文。详见
[HTML 文本转义](html.md)。

## 表达式与控制

表达式支持 `null`、布尔值、有符号整数字面量、浮点数、带引号字符串、路径和括号；输入 `xvalue` 也可提供完整 `uint64`。逻辑运算支持 `not` / `!`、`and` / `&&`、`or` / `||`，并执行短路求值。比较支持 `=`、`==`、`!=`、`~=`、`>`、`<`、`>=`、`<=`；有符号整数、无符号整数与浮点数执行精确的混合比较，NaN 不会被当成普通有序值。

| 写法 | 含义 |
| --- | --- |
| `{?expr:true-text:false-text}` | 行内条件；文本中的冒号写为 `\:` |
| `{#if:expr}...{#elseif:expr}...{#else}...{#end}` | 条件块 |
| `{#for:start:end}...{#end}` | 包含两端的整数范围 |
| `{#for:start:end:step}...{#end}` | 显式步长范围；步长为零是错误 |
| `{#foreach:expr}...{#end}` | 遍历数组、整数映射、集合或对象 |
| `{#break}` / `{#continue}` | 控制最近一层循环 |

`for` 未给出步长时根据起止方向选择 `1` 或 `-1`；显式步长方向不可能到达终点时循环为空。`foreach` 为迭代建立稳定快照，并把 Current 临时替换为当前项。

循环体可以读取 `loop.value`、`loop.index`、`loop.number`、`loop.key`、`loop.first`、`loop.last` 和 `loop.depth`。`index` 从零开始，`number` 从一开始。

## 组合

`{#define:'name'}...{#end}` 创建当前模板内的不可变定义。定义支持前向引用，同一名称不能重复。`{#include:expression}` 先查找本地定义，再调用 `xtemplateresolvefn` 查找外部模板。

解析回调的 `*pTemplate` 进入回调前为 `NULL`。成功且仍为 `NULL` 表示未找到；回调写入的任何非空引用都由渲染器接管，即使回调随后返回 `false`，渲染器也会释放该引用。外部模板按精确身份检测递归环，并受 `MaxIncludeDepth` 限制。

`{#raw}...{#end}` 原样输出主体，不解析其中的模板语法。需要在 raw 主体中放置看似结束标记的文本时，重复开始标记可以阻止它被识别为结束节点。

## 扩展

`xtemplateregistry` 是不可变、引用计数的扩展注册表。`xtemplateextensionfn` 是统一
扩展调用函数类型，`xtemplateextensiondrop` 是注册项用户数据的析构函数类型。
注册表最终释放时才调用析构函数。

`xtemplateextension` 描述一个扩展：

| 字段 | 含义 |
| --- | --- |
| `Name` | 明确长度名称；创建注册表时复制 |
| `Type` | 调用形态 |
| `MinArguments` / `MaxArguments` | 编译时参数数量约束 |
| `Call` | 渲染回调 |
| `Data` | 独立用户数据 |
| `Drop` | 注册表最终释放时的数据析构函数 |

`xtemplateextensiontype` 有四个明确值：

| 值 | 语法与主体 |
| --- | --- |
| `XTEMPLATE_EXTENSION_FUNCTION` | `{@name:args}`，行内函数 |
| `XTEMPLATE_EXTENSION_STATEMENT` | `{#name:args}`，无主体语句 |
| `XTEMPLATE_EXTENSION_BLOCK` | `{#name:args}...{#end}`，主体预编译为节点 |
| `XTEMPLATE_EXTENSION_RAW_BLOCK` | `{#name:args}...{#end}`，主体保留原始字节 |

`xrtTemplateRegistryCreate` 校验并复制全部描述和名称。只有创建完整成功后，注册表才接管每一项 `Data`；失败时仍由调用方处理所有数据。函数和语句使用独立名称空间，同名函数与语句可以同时注册。`xrtTemplateRegistryRef` 和 `xrtTemplateRegistryRelease` 管理不可变注册表引用。

参数用顶层冒号分隔，解析器会识别引号、转义和括号内的冒号。`name=expression` 是命名参数，其余是位置参数；命名参数不能重复，比较表达式可以使用 `==` 避免与命名符号混淆。表达式在模板编译时预编译，不在每次渲染时重新解析。

扩展回调通过 `xtemplatecall` 使用当前调用：

- `xrtTemplateCallName`、`xrtTemplateCallData` 返回扩展名称与描述中的用户数据。
- `xrtTemplateCallArgumentCount` 返回参数数。
- `xrtTemplateCallArgument` 按零基索引返回 `xtemplateargview`。
- `xrtTemplateCallFind` 查找命名参数；位置参数不会以空名称参与匹配。
- `xrtTemplateCallEval` 在当前作用域求值参数，结果写入 `xtemplatevalue`。
- `xrtTemplateCallWrite` 向共享 writer 写入一个明确长度分片。
- `xrtTemplateCallRender` 渲染解析块主体；可以调用零次或多次。
- `xrtTemplateCallRenderCurrent` 使用临时 Current 渲染解析块主体。
- `xrtTemplateCallRaw` 返回原样块主体；其他类型返回空视图。
- `xrtTemplateCallCurrent`、`xrtTemplateCallRoot`、`xrtTemplateCallGlobal` 返回借用作用域值。

`xtemplateargview` 的 `Index` 是稳定句柄，`Name` 和 `Source` 借用模板。`xtemplatevalue` 的 `Type` 决定 `Value`、`Bool`、`Integer`、`Unsigned`、`Float`、`Text` 或 `Time` 中哪些字段有效；文本和值仍由输入数据或模板拥有。`xtemplatecall` 及其返回视图只在当前回调期间有效，不得保存到回调外。

回调返回 `false` 时，如果已经设置 `xrt.template` 错误，该错误直接传播；其他错误会作为 cause 包装为 `XTEMPLATE_ERROR_CALLBACK` 并补充调用位置；未设置错误时创建 callback 错误。成功回调产生的临时错误会被丢弃，并恢复进入回调前的线程错误。`break` 和 `continue` 不能越过扩展回调边界。

注册表与模板都不可变且可跨线程共享。共享扩展的 `Data` 和 `Call` 必须由扩展实现保证并发只读或自行同步。

### `xrtTemplateCallData`

返回当前扩展调用名称和描述项携带的用户数据。

```c
ptr xrtTemplateCallData(const xtemplatecall* pCall)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCall` | 输入 | 非空 | 当前调用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 值 | 扩展描述的 `UserData` 原样返回 | — |
| `NULL` | 未携带 | 不设错误 |

#### 错误

- 无错误 — 未携带数据返回 `NULL` 且不设置错误

#### 范例

[tour](../../examples/template/tour/main.c) · 调用数据

```c
	examplectx* pCtx = (examplectx*)xrtTemplateCallData(pCall);
```

### `xrtTemplateCallName`

返回当前扩展调用的名称视图。

```c
xstrview xrtTemplateCallName(const xtemplatecall* pCall)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCall` | 输入 | 非空 | 当前调用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 名称借用（回调期间有效） | — |
| 空视图 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tour](../../examples/template/tour/main.c) · 调用名称

```c
	if ( (xrtTemplateCallName(pCall).Size != 5u) ||
		(memcmp(xrtTemplateCallName(pCall).Data, "shout",
			5u) != 0) ||
		(xrtTemplateCallArgumentCount(pCall) != 1u) ) {
```

### `xrtTemplateCallRaw`

返回原样主体和当前、根、全局作用域的借用视图。

```c
xstrview xrtTemplateCallRaw(const xtemplatecall* pCall)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCall` | 输入 | 非空 | 当前调用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 原样主体借用 | — |
| 空视图 | 无主体或参数非法 | 不设错误 |

#### 错误

- 无主体返回空视图且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/template/tour/main.c) · 原样主体

```c
	(void)xrtTemplateCallRaw(pCall);
```

### `xrtTemplateCallCurrent`

返回当前渲染作用域的借用数据值。

```c
const xvalue* xrtTemplateCallCurrent(const xtemplatecall* pCall)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCall` | 输入 | 非空 | 当前调用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 当前值借用（回调期间有效） | — |
| `NULL` | 无当前值 | 不设错误 |

#### 错误

- 无当前值返回 `NULL` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/template/tour/main.c) · 当前作用域值

```c
		(xrtTemplateCallCurrent(pCall) == NULL) ) {
```

### `xrtTemplateCallRoot`

返回根作用域的借用数据值。

```c
const xvalue* xrtTemplateCallRoot(const xtemplatecall* pCall)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCall` | 输入 | 非空 | 当前调用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 根值借用（回调期间有效） | — |
| `NULL` | 无根值 | 不设错误 |

#### 错误

- 无根值返回 `NULL` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/template/tour/main.c) · 根作用域值

```c
	pRoot = xrtTemplateCallRoot(pCall);
```

### `xrtTemplateCallGlobal`

返回全局作用域的借用数据值。

```c
const xvalue* xrtTemplateCallGlobal(const xtemplatecall* pCall)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCall` | 输入 | 非空 | 当前调用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 全局值借用（回调期间有效） | — |
| `NULL` | 无全局值 | 不设错误 |

#### 错误

- 无全局值返回 `NULL` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/template/tour/main.c) · 全局作用域值

```c
	pGlobal = xrtTemplateCallGlobal(pCall);
```

### `xrtTemplateCallArgumentCount`

返回参数数量、指定位置参数或命名参数。

```c
size_t xrtTemplateCallArgumentCount(const xtemplatecall* pCall)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCall` | 输入 | 非空 | 当前调用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实参数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tour](../../examples/template/tour/main.c) · 参数数量

```c
		(xrtTemplateCallArgumentCount(pCall) != 1u) ) {
```

### `xrtTemplateCallArgument`

返回指定位置的实参视图。

```c
bool xrtTemplateCallArgument(const xtemplatecall* pCall, size_t iIndex, xtemplateargview* pArgument)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCall` | 输入 | 非空 | 当前调用 |
| `iIndex` | 输入 | < 实参数量 | 位置 |
| `pArgument` | 输出 | 非空 | 接收实参视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 位置越界 | `XERR_RANGE` |

#### 错误

- `XERR_RANGE` — 位置越界；句柄为空 `XERR_ARGUMENT`

#### 范例

[extension](../../examples/template/extension/main.c) · 按位取参

```c
	if ( !xrtTemplateCallArgument(pCall, 0u, &Argument) ||
		 !xrtTemplateCallEval(pCall, &Argument, &Value) ||
		 (Value.Type != XVALUE_STRING) ) {
```

### `xrtTemplateCallFind`

按名称查找实参视图。

```c
bool xrtTemplateCallFind(const xtemplatecall* pCall, xstrview Name, xtemplateargview* pArgument)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCall` | 输入 | 非空 | 当前调用 |
| `Name` | 输入 | 借用 | 参数名 |
| `pArgument` | 输出 | 非空 | 接收实参视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已找到并写出 | — |
| `false` | 未找到 | 不设错误 |

#### 错误

- 未找到返回 `false` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/template/tour/main.c) · 按名取参

```c
		xrtTemplateCallFind(pCall, SV("nope"), &Argument) ||
```

### `xrtTemplateCallEval`

在当前渲染作用域内求值参数，或通过共享 writer 写出分片。

```c
bool xrtTemplateCallEval(xtemplatecall* pCall, const xtemplateargview* pArgument, xtemplatevalue* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCall` | 输入/输出 | 非空 | 当前调用 |
| `pArgument` | 输入 | 非空 | 待求值实参 |
| `pValue` | 输出 | 非空 | 接收结果值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已求值 | — |
| `false` | 求值失败 | `xrt.template` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.template` / `XTEMPLATE_ERROR_TYPE` — 表达式类型不匹配
- `xrt.template` / `XTEMPLATE_ERROR_UNDEFINED` — 引用了未定义的名称
- `xrt.template` / `XTEMPLATE_ERROR_LIMIT` — 超出编译或渲染预算

#### 范例

[extension](../../examples/template/extension/main.c) · 求值参数

```c
		 !xrtTemplateCallEval(pCall, &Argument, &Value) ||
```

### `xrtTemplateCallWrite`

在当前渲染作用域内写出文本分片。

```c
bool xrtTemplateCallWrite(xtemplatecall* pCall, xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCall` | 输入/输出 | 非空 | 当前调用 |
| `Text` | 输入 | 借用 | 文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 写出失败 | `xrt.template` / `XTEMPLATE_ERROR_WRITE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.template` / `XTEMPLATE_ERROR_WRITE` — 输出预算耗尽或回调失败

#### 范例

[extension](../../examples/template/extension/main.c) · 写出分片

```c
	return xrtTemplateCallWrite(pCall, XRT_STR_LITERAL("<a href=\"")) &&
```

### `xrtTemplateCallRender`

渲染解析块主体，或临时替换当前值后渲染主体。

```c
bool xrtTemplateCallRender(xtemplatecall* pCall)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCall` | 输入/输出 | 非空 | 当前调用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已渲染 | — |
| `false` | 渲染失败 | `xrt.template` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.template` / `XTEMPLATE_ERROR_TYPE` — 表达式类型不匹配
- `xrt.template` / `XTEMPLATE_ERROR_UNDEFINED` — 引用了未定义的名称
- `xrt.template` / `XTEMPLATE_ERROR_LIMIT` — 超出编译或渲染预算

#### 范例

[extension](../../examples/template/extension/main.c) · 渲染主体

```c
		xrtTemplateCallRender(pCall) &&
```

### `xrtTemplateCallRenderCurrent`

临时替换当前值后渲染主体。

```c
bool xrtTemplateCallRenderCurrent(xtemplatecall* pCall, const xvalue* pCurrent)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCall` | 输入/输出 | 非空 | 当前调用 |
| `pCurrent` | 输入 | 非空 | 替换用的当前值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已渲染 | — |
| `false` | 渲染失败 | `xrt.template` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.template` / `XTEMPLATE_ERROR_TYPE` — 表达式类型不匹配
- `xrt.template` / `XTEMPLATE_ERROR_UNDEFINED` — 引用了未定义的名称
- `xrt.template` / `XTEMPLATE_ERROR_LIMIT` — 超出编译或渲染预算

#### 范例

[tour](../../examples/template/tour/main.c) · 替换当前值渲染

```c
	return xrtTemplateCallRenderCurrent(pCall, pCtx->pAlt);
```

## 渲染

`xtemplaterenderconfig` 的 `Root`、`Current`、`Global` 建立三个显式作用域；`MaxOutputBytes` 和 `MaxSteps` 限制所有层共享的总输出与执行步数。control 的 `MaxDepth`、`MaxLoopIterations`，compose 的 `Resolve`、`ResolveData`、`MaxIncludeDepth` 只在对应模块启用时出现。

`xtemplaterenderflag` 当前定义 `XTEMPLATE_STRICT_UNDEFINED`，把缺失路径从空输出
提升为 `XTEMPLATE_ERROR_UNDEFINED`。未启用严格模式时，缺失的普通输出为空；
类型错误、格式错误和控制表达式错误仍然失败。

`xrtTemplateWrite` 是基础流式入口。`xtemplatewritefn` 借用当前输出分片；回调返回 `false` 立即停止。已经交给 writer 的分片无法回滚。

`xrtTemplateRenderTo` 把结果追加到 `xstrbuf`，本次调用具有事务性：任何失败都会撤销本次追加，调用前已有内容保持不变。`xrtTemplateRender` 是常见路径 helper，把同一 `xvalue` 同时作为 Root 和 Current，返回零结尾字符串；调用方使用 `xrtFree` 释放，并可通过 `pSize` 取得包含嵌入零字节的精确长度。

同一 `xtemplate` 可以并发渲染，但每次渲染的配置、writer、输出构建器和数据访问必须独立。传入的 `xvalue` 不会被模板修改；调用方必须保证渲染期间数据有效，并保证共享数据可并发读取。

### `xrtTemplateRenderConfigInit`

初始化默认作用域和有限渲染预算；默认保留通用模板的原样动态输出。

```c
void xrtTemplateRenderConfigInit(xtemplaterenderconfig* pConfig)
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

[compose](../../examples/template/compose/main.c) · 渲染配置

```c
	xrtTemplateRenderConfigInit(&Render);
```

### `xrtTemplateRenderHtmlConfigInit`

初始化 HTML 渲染配置（对动态输出执行 HTML 转义）。

```c
void xrtTemplateRenderHtmlConfigInit(xtemplaterenderconfig* pConfig)
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

[tour](../../examples/template/tour/main.c) · HTML 渲染配置

```c
	xrtTemplateRenderHtmlConfigInit(&HtmlConfig);
```

### `xrtTemplateWrite`

把渲染分片写入回调；回调已经写出的内容不能回滚。

```c
bool xrtTemplateWrite(const xtemplate* pTemplate, const xtemplaterenderconfig* pConfig, xtemplatewritefn pWrite, ptr pUserData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTemplate` | 输入 | 非空 | 目标模板 |
| `pConfig` | 输入 | 非空 | 渲染配置 |
| `pWrite` | 输入 | 非空 | 字节写入回调 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已完整渲染 | — |
| `false` | 渲染或写出失败 | `xrt.template` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.template` / `XTEMPLATE_ERROR_TYPE` — 表达式类型不匹配
- `xrt.template` / `XTEMPLATE_ERROR_UNDEFINED` — 引用了未定义的名称
- `xrt.template` / `XTEMPLATE_ERROR_LIMIT` — 超出编译或渲染预算
- `xrt.template` / `XTEMPLATE_ERROR_CALLBACK` — 写入回调失败

#### 范例

[tour](../../examples/template/tour/main.c) · 流式渲染

```c
	if ( !xrtTemplateWrite(pTemplate, &RenderConfig,
			exampleWriter, &iTotal) ||
		(iTotal != 10u) ) {  /* "Hello xrt!" 共 10 字节 */
```

### `xrtTemplateRenderTo`

把渲染结果事务追加到字符串构建器。

```c
bool xrtTemplateRenderTo(const xtemplate* pTemplate, const xtemplaterenderconfig* pConfig, xstrbuf* pOutput)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTemplate` | 输入 | 非空 | 目标模板 |
| `pConfig` | 输入 | 非空 | 渲染配置 |
| `pOutput` | 输出 | 非空 | 字符串构建器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已完整追加 | — |
| `false` | 渲染失败 | `xrt.template` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.template` / `XTEMPLATE_ERROR_TYPE` — 表达式类型不匹配
- `xrt.template` / `XTEMPLATE_ERROR_UNDEFINED` — 引用了未定义的名称
- `xrt.template` / `XTEMPLATE_ERROR_LIMIT` — 超出编译或渲染预算
- `XERR_MEMORY` — 分配失败

#### 范例

[compose](../../examples/template/compose/main.c) · 渲染至构建器

```c
	if ( !xrtTemplateRenderTo(pTemplate, &Render, &Output) ) {
```

### `xrtTemplateRender`

使用当前值作为根和当前作用域，返回由 `xrtFree` 释放的字符串。

```c
str xrtTemplateRender(const xtemplate* pTemplate, const xvalue* pData, size_t* pSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTemplate` | 输入 | 非空 | 目标模板 |
| `pData` | 输入 | 允许空 | 根数据值 |
| `pSize` | 输出 | 允许空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾渲染结果，`xrtFree` 释放 | — |
| `NULL` | 渲染失败 | `xrt.template` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.template` / `XTEMPLATE_ERROR_TYPE` — 表达式类型不匹配
- `xrt.template` / `XTEMPLATE_ERROR_UNDEFINED` — 引用了未定义的名称
- `xrt.template` / `XTEMPLATE_ERROR_LIMIT` — 超出编译或渲染预算
- `XERR_MEMORY` — 分配失败

#### 范例

[control](../../examples/template/control/main.c) · 分配渲染

```c
	sOutput = xrtTemplateRender(pTemplate, pRoot, NULL);
```

### `xrtTemplateErrorLocation`

从模板错误的数据字段读取源码位置。

```c
bool xrtTemplateErrorLocation(const xerror* pError, xtemplatelocation* pLocation)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 非空、`xrt.template` 域 | 模板错误 |
| `pLocation` | 输出 | 非空 | 接收源码位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出位置 | — |
| `false` | 错误不含位置 | 不设错误 |

#### 错误

- 无位置数据返回 `false` 且不设置错误；指针为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/template/tour/main.c) · 错误定位

```c
			!xrtTemplateErrorLocation(pError, &Location) ||
```

## 检查

`xrtTemplateSource` 返回模板持有的原始源视图。`xrtTemplateNodeCount` 返回全部编译节点数，包括控制块内部节点。`xrtTemplateNode` 按索引填写 `xtemplatenodeview`：

- `Type` 使用 `xtemplatenodetype`。核心值是 `XTEMPLATE_NODE_TEXT`、
  `XTEMPLATE_NODE_OUTPUT`；control 增加 `XTEMPLATE_NODE_INLINE_IF`、
  `XTEMPLATE_NODE_IF`、`XTEMPLATE_NODE_FOR`、`XTEMPLATE_NODE_FOREACH`、
  `XTEMPLATE_NODE_BREAK`、`XTEMPLATE_NODE_CONTINUE`；compose 增加
  `XTEMPLATE_NODE_DEFINE`、`XTEMPLATE_NODE_INCLUDE`、`XTEMPLATE_NODE_RAW`；
  extension 增加 `XTEMPLATE_NODE_EXTENSION`。
- `Output` 使用 `xtemplateoutputtype`，通过 `XTEMPLATE_OUTPUT_TEXT`、
  `XTEMPLATE_OUTPUT_NUMBER`、`XTEMPLATE_OUTPUT_TIME` 区分字符串、数字和时间输出。
- `Location` 提供字节范围和行列。
- `Source`、`Expression`、`Format` 以及 compose 启用后的 `Name` 都是模板拥有的借用视图。

这些检查 API 提供稳定的只读结构信息，不公开内部 AST 指针，也不要求维护另一套序列化格式。

## 错误

`xtemplateerror` 定义 `xrt.template` 域中的稳定模板错误代码：

| 值 | 含义 |
| --- | --- |
| `XTEMPLATE_ERROR_CONFIG` | 配置、分隔符或注册描述无效 |
| `XTEMPLATE_ERROR_SYNTAX` | 模板或表达式语法错误 |
| `XTEMPLATE_ERROR_LIMIT` | 编译或渲染预算耗尽 |
| `XTEMPLATE_ERROR_UNDEFINED` | 严格模式下路径缺失 |
| `XTEMPLATE_ERROR_TYPE` | 值类型与操作不匹配 |
| `XTEMPLATE_ERROR_FORMAT` | 数字或时间格式无效 |
| `XTEMPLATE_ERROR_ITERATE` | 值不可遍历或迭代失败 |
| `XTEMPLATE_ERROR_WRITE` | writer 拒绝输出且未提供更具体错误 |
| `XTEMPLATE_ERROR_CALLBACK` | 外部扩展回调失败 |
| `XTEMPLATE_ERROR_INCLUDE` | 本地或外部包含解析失败 |
| `XTEMPLATE_ERROR_CYCLE` | 检测到模板包含环 |

`xtemplatelocation.Offset` 和 `Size` 是零基字节范围，`Line` 和 `Column` 从一开始。`xrtTemplateErrorLocation` 从模板错误中读取位置；没有位置或错误域不匹配时返回 `false`。内存不足保持统一 `XERR_MEMORY`，不为了包装错误再次分配。

## 示例

- `examples/template/core/main.c`：字符串、数字、时间的基础编译和输出。
- `examples/template/control/main.c`：条件、容器遍历、范围循环与 `break/continue`。
- `examples/template/compose/main.c`：前向定义、本地包含与外部解析器。
- `examples/template/extension/main.c`：四类扩展与参数求值。
- `examples/template/file/main.c`：一行式文件模板编译。
