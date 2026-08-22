#ifndef XRT_JSON_H
#define XRT_JSON_H

#include <xrt/value.h>



#if defined(XRT_FEATURE_JSON) && !defined(XRT_FEATURE_JSON_FILE)
	#error "XRT_FEATURE_JSON requires XRT_FEATURE_JSON_FILE"
#endif

#if (defined(XRT_FEATURE_JSON_READ) || defined(XRT_FEATURE_JSON_WRITE)) && \
	!defined(XRT_FEATURE_JSON_CORE)
	#error "JSON read and write features require XRT_FEATURE_JSON_CORE"
#endif

#if defined(XRT_FEATURE_JSON_ESCAPE) && \
	(!defined(XRT_FEATURE_JSON_CORE) || !defined(XRT_FEATURE_UNICODE))
	#error "XRT_FEATURE_JSON_ESCAPE requires JSON core and Unicode"
#endif

#if defined(XRT_FEATURE_JSON_WRITE) && !defined(XRT_FEATURE_JSON_ESCAPE)
	#error "XRT_FEATURE_JSON_WRITE requires XRT_FEATURE_JSON_ESCAPE"
#endif

#if defined(XRT_FEATURE_JSON_READ) && !defined(XRT_FEATURE_VALUE_CONTAINER)
	#error "XRT_FEATURE_JSON_READ requires XRT_FEATURE_VALUE_CONTAINER"
#endif

#if defined(XRT_FEATURE_JSON_READ) && !defined(XRT_FEATURE_BUFFER)
	#error "XRT_FEATURE_JSON_READ requires XRT_FEATURE_BUFFER"
#endif

#if defined(XRT_FEATURE_JSON_READ) && !defined(XRT_FEATURE_NUMBER_INTEGER)
	#error "XRT_FEATURE_JSON_READ requires XRT_FEATURE_NUMBER_INTEGER"
#endif

#if defined(XRT_FEATURE_JSON_READ) && !defined(XRT_FEATURE_NUMBER_FLOAT)
	#error "XRT_FEATURE_JSON_READ requires XRT_FEATURE_NUMBER_FLOAT"
#endif

#if defined(XRT_FEATURE_JSON_READ) && !defined(XRT_FEATURE_UNICODE)
	#error "XRT_FEATURE_JSON_READ requires XRT_FEATURE_UNICODE"
#endif

#if defined(XRT_FEATURE_JSON_WRITE) && !defined(XRT_FEATURE_VALUE_CONTAINER)
	#error "XRT_FEATURE_JSON_WRITE requires XRT_FEATURE_VALUE_CONTAINER"
#endif

#if defined(XRT_FEATURE_JSON_WRITE) && !defined(XRT_FEATURE_BUFFER)
	#error "XRT_FEATURE_JSON_WRITE requires XRT_FEATURE_BUFFER"
#endif

#if defined(XRT_FEATURE_JSON_WRITE) && !defined(XRT_FEATURE_NUMBER_INTEGER)
	#error "XRT_FEATURE_JSON_WRITE requires XRT_FEATURE_NUMBER_INTEGER"
#endif

#if defined(XRT_FEATURE_JSON_WRITE) && !defined(XRT_FEATURE_NUMBER_FLOAT)
	#error "XRT_FEATURE_JSON_WRITE requires XRT_FEATURE_NUMBER_FLOAT"
#endif

#if defined(XRT_FEATURE_JSON_WRITE) && !defined(XRT_FEATURE_UNICODE)
	#error "XRT_FEATURE_JSON_WRITE requires XRT_FEATURE_UNICODE"
#endif

#if defined(XRT_FEATURE_JSON_FILE) && !defined(XRT_FEATURE_FILE_WHOLE)
	#error "XRT_FEATURE_JSON_FILE requires XRT_FEATURE_FILE_WHOLE"
#endif

#if defined(XRT_FEATURE_JSON_FILE) && \
	(!defined(XRT_FEATURE_JSON_READ) || !defined(XRT_FEATURE_JSON_WRITE))
	#error "XRT_FEATURE_JSON_FILE requires JSON read and write features"
