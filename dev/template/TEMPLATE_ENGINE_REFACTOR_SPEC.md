# XTE 模板引擎重构实现 Spec

## 1. 目的

本文档用于定义 XRT 中 XTE 模板引擎重构的实现规格。

本 spec 的目标不是解释“为什么重构”，而是明确：
- 要做什么
- 不做什么
- 对外 API 长什么样
- 内部数据如何存储
- 预编译文件如何保存和加载
- 调试输出如何工作
- 回调协议如何定义
- 代码体积和性能的约束是什么


## 2. 最终边界

本次重构的固定边界如下：

- 不保留旧 `XTE_LiteObject` 结构体字段兼容。
- 不保留旧 `Tokens / Actions / SubTemplates` 外部布局兼容。
- `xrt` 内只有一个后端：渲染后端。
- 不在 `xrt` 内实现“模板编译到 C”。
- 不引入独立 IR 层。
- 采用“AST + 语义注解”模型。
- 自定义语句支持：
	- `{#xxx}`
	- `{#xxx}...{#end}`
	- `HYBRID` 同时支持两种
- 模板预编译结果允许保存成文件并从文件直接加载。
- `DEBUGMODE` 下允许输出内部结构到控制台或 writer。
- `DEBUGMODE` 下不要求导出为 `xvalue`。


## 3. 编译开关

模板模块只允许两个主要扩展开关：

- `XTE_ENABLE_FILE`
	- 启用模板预编译结果保存到文件和从文件加载。
- `XTE_DEBUGMODE`
	- 启用内部结构 dump、控制台输出和额外调试检查。

要求：
- 不再继续新增模板模块细粒度功能宏。
- `XTE_DEBUGMODE` 之外，调试输出代码不得编进发布版本。
- `XTE_ENABLE_FILE` 之外，文件读写与序列化代码不得编进发布版本。

推荐兼容写法：

```c
#if defined(DEBUG) || defined(_DEBUG)
	#ifndef XTE_DEBUGMODE
		#define XTE_DEBUGMODE
	#endif
#endif
```


## 4. 模块划分

模板模块内部拆分为以下子模块：

- `template_base`
	- 错误码、基础类型、公共工具
- `template_lexer`
	- 词法分析
- `template_expr`
	- 表达式与路径解析、求值
- `template_ast`
	- AST 节点、模板对象、参数对象、字符串池
- `template_parser`
	- parse、块匹配、语义注解、自定义语句接入
- `template_render`
	- 渲染器、writer、作用域模型、输出控制
- `template_builtin`
	- 内建语句注册与实现
- `template_file`
	- 仅在 `XTE_ENABLE_FILE` 下编译
- `template_debug`
	- 仅在 `XTE_DEBUGMODE` 下编译

外部仍统一从 [`D:/git/xrt/xrt.h`](D:/git/xrt/xrt.h) 导出公开 API。


## 5. 对外 API

### 5.1 句柄类型

```c
typedef struct XTE_Engine_Struct* xteengine;
typedef struct XTE_Template_Struct* xtetemplate;
```

### 5.2 错误结构

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

### 5.3 Writer

```c
typedef struct
{
	int (*procWrite)(void* pUserData, const char* sText, size_t iSize);
	void* pUserData;
	size_t iWritten;
} XTE_Writer;
```

### 5.4 Parse / Render 选项

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

### 5.5 Engine API

```c
XXAPI xteengine xteCreateEngine(void);
XXAPI void xteDestroyEngine(xteengine hEngine);

XXAPI int xteRegisterBuiltinStatements(xteengine hEngine);
XXAPI int xteRegisterStatement(xteengine hEngine, const XTE_StatementDef* pDef);
XXAPI int xteRegisterFunction(xteengine hEngine, const XTE_FunctionDef* pDef);
```

### 5.6 Template API

```c
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

XXAPI char* xteMake(
	xtetemplate hTemplate,
	xvalue pCurrent,
	xvalue pGlobal,
	xdict pIncludeMap,
	size_t* pRetSize
);
```

### 5.7 Path API

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

### 5.8 调试与文件 API

```c
#ifdef XTE_ENABLE_FILE
XXAPI int xteTemplateSaveFile(xtetemplate hTemplate, const char* sFilePath, uint32 iFlags, XTE_Error* pError);
XXAPI xtetemplate xteTemplateLoadFile(xteengine hEngine, const char* sFilePath, uint32 iFlags, XTE_Error* pError);
#endif

#ifdef XTE_DEBUGMODE
XXAPI int xteTemplateDump(xtetemplate hTemplate, XTE_Writer* pWriter, uint32 iFlags);
XXAPI int xteTemplateDumpConsole(xtetemplate hTemplate, uint32 iFlags);
#endif
```

