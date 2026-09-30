#include "../internal/xrt_xlon.h"

#include <math.h>



#if defined(XRT_FEATURE_XLON_WRITE)

/* XLON writer 保存格式策略、用户 sink 和共享状态机。 */
struct xxlonwriter {
	xxlonwriteconfig Config;
	xxlonwriteproc Write;
	ptr UserData;
	xtextvaluewriter* Core;
};



/* 把共享输出错误映射到 xrt.xlon 错误域。 */
static void __xrtXlonWriteError(
	xerrkind Kind,
	xtextvaluewriteerror Code,
	cstr sMessage,
	ptr pUserData
)
{
	xxlonerror XlonCode;

	(void)pUserData;
	if ( Code == XTEXT_VALUE_WRITE_ERROR_LIMIT ) {
		XlonCode = XXLON_ERROR_LIMIT;
	} else if ( Code == XTEXT_VALUE_WRITE_ERROR_STATE ) {
		XlonCode = XXLON_ERROR_STATE;
	} else if ( Code == XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED ) {
		XlonCode = XXLON_ERROR_UNSUPPORTED;
	} else {
		XlonCode = XXLON_ERROR_OUTPUT;
	}
	__xrtXlonError(Kind, XlonCode, "write", sMessage, NULL);
}



/* 用户 sink 桥接器保持错误传播和回调重入检查。 */
static bool __xrtXlonWriteSink(xbytesview Data, ptr pUserData)
{
	xxlonwriter* pWriter = (xxlonwriter*)pUserData;

	return pWriter->Write(Data, pWriter->UserData);
}



/* 验证 XLON 写出配置及全部保留字段。 */
bool __xrtXlonWriteConfigValid(const xxlonwriteconfig* pConfig)
{
	uint32 iKnownFlags =
		XXLON_WRITE_PRETTY |
		XXLON_WRITE_ESCAPE_SLASH |
		XXLON_WRITE_ESCAPE_HTML |
		XXLON_WRITE_ESCAPE_NON_ASCII;

	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		((pConfig->Flags & ~iKnownFlags) != 0) ||
		(pConfig->Unsupported < XXLON_UNSUPPORTED_REJECT) ||
		(pConfig->Unsupported > XXLON_UNSUPPORTED_SKIP) ||
		(pConfig->MaxDepth == 0) ||
		(pConfig->MaxDepth > XRT_VALUE_DEPTH_MAX) ||
		(pConfig->Indent > 16u) ||
		(pConfig->MaxOutputBytes == 0)
	) {
		__xrtXlonError(
			XERR_ARGUMENT,
			XXLON_ERROR_CONFIG,
			"write",
			"invalid XLON write configuration",
			NULL
		);
		return false;
	}
	for ( size_t i = 0; i < 4u; i++ ) {
		if ( pConfig->Reserved[i] != 0 ) {
			__xrtXlonError(
				XERR_ARGUMENT,
				XXLON_ERROR_CONFIG,
				"write",
				"reserved XLON write configuration fields must be zero",
				NULL
			);
			return false;
		}
	}
	return true;
}



/* 把 XLON 布局配置压缩为共享 writer 字段。 */
static xtextvaluewriteconfig __xrtXlonWriteTextConfig(
	const xxlonwriteconfig* pConfig
)
{
	xtextvaluewriteconfig Config;

	Config.Flags = pConfig->Flags;
	Config.MaxDepth = pConfig->MaxDepth;
	Config.Indent = pConfig->Indent;
	Config.MaxOutputBytes = pConfig->MaxOutputBytes;
	return Config;
}



