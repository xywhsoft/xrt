#ifndef XWS_SERVER_INTERNAL_HTTP_H
#define XWS_SERVER_INTERNAL_HTTP_H

#include <string.h>
#include <xrt/websocket_server.h>
#include <xrt/memory.h>

/* 构建并安装扩展库自己的结构化错误。 */
static inline void __xwsServerErrorSetDetail(
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
static inline void __xwsServerErrorWrapDetail(
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
static inline void __xwsServerHandshakeError(
	xerrkind Kind,
	xwshandshakeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsServerErrorSetDetail(
		Kind,
		"xrt.websocket.handshake",
		(int32)Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* 将当前底层错误包装为稳定的 WebSocket 握手错误。 */
static inline void __xwsServerHandshakeWrap(
	xerrkind DefaultKind,
	xwshandshakeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsServerErrorWrapDetail(
		DefaultKind,
		"xrt.websocket.handshake",
		(int32)Code,
		sOperation,
		sMessage
	);
}

#endif



#include <xrt/http_upgrade.h>
#include <xrt/websocket_server.h>



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER)

typedef struct __xws_server_fields {
	const xhttpfield* Data;
	size_t Count;
} __xws_server_fields;



/* 验证 HTTP 适配层公开入口使用的完整对象范围。 */
static inline bool __xwsServerHttpRangeCheck(
	const void* pObject,
	size_t iSize,
	cstr sOperation,
	cstr sMessage
)
{
	if ( xrtMemRangeValid(pObject, iSize) ) {
		return true;
	}
	__xwsServerHandshakeError(
		XERR_ARGUMENT,
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		sOperation,
		sMessage
	);
	return false;
}



/* 验证不透明 HTTP 对象存在，不依赖扩展库之外的对象布局。 */
static inline bool __xwsServerHttpObjectCheck(
	const void* pObject,
	cstr sOperation,
	cstr sMessage
)
{
	if ( xrtMemRangeValid(pObject, sizeof(ptr)) &&
		(((uintptr_t)pObject & (sizeof(ptr) - 1u)) == 0u) ) {
		return true;
	}
	__xwsServerHandshakeError(
		XERR_ARGUMENT,
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		sOperation,
		sMessage
	);
	return false;
}



/* 把可选事件表复制到对齐的独立快照。 */
static inline bool __xwsServerConnEventsSnapshot(
	xwsconnevents* pOutput,
	const xwsconnevents* pInput,
	cstr sOperation
)
{
	memset(pOutput, 0, sizeof(*pOutput));
	if ( pInput == NULL ) {
		return true;
	}
	if ( !__xwsServerHttpRangeCheck(
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



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER)

/* 把可选服务端配置复制到对齐的默认值快照。 */
static inline bool __xwsServerServerConfigSnapshot(
	xwsserverconfig* pOutput,
	const xwsserverconfig* pInput,
	cstr sOperation
)
{
	xrtWsServerConfigInit(pOutput);
	if ( pInput == NULL ) {
		return true;
	}
	if ( !__xwsServerHttpRangeCheck(
		pInput,
		sizeof(*pInput),
		sOperation,
		"WebSocket server configuration range is invalid"
	) ) {
		return false;
	}
	memcpy(pOutput, pInput, sizeof(*pOutput));
	#if !defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		if (pOutput->EnableDeflate || pOutput->RequireDeflate) {
			__xwsServerHandshakeError(XERR_VALUE, XWS_HANDSHAKE_ERROR_EXTENSION, sOperation, "WebSocket compression support is not selected");
			return false;
		}
	#endif
	return true;
}

#endif



/* 按大小写敏感规则比较两个协议文本视图。 */
static inline bool __xwsServerHttpTextEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 统计同名字段，不借出容器内部结构。 */
static inline size_t __xwsServerHttpFieldCount(
	const __xws_server_fields* pFields,
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
static inline const xhttpfield* __xwsServerHttpFieldUnique(
	const __xws_server_fields* pFields,
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
static inline bool __xwsServerHttpTokenHas(
	const __xws_server_fields* pFields,
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
static inline bool __xwsServerHttpTokenExact(
	const __xws_server_fields* pFields,
	xstrview Name,
	xstrview Token
)
{
	const xhttpfield* pField =
		__xwsServerHttpFieldUnique(pFields, Name);
	size_t iCount;

	return (pField != NULL) &&
		xrtHttpTokenListCount(pField->Value, &iCount) &&
		(iCount == 1) &&
		xrtHttpTokenListHas(pField->Value, Token);
}



/* 验证 Upgrade 列表完整合法且至少包含一个无版本 websocket。 */
static inline bool __xwsServerHttpUpgradeHas(
	const __xws_server_fields* pFields
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



#if defined(XRT_FEATURE_WEBSOCKET_EXTENSION)

typedef bool (*__xws_server_extension_visit)(
	const xwsextension* pExtension,
	ptr pData
);



/* 把重复的扩展字段视为一份列表，并依次访问每个严格解析的扩展项。 */
static inline bool __xwsServerHttpExtensionsVisit(
	const __xws_server_fields* pFields,
	__xws_server_extension_visit pVisit,
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
