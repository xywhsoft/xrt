#ifndef XRT_XLON_H
#define XRT_XLON_H

#include <xrt/value.h>



#if defined(XRT_FEATURE_XLON) && !defined(XRT_FEATURE_XLON_FILE)
	#error "XRT_FEATURE_XLON requires XRT_FEATURE_XLON_FILE"
#endif

#if (defined(XRT_FEATURE_XLON_READ) || defined(XRT_FEATURE_XLON_WRITE)) && \
	!defined(XRT_FEATURE_XLON_CORE)
	#error "XLON read and write features require XRT_FEATURE_XLON_CORE"
#endif

#if defined(XRT_FEATURE_XLON_READ) && !defined(XRT_FEATURE_VALUE_CONTAINER)
	#error "XRT_FEATURE_XLON_READ requires XRT_FEATURE_VALUE_CONTAINER"
#endif

#if defined(XRT_FEATURE_XLON_READ) && !defined(XRT_FEATURE_BUFFER)
	#error "XRT_FEATURE_XLON_READ requires XRT_FEATURE_BUFFER"
#endif

#if defined(XRT_FEATURE_XLON_READ) && !defined(XRT_FEATURE_CODEC_BASE64)
	#error "XRT_FEATURE_XLON_READ requires XRT_FEATURE_CODEC_BASE64"
#endif

#if defined(XRT_FEATURE_XLON_READ) && !defined(XRT_FEATURE_TIME_TEXT)
	#error "XRT_FEATURE_XLON_READ requires XRT_FEATURE_TIME_TEXT"
#endif

#if defined(XRT_FEATURE_XLON_READ) && !defined(XRT_FEATURE_NUMBER_INTEGER)
	#error "XRT_FEATURE_XLON_READ requires XRT_FEATURE_NUMBER_INTEGER"
#endif

#if defined(XRT_FEATURE_XLON_READ) && !defined(XRT_FEATURE_NUMBER_FLOAT)
	#error "XRT_FEATURE_XLON_READ requires XRT_FEATURE_NUMBER_FLOAT"
#endif

#if defined(XRT_FEATURE_XLON_READ) && !defined(XRT_FEATURE_UNICODE)
	#error "XRT_FEATURE_XLON_READ requires XRT_FEATURE_UNICODE"
#endif

#if defined(XRT_FEATURE_XLON_WRITE) && !defined(XRT_FEATURE_VALUE_CONTAINER)
	#error "XRT_FEATURE_XLON_WRITE requires XRT_FEATURE_VALUE_CONTAINER"
#endif

#if defined(XRT_FEATURE_XLON_WRITE) && !defined(XRT_FEATURE_BUFFER)
	#error "XRT_FEATURE_XLON_WRITE requires XRT_FEATURE_BUFFER"
#endif

#if defined(XRT_FEATURE_XLON_WRITE) && !defined(XRT_FEATURE_CODEC_BASE64)
	#error "XRT_FEATURE_XLON_WRITE requires XRT_FEATURE_CODEC_BASE64"
#endif

#if defined(XRT_FEATURE_XLON_WRITE) && !defined(XRT_FEATURE_TIME_TEXT)
	#error "XRT_FEATURE_XLON_WRITE requires XRT_FEATURE_TIME_TEXT"
#endif

#if defined(XRT_FEATURE_XLON_WRITE) && !defined(XRT_FEATURE_NUMBER_INTEGER)
	#error "XRT_FEATURE_XLON_WRITE requires XRT_FEATURE_NUMBER_INTEGER"
#endif

#if defined(XRT_FEATURE_XLON_WRITE) && !defined(XRT_FEATURE_NUMBER_FLOAT)
	#error "XRT_FEATURE_XLON_WRITE requires XRT_FEATURE_NUMBER_FLOAT"
#endif

#if defined(XRT_FEATURE_XLON_WRITE) && !defined(XRT_FEATURE_UNICODE)
	#error "XRT_FEATURE_XLON_WRITE requires XRT_FEATURE_UNICODE"
#endif

#if defined(XRT_FEATURE_XLON_WRITE) && !defined(XRT_FEATURE_JSON_ESCAPE)
	#error "XRT_FEATURE_XLON_WRITE requires XRT_FEATURE_JSON_ESCAPE"
#endif

#if defined(XRT_FEATURE_XLON_FILE) && !defined(XRT_FEATURE_FILE_WHOLE)
	#error "XRT_FEATURE_XLON_FILE requires XRT_FEATURE_FILE_WHOLE"
#endif

#if defined(XRT_FEATURE_XLON_FILE) && \
	(!defined(XRT_FEATURE_XLON_READ) || !defined(XRT_FEATURE_XLON_WRITE))
	#error "XRT_FEATURE_XLON_FILE requires XLON read and write features"
