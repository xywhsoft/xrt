# XTE 模板引擎重构设计文档

## 1. 文档目标

本文档用于定义 XRT 中 XTE 模板引擎的新一代设计方案。

本次重构目标：
- 保留现有模板语法主线与主要使用方式。
- 保留自定义语句能力，支持 `{#xxx}` 独立块与 `{#xxx}...{#end}` 闭合块。
- 去掉旧内部结构兼容负担，不再保留旧结构体视图。
- 在 `xrt` 内仅保留“解析 + 渲染后端”。
- `xlang` 未来如需“模板编译到 C”，在 `xlang` 内单独基于 AST 处理，不在 `xrt` 内实现第二后端。
- 控制最终体积，不因重构无节制膨胀。


## 2. 非目标

本次不做的事情：
- 不保留旧 `XTE_LiteObject` 公开字段兼容。
- 不保留旧 `XTE_TokenList / XTE_TokenItem / Actions / SubTemplates` 的外部可见布局。
- 不在 `xrt` 内实现 C 代码生成后端。
- 不为自定义语句提供 `elseif/else` 这类中间分支语法。
- 不引入额外第三方依赖。


## 3. 现状问题

当前实现主要问题集中在 [`lib/template.h`](D:/git/xrt/lib/template.h)：

- 词法、解析、表达式、路径解析、渲染、控制流全部堆在一个文件中。
- `xteMakeActions_ex()` 过大，且存在明显重复逻辑。
- `if/for/foreach` 的块结构大量依赖运行时扫描，而不是解析阶段一次成形。
- 路径解析、表达式求值、普通变量取值没有统一到同一套语义模型。
- 自定义语句机制不完整，现有“函数/回调”参数传递模型过于简陋。
- 错误处理大量直接写入输出文本，不利于宿主侧控制。
- 存在全局状态与缓存，线程模型和生命周期都不够干净。


## 4. 设计原则

- 语义前置：能在 parse 阶段确定的内容，不放到 render 阶段再猜。
- 结构收口：解析器产出 AST，渲染器只消费 AST，不再反复扫描 token 列表。
- 自定义语句一等公民：内建语句与扩展语句共享同一注册体系。
- 作用域清晰：局部变量、当前对象、根对象、全局环境分层明确。
- 输出流式化：渲染时直接写入 writer，避免中间字符串级联拼接。
- 体积优先：只保留一个渲染后端，不引入独立 IR 层，不保留旧视图兼容。
- 宿主友好：错误、include、函数、语句、脚本能力都通过显式接口接入。
- 开关克制：模板模块只新增两个主要编译开关，避免维护者心智负担上升。


## 5. 总体架构

新的 XTE 流程：

1. `Lexer`
2. `Parser`
3. `AST + 语义注解`
4. `Renderer`

说明：
- 不单独引入公开 IR 层。
- AST 节点在解析后会挂载“语义编译结果”，例如表达式树、路径树、语句 spec 指针、参数描述等。
- 渲染器直接消费“已编译 AST”。

### 5.1 为什么不做独立 IR

当前边界下，`xrt` 只有一个后端，即渲染后端。此时单独维护一套 IR 的收益不足以覆盖新增成本。

不做 IR 的收益：
- 代码量更小。
- 内存对象更少。
- 调试链路更短。
- 重构周期更可控。

不做 IR 的代价：
- 渲染器会直接理解 AST 语义。
- 统一 lower 优化能力弱于 AST -> IR -> Backend 三段式结构。
- 未来如果别的模块想复用更底层执行表示，需要额外适配。

在当前设计约束下，这个代价可接受。

结论：
- `xrt` 内采用“AST + 语义注解”。
- `xlang` 如果未来需要更低层表示，可以在自己的编译链里额外做 lowering，不把这层强塞回 `xrt`。


## 5.2 编译开关策略

本次重构不采用大量碎片化开关。

模板模块只保留两个主要扩展开关：

- `XTE_ENABLE_FILE`
	- 启用预编译模板保存到文件、从文件加载。
