#include "../internal/xrt_json.h"
#include "../internal/xrt_text_value.h"

#include <math.h>



#if defined(XRT_FEATURE_JSON_WRITE)

/* JSON writer 只保存格式策略、用户 sink 和共享状态机。 */
struct xjsonwriter {
	xjsonwriteconfig Config;
	xjsonwriteproc Write;
	ptr UserData;
	xtextvaluewriter* Core;
};



/* 把共享输出错误映射到 xrt.json 错误域。 */
static void __xrtJsonWriteError(
	xerrkind Kind,
	xtextvaluewriteerror Code,
	cstr sMessage,
	ptr pUserData
)
{
	xjsonerror JsonCode;

	(void)pUserData;
	if ( Code == XTEXT_VALUE_WRITE_ERROR_LIMIT ) {
		JsonCode = XJSON_ERROR_LIMIT;
	} else if ( Code == XTEXT_VALUE_WRITE_ERROR_STATE ) {
		JsonCode = XJSON_ERROR_STATE;
	} else if ( Code == XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED ) {
		JsonCode = XJSON_ERROR_UNSUPPORTED;
	} else {
		JsonCode = XJSON_ERROR_OUTPUT;
	}
	__xrtJsonError(Kind, JsonCode, "write", sMessage, NULL);
}



/* 用户 sink 桥接器不改变错误所有权和回调时序。 */
static bool __xrtJsonWriteSink(xbytesview Data, ptr pUserData)
{
	xjsonwriter* pWriter = (xjsonwriter*)pUserData;

	return pWriter->Write(Data, pWriter->UserData);
}



/* 验证 JSON 写出配置及全部保留字段。 */
static bool __xrtJsonWriteConfigValid(const xjsonwriteconfig* pConfig)
{
	uint32 iKnownFlags =
		XJSON_WRITE_PRETTY |
		XJSON_WRITE_ESCAPE_SLASH |
		XJSON_WRITE_ESCAPE_HTML |
		XJSON_WRITE_ESCAPE_NON_ASCII |
		XJSON_WRITE_CONTAINER_COMPAT;

	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		((pConfig->Flags & ~iKnownFlags) != 0) ||
		(pConfig->NonFinite < XJSON_NONFINITE_REJECT) ||
		(pConfig->NonFinite > XJSON_NONFINITE_STRING) ||
		(pConfig->Unsupported < XJSON_UNSUPPORTED_REJECT) ||
		(pConfig->Unsupported > XJSON_UNSUPPORTED_SKIP) ||
		(pConfig->MaxDepth == 0) ||
		(pConfig->MaxDepth > XRT_VALUE_DEPTH_MAX) ||
		(pConfig->Indent > 16u) ||
		(pConfig->MaxOutputBytes == 0)
	) {
		__xrtJsonError(
			XERR_ARGUMENT,
			XJSON_ERROR_CONFIG,
			"write",
			"invalid JSON write configuration",
			NULL
		);
		return false;
	}
	for ( size_t i = 0; i < 4u; i++ ) {
		if ( pConfig->Reserved[i] != 0 ) {
			__xrtJsonError(
				XERR_ARGUMENT,
				XJSON_ERROR_CONFIG,
				"write",
				"reserved JSON write configuration fields must be zero",
				NULL
			);
			return false;
		}
	}
	return true;
}



/* 把 JSON 布局配置压缩为共享 writer 字段。 */
static xtextvaluewriteconfig __xrtJsonWriteTextConfig(
	const xjsonwriteconfig* pConfig
)
{
	xtextvaluewriteconfig Config;

	Config.Flags = pConfig->Flags & (
		XJSON_WRITE_PRETTY |
		XJSON_WRITE_ESCAPE_SLASH |
		XJSON_WRITE_ESCAPE_HTML |
		XJSON_WRITE_ESCAPE_NON_ASCII
	);
	Config.MaxDepth = pConfig->MaxDepth;
	Config.Indent = pConfig->Indent;
	Config.MaxOutputBytes = pConfig->MaxOutputBytes;
	return Config;
}



