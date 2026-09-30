#include "../internal/xrt_xlon.h"

#include <math.h>



#if defined(XRT_FEATURE_XLON_READ)

/* DOM 栈帧只拥有因 KEEP 策略未挂入父容器的临时子树。 */
typedef struct xlondomframe {
	xvalue* Value;
	bool Owned;
} xlondomframe;



/* DOM 构建器保存重复策略、自定义解码器和复用的二进制缓冲。 */
typedef struct xlondombuilder {
	xxlonreadconfig Config;
	xtextvaluebudget* Budget;
	bool Validate;
	xvalue* Root;
	xlondomframe Frames[XRT_VALUE_DEPTH_MAX];
	xbuffer Bytes;
} xlondombuilder;



/* 公开访问器适配器复用一个二进制缓冲，事件返回后即可覆盖。 */
typedef struct xlonvisitadapter {
	const xxlonreadconfig* Config;
	xxlonvisitproc Visitor;
	ptr UserData;
	xbuffer Bytes;
} xlonvisitadapter;



/* 把内部文本位置复制为 XLON 稳定位置。 */
static xxlonlocation __xrtXlonLocation(
	const xtextvaluelocation* pLocation
)
{
	xxlonlocation Location;

	Location.Offset = pLocation->Offset;
	Location.Line = pLocation->Line;
	Location.Column = pLocation->Column;
	return Location;
}



/* 在事件位置设置 XLON 读取错误。 */
static void __xrtXlonEventError(
	const xtextvalueevent* pEvent,
	xerrkind Kind,
	xxlonerror Code,
	cstr sMessage
)
{
	xxlonlocation Location = __xrtXlonLocation(&pEvent->Location);

	__xrtXlonError(Kind, Code, "read", sMessage, &Location);
}



/* 把共享读取错误映射到 xrt.xlon 错误域。 */
static void __xrtXlonReadError(
	xerrkind Kind,
	xtextvalueerror Code,
	cstr sMessage,
	const xtextvaluelocation* pLocation,
	ptr pUserData
)
{
	xxlonlocation Location;
	xxlonerror XlonCode;

	(void)pUserData;
	if ( Code == XTEXT_VALUE_ERROR_LIMIT ) {
		XlonCode = XXLON_ERROR_LIMIT;
	} else if ( Code == XTEXT_VALUE_ERROR_NUMBER ) {
		XlonCode = XXLON_ERROR_NUMBER;
	} else if ( Code == XTEXT_VALUE_ERROR_STATE ) {
		XlonCode = XXLON_ERROR_STATE;
	} else {
		XlonCode = XXLON_ERROR_SYNTAX;
	}
	if ( pLocation == NULL ) {
		__xrtXlonError(Kind, XlonCode, "read", sMessage, NULL);
		return;
	}
	Location = __xrtXlonLocation(pLocation);
	__xrtXlonError(Kind, XlonCode, "read", sMessage, &Location);
}



/* 判断标签名称是否等于固定 ASCII 文本。 */
static bool __xrtXlonTagEqual(
	xstrview Tag,
	cstr sName,
	size_t iSize
)
{
	return
		(Tag.Size == iSize) &&
		(memcmp(Tag.Data, sName, iSize) == 0);
}