/* 创建内存或 sink 模式 XLON writer。 */
static xxlonwriter* __xrtXlonWriterCreate(
	const xxlonwriteconfig* pConfig,
	xxlonwriteproc pWrite,
	ptr pUserData,
	bool bMemory
)
{
	xtextvaluewriteconfig TextConfig;
	xxlonwriter* pWriter;

	if ( !__xrtXlonWriteConfigValid(pConfig) ) {
		return NULL;
	}
	if ( !bMemory && (pWrite == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pWriter = (xxlonwriter*)xrtCalloc(1, sizeof(xxlonwriter));
	if ( pWriter == NULL ) {
		return NULL;
	}
	pWriter->Config = *pConfig;
	pWriter->Write = pWrite;
	pWriter->UserData = pUserData;
	TextConfig = __xrtXlonWriteTextConfig(pConfig);
	pWriter->Core = __xrtTextValueWriterCreate(
		&TextConfig,
		bMemory ? NULL : __xrtXlonWriteSink,
		pWriter,
		bMemory,
		__xrtXlonWriteError,
		NULL
	);
	if ( pWriter->Core == NULL ) {
		xrtFree(pWriter);
		return NULL;
	}
	return pWriter;
}



/* 判断值是否应按显式策略从父容器中跳过。 */
static bool __xrtXlonWriterSkipValue(
	const xxlonwriter* pWriter,
	const xvalue* pValue
)
{
	xvaluetype Type;

	if (
		(pValue == NULL) ||
		(pWriter->Config.Unsupported != XXLON_UNSUPPORTED_SKIP)
	) {
		return false;
	}
	Type = xrtValueType(pValue);
	return (Type == XVALUE_POINTER) || (Type == XVALUE_HANDLE);
}



/* 判断标签名称是否保留给 XLON 内建类型或容器。 */
static bool __xrtXlonReservedTag(xstrview Tag)
{
	static const cstr arrNames[] = {
		"bytes", "char", "time", "float", "set", "intmap"
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



/* 写出非有限浮点的显式 XLON 标签。 */
static bool __xrtXlonWriterFloatValue(
	xxlonwriter* pWriter,
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
static bool __xrtXlonWriterBytesValue(
	xxlonwriter* pWriter,
	xbytesview Data
)
{
	return __xrtTextValueWriterBase64Tag(
		pWriter->Core,
		XRT_STR_LITERAL("bytes"),
		Data
	);
}



/* 写出只包含一个 Unicode 标量的显式字符标签。 */
static bool __xrtXlonWriterCharValue(
	xxlonwriter* pWriter,
	uint32 iValue
)
{
	char arrText[4];
	size_t iSize = xrtUtf8Encode(iValue, arrText);

	if ( iSize == 0 ) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_VALUE,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"character is not a Unicode scalar"
		);
	}
	return __xrtTextValueWriterTag(
		pWriter->Core,
		XRT_STR_LITERAL("char"),
		(xstrview){ arrText, iSize }
	);
}



/* 把绝对时间规范化为 UTC RFC 3339 标签。 */
static bool __xrtXlonWriterTimeValue(
	xxlonwriter* pWriter,
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
static bool __xrtXlonWriterCustomValue(
	xxlonwriter* pWriter,
	const xvalue* pValue
)
{
	const xvalue* arrValues[1] = { pValue };
	xvalue Snapshot;
	const xerror* pPrevious;
	xerror* pHeld;
	xstrview Tag = { NULL, 0 };
	xstrview Payload = { NULL, 0 };
	xxloncoderesult Result;
	bool bWritten = false;

	if ( pWriter->Config.Encode == NULL ) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_UNSUPPORTED,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"Value type has no XLON representation"
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
	} else if ( Result == XXLON_CODE_OK ) {
		if ( __xrtXlonReservedTag(Tag) ) {
			bWritten = __xrtTextValueWriterFail(
				pWriter->Core,
				XERR_VALUE,
				XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
				"custom encoder returned a reserved XLON tag"
			);
		} else {
			bWritten = __xrtTextValueWriterTag(
				pWriter->Core,
				Tag,
				Payload
			);
		}
	} else if ( Result == XXLON_CODE_ERROR ) {
		if ( xrtGetError() == pPrevious ) {
			bWritten = __xrtTextValueWriterFail(
				pWriter->Core,
				XERR_VALUE,
				XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
				"custom XLON encoder failed"
			);
		} else {
			__xrtTextValueWriterPoison(pWriter->Core);
		}
	} else if ( Result == XXLON_CODE_UNSUPPORTED ) {
		bWritten = __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_UNSUPPORTED,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"custom XLON encoder did not handle Value"
		);
	} else {
		bWritten = __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_STATE,
			XTEXT_VALUE_WRITE_ERROR_STATE,
			"custom XLON encoder returned an invalid result"
		);
	}
	xrtErrorFree(pHeld);
	__xrtValueCallbackUnprotect(arrValues, 1u);
	return bWritten;
}



/* 前置声明递归 XLON 子树写出入口，供容器写出器调用。 */
static bool __xrtXlonWriterTree(
	xxlonwriter* pWriter,
	const xvalue* pValue,
	size_t iDepth,
	ptr* arrActive
);



/* 写出数组或集合，并在取值前跳过显式不支持成员。 */
static bool __xrtXlonWriterSequence(
	xxlonwriter* pWriter,
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
		if ( __xrtXlonWriterSkipValue(pWriter, pItem) ) {
			continue;
		}
		bResult = __xrtXlonWriterTree(
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
static bool __xrtXlonWriterObjectValue(
	xxlonwriter* pWriter,
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
		if ( __xrtXlonWriterSkipValue(pWriter, pItem) ) {
			continue;
		}
		bResult =
			__xrtTextValueWriterName(pWriter->Core, Key.String) &&
			__xrtXlonWriterTree(
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
static bool __xrtXlonWriterIntMapValue(
	xxlonwriter* pWriter,
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
		if ( __xrtXlonWriterSkipValue(pWriter, pItem) ) {
			continue;
		}
		bResult =
			__xrtTextValueWriterKey(pWriter->Core, Key.Integer) &&
			__xrtXlonWriterTree(
				pWriter,
				pItem,
				iDepth + 1u,
				arrActive
			);
	}
	xrtValueIterEnd(&Iterator);
	return bResult && __xrtTextValueWriterEnd(pWriter->Core);
}



/* 写出完整 XLON Value 子树，并检测活动容器 backing 环。 */
static bool __xrtXlonWriterTree(
	xxlonwriter* pWriter,
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
			"cannot write a null Value pointer as XLON"
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
	if ( Type == XVALUE_CHAR ) {
		uint32 iCharacter;

		if ( !xrtValueGetChar(pValue, &iCharacter) ) {
			__xrtTextValueWriterPoison(pWriter->Core);
			return false;
		}
		return __xrtXlonWriterCharValue(pWriter, iCharacter);
	}
	if ( Type == XVALUE_FLOAT ) {
		if ( !xrtValueGetFloat(pValue, &fValue) ) {
			__xrtTextValueWriterPoison(pWriter->Core);
			return false;
		}
		return __xrtXlonWriterFloatValue(pWriter, fValue);
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
		return __xrtXlonWriterBytesValue(pWriter, Data);
	}
	if ( Type == XVALUE_TIME ) {
		if ( !xrtValueGetTime(pValue, &Time) ) {
			__xrtTextValueWriterPoison(pWriter->Core);
			return false;
		}
		return __xrtXlonWriterTimeValue(pWriter, Time);
	}
	if ( (Type == XVALUE_POINTER) || (Type == XVALUE_HANDLE) ) {
		return __xrtXlonWriterCustomValue(pWriter, pValue);
	}
	if (
		(Type != XVALUE_ARRAY) && (Type != XVALUE_INT_MAP) &&
		(Type != XVALUE_SET) && (Type != XVALUE_OBJECT)
	) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_UNSUPPORTED,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"Value type has no XLON representation"
		);
	}
	if ( iDepth >= pWriter->Config.MaxDepth ) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_RANGE,
			XTEXT_VALUE_WRITE_ERROR_LIMIT,
			"XLON Value nesting exceeds configured depth"
		);
	}
	pIdentity = (ptr)pValue->Data.Backing;
	for ( size_t i = 0; i < iDepth; i++ ) {
		if ( arrActive[i] == pIdentity ) {
			return __xrtTextValueWriterFail(
				pWriter->Core,
				XERR_VALUE,
				XTEXT_VALUE_WRITE_ERROR_STATE,
				"cyclic Value graph cannot be written as XLON"
			);
		}
	}
	arrActive[iDepth] = pIdentity;
	if ( Type == XVALUE_ARRAY ) {
		return __xrtXlonWriterSequence(
			pWriter,
			pValue,
			XTEXT_VALUE_CONTAINER_ARRAY,
			iDepth,
			arrActive
		);
	}
	if ( Type == XVALUE_SET ) {
		return __xrtXlonWriterSequence(
			pWriter,
			pValue,
			XTEXT_VALUE_CONTAINER_SET,
			iDepth,
			arrActive
		);
	}
	if ( Type == XVALUE_OBJECT ) {
		return __xrtXlonWriterObjectValue(
			pWriter,
			pValue,
			iDepth,
			arrActive
		);
	}
	return __xrtXlonWriterIntMapValue(
		pWriter,
		pValue,
		iDepth,
		arrActive
	);
}