- `XTE_DEBUGMODE`
	- 启用内部结构调试输出能力，例如文本 dump、控制台输出、额外调试检查。

说明：
- 默认关闭这两个开关时，只保留“parse + render”核心能力。
- `XTE_DEBUGMODE` 下允许引入偏重的调试代码，但这些代码不得进入发布版本。
- 不再继续拆分更多模板子功能开关，避免配置组合过多导致维护复杂化。


## 6. 代码组织建议

建议仍保持 XRT 当前“`xrt.c` 聚合多个 `lib/*.h` 模块”的风格，但把模板内部拆开。

建议文件拆分：
- `lib/template.h`
	- 模板模块总入口，供 `xrt.c` 引入。
- `lib/template_base.h`
	- 基础定义、错误码、公共内部工具。
- `lib/template_lexer.h`
	- 词法分析。
- `lib/template_expr.h`
	- 表达式与路径解析。
- `lib/template_ast.h`
	- AST 节点、模板对象、参数对象。
- `lib/template_parser.h`
	- parser 与语义编译。
- `lib/template_render.h`
	- 渲染器、writer、运行时作用域。
- `lib/template_builtin.h`
	- 内建语句注册与实现。

要求：
- 外部公开 API 仍统一从 [`xrt.h`](D:/git/xrt/xrt.h) 导出。
- `xrt.c` 只 include [`lib/template.h`](D:/git/xrt/lib/template.h)。


## 7. 新的公开 API 设计

### 7.1 句柄类型

```c
typedef struct XTE_Engine_Struct* xteengine;
typedef struct XTE_Template_Struct* xtetemplate;
```

### 7.2 错误结构

```c
typedef struct
{
	int iCode;
	const char* sDesc;
	uint32 iLine;
	uint32 iColumn;
	uint32 iPos;
	uint32 iRefLine;
	uint32 iRefColumn;
	uint32 iRefPos;
} XTE_Error;
```

### 7.3 Writer 结构

```c
typedef struct
{
	int (*procWrite)(void* pUserData, const char* sText, size_t iSize);
	void* pUserData;
	size_t iWritten;
} XTE_Writer;
```

### 7.4 Parse / Render 选项

```c
typedef struct
{
	const char* sBracket;
	uint32 iFlags;
} XTE_ParseOptions;

typedef struct
{
	xvalue pRoot;
	xvalue pCurrent;
	xvalue pGlobal;
	xdict pIncludeMap;
	XTE_Writer* pWriter;
	uint32 iFlags;
} XTE_RenderOptions;
```

### 7.5 核心 API 草案

```c
XXAPI xteengine xteCreateEngine(void);
XXAPI void xteDestroyEngine(xteengine hEngine);

XXAPI int xteRegisterBuiltinStatements(xteengine hEngine);
XXAPI int xteRegisterStatement(xteengine hEngine, const struct XTE_StatementDef_Struct* pDef);
XXAPI int xteRegisterFunction(xteengine hEngine, const struct XTE_FunctionDef_Struct* pDef);

XXAPI xtetemplate xteParseEx(
	xteengine hEngine,
	const char* sText,
	size_t iSize,
	const XTE_ParseOptions* pOptions,
	XTE_Error* pError
);

XXAPI void xteDestroyTemplate(xtetemplate hTemplate);

XXAPI int xteRenderEx(
	xtetemplate hTemplate,
	const XTE_RenderOptions* pOptions,
	XTE_Error* pError
);

#ifdef XTE_ENABLE_FILE
XXAPI int xteTemplateSaveFile(xtetemplate hTemplate, const char* sFilePath, uint32 iFlags, XTE_Error* pError);
XXAPI xtetemplate xteTemplateLoadFile(xteengine hEngine, const char* sFilePath, uint32 iFlags, XTE_Error* pError);
#endif

#ifdef XTE_DEBUGMODE
XXAPI int xteTemplateDump(xtetemplate hTemplate, XTE_Writer* pWriter, uint32 iFlags);
XXAPI int xteTemplateDumpConsole(xtetemplate hTemplate, uint32 iFlags);
#endif

XXAPI char* xteMake(
	xtetemplate hTemplate,
	xvalue pCurrent,
	xvalue pGlobal,
	xdict pIncludeMap,
	size_t* pRetSize
);

XXAPI xvalue xteResolvePath(
	const char* sPath,
	size_t iPathSize,
	xvalue pCurrent,
	xvalue pRoot,
	xvalue pLocal,
	xvalue pGlobal
);
```