/* 严格解码规范 Base64，并把结果保存在调用方复用缓冲中。 */
static bool __xrtXlonDecodeBytes(
	const xtextvalueevent* pSource,
	const xxlonreadconfig* pConfig,
	xtextvaluebudget* pBudget,
	xbuffer* pBuffer,
	xbytesview* pBytes
)
{
	size_t iSize;

	if ( !xrtBase64Decode(
		pSource->Value.Tag.Payload.Data,
		pSource->Value.Tag.Payload.Size,
		NULL,
		0,
		&iSize,
		NULL
	) ) {
		xrtClearError();
		__xrtXlonEventError(
			pSource,
			XERR_VALUE,
			XXLON_ERROR_TAG,
			"bytes tag contains invalid Base64"
		);
		return false;
	}
	if ( (iSize > pConfig->MaxDecodedBytes) ||
		 ((pBudget != NULL) && (iSize > pBudget->DecodedBytes)) ) {
		__xrtXlonEventError(
			pSource,
			XERR_RANGE,
			XXLON_ERROR_LIMIT,
			"decoded bytes exceed configured limit"
		);
		return false;
	}
	if ( pBudget != NULL ) {
		pBudget->DecodedBytes -= iSize;
	}
	if ( !xrtBufferResize(pBuffer, iSize) ) {
		return false;
	}
	if (
		!xrtBase64Decode(
			pSource->Value.Tag.Payload.Data,
			pSource->Value.Tag.Payload.Size,
			pBuffer->Data,
			pBuffer->Size,
			&iSize,
			NULL
		)
	) {
		xrtClearError();
		__xrtXlonEventError(
			pSource,
			XERR_VALUE,
			XXLON_ERROR_TAG,
			"bytes tag could not be decoded"
		);
		return false;
	}
	pBytes->Data = pBuffer->Data;
	pBytes->Size = iSize;
	return true;
}



/* 把共享事件转换为完整 XLON 事件，并解释所有内建标签。 */
static bool __xrtXlonMakeEvent(
	const xtextvalueevent* pSource,
	const xxlonreadconfig* pConfig,
	xtextvaluebudget* pBudget,
	xbuffer* pBytes,
	xxlonevent* pEvent
)
{
	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->Location = __xrtXlonLocation(&pSource->Location);
	pEvent->Depth = pSource->Depth;
	pEvent->Key = pSource->Key;
	pEvent->Raw = pSource->Raw;

	if ( pSource->Type == XTEXT_VALUE_EVENT_NULL ) {
		pEvent->Type = XXLON_EVENT_NULL;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_BOOL ) {
		pEvent->Type = XXLON_EVENT_BOOL;
		pEvent->Value.Boolean = pSource->Value.Boolean;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_INT ) {
		pEvent->Type = XXLON_EVENT_INT;
		pEvent->Value.Integer = pSource->Value.Integer;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_UINT ) {
		pEvent->Type = XXLON_EVENT_UINT;
		pEvent->Value.Unsigned = pSource->Value.Unsigned;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_FLOAT ) {
		pEvent->Type = XXLON_EVENT_FLOAT;
		pEvent->Value.Float = pSource->Value.Float;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_STRING ) {
		pEvent->Type = XXLON_EVENT_STRING;
		pEvent->Value.String = pSource->Value.String;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_ARRAY_BEGIN ) {
		pEvent->Type = XXLON_EVENT_ARRAY_BEGIN;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_ARRAY_END ) {
		pEvent->Type = XXLON_EVENT_ARRAY_END;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_INT_MAP_BEGIN ) {
		pEvent->Type = XXLON_EVENT_INT_MAP_BEGIN;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_INT_MAP_END ) {
		pEvent->Type = XXLON_EVENT_INT_MAP_END;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_SET_BEGIN ) {
		pEvent->Type = XXLON_EVENT_SET_BEGIN;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_SET_END ) {
		pEvent->Type = XXLON_EVENT_SET_END;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_OBJECT_BEGIN ) {
		pEvent->Type = XXLON_EVENT_OBJECT_BEGIN;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_OBJECT_END ) {
		pEvent->Type = XXLON_EVENT_OBJECT_END;
	} else if ( pSource->Type != XTEXT_VALUE_EVENT_TAG ) {
		__xrtXlonEventError(
			pSource,
			XERR_STATE,
			XXLON_ERROR_STATE,
			"unknown internal XLON event"
		);
		return false;
	} else if ( __xrtXlonTagEqual(pSource->Value.Tag.Name, "bytes", 5u) ) {
		pEvent->Type = XXLON_EVENT_BYTES;
		if ( !__xrtXlonDecodeBytes(
			pSource,
			pConfig,
			pBudget,
			pBytes,
			&pEvent->Value.Bytes
		) ) {
			return false;
		}
	} else if ( __xrtXlonTagEqual(pSource->Value.Tag.Name, "char", 4u) ) {
		size_t iRead = 0;

		pEvent->Type = XXLON_EVENT_CHAR;
		if ( (xrtUtf8Decode(
			pSource->Value.Tag.Payload,
			&pEvent->Value.Character,
			&iRead
		) != XUTF_OK) || (iRead != pSource->Value.Tag.Payload.Size) ) {
			__xrtXlonEventError(
				pSource,
				XERR_VALUE,
				XXLON_ERROR_TAG,
				"char tag requires exactly one Unicode scalar"
			);
			return false;
		}
	} else if ( __xrtXlonTagEqual(pSource->Value.Tag.Name, "time", 4u) ) {
		pEvent->Type = XXLON_EVENT_TIME;
		if ( !xrtTimeParseRFC3339(
			pSource->Value.Tag.Payload,
			&pEvent->Value.Time
		) ) {
			xrtClearError();
			__xrtXlonEventError(
				pSource,
				XERR_VALUE,
				XXLON_ERROR_TAG,
				"time tag requires strict RFC 3339 text"
			);
			return false;
		}
	} else if ( __xrtXlonTagEqual(pSource->Value.Tag.Name, "float", 5u) ) {
		pEvent->Type = XXLON_EVENT_FLOAT;
		if ( __xrtXlonTagEqual(pSource->Value.Tag.Payload, "nan", 3u) ) {
			pEvent->Value.Float = NAN;
		} else if ( __xrtXlonTagEqual(pSource->Value.Tag.Payload, "inf", 3u) ) {
			pEvent->Value.Float = INFINITY;
		} else if ( __xrtXlonTagEqual(pSource->Value.Tag.Payload, "-inf", 4u) ) {
			pEvent->Value.Float = -INFINITY;
		} else {
			__xrtXlonEventError(
				pSource,
				XERR_VALUE,
				XXLON_ERROR_TAG,
				"float tag payload must be nan, inf or -inf"
			);
			return false;
		}
	} else {
		if ( (pConfig->Flags & XXLON_READ_CUSTOM) == 0 ) {
			__xrtXlonEventError(
				pSource,
				XERR_UNSUPPORTED,
				XXLON_ERROR_UNSUPPORTED,
				"custom XLON tag is disabled"
			);
			return false;
		}
		pEvent->Type = XXLON_EVENT_CUSTOM;
		pEvent->Value.Tag.Name = pSource->Value.Tag.Name;
		pEvent->Value.Tag.Payload = pSource->Value.Tag.Payload;
	}
	return true;
}



