#include "../internal/xrt_json.h"
#include "../internal/xrt_text_value.h"



#if defined(XRT_FEATURE_JSON_READ)

/* DOM 栈帧借用已挂入父容器的值，只拥有 KEEP 策略丢弃的临时子树。 */
typedef struct xjsondomframe {
	xvalue* Value;
	bool Owned;
} xjsondomframe;



/* DOM 构建器使用固定深度栈，避免每层重复分配。 */
typedef struct xjsondombuilder {
	xjsonreadconfig Config;
	xvalue* Root;
	xjsondomframe Frames[XRT_VALUE_DEPTH_MAX];
} xjsondombuilder;



/* 公开访问器适配器只借用用户回调和上下文。 */
typedef struct xjsonvisitadapter {
	xjsonvisitproc Visitor;
	ptr UserData;
} xjsonvisitadapter;



/* 把内部文本位置复制为 JSON 稳定位置。 */
static xjsonlocation __xrtJsonLocation(
	const xtextvaluelocation* pLocation
)
{
	xjsonlocation Location;

	Location.Offset = pLocation->Offset;
	Location.Line = pLocation->Line;
	Location.Column = pLocation->Column;
	return Location;
}



/* 把共享读取错误映射到 xrt.json 错误域。 */
static void __xrtJsonReadError(
	xerrkind Kind,
	xtextvalueerror Code,
	cstr sMessage,
	const xtextvaluelocation* pLocation,
	ptr pUserData
)
{
	xjsonlocation Location;
	xjsonerror JsonCode;

	(void)pUserData;
	if ( Code == XTEXT_VALUE_ERROR_LIMIT ) {
		JsonCode = XJSON_ERROR_LIMIT;
	} else if ( Code == XTEXT_VALUE_ERROR_NUMBER ) {
		JsonCode = XJSON_ERROR_NUMBER;
	} else if ( Code == XTEXT_VALUE_ERROR_STATE ) {
		JsonCode = XJSON_ERROR_STATE;
	} else {
		JsonCode = XJSON_ERROR_SYNTAX;
	}
	if ( pLocation == NULL ) {
		__xrtJsonError(Kind, JsonCode, "read", sMessage, NULL);
		return;
	}
	Location = __xrtJsonLocation(pLocation);
	__xrtJsonError(Kind, JsonCode, "read", sMessage, &Location);
}



/* 验证读取配置已初始化且不会超过当前 Value 图深度。 */
bool __xrtJsonReadConfigValid(const xjsonreadconfig* pConfig)
{
	uint32 iKnownFlags = XJSON_READ_COMMENTS | XJSON_READ_TRAILING_COMMA;

	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		((pConfig->Flags & ~iKnownFlags) != 0) ||
		(pConfig->Duplicate < XJSON_DUPLICATE_REJECT) ||
		(pConfig->Duplicate > XJSON_DUPLICATE_REPLACE) ||
		(pConfig->BigInteger < XJSON_BIGINT_REJECT) ||
		(pConfig->BigInteger > XJSON_BIGINT_FLOAT) ||
		(pConfig->MaxDepth == 0) ||
		(pConfig->MaxDepth > XRT_VALUE_DEPTH_MAX) ||
		(pConfig->MaxInputBytes == 0) ||
		(pConfig->MaxStringBytes == 0) ||
		(pConfig->MaxValues == 0) ||
		(pConfig->MaxContainerItems == 0)
	) {
		__xrtJsonError(
			XERR_ARGUMENT,
			XJSON_ERROR_CONFIG,
			"read",
			"invalid JSON read configuration",
			NULL
		);
		return false;
	}
	for ( size_t i = 0; i < 4u; i++ ) {
		if ( pConfig->Reserved[i] != 0 ) {
			__xrtJsonError(
				XERR_ARGUMENT,
				XJSON_ERROR_CONFIG,
				"read",
				"reserved JSON read configuration fields must be zero",
				NULL
			);
			return false;
		}
	}
	return true;
}



/* 把 JSON 配置压缩为共享解析器只需要的字段。 */
static xtextvaluereadconfig __xrtJsonReadTextConfig(
	const xjsonreadconfig* pConfig
)
{
	xtextvaluereadconfig Config;

	memset(&Config, 0, sizeof(Config));
	Config.Dialect = XTEXT_VALUE_JSON;
	Config.Flags = pConfig->Flags;
	Config.BigIntegerFloat = pConfig->BigInteger == XJSON_BIGINT_FLOAT;
	Config.MaxDepth = pConfig->MaxDepth;
	Config.MaxInputBytes = pConfig->MaxInputBytes;
	Config.MaxStringBytes = pConfig->MaxStringBytes;
	Config.MaxValues = pConfig->MaxValues;
	Config.MaxContainerItems = pConfig->MaxContainerItems;
	return Config;
}