/* 初始化紧凑输出、严格类型和有限输出预算。 */
XRT_API void xrtXlonWriteConfigInit(xxlonwriteconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Unsupported = XXLON_UNSUPPORTED_REJECT;
	pConfig->MaxDepth = XXLON_DEPTH_DEFAULT;
	pConfig->Indent = 2u;
	pConfig->MaxOutputBytes = XXLON_INPUT_DEFAULT;
}



/* 创建内存增量 writer。 */
XRT_API xxlonwriter* xrtXlonWriterCreate(
	const xxlonwriteconfig* pConfig
)
{
	return __xrtXlonWriterCreate(pConfig, NULL, NULL, true);
}



/* 创建同步 sink 增量 writer。 */
XRT_API xxlonwriter* xrtXlonWriterCreateSink(
	const xxlonwriteconfig* pConfig,
	xxlonwriteproc pWrite,
	ptr pUserData
)
{
	return __xrtXlonWriterCreate(pConfig, pWrite, pUserData, false);
}



/* 在当前位置开始对象。 */
XRT_API bool xrtXlonWriterObject(xxlonwriter* pWriter)
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
XRT_API bool xrtXlonWriterArray(xxlonwriter* pWriter)
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
XRT_API bool xrtXlonWriterIntMap(xxlonwriter* pWriter)
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
XRT_API bool xrtXlonWriterSet(xxlonwriter* pWriter)
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
XRT_API bool xrtXlonWriterEnd(xxlonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterEnd(pWriter->Core);
}