/* 验证读取配置和全部保留字段。 */
bool __xrtXlonReadConfigValid(const xxlonreadconfig* pConfig)
{
	uint32 iKnownFlags =
		XXLON_READ_COMMENTS |
		XXLON_READ_TRAILING_COMMA |
		XXLON_READ_CUSTOM;

	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		((pConfig->Flags & ~iKnownFlags) != 0) ||
		(pConfig->Duplicate < XXLON_DUPLICATE_REJECT) ||
		(pConfig->Duplicate > XXLON_DUPLICATE_REPLACE) ||
		(pConfig->BigInteger < XXLON_BIGINT_REJECT) ||
		(pConfig->BigInteger > XXLON_BIGINT_FLOAT) ||
		(pConfig->MaxDepth == 0) ||
		(pConfig->MaxDepth > XRT_VALUE_DEPTH_MAX) ||
		(pConfig->MaxInputBytes == 0) ||
		(pConfig->MaxStringBytes == 0) ||
		(pConfig->MaxValues == 0) ||
		(pConfig->MaxContainerItems == 0) ||
		(pConfig->MaxDecodedBytes == 0)
	) {
		__xrtXlonError(
			XERR_ARGUMENT,
			XXLON_ERROR_CONFIG,
			"read",
			"invalid XLON read configuration",
			NULL
		);
		return false;
	}
	for ( size_t i = 0; i < 4u; i++ ) {
		if ( pConfig->Reserved[i] != 0 ) {
			__xrtXlonError(
				XERR_ARGUMENT,
				XXLON_ERROR_CONFIG,
				"read",
				"reserved XLON read configuration fields must be zero",
				NULL
			);
			return false;
		}
	}
	return true;
}



