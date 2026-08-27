#include "../internal/xrt_xson.h"

#include <math.h>



#if defined(XRT_FEATURE_XSON_WRITE)

/* XSON writer 保存格式策略、用户 sink 和共享状态机。 */
struct xxsonwriter {
	xxsonwriteconfig Config;
	xxsonwriteproc Write;
	ptr UserData;
	xtextvaluewriter* Core;
};



/* 把共享输出错误映射到 xrt.xson 错误域。 */
static void __xrtXsonWriteError(
	xerrkind Kind,
	xtextvaluewriteerror Code,
	cstr sMessage,
	ptr pUserData
)
{
	xxsonerror XsonCode;

	(void)pUserData;
	if ( Code == XTEXT_VALUE_WRITE_ERROR_LIMIT ) {
		XsonCode = XXSON_ERROR_LIMIT;
	} else if ( Code == XTEXT_VALUE_WRITE_ERROR_STATE ) {
		XsonCode = XXSON_ERROR_STATE;
	} else if ( Code == XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED ) {
		XsonCode = XXSON_ERROR_UNSUPPORTED;
	} else {
		XsonCode = XXSON_ERROR_OUTPUT;
	}
	__xrtXsonError(Kind, XsonCode, "write", sMessage, NULL);
}



/* 用户 sink 桥接器保持错误传播和回调重入检查。 */
static bool __xrtXsonWriteSink(xbytesview Data, ptr pUserData)
{
	xxsonwriter* pWriter = (xxsonwriter*)pUserData;

	return pWriter->Write(Data, pWriter->UserData);
}



/* 验证 XSON 写出配置及全部保留字段。 */
bool __xrtXsonWriteConfigValid(const xxsonwriteconfig* pConfig)
{
	uint32 iKnownFlags =
		XXSON_WRITE_PRETTY |
		XXSON_WRITE_ESCAPE_SLASH |
		XXSON_WRITE_ESCAPE_HTML |
		XXSON_WRITE_ESCAPE_NON_ASCII;

	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		((pConfig->Flags & ~iKnownFlags) != 0) ||
		(pConfig->Unsupported < XXSON_UNSUPPORTED_REJECT) ||
		(pConfig->Unsupported > XXSON_UNSUPPORTED_SKIP) ||
		(pConfig->MaxDepth == 0) ||
		(pConfig->MaxDepth > XRT_VALUE_DEPTH_MAX) ||
		(pConfig->Indent > 16u) ||
		(pConfig->MaxOutputBytes == 0)
	) {
		__xrtXsonError(
			XERR_ARGUMENT,
			XXSON_ERROR_CONFIG,
			"write",
			"invalid XSON write configuration",
			NULL
		);
		return false;
	}
	for ( size_t i = 0; i < 4u; i++ ) {
		if ( pConfig->Reserved[i] != 0 ) {
			__xrtXsonError(
				XERR_ARGUMENT,
				XXSON_ERROR_CONFIG,
				"write",
				"reserved XSON write configuration fields must be zero",
				NULL
			);
			return false;
		}
	}
	return true;
}



/* 把 XSON 布局配置压缩为共享 writer 字段。 */
static xtextvaluewriteconfig __xrtXsonWriteTextConfig(
	const xxsonwriteconfig* pConfig
)
{
	xtextvaluewriteconfig Config;

	Config.Flags = pConfig->Flags;
	Config.MaxDepth = pConfig->MaxDepth;
	Config.Indent = pConfig->Indent;
	Config.MaxOutputBytes = pConfig->MaxOutputBytes;
	return Config;
}



