#ifndef XRT_INTERNAL_TEMPLATE_H
#define XRT_INTERNAL_TEMPLATE_H

#include "xrt_internal.h"
#include "xrt_string.h"

#include <xrt/array.h>
#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
#include <xrt/map.h>
#endif
#include <xrt/number.h>
#include <xrt/template.h>
#include <xrt/time.h>



#if defined(XRT_FEATURE_TEMPLATE_CORE)

#define XRT_TEMPLATE_RENDER_FLAG_MASK \
	(XTEMPLATE_STRICT_UNDEFINED | XTEMPLATE_ESCAPE_HTML_TEXT)



/* 编译节点使用紧凑 32 位偏移和按类型复用的负载。 */
typedef struct xrt_template_node {
	uint16 Type;
	uint16 Output;
	uint32 SourceOffset;
	uint32 SourceSize;
	union {
		struct {
			uint32 Offset;
			uint32 Size;
		} Text;
		struct {
			uint32 ExpressionOffset;
			uint32 ExpressionSize;
			uint32 PathStart;
			uint32 PathCount;
			uint32 FormatOffset;
			uint32 FormatSize;
		} Output;
		#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
		struct {
			uint32 ExpressionOffset;
			uint32 ExpressionSize;
			uint32 Expression;
			uint32 TrueOffset;
			uint32 TrueSize;
			uint32 FalseOffset;
			uint32 FalseSize;
		} InlineIf;
		struct {
			uint32 BranchStart;
			uint32 BranchCount;
			uint32 Next;
		} If;
		struct {
			uint32 StartExpression;
			uint32 EndExpression;
			uint32 StepExpression;
			uint32 BodyStart;
			uint32 BodyEnd;
			uint32 Next;
		} For;
		struct {
			uint32 Expression;
			uint32 ExpressionOffset;
			uint32 ExpressionSize;
			uint32 BodyStart;
			uint32 BodyEnd;
			uint32 Next;
		} Foreach;
		#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
		struct {
			uint32 NameOffset;
			uint32 NameSize;
			uint32 BodyStart;
			uint32 BodyEnd;
			uint32 Next;
		} Define;
		struct {
			uint32 Expression;
			uint32 ExpressionOffset;
			uint32 ExpressionSize;
		} Include;
		struct {
			uint32 Offset;
			uint32 Size;
		} Raw;
		#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)
		struct {
			const struct xrt_template_extension_def* Definition;
			uint32 NameOffset;
			uint32 NameSize;
			uint32 ArgumentStart;
			uint32 ArgumentCount;
			uint32 BodyStart;
			uint32 BodyEnd;
			uint32 RawOffset;
			uint32 RawSize;
			uint32 Next;
		} Extension;
		#endif
		#endif
		#endif
	} Data;
} xrt_template_node;



#define XRT_TEMPLATE_INDEX_NONE UINT32_MAX



/* 路径段是对象键或带符号数组索引。 */
typedef enum xrt_template_path_type {
	XRT_TEMPLATE_PATH_KEY = 1,
	XRT_TEMPLATE_PATH_INDEX
} xrt_template_path_type;



/* 键直接引用模板源码，索引保存已经解析的整数。 */
typedef struct xrt_template_path {
	uint16 Type;
	uint16 Reserved;
	uint32 Offset;
	uint32 Size;
	int64 Index;
} xrt_template_path;



#if defined(XRT_FEATURE_TEMPLATE_CONTROL)

/* 条件节点把各分支的表达式和扁平节点范围分开保存。 */
typedef struct xrt_template_branch {
	uint32 Expression;
	uint32 ExpressionOffset;
	uint32 ExpressionSize;
	uint32 BodyStart;
	uint32 BodyEnd;
	uint32 Next;
} xrt_template_branch;



/* 表达式节点按后序 AST 保存，渲染时无需重新词法分析。 */
typedef enum xrt_template_expr_type {
	XRT_TEMPLATE_EXPR_NULL = 1,
	XRT_TEMPLATE_EXPR_BOOL,
	XRT_TEMPLATE_EXPR_INT,
	XRT_TEMPLATE_EXPR_FLOAT,
	XRT_TEMPLATE_EXPR_STRING,
	XRT_TEMPLATE_EXPR_PATH,
	XRT_TEMPLATE_EXPR_NOT,
	XRT_TEMPLATE_EXPR_EQUAL,
	XRT_TEMPLATE_EXPR_NOT_EQUAL,
	XRT_TEMPLATE_EXPR_APPROX,
	XRT_TEMPLATE_EXPR_GREATER,
	XRT_TEMPLATE_EXPR_LESS,
	XRT_TEMPLATE_EXPR_GREATER_EQUAL,
	XRT_TEMPLATE_EXPR_LESS_EQUAL,
	XRT_TEMPLATE_EXPR_AND,
	XRT_TEMPLATE_EXPR_OR
} xrt_template_expr_type;



