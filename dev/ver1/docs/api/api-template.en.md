# Template Engine Library

> Current mainline template engine built around `xvalue`, exposed as `xteengine / xtetemplate`

[English](api-template.en.md) | [中文](api-template.md) | [Back to Index](README.en.md)

---

## Overview

The current XTE public surface no longer exposes the legacy objects:

- `XTE_LiteObject`
- `XTE_TokenList`
- `XTE_TokenItem`
- `xteLexer()`
- `xteParseFromTokenList()`
- `xteMakeActions()`

The current model is:

- `xteengine`: engine instance used for builtin and custom registration
- `xtetemplate`: compiled template handle
- `xteParseEx()`: compile template text into a reusable object
- `xteRenderEx()` / `xteMake()`: render from `xvalue`

The compiled template is stored internally as a compact AST. You can inspect it through accessors, and under `XTE_DEBUGMODE` you can dump the internal structure directly.

---

## Current Feature Set

Current mainline supports:

- text output `{$expr}`
- numeric formatting `{%expr:fmt}`
- time formatting `{&expr:fmt}`
- inline boolean branch `{?expr:true:false}`
- function output `{@name:arg1:arg2}` / `{@name:key=value}`
- custom statements `{#xxx}` / `{#xxx}...{#end}`
- hybrid custom statements via `XTE_STMT_HYBRID`
- named arguments
- RAW_BODY blocks
- builtin statements:
	- `if / elseif / else`
	- `for`
	- `foreach`
	- `break / continue`
	- `define`
	- `script`
	- `include`
- compiled template save/load (`XTE_ENABLE_FILE`)
- debug structure dump (`XTE_DEBUGMODE`)

The expression layer is still intentionally lightweight. It currently focuses on:

- paths: `user.name`
- text literals: `'Alice'`, `"Alice"`
- integer literals: `123`
- boolean literals: `true` / `false`
- compare operators: `=`, `!=`, `~=`, `>`, `<`, `>=`, `<=`
- logical operators: `and`, `or`, `not`
- parenthesized groups: `(expr)`

So current `if` / `elseif` already support lightweight conditional expressions, but this is still not the old fully featured expression language.

---

## Delimiter Configuration

By default templates use single `{}` delimiters.

To embed templates into JS, JSON, HTML fragments, or other environments, use `XTE_ParseOptions.sBracket`:

```c
typedef struct {
	const char* sBracket;
	uint32 iFlags;
} XTE_ParseOptions;
```

Examples:

- `"{}"` -> `{$name}`
- `"<>"` -> `<$name>`
- `"{{}}"` -> `{{$name}}`
- `"(())"` -> `(($name))`

Rules:

- `sBracket` is a compact shorthand: first half is open delimiter, second half is close delimiter
- the total length must be even
- this only affects syntax recognition, not the persisted AST format

---

## Public Types

### Handles and core structs

```c
typedef struct XTE_Engine_Struct* xteengine;
typedef struct XTE_Template_Struct* xtetemplate;
typedef struct XTE_RenderCtx_Struct XTE_RenderCtx;
typedef struct XTE_StmtParseCtx_Struct XTE_StmtParseCtx;
typedef struct XTE_StmtRenderCtx_Struct XTE_StmtRenderCtx;
typedef struct XTE_FuncCtx_Struct XTE_FuncCtx;
```

### Error object

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

### Render options

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

### Inspection views

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

These are read-only inspection views. They are not a compatibility promise for the old exposed layout.

---

## Core API

### Engine lifecycle

```c
XXAPI xteengine xteCreateEngine(void);
XXAPI void xteDestroyEngine(xteengine hEngine);
XXAPI int xteRegisterBuiltinStatements(xteengine hEngine);
```

Notes:

- `xteCreateEngine()` registers builtin statements by default
- `xteRegisterBuiltinStatements()` is idempotent

### Custom registration

```c
XXAPI int xteRegisterStatement(xteengine hEngine, const XTE_StatementDef* pDef);
XXAPI int xteRegisterFunction(xteengine hEngine, const XTE_FunctionDef* pDef);
```

In the current syntax:

- custom statements are dispatched through `{#name}` / `{#name}...{#end}`
- custom functions are dispatched through `{@name:arg1:arg2}` or `{@name:key=value}`
- function tags support both positional and named arguments

### Parse and destroy templates

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

### Render

```c
XXAPI int xteRenderEx(xtetemplate hTemplate, const XTE_RenderOptions* pOptions, XTE_Error* pError);
XXAPI char* xteMake(xtetemplate hTemplate, xvalue pCurrent, xvalue pGlobal, xdict pIncludeMap, size_t* pRetSize);
```

Notes:

- `xteRenderEx()` is the generic writer-based path
- `xteMake()` is the string-output convenience wrapper
- `pCurrent` is also the default `pRoot`
- `pGlobal` is the host-provided global scope
- `pIncludeMap` is used by `{#include}`

### Path resolution

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

Lookup order is currently:

- `Local`
- `Current`
- `Root`
- `Global`

---

## Basic Example

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

## Builtin Statements

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

Output:

```text
123
```

### foreach

```text
{#foreach:users}
	{$name}
{#end}
```

Loop locals:

- `__index__`
- `__value__`
- `__key__` (table iteration only)

### break / continue

```text
{#foreach:users}
	{#if:skip}{#continue}{#end}
	{$name}
	{#if:stop}{#break}{#end}
{#end}
```

