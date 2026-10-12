#ifndef XWS_CLIENT_INTERNAL_HTTP_H
#define XWS_CLIENT_INTERNAL_HTTP_H

#include <string.h>
#include <xrt/websocket_client.h>
#include <xrt/memory.h>

/* 构建并安装扩展库自己的结构化错误。 */
static inline void __xwsClientErrorSetDetail(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = sDomain;
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 取走当前原因链，并用扩展库稳定域包装后重新安装。 */
static inline void __xwsClientErrorWrapDetail(
	xerrkind DefaultKind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ?
		xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = sDomain;
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



#if defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE)

/* 设置 WebSocket 握手层的结构化错误。 */
static inline void __xwsClientHandshakeError(
	xerrkind Kind,
	xwshandshakeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsClientErrorSetDetail(
		Kind,
		"xrt.websocket.handshake",
		(int32)Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* 将当前底层错误包装为稳定的 WebSocket 握手错误。 */
static inline void __xwsClientHandshakeWrap(
	xerrkind DefaultKind,
	xwshandshakeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsClientErrorWrapDetail(
		DefaultKind,
		"xrt.websocket.handshake",
		(int32)Code,
		sOperation,
		sMessage
	);
}

#endif



#include <xrt/http_upgrade.h>
#include <xrt/websocket_client.h>



#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT)

typedef struct __xws_client_fields {
	const xhttpfield* Data;
	size_t Count;
} __xws_client_fields;



/* 验证 HTTP 适配层公开入口使用的完整对象范围。 */
static inline bool __xwsClientHttpRangeCheck(
	const void* pObject,
	size_t iSize,
	cstr sOperation,
	cstr sMessage
)
{
	if ( xrtMemRangeValid(pObject, iSize) ) {
		return true;
	}
	__xwsClientHandshakeError(
		XERR_ARGUMENT,
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		sOperation,
		sMessage
	);
	return false;
}



/* 验证不透明 HTTP 对象存在，不依赖扩展库之外的对象布局。 */
static inline bool __xwsClientHttpObjectCheck(
	const void* pObject,
	cstr sOperation,
	cstr sMessage
)
{
	if ( xrtMemRangeValid(pObject, sizeof(ptr)) &&
		(((uintptr_t)pObject & (sizeof(ptr) - 1u)) == 0u) ) {
		return true;
	}
	__xwsClientHandshakeError(
		XERR_ARGUMENT,
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		sOperation,
		sMessage
	);
	return false;
}



/* 把可选事件表复制到对齐的独立快照。 */
static inline bool __xwsClientConnEventsSnapshot(
	xwsconnevents* pOutput,
	const xwsconnevents* pInput,
	cstr sOperation
)
{
	memset(pOutput, 0, sizeof(*pOutput));
	if ( pInput == NULL ) {
		return true;
	}
	if ( !__xwsClientHttpRangeCheck(
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



#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT)

/* 把可选客户端配置复制到对齐的默认值快照。 */
static inline bool __xwsClientClientConfigSnapshot(
	xwsclientconfig* pOutput,
	const xwsclientconfig* pInput,
	cstr sOperation
)
{
	xrtWsClientConfigInit(pOutput);
	if ( pInput == NULL ) {
		return true;
	}
	if ( !__xwsClientHttpRangeCheck(
		pInput,
		sizeof(*pInput),
		sOperation,
		"WebSocket client configuration range is invalid"
	) ) {
		return false;
	}
	memcpy(pOutput, pInput, sizeof(*pOutput));
	#if !defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		if (pOutput->EnableDeflate || pOutput->RequireDeflate) {
			__xwsClientHandshakeError(XERR_VALUE, XWS_HANDSHAKE_ERROR_EXTENSION, sOperation, "WebSocket compression support is not selected");
			return false;
		}
	#endif
	return true;
}

#endif



/* 按大小写敏感规则比较两个协议文本视图。 */
static inline bool __xwsClientHttpTextEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 统计同名字段，不借出容器内部结构。 */
static inline size_t __xwsClientHttpFieldCount(
	const __xws_client_fields* pFields,
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
static inline const xhttpfield* __xwsClientHttpFieldUnique(
	const __xws_client_fields* pFields,
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
static inline bool __xwsClientHttpTokenHas(
	const __xws_client_fields* pFields,
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



/* 验证 Upgrade 列表完整合法且至少包含一个无版本 websocket。 */



/* 验证响应只选择唯一一个无版本 websocket 升级协议。 */
static inline bool __xwsClientHttpUpgradeExact(
	const __xws_client_fields* pFields
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

typedef bool (*__xws_client_extension_visit)(
	const xwsextension* pExtension,
	ptr pData
);



/* 把重复的扩展字段视为一份列表，并依次访问每个严格解析的扩展项。 */
static inline bool __xwsClientHttpExtensionsVisit(
	const __xws_client_fields* pFields,
	__xws_client_extension_visit pVisit,
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