/* 单个表达式节点只使用与类型对应的负载字段。 */
typedef struct xrt_template_expr {
	uint16 Type;
	uint16 Reserved;
	uint32 SourceOffset;
	uint32 SourceSize;
	uint32 Left;
	uint32 Right;
	uint32 PathStart;
	uint32 PathCount;
	uint32 TextOffset;
	uint32 TextSize;
	union {
		int64 Integer;
		double Float;
		bool Bool;
	} Value;
} xrt_template_expr;

#endif



#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)

/* 扩展参数保存可选名称、原始表达式和预编译表达式根。 */
typedef struct xrt_template_argument {
	uint32 NameOffset;
	uint32 NameSize;
	uint32 SourceOffset;
	uint32 SourceSize;
	uint32 Expression;
} xrt_template_argument;



/* 注册项位于不可变映射的稳定值槽中。 */
typedef struct xrt_template_extension_def {
	xtemplateextensiontype Type;
	size_t MinArguments;
	size_t MaxArguments;
	xtemplateextensionfn Call;
	ptr Data;
	xtemplateextensiondrop Drop;
} xrt_template_extension_def;



/* 注册表按语法类别分开索引，允许函数和语句使用相同名称。 */
struct xtemplateregistry {
	volatile int32 RefCount;
	xmap Functions;
	xmap Statements;
	bool OwnData;
};

#endif



/* 不可变模板集中持有源码、转义文本池和紧凑编译数组。 */
struct xtemplate {
	volatile int32 RefCount;
	str Source;
	size_t SourceSize;
	xstrbuf Text;
	xarray Nodes;
	xarray Paths;
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
	xarray Expressions;
	xarray Branches;
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
	struct xmap* Definitions;
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)
	xtemplateregistry* Registry;
	xarray Arguments;
	#endif
};



/* 解析器只在一次编译调用期间存在。 */
typedef struct xrt_template_parser {
	xtemplate* Template;
	const xtemplateconfig* Config;
	size_t Position;
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
	size_t Depth;
	#endif
} xrt_template_parser;



/* 表达式求值结果借用模板、输入值或循环帧，不产生临时 xvalue。 */
typedef struct xrt_template_eval {
	xvaluetype Type;
	const xvalue* Value;
	union {
		bool Bool;
		int64 Integer;
		uint64 Unsigned;
		double Float;
		xstrview String;
		xtime Time;
	} Data;
} xrt_template_eval;



#if defined(XRT_FEATURE_TEMPLATE_CONTROL)

/* 循环元数据按栈链接，避免每次迭代创建局部对象。 */
typedef struct xrt_template_loop {
	const struct xrt_template_loop* Parent;
	const xvalue* Value;
	xvaluekey Key;
	int64 Integer;
	size_t Index;
	size_t Count;
	size_t Depth;
	bool IntegerValue;
} xrt_template_loop;

#endif



#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)

/* include 栈同时标识完整模板和模板内定义，用于精确检测递归环。 */
typedef struct xrt_template_frame {
	const struct xrt_template_frame* Parent;
	const xtemplate* Template;
	uint32 Definition;
} xrt_template_frame;

#endif



/* 渲染上下文保存每次调用独有的作用域、预算和 writer。 */
typedef struct xrt_template_render {
	const xtemplate* Template;
	const xtemplaterenderconfig* Config;
	xtemplatewritefn Write;
	ptr UserData;
	const xvalue* Current;
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
	const xrt_template_loop* Loop;
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
	const xrt_template_frame* Frame;
	#endif
	size_t Written;
	size_t Steps;
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
	size_t Depth;
	size_t LoopIterations;
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
	size_t IncludeDepth;
	#endif
} xrt_template_render;



#if defined(XRT_FEATURE_TEMPLATE_CONTROL)

/* 控制节点向外层传播的结构化流程。 */
typedef enum xrt_template_flow {
	XRT_TEMPLATE_FLOW_ERROR = -1,
	XRT_TEMPLATE_FLOW_OK = 0,
	XRT_TEMPLATE_FLOW_BREAK,
	XRT_TEMPLATE_FLOW_CONTINUE
} xrt_template_flow;

#endif



#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)

/* 公共调用上下文只在一次扩展回调期间有效。 */
struct xtemplatecall {
	xrt_template_render* Render;
	const xrt_template_node* Node;
	const xrt_template_extension_def* Definition;
};

#endif