/* 把 XLON 配置压缩为共享解析器只需要的字段。 */
static xtextvaluereadconfig __xrtXlonReadTextConfig(
	const xxlonreadconfig* pConfig
)
{
	xtextvaluereadconfig Config;

	memset(&Config, 0, sizeof(Config));
	Config.Dialect = XTEXT_VALUE_XLON;
	Config.Flags = pConfig->Flags & (
		XXLON_READ_COMMENTS | XXLON_READ_TRAILING_COMMA
	);
	Config.BigIntegerFloat = pConfig->BigInteger == XXLON_BIGINT_FLOAT;
	Config.MaxDepth = pConfig->MaxDepth;
	Config.MaxInputBytes = pConfig->MaxInputBytes;
	Config.MaxStringBytes = pConfig->MaxStringBytes;
	Config.MaxValues = pConfig->MaxValues;
	Config.MaxContainerItems = pConfig->MaxContainerItems;
	return Config;
}



/* 返回当前 DOM 事件的父容器。 */
static xvalue* __xrtXlonDomParent(
	xlondombuilder* pBuilder,
	const xxlonevent* pEvent
)
{
	if ( pEvent->Depth == 0 ) {
		return NULL;
	}
	return pBuilder->Frames[pEvent->Depth - 1u].Value;
}



/* 按父容器类型挂入值，并执行对象与整数映射重复键策略。 */
static int __xrtXlonDomAttach(
	xlondombuilder* pBuilder,
	const xxlonevent* pEvent,
	xvalue* pValue
)
{
	xvalue* pParent = __xrtXlonDomParent(pBuilder, pEvent);
	xvaluetype Type;

	if ( pEvent->Depth == 0 ) {
		if ( pBuilder->Root != NULL ) {
			__xrtXlonError(
				XERR_STATE,
				XXLON_ERROR_STATE,
				"read",
				"XLON DOM already has a root value",
				&pEvent->Location
			);
			return -1;
		}
		pBuilder->Root = pValue;
		return 0;
	}
	if ( pParent == NULL ) {
		__xrtXlonError(
			XERR_STATE,
			XXLON_ERROR_STATE,
			"read",
			"XLON DOM parent is missing",
			&pEvent->Location
		);
		return -1;
	}
	Type = xrtValueType(pParent);
	if ( Type == XVALUE_ARRAY ) {
		return
			(pEvent->Key.Type == XVALUE_KEY_INDEX) &&
			xrtValueArrayAppend(pParent, pValue)
			? 0
			: -1;
	}
	if ( Type == XVALUE_SET ) {
		return
			(pEvent->Key.Type == XVALUE_KEY_NONE) &&
			xrtValueSetAdd(pParent, pValue)
			? 0
			: -1;
	}
	if ( Type == XVALUE_INT_MAP ) {
		if ( pEvent->Key.Type != XVALUE_KEY_INT ) {
			return -1;
		}
		if ( xrtValueIntMapHas(pParent, pEvent->Key.Integer) ) {
			if ( pBuilder->Config.Duplicate == XXLON_DUPLICATE_REJECT ) {
				__xrtXlonError(
					XERR_EXISTS,
					XXLON_ERROR_DUPLICATE,
					"read",
					"duplicate XLON integer map key",
					&pEvent->Location
				);
				return -1;
			}
			if ( pBuilder->Config.Duplicate == XXLON_DUPLICATE_KEEP ) {
				return 1;
			}
		}
		return xrtValueIntMapSet(pParent, pEvent->Key.Integer, pValue)
			? 0
			: -1;
	}
	if ( Type != XVALUE_OBJECT ) {
		return -1;
	}
	if ( pEvent->Key.Type != XVALUE_KEY_STRING ) {
		return -1;
	}
	if ( xrtValueObjectHas(pParent, pEvent->Key.String) ) {
		if ( pBuilder->Config.Duplicate == XXLON_DUPLICATE_REJECT ) {
			__xrtXlonError(
				XERR_EXISTS,
				XXLON_ERROR_DUPLICATE,
				"read",
				"duplicate XLON object name",
				&pEvent->Location
			);
			return -1;
		}
		if ( pBuilder->Config.Duplicate == XXLON_DUPLICATE_KEEP ) {
			return 1;
		}
	}
	return xrtValueObjectSet(pParent, pEvent->Key.String, pValue) ? 0 : -1;
}