### 5.9 轻量访问器

为了保证“内部数据可以间接访问”，需要提供轻量访问器。

```c
XXAPI uint32 xteTemplateGetNodeCount(xtetemplate hTemplate);
XXAPI uint32 xteTemplateGetExprCount(xtetemplate hTemplate);
XXAPI uint32 xteTemplateGetStringPoolSize(xtetemplate hTemplate);
XXAPI const XTE_Node* xteTemplateGetNode(xtetemplate hTemplate, uint32 iIndex);
XXAPI const XTE_ExprNode* xteTemplateGetExpr(xtetemplate hTemplate, uint32 iIndex);
```

要求：
- 访问器返回只读视图。
- 不暴露需要宿主手工释放的内部对象。
- 访问器必须在非 `DEBUGMODE` 下也可用。


## 6. 运行时作用域模型

统一为四层：

- `Local`
	- 当前循环或语句的局部变量
- `Current`
	- 当前渲染对象
- `Root`
	- 根对象
- `Global`
	- 宿主注入环境

查找顺序固定为：

1. `Local`
2. `Current`
3. `Root`
4. `Global`

要求：
- 不允许再回到旧的 `tblVal / tblRoot / tblENV` 混合逻辑。
- `__index__`、`__value__`、`__key__` 等临时变量统一写入 `Local`。


## 7. 内部存储模型

### 7.1 总原则

预编译结果本体使用紧凑原生结构，不使用 `xvalue` 作为结构描述。

### 7.2 模板对象

```c
typedef struct XTE_Template_Struct
{
	xteengine hEngine;
	XTE_Error LastError;

	char* sStringPool;
	uint32 iStringPoolSize;

	XTE_Node* arrNodes;
	uint32 iNodeCount;

	XTE_ExprNode* arrExprs;
	uint32 iExprCount;

	XTE_ArgItem* arrArgs;
	uint32 iArgCount;

	XTE_SubTemplateItem* arrSubTemplates;
	uint32 iSubTemplateCount;

	XTE_NodeSpan Root;

	void* pArena;
	uint32 iFlags;
} XTE_Template_Struct;
```

### 7.3 存储原则

- 节点使用连续数组存储。
- 表达式使用连续数组存储。
- 参数使用连续数组存储。
- 子模板表使用连续数组或紧凑表结构存储。
- 所有字符串进入字符串池。
- 节点之间用索引和 span 关联，不依赖散指针树。

### 7.4 NodeSpan

```c
typedef struct
{
	uint32 iStart;
	uint32 iCount;
} XTE_NodeSpan;
```

用途：
- 表示某个节点列表范围
- 表示 `if` 分支体
- 表示循环体
- 表示子模板体
- 表示自定义语句体

### 7.5 Arena 规则

模板 parse 期间产生的所有内部对象应优先进入同一 arena：
- 模板销毁时一次释放
- 减少碎片
- 简化错误回滚


## 8. AST 节点规格

### 8.1 节点类型

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

### 8.2 输出节点

统一表示：
- `{$expr}`
- `{%expr:fmt}`
- `{&expr:fmt}`

字段至少包含：
- 输出类型
- 表达式索引
- 可选格式参数

### 8.3 内联布尔节点

表示旧语法：
- `{?cond:true_text:false_text}`

要求：
- 语义保持兼容
- 条件表达式必须预编译

### 8.4 自定义语句节点

```c
typedef struct
{
	uint32 iStmtDefIndex;
	uint32 iArgStart;
	uint32 iArgCount;
	XTE_NodeSpan Body;
	uint32 iRawBodyOff;
	uint32 iRawBodySize;
	void* pData;
	uint8 bHasBody;
} XTE_StatementNode;
```

要求：
- `pData` 由 `procParse` 生成
- 若存在 `pData`，模板销毁时需调用 `procFreeData`
- 文件保存时不直接序列化回调指针


## 9. 表达式规格

### 9.1 支持能力

首阶段必须支持：
- 数字字面量
- 字符串字面量
- 布尔字面量
- 路径引用
- `= != ~= > < >= <=`
- `and or not`
- 括号

### 9.2 路径语法

必须支持：
- `a`
- `a.b`
- `a.b.c`
- `arr[0]`
- `arr[0].title`

### 9.3 统一规则

以下场景必须共用同一套表达式/路径语义：
- 输出节点
- `if`
- `for`
- `foreach`
- 自定义语句参数
- 函数调用参数


## 10. Parser 规格

### 10.1 Parser 输出

parser 必须直接输出“已带语义注解的 AST”。

禁止：
- parse 只产出 token 列表，然后 render 再做块匹配