#endif



#if defined(XRT_FEATURE_JSON_READ) || \
	defined(XRT_FEATURE_JSON_WRITE) || \
	defined(XRT_FEATURE_JSON_ESCAPE)

#define XJSON_DEPTH_DEFAULT 256u
#define XJSON_INPUT_DEFAULT (64u * 1024u * 1024u)
#define XJSON_STRING_DEFAULT (16u * 1024u * 1024u)
#define XJSON_VALUES_DEFAULT 1000000u
#define XJSON_CONTAINER_DEFAULT 1000000u



/* JSON 模块错误码在 xrt.json 域内保持稳定。 */
typedef enum xjsonerror {
	XJSON_ERROR_CONFIG = 1301,
	XJSON_ERROR_SYNTAX,
	XJSON_ERROR_LIMIT,
	XJSON_ERROR_DUPLICATE,
	XJSON_ERROR_NUMBER,
	XJSON_ERROR_STATE,
	XJSON_ERROR_UNSUPPORTED,
	XJSON_ERROR_OUTPUT,
	XJSON_ERROR_IO
} xjsonerror;



/* 文本位置使用零基字节偏移和一基行列；列按 UTF-8 字节计算。 */
typedef struct xjsonlocation {
	size_t Offset;
	size_t Line;
	size_t Column;
} xjsonlocation;



XRT_EXTERN_C_BEGIN



/* 从 xrt.json 错误的机器数据中读取文本位置。 */
XRT_API bool xrtJsonErrorLocation(
	const xerror* pError,
	xjsonlocation* pLocation
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_JSON_READ)

/* 非标准读取能力默认全部关闭，只能由调用方逐项开启。 */
typedef enum xjsonreadflag {
	XJSON_READ_COMMENTS = UINT32_C(0x00000001),
	XJSON_READ_TRAILING_COMMA = UINT32_C(0x00000002)
} xjsonreadflag;



/* 对象重复键必须由 DOM 调用方明确选择处理口径。 */
typedef enum xjsonduplicate {
	XJSON_DUPLICATE_REJECT = 0,
	XJSON_DUPLICATE_KEEP,
	XJSON_DUPLICATE_REPLACE
} xjsonduplicate;



/* 超出 int64 的整数字面量默认失败，显式浮点策略允许有损接收。 */
typedef enum xjsonbigint {
	XJSON_BIGINT_REJECT = 0,
	XJSON_BIGINT_FLOAT
} xjsonbigint;



/* JSON 读取配置同时约束资源消耗和少量显式兼容语法。 */
typedef struct xjsonreadconfig {
	uint32 Flags;
	xjsonduplicate Duplicate;
	xjsonbigint BigInteger;
	uint32 MaxDepth;
	size_t MaxInputBytes;
	size_t MaxStringBytes;
	size_t MaxValues;
	size_t MaxContainerItems;
	uint32 Reserved[4];
} xjsonreadconfig;



/* 访问事件在回调返回后失效；字符串与名称已经完成反转义。 */
typedef enum xjsoneventtype {
	XJSON_EVENT_NULL = 0,
	XJSON_EVENT_BOOL,
	XJSON_EVENT_INT,
	XJSON_EVENT_FLOAT,
	XJSON_EVENT_STRING,
	XJSON_EVENT_ARRAY_BEGIN,
	XJSON_EVENT_ARRAY_END,
	XJSON_EVENT_OBJECT_BEGIN,
	XJSON_EVENT_OBJECT_END
} xjsoneventtype;



/* 回调可继续、正常提前停止或报告失败。 */
typedef enum xjsonvisitaction {
	XJSON_VISIT_NEXT = 0,
	XJSON_VISIT_STOP,
	XJSON_VISIT_FAIL
} xjsonvisitaction;



/* 访问结果明确区分完整完成、调用方停止和解析失败。 */
typedef enum xjsonvisitresult {
	XJSON_VISIT_ERROR = -1,
	XJSON_VISIT_DONE = 0,
	XJSON_VISIT_STOPPED = 1
} xjsonvisitresult;



