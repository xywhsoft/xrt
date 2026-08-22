#ifndef XRT_INTERNAL_TEXT_VALUE_H
#define XRT_INTERNAL_TEXT_VALUE_H

#include "xrt_value.h"



#if defined(XRT_FEATURE_JSON_CORE) || defined(XRT_FEATURE_XSON_CORE)

/* 建立带可选文本位置的格式错误。 */
void __xrtTextValueError(
	xerrkind Kind,
	int32 iCode,
	cstr sDomain,
	cstr sOperation,
	cstr sMessage,
	bool bLocation,
	size_t iOffset,
	size_t iLine,
	size_t iColumn
);



/* 从指定格式错误域的机器数据中读取文本位置。 */
bool __xrtTextValueErrorLocation(
	const xerror* pError,
	cstr sDomain,
	size_t* pOffset,
	size_t* pLine,
	size_t* pColumn
);

#endif



#if defined(XRT_FEATURE_JSON_FILE) || defined(XRT_FEATURE_XSON_FILE)

/* 限额读取完整协议文件，并按调用方错误域包装 I/O 原因链。 */
bytes __xrtTextValueFileReadAll(
	cstr sPath,
	size_t iLimit,
	size_t* pSize,
	cstr sDomain,
	int32 iCode,
	cstr sMessage
);



/* 原子替换完整协议文件，并按调用方错误域包装 I/O 原因链。 */
bool __xrtTextValueFileWriteAll(
	cstr sPath,
	xbytesview Data,
	cstr sDomain,
	int32 iCode,
	cstr sMessage
);

#endif



#if defined(XRT_FEATURE_JSON_READ) || defined(XRT_FEATURE_XSON_READ)

/* 文本值读取器支持严格 JSON，以及由 XSON 显式开启的类型标签。 */
typedef enum xtextvaluedialect {
	XTEXT_VALUE_JSON = 0,
	XTEXT_VALUE_XSON
} xtextvaluedialect;



/* 共同扩展位保持与两种公开读取配置一一对应。 */
typedef enum xtextvaluereadflag {
	XTEXT_VALUE_READ_COMMENTS = UINT32_C(0x00000001),
	XTEXT_VALUE_READ_TRAILING_COMMA = UINT32_C(0x00000002)
} xtextvaluereadflag;



/* 内部错误分类由公开格式适配器映射到各自稳定的错误码。 */
typedef enum xtextvalueerror {
	XTEXT_VALUE_ERROR_SYNTAX = 0,
	XTEXT_VALUE_ERROR_LIMIT,
	XTEXT_VALUE_ERROR_NUMBER,
	XTEXT_VALUE_ERROR_STATE
} xtextvalueerror;



/* 文本位置使用零基字节偏移和一基行列。 */
typedef struct xtextvaluelocation {
	size_t Offset;
	size_t Line;
	size_t Column;
} xtextvaluelocation;



/* 内部事件覆盖 JSON 与 XSON 的共同值树和 XSON 显式标签。 */
typedef enum xtextvalueeventtype {
	XTEXT_VALUE_EVENT_NULL = 0,
	XTEXT_VALUE_EVENT_BOOL,
	XTEXT_VALUE_EVENT_INT,
	XTEXT_VALUE_EVENT_FLOAT,
	XTEXT_VALUE_EVENT_STRING,
	XTEXT_VALUE_EVENT_TAG,
	XTEXT_VALUE_EVENT_ARRAY_BEGIN,
	XTEXT_VALUE_EVENT_ARRAY_END,
	XTEXT_VALUE_EVENT_INT_MAP_BEGIN,
	XTEXT_VALUE_EVENT_INT_MAP_END,
	XTEXT_VALUE_EVENT_SET_BEGIN,
	XTEXT_VALUE_EVENT_SET_END,
	XTEXT_VALUE_EVENT_OBJECT_BEGIN,
	XTEXT_VALUE_EVENT_OBJECT_END
} xtextvalueeventtype;



