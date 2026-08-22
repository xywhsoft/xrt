#ifndef XRT_XSON_H
#define XRT_XSON_H

#include <xrt/value.h>



#if defined(XRT_FEATURE_XSON) && !defined(XRT_FEATURE_XSON_FILE)
	#error "XRT_FEATURE_XSON requires XRT_FEATURE_XSON_FILE"
#endif

#if (defined(XRT_FEATURE_XSON_READ) || defined(XRT_FEATURE_XSON_WRITE)) && \
	!defined(XRT_FEATURE_XSON_CORE)
	#error "XSON read and write features require XRT_FEATURE_XSON_CORE"
#endif

#if defined(XRT_FEATURE_XSON_READ) && !defined(XRT_FEATURE_VALUE_CONTAINER)
	#error "XRT_FEATURE_XSON_READ requires XRT_FEATURE_VALUE_CONTAINER"
#endif

#if defined(XRT_FEATURE_XSON_READ) && !defined(XRT_FEATURE_BUFFER)
	#error "XRT_FEATURE_XSON_READ requires XRT_FEATURE_BUFFER"
#endif

#if defined(XRT_FEATURE_XSON_READ) && !defined(XRT_FEATURE_CODEC_BASE64)
	#error "XRT_FEATURE_XSON_READ requires XRT_FEATURE_CODEC_BASE64"
#endif

#if defined(XRT_FEATURE_XSON_READ) && !defined(XRT_FEATURE_TIME_TEXT)
	#error "XRT_FEATURE_XSON_READ requires XRT_FEATURE_TIME_TEXT"
#endif

#if defined(XRT_FEATURE_XSON_READ) && !defined(XRT_FEATURE_NUMBER_INTEGER)
	#error "XRT_FEATURE_XSON_READ requires XRT_FEATURE_NUMBER_INTEGER"
#endif

#if defined(XRT_FEATURE_XSON_READ) && !defined(XRT_FEATURE_NUMBER_FLOAT)
	#error "XRT_FEATURE_XSON_READ requires XRT_FEATURE_NUMBER_FLOAT"
#endif

#if defined(XRT_FEATURE_XSON_READ) && !defined(XRT_FEATURE_UNICODE)
	#error "XRT_FEATURE_XSON_READ requires XRT_FEATURE_UNICODE"
#endif

#if defined(XRT_FEATURE_XSON_WRITE) && !defined(XRT_FEATURE_VALUE_CONTAINER)
	#error "XRT_FEATURE_XSON_WRITE requires XRT_FEATURE_VALUE_CONTAINER"
#endif

#if defined(XRT_FEATURE_XSON_WRITE) && !defined(XRT_FEATURE_BUFFER)
	#error "XRT_FEATURE_XSON_WRITE requires XRT_FEATURE_BUFFER"
#endif

#if defined(XRT_FEATURE_XSON_WRITE) && !defined(XRT_FEATURE_CODEC_BASE64)
	#error "XRT_FEATURE_XSON_WRITE requires XRT_FEATURE_CODEC_BASE64"
#endif

#if defined(XRT_FEATURE_XSON_WRITE) && !defined(XRT_FEATURE_TIME_TEXT)
	#error "XRT_FEATURE_XSON_WRITE requires XRT_FEATURE_TIME_TEXT"
#endif

#if defined(XRT_FEATURE_XSON_WRITE) && !defined(XRT_FEATURE_NUMBER_INTEGER)
	#error "XRT_FEATURE_XSON_WRITE requires XRT_FEATURE_NUMBER_INTEGER"
#endif

#if defined(XRT_FEATURE_XSON_WRITE) && !defined(XRT_FEATURE_NUMBER_FLOAT)
	#error "XRT_FEATURE_XSON_WRITE requires XRT_FEATURE_NUMBER_FLOAT"
#endif

#if defined(XRT_FEATURE_XSON_WRITE) && !defined(XRT_FEATURE_UNICODE)
	#error "XRT_FEATURE_XSON_WRITE requires XRT_FEATURE_UNICODE"
#endif

#if defined(XRT_FEATURE_XSON_WRITE) && !defined(XRT_FEATURE_JSON_ESCAPE)
	#error "XRT_FEATURE_XSON_WRITE requires XRT_FEATURE_JSON_ESCAPE"
#endif

#if defined(XRT_FEATURE_XSON_FILE) && !defined(XRT_FEATURE_FILE_WHOLE)
	#error "XRT_FEATURE_XSON_FILE requires XRT_FEATURE_FILE_WHOLE"
#endif