/* 返回 DOM 事件应挂入的父容器。 */
static xvalue* __xrtJsonDomParent(
	xjsondombuilder* pBuilder,
	const xtextvalueevent* pEvent
)
{
	if ( pEvent->Depth == 0 ) {
		return NULL;
	}
	return pBuilder->Frames[pEvent->Depth - 1u].Value;
}



/* 把新值挂入根、数组或对象，并报告重复键 KEEP 丢弃。 */
static int __xrtJsonDomAttach(
	xjsondombuilder* pBuilder,
	const xtextvalueevent* pEvent,
	xvalue* pValue
)
{
	xvalue* pParent = __xrtJsonDomParent(pBuilder, pEvent);
	xvaluetype Type;
	xjsonlocation Location = __xrtJsonLocation(&pEvent->Location);

	if ( pEvent->Depth == 0 ) {
		if ( pBuilder->Root != NULL ) {
			__xrtJsonError(
				XERR_STATE,
				XJSON_ERROR_STATE,
				"read",
				"JSON DOM already has a root value",
				&Location
			);
			return -1;
		}
		pBuilder->Root = pValue;
		return 0;
	}
	if ( pParent == NULL ) {
		__xrtJsonError(
			XERR_STATE,
			XJSON_ERROR_STATE,
			"read",
			"JSON DOM parent is missing",
			&Location
		);
		return -1;
	}
	Type = xrtValueType(pParent);
	if ( Type == XVALUE_ARRAY ) {
		if (
			(pEvent->Key.Type != XVALUE_KEY_INDEX) ||
			!xrtValueArrayAppend(pParent, pValue)
		) {
			if ( pEvent->Key.Type != XVALUE_KEY_INDEX ) {
				__xrtJsonError(
					XERR_STATE,
					XJSON_ERROR_STATE,
					"read",
					"JSON array item has an invalid key",
					&Location
				);
			}
			return -1;
		}
		return 0;
	}
	if ( Type != XVALUE_OBJECT ) {
		__xrtJsonError(
			XERR_STATE,
			XJSON_ERROR_STATE,
			"read",
			"JSON DOM parent is not a container",
			&Location
		);
		return -1;
	}
	if ( pEvent->Key.Type != XVALUE_KEY_STRING ) {
		__xrtJsonError(
			XERR_STATE,
			XJSON_ERROR_STATE,
			"read",
			"JSON object value is missing its name",
			&Location
		);
		return -1;
	}
	if ( xrtValueObjectHas(pParent, pEvent->Key.String) ) {
		if ( pBuilder->Config.Duplicate == XJSON_DUPLICATE_REJECT ) {
			__xrtJsonError(
				XERR_EXISTS,
				XJSON_ERROR_DUPLICATE,
				"read",
				"duplicate JSON object name",
				&Location
			);
			return -1;
		}
		if ( pBuilder->Config.Duplicate == XJSON_DUPLICATE_KEEP ) {
			return 1;
		}
	}
	return xrtValueObjectSet(pParent, pEvent->Key.String, pValue) ? 0 : -1;
}



/* 创建标量事件对应的动态值。 */
static xvalue* __xrtJsonDomScalar(const xtextvalueevent* pEvent)
{
	switch ( pEvent->Type ) {
		case XTEXT_VALUE_EVENT_NULL:
			return xrtValueRetain(xrtValueNull());
		case XTEXT_VALUE_EVENT_BOOL:
			return xrtValueRetain(xrtValueBool(pEvent->Value.Boolean));
		case XTEXT_VALUE_EVENT_INT:
			return xrtValueInt(pEvent->Value.Integer);
		case XTEXT_VALUE_EVENT_FLOAT:
			return xrtValueFloat(pEvent->Value.Float);
		case XTEXT_VALUE_EVENT_STRING:
			return xrtValueString(pEvent->Value.String);
		default:
			__xrtErrorSetInvalidState();
			return NULL;
	}
}