/* 创建内存或 sink 模式 XSON writer。 */
static xxsonwriter* __xrtXsonWriterCreate(
	const xxsonwriteconfig* pConfig,
	xxsonwriteproc pWrite,
	ptr pUserData,
	bool bMemory
)
{
	xtextvaluewriteconfig TextConfig;
	xxsonwriter* pWriter;

	if ( !__xrtXsonWriteConfigValid(pConfig) ) {
		return NULL;
	}
	if ( !bMemory && (pWrite == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pWriter = (xxsonwriter*)xrtCalloc(1, sizeof(xxsonwriter));
	if ( pWriter == NULL ) {
		return NULL;
	}
	pWriter->Config = *pConfig;
	pWriter->Write = pWrite;
	pWriter->UserData = pUserData;
	TextConfig = __xrtXsonWriteTextConfig(pConfig);
	pWriter->Core = __xrtTextValueWriterCreate(
		&TextConfig,
		bMemory ? NULL : __xrtXsonWriteSink,
		pWriter,
		bMemory,
		__xrtXsonWriteError,
		NULL
	);
	if ( pWriter->Core == NULL ) {
		xrtFree(pWriter);
		return NULL;
	}
	return pWriter;
}



/* 判断值是否应按显式策略从父容器中跳过。 */
static bool __xrtXsonWriterSkipValue(
	const xxsonwriter* pWriter,
	const xvalue* pValue
)
{
	xvaluetype Type;

	if (
		(pValue == NULL) ||
		(pWriter->Config.Unsupported != XXSON_UNSUPPORTED_SKIP)
	) {
		return false;
	}
	Type = xrtValueType(pValue);
	return (Type == XVALUE_POINTER) || (Type == XVALUE_HANDLE);
}



/* 判断标签名称是否保留给 XSON 内建类型或容器。 */
static bool __xrtXsonReservedTag(xstrview Tag)
{
	static const cstr arrNames[] = {
		"bytes", "time", "float", "set", "intmap"
	};

	for ( size_t i = 0; i < (sizeof(arrNames) / sizeof(arrNames[0])); i++ ) {
		size_t iSize = strlen(arrNames[i]);

		if (
			(Tag.Size == iSize) &&
			(memcmp(Tag.Data, arrNames[i], iSize) == 0)
		) {
			return true;
		}
	}
	return false;
}



/* 写出非有限浮点的显式 XSON 标签。 */
static bool __xrtXsonWriterFloatValue(
	xxsonwriter* pWriter,
	double fValue
)
{
	if ( isfinite(fValue) ) {
		return __xrtTextValueWriterFloat(pWriter->Core, fValue);
	}
	if ( isnan(fValue) ) {
		return __xrtTextValueWriterTag(
			pWriter->Core,
			XRT_STR_LITERAL("float"),
			XRT_STR_LITERAL("nan")
		);
	}
	return __xrtTextValueWriterTag(
		pWriter->Core,
		XRT_STR_LITERAL("float"),
		signbit(fValue)
			? XRT_STR_LITERAL("-inf")
			: XRT_STR_LITERAL("inf")
	);
}



/* 写出规范 Base64 二进制标签，不建立完整临时文本。 */
static bool __xrtXsonWriterBytesValue(
	xxsonwriter* pWriter,
	xbytesview Data
)
{
	return __xrtTextValueWriterBase64Tag(
		pWriter->Core,
		XRT_STR_LITERAL("bytes"),
		Data
	);
}



/* 把绝对时间规范化为 UTC RFC 3339 标签。 */
static bool __xrtXsonWriterTimeValue(
	xxsonwriter* pWriter,
	xtime Time
)
{
	char sText[64];
	size_t iSize = xrtTimeWriteRFC3339(
		sText,
		sizeof(sText),
		Time,
		0
	);

	if ( iSize == XRT_NPOS ) {
		__xrtTextValueWriterPoison(pWriter->Core);
		return false;
	}
	return __xrtTextValueWriterTag(
		pWriter->Core,
		XRT_STR_LITERAL("time"),
		(xstrview){ sText, iSize }
	);
}



/* 调用自定义编码器并立即消费其借用标签和载荷。 */
static bool __xrtXsonWriterCustomValue(
	xxsonwriter* pWriter,
	const xvalue* pValue
)
{
	const xvalue* arrValues[1] = { pValue };
	xvalue Snapshot;
	const xerror* pPrevious;
	xerror* pHeld;
	xstrview Tag = { NULL, 0 };
	xstrview Payload = { NULL, 0 };
	xxsoncoderesult Result;
	bool bWritten = false;

	if ( pWriter->Config.Encode == NULL ) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_UNSUPPORTED,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"Value type has no XSON representation"
		);
	}
	if ( !__xrtValueCallbackProtect(arrValues, 1u) ) {
		__xrtTextValueWriterPoison(pWriter->Core);
		return false;
	}
	Snapshot = *pValue;
	Snapshot.RefCount = INT32_MAX;
	Snapshot.Flags &= ~XRT_VALUE_FLAG_BUSY;
	Snapshot.Flags |= XRT_VALUE_FLAG_STATIC;
	pPrevious = xrtGetError();
	pHeld = xrtErrorRef(pPrevious);
	if ( !__xrtTextValueWriterCallbackEnter(pWriter->Core) ) {
		xrtErrorFree(pHeld);
		__xrtValueCallbackUnprotect(arrValues, 1u);
		return false;
	}
	Result = pWriter->Config.Encode(
		&Snapshot,
		&Tag,
		&Payload,
		pWriter->Config.EncodeData
	);
	if ( !__xrtTextValueWriterCallbackLeave(pWriter->Core) ) {
		bWritten = false;
	} else if ( Result == XXSON_CODE_OK ) {
		if ( __xrtXsonReservedTag(Tag) ) {
			bWritten = __xrtTextValueWriterFail(
				pWriter->Core,
				XERR_VALUE,
				XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
				"custom encoder returned a reserved XSON tag"
			);
		} else {
			bWritten = __xrtTextValueWriterTag(
				pWriter->Core,
				Tag,
				Payload
			);
		}
	} else if ( Result == XXSON_CODE_ERROR ) {
		if ( xrtGetError() == pPrevious ) {
			bWritten = __xrtTextValueWriterFail(
				pWriter->Core,
				XERR_VALUE,
				XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
				"custom XSON encoder failed"
			);
		} else {
			__xrtTextValueWriterPoison(pWriter->Core);
		}
	} else if ( Result == XXSON_CODE_UNSUPPORTED ) {
		bWritten = __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_UNSUPPORTED,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"custom XSON encoder did not handle Value"
		);
	} else {
		bWritten = __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_STATE,
			XTEXT_VALUE_WRITE_ERROR_STATE,
			"custom XSON encoder returned an invalid result"
		);
	}
	xrtErrorFree(pHeld);
	__xrtValueCallbackUnprotect(arrValues, 1u);
	return bWritten;
}



