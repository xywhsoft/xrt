#ifndef XRT_TEMPLATE_H
#define XRT_TEMPLATE_H

#include <xrt/error.h>
#include <xrt/string.h>
#include <xrt/value.h>



#if defined(XRT_FEATURE_TEMPLATE_CORE) && \
	(!defined(XRT_FEATURE_STRING) || \
	 !defined(XRT_FEATURE_ARRAY) || \
	 !defined(XRT_FEATURE_VALUE_CONTAINER) || \
	 !defined(XRT_FEATURE_NUMBER_FORMAT) || \
	 !defined(XRT_FEATURE_TIME_TEXT))
	#error "XRT_FEATURE_TEMPLATE_CORE requires string, array, value_container, number_format and time_text"
#endif

#if defined(XRT_FEATURE_TEMPLATE_CONTROL) && \
	!defined(XRT_FEATURE_TEMPLATE_CORE)
	#error "XRT_FEATURE_TEMPLATE_CONTROL requires XRT_FEATURE_TEMPLATE_CORE"
#endif

#if defined(XRT_FEATURE_TEMPLATE_COMPOSE) && \
	!defined(XRT_FEATURE_TEMPLATE_CONTROL)
	#error "XRT_FEATURE_TEMPLATE_COMPOSE requires XRT_FEATURE_TEMPLATE_CONTROL"
#endif

#if defined(XRT_FEATURE_TEMPLATE_EXTENSION) && \
	!defined(XRT_FEATURE_TEMPLATE_COMPOSE)
	#error "XRT_FEATURE_TEMPLATE_EXTENSION requires XRT_FEATURE_TEMPLATE_COMPOSE"
#endif

#if defined(XRT_FEATURE_TEMPLATE_FILE) && \
	(!defined(XRT_FEATURE_TEMPLATE_CORE) || \
	 !defined(XRT_FEATURE_FILE_WHOLE))
	#error "XRT_FEATURE_TEMPLATE_FILE requires template_core and file_whole"
#endif



#if defined(XRT_FEATURE_TEMPLATE_CORE)

#define XTEMPLATE_SOURCE_DEFAULT		(16u * 1024u * 1024u)
#define XTEMPLATE_NODES_DEFAULT		1000000u
#define XTEMPLATE_PATH_SEGMENTS_DEFAULT	1000000u
#define XTEMPLATE_PATH_DEPTH_DEFAULT	64u
#define XTEMPLATE_EXPRESSIONS_DEFAULT	1000000u
#define XTEMPLATE_BLOCK_DEPTH_DEFAULT	64u
#define XTEMPLATE_EXPRESSION_DEPTH_DEFAULT 128u
#define XTEMPLATE_OUTPUT_DEFAULT		(16u * 1024u * 1024u)
#define XTEMPLATE_STEPS_DEFAULT		10000000u
#define XTEMPLATE_RENDER_DEPTH_DEFAULT	64u
#define XTEMPLATE_LOOP_DEFAULT		1000000u
#define XTEMPLATE_INCLUDE_DEPTH_DEFAULT	32u
#define XTEMPLATE_ARGUMENTS_DEFAULT	1000000u
#define XTEMPLATE_CALL_ARGUMENTS_DEFAULT 256u



/* 模板错误代码在 xrt.template 域内稳定标识失败阶段。 */
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



/* 编译模板是不可变且可跨线程共享的对象。 */
typedef struct xtemplate xtemplate;
typedef struct xtemplateregistry xtemplateregistry;
typedef struct xtemplatecall xtemplatecall;



/* 源码位置使用 0 基字节偏移和 1 基行列。 */
typedef struct xtemplatelocation {
	size_t Offset;
	size_t Size;
	size_t Line;
	size_t Column;
} xtemplatelocation;



/* 核心层节点区分原样文本和动态输出。 */
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



/* 输出类型决定动态值允许的类型与格式化规则。 */
typedef enum xtemplateoutputtype {
	XTEMPLATE_OUTPUT_TEXT = 1,
	XTEMPLATE_OUTPUT_NUMBER,
	XTEMPLATE_OUTPUT_TIME
} xtemplateoutputtype;