#if defined(XRT_FEATURE_XSON_FILE) && \
	(!defined(XRT_FEATURE_XSON_READ) || !defined(XRT_FEATURE_XSON_WRITE))
	#error "XRT_FEATURE_XSON_FILE requires XSON read and write features"
#endif



#if defined(XRT_FEATURE_XSON_READ) || defined(XRT_FEATURE_XSON_WRITE)

#define XXSON_DEPTH_DEFAULT 256u
#define XXSON_INPUT_DEFAULT (64u * 1024u * 1024u)
#define XXSON_STRING_DEFAULT (16u * 1024u * 1024u)
#define XXSON_VALUES_DEFAULT 1000000u
#define XXSON_CONTAINER_DEFAULT 1000000u
#define XXSON_DECODED_DEFAULT (64u * 1024u * 1024u)



/* XSON 模块错误码在 xrt.xson 域内保持稳定。 */
typedef enum xxsonerror {
	XXSON_ERROR_CONFIG = 1401,
	XXSON_ERROR_SYNTAX,
	XXSON_ERROR_LIMIT,
	XXSON_ERROR_DUPLICATE,
	XXSON_ERROR_NUMBER,
	XXSON_ERROR_TAG,
	XXSON_ERROR_STATE,
	XXSON_ERROR_UNSUPPORTED,
	XXSON_ERROR_OUTPUT,
	XXSON_ERROR_IO
} xxsonerror;



/* 文本位置使用零基字节偏移和一基行列。 */
typedef struct xxsonlocation {
	size_t Offset;
	size_t Line;
	size_t Column;
} xxsonlocation;



XRT_EXTERN_C_BEGIN



/* 从 xrt.xson 错误的机器数据中读取文本位置。 */
XRT_API bool xrtXsonErrorLocation(
	const xerror* pError,
	xxsonlocation* pLocation
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_XSON_READ)

/* 非标准空白扩展和自定义标签默认全部关闭。 */
typedef enum xxsonreadflag {
	XXSON_READ_COMMENTS = UINT32_C(0x00000001),
	XXSON_READ_TRAILING_COMMA = UINT32_C(0x00000002),
	XXSON_READ_CUSTOM = UINT32_C(0x00000004)
} xxsonreadflag;



/* 对象和整数映射使用同一套明确的重复键策略。 */
typedef enum xxsonduplicate {
	XXSON_DUPLICATE_REJECT = 0,
	XXSON_DUPLICATE_KEEP,
	XXSON_DUPLICATE_REPLACE
} xxsonduplicate;



/* 超出 int64 的整数默认失败，可显式按 double 接收。 */
typedef enum xxsonbigint {
	XXSON_BIGINT_REJECT = 0,
	XXSON_BIGINT_FLOAT
} xxsonbigint;



/* 自定义标签解码器返回一个拥有引用；失败时应设置具体错误。 */
typedef xvalue* (*xxsondecodeproc)(
	xstrview Tag,
	xstrview Payload,
	ptr pUserData
);



/* XSON 读取配置同时约束语法、资源预算和自定义类型入口。 */
typedef struct xxsonreadconfig {
	uint32 Flags;
	xxsonduplicate Duplicate;
	xxsonbigint BigInteger;
	uint32 MaxDepth;
	size_t MaxInputBytes;
	size_t MaxStringBytes;
	size_t MaxValues;
	size_t MaxContainerItems;
	size_t MaxDecodedBytes;
	xxsondecodeproc Decode;
	ptr DecodeData;
	uint32 Reserved[4];
} xxsonreadconfig;



/* 访问事件直接表达全部可移植 XSON 类型。 */
typedef enum xxsoneventtype {
	XXSON_EVENT_NULL = 0,
	XXSON_EVENT_BOOL,
	XXSON_EVENT_INT,
	XXSON_EVENT_FLOAT,
	XXSON_EVENT_STRING,
	XXSON_EVENT_BYTES,
	XXSON_EVENT_TIME,
	XXSON_EVENT_CUSTOM,
	XXSON_EVENT_ARRAY_BEGIN,
	XXSON_EVENT_ARRAY_END,
	XXSON_EVENT_INT_MAP_BEGIN,
	XXSON_EVENT_INT_MAP_END,
	XXSON_EVENT_SET_BEGIN,
	XXSON_EVENT_SET_END,
	XXSON_EVENT_OBJECT_BEGIN,
	XXSON_EVENT_OBJECT_END
} xxsoneventtype;