/* 标签事件的名称借用输入，载荷是已经完成 JSON 反转义的临时视图。 */
typedef struct xtextvaluetag {
	xstrview Name;
	xstrview Payload;
} xtextvaluetag;



/* 键明确区分数组索引、整数键、字符串键和无键集合。 */
typedef struct xtextvalueevent {
	xtextvalueeventtype Type;
	xtextvaluelocation Location;
	size_t Depth;
	xvaluekey Key;
	xstrview Raw;
	union {
		bool Boolean;
		int64 Integer;
		double Float;
		xstrview String;
		xtextvaluetag Tag;
	} Value;
} xtextvalueevent;



/* 内部访问控制与公开 JSON/XSON 访问器保持相同的三态语义。 */
typedef enum xtextvaluevisitaction {
	XTEXT_VALUE_VISIT_NEXT = 0,
	XTEXT_VALUE_VISIT_STOP,
	XTEXT_VALUE_VISIT_FAIL
} xtextvaluevisitaction;



/* 内部运行结果区分完整完成、调用方停止和失败。 */
typedef enum xtextvaluevisitresult {
	XTEXT_VALUE_VISIT_ERROR = -1,
	XTEXT_VALUE_VISIT_DONE = 0,
	XTEXT_VALUE_VISIT_STOPPED = 1
} xtextvaluevisitresult;



/* 解析器只持有语法和资源预算，不持有 DOM 策略。 */
typedef struct xtextvaluereadconfig {
	xtextvaluedialect Dialect;
	uint32 Flags;
	bool BigIntegerFloat;
	uint32 MaxDepth;
	size_t MaxInputBytes;
	size_t MaxStringBytes;
	size_t MaxValues;
	size_t MaxContainerItems;
} xtextvaluereadconfig;



/* 格式适配器负责建立自己的错误域、错误码和机器数据。 */
typedef void (*xtextvalueerrorproc)(
	xerrkind Kind,
	xtextvalueerror Code,
	cstr sMessage,
	const xtextvaluelocation* pLocation,
	ptr pUserData
);



/* 事件中的所有视图只在回调期间有效。 */
typedef xtextvaluevisitaction (*xtextvaluevisitproc)(
	const xtextvalueevent* pEvent,
	ptr pUserData
);



/* 解析完整输入并直接发送事件；不构造中间 DOM。 */
xtextvaluevisitresult __xrtTextValueRead(
	xstrview Text,
	const xtextvaluereadconfig* pConfig,
	xtextvaluevisitproc pVisitor,
	ptr pVisitorData,
	xtextvalueerrorproc pError,
	ptr pErrorData,
	bool bDecodeStrings
);

#endif



#if defined(XRT_FEATURE_JSON_WRITE) || defined(XRT_FEATURE_XSON_WRITE)

/* 共同输出标志与 JSON/XSON 公开配置的低四位保持一致。 */
typedef enum xtextvaluewriteflag {
	XTEXT_VALUE_WRITE_PRETTY = UINT32_C(0x00000001),
	XTEXT_VALUE_WRITE_ESCAPE_SLASH = UINT32_C(0x00000002),
	XTEXT_VALUE_WRITE_ESCAPE_HTML = UINT32_C(0x00000004),
	XTEXT_VALUE_WRITE_ESCAPE_NON_ASCII = UINT32_C(0x00000008)
} xtextvaluewriteflag;



/* 状态机明确区分四种文本容器。 */
typedef enum xtextvaluecontainertype {
	XTEXT_VALUE_CONTAINER_ARRAY = 1,
	XTEXT_VALUE_CONTAINER_OBJECT,
	XTEXT_VALUE_CONTAINER_INT_MAP,
	XTEXT_VALUE_CONTAINER_SET
} xtextvaluecontainertype;