### 7.6 兼容保留 API

为了兼顾现有使用方式，建议保留这些便捷 API：

```c
XXAPI xtetemplate xteParse(char* sText, size_t iSize, char* sBracket);
XXAPI char* xteMake(xtetemplate hTemplate, xvalue pCurrent, xvalue pGlobal, xdict pIncludeMap, size_t* pRetSize);
```

但内部实现不再依赖旧结构体。


## 8. 模板对象与 AST 模型

### 8.1 模板对象

```c
typedef struct XTE_Template_Struct
{
	xteengine hEngine;
	int bFrozen;
	XTE_Error LastError;

	struct XTE_NodeList_Struct RootNodes;
	xdict_struct tblSubTemplates;

	const char* sSourceText;
	size_t iSourceSize;
} XTE_Template_Struct;
```

说明：
- `RootNodes` 为主模板节点列表。
- `tblSubTemplates` 保存 `{#define:name}...{#end}` 编译出的子模板块。
- 模板在 parse 完成后即视为只读。

### 8.2 AST 节点类型

```c
typedef enum
{
	XTE_NODE_TEXT = 1,
	XTE_NODE_OUTPUT,
	XTE_NODE_INLINE_BOOL,
	XTE_NODE_SUBTEMPLATE,
	XTE_NODE_ARRAY_RENDER,
	XTE_NODE_INCLUDE,
	XTE_NODE_IF,
	XTE_NODE_FOR,
	XTE_NODE_FOREACH,
	XTE_NODE_BREAK,
	XTE_NODE_CONTINUE,
	XTE_NODE_STATEMENT
} XTE_NodeType;
```

说明：
- `XTE_NODE_OUTPUT` 统一表示 `{$...}`、`{%...}`、`{&...}` 这类“输出一个表达式”的节点，输出格式通过字段区分。
- `{?...}` 作为 `XTE_NODE_INLINE_BOOL` 保留。
- 自定义 `{#xxx}` 统一进入 `XTE_NODE_STATEMENT`。

### 8.3 AST 节点基础结构

```c
typedef struct XTE_Node_Struct
{
	XTE_NodeType Type;
	uint32 iFlags;
	uint32 iLine;
	uint32 iColumn;
	uint32 iPos;

	union
	{
		struct XTE_TextNode_Struct Text;
		struct XTE_OutputNode_Struct Output;
		struct XTE_InlineBoolNode_Struct InlineBool;
		struct XTE_IfNode_Struct IfStmt;
		struct XTE_ForNode_Struct ForStmt;
		struct XTE_ForeachNode_Struct ForeachStmt;
		struct XTE_StatementNode_Struct Statement;
	} Data;
} XTE_Node_Struct;
```

### 8.4 语义注解

每个节点在 parser 结束后完成语义注解：
- 输出节点持有已解析表达式树。
- 路径节点持有已分段路径对象。
- `if/for/foreach` 持有已编译条件和参数表达式。
- 自定义语句节点持有对应的 `StatementDef` 指针和私有 payload。
- 文本节点在 parse 阶段完成相邻文本合并。

这部分语义注解用于替代单独 IR 层。


## 8.5 预编译结果的存储策略

模板预编译结果的本体，不采用 `xvalue` 直接存储。

正式存储模型采用：
- 紧凑 AST
- 连续数组
- 字符串池
- 子模板索引
- 可选的语句私有编译数据

