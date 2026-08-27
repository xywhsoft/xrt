#include "../internal/xrt_xson.h"

#include <math.h>



#if defined(XRT_FEATURE_XSON_READ)

/* DOM 栈帧只拥有因 KEEP 策略未挂入父容器的临时子树。 */
typedef struct xsondomframe {
	xvalue* Value;
	bool Owned;
} xsondomframe;



/* DOM 构建器保存重复策略、自定义解码器和复用的二进制缓冲。 */
typedef struct xsondombuilder {
	xxsonreadconfig Config;
	xvalue* Root;
	xsondomframe Frames[XRT_VALUE_DEPTH_MAX];
	xbuffer Bytes;
} xsondombuilder;



/* 公开访问器适配器复用一个二进制缓冲，事件返回后即可覆盖。 */
typedef struct xsonvisitadapter {
	const xxsonreadconfig* Config;
	xxsonvisitproc Visitor;
	ptr UserData;
	xbuffer Bytes;
} xsonvisitadapter;



/* 把内部文本位置复制为 XSON 稳定位置。 */
static xxsonlocation __xrtXsonLocation(
	const xtextvaluelocation* pLocation
)
{
	xxsonlocation Location;

	Location.Offset = pLocation->Offset;
	Location.Line = pLocation->Line;
	Location.Column = pLocation->Column;
	return Location;
}



/* 在事件位置设置 XSON 读取错误。 */
static void __xrtXsonEventError(
	const xtextvalueevent* pEvent,
	xerrkind Kind,
	xxsonerror Code,
	cstr sMessage
)
{
	xxsonlocation Location = __xrtXsonLocation(&pEvent->Location);

	__xrtXsonError(Kind, Code, "read", sMessage, &Location);
}



/* 把共享读取错误映射到 xrt.xson 错误域。 */
static void __xrtXsonReadError(
	xerrkind Kind,
	xtextvalueerror Code,
	cstr sMessage,
	const xtextvaluelocation* pLocation,
	ptr pUserData
)
{
	xxsonlocation Location;
	xxsonerror XsonCode;

	(void)pUserData;
	if ( Code == XTEXT_VALUE_ERROR_LIMIT ) {
		XsonCode = XXSON_ERROR_LIMIT;
	} else if ( Code == XTEXT_VALUE_ERROR_NUMBER ) {
		XsonCode = XXSON_ERROR_NUMBER;
	} else if ( Code == XTEXT_VALUE_ERROR_STATE ) {
		XsonCode = XXSON_ERROR_STATE;
	} else {
		XsonCode = XXSON_ERROR_SYNTAX;
	}
	if ( pLocation == NULL ) {
		__xrtXsonError(Kind, XsonCode, "read", sMessage, NULL);
		return;
	}
	Location = __xrtXsonLocation(pLocation);
	__xrtXsonError(Kind, XsonCode, "read", sMessage, &Location);
}



/* 判断标签名称是否等于固定 ASCII 文本。 */
static bool __xrtXsonTagEqual(
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
static bool __xrtXsonDecodeBytes(
	const xtextvalueevent* pSource,
	const xxsonreadconfig* pConfig,
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
		__xrtXsonEventError(
			pSource,
			XERR_VALUE,
			XXSON_ERROR_TAG,
			"bytes tag contains invalid Base64"
		);
		return false;
	}
	if ( iSize > pConfig->MaxDecodedBytes ) {
		__xrtXsonEventError(
			pSource,
			XERR_RANGE,
			XXSON_ERROR_LIMIT,
			"decoded bytes exceed configured limit"
		);
		return false;
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
		__xrtXsonEventError(
			pSource,
			XERR_VALUE,
			XXSON_ERROR_TAG,
			"bytes tag could not be decoded"
		);
		return false;
	}
	pBytes->Data = pBuffer->Data;
	pBytes->Size = iSize;
	return true;
}