/* 单个事件携带父容器定位、token 位置和值；Raw 只用于数字事件。 */
typedef struct xjsonevent {
	xjsoneventtype Type;
	xjsonlocation Location;
	size_t Depth;
	bool HasName;
	xstrview Name;
	size_t Index;
	xstrview Raw;
	union {
		bool Boolean;
		int64 Integer;
		double Float;
		xstrview String;
	} Value;
} xjsonevent;



/* JSON 访问器不得保存事件中的借用视图，失败时应设置更具体的错误。 */
typedef xjsonvisitaction (*xjsonvisitproc)(
	const xjsonevent* pEvent,
	ptr pUserData
);



XRT_EXTERN_C_BEGIN



/* 初始化严格 JSON、重复键拒绝和有限资源预算。 */
XRT_API void xrtJsonReadConfigInit(xjsonreadconfig* pConfig);



/* 使用默认严格配置解析一个完整 JSON 文本。 */
XRT_API xvalue* xrtJsonParse(xstrview Text);



/* 使用高级配置解析一个完整 JSON 文本。 */
XRT_API xvalue* xrtJsonRead(
	xstrview Text,
	const xjsonreadconfig* pConfig
);



/* 使用默认严格配置验证一个完整 JSON 文本，不构造 Value DOM。 */
XRT_API bool xrtJsonValid(xstrview Text);



/* 直接访问解析事件，不构造中间 DOM。 */
XRT_API xjsonvisitresult xrtJsonVisit(
	xstrview Text,
	const xjsonreadconfig* pConfig,
	xjsonvisitproc pVisitor,
	ptr pUserData
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_JSON_WRITE) || defined(XRT_FEATURE_JSON_ESCAPE)

/* 输出标志只改变文本表示，不改变 Value 数据。 */
typedef enum xjsonwriteflag {
	XJSON_WRITE_PRETTY = UINT32_C(0x00000001),
	XJSON_WRITE_ESCAPE_SLASH = UINT32_C(0x00000002),
	XJSON_WRITE_ESCAPE_HTML = UINT32_C(0x00000004),
	XJSON_WRITE_ESCAPE_NON_ASCII = UINT32_C(0x00000008),
	XJSON_WRITE_CONTAINER_COMPAT = UINT32_C(0x00000010)
} xjsonwriteflag;



/* 输出回调必须在返回前消费借用字节，失败时应设置具体错误。 */
typedef bool (*xjsonwriteproc)(xbytesview Data, ptr pUserData);

#endif



#if defined(XRT_FEATURE_JSON_ESCAPE)

XRT_EXTERN_C_BEGIN



/* 严格校验 UTF-8 并流式写出包含双引号的 JSON 字符串 token。 */
XRT_API bool xrtJsonQuoteWrite(
	xstrview Text,
	uint32 iFlags,
	xjsonwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_JSON_WRITE)



/* 非有限浮点默认失败，也可显式写成 null 或字符串。 */
typedef enum xjsonnonfinite {
	XJSON_NONFINITE_REJECT = 0,
	XJSON_NONFINITE_NULL,
	XJSON_NONFINITE_STRING
} xjsonnonfinite;



/* 不受 JSON 表达的 Value 默认失败，也可显式写 null 或跳过成员。 */
typedef enum xjsonunsupported {
	XJSON_UNSUPPORTED_REJECT = 0,
	XJSON_UNSUPPORTED_NULL,
	XJSON_UNSUPPORTED_SKIP
} xjsonunsupported;



/* JSON 写出配置提供固定上限；Indent 只在美化输出时生效。 */
typedef struct xjsonwriteconfig {
	uint32 Flags;
	xjsonnonfinite NonFinite;
	xjsonunsupported Unsupported;
	uint32 MaxDepth;
	uint32 Indent;
	size_t MaxOutputBytes;
	uint32 Reserved[4];
} xjsonwriteconfig;