原因：
- 渲染热路径不能承担 `xvalue` 作为结构描述带来的额外查表和堆分配成本。
- 连续数组 + 偏移量更适合直接保存成文件。
- 释放逻辑更简单，也更利于后续体积控制。

但同时，模板内部结构不能变成完全不可见黑盒。

因此采用“双层方案”：
- 正式运行结构：紧凑 AST
- 调试查看结构：仅在 `XTE_DEBUGMODE` 下按需 dump 为文本或控制台输出

这样可以同时满足：
- 发布版本轻量
- 开发和维护时仍然可以直接查看内部数据结构


## 8.6 内部可见性策略

为了兼顾维护便利性与发布体积，建议提供三层可见性能力：

第一层：常驻轻量访问器
- 始终可用
- 用于按索引查看节点、表达式、字符串池信息

第二层：文本 dump
- 仅在 `XTE_DEBUGMODE` 下启用
- 适合快速查看 AST 层次和语义注解结果

结论：
- 调试输出是文本视图，不是模板编译结果的本体格式
- 发布版本不承担这部分死重代码


## 9. 表达式与路径模型

### 9.1 统一原则

必须统一以下场景：
- `{$user.name}`
- `{%items[0].price}`
- `{#if:user.age >= 18}`
- 自定义函数参数中的路径表达式
- 自定义语句参数中的表达式

统一方案：
- 路径解析是表达式系统的子集。
- 纯路径表达式可降为轻量 `PathExpr`。
- 更复杂的逻辑表达式走 `ExprAst`。

### 9.2 表达式节点

保留当前表达式体系的核心能力：
- 数字、字符串、布尔字面量
- 路径引用
- `= != ~= > < >= <=`
- `and or not`
- 括号

建议暂不在第一阶段扩展更多运算符，避免范围失控。

### 9.3 路径解析顺序

新的查找顺序建议为：
- `Local`
- `Current`
- `Root`
- `Global`

说明：
- `Local` 用于循环变量和语句局部变量。
- `Current` 表示当前渲染对象。
- `Root` 表示渲染根对象。
- `Global` 表示宿主注入环境，例如函数、常量、全局配置。


## 10. 运行时上下文

```c
typedef struct XTE_RenderScope_Struct
{
	xvalue pLocal;
	xvalue pCurrent;
	xvalue pRoot;
	xvalue pGlobal;
} XTE_RenderScope;

typedef struct XTE_RenderCtx_Struct
{
	xteengine hEngine;
	xtetemplate hTemplate;
	XTE_Writer* pWriter;
	xdict pIncludeMap;
	XTE_RenderScope Scope;
	XTE_Error* pError;
	uint32 iFlags;
} XTE_RenderCtx;
```

说明：
- 不再沿用旧的 `tblVal / tblRoot / tblENV` 混合风格。
- 所有渲染 API 和回调都通过 `XTE_RenderCtx` 取作用域。


## 11. 自定义语句系统

### 11.1 目标

用户可以注册自定义语句 `xxx`，并支持：
- `{#xxx}` 独立块
- `{#xxx}...{#end}` 闭合块
- 同时支持两种形式

### 11.2 语句类型标记

```c
#define XTE_STMT_INLINE		0x0001
#define XTE_STMT_BLOCK		0x0002
#define XTE_STMT_HYBRID		(XTE_STMT_INLINE | XTE_STMT_BLOCK)
#define XTE_STMT_RAW_BODY	0x0004
#define XTE_STMT_ALLOW_NAMED_ARGS	0x0008
```

说明：
- `INLINE` 只允许 `{#xxx}`。
- `BLOCK` 只允许 `{#xxx}...{#end}`。
- `HYBRID` 同时允许两种形式。
- `RAW_BODY` 表示块体不递归解析模板语法，而是按原始文本整体交给回调。

### 11.3 语句定义结构