/* 创建内存或 sink 模式 JSON writer。 */
static xjsonwriter* __xrtJsonWriterCreate(
	const xjsonwriteconfig* pConfig,
	xjsonwriteproc pWrite,
	ptr pUserData,
	bool bMemory
)
{
	xtextvaluewriteconfig TextConfig;
	xjsonwriter* pWriter;

	if ( !__xrtJsonWriteConfigValid(pConfig) ) {
		return NULL;
	}
	if ( !bMemory && (pWrite == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pWriter = (xjsonwriter*)xrtCalloc(1, sizeof(xjsonwriter));
	if ( pWriter == NULL ) {
		return NULL;
	}
	pWriter->Config = *pConfig;
	pWriter->Write = pWrite;
	pWriter->UserData = pUserData;
	TextConfig = __xrtJsonWriteTextConfig(pConfig);
	pWriter->Core = __xrtTextValueWriterCreate(
		&TextConfig,
		bMemory ? NULL : __xrtJsonWriteSink,
		pWriter,
		bMemory,
		__xrtJsonWriteError,
		NULL
	);
	if ( pWriter->Core == NULL ) {
		xrtFree(pWriter);
		return NULL;
	}
	return pWriter;
}



/* 判断值是否应按显式策略从父容器中跳过。 */
static bool __xrtJsonWriterSkipValue(
	const xjsonwriter* pWriter,
	const xvalue* pValue
)
{
	xvaluetype Type;

	if (
		(pValue == NULL) ||
		(pWriter->Config.Unsupported != XJSON_UNSUPPORTED_SKIP)
	) {
		return false;
	}
	Type = xrtValueType(pValue);
	if (
		(Type == XVALUE_NULL) || (Type == XVALUE_BOOL) ||
		(Type == XVALUE_INT) || (Type == XVALUE_UINT) ||
		(Type == XVALUE_FLOAT) ||
		(Type == XVALUE_STRING) || (Type == XVALUE_ARRAY) ||
		(Type == XVALUE_OBJECT)
	) {
		return false;
	}
	if (
		((Type == XVALUE_INT_MAP) || (Type == XVALUE_SET)) &&
		((pWriter->Config.Flags & XJSON_WRITE_CONTAINER_COMPAT) != 0)
	) {
		return false;
	}
	return true;
}



/* 前置声明递归 JSON 子树写出入口，供容器写出器调用。 */
static bool __xrtJsonWriterTree(
	xjsonwriter* pWriter,
	const xvalue* pValue,
	size_t iDepth,
	ptr* arrActive
);



/* 写出数组或兼容映射后的集合。 */
static bool __xrtJsonWriterSequence(
	xjsonwriter* pWriter,
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
		XTEXT_VALUE_CONTAINER_ARRAY
	);

	memset(&Iterator, 0, sizeof(Iterator));
	if ( bResult && !xrtValueIterBegin(pValue, &Iterator) ) {
		__xrtTextValueWriterPoison(pWriter->Core);
		return false;
	}
	while ( bResult && ((pItem = xrtValueIterNext(&Iterator, &Key)) != NULL) ) {
		if ( __xrtJsonWriterSkipValue(pWriter, pItem) ) {
			continue;
		}
		bResult = __xrtJsonWriterTree(
			pWriter,
			pItem,
			iDepth + 1u,
			arrActive
		);
	}
	xrtValueIterEnd(&Iterator);
	return bResult && __xrtTextValueWriterEnd(pWriter->Core);
}



/* 写出对象并在写名称前决定是否跳过成员。 */
static bool __xrtJsonWriterObjectValue(
	xjsonwriter* pWriter,
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
		if ( __xrtJsonWriterSkipValue(pWriter, pItem) ) {
			continue;
		}
		bResult =
			__xrtTextValueWriterName(pWriter->Core, Key.String) &&
			__xrtJsonWriterTree(
				pWriter,
				pItem,
				iDepth + 1u,
				arrActive
			);
	}
	xrtValueIterEnd(&Iterator);
	return bResult && __xrtTextValueWriterEnd(pWriter->Core);
}



