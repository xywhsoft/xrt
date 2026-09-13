#ifndef XRT_INTERNAL_TEXT_LINES_H
#define XRT_INTERNAL_TEXT_LINES_H

#include "xrt_text_value.h"

#if defined(XRT_FEATURE_JSONL_CORE) || defined(XRT_FEATURE_XSONL_CORE)

/* 错误偏移在两个公开格式枚举中保持一致。 */
typedef enum xtextlineserror {
	XTEXT_LINES_CONFIG = 0,
	XTEXT_LINES_SYNTAX,
	XTEXT_LINES_LIMIT,
	XTEXT_LINES_RECORD,
	XTEXT_LINES_TYPE,
	XTEXT_LINES_OUTPUT,
	XTEXT_LINES_IO,
	XTEXT_LINES_STATE
} xtextlineserror;

typedef struct xtextlinesformat {
	cstr Domain;
	cstr RecordDomain;
	int32 ErrorBase;
} xtextlinesformat;

typedef struct xtextlineslocation {
	size_t Offset;
	size_t Line;
	size_t Column;
	size_t RecordIndex;
} xtextlineslocation;

/* bCause 为 true 时接管当前错误作为原因链；包装 OOM 时保留原始原因。 */
void __xrtTextLinesError(
	const xtextlinesformat* pFormat, xtextlineserror Code, xerrkind Kind,
	cstr sOperation, cstr sMessage, const xtextlineslocation* pLocation, bool bCause
);
bool __xrtTextLinesErrorLocation(
	const xerror* pError, const xtextlinesformat* pFormat, xtextlineslocation* pLocation
);
#endif

#if defined(XRT_FEATURE_JSONL_READ) || defined(XRT_FEATURE_XSONL_READ)

/* 验证路径成功返回 null 单例；读取路径返回拥有的单条值。 */
typedef xvalue* (*xtextlinesreadproc)(
	xstrview Text, const void* pConfig, xtextvaluebudget* pBudget, bool bValidate
);
typedef struct xtextlinesreadconfig {
	bool RejectEmpty;
	size_t MaxInputBytes;
	size_t MaxLineBytes;
	size_t MaxRecords;
	size_t MaxTotalValues;
	size_t MaxTotalDecodedBytes;
	const void* Record;
	xtextlinesreadproc Read;
} xtextlinesreadconfig;

xvalue* __xrtTextLinesRead(
	xstrview Text, const xtextlinesreadconfig* pConfig,
	const xtextlinesformat* pFormat, bool bValidate
);
#endif

#if defined(XRT_FEATURE_JSONL_WRITE) || defined(XRT_FEATURE_XSONL_WRITE)
typedef bool (*xtextlineswriteproc)(xbytesview Data, ptr pUserData);
typedef bool (*xtextlinesrecordwriteproc)(
	const xvalue* pValue, const void* pConfig, xtextlineswriteproc pWrite, ptr pUserData
);
typedef struct xtextlineswriteconfig {
	size_t MaxOutputBytes;
	size_t MaxRecords;
	const void* Record;
	xtextlinesrecordwriteproc Write;
} xtextlineswriteconfig;

bool __xrtTextLinesWrite(
	const xvalue* pArray, const xtextlineswriteconfig* pConfig,
	const xtextlinesformat* pFormat, xtextlineswriteproc pWrite, ptr pUserData
);
str __xrtTextLinesStringify(
	const xvalue* pArray, const xtextlineswriteconfig* pConfig,
	const xtextlinesformat* pFormat, size_t* pSize
);
#endif

#endif