/* 写入对象名称。 */
XRT_API bool xrtXlonWriterName(xxlonwriter* pWriter, xstrview Name)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterName(pWriter->Core, Name);
}



/* 写入整数映射键。 */
XRT_API bool xrtXlonWriterKey(xxlonwriter* pWriter, int64 iKey)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterKey(pWriter->Core, iKey);
}



/* 写入 null。 */
XRT_API bool xrtXlonWriterNull(xxlonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterNull(pWriter->Core);
}



/* 写入布尔值。 */
XRT_API bool xrtXlonWriterBool(xxlonwriter* pWriter, bool bValue)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterBool(pWriter->Core, bValue);
}



/* 写入 int64。 */
XRT_API bool xrtXlonWriterInt(xxlonwriter* pWriter, int64 iValue)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterInt(pWriter->Core, iValue);
}



/* 写入 uint64。 */
XRT_API bool xrtXlonWriterUInt(xxlonwriter* pWriter, uint64 iValue)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterUInt(pWriter->Core, iValue);
}



/* 写入保留字符身份的 Unicode 标量。 */
XRT_API bool xrtXlonWriterChar(xxlonwriter* pWriter, uint32 iValue)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtXlonWriterCharValue(pWriter, iValue);
}



/* 写入 double，非有限值使用显式标签。 */
XRT_API bool xrtXlonWriterFloat(xxlonwriter* pWriter, double fValue)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtXlonWriterFloatValue(pWriter, fValue);
}