/* 前置声明递归 XSON 子树写出入口，供容器写出器调用。 */
static bool __xrtXsonWriterTree(
	xxsonwriter* pWriter,
	const xvalue* pValue,
	size_t iDepth,
	ptr* arrActive
);



/* 写出数组或集合，并在取值前跳过显式不支持成员。 */
static bool __xrtXsonWriterSequence(
	xxsonwriter* pWriter,
	const xvalue* pValue,
	xtextvaluecontainertype Container,
	size_t iDepth,
	ptr* arrActive
)
{
	xvalueiter Iterator;
	xvaluekey Key;
	xvalue* pItem;
	bool bResult = __xrtTextValueWriterBegin(pWriter->Core, Container);

	memset(&Iterator, 0, sizeof(Iterator));
	if ( bResult && !xrtValueIterBegin(pValue, &Iterator) ) {
		__xrtTextValueWriterPoison(pWriter->Core);
		return false;
	}
	while ( bResult && ((pItem = xrtValueIterNext(&Iterator, &Key)) != NULL) ) {
		if ( __xrtXsonWriterSkipValue(pWriter, pItem) ) {
			continue;
		}
		bResult = __xrtXsonWriterTree(
			pWriter,
			pItem,
			iDepth + 1u,
			arrActive
		);
	}
	xrtValueIterEnd(&Iterator);
	return bResult && __xrtTextValueWriterEnd(pWriter->Core);
}



/* 写出字符串键对象。 */
static bool __xrtXsonWriterObjectValue(
	xxsonwriter* pWriter,
	const xvalue* pValue,
	size_t iDepth,
	ptr* arrActive
)
{
	xvalueiter Iterator;
	xvaluekey Key;
	xvalue* pItem;
	bool bResult = __xrtTextValueWriterBegin(
		pWriter->Core,
		XTEXT_VALUE_CONTAINER_OBJECT
	);

	memset(&Iterator, 0, sizeof(Iterator));
	if ( bResult && !xrtValueIterBegin(pValue, &Iterator) ) {
		__xrtTextValueWriterPoison(pWriter->Core);
		return false;
	}
	while ( bResult && ((pItem = xrtValueIterNext(&Iterator, &Key)) != NULL) ) {
		if ( __xrtXsonWriterSkipValue(pWriter, pItem) ) {
			continue;
		}
		bResult =
			__xrtTextValueWriterName(pWriter->Core, Key.String) &&
			__xrtXsonWriterTree(
				pWriter,
				pItem,
				iDepth + 1u,
				arrActive
			);
	}
	xrtValueIterEnd(&Iterator);
	return bResult && __xrtTextValueWriterEnd(pWriter->Core);
}