/* 调用自定义标签解码器，并在无具体错误时建立标准错误。 */
static xvalue* __xrtXlonDecodeCustom(
	xlondombuilder* pBuilder,
	const xxlonevent* pEvent
)
{
	const xerror* pPrevious;
	xerror* pHeld;
	xvalue* pValue;

	if ( pBuilder->Config.Decode == NULL ) {
		__xrtXlonError(
			XERR_UNSUPPORTED,
			XXLON_ERROR_UNSUPPORTED,
			"read",
			"custom XLON tag has no decoder",
			&pEvent->Location
		);
		return NULL;
	}
	pPrevious = xrtGetError();
	pHeld = xrtErrorRef(pPrevious);
	pValue = pBuilder->Config.Decode(
		pEvent->Value.Tag.Name,
		pEvent->Value.Tag.Payload,
		pBuilder->Config.DecodeData
	);
	if ( (pValue == NULL) && (xrtGetError() == pPrevious) ) {
		__xrtXlonError(
			XERR_VALUE,
			XXLON_ERROR_TAG,
			"read",
			"custom XLON decoder rejected tag",
			&pEvent->Location
		);
	}
	xrtErrorFree(pHeld);
	return pValue;
}



/* 创建标量 XLON 事件对应的动态值。 */
static xvalue* __xrtXlonDomScalar(
	xlondombuilder* pBuilder,
	const xxlonevent* pEvent
)
{
	switch ( pEvent->Type ) {
		case XXLON_EVENT_NULL:
			return xrtValueRetain(xrtValueNull());
		case XXLON_EVENT_BOOL:
			return xrtValueRetain(xrtValueBool(pEvent->Value.Boolean));
		case XXLON_EVENT_INT:
			return xrtValueInt(pEvent->Value.Integer);
		case XXLON_EVENT_UINT:
			return xrtValueUInt(pEvent->Value.Unsigned);
		case XXLON_EVENT_CHAR:
			return xrtValueChar(pEvent->Value.Character);
		case XXLON_EVENT_FLOAT:
			return xrtValueFloat(pEvent->Value.Float);
		case XXLON_EVENT_STRING:
			return xrtValueString(pEvent->Value.String);
		case XXLON_EVENT_BYTES:
			return xrtValueBytes(pEvent->Value.Bytes);
		case XXLON_EVENT_TIME:
			return xrtValueTime(pEvent->Value.Time);
		case XXLON_EVENT_CUSTOM:
			return __xrtXlonDecodeCustom(pBuilder, pEvent);
		default:
			__xrtErrorSetInvalidState();
			return NULL;
	}
}