/* 节点视图中的字符串全部借用模板，模板释放后立即失效。 */
typedef struct xtemplatenodeview {
	xtemplatenodetype Type;
	xtemplateoutputtype Output;
	xtemplatelocation Location;
	xstrview Source;
	xstrview Expression;
	xstrview Format;
	xstrview Name;
} xtemplatenodeview;



#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)

/* 扩展类型明确区分行内调用、解析主体和完全原样主体。 */
typedef enum xtemplateextensiontype {
	XTEMPLATE_EXTENSION_FUNCTION = 1,
	XTEMPLATE_EXTENSION_STATEMENT,
	XTEMPLATE_EXTENSION_BLOCK,
	XTEMPLATE_EXTENSION_RAW_BLOCK
} xtemplateextensiontype;



/* 扩展回调返回 false 时直接传播模板错误；其他错误会保留为 cause 并补充调用位置。 */
typedef bool (*xtemplateextensionfn)(xtemplatecall* pCall);



/* 注册表释放时调用数据析构；每个描述项独立拥有自己的数据。 */
typedef void (*xtemplateextensiondrop)(ptr pData);



/* 注册描述在创建期间借用，注册表成功创建后复制名称并接管用户数据。 */
typedef struct xtemplateextension {
	xstrview Name;
	xtemplateextensiontype Type;
	size_t MinArguments;
	size_t MaxArguments;
	xtemplateextensionfn Call;
	ptr Data;
	xtemplateextensiondrop Drop;
} xtemplateextension;



/* 参数视图借用模板源码，并以调用内相对索引作为稳定句柄。 */
typedef struct xtemplateargview {
	size_t Index;
	xstrview Name;
	xstrview Source;
} xtemplateargview;



/* 求值结果借用输入值或模板文本，仅对应当前类型的字段有效。 */
typedef struct xtemplatevalue {
	xvaluetype Type;
	const xvalue* Value;
	bool Bool;
	int64 Integer;
	double Float;
	xstrview Text;
	xtime Time;
} xtemplatevalue;

#endif



/* 编译配置限制源码和编译产物规模，并允许替换成对标签括号。 */
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



/* 严格未定义模式把缺失路径从空输出提升为错误。 */
typedef enum xtemplaterenderflag {
	XTEMPLATE_STRICT_UNDEFINED = 0x0001u
} xtemplaterenderflag;



/* 外部解析器成功时返回一个由渲染器释放的模板引用；空模板表示未找到。 */
/* 输出初始为空，回调写入的任何非空模板引用都会由渲染器接管。 */
typedef bool (*xtemplateresolvefn)(
	ptr pUserData,
	xstrview Name,
	xtemplate** pTemplate
);



/* 每次渲染使用独立配置，因此同一模板可以并发执行。 */
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



/* Writer 借用当前分片；返回 false 会停止渲染并保留回调设置的错误。 */
typedef bool (*xtemplatewritefn)(ptr pUserData, xstrview Text);



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)

/* 校验并复制全部扩展定义，成功后注册表接管每项用户数据。 */
XRT_API xtemplateregistry* xrtTemplateRegistryCreate(
	const xtemplateextension* pExtensions,
	size_t iCount
);



/* 增加不可变注册表引用并返回原指针。 */
XRT_API xtemplateregistry* xrtTemplateRegistryRef(
	const xtemplateregistry* pRegistry
);



/* 释放注册表引用及其最终拥有的扩展用户数据。 */
XRT_API void xrtTemplateRegistryRelease(
	const xtemplateregistry* pRegistry
);



/* 返回当前扩展调用名称和描述项携带的用户数据。 */
XRT_API xstrview xrtTemplateCallName(const xtemplatecall* pCall);
XRT_API ptr xrtTemplateCallData(const xtemplatecall* pCall);



/* 返回参数数量、指定位置参数或命名参数。 */
XRT_API size_t xrtTemplateCallArgumentCount(const xtemplatecall* pCall);
XRT_API bool xrtTemplateCallArgument(
	const xtemplatecall* pCall,
	size_t iIndex,
	xtemplateargview* pArgument
);
XRT_API bool xrtTemplateCallFind(
	const xtemplatecall* pCall,
	xstrview Name,
	xtemplateargview* pArgument
);