/* 回调可继续、正常提前停止或报告失败。 */
typedef enum xxsonvisitaction {
	XXSON_VISIT_NEXT = 0,
	XXSON_VISIT_STOP,
	XXSON_VISIT_FAIL
} xxsonvisitaction;



/* 访问结果明确区分完成、调用方停止和失败。 */
typedef enum xxsonvisitresult {
	XXSON_VISIT_ERROR = -1,
	XXSON_VISIT_DONE = 0,
	XXSON_VISIT_STOPPED = 1
} xxsonvisitresult;



/* 自定义标签保留名称和已经完成 JSON 反转义的字符串载荷。 */
typedef struct xxsontag {
	xstrview Name;
	xstrview Payload;
} xxsontag;



/* 键按父容器类型明确区分，事件视图只在回调期间有效。 */
typedef struct xxsonevent {
	xxsoneventtype Type;
	xxsonlocation Location;
	size_t Depth;
	xvaluekey Key;
	xstrview Raw;
	union {
		bool Boolean;
		int64 Integer;
		double Float;
		xstrview String;
		xbytesview Bytes;
		xtime Time;
		xxsontag Tag;
	} Value;
} xxsonevent;



/* XSON 访问器不得保存事件中的借用视图。 */
typedef xxsonvisitaction (*xxsonvisitproc)(
	const xxsonevent* pEvent,
	ptr pUserData
);



XRT_EXTERN_C_BEGIN



/* 初始化严格语法、拒绝重复键和有限资源预算。 */
XRT_API void xrtXsonReadConfigInit(xxsonreadconfig* pConfig);



/* 使用默认严格配置解析一个完整 XSON 文本。 */
XRT_API xvalue* xrtXsonParse(xstrview Text);



/* 使用高级配置解析一个完整 XSON 文本。 */
XRT_API xvalue* xrtXsonRead(
	xstrview Text,
	const xxsonreadconfig* pConfig
);



/* 验证默认 XSON 语法和内建标签，不构造 Value DOM。 */
XRT_API bool xrtXsonValid(xstrview Text);



/* 直接访问解析事件，不构造中间 DOM。 */
XRT_API xxsonvisitresult xrtXsonVisit(
	xstrview Text,
	const xxsonreadconfig* pConfig,
	xxsonvisitproc pVisitor,
	ptr pUserData
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_XSON_WRITE)

/* 输出标志只改变文本布局和字符串转义。 */
typedef enum xxsonwriteflag {
	XXSON_WRITE_PRETTY = UINT32_C(0x00000001),
	XXSON_WRITE_ESCAPE_SLASH = UINT32_C(0x00000002),
	XXSON_WRITE_ESCAPE_HTML = UINT32_C(0x00000004),
	XXSON_WRITE_ESCAPE_NON_ASCII = UINT32_C(0x00000008)
} xxsonwriteflag;



/* 不可直接表示的值默认失败，也可显式跳过容器成员。 */
typedef enum xxsonunsupported {
	XXSON_UNSUPPORTED_REJECT = 0,
	XXSON_UNSUPPORTED_SKIP
} xxsonunsupported;



/* 自定义编码回调明确区分不处理、成功和失败。 */
typedef enum xxsoncoderesult {
	XXSON_CODE_ERROR = -1,
	XXSON_CODE_UNSUPPORTED = 0,
	XXSON_CODE_OK = 1
} xxsoncoderesult;



/* 编码器接收仅在回调期间有效的只读快照；返回视图保持到本次调用返回。 */
typedef xxsoncoderesult (*xxsonencodeproc)(
	const xvalue* pValue,
	xstrview* pTag,
	xstrview* pPayload,
	ptr pUserData
);



/* XSON 写出配置提供固定上限和唯一自定义类型入口。 */
typedef struct xxsonwriteconfig {
	uint32 Flags;
	xxsonunsupported Unsupported;
	uint32 MaxDepth;
	uint32 Indent;
	size_t MaxOutputBytes;
	xxsonencodeproc Encode;
	ptr EncodeData;
	uint32 Reserved[4];
} xxsonwriteconfig;



/* 输出回调必须在返回前消费借用字节。 */
typedef bool (*xxsonwriteproc)(xbytesview Data, ptr pUserData);



/* 增量写入器保持不透明，所有方法都拒绝回调重入。 */
typedef struct xxsonwriter xxsonwriter;



XRT_EXTERN_C_BEGIN



/* 初始化紧凑输出、严格类型和有限输出预算。 */
XRT_API void xrtXsonWriteConfigInit(xxsonwriteconfig* pConfig);