### define / local subtemplates

```text
A{#define:'card'}[{$user.name}]{#end}B{#include:'card'}C
```

`define` is a compile-time block statement:

- `define` itself does not render output
- its body is registered into the current template's local subtemplate table
- later `{#include:'card'}` resolves local `define` entries first

Supported forms:

- `{#define:'card'}...{#end}`
- `{#define:name='card'}...{#end}`

The local subtemplate name must currently be static text or an identifier-style path name. It cannot depend on runtime values.

---

### include

```text
X{#include:'card'}Y
```

`include` resolves in this order:

- local subtemplates registered by `define`
- `pIncludeMap` when no local match exists

External template-map example:

```c
xdict tblInclude = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
xrtDictSetPtr(tblInclude, "card", 0, hCardTemplate, NULL);
```

---

### script

```text
{#script}{#if:active}{$user.name}{#end}{#end}
```

`script` is a builtin `RAW_BODY` block statement:

- its body is not recursively parsed as template syntax
- render writes the raw body back unchanged
- useful for embedded script fragments, template-source fragments, or delayed processing

---

## Custom Statements

Register custom statements with `XTE_StatementDef`:

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

Important flags:

- `XTE_STMT_INLINE`
- `XTE_STMT_BLOCK`
- `XTE_STMT_HYBRID`
- `XTE_STMT_RAW_BODY`
- `XTE_STMT_ALLOW_NAMED_ARGS`

Examples:

```text
{#wrap:tag='section'}Body{#end}
{#repeat:3:'ha'}
```

### Useful statement helpers

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

These three helpers are intended to be used like this:

- `xteStmtParseSetError()` from `procParse()` as `return xteStmtParseSetError(...)`
- `xteStmtSetError()` from `procRender()` as `return xteStmtSetError(...)`
- `xteFuncSetError()` from `procCall()` as `return xteFuncSetError(...)`

Recommended argument-validation helpers:

- `xteStmtParseRequireArg()` / `xteStmtParseRequireNamedArg()` for parse callbacks
- `xteStmtParseRequireExprType()` / `xteStmtParseRequireNamedExprType()` to fetch an argument, validate its compiled expression type, and set parse errors in one step
- `xteStmtRequireArg()` / `xteStmtRequireNamedArg()` for statement render callbacks
- `xteFuncRequireArg()` / `xteFuncRequireNamedArg()` for function callbacks
- `xteArgExprType()` when you need to validate expression shape
- `xteArgRawText()` when you need the original source text
- `xteEvalArgBool/Int/Float/Text()` for permissive coercion
- `xteEvalArgBoolStrict/IntStrict/FloatStrict/TextStrict()` for strict type checks without coercion
- `xteStmtRequire*Strict()` / `xteStmtRequireNamed*Strict()` to combine missing-arg checks, strict type checks, and error reporting in statement callbacks
- `xteFuncRequire*Strict()` / `xteFuncRequireNamed*Strict()` to combine missing-arg checks, strict type checks, and error reporting in one step

Example:

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

## Template Inspection

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

These are useful for:

- indirect inspection of compiled templates
- debugging custom parse callbacks
- structure analysis without exposing the internal layout itself

---

## Compile-Time Switches

The template module now intentionally keeps only two expansion switches.

### `XTE_ENABLE_FILE`

Enables save/load for compiled templates:

```c
XXAPI int xteTemplateSaveFile(xtetemplate hTemplate, const char* sFilePath, uint32 iFlags, XTE_Error* pError);
XXAPI xtetemplate xteTemplateLoadFile(xteengine hEngine, const char* sFilePath, uint32 iFlags, XTE_Error* pError);
```

Additional load behavior:

- statement definitions are rebound against the current engine registry
- if a statement provides `procParse()`, it is executed again during load to rebuild private `pData`
- if the file references a custom statement that is not registered in the target engine, `xteTemplateLoadFile()` fails immediately instead of deferring the error to render time

### `XTE_DEBUGMODE`

Enables debug dump of internal structure:

```c
XXAPI int xteTemplateDump(xtetemplate hTemplate, XTE_Writer* pWriter, uint32 iFlags);
XXAPI int xteTemplateDumpConsole(xtetemplate hTemplate, uint32 iFlags);
```

`XTE_DEBUGMODE` is for inspection and debugging only. It is not part of the release hot path.

The current dump output includes:

- node kind
- statement / function name
- raw statement / function argument text
- compiled argument expression type and value
- expression type markers for plain outputs / inline boolean nodes, for example `OUTPUT(PATH): user.name`

---

## Current Notes

- the expression system now supports paths, literals, comparisons, `and / or / not`, and parentheses, but it is still not the same as the old full conditional expression language
- `xteRegisterFunction()` is now wired into template syntax through `{@name:...}`
- old `XTE_LiteObject / Lexer / TokenList` are no longer mainline API
- `xtetemplate` is a compiled handle; do not depend on its internal layout

---

## Recommended Usage Pattern

Use XTE in this order:

1. assemble input with `xvalue`
2. create an engine with `xteCreateEngine()`
3. register custom statements if needed
4. compile with `xteParseEx()`
5. reuse `xtetemplate` for repeated renders
6. if you need cache persistence, enable `XTE_ENABLE_FILE`
7. if you need internal visibility, enable `XTE_DEBUGMODE` and inspect both raw argument text and compiled expression types