/* 增量写入器保持不透明，写入方法不可从输出回调重入。 */
typedef struct xjsonwriter xjsonwriter;



XRT_EXTERN_C_BEGIN



/* 初始化紧凑输出、严格类型和有限输出预算。 */
XRT_API void xrtJsonWriteConfigInit(xjsonwriteconfig* pConfig);



/* 紧凑或美化地序列化 Value，并返回由 xrtFree 释放的字符串。 */
XRT_API str xrtJsonStringify(
	const xvalue* pValue,
	bool bPretty,
	size_t* pSize
);



/* 使用高级配置把 Value 同步写入调用方输出回调。 */
XRT_API bool xrtJsonWrite(
	const xvalue* pValue,
	const xjsonwriteconfig* pConfig,
	xjsonwriteproc pWrite,
	ptr pUserData
);



/* 创建把增量结果保存在内存中的 JSON 写入器。 */
XRT_API xjsonwriter* xrtJsonWriterCreate(
	const xjsonwriteconfig* pConfig
);



/* 创建把增量结果同步提交给回调的 JSON 写入器。 */
XRT_API xjsonwriter* xrtJsonWriterCreateSink(
	const xjsonwriteconfig* pConfig,
	xjsonwriteproc pWrite,
	ptr pUserData
);



/* 在当前位置开始对象；对象中必须先写 Name，数组中直接写值。 */
XRT_API bool xrtJsonWriterObject(xjsonwriter* pWriter);



/* 在当前位置开始数组。 */
XRT_API bool xrtJsonWriterArray(xjsonwriter* pWriter);



/* 结束最近开始的对象或数组。 */
XRT_API bool xrtJsonWriterEnd(xjsonwriter* pWriter);



/* 为对象中的下一个值写入名称。 */
XRT_API bool xrtJsonWriterName(xjsonwriter* pWriter, xstrview Name);



/* 写入 null。 */
XRT_API bool xrtJsonWriterNull(xjsonwriter* pWriter);



/* 写入布尔值。 */
XRT_API bool xrtJsonWriterBool(xjsonwriter* pWriter, bool bValue);



/* 写入 int64。 */
XRT_API bool xrtJsonWriterInt(xjsonwriter* pWriter, int64 iValue);



/* 按配置写入 double。 */
XRT_API bool xrtJsonWriterFloat(xjsonwriter* pWriter, double fValue);



/* 写入严格 UTF-8 字符串。 */
XRT_API bool xrtJsonWriterString(xjsonwriter* pWriter, xstrview Text);



/* 在当前位置写入完整 Value 子树。 */
XRT_API bool xrtJsonWriterValue(
	xjsonwriter* pWriter,
	const xvalue* pValue
);



/* 验证根值和容器已经完整结束，并封闭写入器。 */
XRT_API bool xrtJsonWriterFinish(xjsonwriter* pWriter);



/* 从已完成的内存写入器移交文本；结果由 xrtFree 释放。 */
XRT_API str xrtJsonWriterTake(xjsonwriter* pWriter, size_t* pSize);



/* 销毁写入器；未移交的内存结果同时释放。 */
XRT_API void xrtJsonWriterFree(xjsonwriter* pWriter);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_JSON_FILE)

XRT_EXTERN_C_BEGIN



/* 使用默认严格配置读取并解析 JSON 文件。 */
XRT_API xvalue* xrtJsonParseFile(cstr sPath);



/* 使用读取配置和其中的输入上限解析 JSON 文件。 */
XRT_API xvalue* xrtJsonReadFile(
	cstr sPath,
	const xjsonreadconfig* pConfig
);



/* 使用高级配置序列化并原子替换 JSON 文件。 */
XRT_API bool xrtJsonWriteFile(
	cstr sPath,
	const xvalue* pValue,
	const xjsonwriteconfig* pConfig
);



/* 紧凑或美化地序列化并原子替换 JSON 文件。 */
XRT_API bool xrtJsonStringifyFile(
	cstr sPath,
	const xvalue* pValue,
	bool bPretty
);



XRT_EXTERN_C_END

#endif

#endif