/* 使用已转换的 XLON 事件构建 Value DOM。 */
static xtextvaluevisitaction __xrtXlonDomVisit(
	const xtextvalueevent* pSource,
	ptr pUserData
)
{
	xlondombuilder* pBuilder = (xlondombuilder*)pUserData;
	xxlonevent Event;
	xlondomframe* pFrame;
	xvalue* pValue;
	int iAttach;
	bool bBegin;
	bool bEnd;

	if ( !__xrtXlonMakeEvent(
		pSource,
		&pBuilder->Config,
		pBuilder->Budget,
		&pBuilder->Bytes,
		&Event
	) ) {
		return XTEXT_VALUE_VISIT_FAIL;
	}
	if ( pBuilder->Validate ) {
		return XTEXT_VALUE_VISIT_NEXT;
	}
	bBegin =
		(Event.Type == XXLON_EVENT_ARRAY_BEGIN) ||
		(Event.Type == XXLON_EVENT_INT_MAP_BEGIN) ||
		(Event.Type == XXLON_EVENT_SET_BEGIN) ||
		(Event.Type == XXLON_EVENT_OBJECT_BEGIN);
	bEnd =
		(Event.Type == XXLON_EVENT_ARRAY_END) ||
		(Event.Type == XXLON_EVENT_INT_MAP_END) ||
		(Event.Type == XXLON_EVENT_SET_END) ||
		(Event.Type == XXLON_EVENT_OBJECT_END);
	if ( bEnd ) {
		pFrame = &pBuilder->Frames[Event.Depth];
		if ( pFrame->Value == NULL ) {
			__xrtXlonError(
				XERR_STATE,
				XXLON_ERROR_STATE,
				"read",
				"XLON DOM container stack is unbalanced",
				&Event.Location
			);
			return XTEXT_VALUE_VISIT_FAIL;
		}
		if ( pFrame->Owned ) {
			xrtValueRelease(pFrame->Value);
		}
		memset(pFrame, 0, sizeof(*pFrame));
		return XTEXT_VALUE_VISIT_NEXT;
	}
	if ( Event.Type == XXLON_EVENT_ARRAY_BEGIN ) {
		pValue = xrtValueArray();
	} else if ( Event.Type == XXLON_EVENT_INT_MAP_BEGIN ) {
		pValue = xrtValueIntMap();
	} else if ( Event.Type == XXLON_EVENT_SET_BEGIN ) {
		pValue = xrtValueSet();
	} else if ( Event.Type == XXLON_EVENT_OBJECT_BEGIN ) {
		pValue = xrtValueObject();
	} else {
		pValue = __xrtXlonDomScalar(pBuilder, &Event);
	}
	if ( pValue == NULL ) {
		return XTEXT_VALUE_VISIT_FAIL;
	}
	iAttach = __xrtXlonDomAttach(pBuilder, &Event, pValue);
	if ( iAttach < 0 ) {
		xrtValueRelease(pValue);
		return XTEXT_VALUE_VISIT_FAIL;
	}
	if ( bBegin ) {
		pFrame = &pBuilder->Frames[Event.Depth];
		pFrame->Value = pValue;
		pFrame->Owned = iAttach > 0;
		if ( (Event.Depth > 0) && (iAttach == 0) ) {
			xrtValueRelease(pValue);
		}
	} else if ( Event.Depth > 0 ) {
		xrtValueRelease(pValue);
	} else if ( iAttach > 0 ) {
		xrtValueRelease(pValue);
	}
	return XTEXT_VALUE_VISIT_NEXT;
}



/* 释放失败解析留下的根、栈所有权和临时二进制缓冲。 */
static void __xrtXlonDomCleanup(xlondombuilder* pBuilder)
{
	for ( size_t i = 0; i < XRT_VALUE_DEPTH_MAX; i++ ) {
		if ( pBuilder->Frames[i].Owned ) {
			xrtValueRelease(pBuilder->Frames[i].Value);
		}
	}
	xrtValueRelease(pBuilder->Root);
	pBuilder->Root = NULL;
	xrtBufferUnit(&pBuilder->Bytes);
}



/* 把共享事件转换后提交给公开 XLON 访问器。 */
static xtextvaluevisitaction __xrtXlonVisitAdapter(
	const xtextvalueevent* pSource,
	ptr pUserData
)
{
	xlonvisitadapter* pAdapter = (xlonvisitadapter*)pUserData;
	xxlonevent Event;
	xxlonvisitaction Action;

	if ( !__xrtXlonMakeEvent(
		pSource,
		pAdapter->Config,
		NULL,
		&pAdapter->Bytes,
		&Event
	) ) {
		return XTEXT_VALUE_VISIT_FAIL;
	}
	Action = pAdapter->Visitor(&Event, pAdapter->UserData);
	return (xtextvaluevisitaction)Action;
}



/* 验证路径只消费事件，内建标签仍会完成严格语义校验。 */
static xxlonvisitaction __xrtXlonValidateVisit(
	const xxlonevent* pEvent,
	ptr pUserData
)
{
	(void)pEvent;
	(void)pUserData;
	return XXLON_VISIT_NEXT;
}