```c
typedef struct XTE_StatementDef_Struct
{
	const char* sName;
	uint32 iFlags;
	uint16 iMinArgs;
	uint16 iMaxArgs;
	void* pUserData;

	int (*procParse)(struct XTE_StmtParseCtx_Struct* pCtx, void** ppData);
	int (*procRender)(struct XTE_StmtRenderCtx_Struct* pCtx);
	void (*procFreeData)(void* pData);
} XTE_StatementDef;
```

### 11.4 语句节点结构

```c
typedef struct XTE_StatementNode_Struct
{
	const XTE_StatementDef* pDef;
	struct XTE_ArgList_Struct Args;
	struct XTE_NodeList_Struct Body;
	const char* sRawBody;
	size_t iRawBodySize;
	void* pData;
	int bHasBody;
} XTE_StatementNode;
```

### 11.5 `HYBRID` 模式的解析规则

对于同时支持独立块与闭合块的语句：

- parser 先按“块语句”尝试匹配同层 `#end`。
- 找到则按 block 解析。
- 找不到则退回 inline 解析。

该逻辑在解析阶段完成，不在运行阶段猜测。

### 11.6 自定义语句的边界

自定义语句只支持两种闭合模型：
- 独立块
- `#end` 闭合块

不支持用户自定义中间分支标记，例如：
- `elseif`
- `else`
- `catch`

这些仍属于内建语法家族，由 parser 专门处理。


## 12. 回调参数传递模型

这是本次重构的重点之一。

结论：
- 回调不能再只拿原始字符串片段。
- 参数必须在 parse 阶段结构化。
- 渲染时允许按需求值，而不是所有参数都提前字符串化。

### 12.1 参数项结构

```c
typedef struct
{
	const char* sName;
	size_t iNameSize;

	const char* sRawText;
	size_t iRawSize;

	uint32 iFlags;
	struct XTE_ExprNode_Struct* pExpr;
} XTE_ArgItem;
```

说明：
- `sRawText` 总是保留，便于完全自定义解析。
- `pExpr` 在“参数可表达式化”时由 parser 预编译。
- `sName` 用于命名参数，例如 `ttl=60`。

### 12.2 参数列表结构

```c
typedef struct XTE_ArgList_Struct
{
	XTE_ArgItem* pItems;
	uint32 iCount;
} XTE_ArgList;
```

### 12.3 命名参数策略

建议支持，但默认只在语句定义声明 `XTE_STMT_ALLOW_NAMED_ARGS` 时启用。

原因：
- 旧模板大量使用 `:` 分段参数。
- 某些表达式天然包含 `=`。
- 不应在所有语句上强行把 `a = b` 解释成命名参数。

命名参数识别条件建议为：
- 只在显式允许命名参数的语句中启用。
- 参数片段整体满足 `identifier = expr` 形式。
- 左侧必须是合法标识符，且不存在空白歧义。

### 12.4 统一参数求值 helper

建议提供以下 helper API：

```c
XXAPI uint32 xteArgCount(const XTE_ArgList* pArgs);
XXAPI const XTE_ArgItem* xteArgAt(const XTE_ArgList* pArgs, uint32 iIndex);
XXAPI const XTE_ArgItem* xteFindNamedArg(const XTE_ArgList* pArgs, const char* sName, size_t iNameSize);

XXAPI xvalue xteEvalArgValue(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg);
XXAPI int xteEvalArgBool(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int* pOut);
XXAPI int xteEvalArgInt(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int64* pOut);
XXAPI int xteEvalArgFloat(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, double* pOut);
XXAPI char* xteEvalArgText(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg);
```

这样：
- 语句回调不需要重复解析参数字符串。
- 参数求值逻辑由模板引擎统一维护。


## 13. 语句回调与函数回调分离

建议明确分成两类回调。

### 13.1 语句回调

语句回调负责：
- 控制输出
- 控制是否渲染子块
- 控制流返回

建议返回值：

```c
typedef enum
{
	XTE_FLOW_OK = 0,
	XTE_FLOW_BREAK = 1,
	XTE_FLOW_CONTINUE = 2,
	XTE_FLOW_ERROR = -1
} XTE_Flow;
```