/* 紧凑或美化地序列化 Value，并返回由 xrtFree 释放的文本。 */
XRT_API str xrtXsonStringify(
	const xvalue* pValue,
	bool bPretty,
	size_t* pSize
);



/* 使用高级配置把 Value 同步写入调用方输出回调。 */
XRT_API bool xrtXsonWrite(
	const xvalue* pValue,
	const xxsonwriteconfig* pConfig,
	xxsonwriteproc pWrite,
	ptr pUserData
);



/* 创建把增量结果保存在内存中的 XSON 写入器。 */
XRT_API xxsonwriter* xrtXsonWriterCreate(
	const xxsonwriteconfig* pConfig
);



/* 创建把增量结果同步提交给回调的 XSON 写入器。 */
XRT_API xxsonwriter* xrtXsonWriterCreateSink(
	const xxsonwriteconfig* pConfig,
	xxsonwriteproc pWrite,
	ptr pUserData
);



/* 在当前位置开始对象。 */
XRT_API bool xrtXsonWriterObject(xxsonwriter* pWriter);



/* 在当前位置开始数组。 */
XRT_API bool xrtXsonWriterArray(xxsonwriter* pWriter);



/* 在当前位置开始整数键映射。 */
XRT_API bool xrtXsonWriterIntMap(xxsonwriter* pWriter);



/* 在当前位置开始集合。 */
XRT_API bool xrtXsonWriterSet(xxsonwriter* pWriter);



/* 结束最近开始的容器。 */
XRT_API bool xrtXsonWriterEnd(xxsonwriter* pWriter);



/* 为对象中的下一个值写入字符串名称。 */
XRT_API bool xrtXsonWriterName(xxsonwriter* pWriter, xstrview Name);



/* 为整数映射中的下一个值写入 int64 键。 */
XRT_API bool xrtXsonWriterKey(xxsonwriter* pWriter, int64 iKey);



/* 写入 null。 */
XRT_API bool xrtXsonWriterNull(xxsonwriter* pWriter);



/* 写入布尔值。 */
XRT_API bool xrtXsonWriterBool(xxsonwriter* pWriter, bool bValue);



/* 写入 int64。 */
XRT_API bool xrtXsonWriterInt(xxsonwriter* pWriter, int64 iValue);



/* 写入 double，非有限值使用显式 float 标签。 */
XRT_API bool xrtXsonWriterFloat(xxsonwriter* pWriter, double fValue);



/* 写入严格 UTF-8 字符串。 */
XRT_API bool xrtXsonWriterString(xxsonwriter* pWriter, xstrview Text);



/* 写入规范 Base64 二进制标签。 */
XRT_API bool xrtXsonWriterBytes(xxsonwriter* pWriter, xbytesview Data);



/* 写入 UTC RFC 3339 时间标签。 */
XRT_API bool xrtXsonWriterTime(xxsonwriter* pWriter, xtime Time);



/* 写入已经验证名称和载荷的自定义标签。 */
XRT_API bool xrtXsonWriterTag(
	xxsonwriter* pWriter,
	xstrview Tag,
	xstrview Payload
);



/* 在当前位置写入完整 Value 子树。 */
XRT_API bool xrtXsonWriterValue(
	xxsonwriter* pWriter,
	const xvalue* pValue
);



/* 验证根值和容器已完整结束，并关闭写入器。 */
XRT_API bool xrtXsonWriterFinish(xxsonwriter* pWriter);



/* 从已完成的内存写入器移交文本。 */
XRT_API str xrtXsonWriterTake(xxsonwriter* pWriter, size_t* pSize);



/* 销毁写入器和未移交的内存结果。 */
XRT_API void xrtXsonWriterFree(xxsonwriter* pWriter);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_XSON_FILE)

XRT_EXTERN_C_BEGIN



/* 使用默认严格配置读取并解析 XSON 文件。 */
XRT_API xvalue* xrtXsonParseFile(cstr sPath);



/* 使用读取配置及其输入上限解析 XSON 文件。 */
XRT_API xvalue* xrtXsonReadFile(
	cstr sPath,
	const xxsonreadconfig* pConfig
);



/* 使用高级配置序列化并原子替换 XSON 文件。 */
XRT_API bool xrtXsonWriteFile(
	cstr sPath,
	const xvalue* pValue,
	const xxsonwriteconfig* pConfig
);



/* 紧凑或美化地序列化并原子替换 XSON 文件。 */
XRT_API bool xrtXsonStringifyFile(
	cstr sPath,
	const xvalue* pValue,
	bool bPretty
);



XRT_EXTERN_C_END

#endif

#endif