/* 写入严格 UTF-8 字符串。 */
XRT_API bool xrtXlonWriterString(xxlonwriter* pWriter, xstrview Text)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterString(pWriter->Core, Text);
}



/* 写入规范 Base64 二进制标签。 */
XRT_API bool xrtXlonWriterBytes(xxlonwriter* pWriter, xbytesview Data)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtXlonWriterBytesValue(pWriter, Data);
}



/* 写入 UTC RFC 3339 时间标签。 */
XRT_API bool xrtXlonWriterTime(xxlonwriter* pWriter, xtime Time)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtXlonWriterTimeValue(pWriter, Time);
}



/* 写入非保留自定义标签。 */
XRT_API bool xrtXlonWriterTag(
	xxlonwriter* pWriter,
	xstrview Tag,
	xstrview Payload
)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtXlonReservedTag(Tag) ) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_ARGUMENT,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"custom XLON tag uses a reserved name"
		);
	}
	return __xrtTextValueWriterTag(pWriter->Core, Tag, Payload);
}



/* 写入完整 Value 子树。 */
XRT_API bool xrtXlonWriterValue(
	xxlonwriter* pWriter,
	const xvalue* pValue
)
{
	ptr arrActive[XRT_VALUE_DEPTH_MAX];
	bool bResult;

	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtXlonWriterSkipValue(pWriter, pValue) ) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_UNSUPPORTED,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"root or direct XLON value cannot be skipped"
		);
	}
	bResult = __xrtXlonWriterTree(pWriter, pValue, 0, arrActive);
	if ( !bResult ) {
		__xrtTextValueWriterPoison(pWriter->Core);
	}
	return bResult;
}



/* 完成 writer。 */
XRT_API bool xrtXlonWriterFinish(xxlonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTextValueWriterFinish(pWriter->Core);
}



/* 从已完成的内存 writer 移交文本。 */
XRT_API str xrtXlonWriterTake(xxlonwriter* pWriter, size_t* pSize)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtTextValueWriterTake(pWriter->Core, pSize);
}



/* 销毁 writer；回调重入时保持对象有效。 */
XRT_API void xrtXlonWriterFree(xxlonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		return;
	}
	if ( __xrtTextValueWriterFree(pWriter->Core) ) {
		xrtFree(pWriter);
	}
}



/* 使用高级配置把 Value 写入同步 sink。 */
XRT_API bool xrtXlonWrite(
	const xvalue* pValue,
	const xxlonwriteconfig* pConfig,
	xxlonwriteproc pWrite,
	ptr pUserData
)
{
	xxlonwriter* pWriter;
	bool bResult;

	pWriter = xrtXlonWriterCreateSink(pConfig, pWrite, pUserData);
	if ( pWriter == NULL ) {
		return false;
	}
	bResult =
		xrtXlonWriterValue(pWriter, pValue) &&
		xrtXlonWriterFinish(pWriter);
	xrtXlonWriterFree(pWriter);
	return bResult;
}



/* 紧凑或美化地序列化 Value 到新文本。 */
XRT_API str xrtXlonStringify(
	const xvalue* pValue,
	bool bPretty,
	size_t* pSize
)
{
	xxlonwriteconfig Config;
	xxlonwriter* pWriter;
	str sText;

	xrtXlonWriteConfigInit(&Config);
	if ( bPretty ) {
		Config.Flags |= XXLON_WRITE_PRETTY;
	}
	pWriter = xrtXlonWriterCreate(&Config);
	if ( pWriter == NULL ) {
		return NULL;
	}
	if (
		!xrtXlonWriterValue(pWriter, pValue) ||
		!xrtXlonWriterFinish(pWriter)
	) {
		xrtXlonWriterFree(pWriter);
		return NULL;
	}
	sText = xrtXlonWriterTake(pWriter, pSize);
	xrtXlonWriterFree(pWriter);
	return sText;
}

#endif