/* 把共享事件转换为完整 XSON 事件，并解释所有内建标签。 */
static bool __xrtXsonMakeEvent(
	const xtextvalueevent* pSource,
	const xxsonreadconfig* pConfig,
	xbuffer* pBytes,
	xxsonevent* pEvent
)
{
	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->Location = __xrtXsonLocation(&pSource->Location);
	pEvent->Depth = pSource->Depth;
	pEvent->Key = pSource->Key;
	pEvent->Raw = pSource->Raw;

	if ( pSource->Type == XTEXT_VALUE_EVENT_NULL ) {
		pEvent->Type = XXSON_EVENT_NULL;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_BOOL ) {
		pEvent->Type = XXSON_EVENT_BOOL;
		pEvent->Value.Boolean = pSource->Value.Boolean;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_INT ) {
		pEvent->Type = XXSON_EVENT_INT;
		pEvent->Value.Integer = pSource->Value.Integer;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_UINT ) {
		pEvent->Type = XXSON_EVENT_UINT;
		pEvent->Value.Unsigned = pSource->Value.Unsigned;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_FLOAT ) {
		pEvent->Type = XXSON_EVENT_FLOAT;
		pEvent->Value.Float = pSource->Value.Float;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_STRING ) {
		pEvent->Type = XXSON_EVENT_STRING;
		pEvent->Value.String = pSource->Value.String;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_ARRAY_BEGIN ) {
		pEvent->Type = XXSON_EVENT_ARRAY_BEGIN;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_ARRAY_END ) {
		pEvent->Type = XXSON_EVENT_ARRAY_END;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_INT_MAP_BEGIN ) {
		pEvent->Type = XXSON_EVENT_INT_MAP_BEGIN;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_INT_MAP_END ) {
		pEvent->Type = XXSON_EVENT_INT_MAP_END;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_SET_BEGIN ) {
		pEvent->Type = XXSON_EVENT_SET_BEGIN;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_SET_END ) {
		pEvent->Type = XXSON_EVENT_SET_END;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_OBJECT_BEGIN ) {
		pEvent->Type = XXSON_EVENT_OBJECT_BEGIN;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_OBJECT_END ) {
		pEvent->Type = XXSON_EVENT_OBJECT_END;
	} else if ( pSource->Type != XTEXT_VALUE_EVENT_TAG ) {
		__xrtXsonEventError(
			pSource,
			XERR_STATE,
			XXSON_ERROR_STATE,
			"unknown internal XSON event"
		);
		return false;
	} else if ( __xrtXsonTagEqual(pSource->Value.Tag.Name, "bytes", 5u) ) {
		pEvent->Type = XXSON_EVENT_BYTES;
		if ( !__xrtXsonDecodeBytes(
			pSource,
			pConfig,
			pBytes,
			&pEvent->Value.Bytes
		) ) {
			return false;
		}
	} else if ( __xrtXsonTagEqual(pSource->Value.Tag.Name, "time", 4u) ) {
		pEvent->Type = XXSON_EVENT_TIME;
		if ( !xrtTimeParseRFC3339(
			pSource->Value.Tag.Payload,
			&pEvent->Value.Time
		) ) {
			xrtClearError();
			__xrtXsonEventError(
				pSource,
				XERR_VALUE,
				XXSON_ERROR_TAG,
				"time tag requires strict RFC 3339 text"
			);
			return false;
		}
	} else if ( __xrtXsonTagEqual(pSource->Value.Tag.Name, "float", 5u) ) {
		pEvent->Type = XXSON_EVENT_FLOAT;
		if ( __xrtXsonTagEqual(pSource->Value.Tag.Payload, "nan", 3u) ) {
			pEvent->Value.Float = NAN;
		} else if ( __xrtXsonTagEqual(pSource->Value.Tag.Payload, "inf", 3u) ) {
			pEvent->Value.Float = INFINITY;
		} else if ( __xrtXsonTagEqual(pSource->Value.Tag.Payload, "-inf", 4u) ) {
			pEvent->Value.Float = -INFINITY;
		} else {
			__xrtXsonEventError(
				pSource,
				XERR_VALUE,
				XXSON_ERROR_TAG,
				"float tag payload must be nan, inf or -inf"
			);
			return false;
		}
	} else {
		if ( (pConfig->Flags & XXSON_READ_CUSTOM) == 0 ) {
			__xrtXsonEventError(
				pSource,
				XERR_UNSUPPORTED,
				XXSON_ERROR_UNSUPPORTED,
				"custom XSON tag is disabled"
			);
			return false;
		}
		pEvent->Type = XXSON_EVENT_CUSTOM;
		pEvent->Value.Tag.Name = pSource->Value.Tag.Name;
		pEvent->Value.Tag.Payload = pSource->Value.Tag.Payload;
	}
	return true;
}