/* 内部输出错误由 JSON/XSON 适配器映射到各自错误域。 */
typedef enum xtextvaluewriteerror {
	XTEXT_VALUE_WRITE_ERROR_LIMIT = 0,
	XTEXT_VALUE_WRITE_ERROR_STATE,
	XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
	XTEXT_VALUE_WRITE_ERROR_OUTPUT
} xtextvaluewriteerror;



/* 共享 writer 只保存文本布局和资源预算。 */
typedef struct xtextvaluewriteconfig {
	uint32 Flags;
	uint32 MaxDepth;
	uint32 Indent;
	size_t MaxOutputBytes;
} xtextvaluewriteconfig;



/* 输出回调在返回前消费借用字节。 */
typedef bool (*xtextvaluewriteproc)(xbytesview Data, ptr pUserData);



/* 格式适配器负责建立自己的稳定错误域。 */
typedef void (*xtextvaluewriteerrorproc)(
	xerrkind Kind,
	xtextvaluewriteerror Code,
	cstr sMessage,
	ptr pUserData
);



/* 共享增量写入器不暴露布局。 */
typedef struct xtextvaluewriter xtextvaluewriter;



/* 创建内存或同步 sink 写入器。 */
xtextvaluewriter* __xrtTextValueWriterCreate(
	const xtextvaluewriteconfig* pConfig,
	xtextvaluewriteproc pWrite,
	ptr pWriteData,
	bool bMemory,
	xtextvaluewriteerrorproc pError,
	ptr pErrorData
);



/* 开始指定容器。 */
bool __xrtTextValueWriterBegin(
	xtextvaluewriter* pWriter,
	xtextvaluecontainertype Type
);



/* 结束最近开始的容器。 */
bool __xrtTextValueWriterEnd(xtextvaluewriter* pWriter);



/* 写入对象名称。 */
bool __xrtTextValueWriterName(
	xtextvaluewriter* pWriter,
	xstrview Name
);



/* 写入整数映射键。 */
bool __xrtTextValueWriterKey(
	xtextvaluewriter* pWriter,
	int64 iKey
);



/* 写入基础标量。 */
bool __xrtTextValueWriterNull(xtextvaluewriter* pWriter);
bool __xrtTextValueWriterBool(xtextvaluewriter* pWriter, bool bValue);
bool __xrtTextValueWriterInt(xtextvaluewriter* pWriter, int64 iValue);
bool __xrtTextValueWriterFloat(xtextvaluewriter* pWriter, double fValue);
bool __xrtTextValueWriterString(xtextvaluewriter* pWriter, xstrview Text);



/* 写入单字符串载荷标签。 */
bool __xrtTextValueWriterTag(
	xtextvaluewriter* pWriter,
	xstrview Tag,
	xstrview Payload
);



#if defined(XRT_FEATURE_XSON_WRITE)

/* 以固定小块把任意字节写成规范 Base64 标签。 */
bool __xrtTextValueWriterBase64Tag(
	xtextvaluewriter* pWriter,
	xstrview Tag,
	xbytesview Data
);

#endif



/* 在格式适配器调用用户回调期间禁止 writer 重入。 */
bool __xrtTextValueWriterCallbackEnter(xtextvaluewriter* pWriter);
bool __xrtTextValueWriterCallbackLeave(xtextvaluewriter* pWriter);



/* 建立格式层契约错误并永久关闭当前 writer。 */
bool __xrtTextValueWriterFail(
	xtextvaluewriter* pWriter,
	xerrkind Kind,
	xtextvaluewriteerror Code,
	cstr sMessage
);



/* 标记下层已设置错误的 writer 为失败，不覆盖具体错误。 */
void __xrtTextValueWriterPoison(xtextvaluewriter* pWriter);



/* 完成、移交内存结果和销毁 writer。 */
bool __xrtTextValueWriterFinish(xtextvaluewriter* pWriter);
str __xrtTextValueWriterTake(xtextvaluewriter* pWriter, size_t* pSize);
bool __xrtTextValueWriterFree(xtextvaluewriter* pWriter);

#endif

#endif
