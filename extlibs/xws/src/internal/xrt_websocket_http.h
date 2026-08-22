#ifndef XRT_INTERNAL_WEBSOCKET_HTTP_H
#define XRT_INTERNAL_WEBSOCKET_HTTP_H

#include "xrt_websocket.h"

#include <xrt/http_upgrade.h>
#include <xrt/websocket_http.h>



#if defined(XWS_FEATURE_WEBSOCKET_SERVER) || \
	defined(XWS_FEATURE_WEBSOCKET_CLIENT)

typedef struct __xrt_ws_fields {
	const xhttpfield* Data;
	size_t Count;
} __xrt_ws_fields;



/* 验证 HTTP 适配层公开入口使用的完整对象范围。 */
static inline bool __xrtWsHttpRangeCheck(
	const void* pObject,
	size_t iSize,
	cstr sOperation,
	cstr sMessage
)
{
	if ( xrtMemRangeValid(pObject, iSize) ) {
		return true;
	}
	__xwsHandshakeError(
		XERR_ARGUMENT,
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		sOperation,
		sMessage
	);
	return false;
}



/* 验证不透明 HTTP 对象存在，不依赖扩展库之外的对象布局。 */
static inline bool __xrtWsHttpObjectCheck(
	const void* pObject,
	cstr sOperation,
	cstr sMessage
)
{
	if ( xrtMemRangeValid(pObject, sizeof(ptr)) &&
		(((uintptr_t)pObject & (sizeof(ptr) - 1u)) == 0u) ) {
		return true;
	}
	__xwsHandshakeError(
		XERR_ARGUMENT,
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		sOperation,
		sMessage
	);
	return false;
}



/* 把可选事件表复制到对齐的独立快照。 */
static inline bool __xrtWsConnEventsSnapshot(
	xwsconnevents* pOutput,
	const xwsconnevents* pInput,
	cstr sOperation
)
{
	memset(pOutput, 0, sizeof(*pOutput));
	if ( pInput == NULL ) {
		return true;
	}
	if ( !__xrtWsHttpRangeCheck(
		pInput,
		sizeof(*pInput),
		sOperation,
		"WebSocket connection event range is invalid"
	) ) {
		return false;
	}
	memcpy(pOutput, pInput, sizeof(*pOutput));
	return true;
}



#if defined(XWS_FEATURE_WEBSOCKET_CLIENT)

/* 把可选客户端配置复制到对齐的默认值快照。 */
static inline bool __xrtWsClientConfigSnapshot(
	xwsclientconfig* pOutput,
	const xwsclientconfig* pInput,
	cstr sOperation
)
{
	xrtWsClientConfigInit(pOutput);
	if ( pInput == NULL ) {
		return true;
	}
	if ( !__xrtWsHttpRangeCheck(
		pInput,
		sizeof(*pInput),
		sOperation,
		"WebSocket client configuration range is invalid"
	) ) {
		return false;
	}
	memcpy(pOutput, pInput, sizeof(*pOutput));
	return true;
}

#endif



#if defined(XWS_FEATURE_WEBSOCKET_SERVER)

/* 把可选服务端配置复制到对齐的默认值快照。 */
static inline bool __xrtWsServerConfigSnapshot(
	xwsserverconfig* pOutput,
	const xwsserverconfig* pInput,
	cstr sOperation
)
{
	xrtWsServerConfigInit(pOutput);
	if ( pInput == NULL ) {
		return true;
	}
	if ( !__xrtWsHttpRangeCheck(
		pInput,
		sizeof(*pInput),
		sOperation,
		"WebSocket server configuration range is invalid"
	) ) {
		return false;
	}
	memcpy(pOutput, pInput, sizeof(*pOutput));
	return true;
}

#endif



/* 按大小写敏感规则比较两个协议文本视图。 */
static inline bool __xrtWsHttpTextEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 统计同名字段，不借出容器内部结构。 */
static inline size_t __xrtWsHttpFieldCount(
	const __xrt_ws_fields* pFields,
	xstrview Name
)
{
	return xrtHttpFieldCount(
		pFields->Data,
		pFields->Count,
		Name
	);
}



/* 返回唯一同名字段；缺失或重复都返回空指针。 */
static inline const xhttpfield* __xrtWsHttpFieldUnique(
	const __xrt_ws_fields* pFields,
	xstrview Name
)
{
	const xhttpfield* pFound = NULL;

	if ( xrtHttpFieldGetUnique(
		pFields->Data,
		pFields->Count,
		Name,
		&pFound
	) != XHTTP_NEXT_ITEM ) {
		return NULL;
	}
	return pFound;
}