/* 使用共享解析事件构建 JSON Value DOM。 */
static xtextvaluevisitaction __xrtJsonDomVisit(
	const xtextvalueevent* pEvent,
	ptr pUserData
)
{
	xjsondombuilder* pBuilder = (xjsondombuilder*)pUserData;
	xvalue* pValue;
	int iAttach;

	if (
		(pEvent->Type == XTEXT_VALUE_EVENT_ARRAY_END) ||
		(pEvent->Type == XTEXT_VALUE_EVENT_OBJECT_END)
	) {
		xjsondomframe* pFrame = &pBuilder->Frames[pEvent->Depth];

		if ( pFrame->Value == NULL ) {
			xjsonlocation Location = __xrtJsonLocation(&pEvent->Location);

			__xrtJsonError(
				XERR_STATE,
				XJSON_ERROR_STATE,
				"read",
				"JSON DOM container stack is unbalanced",
				&Location
			);
			return XTEXT_VALUE_VISIT_FAIL;
		}
		if ( pFrame->Owned ) {
			xrtValueRelease(pFrame->Value);
		}
		memset(pFrame, 0, sizeof(*pFrame));
		return XTEXT_VALUE_VISIT_NEXT;
	}
	if ( pEvent->Type == XTEXT_VALUE_EVENT_ARRAY_BEGIN ) {
		pValue = xrtValueArray();
	} else if ( pEvent->Type == XTEXT_VALUE_EVENT_OBJECT_BEGIN ) {
		pValue = xrtValueObject();
	} else {
		pValue = __xrtJsonDomScalar(pEvent);
	}
	if ( pValue == NULL ) {
		return XTEXT_VALUE_VISIT_FAIL;
	}
	iAttach = __xrtJsonDomAttach(pBuilder, pEvent, pValue);
	if ( iAttach < 0 ) {
		xrtValueRelease(pValue);
		return XTEXT_VALUE_VISIT_FAIL;
	}
	if (
		(pEvent->Type == XTEXT_VALUE_EVENT_ARRAY_BEGIN) ||
		(pEvent->Type == XTEXT_VALUE_EVENT_OBJECT_BEGIN)
	) {
		xjsondomframe* pFrame = &pBuilder->Frames[pEvent->Depth];

		pFrame->Value = pValue;
		pFrame->Owned = iAttach > 0;
		if ( (pEvent->Depth > 0) && (iAttach == 0) ) {
			xrtValueRelease(pValue);
		}
	} else if ( pEvent->Depth > 0 ) {
		xrtValueRelease(pValue);
	} else if ( iAttach > 0 ) {
		xrtValueRelease(pValue);
	}
	return XTEXT_VALUE_VISIT_NEXT;
}



/* 释放失败解析留下的根和栈独立所有权。 */
static void __xrtJsonDomCleanup(xjsondombuilder* pBuilder)
{
	for ( size_t i = 0; i < XRT_VALUE_DEPTH_MAX; i++ ) {
		if ( pBuilder->Frames[i].Owned ) {
			xrtValueRelease(pBuilder->Frames[i].Value);
		}
	}
	xrtValueRelease(pBuilder->Root);
	pBuilder->Root = NULL;
}



/* 把共享事件类型显式映射为稳定的公开 JSON 事件类型。 */
static bool __xrtJsonEventType(
	xtextvalueeventtype Source,
	xjsoneventtype* pType
)
{
	switch ( Source ) {
		case XTEXT_VALUE_EVENT_NULL:
			*pType = XJSON_EVENT_NULL;
			break;
		case XTEXT_VALUE_EVENT_BOOL:
			*pType = XJSON_EVENT_BOOL;
			break;
		case XTEXT_VALUE_EVENT_INT:
			*pType = XJSON_EVENT_INT;
			break;
		case XTEXT_VALUE_EVENT_FLOAT:
			*pType = XJSON_EVENT_FLOAT;
			break;
		case XTEXT_VALUE_EVENT_STRING:
			*pType = XJSON_EVENT_STRING;
			break;
		case XTEXT_VALUE_EVENT_ARRAY_BEGIN:
			*pType = XJSON_EVENT_ARRAY_BEGIN;
			break;
		case XTEXT_VALUE_EVENT_ARRAY_END:
			*pType = XJSON_EVENT_ARRAY_END;
			break;
		case XTEXT_VALUE_EVENT_OBJECT_BEGIN:
			*pType = XJSON_EVENT_OBJECT_BEGIN;
			break;
		case XTEXT_VALUE_EVENT_OBJECT_END:
			*pType = XJSON_EVENT_OBJECT_END;
			break;
		default:
			return false;
	}
	return true;
}