语句渲染上下文：

```c
typedef struct XTE_StmtRenderCtx_Struct
{
	XTE_RenderCtx* pRender;
	const XTE_StatementDef* pDef;
	const XTE_ArgList* pArgs;
	const struct XTE_NodeList_Struct* pBody;
	const char* sRawBody;
	size_t iRawBodySize;
	void* pData;
	void* pUserData;
} XTE_StmtRenderCtx;
```

建议 helper：

```c
XXAPI int xteStmtWrite(XTE_StmtRenderCtx* pCtx, const char* sText, size_t iSize);
XXAPI int xteStmtRenderBody(XTE_StmtRenderCtx* pCtx);
XXAPI int xteStmtRenderBodyWithScope(XTE_StmtRenderCtx* pCtx, xvalue pLocal, xvalue pCurrent);
```

### 13.2 函数回调

函数回调只负责“输入参数 -> 返回值”，不负责控制流，不直接操作块体。

```c
typedef struct XTE_FunctionDef_Struct
{
	const char* sName;
	uint16 iMinArgs;
	uint16 iMaxArgs;
	void* pUserData;
	int (*procCall)(struct XTE_FuncCtx_Struct* pCtx, xvalue* ppRet);
} XTE_FunctionDef;

typedef struct XTE_FuncCtx_Struct
{
	XTE_RenderCtx* pRender;
	const XTE_FunctionDef* pDef;
	const XTE_ArgList* pArgs;
	void* pUserData;
} XTE_FuncCtx;
```

说明：
- `{@xxx:...}` 建议走函数注册表。
- 表达式中未来如要支持函数调用，也复用同一套函数回调接口。

### 13.3 为什么必须分离

如果函数回调和语句回调混用，会导致：
- 控制流语义混乱
- 参数求值语义不清
- 输出路径难统一
- 块体语句很难优雅支持

因此必须拆开。


## 14. 内建语句设计

### 14.1 需要保留的内建语法

- `{#include:...}`
- `{#define:...}...{#end}`
- `{#if:...}...{#elseif:...}...{#else}...{#end}`
- `{#for:...}...{#end}`
- `{#foreach:...}...{#end}`
- `{#break}`
- `{#continue}`
- `{#script:...}...{#end}`

### 14.2 内建语句的实现层次

建议分两类：

第一类，parser 专门建模的控制家族：
- `if / elseif / else / end`
- `for / end`
- `foreach / end`
- `define / end`

第二类，普通注册语句：
- `include`
- `break`
- `continue`
- `script`
- 用户自定义 `xxx`

这样做的原因：
- `if/elseif/else` 本质上是多分支块结构，不适合作为普通“单回调语句”。
- `for/foreach/define` 也有更稳定的固定语义，直接内建更利于体积与可维护性。
- `script` 更适合作为 `RAW_BODY` 块语句的标准示例。

### 14.3 `script` 的位置

建议保留 `script` 名称兼容，但它不再拥有特殊散落实现。

建议实现方式：
- 作为一个预注册内建语句。
- 设置 `XTE_STMT_BLOCK | XTE_STMT_RAW_BODY`。
- 具体执行逻辑委托给宿主注册的脚本执行器回调。

如果宿主未注册脚本执行器：
- render 直接返回结构化错误。
- 不再往输出里拼“调试字符串”。


## 15. 渲染模型

### 15.1 统一 Writer

所有输出都通过 `XTE_Writer` 完成。

原因：
- 避免子块先生成临时字符串，再回填主缓冲区。
- 明显减少中间分配和拼接代码。
- 对最终程序大小和运行时内存都更友好。

`xteMake()` 只是默认的 `xbuffer writer` 包装器。

### 15.2 递归渲染规则

渲染器采用：
- 根节点列表递归遍历
- 子块直接向同一个 writer 写入
- 控制流通过 `XTE_Flow` 向上传播