### 10.2 块匹配

以下语法必须在 parse 阶段闭合：
- `define ... end`
- `if / elseif / else / end`
- `for ... end`
- `foreach ... end`
- 自定义 block / hybrid 语句

### 10.3 HYBRID 语句规则

对于同时支持 inline 和 block 的语句：

1. 优先尝试匹配同层 `#end`
2. 找到则按 block 解析
3. 找不到则按 inline 解析

要求：
- 匹配逻辑只在同层生效
- 不允许跨层误吃别人的 `#end`

### 10.4 RAW_BODY 语句规则

带 `XTE_STMT_RAW_BODY` 的 block 语句：
- 内部内容不递归解析模板语法
- 仅以同层 `#end` 作为结束标记
- 原始 body 写入字符串池


## 11. 自定义语句协议

### 11.1 标志位

```c
#define XTE_STMT_INLINE				0x0001
#define XTE_STMT_BLOCK				0x0002
#define XTE_STMT_HYBRID				(XTE_STMT_INLINE | XTE_STMT_BLOCK)
#define XTE_STMT_RAW_BODY			0x0004
#define XTE_STMT_ALLOW_NAMED_ARGS	0x0008
```

### 11.2 StatementDef

```c
typedef struct XTE_StatementDef_Struct
{
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

### 11.3 约束

- 内建和自定义语句共用同一注册表。
- `if/elseif/else` 这类多分支结构仍由 parser 专门处理，不交给普通语句回调。
- 自定义语句不支持自定义 `elseif` 或 `else`。


## 12. 参数模型

### 12.1 参数项

```c
typedef struct
{
	uint32 iNameOff;
	uint32 iNameSize;

	uint32 iRawOff;
	uint32 iRawSize;

	uint32 iFlags;
	uint32 iExprIndex;
} XTE_ArgItem;
```

### 12.2 参数规则

- 所有参数保留原始文本切片
- 可表达式化的参数在 parse 阶段预编译
- 命名参数只在语句显式允许时启用

### 12.3 参数辅助 API

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


## 13. 回调协议

### 13.1 控制流枚举

```c
typedef enum
{
	XTE_FLOW_OK = 0,
	XTE_FLOW_BREAK = 1,
	XTE_FLOW_CONTINUE = 2,
	XTE_FLOW_ERROR = -1
} XTE_Flow;
```

### 13.2 语句回调上下文

```c
typedef struct XTE_StmtRenderCtx_Struct
{
	XTE_RenderCtx* pRender;
	const XTE_StatementDef* pDef;
	const XTE_ArgList* pArgs;
	const XTE_NodeSpan* pBody;
	const char* sRawBody;
	size_t iRawBodySize;
	void* pData;
	void* pUserData;
} XTE_StmtRenderCtx;
```

### 13.3 语句回调辅助函数

```c
XXAPI int xteStmtWrite(XTE_StmtRenderCtx* pCtx, const char* sText, size_t iSize);
XXAPI int xteStmtRenderBody(XTE_StmtRenderCtx* pCtx);
XXAPI int xteStmtRenderBodyWithScope(XTE_StmtRenderCtx* pCtx, xvalue pLocal, xvalue pCurrent);
```

### 13.4 函数回调

```c
typedef struct XTE_FunctionDef_Struct
{
	const char* sName;
	uint16 iMinArgs;
	uint16 iMaxArgs;
	void* pUserData;
	int (*procCall)(XTE_FuncCtx* pCtx, xvalue* ppRet);
} XTE_FunctionDef;
```

要求：
- 函数回调只返回值，不直接写输出
- 函数回调不参与块控制流
- 语句回调和函数回调必须分离


## 14. 渲染规格

### 14.1 输出模型

渲染器必须直接写 `XTE_Writer`，不得默认走“子块先生成临时字符串再拼接”的主路径。

### 14.2 控制流传播

- `break / continue` 通过 `XTE_Flow` 逐层上传
- 非法位置使用 `break / continue` 必须报 render error

### 14.3 Include

include 规则：
- 优先从 `pIncludeMap` 查找
- 命中结果必须是已编译模板对象
- 若未命中，返回结构化错误

可选扩展：
- 后续允许 engine 增加 include resolver
- 本 spec 首阶段不强制实现


## 15. 预编译文件格式

仅在 `XTE_ENABLE_FILE` 下实现。

### 15.1 总体要求

- 文件必须可直接加载为模板对象
- 文件格式必须版本化
- 文件中不得保存任何回调函数指针
- 文件加载后必须重新绑定 engine 的注册表

### 15.2 建议格式

采用 chunk 结构：

- `HEAD`
- `STRS`
- `NODE`
- `EXPR`
- `ARGS`
- `SUBT`
- `META` 可选

### 15.3 HEAD 内容

至少包含：
- magic
- format version
- endian/version 标志
- 各 chunk 偏移和大小
- flags

### 15.4 自定义语句的文件绑定规则

文件中只保存：
- 语句名
- 参数
- body
- raw body
- 必要的基础编译数据

加载时：
- 根据语句名到 engine 注册表查找对应 `StatementDef`
- 未注册则返回 load error

首阶段不要求把 `procParse` 产出的私有 `pData` 落盘。

结论：
- 文件加载后可根据基础信息重新构建 `pData`
- 若某自定义语句强依赖 `pData`，其 `procParse` 需要可重入


## 16. 调试输出规格

仅在 `XTE_DEBUGMODE` 下实现。

### 16.1 调试输出目标

调试输出的目的：
- 快速查看 AST 结构
- 快速查看节点范围、表达式索引、参数索引
- 快速定位 parse 结果是否正确

### 16.2 输出接口

- `xteTemplateDump()`
	- 输出到指定 writer
- `xteTemplateDumpConsole()`
	- 直接输出到控制台

### 16.3 输出内容

至少包括：
- 模板基本信息
- 字符串池大小
- 节点总数
- 表达式总数
- 子模板数量
- 每个节点的：
	- 类型
	- 位置
	- 关键字段
	- 子体 span

### 16.4 输出原则

- 重点是人类可读
- 不要求 JSON 格式
- 不要求可反序列化
- 不使用 `xvalue` 作为中间格式


## 17. 错误模型

### 17.1 分类

至少区分：
- 词法错误
- 语法错误
- 表达式错误
- 路径错误
- 参数错误
- include 错误
- 文件格式错误
- 控制流错误
- 回调错误

### 17.2 返回方式

- parse 错误通过 `XTE_Error`
- render 错误通过 `XTE_Error`
- load/save 错误通过 `XTE_Error`

禁止默认把核心错误直接写入渲染输出正文。


## 18. 线程与生命周期

### 18.1 Engine

- engine 持有注册表
- template 引用其创建 engine
- template 在销毁前，engine 不可先销毁

### 18.2 Template

- parse 完成后只读
- 多线程并发 render 必须安全
- render 上下文数据在栈上或临时对象中构建，不写回模板本体

### 18.3 全局状态

禁止使用旧式全局：
- 全局 ident list
- 全局表达式缓存
- 全局初始化标志


## 19. 性能与体积要求

### 19.1 性能要求

- render 热路径不得依赖 `xvalue` 描述模板结构
- render 热路径不得反复重新 parse 表达式
- render 主路径尽量避免中间字符串拼接

### 19.2 体积要求

- 不做独立 IR
- 不做 `xvalue` 调试导出
- `DEBUGMODE` 和 `ENABLE_FILE` 以外不引入重代码
- 内建控制语句保留专用实现，不强行全部动态化

### 19.3 维护要求

- 内部结构必须可通过访问器和 debug dump 间接查看
- 不接受完全黑盒、无检查手段的实现


## 20. 测试验收项

必须覆盖：

- 基础文本与转义
- 路径解析
- 表达式优先级
- `if / elseif / else`
- `for` 正向 / 反向 / 步长为 0 / 最大迭代保护
- `foreach` 数组 / 表
- `break / continue`
- `define / include / 子模板`
- `script` raw body
- 自定义语句 `INLINE / BLOCK / HYBRID`
- 命名参数与位置参数
- 文件保存与加载
- debug dump 输出
- 并发 render


## 21. 实施顺序

1. 搭 `engine / template / error / writer` 基础骨架
2. 完成字符串池、节点数组、表达式数组等内部存储
3. 完成 lexer
4. 完成 expr/path
5. 完成 parser 和 AST 语义注解
6. 完成基础 render
7. 完成 `if / for / foreach / break / continue`
8. 完成语句与函数注册体系
9. 完成 `XTE_ENABLE_FILE`
10. 完成 `XTE_DEBUGMODE`
11. 用断言式测试替换旧 demo 测试


## 22. 完成标准

满足以下条件，视为 spec 达成：

- 新模板对象已完全替代旧 `XTE_LiteObject` 公开结构依赖
- 模板可 parse 后多次 render
- 自定义语句可注册并稳定工作
- 模板可保存为预编译文件并重新加载
- `DEBUGMODE` 下可查看内部结构
- 发布版本不带调试导出重代码
- 不引入独立 IR
- 不在 `xrt` 内实现 C 后端


## 23. 附加说明

本 spec 为模板引擎重构的实施基准。

若后续设计文档与本 spec 冲突，以本 spec 为准。