/* 把共享事件转换成不暴露内部类型的 JSON 事件。 */
static xtextvaluevisitaction __xrtJsonVisitAdapter(
	const xtextvalueevent* pSource,
	ptr pUserData
)
{
	xjsonvisitadapter* pAdapter = (xjsonvisitadapter*)pUserData;
	xjsonevent Event;
	xjsonvisitaction Action;

	memset(&Event, 0, sizeof(Event));
	if ( !__xrtJsonEventType(pSource->Type, &Event.Type) ) {
		return XTEXT_VALUE_VISIT_FAIL;
	}
	Event.Location = __xrtJsonLocation(&pSource->Location);
	Event.Depth = pSource->Depth;
	Event.Raw = pSource->Raw;
	if ( pSource->Key.Type == XVALUE_KEY_STRING ) {
		Event.HasName = true;
		Event.Name = pSource->Key.String;
	} else if ( pSource->Key.Type == XVALUE_KEY_INDEX ) {
		Event.Index = pSource->Key.Index;
	}
	if ( pSource->Type == XTEXT_VALUE_EVENT_BOOL ) {
		Event.Value.Boolean = pSource->Value.Boolean;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_INT ) {
		Event.Value.Integer = pSource->Value.Integer;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_FLOAT ) {
		Event.Value.Float = pSource->Value.Float;
	} else if ( pSource->Type == XTEXT_VALUE_EVENT_STRING ) {
		Event.Value.String = pSource->Value.String;
	}
	Action = pAdapter->Visitor(&Event, pAdapter->UserData);
	return (xtextvaluevisitaction)Action;
}



/* 初始化严格且带安全资源预算的 JSON 读取配置。 */
XRT_API void xrtJsonReadConfigInit(xjsonreadconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Duplicate = XJSON_DUPLICATE_REJECT;
	pConfig->BigInteger = XJSON_BIGINT_REJECT;
	pConfig->MaxDepth = XJSON_DEPTH_DEFAULT;
	pConfig->MaxInputBytes = XJSON_INPUT_DEFAULT;
	pConfig->MaxStringBytes = XJSON_STRING_DEFAULT;
	pConfig->MaxValues = XJSON_VALUES_DEFAULT;
	pConfig->MaxContainerItems = XJSON_CONTAINER_DEFAULT;
}



/* 使用高级配置解析完整 JSON 文本。 */
XRT_API xvalue* xrtJsonRead(
	xstrview Text,
	const xjsonreadconfig* pConfig
)
{
	xjsondombuilder Builder;
	xtextvaluereadconfig TextConfig;
	xtextvaluevisitresult Result;

	if ( !__xrtJsonReadConfigValid(pConfig) ) {
		return NULL;
	}
	memset(&Builder, 0, sizeof(Builder));
	Builder.Config = *pConfig;
	TextConfig = __xrtJsonReadTextConfig(pConfig);
	Result = __xrtTextValueRead(
		Text,
		&TextConfig,
		__xrtJsonDomVisit,
		&Builder,
		__xrtJsonReadError,
		NULL,
		true
	);
	if ( Result != XTEXT_VALUE_VISIT_DONE ) {
		__xrtJsonDomCleanup(&Builder);
		return NULL;
	}
	return Builder.Root;
}



/* 使用默认严格配置解析完整 JSON 文本。 */
XRT_API xvalue* xrtJsonParse(xstrview Text)
{
	xjsonreadconfig Config;

	xrtJsonReadConfigInit(&Config);
	return xrtJsonRead(Text, &Config);
}



/* 使用默认严格配置验证语法，不分配字符串或 DOM。 */
XRT_API bool xrtJsonValid(xstrview Text)
{
	xjsonreadconfig Config;
	xtextvaluereadconfig TextConfig;

	xrtJsonReadConfigInit(&Config);
	TextConfig = __xrtJsonReadTextConfig(&Config);
	return __xrtTextValueRead(
		Text,
		&TextConfig,
		NULL,
		NULL,
		__xrtJsonReadError,
		NULL,
		false
	) == XTEXT_VALUE_VISIT_DONE;
}



/* 直接访问解析事件，不构建中间 DOM。 */
XRT_API xjsonvisitresult xrtJsonVisit(
	xstrview Text,
	const xjsonreadconfig* pConfig,
	xjsonvisitproc pVisitor,
	ptr pUserData
)
{
	xjsonvisitadapter Adapter;
	xtextvaluereadconfig TextConfig;
	xtextvaluevisitresult Result;

	if ( (pVisitor == NULL) || !__xrtJsonReadConfigValid(pConfig) ) {
		if ( pVisitor == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XJSON_VISIT_ERROR;
	}
	Adapter.Visitor = pVisitor;
	Adapter.UserData = pUserData;
	TextConfig = __xrtJsonReadTextConfig(pConfig);
	Result = __xrtTextValueRead(
		Text,
		&TextConfig,
		__xrtJsonVisitAdapter,
		&Adapter,
		__xrtJsonReadError,
		NULL,
		true
	);
	return (xjsonvisitresult)Result;
}

#endif