#endif



#if defined(XRT_FEATURE_XLON_READ) || defined(XRT_FEATURE_XLON_WRITE)

#define XXLON_DEPTH_DEFAULT 256u
#define XXLON_INPUT_DEFAULT (64u * 1024u * 1024u)
#define XXLON_STRING_DEFAULT (16u * 1024u * 1024u)
#define XXLON_VALUES_DEFAULT 1000000u
#define XXLON_CONTAINER_DEFAULT 1000000u
#define XXLON_DECODED_DEFAULT (64u * 1024u * 1024u)



/* XLON 模块错误码在 xrt.xlon 域内保持稳定。 */
typedef enum xxlonerror {
	XXLON_ERROR_CONFIG = 1401,
	XXLON_ERROR_SYNTAX,
	XXLON_ERROR_LIMIT,
	XXLON_ERROR_DUPLICATE,
	XXLON_ERROR_NUMBER,
	XXLON_ERROR_TAG,
	XXLON_ERROR_STATE,
	XXLON_ERROR_UNSUPPORTED,
	XXLON_ERROR_OUTPUT,
	XXLON_ERROR_IO
} xxlonerror;



/* 文本位置使用零基字节偏移和一基行列。 */
typedef struct xxlonlocation {
	size_t Offset;
	size_t Line;
	size_t Column;
} xxlonlocation;



XRT_EXTERN_C_BEGIN



/* 从 xrt.xlon 错误的机器数据中读取文本位置。 */
XRT_API bool xrtXlonErrorLocation(
	const xerror* pError,
	xxlonlocation* pLocation
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_XLON_READ)

/* 非标准空白扩展和自定义标签默认全部关闭。 */
typedef enum xxlonreadflag {
	XXLON_READ_COMMENTS = UINT32_C(0x00000001),
	XXLON_READ_TRAILING_COMMA = UINT32_C(0x00000002),
	XXLON_READ_CUSTOM = UINT32_C(0x00000004)
} xxlonreadflag;



/* 对象和整数映射使用同一套明确的重复键策略。 */
typedef enum xxlonduplicate {
	XXLON_DUPLICATE_REJECT = 0,
	XXLON_DUPLICATE_KEEP,
	XXLON_DUPLICATE_REPLACE
} xxlonduplicate;



/* 超出 int64/uint64 的整数默认失败，可显式按 double 接收。 */
typedef enum xxlonbigint {
	XXLON_BIGINT_REJECT = 0,
	XXLON_BIGINT_FLOAT
} xxlonbigint;



/* 自定义标签解码器返回一个拥有引用；失败时应设置具体错误。 */
typedef xvalue* (*xxlondecodeproc)(
	xstrview Tag,
	xstrview Payload,
	ptr pUserData
);



/* XLON 读取配置同时约束语法、资源预算和自定义类型入口。 */
typedef struct xxlonreadconfig {
	uint32 Flags;
	xxlonduplicate Duplicate;
	xxlonbigint BigInteger;
	uint32 MaxDepth;
	size_t MaxInputBytes;
	size_t MaxStringBytes;
	size_t MaxValues;
	size_t MaxContainerItems;
	size_t MaxDecodedBytes;
	xxlondecodeproc Decode;
	ptr DecodeData;
	uint32 Reserved[4];
} xxlonreadconfig;



/* 访问事件直接表达全部可移植 XLON 类型。 */
typedef enum xxloneventtype {
	XXLON_EVENT_NULL = 0,
	XXLON_EVENT_BOOL,
	XXLON_EVENT_INT,
	XXLON_EVENT_FLOAT,
	XXLON_EVENT_STRING,
	XXLON_EVENT_BYTES,
	XXLON_EVENT_TIME,
	XXLON_EVENT_CUSTOM,
	XXLON_EVENT_ARRAY_BEGIN,
	XXLON_EVENT_ARRAY_END,
	XXLON_EVENT_INT_MAP_BEGIN,
	XXLON_EVENT_INT_MAP_END,
	XXLON_EVENT_SET_BEGIN,
	XXLON_EVENT_SET_END,
	XXLON_EVENT_OBJECT_BEGIN,
	XXLON_EVENT_OBJECT_END,
	XXLON_EVENT_UINT,
	/* Appended to preserve the numeric identity of published event kinds. */
	XXLON_EVENT_CHAR
} xxloneventtype;



/* 回调可继续、正常提前停止或报告失败。 */
typedef enum xxlonvisitaction {
	XXLON_VISIT_NEXT = 0,
	XXLON_VISIT_STOP,
	XXLON_VISIT_FAIL
} xxlonvisitaction;