/* 把整数映射键格式化为 JSON 对象名称。 */
static bool __xrtJsonWriterIntMapValue(
	xjsonwriter* pWriter,
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
		char sKey[32];
		size_t iKeySize;

		if ( __xrtJsonWriterSkipValue(pWriter, pItem) ) {
			continue;
		}
		if ( !xrtIntWrite(
			Key.Integer,
			10u,
			sKey,
			sizeof(sKey),
			&iKeySize,
			0
		) ) {
			__xrtTextValueWriterPoison(pWriter->Core);
			bResult = false;
			break;
		}
		bResult =
			__xrtTextValueWriterName(
				pWriter->Core,
				(xstrview){ sKey, iKeySize }
			) &&
			__xrtJsonWriterTree(
				pWriter,
				pItem,
				iDepth + 1u,
				arrActive
			);
	}
	xrtValueIterEnd(&Iterator);
	return bResult && __xrtTextValueWriterEnd(pWriter->Core);
}



/* 按 JSON 策略写出 double，包括显式非有限映射。 */
static bool __xrtJsonWriterFloatValue(
	xjsonwriter* pWriter,
	double fValue
)
{
	if ( isfinite(fValue) ) {
		return __xrtTextValueWriterFloat(pWriter->Core, fValue);
	}
	if ( pWriter->Config.NonFinite == XJSON_NONFINITE_NULL ) {
		return __xrtTextValueWriterNull(pWriter->Core);
	}
	if ( pWriter->Config.NonFinite == XJSON_NONFINITE_STRING ) {
		if ( isnan(fValue) ) {
			return __xrtTextValueWriterString(
				pWriter->Core,
				XRT_STR_LITERAL("NaN")
			);
		}
		return __xrtTextValueWriterString(
			pWriter->Core,
			signbit(fValue)
				? XRT_STR_LITERAL("-Infinity")
				: XRT_STR_LITERAL("Infinity")
		);
	}
	return __xrtTextValueWriterFail(
		pWriter->Core,
		XERR_VALUE,
		XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
		"non-finite floating-point value is not JSON"
	);
}



/* 写出 Value 子树，并检测活动容器 backing 环。 */
static bool __xrtJsonWriterTree(
	xjsonwriter* pWriter,
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

	if ( pValue == NULL ) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_ARGUMENT,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"cannot write a null Value pointer as JSON"
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
		return __xrtJsonWriterFloatValue(pWriter, fValue);
	}
	if ( Type == XVALUE_STRING ) {
		if ( !xrtValueGetString(pValue, &Text) ) {
			__xrtTextValueWriterPoison(pWriter->Core);
			return false;
		}
		return __xrtTextValueWriterString(pWriter->Core, Text);
	}
	if (
		(Type != XVALUE_ARRAY) && (Type != XVALUE_OBJECT) &&
		(Type != XVALUE_INT_MAP) && (Type != XVALUE_SET)
	) {
		if ( pWriter->Config.Unsupported == XJSON_UNSUPPORTED_NULL ) {
			return __xrtTextValueWriterNull(pWriter->Core);
		}
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_UNSUPPORTED,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"Value type has no JSON representation"
		);
	}
	if (
		((Type == XVALUE_INT_MAP) || (Type == XVALUE_SET)) &&
		((pWriter->Config.Flags & XJSON_WRITE_CONTAINER_COMPAT) == 0)
	) {
		if ( pWriter->Config.Unsupported == XJSON_UNSUPPORTED_NULL ) {
			return __xrtTextValueWriterNull(pWriter->Core);
		}
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_UNSUPPORTED,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"extended Value container requires JSON compatibility mapping"
		);
	}
	if ( iDepth >= pWriter->Config.MaxDepth ) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_RANGE,
			XTEXT_VALUE_WRITE_ERROR_LIMIT,
			"JSON Value nesting exceeds configured depth"
		);
	}
	pIdentity = (ptr)pValue->Data.Backing;
	for ( size_t i = 0; i < iDepth; i++ ) {
		if ( arrActive[i] == pIdentity ) {
			return __xrtTextValueWriterFail(
				pWriter->Core,
				XERR_VALUE,
				XTEXT_VALUE_WRITE_ERROR_STATE,
				"cyclic Value graph cannot be written as JSON"
			);
		}
	}
	arrActive[iDepth] = pIdentity;
	if ( (Type == XVALUE_ARRAY) || (Type == XVALUE_SET) ) {
		return __xrtJsonWriterSequence(
			pWriter,
			pValue,
			iDepth,
			arrActive
		);
	}
	if ( Type == XVALUE_OBJECT ) {
		return __xrtJsonWriterObjectValue(
			pWriter,
			pValue,
			iDepth,
			arrActive
		);
	}
	return __xrtJsonWriterIntMapValue(
		pWriter,
		pValue,
		iDepth,
		arrActive
	);
}