/* 设置带源码位置的模板错误。 */
void __xrtTemplateError(
	xerrkind Kind,
	xtemplateerror Code,
	cstr sOperation,
	cstr sMessage,
	const xtemplate* pTemplate,
	size_t iOffset,
	size_t iSize
);



/* 把当前底层错误包装为带模板源码位置的错误链。 */
void __xrtTemplateWrapCurrent(
	xtemplateerror Code,
	cstr sOperation,
	cstr sMessage,
	const xtemplate* pTemplate,
	size_t iOffset,
	size_t iSize
);



/* 验证字符串视图的指针与长度组合。 */
bool __xrtTemplateViewValid(xstrview Text);



/* 判断字节是否属于模板语法允许忽略的 ASCII 空白。 */
bool __xrtTemplateSpace(char iByte);



/* 验证编译配置和全部预算。 */
bool __xrtTemplateConfigValid(const xtemplateconfig* pConfig);



/* 验证渲染配置和全部预算。 */
bool __xrtTemplateRenderConfigValid(
	const xtemplaterenderconfig* pConfig
);



/* 把源码解析成紧凑核心节点。 */
bool __xrtTemplateParse(
	xtemplate* pTemplate,
	const xtemplateconfig* pConfig
);



/* 编译路径并追加到模板统一路径池。 */
bool __xrtTemplateCompilePath(
	xrt_template_parser* pParser,
	size_t iStart,
	size_t iEnd,
	uint32* pPathStart,
	uint32* pPathCount
);



/* 把源区间解转义后追加到模板私有文本池。 */
bool __xrtTemplateTextUnescape(
	xrt_template_parser* pParser,
	size_t iStart,
	size_t iEnd,
	uint32* pOffset,
	uint32* pSize
);



#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
/* 编译一个完整表达式并返回根节点索引。 */
bool __xrtTemplateCompileExpression(
	xrt_template_parser* pParser,
	size_t iStart,
	size_t iEnd,
	uint32* pExpression
);



/* 求值已经编译的表达式。 */
bool __xrtTemplateEvalExpression(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	uint32 iExpression,
	xrt_template_eval* pValue
);



/* 按模板语言规则读取求值结果的真值。 */
bool __xrtTemplateEvalTruthy(
	const xrt_template_eval* pValue,
	bool* pResult
);



/* 严格读取求值结果中的整数。 */
bool __xrtTemplateEvalInteger(
	const xrt_template_eval* pValue,
	int64* pResult
);



/* 严格读取求值结果中的文本或字节视图。 */
bool __xrtTemplateEvalText(
	const xrt_template_eval* pValue,
	xstrview* pText
);



/* 执行一个扁平节点范围并传播结构化循环控制流。 */
xrt_template_flow __xrtTemplateRenderSpan(
	xrt_template_render* pRender,
	uint32 iStart,
	uint32 iEnd
);
#endif



#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)

/* 注册模板内不可变定义，重复名称返回模板语法错误。 */
bool __xrtTemplateDefinitionAdd(
	xtemplate* pTemplate,
	xstrview Name,
	uint32 iNode
);



/* 查找模板内定义节点，缺失是正常结果。 */
const xrt_template_node* __xrtTemplateDefinition(
	const xtemplate* pTemplate,
	xstrview Name,
	uint32* pNode
);



/* 解析并执行 include 节点，不允许循环控制跨越 include 边界。 */
xrt_template_flow __xrtTemplateRenderInclude(
	xrt_template_render* pRender,
	const xrt_template_node* pNode
);

#endif



#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)

/* 按语法类别查找不可变扩展定义，缺失是正常结果。 */
const xrt_template_extension_def* __xrtTemplateExtensionFind(
	const xtemplateregistry* pRegistry,
	xtemplateextensiontype Type,
	xstrview Name
);



/* 调用扩展并把回调错误包装到当前节点位置。 */
xrt_template_flow __xrtTemplateRenderExtension(
	xrt_template_render* pRender,
	const xrt_template_node* pNode
);

#endif



/* 消耗一个有限渲染步骤。 */
bool __xrtTemplateStep(
	xrt_template_render* pRender,
	const xrt_template_node* pNode
);



/* 写出一个已经通过预算检查的模板分片。 */
bool __xrtTemplateEmit(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	xstrview Text
);



/* 解析预编译路径并返回无临时分配的求值结果。 */
bool __xrtTemplateResolveCompiled(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	uint32 iPathStart,
	uint32 iPathCount,
	xrt_template_eval* pValue,
	bool* pFound
);



/* 执行已经验证的核心模板。 */
bool __xrtTemplateRender(xrt_template_render* pRender);

#endif

#endif