/* 访问结果明确区分完成、调用方停止和失败。 */
typedef enum xxlonvisitresult {
	XXLON_VISIT_ERROR = -1,
	XXLON_VISIT_DONE = 0,
	XXLON_VISIT_STOPPED = 1
} xxlonvisitresult;



/* 自定义标签保留名称和已经完成 JSON 反转义的字符串载荷。 */
typedef struct xxlontag {
	xstrview Name;
	xstrview Payload;
} xxlontag;



/* 键按父容器类型明确区分，事件视图只在回调期间有效。 */
typedef struct xxlonevent {
	xxloneventtype Type;
	xxlonlocation Location;
	size_t Depth;
	xvaluekey Key;
	xstrview Raw;
	union {
		bool Boolean;
		int64 Integer;
		uint64 Unsigned;
		uint32 Character;
		double Float;
		xstrview String;
		xbytesview Bytes;
		xtime Time;
		xxlontag Tag;
	} Value;
} xxlonevent;



/* XLON 访问器不得保存事件中的借用视图。 */
typedef xxlonvisitaction (*xxlonvisitproc)(
	const xxlonevent* pEvent,
	ptr pUserData
);



XRT_EXTERN_C_BEGIN



/* 初始化严格语法、拒绝重复键和有限资源预算。 */
XRT_API void xrtXlonReadConfigInit(xxlonreadconfig* pConfig);



/* 使用默认严格配置解析一个完整 XLON 文本。 */
XRT_API xvalue* xrtXlonParse(xstrview Text);



/* 使用高级配置解析一个完整 XLON 文本。 */
XRT_API xvalue* xrtXlonRead(
	xstrview Text,
	const xxlonreadconfig* pConfig
);



/* 验证默认 XLON 语法和内建标签，不构造 Value DOM。 */
XRT_API bool xrtXlonValid(xstrview Text);



/* 直接访问解析事件，不构造中间 DOM。 */
XRT_API xxlonvisitresult xrtXlonVisit(
	xstrview Text,
	const xxlonreadconfig* pConfig,
	xxlonvisitproc pVisitor,
	ptr pUserData
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_XLON_WRITE)

/* 输出标志只改变文本布局和字符串转义。 */
typedef enum xxlonwriteflag {
	XXLON_WRITE_PRETTY = UINT32_C(0x00000001),
	XXLON_WRITE_ESCAPE_SLASH = UINT32_C(0x00000002),
	XXLON_WRITE_ESCAPE_HTML = UINT32_C(0x00000004),
	XXLON_WRITE_ESCAPE_NON_ASCII = UINT32_C(0x00000008)
} xxlonwriteflag;



/* 不可直接表示的值默认失败，也可显式跳过容器成员。 */
typedef enum xxlonunsupported {
	XXLON_UNSUPPORTED_REJECT = 0,
	XXLON_UNSUPPORTED_SKIP
} xxlonunsupported;



/* 自定义编码回调明确区分不处理、成功和失败。 */
typedef enum xxloncoderesult {
	XXLON_CODE_ERROR = -1,
	XXLON_CODE_UNSUPPORTED = 0,
	XXLON_CODE_OK = 1
} xxloncoderesult;



/* 编码器接收仅在回调期间有效的只读快照；返回视图保持到本次调用返回。 */
typedef xxloncoderesult (*xxlonencodeproc)(
	const xvalue* pValue,
	xstrview* pTag,
	xstrview* pPayload,
	ptr pUserData
);



/* XLON 写出配置提供固定上限和唯一自定义类型入口。 */
typedef struct xxlonwriteconfig {
	uint32 Flags;
	xxlonunsupported Unsupported;
	uint32 MaxDepth;
	uint32 Indent;
	size_t MaxOutputBytes;
	xxlonencodeproc Encode;
	ptr EncodeData;
	uint32 Reserved[4];
} xxlonwriteconfig;



/* 输出回调必须在返回前消费借用字节。 */
typedef bool (*xxlonwriteproc)(xbytesview Data, ptr pUserData);



/* 增量写入器保持不透明，所有方法都拒绝回调重入。 */
typedef struct xxlonwriter xxlonwriter;



XRT_EXTERN_C_BEGIN



/* 初始化紧凑输出、严格类型和有限输出预算。 */
XRT_API void xrtXlonWriteConfigInit(xxlonwriteconfig* pConfig);



/* 紧凑或美化地序列化 Value，并返回由 xrtFree 释放的文本。 */
XRT_API str xrtXlonStringify(
	const xvalue* pValue,
	bool bPretty,
	size_t* pSize
);