/* 初始化紧凑输出、严格类型和有限输出预算。 */
XRT_API void xrtJsonWriteConfigInit(xjsonwriteconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->NonFinite = XJSON_NONFINITE_REJECT;
	pConfig->Unsupported = XJSON_UNSUPPORTED_REJECT;
	pConfig->MaxDepth = XJSON_DEPTH_DEFAULT;
	pConfig->Indent = 2u;
	pConfig->MaxOutputBytes = XJSON_INPUT_DEFAULT;
}



/* 创建内存增量 writer。 */
XRT_API xjsonwriter* xrtJsonWriterCreate(
	const xjsonwriteconfig* pConfig
)
{
	return __xrtJsonWriterCreate(pConfig, NULL, NULL, true);
}



/* 创建同步 sink 增量 writer。 */
XRT_API xjsonwriter* xrtJsonWriterCreateSink(
	const xjsonwriteconfig* pConfig,
	xjsonwriteproc pWrite,
	ptr pUserData
)
{
	return __xrtJsonWriterCreate(pConfig, pWrite, pUserData, false);
}



/* 验证公开 writer 外壳存在，并建立统一参数错误。 */
static bool __xrtJsonWriterValid(const xjsonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 在当前位置开始对象。 */
XRT_API bool xrtJsonWriterObject(xjsonwriter* pWriter)
{
	if ( !__xrtJsonWriterValid(pWriter) ) {
		return false;
	}
	return __xrtTextValueWriterBegin(
		pWriter->Core,
		XTEXT_VALUE_CONTAINER_OBJECT
	);
}



/* 在当前位置开始数组。 */
XRT_API bool xrtJsonWriterArray(xjsonwriter* pWriter)
{
	if ( !__xrtJsonWriterValid(pWriter) ) {
		return false;
	}
	return __xrtTextValueWriterBegin(
		pWriter->Core,
		XTEXT_VALUE_CONTAINER_ARRAY
	);
}



/* 结束当前对象或数组。 */
XRT_API bool xrtJsonWriterEnd(xjsonwriter* pWriter)
{
	if ( !__xrtJsonWriterValid(pWriter) ) {
		return false;
	}
	return __xrtTextValueWriterEnd(pWriter->Core);
}



/* 写入对象名称。 */
XRT_API bool xrtJsonWriterName(xjsonwriter* pWriter, xstrview Name)
{
	if ( !__xrtJsonWriterValid(pWriter) ) {
		return false;
	}
	return __xrtTextValueWriterName(pWriter->Core, Name);
}



/* 写入 null。 */
XRT_API bool xrtJsonWriterNull(xjsonwriter* pWriter)
{
	if ( !__xrtJsonWriterValid(pWriter) ) {
		return false;
	}
	return __xrtTextValueWriterNull(pWriter->Core);
}



/* 写入布尔值。 */
XRT_API bool xrtJsonWriterBool(xjsonwriter* pWriter, bool bValue)
{
	if ( !__xrtJsonWriterValid(pWriter) ) {
		return false;
	}
	return __xrtTextValueWriterBool(pWriter->Core, bValue);
}



/* 写入 int64。 */
XRT_API bool xrtJsonWriterInt(xjsonwriter* pWriter, int64 iValue)
{
	if ( !__xrtJsonWriterValid(pWriter) ) {
		return false;
	}
	return __xrtTextValueWriterInt(pWriter->Core, iValue);
}



/* 写入 uint64。 */
XRT_API bool xrtJsonWriterUInt(xjsonwriter* pWriter, uint64 iValue)
{
	if ( !__xrtJsonWriterValid(pWriter) ) {
		return false;
	}
	return __xrtTextValueWriterUInt(pWriter->Core, iValue);
}



/* 按配置写入 double。 */
XRT_API bool xrtJsonWriterFloat(xjsonwriter* pWriter, double fValue)
{
	if ( !__xrtJsonWriterValid(pWriter) ) {
		return false;
	}
	return __xrtJsonWriterFloatValue(pWriter, fValue);
}



/* 写入严格 UTF-8 字符串。 */
XRT_API bool xrtJsonWriterString(xjsonwriter* pWriter, xstrview Text)
{
	if ( !__xrtJsonWriterValid(pWriter) ) {
		return false;
	}
	return __xrtTextValueWriterString(pWriter->Core, Text);
}



/* 写入完整 Value 子树。 */
XRT_API bool xrtJsonWriterValue(
	xjsonwriter* pWriter,
	const xvalue* pValue
)
{
	ptr arrActive[XRT_VALUE_DEPTH_MAX];
	bool bResult;

	if ( !__xrtJsonWriterValid(pWriter) ) {
		return false;
	}
	if ( __xrtJsonWriterSkipValue(pWriter, pValue) ) {
		return __xrtTextValueWriterFail(
			pWriter->Core,
			XERR_UNSUPPORTED,
			XTEXT_VALUE_WRITE_ERROR_UNSUPPORTED,
			"root or direct JSON value cannot be skipped"
		);
	}
	bResult = __xrtJsonWriterTree(pWriter, pValue, 0, arrActive);
	if ( !bResult ) {
		__xrtTextValueWriterPoison(pWriter->Core);
	}
	return bResult;
}



/* 完成 writer。 */
XRT_API bool xrtJsonWriterFinish(xjsonwriter* pWriter)
{
	if ( !__xrtJsonWriterValid(pWriter) ) {
		return false;
	}
	return __xrtTextValueWriterFinish(pWriter->Core);
}



/* 从已完成的内存 writer 移交文本。 */
XRT_API str xrtJsonWriterTake(xjsonwriter* pWriter, size_t* pSize)
{
	if ( !__xrtJsonWriterValid(pWriter) ) {
		return NULL;
	}
	return __xrtTextValueWriterTake(pWriter->Core, pSize);
}



/* 销毁 writer；回调重入时保持对象有效。 */
XRT_API void xrtJsonWriterFree(xjsonwriter* pWriter)
{
	if ( pWriter == NULL ) {
		return;
	}
	if ( __xrtTextValueWriterFree(pWriter->Core) ) {
		xrtFree(pWriter);
	}
}



/* 使用高级配置把 Value 写入同步 sink。 */
XRT_API bool xrtJsonWrite(
	const xvalue* pValue,
	const xjsonwriteconfig* pConfig,
	xjsonwriteproc pWrite,
	ptr pUserData
)
{
	xjsonwriter* pWriter;
	bool bResult;

	pWriter = xrtJsonWriterCreateSink(pConfig, pWrite, pUserData);
	if ( pWriter == NULL ) {
		return false;
	}
	bResult =
		xrtJsonWriterValue(pWriter, pValue) &&
		xrtJsonWriterFinish(pWriter);
	xrtJsonWriterFree(pWriter);
	return bResult;
}



/* 紧凑或美化地序列化 Value 到新文本。 */
XRT_API str xrtJsonStringify(
	const xvalue* pValue,
	bool bPretty,
	size_t* pSize
)
{
	xjsonwriteconfig Config;
	xjsonwriter* pWriter;
	str sText;

	xrtJsonWriteConfigInit(&Config);
	if ( bPretty ) {
		Config.Flags |= XJSON_WRITE_PRETTY;
	}
	pWriter = xrtJsonWriterCreate(&Config);
	if ( pWriter == NULL ) {
		return NULL;
	}
	if (
		!xrtJsonWriterValue(pWriter, pValue) ||
		!xrtJsonWriterFinish(pWriter)
	) {
		xrtJsonWriterFree(pWriter);
		return NULL;
	}
	sText = xrtJsonWriterTake(pWriter, pSize);
	xrtJsonWriterFree(pWriter);
	return sText;
}

#endif
