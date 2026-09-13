#ifndef XRT_XSONL_H
#define XRT_XSONL_H

#include <xrt/xson.h>

#if defined(XRT_FEATURE_XSONL) && (!defined(XRT_FEATURE_XSONL_FILE))
	#error "XRT_FEATURE_XSONL requires XSONL_FILE"
#endif

#if defined(XRT_FEATURE_XSONL_READ) && (!defined(XRT_FEATURE_XSONL_CORE))
	#error "XRT_FEATURE_XSONL_READ requires XSONL_CORE"
#endif

#if defined(XRT_FEATURE_XSONL_WRITE) && (!defined(XRT_FEATURE_XSONL_CORE))
	#error "XRT_FEATURE_XSONL_WRITE requires XSONL_CORE"
#endif

#if defined(XRT_FEATURE_XSONL_FILE) && (!defined(XRT_FEATURE_XSONL_READ) || !defined(XRT_FEATURE_XSONL_WRITE))
	#error "XRT_FEATURE_XSONL_FILE requires XSONL_READ and XSONL_WRITE"
#endif

#if defined(XRT_FEATURE_XSONL_READ) && !defined(XRT_FEATURE_XSON_READ)
	#error "XSONL read requires XSON read"
#endif
#if defined(XRT_FEATURE_XSONL_WRITE) && !defined(XRT_FEATURE_XSON_WRITE)
	#error "XSONL write requires XSON write"
#endif
#if defined(XRT_FEATURE_XSONL_FILE) && !defined(XRT_FEATURE_FILE_WHOLE)
	#error "XSONL file requires FILE_WHOLE"
#endif

#if defined(XRT_FEATURE_XSONL_CORE)

/* XSONL 的稳定错误域为 xrt.xsonl；RECORD 保留单条编解码错误原因链。 */
typedef enum xxsonlerror {
	XXSONL_ERROR_CONFIG = 1801,
	XXSONL_ERROR_SYNTAX,
	XXSONL_ERROR_LIMIT,
	XXSONL_ERROR_RECORD,
	XXSONL_ERROR_TYPE,
	XXSONL_ERROR_OUTPUT,
	XXSONL_ERROR_IO,
	XXSONL_ERROR_STATE
} xxsonlerror;

/* 列按 UTF-8 字节计数；空白行计入 Line，不计入 RecordIndex。 */
typedef struct xxsonllocation {
	size_t Offset;
	size_t Line;
	size_t Column;
	size_t RecordIndex;
} xxsonllocation;

XRT_EXTERN_C_BEGIN

/* 读取全局字节偏移、一基物理行列及零基记录下标；无位置时保持输出不变。 */
XRT_API bool xrtXsonlErrorLocation(
	const xerror* pError,
	xxsonllocation* pLocation
);

XRT_EXTERN_C_END
#endif

#if defined(XRT_FEATURE_XSONL_READ)

/* 默认跳过空白行；此标志使空白行报告语法错误。 */
typedef enum xxsonlreadflag {
	XXSONL_READ_REJECT_EMPTY_LINES = UINT32_C(0x00000001)
} xxsonlreadflag;

/* Record 限制单条；外层限制累计消耗，合成的汇总 Array 不计入值数或深度。 */
typedef struct xxsonlreadconfig {
	xxsonreadconfig Record;
	uint32 Flags;
	size_t MaxInputBytes;
	size_t MaxRecords;
	size_t MaxTotalValues;
	size_t MaxTotalDecodedBytes;
	uint32 Reserved[4];
} xxsonlreadconfig;

XRT_EXTERN_C_BEGIN

/* 初始化默认忽略空白行、严格单条语法和有限累计预算。 */
XRT_API void xrtXsonlReadConfigInit(
	xxsonlreadconfig* pConfig
);


/* 使用默认配置解析记录序列；成功返回拥有的 Array，空输入返回空 Array。 */
XRT_API xvalue* xrtXsonlParse(
	xstrview Text
);


/* 按配置解析全部记录；失败释放部分结果并返回 NULL，结果由 xrtValueRelease 释放。 */
XRT_API xvalue* xrtXsonlRead(
	xstrview Text,
	const xxsonlreadconfig* pConfig
);


/* 默认忽略空白行，验证逐行语法和累计预算，不构造 Value DOM；重复键策略不参与验证。 */
XRT_API bool xrtXsonlValid(
	xstrview Text
);

XRT_EXTERN_C_END
#endif

#if defined(XRT_FEATURE_XSONL_WRITE)

/* Record.MaxOutputBytes 不含分隔符；外层 MaxOutputBytes 包含每条 LF，不含末尾 NUL。 */
typedef struct xxsonlwriteconfig {
	xxsonwriteconfig Record;
	size_t MaxOutputBytes;
	size_t MaxRecords;
	uint32 Reserved[4];
} xxsonlwriteconfig;

XRT_EXTERN_C_BEGIN

/* 初始化紧凑单行输出、LF 分隔及有限累计预算；PRETTY 配置非法。 */
XRT_API void xrtXsonlWriteConfigInit(
	xxsonlwriteconfig* pConfig
);


/* 每个 Array 元素写成一行；返回 xrtFree 释放的 NUL 结尾文本，失败不修改可空的 pSize。 */
XRT_API str xrtXsonlStringify(
	const xvalue* pArray,
	size_t* pSize
);


/* 同步分块输出各条记录及 LF；回调借用字节仅在调用期间有效，失败不能撤回已提交字节。 */
XRT_API bool xrtXsonlWrite(
	const xvalue* pArray,
	const xxsonlwriteconfig* pConfig,
	xxsonwriteproc pWrite,
	ptr pUserData
);

XRT_EXTERN_C_END
#endif

#if defined(XRT_FEATURE_XSONL_FILE)
XRT_EXTERN_C_BEGIN

/* 按默认配置限额读取文件并返回拥有的 Array。 */
XRT_API xvalue* xrtXsonlParseFile(
	cstr sPath
);


/* 按整体输入上限读取文件，逐行解析；失败不返回部分 Array。 */
XRT_API xvalue* xrtXsonlReadFile(
	cstr sPath,
	const xxsonlreadconfig* pConfig
);


/* 按默认配置完整序列化 Array 后原子替换文件。 */
XRT_API bool xrtXsonlStringifyFile(
	cstr sPath,
	const xvalue* pArray
);


/* 按高级配置完整序列化后原子替换文件；序列化失败保留原文件。 */
XRT_API bool xrtXsonlWriteFile(
	cstr sPath,
	const xvalue* pArray,
	const xxsonlwriteconfig* pConfig
);

XRT_EXTERN_C_END
#endif

#endif