/* 使用高级配置把 Value 同步写入调用方输出回调。 */
XRT_API bool xrtXlonWrite(
	const xvalue* pValue,
	const xxlonwriteconfig* pConfig,
	xxlonwriteproc pWrite,
	ptr pUserData
);



/* 创建把增量结果保存在内存中的 XLON 写入器。 */
XRT_API xxlonwriter* xrtXlonWriterCreate(
	const xxlonwriteconfig* pConfig
);



/* 创建把增量结果同步提交给回调的 XLON 写入器。 */
XRT_API xxlonwriter* xrtXlonWriterCreateSink(
	const xxlonwriteconfig* pConfig,
	xxlonwriteproc pWrite,
	ptr pUserData
);



/* 在当前位置开始对象。 */
XRT_API bool xrtXlonWriterObject(xxlonwriter* pWriter);



/* 在当前位置开始数组。 */
XRT_API bool xrtXlonWriterArray(xxlonwriter* pWriter);



/* 在当前位置开始整数键映射。 */
XRT_API bool xrtXlonWriterIntMap(xxlonwriter* pWriter);



/* 在当前位置开始集合。 */
XRT_API bool xrtXlonWriterSet(xxlonwriter* pWriter);



/* 结束最近开始的容器。 */
XRT_API bool xrtXlonWriterEnd(xxlonwriter* pWriter);



/* 为对象中的下一个值写入字符串名称。 */
XRT_API bool xrtXlonWriterName(xxlonwriter* pWriter, xstrview Name);



/* 为整数映射中的下一个值写入 int64 键。 */
XRT_API bool xrtXlonWriterKey(xxlonwriter* pWriter, int64 iKey);



/* 写入 null。 */
XRT_API bool xrtXlonWriterNull(xxlonwriter* pWriter);



/* 写入布尔值。 */
XRT_API bool xrtXlonWriterBool(xxlonwriter* pWriter, bool bValue);



/* 写入 int64。 */
XRT_API bool xrtXlonWriterInt(xxlonwriter* pWriter, int64 iValue);



/* 写入 uint64。 */
XRT_API bool xrtXlonWriterUInt(xxlonwriter* pWriter, uint64 iValue);



/* 写入保留字符身份的 Unicode 标量标签。 */
XRT_API bool xrtXlonWriterChar(xxlonwriter* pWriter, uint32 iValue);



/* 写入 double，非有限值使用显式 float 标签。 */
XRT_API bool xrtXlonWriterFloat(xxlonwriter* pWriter, double fValue);



/* 写入严格 UTF-8 字符串。 */
XRT_API bool xrtXlonWriterString(xxlonwriter* pWriter, xstrview Text);



/* 写入规范 Base64 二进制标签。 */
XRT_API bool xrtXlonWriterBytes(xxlonwriter* pWriter, xbytesview Data);



/* 写入 UTC RFC 3339 时间标签。 */
XRT_API bool xrtXlonWriterTime(xxlonwriter* pWriter, xtime Time);



/* 写入已经验证名称和载荷的自定义标签。 */
XRT_API bool xrtXlonWriterTag(
	xxlonwriter* pWriter,
	xstrview Tag,
	xstrview Payload
);



/* 在当前位置写入完整 Value 子树。 */
XRT_API bool xrtXlonWriterValue(
	xxlonwriter* pWriter,
	const xvalue* pValue
);



/* 验证根值和容器已完整结束，并关闭写入器。 */
XRT_API bool xrtXlonWriterFinish(xxlonwriter* pWriter);



/* 从已完成的内存写入器移交文本。 */
XRT_API str xrtXlonWriterTake(xxlonwriter* pWriter, size_t* pSize);



/* 销毁写入器和未移交的内存结果。 */
XRT_API void xrtXlonWriterFree(xxlonwriter* pWriter);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_XLON_FILE)

XRT_EXTERN_C_BEGIN



/* 使用默认严格配置读取并解析 XLON 文件。 */
XRT_API xvalue* xrtXlonParseFile(cstr sPath);



/* 使用读取配置及其输入上限解析 XLON 文件。 */
XRT_API xvalue* xrtXlonReadFile(
	cstr sPath,
	const xxlonreadconfig* pConfig
);



/* 使用高级配置序列化并原子替换 XLON 文件。 */
XRT_API bool xrtXlonWriteFile(
	cstr sPath,
	const xvalue* pValue,
	const xxlonwriteconfig* pConfig
);



/* 紧凑或美化地序列化并原子替换 XLON 文件。 */
XRT_API bool xrtXlonStringifyFile(
	cstr sPath,
	const xvalue* pValue,
	bool bPretty
);



XRT_EXTERN_C_END

#endif

#endif
