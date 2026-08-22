# Template 模板引擎库

> 当前主线模板引擎，基于 `xvalue` 渲染，公开对象为 `xteengine / xtetemplate`

[English](api-template.en.md) | [中文](api-template.md) | [返回索引](README.md)

---

## 概览

当前版本的 XTE 已不再公开旧版的：

- `XTE_LiteObject`
- `XTE_TokenList`
- `XTE_TokenItem`
- `xteLexer()`
- `xteParseFromTokenList()`
- `xteMakeActions()`

现在的公开模型是：

- `xteengine`：模板引擎实例，负责注册内建语句、自定义语句、自定义函数
- `xtetemplate`：预编译模板对象
- `xteParseEx()`：解析模板，得到预编译结果
- `xteRenderEx()` / `xteMake()`：基于 `xvalue` 渲染输出

模板编译结果内部使用紧凑 AST 存储，可通过访问器间接查看结构；如定义 `XTE_DEBUGMODE`，还可以直接输出内部结构 dump。

---

## 功能范围

当前主线已支持：

- 文本输出 `{$expr}`
- 数字格式化 `{%expr:fmt}`
- 时间格式化 `{&expr:fmt}`
- 行内布尔分支 `{?expr:true:false}`
- 函数输出 `{@name:arg1:arg2}` / `{@name:key=value}`
- 自定义语句 `{#xxx}` / `{#xxx}...{#end}`
- 自定义混合语句 `XTE_STMT_HYBRID`
- 命名参数
- RAW_BODY 块
- 内建语句：
	- `if / elseif / else`
	- `for`
	- `foreach`
	- `break / continue`
	- `define`
	- `script`
	- `include`
- 预编译模板保存/加载（`XTE_ENABLE_FILE`）
- 调试结构输出（`XTE_DEBUGMODE`）

当前表达式层是轻量版本，主要支持：

- 路径：`user.name`
- 文本字面量：`'Alice'`、`"Alice"`
- 整数字面量：`123`
- 布尔字面量：`true` / `false`
- 比较运算：`=`、`!=`、`~=`、`>`、`<`、`>=`、`<=`
- 逻辑运算：`and`、`or`、`not`
- 括号分组：`(expr)`

也就是说，`if` / `elseif` 已支持轻量条件表达式，但仍不是旧版那套完整表达式语言。

---

## 定界符配置

模板默认使用单层 `{}` 作为定界符。

如果需要兼容 JS、JSON、HTML 片段嵌入，可以通过 `XTE_ParseOptions.sBracket` 改成其他等长开闭定界符：

```c
typedef struct {
	const char* sBracket;
	uint32 iFlags;
} XTE_ParseOptions;
```

示例：

- `"{}"` -> `{$name}`
- `"<>"` -> `<$name>`
- `"{{}}"` -> `{{$name}}`
- `"(())"` -> `(($name))`

注意：

- `sBracket` 是“前半段为开始符，后半段为结束符”的紧凑写法
- 长度必须为偶数
- 该配置只影响语法识别，不影响模板 AST 持久化格式

---

## 公开类型

### 句柄与基础结构

```c
typedef struct XTE_Engine_Struct* xteengine;
typedef struct XTE_Template_Struct* xtetemplate;
typedef struct XTE_RenderCtx_Struct XTE_RenderCtx;
typedef struct XTE_StmtParseCtx_Struct XTE_StmtParseCtx;
typedef struct XTE_StmtRenderCtx_Struct XTE_StmtRenderCtx;
typedef struct XTE_FuncCtx_Struct XTE_FuncCtx;
```

### 错误对象

```c
typedef struct {
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

### Writer

```c
typedef struct {
	int (*procWrite)(void* pUserData, const char* sText, size_t iSize);
	void* pUserData;
	size_t iWritten;
} XTE_Writer;
```

### 渲染配置

```c
typedef struct {
	xvalue pRoot;
	xvalue pCurrent;
	xvalue pGlobal;
	xdict pIncludeMap;
	XTE_Writer* pWriter;
	uint32 iFlags;
} XTE_RenderOptions;
```

### 访问器结构

```c
typedef struct {
	uint32 iStart;
	uint32 iCount;
} XTE_NodeSpan;