/* 写出 int64 键映射。 */
static bool __xrtXsonWriterIntMapValue(
	xxsonwriter* pWriter,
	const xvalue* pValue,
	size_t iDepth,
	ptr* arrActive
)
{
	xvalueiter Iterator;
	xvaluekey Key;
	xvalue* pItem;
	bool bResult = __xrtTextValueWriterBegin(
		pWriter->Core,
		XTEXT_VALUE_CONTAINER_INT_MAP
	);

	memset(&Iterator, 0, sizeof(Iterator));
	if ( bResult && !xrtValueIterBegin(pValue, &Iterator) ) {
		__xrtTextValueWriterPoison(pWriter->Core);
		return false;
	}
	while ( bResult && ((pItem = xrtValueIterNext(&Iterator, &Key)) != NULL) ) {
		if ( __xrtXsonWriterSkipValue(pWriter, pItem) ) {
			continue;
		}
		bResult =
			__xrtTextValueWriterKey(pWriter->Core, Key.Integer) &&
			__xrtXsonWriterTree(
				pWriter,
				pItem,
				iDepth + 1u,
				arrActive
			);
	}
	xrtValueIterEnd(&Iterator);
	return bResult && __xrtTextValueWriterEnd(pWriter->Core);
}



/* 写出完整 XSON Value 子树，并检测活动容器 backing 环。 */
static bool __xrtXsonWriterTree(
	xxsonwriter* pWriter,
	const xvalue* pValue,
	size_t iDepth,
	ptr* arrActive
)
{
	xvaluetype Type;
	ptr pIdentity;
	bool bValue;
	int64 iInteger;
	uint64 iUnsigned;
	double fValue;
	xstrview Text;
	xbytesview Data;
	xtime Time;

	if ( pValue == NULL ) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_ARGUMENT,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"cannot write a null Value pointer as XSON"
		);
	}
	Type = xrtValueType(pValue);
	if ( Type == XVALUE_NULL ) {
		return __xrtTextValueWriterNull(pWriter->Core);
	}
	if ( Type == XVALUE_BOOL ) {
		if ( !xrtValueGetBool(pValue, &bValue) ) {
			__xrtTextValueWriterPoison(pWriter->Core);
			return false;
		}
		return __xrtTextValueWriterBool(pWriter->Core, bValue);
	}
	if ( Type == XVALUE_INT ) {
		if ( !xrtValueGetInt(pValue, &iInteger) ) {
			__xrtTextValueWriterPoison(pWriter->Core);
			return false;
		}
		return __xrtTextValueWriterInt(pWriter->Core, iInteger);
	}
	if ( Type == XVALUE_UINT ) {
		if ( !xrtValueGetUInt(pValue, &iUnsigned) ) {
			__xrtTextValueWriterPoison(pWriter->Core);
			return false;
		}
		return __xrtTextValueWriterUInt(pWriter->Core, iUnsigned);
	}
	if ( Type == XVALUE_FLOAT ) {
		if ( !xrtValueGetFloat(pValue, &fValue) ) {
			__xrtTextValueWriterPoison(pWriter->Core);
			return false;
		}
		return __xrtXsonWriterFloatValue(pWriter, fValue);
	}
	if ( Type == XVALUE_STRING ) {
		if ( !xrtValueGetString(pValue, &Text) ) {
			__xrtTextValueWriterPoison(pWriter->Core);
			return false;
		}
		return __xrtTextValueWriterString(pWriter->Core, Text);
	}
	if ( Type == XVALUE_BYTES ) {
		if ( !xrtValueGetBytes(pValue, &Data) ) {
			__xrtTextValueWriterPoison(pWriter->Core);
			return false;
		}
		return __xrtXsonWriterBytesValue(pWriter, Data);
	}
	if ( Type == XVALUE_TIME ) {
		if ( !xrtValueGetTime(pValue, &Time) ) {
			__xrtTextValueWriterPoison(pWriter->Core);
			return false;
		}
		return __xrtXsonWriterTimeValue(pWriter, Time);
	}
	if ( (Type == XVALUE_POINTER) || (Type == XVALUE_HANDLE) ) {
		return __xrtXsonWriterCustomValue(pWriter, pValue);
	}
	if (
		(Type != XVALUE_ARRAY) && (Type != XVALUE_INT_MAP) &&
		(Type != XVALUE_SET) && (Type != XVALUE_OBJECT)
	) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_UNSUPPORTED,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"Value type has no XSON representation"
		);
	}
	if ( iDepth >= pWriter->Config.MaxDepth ) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_RANGE,
			XTEXT_VALUE_WRITE_ERROR_LIMIT,
			"XSON Value nesting exceeds configured depth"
		);
	}
	pIdentity = (ptr)pValue->Data.Backing;
	for ( size_t i = 0; i < iDepth; i++ ) {
		if ( arrActive[i] == pIdentity ) {
			return __xrtTextValueWriterFail(
				pWriter->Core,
				XERR_VALUE,
				XTEXT_VALUE_WRITE_ERROR_STATE,
				"cyclic Value graph cannot be written as XSON"
			);
		}
	}
	arrActive[iDepth] = pIdentity;
	if ( Type == XVALUE_ARRAY ) {
		return __xrtXsonWriterSequence(
			pWriter,
			pValue,
			XTEXT_VALUE_CONTAINER_ARRAY,
			iDepth,
			arrActive
		);
	}
	if ( Type == XVALUE_SET ) {
		return __xrtXsonWriterSequence(
			pWriter,
			pValue,
			XTEXT_VALUE_CONTAINER_SET,
			iDepth,
			arrActive
		);
	}
	if ( Type == XVALUE_OBJECT ) {
		return __xrtXsonWriterObjectValue(
			pWriter,
			pValue,
			iDepth,
			arrActive
		);
	}
	return __xrtXsonWriterIntMapValue(
		pWriter,
		pValue,
		iDepth,
		arrActive
	);
}