要求：
- `break/continue` 只能在循环体内合法传播。
- 非法位置使用时，返回结构化 render error。

### 15.3 输出节点统一化

当前 `{$}`、`{%}`、`{&}` 分支逻辑重复较多。

新的做法：
- 统一为 `XTE_NODE_OUTPUT`
- 输出时根据 `OutputKind` 选择：
	- 文本输出
	- 数字格式化
	- 时间格式化

这样可以减少重复分支代码。


## 16. include / define / 子模板

### 16.1 define

`{#define:name}...{#end}` 在 parse 阶段抽成命名子模板。

规则：
- 子模板名称必须唯一。
- 默认不允许嵌套 define。
- 子模板本身仍保存为节点列表。

### 16.2 子模板调用

保留：
- `{=subtpl}`
- `{*items:subtpl}`

但内部不再直接靠 token 参数硬编码遍历，而是：
- parse 阶段生成专用节点
- render 阶段走统一子模板调用逻辑

### 16.3 include

建议保留当前“include map 传入已编译模板对象”的能力。

可选增强：
- engine 支持 include resolver 回调
- render 时若 `pIncludeMap` 未命中，再尝试 resolver

但第一阶段不是必需项。


## 17. 错误模型

### 17.1 原则

不能再把核心错误默认写到输出正文里。

新的错误处理原则：
- parse 错误通过 `XTE_Error` 返回
- render 错误通过 `XTE_Error` 返回
- 是否把错误渲染成可见文本，由宿主决定

### 17.2 错误分类

建议至少区分：
- 词法错误
- 语法错误
- 表达式编译错误
- 语句注册错误
- 语句参数错误
- 路径解析错误
- include 未命中
- 非法控制流错误
- 回调执行错误

### 17.3 错误位置

错误必须尽量附带：
- 行号
- 列号
- 源文本位置
- 参考位置

这样测试和宿主日志都更有用。


## 18. 线程安全与生命周期

### 18.1 取消全局状态

不再保留：
- 全局关键字表
- 全局表达式缓存
- 全局初始化标志

引擎实例负责持有：
- 语句注册表
- 函数注册表
- 可选脚本执行器
- 可选 include resolver

### 18.2 生命周期规则

- engine 创建后先注册内建/自定义语句和函数。
- template 由 engine parse 产生。
- template parse 完成后只读，可并发 render。
- engine 在关联 template 全部销毁前不应释放。

### 18.3 回调生命周期

语句和函数定义中的 `pUserData` 生命周期由宿主保证。

模板引擎只负责：
- 保存指针
- 在需要时回调
- 对 `procParse` 产出的 `pData` 调用 `procFreeData`


## 19. 程序体积评估

### 19.1 会缩小的部分

- 去掉旧 `XTE_LiteObject` 字段兼容。
- 去掉旧 token / action 公开布局。
- 去掉运行时块扫描与重复分支逻辑。
- 去掉大量“先生成子串再拼接”的中间字符串路径。
- 去掉全局缓存和相关清理逻辑。
- 不在 `xrt` 中实现 C 代码生成后端。
- 不引入独立 IR 层。

### 19.2 可能变大的部分

- 新增 engine / registry / writer / error 结构。
- 新增 AST 节点和 parser helper。
- 新增自定义语句参数结构化逻辑。
- 若启用 `XTE_DEBUGMODE`，会额外带入 dump / 控制台输出代码。
- 若启用 `XTE_ENABLE_FILE`，会额外带入序列化与反序列化代码。

### 19.3 体积判断

在“不保留旧结构兼容、不做 C 后端、不做独立 IR”的前提下：

- 源码组织会更细，但不代表最终程序更大。
- 模板模块最终体积大概率可以做到持平，甚至略降。
- 影响最终大小的关键，不是 AST 本身，而是是否仍保留旧执行路径与字符串拼接路径。

### 19.4 体积控制措施