typedef struct {
	uint32 iType;
	uint32 iFlags;
	uint32 iTextOff;
	uint32 iTextSize;
	int64 iIntValue;
	int iBoolValue;
} XTE_ExprNode;

typedef struct {
	uint32 iNameOff;
	uint32 iNameSize;
	uint32 iRawOff;
	uint32 iRawSize;
	uint32 iFlags;
	uint32 iExprIndex;
} XTE_ArgItem;
```

这些结构是“只读访问视图”，用于查看预编译模板数据，不代表旧版公开布局兼容。

---

## 核心 API

### 创建 / 销毁引擎

```c
XXAPI xteengine xteCreateEngine(void);
XXAPI void xteDestroyEngine(xteengine hEngine);
XXAPI int xteRegisterBuiltinStatements(xteengine hEngine);
```

说明：

- `xteCreateEngine()` 默认会注册内建语句
- `xteRegisterBuiltinStatements()` 是幂等的，可重复调用

### 注册自定义扩展

```c
XXAPI int xteRegisterStatement(xteengine hEngine, const XTE_StatementDef* pDef);
XXAPI int xteRegisterFunction(xteengine hEngine, const XTE_FunctionDef* pDef);
```

其中：

- 自定义语句通过 `{#name}` / `{#name}...{#end}` 调度
- 自定义函数通过 `{@name:arg1:arg2}` 或 `{@name:key=value}` 调度
- 当前函数标签支持位置参数和命名参数

### 解析与销毁模板

```c
XXAPI xtetemplate xteParseEx(
	xteengine hEngine,
	const char* sText,
	size_t iSize,
	const XTE_ParseOptions* pOptions,
	XTE_Error* pError
);

XXAPI xtetemplate xteParse(const char* sText, size_t iSize, const char* sBracket);
XXAPI void xteDestroyTemplate(xtetemplate hTemplate);
XXAPI void xteParseFree(xtetemplate hTemplate);
```

### 渲染

```c
XXAPI int xteRenderEx(xtetemplate hTemplate, const XTE_RenderOptions* pOptions, XTE_Error* pError);
XXAPI char* xteMake(xtetemplate hTemplate, xvalue pCurrent, xvalue pGlobal, xdict pIncludeMap, size_t* pRetSize);
```

说明：

- `xteRenderEx()` 面向任意 writer
- `xteMake()` 是“输出到字符串”的便捷包装
- `pCurrent` 同时也是默认 `pRoot`
- `pGlobal` 用于宿主注入全局环境
- `pIncludeMap` 用于 `{#include}` 查找子模板

### 路径解析

```c
XXAPI xvalue xteResolvePath(
	const char* sPath,
	size_t iPathSize,
	xvalue pCurrent,
	xvalue pRoot,
	xvalue pLocal,
	xvalue pGlobal
);
```

当前查找顺序：

- `Local`
- `Current`
- `Root`
- `Global`

---

## 基本示例

```c
xteengine hEngine = xteCreateEngine();
xtetemplate hTemplate = NULL;
xvalue tblData = xvoCreateTable();
xvalue tblUser = xvoCreateTable();
char* sResult = NULL;
size_t iRetSize = 0;

xvoTableSetText(tblUser, "name", 0, "Alice", 0, FALSE);
xvoTableSetValue(tblData, "user", 0, tblUser, TRUE);
xvoTableSetBool(tblData, "active", 0, TRUE);

hTemplate = xteParseEx(
	hEngine,
	"Hello {$user.name}! {#if:active}ON{#else}OFF{#end}",
	0,
	NULL,
	NULL
);

sResult = xteMake(hTemplate, tblData, NULL, NULL, &iRetSize);
printf("%s\n", sResult);	// Hello Alice! ON

xrtFree(sResult);
xteDestroyTemplate(hTemplate);
xvoUnref(tblData);
xteDestroyEngine(hEngine);
```

---

## 内建语句

### if / elseif / else

```text
{#if:active}
	ON
{#elseif:(count >= 1000000) and active}
	BIG
{#elseif:false}
	OFF-2
{#else}
	OFF
{#end}
```

### for

```text
{#for:1:3:1}
	{%__index__}
{#end}
```

输出：

```text
123
```

### foreach

```text
{#foreach:users}
	{$name}
{#end}
```

循环局部变量：

- `__index__`
- `__value__`
- `__key__`（仅遍历 table 时可用）

### break / continue

```text
{#foreach:users}
	{#if:skip}{#continue}{#end}
	{$name}
	{#if:stop}{#break}{#end}
{#end}
```

### define / 子模板

```text
A{#define:'card'}[{$user.name}]{#end}B{#include:'card'}C
```

`define` 是编译期登记的块语句：

- `define` 自身不会直接输出内容
- 其 `body` 会登记到当前模板对象的本地子模板表
- 后续 `{#include:'card'}` 会优先命中本地 `define`

支持两种写法：

- `{#define:'card'}...{#end}`
- `{#define:name='card'}...{#end}`

子模板名当前要求是静态的文本或标识符路径样式名字，不能依赖运行时值。

---

### include

```text
X{#include:'card'}Y
```

`include` 的查找顺序是：

- 先查当前模板内部由 `define` 登记的本地子模板
- 未命中时，再查 `pIncludeMap`

外部模板映射示例：

```c
xdict tblInclude = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
xrtDictSetPtr(tblInclude, "card", 0, hCardTemplate, NULL);
```

---

### script

```text
{#script}{#if:active}{$user.name}{#end}{#end}
```

`script` 是内建 `RAW_BODY` 块语句：

- block 内部不会继续按模板语法递归解析
- 渲染时原样输出 raw body
- 适合嵌入脚本片段、模板源码片段、或需要延迟处理的内容

---

## 自定义语句

自定义语句使用 `XTE_StatementDef` 注册：

```c
typedef struct XTE_StatementDef_Struct {
	const char* sName;
	uint32 iFlags;
	uint16 iMinArgs;
	uint16 iMaxArgs;
	void* pUserData;

	int (*procParse)(XTE_StmtParseCtx* pCtx, void** ppData);
	XTE_Flow (*procRender)(XTE_StmtRenderCtx* pCtx);
	void (*procFreeData)(void* pData);
} XTE_StatementDef;
```

常用标记：

- `XTE_STMT_INLINE`
- `XTE_STMT_BLOCK`
- `XTE_STMT_HYBRID`
- `XTE_STMT_RAW_BODY`
- `XTE_STMT_ALLOW_NAMED_ARGS`

示例：

```text
{#wrap:tag='section'}Body{#end}
{#repeat:3:'ha'}
```

### 语句回调常用辅助函数

```c
XXAPI uint32 xteArgCount(const XTE_ArgList* pArgs);
XXAPI const XTE_ArgItem* xteArgAt(const XTE_ArgList* pArgs, uint32 iIndex);
XXAPI const XTE_ArgItem* xteFindNamedArg(const XTE_ArgList* pArgs, const char* sName, size_t iNameSize);
XXAPI const char* xteArgNameText(const XTE_ArgList* pArgs, const XTE_ArgItem* pArg);
XXAPI const char* xteArgRawText(const XTE_ArgList* pArgs, const XTE_ArgItem* pArg);
XXAPI uint32 xteArgExprType(const XTE_ArgList* pArgs, const XTE_ArgItem* pArg);

XXAPI xvalue xteEvalArgValue(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg);
XXAPI int xteEvalArgBool(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int* pOut);
XXAPI int xteEvalArgInt(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int64* pOut);
XXAPI int xteEvalArgFloat(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, double* pOut);
XXAPI char* xteEvalArgText(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg);
XXAPI int xteEvalArgBoolStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int* pOut);
XXAPI int xteEvalArgIntStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int64* pOut);
XXAPI int xteEvalArgFloatStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, double* pOut);
XXAPI char* xteEvalArgTextStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg);

XXAPI const XTE_ArgItem* xteStmtParseRequireArg(XTE_StmtParseCtx* pCtx, uint32 iIndex, const char* sDesc);
XXAPI const XTE_ArgItem* xteStmtParseRequireNamedArg(XTE_StmtParseCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
XXAPI const XTE_ArgItem* xteStmtParseRequireExprType(XTE_StmtParseCtx* pCtx, uint32 iIndex, uint32 iExprType, const char* sDesc);
XXAPI const XTE_ArgItem* xteStmtParseRequireNamedExprType(XTE_StmtParseCtx* pCtx, const char* sName, size_t iNameSize, uint32 iExprType, const char* sDesc);
XXAPI const XTE_ArgItem* xteStmtRequireArg(XTE_StmtRenderCtx* pCtx, uint32 iIndex, const char* sDesc);
XXAPI const XTE_ArgItem* xteStmtRequireNamedArg(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
XXAPI int xteStmtRequireBoolStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, int* pOut, const char* sDesc);
XXAPI int xteStmtRequireNamedBoolStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, int* pOut, const char* sDesc);
XXAPI int xteStmtRequireIntStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, int64* pOut, const char* sDesc);
XXAPI int xteStmtRequireNamedIntStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, int64* pOut, const char* sDesc);
XXAPI int xteStmtRequireFloatStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, double* pOut, const char* sDesc);
XXAPI int xteStmtRequireNamedFloatStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, double* pOut, const char* sDesc);
XXAPI char* xteStmtRequireTextStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, const char* sDesc);
XXAPI char* xteStmtRequireNamedTextStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
XXAPI const XTE_ArgItem* xteFuncRequireArg(XTE_FuncCtx* pCtx, uint32 iIndex, const char* sDesc);
XXAPI const XTE_ArgItem* xteFuncRequireNamedArg(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
XXAPI int xteFuncRequireBoolStrict(XTE_FuncCtx* pCtx, uint32 iIndex, int* pOut, const char* sDesc);
XXAPI int xteFuncRequireNamedBoolStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, int* pOut, const char* sDesc);
XXAPI int xteFuncRequireIntStrict(XTE_FuncCtx* pCtx, uint32 iIndex, int64* pOut, const char* sDesc);
XXAPI int xteFuncRequireNamedIntStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, int64* pOut, const char* sDesc);
XXAPI int xteFuncRequireFloatStrict(XTE_FuncCtx* pCtx, uint32 iIndex, double* pOut, const char* sDesc);
XXAPI int xteFuncRequireNamedFloatStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, double* pOut, const char* sDesc);
XXAPI char* xteFuncRequireTextStrict(XTE_FuncCtx* pCtx, uint32 iIndex, const char* sDesc);
XXAPI char* xteFuncRequireNamedTextStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);

XXAPI int xteStmtParseSetError(XTE_StmtParseCtx* pCtx, int iCode, const char* sDesc);
XXAPI XTE_Flow xteStmtSetError(XTE_StmtRenderCtx* pCtx, int iCode, const char* sDesc);
XXAPI int xteFuncSetError(XTE_FuncCtx* pCtx, int iCode, const char* sDesc);

XXAPI int xteStmtWrite(XTE_StmtRenderCtx* pCtx, const char* sText, size_t iSize);
XXAPI int xteStmtRenderBody(XTE_StmtRenderCtx* pCtx);
XXAPI int xteStmtRenderBodyWithScope(XTE_StmtRenderCtx* pCtx, xvalue pLocal, xvalue pCurrent);
```

这 3 个错误辅助接口的用法约定是：

- `xteStmtParseSetError()` 适合在 `procParse()` 中直接 `return xteStmtParseSetError(...)`
- `xteStmtSetError()` 适合在 `procRender()` 中直接 `return xteStmtSetError(...)`
- `xteFuncSetError()` 适合在 `procCall()` 中直接 `return xteFuncSetError(...)`

建议优先使用这组参数辅助接口：

- `xteStmtParseRequireArg()` / `xteStmtParseRequireNamedArg()`：parse 回调里做缺参校验
- `xteStmtParseRequireExprType()` / `xteStmtParseRequireNamedExprType()`：parse 回调里一步完成“取参 + 表达式类型校验 + 设错”
- `xteStmtRequireArg()` / `xteStmtRequireNamedArg()`：语句渲染回调里做缺参校验
- `xteFuncRequireArg()` / `xteFuncRequireNamedArg()`：函数回调里做缺参校验
- `xteArgExprType()`：检查参数是路径、文本字面量、整型、布尔或布尔表达式
- `xteArgRawText()`：读取参数的原始源码文本
- `xteEvalArgBool/Int/Float/Text()`：宽松转换，适合兼容型模板
- `xteEvalArgBoolStrict/IntStrict/FloatStrict/TextStrict()`：严格类型校验，不做文本转数值、数值转文本
- `xteStmtRequire*Strict()` / `xteStmtRequireNamed*Strict()`：语句回调里一步完成“缺参校验 + 严格类型校验 + 设错”
- `xteFuncRequire*Strict()` / `xteFuncRequireNamed*Strict()`：函数回调里一步完成“缺参校验 + 严格类型校验 + 设错”

示例：

```c
static int procParse(XTE_StmtParseCtx* pCtx, void** ppData)
{
	const XTE_ArgItem* pArg = xteStmtParseRequireExprType(pCtx, 0, XTE_EXPR_TEXT, "prefix requires text literal");

	if ( pArg == NULL ) {
		return 0;
	}
	ppData[0] = xrtCopyStr(xteArgRawText(pCtx->pArgs, pArg), pArg->iRawSize);
	return (ppData[0] != NULL);
}
```

---

## 模板结构访问器

```c
XXAPI uint32 xteTemplateGetNodeCount(xtetemplate hTemplate);
XXAPI uint32 xteTemplateGetExprCount(xtetemplate hTemplate);
XXAPI uint32 xteTemplateGetArgCount(xtetemplate hTemplate);
XXAPI uint32 xteTemplateGetStringPoolSize(xtetemplate hTemplate);
XXAPI XTE_NodeSpan xteTemplateGetRootSpan(xtetemplate hTemplate);
XXAPI const XTE_Node* xteTemplateGetNode(xtetemplate hTemplate, uint32 iIndex);
XXAPI const XTE_ExprNode* xteTemplateGetExpr(xtetemplate hTemplate, uint32 iIndex);
XXAPI const XTE_ArgItem* xteTemplateGetArg(xtetemplate hTemplate, uint32 iIndex);
XXAPI const char* xteTemplateGetString(xtetemplate hTemplate, uint32 iOff);
```

这套接口用于：

- 间接查看预编译结果
- 调试自定义语句的 parse 数据
- 在不暴露内部实现布局的前提下做结构分析

---

## 条件编译开关

模板模块当前只保留两个扩展开关：

### `XTE_ENABLE_FILE`

启用预编译模板保存与加载：

```c
XXAPI int xteTemplateSaveFile(xtetemplate hTemplate, const char* sFilePath, uint32 iFlags, XTE_Error* pError);
XXAPI xtetemplate xteTemplateLoadFile(xteengine hEngine, const char* sFilePath, uint32 iFlags, XTE_Error* pError);
```

文件加载行为补充：

- 加载后会按当前 `engine` 的语句注册表重新绑定 `StatementDef`
- 若某语句带 `procParse()`，加载时会重新执行它以重建私有 `pData`
- 若文件里引用了当前 `engine` 未注册的自定义语句，`xteTemplateLoadFile()` 会直接返回 load error，而不是拖到 render 阶段

### `XTE_DEBUGMODE`

启用内部结构调试输出：

```c
XXAPI int xteTemplateDump(xtetemplate hTemplate, XTE_Writer* pWriter, uint32 iFlags);
XXAPI int xteTemplateDumpConsole(xtetemplate hTemplate, uint32 iFlags);
```

`DEBUGMODE` 目的仅是方便查看内部结构，不参与发布版热路径。

当前 dump 输出会包含：

- 节点类型
- 语句名 / 函数名
- 语句参数 / 函数参数的原始文本
- 参数对应的编译表达式类型和值
- 普通输出 / 行内布尔分支的表达式类型标记，例如 `OUTPUT(PATH): user.name`

---

## 当前注意事项

- 当前表达式系统已支持路径、字面量、比较运算、`and / or / not`、括号，但仍不等同于旧版完整条件表达式语言
- `xteRegisterFunction()` 当前已经接入模板语法，使用 `{@name:...}` 调用
- 旧版 `XTE_LiteObject / Lexer / TokenList` 不再是主线 API
- 新模板对象是预编译句柄，不应假定其内部布局稳定

---

## 推荐使用方式

建议按下面的方式使用 XTE：

1. 用 `xvalue` 组织输入数据
2. 用 `xteCreateEngine()` 注册内建语句和自定义语句
3. 用 `xteParseEx()` 预编译模板
4. 需要高频渲染时复用 `xtetemplate`
5. 需要缓存时，在 `XTE_ENABLE_FILE` 下保存为预编译文件
6. 调试复杂模板时，在 `XTE_DEBUGMODE` 下直接 dump，直接核对原始参数文本和编译后的表达式类型