/* 初始化紧凑输出、严格类型和有限输出预算。 */
XRT_API void xrtXsonWriteConfigInit(xxsonwriteconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Unsupported = XXSON_UNSUPPORTED_REJECT;
	pConfig->MaxDepth = XXSON_DEPTH_DEFAULT;
	pConfig->Indent = 2u;
	pConfig->MaxOutputBytes = XXSON_INPUT_DEFAULT;
}



/* 创建内存增量 writer。 */
XRT_API xxsonwriter* xrtXsonWriterCreate(
	const xxsonwriteconfig* pConfig
)
{
	return __xrtXsonWriterCreate(pConfig, NULL, NULL, true);
}



/* 创建同步 sink 增量 writer。 */
XRT_API xxsonwriter* xrtXsonWriterCreateSink(
	const xxsonwriteconfig* pConfig,
	xxsonwriteproc pWrite,
	ptr pUserData
)
{
	return __xrtXsonWriterCreate(pConfig, pWrite, pUserData, false);
}



/* 在当前位置开始对象。 */
XRT_API bool xrtXsonWriterObject(xxsonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterBegin(
		pWriter->Core,
		XTEXT_VALUE_CONTAINER_OBJECT
	);
}



/* 在当前位置开始数组。 */
XRT_API bool xrtXsonWriterArray(xxsonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterBegin(
		pWriter->Core,
		XTEXT_VALUE_CONTAINER_ARRAY
	);
}



/* 在当前位置开始整数映射。 */
XRT_API bool xrtXsonWriterIntMap(xxsonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterBegin(
		pWriter->Core,
		XTEXT_VALUE_CONTAINER_INT_MAP
	);
}



/* 在当前位置开始集合。 */
XRT_API bool xrtXsonWriterSet(xxsonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterBegin(
		pWriter->Core,
		XTEXT_VALUE_CONTAINER_SET
	);
}



/* 结束当前容器。 */
XRT_API bool xrtXsonWriterEnd(xxsonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterEnd(pWriter->Core);
}



/* 写入对象名称。 */
XRT_API bool xrtXsonWriterName(xxsonwriter* pWriter, xstrview Name)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterName(pWriter->Core, Name);
}



/* 写入整数映射键。 */
XRT_API bool xrtXsonWriterKey(xxsonwriter* pWriter, int64 iKey)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterKey(pWriter->Core, iKey);
}



/* 写入 null。 */
XRT_API bool xrtXsonWriterNull(xxsonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterNull(pWriter->Core);
}



/* 写入布尔值。 */
XRT_API bool xrtXsonWriterBool(xxsonwriter* pWriter, bool bValue)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterBool(pWriter->Core, bValue);
}