/* 在当前渲染作用域内求值参数，或通过共享 writer 写出分片。 */
XRT_API bool xrtTemplateCallEval(
	xtemplatecall* pCall,
	const xtemplateargview* pArgument,
	xtemplatevalue* pValue
);
XRT_API bool xrtTemplateCallWrite(
	xtemplatecall* pCall,
	xstrview Text
);



/* 渲染解析块主体，或临时替换当前值后渲染主体。 */
XRT_API bool xrtTemplateCallRender(xtemplatecall* pCall);
XRT_API bool xrtTemplateCallRenderCurrent(
	xtemplatecall* pCall,
	const xvalue* pCurrent
);



/* 返回原样主体和当前、根、全局作用域的借用视图。 */
XRT_API xstrview xrtTemplateCallRaw(const xtemplatecall* pCall);
XRT_API const xvalue* xrtTemplateCallCurrent(const xtemplatecall* pCall);
XRT_API const xvalue* xrtTemplateCallRoot(const xtemplatecall* pCall);
XRT_API const xvalue* xrtTemplateCallGlobal(const xtemplatecall* pCall);

#endif



/* 初始化默认括号和有限编译预算。 */
XRT_API void xrtTemplateConfigInit(xtemplateconfig* pConfig);



/* 初始化默认作用域和有限渲染预算。 */
XRT_API void xrtTemplateRenderConfigInit(xtemplaterenderconfig* pConfig);



/* 使用默认配置编译模板源码。 */
XRT_API xtemplate* xrtTemplateCompile(xstrview Source);



/* 使用显式配置编译模板源码。 */
XRT_API xtemplate* xrtTemplateCompileConfig(
	xstrview Source,
	const xtemplateconfig* pConfig
);



#if defined(XRT_FEATURE_TEMPLATE_FILE)

/* 在配置源码上限内读取完整文件并编译，文件内容只在调用期间持有。 */
XRT_API xtemplate* xrtTemplateCompileFileConfig(
	cstr sPath,
	const xtemplateconfig* pConfig
);



/* 使用默认模板配置读取并编译完整文件。 */
XRT_API xtemplate* xrtTemplateCompileFile(cstr sPath);

#endif



/* 增加不可变模板引用并返回原指针。 */
XRT_API xtemplate* xrtTemplateRef(xtemplate* pTemplate);



/* 释放模板引用。 */
XRT_API void xrtTemplateRelease(xtemplate* pTemplate);



/* 返回模板持有的原始源码视图。 */
XRT_API xstrview xrtTemplateSource(const xtemplate* pTemplate);



/* 返回模板中的全部编译节点数量，包括控制块内部节点。 */
XRT_API size_t xrtTemplateNodeCount(const xtemplate* pTemplate);



/* 返回指定编译节点的只读视图。 */
XRT_API bool xrtTemplateNode(
	const xtemplate* pTemplate,
	size_t iIndex,
	xtemplatenodeview* pNode
);



/* 把渲染分片写入回调；回调已经写出的内容不能回滚。 */
XRT_API bool xrtTemplateWrite(
	const xtemplate* pTemplate,
	const xtemplaterenderconfig* pConfig,
	xtemplatewritefn pWrite,
	ptr pUserData
);



/* 把渲染结果事务追加到字符串构建器。 */
XRT_API bool xrtTemplateRenderTo(
	const xtemplate* pTemplate,
	const xtemplaterenderconfig* pConfig,
	xstrbuf* pOutput
);



/* 使用当前值作为根和当前作用域，返回由 xrtFree 释放的字符串。 */
XRT_API str xrtTemplateRender(
	const xtemplate* pTemplate,
	const xvalue* pData,
	size_t* pSize
);



/* 从模板错误的数据字段读取源码位置。 */
XRT_API bool xrtTemplateErrorLocation(
	const xerror* pError,
	xtemplatelocation* pLocation
);



XRT_EXTERN_C_END

#endif

#endif