/* 验证全部同名 token-list 字段并查找目标 token。 */
static inline bool __xrtWsHttpTokenHas(
	const __xrt_ws_fields* pFields,
	xstrview Name,
	xstrview Token,
	bool* pFound
)
{
	bool bPresent = false;
	bool bFound = false;

	for ( size_t i = 0; i < pFields->Count; i++ ) {
		const xhttpfield* pField = &pFields->Data[i];
		size_t iCount;

		if ( !xrtHttpFieldNameEqual(pField->Name, Name) ) {
			continue;
		}
		bPresent = true;
		if ( !xrtHttpTokenListCount(
			pField->Value,
			&iCount
		) || (iCount == 0) ) {
			return false;
		}
		if ( xrtHttpTokenListHas(
			pField->Value,
			Token
		) ) {
			bFound = true;
		}
	}
	*pFound = bPresent && bFound;
	return true;
}



/* 验证唯一字段只包含指定 token。 */
static inline bool __xrtWsHttpTokenExact(
	const __xrt_ws_fields* pFields,
	xstrview Name,
	xstrview Token
)
{
	const xhttpfield* pField =
		__xrtWsHttpFieldUnique(pFields, Name);
	size_t iCount;

	return (pField != NULL) &&
		xrtHttpTokenListCount(pField->Value, &iCount) &&
		(iCount == 1) &&
		xrtHttpTokenListHas(pField->Value, Token);
}




/* 验证 Upgrade 列表完整合法且至少包含一个无版本 websocket。 */
static inline bool __xrtWsHttpUpgradeHas(
	const __xrt_ws_fields* pFields
)
{
	xhttpupgradefieldcursor Cursor;
	xhttpupgradeitem Upgrade;
	xhttpnext Next;
	bool bFound = false;

	xrtHttpUpgradeFieldCursorInit(&Cursor);
	while ( (Next = xrtHttpUpgradeFieldNext(
		pFields->Data,
		pFields->Count,
		&Cursor,
		&Upgrade
	)) == XHTTP_NEXT_ITEM ) {
		if ( (Upgrade.Version.Size == 0) &&
			xrtHttpTokenEqual(
				Upgrade.Protocol,
				XRT_STR_LITERAL("websocket")
			) ) {
			bFound = true;
		}
	}
	return (Next == XHTTP_NEXT_END) && bFound;
}



/* 验证响应只选择唯一一个无版本 websocket 升级协议。 */
static inline bool __xrtWsHttpUpgradeExact(
	const __xrt_ws_fields* pFields
)
{
	xhttpupgradefieldcursor Cursor;
	xhttpupgradeitem Upgrade;
	xhttpnext Next;
	size_t iCount = 0;
	bool bMatch = false;

	xrtHttpUpgradeFieldCursorInit(&Cursor);
	while ( (Next = xrtHttpUpgradeFieldNext(
		pFields->Data,
		pFields->Count,
		&Cursor,
		&Upgrade
	)) == XHTTP_NEXT_ITEM ) {
		if ( iCount == SIZE_MAX ) {
			return false;
		}
		iCount++;
		bMatch = (Upgrade.Version.Size == 0) &&
			xrtHttpTokenEqual(
				Upgrade.Protocol,
				XRT_STR_LITERAL("websocket")
			);
	}
	return (Next == XHTTP_NEXT_END) &&
		(iCount == 1u) && bMatch;
}



#if defined(XRT_FEATURE_WEBSOCKET_EXTENSION)

typedef bool (*__xrt_ws_extension_visit)(
	const xwsextension* pExtension,
	ptr pData
);



/* 把重复的扩展字段视为一份列表，并依次访问每个严格解析的扩展项。 */
static inline bool __xrtWsHttpExtensionsVisit(
	const __xrt_ws_fields* pFields,
	__xrt_ws_extension_visit pVisit,
	ptr pData
)
{
	for ( size_t i = 0; i < pFields->Count; i++ ) {
		const xhttpfield* pField = &pFields->Data[i];
		size_t iOffset = 0;

		if ( !xrtHttpFieldNameEqual(
				pField->Name,
				XRT_STR_LITERAL(
					"Sec-WebSocket-Extensions"
				)
			) ) {
			continue;
		}
		for ( ;; ) {
			xwsextension Extension;
			xhttpnext Next = xrtWsExtensionNext(
				pField->Value,
				&iOffset,
				&Extension
			);

			if ( Next == XHTTP_NEXT_ERROR ) {
				return false;
			}
			if ( Next == XHTTP_NEXT_END ) {
				break;
			}
			if ( !pVisit(&Extension, pData) ) {
				return false;
			}
		}
	}
	return true;
}

#endif

#endif

#endif