/* 写入 int64。 */
XRT_API bool xrtXsonWriterInt(xxsonwriter* pWriter, int64 iValue)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterInt(pWriter->Core, iValue);
}



/* 写入 uint64。 */
XRT_API bool xrtXsonWriterUInt(xxsonwriter* pWriter, uint64 iValue)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterUInt(pWriter->Core, iValue);
}



/* 写入 double，非有限值使用显式标签。 */
XRT_API bool xrtXsonWriterFloat(xxsonwriter* pWriter, double fValue)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtXsonWriterFloatValue(pWriter, fValue);
}



/* 写入严格 UTF-8 字符串。 */
XRT_API bool xrtXsonWriterString(xxsonwriter* pWriter, xstrview Text)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterString(pWriter->Core, Text);
}



/* 写入规范 Base64 二进制标签。 */
XRT_API bool xrtXsonWriterBytes(xxsonwriter* pWriter, xbytesview Data)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtXsonWriterBytesValue(pWriter, Data);
}



/* 写入 UTC RFC 3339 时间标签。 */
XRT_API bool xrtXsonWriterTime(xxsonwriter* pWriter, xtime Time)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtXsonWriterTimeValue(pWriter, Time);
}



/* 写入非保留自定义标签。 */
XRT_API bool xrtXsonWriterTag(
	xxsonwriter* pWriter,
	xstrview Tag,
	xstrview Payload
)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtXsonReservedTag(Tag) ) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_ARGUMENT,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"custom XSON tag uses a reserved name"
		);
	}
	return __xrtTextValueWriterTag(pWriter->Core, Tag, Payload);
}



/* 写入完整 Value 子树。 */
XRT_API bool xrtXsonWriterValue(
	xxsonwriter* pWriter,
	const xvalue* pValue
)
{
	ptr arrActive[XRT_VALUE_DEPTH_MAX];
	bool bResult;

	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtXsonWriterSkipValue(pWriter, pValue) ) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_UNSUPPORTED,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"root or direct XSON value cannot be skipped"
		);
	}
	bResult = __xrtXsonWriterTree(pWriter, pValue, 0, arrActive);
	if ( !bResult ) {
		__xrtTextValueWriterPoison(pWriter->Core);
	}
	return bResult;
}



/* 完成 writer。 */
XRT_API bool xrtXsonWriterFinish(xxsonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterFinish(pWriter->Core);
}



/* 从已完成的内存 writer 移交文本。 */
XRT_API str xrtXsonWriterTake(xxsonwriter* pWriter, size_t* pSize)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtTextValueWriterTake(pWriter->Core, pSize);
}



/* 销毁 writer；回调重入时保持对象有效。 */
XRT_API void xrtXsonWriterFree(xxsonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		return;
	}
	if ( __xrtTextValueWriterFree(pWriter->Core) ) {
		xrtFree(pWriter);
	}
}



/* 使用高级配置把 Value 写入同步 sink。 */
XRT_API bool xrtXsonWrite(
	const xvalue* pValue,
	const xxsonwriteconfig* pConfig,
	xxsonwriteproc pWrite,
	ptr pUserData
)
{
	xxsonwriter* pWriter;
	bool bResult;

	pWriter = xrtXsonWriterCreateSink(pConfig, pWrite, pUserData);
	if ( pWriter == NULL ) {
		return false;
	}
	bResult =
		xrtXsonWriterValue(pWriter, pValue) &&
		xrtXsonWriterFinish(pWriter);
	xrtXsonWriterFree(pWriter);
	return bResult;
}



/* 紧凑或美化地序列化 Value 到新文本。 */
XRT_API str xrtXsonStringify(
	const xvalue* pValue,
	bool bPretty,
	size_t* pSize
)
{
	xxsonwriteconfig Config;
	xxsonwriter* pWriter;
	str sText;

	xrtXsonWriteConfigInit(&Config);
	if ( bPretty ) {
		Config.Flags |= XXSON_WRITE_PRETTY;
	}
	pWriter = xrtXsonWriterCreate(&Config);
	if ( pWriter == NULL ) {
		return NULL;
	}
	if (
		!xrtXsonWriterValue(pWriter, pValue) ||
		!xrtXsonWriterFinish(pWriter)
	) {
		xrtXsonWriterFree(pWriter);
		return NULL;
	}
	sText = xrtXsonWriterTake(pWriter, pSize);
	xrtXsonWriterFree(pWriter);
	return sText;
}

#endif