/* 初始化严格且带安全资源预算的 XLON 读取配置。 */
XRT_API void xrtXlonReadConfigInit(xxlonreadconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Duplicate = XXLON_DUPLICATE_REJECT;
	pConfig->BigInteger = XXLON_BIGINT_REJECT;
	pConfig->MaxDepth = XXLON_DEPTH_DEFAULT;
	pConfig->MaxInputBytes = XXLON_INPUT_DEFAULT;
	pConfig->MaxStringBytes = XXLON_STRING_DEFAULT;
	pConfig->MaxValues = XXLON_VALUES_DEFAULT;
	pConfig->MaxContainerItems = XXLON_CONTAINER_DEFAULT;
	pConfig->MaxDecodedBytes = XXLON_DECODED_DEFAULT;
}



/* 使用高级配置解析完整 XLON 文本。 */
xvalue* __xrtXlonReadBudget(
	xstrview Text,
	const xxlonreadconfig* pConfig,
	xtextvaluebudget* pBudget,
	bool bValidate
)
{
	xlondombuilder Builder;
	xtextvaluereadconfig TextConfig;
	xtextvaluevisitresult Result;

	if ( !__xrtXlonReadConfigValid(pConfig) ) {
		return NULL;
	}
	memset(&Builder, 0, sizeof(Builder));
	Builder.Config = *pConfig;
	Builder.Budget = pBudget;
	Builder.Validate = bValidate;
	if ( !xrtBufferInit(&Builder.Bytes) ) {
		return NULL;
	}
	TextConfig = __xrtXlonReadTextConfig(pConfig);
	TextConfig.Budget = pBudget;
	Result = __xrtTextValueRead(
		Text,
		&TextConfig,
		__xrtXlonDomVisit,
		&Builder,
		__xrtXlonReadError,
		NULL,
		true
	);
	if ( Result != XTEXT_VALUE_VISIT_DONE ) {
		xerror* pError = xrtTakeError();
		__xrtXlonDomCleanup(&Builder);
		xrtSetErrorTake(pError);
		return NULL;
	}
	xrtBufferUnit(&Builder.Bytes);
	return bValidate ? xrtValueNull() : Builder.Root;
}



/* 使用高级配置解析完整 XLON 文本。 */
XRT_API xvalue* xrtXlonRead(xstrview Text, const xxlonreadconfig* pConfig)
{
	return __xrtXlonReadBudget(Text, pConfig, NULL, false);
}



/* 使用默认严格配置解析完整 XLON 文本。 */
XRT_API xvalue* xrtXlonParse(xstrview Text)
{
	xxlonreadconfig Config;

	xrtXlonReadConfigInit(&Config);
	return xrtXlonRead(Text, &Config);
}



/* 验证默认 XLON 语法和内建标签，不构造 Value DOM。 */
XRT_API bool xrtXlonValid(xstrview Text)
{
	xxlonreadconfig Config;

	xrtXlonReadConfigInit(&Config);
	return xrtXlonVisit(
		Text,
		&Config,
		__xrtXlonValidateVisit,
		NULL
	) == XXLON_VISIT_DONE;
}



/* 直接访问解析事件，不构造中间 DOM。 */
XRT_API xxlonvisitresult xrtXlonVisit(
	xstrview Text,
	const xxlonreadconfig* pConfig,
	xxlonvisitproc pVisitor,
	ptr pUserData
)
{
	xlonvisitadapter Adapter;
	xtextvaluereadconfig TextConfig;
	xtextvaluevisitresult Result;

	if ( (pVisitor == NULL) || !__xrtXlonReadConfigValid(pConfig) ) {
		if ( pVisitor == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XXLON_VISIT_ERROR;
	}
	memset(&Adapter, 0, sizeof(Adapter));
	Adapter.Config = pConfig;
	Adapter.Visitor = pVisitor;
	Adapter.UserData = pUserData;
	if ( !xrtBufferInit(&Adapter.Bytes) ) {
		return XXLON_VISIT_ERROR;
	}
	TextConfig = __xrtXlonReadTextConfig(pConfig);
	Result = __xrtTextValueRead(
		Text,
		&TextConfig,
		__xrtXlonVisitAdapter,
		&Adapter,
		__xrtXlonReadError,
		NULL,
		true
	);
	xrtBufferUnit(&Adapter.Bytes);
	return (xxlonvisitresult)Result;
}

#endif