/* 验证读取配置和全部保留字段。 */
bool __xrtXsonReadConfigValid(const xxsonreadconfig* pConfig)
{
	uint32 iKnownFlags =
		XXSON_READ_COMMENTS |
		XXSON_READ_TRAILING_COMMA |
		XXSON_READ_CUSTOM;

	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		((pConfig->Flags & ~iKnownFlags) != 0) ||
		(pConfig->Duplicate < XXSON_DUPLICATE_REJECT) ||
		(pConfig->Duplicate > XXSON_DUPLICATE_REPLACE) ||
		(pConfig->BigInteger < XXSON_BIGINT_REJECT) ||
		(pConfig->BigInteger > XXSON_BIGINT_FLOAT) ||
		(pConfig->MaxDepth == 0) ||
		(pConfig->MaxDepth > XRT_VALUE_DEPTH_MAX) ||
		(pConfig->MaxInputBytes == 0) ||
		(pConfig->MaxStringBytes == 0) ||
		(pConfig->MaxValues == 0) ||
		(pConfig->MaxContainerItems == 0) ||
		(pConfig->MaxDecodedBytes == 0)
	) {
		__xrtXsonError(
			XERR_ARGUMENT,
			XXSON_ERROR_CONFIG,
			"read",
			"invalid XSON read configuration",
			NULL
		);
		return false;
	}
	for ( size_t i = 0; i < 4u; i++ ) {
		if ( pConfig->Reserved[i] != 0 ) {
			__xrtXsonError(
				XERR_ARGUMENT,
				XXSON_ERROR_CONFIG,
				"read",
				"reserved XSON read configuration fields must be zero",
				NULL
			);
			return false;
		}
	}
	return true;
}



/* 把 XSON 配置压缩为共享解析器只需要的字段。 */
static xtextvaluereadconfig __xrtXsonReadTextConfig(
	const xxsonreadconfig* pConfig
)
{
	xtextvaluereadconfig Config;

	memset(&Config, 0, sizeof(Config));
	Config.Dialect = XTEXT_VALUE_XSON;
	Config.Flags = pConfig->Flags & (
		XXSON_READ_COMMENTS | XXSON_READ_TRAILING_COMMA
	);
	Config.BigIntegerFloat = pConfig->BigInteger == XXSON_BIGINT_FLOAT;
	Config.MaxDepth = pConfig->MaxDepth;
	Config.MaxInputBytes = pConfig->MaxInputBytes;
	Config.MaxStringBytes = pConfig->MaxStringBytes;
	Config.MaxValues = pConfig->MaxValues;
	Config.MaxContainerItems = pConfig->MaxContainerItems;
	return Config;
}



/* 返回当前 DOM 事件的父容器。 */
static xvalue* __xrtXsonDomParent(
	xsondombuilder* pBuilder,
	const xxsonevent* pEvent
)
{
	if ( pEvent->Depth == 0 ) {
		return NULL;
	}
	return pBuilder->Frames[pEvent->Depth - 1u].Value;
}