- 输出统一走 writer。
- 节点结构尽量紧凑，避免节点级 heap 碎片。
- 文本节点 parse 阶段合并。
- 表达式和路径统一表示，避免双套求值器。
- 内建控制流保留专用实现，不对所有语句都走超泛化动态分派。
- `XTE_DEBUGMODE` 之外不编译 dump / 控制台调试输出代码。
- `XTE_ENABLE_FILE` 之外不编译模板文件读写代码。
- release 构建可选择裁剪详细错误文本，仅保留错误码。


## 19.5 开关取舍建议

建议按以下方式使用：

开发期：
- 开启 `XTE_DEBUGMODE`
- 需要预编译文件联调时，再开启 `XTE_ENABLE_FILE`

发布期：
- 默认关闭 `XTE_DEBUGMODE`
- 是否开启 `XTE_ENABLE_FILE` 由产品是否需要“模板预编译文件直载”决定

这样可保证：
- 日常调试能力足够
- 发布版本不背额外调试结构
- 配置组合简单，不增加太多维护负担


## 20. 迁移策略

### 阶段 1：搭框架

- 引入 `xteengine / xtetemplate / XTE_Error / XTE_Writer`
- 建立新的模板对象和 AST 类型
- 先不接旧 `xteMakeActions_ex`

### 阶段 2：表达式与路径统一

- 独立实现新表达式解析器
- 路径解析改为表达式系统子集
- 统一 `{$}`、`{%}`、`{&}` 的输入解析

### 阶段 3：基础渲染

- 完成文本节点
- 完成输出节点
- 完成 inline bool
- 完成子模板调用与 include

### 阶段 4：控制流

- 完成 `if`
- 完成 `for`
- 完成 `foreach`
- 完成 `break / continue`

### 阶段 5：自定义语句与函数

- 完成语句注册表
- 完成函数注册表
- 完成 `script` 标准扩展

### 阶段 6：兼容入口收口

- `xteParse()` 改为默认 engine 包装
- `xteMake()` 改为 buffer writer 包装
- 旧 lexer/token/action 相关 API 退为内部接口或直接删除

### 阶段 7：测试替换

- 用断言式测试替换当前 demo/printf 风格测试
- 覆盖路径、表达式、block、hybrid、自定义语句、错误定位


## 21. 测试计划

必须覆盖：
- 基础文本与转义
- 路径解析
- 表达式优先级
- `if / elseif / else`
- `for` 正向、反向、步长为 0、最大迭代数
- `foreach` 数组与表
- `break / continue`
- `define / include / 子模板`
- `script` raw body
- 自定义语句 `INLINE / BLOCK / HYBRID`
- 命名参数与位置参数
- 错误定位
- 多线程共享模板并发 render

建议新增一组专门测试：
- “旧语法兼容性样例”
- “新自定义语句能力样例”
- “非法模板拒绝样例”


## 22. 风险点

- `HYBRID` 语句的块匹配如果实现粗糙，容易造成误判。
- 命名参数识别若默认过宽，会破坏旧表达式兼容。
- `script` raw body 若词法边界处理不好，容易和 `#end` 匹配混淆。
- 如果 render 仍频繁创建中间字符串，体积和性能都会打回原形。
- 如果把所有内建语句都改成完全动态分派，可能增加代码路径和二进制体积。


## 23. 最终结论

本次重构建议采用以下最终决策：

- 仅保留一个后端：渲染后端。
- 不保留旧结构体公开兼容。
- 不在 `xrt` 内实现 C 代码生成。
- 不引入独立 IR 层。
- 采用“AST + 语义注解”模型。
- 采用 engine 实例化模型，去掉全局模板状态。
- 采用统一 writer 输出模型。
- 采用“语句回调”和“函数回调”分离模型。
- 自定义语句支持 `INLINE / BLOCK / HYBRID / RAW_BODY`。
- 作用域统一为 `Local / Current / Root / Global`。

如果按该方案实施，模板引擎将从“散落在一个大渲染函数中的解释逻辑”，收敛为“语法明确、扩展明确、作用域明确、体积可控”的稳定模块。