/* 按父容器类型挂入值，并执行对象与整数映射重复键策略。 */
static int __xrtXsonDomAttach(
	xsondombuilder* pBuilder,
	const xxsonevent* pEvent,
	xvalue* pValue
)
{
	xvalue* pParent = __xrtXsonDomParent(pBuilder, pEvent);
	xvaluetype Type;

	if ( pEvent->Depth == 0 ) {
		if ( pBuilder->Root != NULL ) {
			__xrtXsonError(
				XERR_STATE,
				XXSON_ERROR_STATE,
				"read",
				"XSON DOM already has a root value",
				&pEvent->Location
			);
			return -1;
		}
		pBuilder->Root = pValue;
		return 0;
	}
	if ( pParent == NULL ) {
		__xrtXsonError(
			XERR_STATE,
			XXSON_ERROR_STATE,
			"read",
			"XSON DOM parent is missing",
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
			if ( pBuilder->Config.Duplicate == XXSON_DUPLICATE_REJECT ) {
				__xrtXsonError(
					XERR_EXISTS,
					XXSON_ERROR_DUPLICATE,
					"read",
					"duplicate XSON integer map key",
					&pEvent->Location
				);
				return -1;
			}
			if ( pBuilder->Config.Duplicate == XXSON_DUPLICATE_KEEP ) {
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
		if ( pBuilder->Config.Duplicate == XXSON_DUPLICATE_REJECT ) {
			__xrtXsonError(
				XERR_EXISTS,
				XXSON_ERROR_DUPLICATE,
				"read",
				"duplicate XSON object name",
				&pEvent->Location
			);
			return -1;
		}
		if ( pBuilder->Config.Duplicate == XXSON_DUPLICATE_KEEP ) {
			return 1;
		}
	}
	return xrtValueObjectSet(pParent, pEvent->Key.String, pValue) ? 0 : -1;
}



/* 调用自定义标签解码器，并在无具体错误时建立标准错误。 */
static xvalue* __xrtXsonDecodeCustom(
	xsondombuilder* pBuilder,
	const xxsonevent* pEvent
)
{
	const xerror* pPrevious;
	xerror* pHeld;
	xvalue* pValue;

	if ( pBuilder->Config.Decode == NULL ) {
		__xrtXsonError(
			XERR_UNSUPPORTED,
			XXSON_ERROR_UNSUPPORTED,
			"read",
			"custom XSON tag has no decoder",
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
		__xrtXsonError(
			XERR_VALUE,
			XXSON_ERROR_TAG,
			"read",
			"custom XSON decoder rejected tag",
			&pEvent->Location
		);
	}
	xrtErrorFree(pHeld);
	return pValue;
}



/* 创建标量 XSON 事件对应的动态值。 */
static xvalue* __xrtXsonDomScalar(
	xsondombuilder* pBuilder,
	const xxsonevent* pEvent
)
{
	switch ( pEvent->Type ) {
		case XXSON_EVENT_NULL:
			return xrtValueRetain(xrtValueNull());
		case XXSON_EVENT_BOOL:
			return xrtValueRetain(xrtValueBool(pEvent->Value.Boolean));
		case XXSON_EVENT_INT:
			return xrtValueInt(pEvent->Value.Integer);
		case XXSON_EVENT_UINT:
			return xrtValueUInt(pEvent->Value.Unsigned);
		case XXSON_EVENT_FLOAT:
			return xrtValueFloat(pEvent->Value.Float);
		case XXSON_EVENT_STRING:
			return xrtValueString(pEvent->Value.String);
		case XXSON_EVENT_BYTES:
			return xrtValueBytes(pEvent->Value.Bytes);
		case XXSON_EVENT_TIME:
			return xrtValueTime(pEvent->Value.Time);
		case XXSON_EVENT_CUSTOM:
			return __xrtXsonDecodeCustom(pBuilder, pEvent);
		default:
			__xrtErrorSetInvalidState();
			return NULL;
	}
}



/* 使用已转换的 XSON 事件构建 Value DOM。 */
static xtextvaluevisitaction __xrtXsonDomVisit(
	const xtextvalueevent* pSource,
	ptr pUserData
)
{
	xsondombuilder* pBuilder = (xsondombuilder*)pUserData;
	xxsonevent Event;
	xsondomframe* pFrame;
	xvalue* pValue;
	int iAttach;
	bool bBegin;
	bool bEnd;

	if ( !__xrtXsonMakeEvent(
		pSource,
		&pBuilder->Config,
		&pBuilder->Bytes,
		&Event
	) ) {
		return XTEXT_VALUE_VISIT_FAIL;
	}
	bBegin =
		(Event.Type == XXSON_EVENT_ARRAY_BEGIN) ||
		(Event.Type == XXSON_EVENT_INT_MAP_BEGIN) ||
		(Event.Type == XXSON_EVENT_SET_BEGIN) ||
		(Event.Type == XXSON_EVENT_OBJECT_BEGIN);
	bEnd =
		(Event.Type == XXSON_EVENT_ARRAY_END) ||
		(Event.Type == XXSON_EVENT_INT_MAP_END) ||
		(Event.Type == XXSON_EVENT_SET_END) ||
		(Event.Type == XXSON_EVENT_OBJECT_END);
	if ( bEnd ) {
		pFrame = &pBuilder->Frames[Event.Depth];
		if ( pFrame->Value == NULL ) {
			__xrtXsonError(
				XERR_STATE,
				XXSON_ERROR_STATE,
				"read",
				"XSON DOM container stack is unbalanced",
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
	if ( Event.Type == XXSON_EVENT_ARRAY_BEGIN ) {
		pValue = xrtValueArray();
	} else if ( Event.Type == XXSON_EVENT_INT_MAP_BEGIN ) {
		pValue = xrtValueIntMap();
	} else if ( Event.Type == XXSON_EVENT_SET_BEGIN ) {
		pValue = xrtValueSet();
	} else if ( Event.Type == XXSON_EVENT_OBJECT_BEGIN ) {
		pValue = xrtValueObject();
	} else {
		pValue = __xrtXsonDomScalar(pBuilder, &Event);
	}
	if ( pValue == NULL ) {
		return XTEXT_VALUE_VISIT_FAIL;
	}
	iAttach = __xrtXsonDomAttach(pBuilder, &Event, pValue);
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
static void __xrtXsonDomCleanup(xsondombuilder* pBuilder)
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



/* 把共享事件转换后提交给公开 XSON 访问器。 */
static xtextvaluevisitaction __xrtXsonVisitAdapter(
	const xtextvalueevent* pSource,
	ptr pUserData
)
{
	xsonvisitadapter* pAdapter = (xsonvisitadapter*)pUserData;
	xxsonevent Event;
	xxsonvisitaction Action;

	if ( !__xrtXsonMakeEvent(
		pSource,
		pAdapter->Config,
		&pAdapter->Bytes,
		&Event
	) ) {
		return XTEXT_VALUE_VISIT_FAIL;
	}
	Action = pAdapter->Visitor(&Event, pAdapter->UserData);
	return (xtextvaluevisitaction)Action;
}



/* 验证路径只消费事件，内建标签仍会完成严格语义校验。 */
static xxsonvisitaction __xrtXsonValidateVisit(
	const xxsonevent* pEvent,
	ptr pUserData
)
{
	(void)pEvent;
	(void)pUserData;
	return XXSON_VISIT_NEXT;
}



/* 初始化严格且带安全资源预算的 XSON 读取配置。 */
XRT_API void xrtXsonReadConfigInit(xxsonreadconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Duplicate = XXSON_DUPLICATE_REJECT;
	pConfig->BigInteger = XXSON_BIGINT_REJECT;
	pConfig->MaxDepth = XXSON_DEPTH_DEFAULT;
	pConfig->MaxInputBytes = XXSON_INPUT_DEFAULT;
	pConfig->MaxStringBytes = XXSON_STRING_DEFAULT;
	pConfig->MaxValues = XXSON_VALUES_DEFAULT;
	pConfig->MaxContainerItems = XXSON_CONTAINER_DEFAULT;
	pConfig->MaxDecodedBytes = XXSON_DECODED_DEFAULT;
}



/* 使用高级配置解析完整 XSON 文本。 */
XRT_API xvalue* xrtXsonRead(
	xstrview Text,
	const xxsonreadconfig* pConfig
)
{
	xsondombuilder Builder;
	xtextvaluereadconfig TextConfig;
	xtextvaluevisitresult Result;

	if ( !__xrtXsonReadConfigValid(pConfig) ) {
		return NULL;
	}
	memset(&Builder, 0, sizeof(Builder));
	Builder.Config = *pConfig;
	if ( !xrtBufferInit(&Builder.Bytes) ) {
		return NULL;
	}
	TextConfig = __xrtXsonReadTextConfig(pConfig);
	Result = __xrtTextValueRead(
		Text,
		&TextConfig,
		__xrtXsonDomVisit,
		&Builder,
		__xrtXsonReadError,
		NULL,
		true
	);
	if ( Result != XTEXT_VALUE_VISIT_DONE ) {
		__xrtXsonDomCleanup(&Builder);
		return NULL;
	}
	xrtBufferUnit(&Builder.Bytes);
	return Builder.Root;
}



/* 使用默认严格配置解析完整 XSON 文本。 */
XRT_API xvalue* xrtXsonParse(xstrview Text)
{
	xxsonreadconfig Config;

	xrtXsonReadConfigInit(&Config);
	return xrtXsonRead(Text, &Config);
}



/* 验证默认 XSON 语法和内建标签，不构造 Value DOM。 */
XRT_API bool xrtXsonValid(xstrview Text)
{
	xxsonreadconfig Config;

	xrtXsonReadConfigInit(&Config);
	return xrtXsonVisit(
		Text,
		&Config,
		__xrtXsonValidateVisit,
		NULL
	) == XXSON_VISIT_DONE;
}



/* 直接访问解析事件，不构造中间 DOM。 */
XRT_API xxsonvisitresult xrtXsonVisit(
	xstrview Text,
	const xxsonreadconfig* pConfig,
	xxsonvisitproc pVisitor,
	ptr pUserData
)
{
	xsonvisitadapter Adapter;
	xtextvaluereadconfig TextConfig;
	xtextvaluevisitresult Result;

	if ( (pVisitor == NULL) || !__xrtXsonReadConfigValid(pConfig) ) {
		if ( pVisitor == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XXSON_VISIT_ERROR;
	}
	memset(&Adapter, 0, sizeof(Adapter));
	Adapter.Config = pConfig;
	Adapter.Visitor = pVisitor;
	Adapter.UserData = pUserData;
	if ( !xrtBufferInit(&Adapter.Bytes) ) {
		return XXSON_VISIT_ERROR;
	}
	TextConfig = __xrtXsonReadTextConfig(pConfig);
	Result = __xrtTextValueRead(
		Text,
		&TextConfig,
		__xrtXsonVisitAdapter,
		&Adapter,
		__xrtXsonReadError,
		NULL,
		true
	);
	xrtBufferUnit(&Adapter.Bytes);
	return (xxsonvisitresult)Result;
}

#endif
