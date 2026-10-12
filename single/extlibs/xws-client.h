/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xws_client requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XWS_CLIENT_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XWS_CLIENT_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XWS_CLIENT_SINGLE_HEADER_H
#define XWS_CLIENT_SINGLE_HEADER_H
#define XWS_CLIENT_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xws-client/include/xws-client/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XWS_CLIENT_FEATURES_H
#define XWS_CLIENT_FEATURES_H

/* websocket_client_https 及其直接依赖。 */
#if defined(XWS_CLIENT_MODULE_ALL) || defined(XWS_CLIENT_MODULE_WEBSOCKET_CLIENT_HTTPS)
#ifndef XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_HTTPS
#define XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_HTTPS
#endif
#ifndef XWS_CLIENT_MODULE_WEBSOCKET_CLIENT
#define XWS_CLIENT_MODULE_WEBSOCKET_CLIENT
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION_TLS
#define XWS_MODULE_WEBSOCKET_CONNECTION_TLS
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT_HTTPS
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_HTTPS
#endif
#endif

/* websocket_client_deflate 及其直接依赖。 */
#if defined(XWS_CLIENT_MODULE_ALL) || defined(XWS_CLIENT_MODULE_WEBSOCKET_CLIENT_DEFLATE)
#ifndef XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE
#define XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE
#endif
#ifndef XWS_CLIENT_MODULE_WEBSOCKET_CLIENT
#define XWS_CLIENT_MODULE_WEBSOCKET_CLIENT
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION_DEFLATE
#define XWS_MODULE_WEBSOCKET_CONNECTION_DEFLATE
#endif
#ifndef XRT_MODULE_WEBSOCKET_DEFLATE
#define XRT_MODULE_WEBSOCKET_DEFLATE
#endif
#endif

/* websocket_client_future 及其直接依赖。 */
#if defined(XWS_CLIENT_MODULE_ALL) || defined(XWS_CLIENT_MODULE_WEBSOCKET_CLIENT_FUTURE)
#ifndef XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE
#define XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE
#endif
#ifndef XWS_CLIENT_MODULE_WEBSOCKET_CLIENT
#define XWS_CLIENT_MODULE_WEBSOCKET_CLIENT
#endif
#ifndef XRT_MODULE_FUTURE_BRIDGE
#define XRT_MODULE_FUTURE_BRIDGE
#endif
#endif

/* websocket_client 及其直接依赖。 */
#if defined(XWS_CLIENT_MODULE_ALL) || defined(XWS_CLIENT_MODULE_WEBSOCKET_CLIENT)
#ifndef XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT
#define XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT
#endif
#ifndef XRT_MODULE_WEBSOCKET_KEYGEN
#define XRT_MODULE_WEBSOCKET_KEYGEN
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION
#define XWS_MODULE_WEBSOCKET_CONNECTION
#endif
#ifndef XRT_MODULE_HTTP_UPGRADE
#define XRT_MODULE_HTTP_UPGRADE
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT
#endif
#endif

#endif /* XWS_CLIENT_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xws-client/include/xws-client/export.h */
/* ========================================================================== */

#ifndef XWS_CLIENT_EXPORT_H
#define XWS_CLIENT_EXPORT_H


/* 独立动态库只导出本库接口，依赖由宿主共同链接。 */
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XWS_CLIENT_BUILD_SHARED)
  #define XWS_CLIENT_API __declspec(dllexport)
 #elif defined(XWS_CLIENT_USE_SHARED)
  #define XWS_CLIENT_API __declspec(dllimport)
 #else
  #define XWS_CLIENT_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XWS_CLIENT_BUILD_SHARED)
  #define XWS_CLIENT_API __attribute__((visibility("default")))
 #else
  #define XWS_CLIENT_API
 #endif
#else
 #define XWS_CLIENT_API
#endif

#endif


/* ========================================================================== */
/* public: extlibs/xws-client/include/xrt/websocket_client.h */
/* ========================================================================== */

#ifndef XWS_CLIENT_H_API
#define XWS_CLIENT_H_API



#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT)
#endif




#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT) && \
	(!defined(XRT_FEATURE_WEBSOCKET_KEYGEN) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XHTTP_FEATURE_HTTP_CLIENT))
	#error "XRT WebSocket client requires keygen, connection and HTTP client"
#endif


#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_HTTPS) && \
	(!defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS) || \
	 !defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS))
	#error "XRT WebSocket HTTPS client requires client, TLS connection and HTTPS client"
#endif


#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE) && \
	(!defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE) || \
	 !defined(XRT_FEATURE_WEBSOCKET_DEFLATE))
	#error "XRT WebSocket Deflate client requires client, compressed connection and negotiation"
#endif









#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT)

/*
	Protocols 和 Http 内的借用对象只需覆盖 Connect 调用。
	内部请求、协议列表、连接配置和事件表都会在返回前取得独立快照。
*/
typedef struct xwsclientconfig {
	xwsconnconfig Connection;
	xhttpcalloptions Http;
	xstrview Protocols;
	xwsdeflate Deflate;
	bool EnableDeflate;
	bool RequireDeflate;

} xwsclientconfig;



/* 客户端握手快照借用 Response 的子协议，并拥有扩展协商参数。 */
typedef struct xwsclienthandshake {
	xstrview Protocol;
	xwsdeflate Deflate;
	bool DeflateEnabled;

} xwsclienthandshake;



/*
	完成回调在 HTTP Call 的网络 Worker 上发布一次。
	Response 无论握手成功失败都把非空调用方引用转移给回调；
	成功时 Connection 调用方引用同时转移，Error 只在回调期间借用。
*/
typedef void (*xwsconnectproc)(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
);

#endif



XRT_EXTERN_C_BEGIN






#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT)

/* 在完整、允许未对齐的存储中初始化客户端默认配置。 */
XWS_CLIENT_API void xrtWsClientConfigInit(
	xwsclientconfig* pConfig
);



/*
	创建可继续添加自定义 Header 的基础 GET 请求。
	Url 接受 ws、wss、http 或 https，并映射为 HTTP Client 可执行的 URL；
	Url 不得包含 userinfo 或 fragment；该函数不写入任何 WebSocket 握手管理字段。
*/
XWS_CLIENT_API xhttprequest* xrtWsRequestCreate(
	xstrview Url
);



/*
	创建 GET WebSocket 请求；Url 接受 ws、wss、http 或 https。
	Url 不得包含 userinfo 或 fragment；资源名中的井号必须编码为 %23。
	配置先复制为局部快照；成功时 Key 写入完整的固定输出。
*/
XWS_CLIENT_API xhttprequest* xrtWsClientRequestCreate(
	xstrview Url,
	const xwsclientconfig* pConfig,
	char sKey[XWS_KEY_CAPACITY]
);



/*
	克隆自定义 GET 请求并原子替换 WebSocket 管理字段。
	配置先复制为局部快照；Key 必须覆盖完整固定输出；
	源请求不得包含 userinfo、fragment、正文、分帧字段或扩展字段。
*/
XWS_CLIENT_API xhttprequest* xrtWsClientRequestClone(
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	char sKey[XWS_KEY_CAPACITY]
);



/*
	严格验证 101 响应、Accept、所选子协议和扩展响应。
	配置和输出允许未对齐，输出不得覆盖 Response、Key 或配置；
	Handshake 的子协议借用 Response，失败时保持不变。
*/
XWS_CLIENT_API bool xrtWsClientCheck(
	const xhttpresponse* pResponse,
	xstrview Key,
	const xwsclientconfig* pConfig,
	xwsclienthandshake* pHandshake
);



/*
	快照配置和事件表后，使用 ws/wss URL 异步执行 WebSocket 握手。
	EventData 属于 Connection 事件，Data 属于完成回调，生命周期彼此独立。
*/
XWS_CLIENT_API xhttpcall* xrtWsConnect(
	xhttpclient* pClient,
	xstrview Url,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsconnectproc pProc,
	ptr pData
);



/*
	以自定义 GET 请求为基础异步握手。
	配置和事件表在任何分配或 HTTP 提交前复制为局部快照。
	管理字段由适配器替换，其它 Header、代理、Cookie 和超时策略继续保留。
	EventData 与 Data 分别交给 Connection 事件和完成回调。
*/
XWS_CLIENT_API xhttpcall* xrtWsConnectRequest(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsconnectproc pProc,
	ptr pData
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xws-client/include/xrt/websocket_client_future.h */
/* ========================================================================== */

#ifndef XWS_CLIENT_FUTURE_H_API
#define XWS_CLIENT_FUTURE_H_API


#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)
#endif




#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE) && \
	(!defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT) || \
	 !defined(XRT_FEATURE_FUTURE) || \
	 !defined(XRT_FEATURE_FUTURE_BRIDGE))
	#error "XRT WebSocket client Future requires client, result and Future bridge support"
#endif





#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)

/*
	连接建立结果拥有 WebSocket Connection。
	客户端 Future 启用时，成功结果还拥有对应的 101 Response。
*/
typedef struct xwsclientresult xwsclientresult;



/* 连接建立结果错误码只描述结果对象本身，不依赖 HTTP 握手实现。 */
typedef enum xwsclientresulterror {
	XWS_CLIENT_RESULT_ERROR_ARGUMENT = 1,
	XWS_CLIENT_RESULT_ERROR_STATE
} xwsclientresulterror;



XRT_EXTERN_C_BEGIN



/*
	增加连接建立结果引用并返回原指针。
	空指针、无效对象范围或不可再增加的引用状态返回 NULL 并记录错误。
*/
XWS_CLIENT_API xwsclientresult* xrtWsClientResultRef(
	xwsclientresult* pResult
);



/*
	释放结果引用。
	最后一个引用会中止尚未取走的 Connection，并销毁尚未取走的 Response。
	NULL 是空操作；非空但无效的对象范围会记录参数错误。
*/
XWS_CLIENT_API void xrtWsClientResultDestroy(
	xwsclientresult* pResult
);



/*
	返回结果借用的 Connection；结果有效且 Connection 未被取走期间可用。
	空指针或无效对象范围返回 NULL 并记录参数错误。
*/
XWS_CLIENT_API xwsconn* xrtWsClientResultConnection(
	const xwsclientresult* pResult
);



/*
	原子取走 Connection 所有权。
	调用方必须在使用结束后关闭或中止 Connection，并释放其调用方引用。
	同一结果的 Take 与借用访问不可并发。
	空指针或无效对象范围返回 NULL 并记录参数错误。
*/
XWS_CLIENT_API xwsconn* xrtWsClientResultTakeConnection(
	xwsclientresult* pResult
);



#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)

/* 返回客户端结果借用的 101 Response；无效结果返回 NULL 并记录参数错误。 */
XWS_CLIENT_API const xhttpresponse* xrtWsClientResultResponse(
	const xwsclientresult* pResult
);



/*
	原子取走客户端 101 Response 所有权；同一结果的 Take 与借用访问不可并发。
	无效结果返回 NULL 并记录参数错误。
*/
XWS_CLIENT_API xhttpresponse* xrtWsClientResultTakeResponse(
	xwsclientresult* pResult
);



/*
	使用 ws/wss URL 异步建立 WebSocket。
	Future 成功值是其拥有的 xwsclientresult；取消会协作取消底层 HTTP Call。
	Client、Config 和 Events 在创建任何异步状态前验证完整对象范围。
	Connection 事件使用独立的 Data，不会看到 Future 内部桥接上下文。
*/
XWS_CLIENT_API xfuture* xrtWsConnectAsync(
	xhttpclient* pClient,
	xstrview Url,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);



/* 以自定义 GET 请求为基础异步建立 WebSocket；Request 同样先验证完整对象范围。 */
XWS_CLIENT_API xfuture* xrtWsConnectRequestAsync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);



/*
	阻塞建立 WebSocket 并返回拥有型结果。
	网络 Worker 不允许调用；协程应等待 xrtWsConnectAsync 返回的 Future。
*/
XWS_CLIENT_API xwsclientresult* xrtWsConnectSync(
	xhttpclient* pClient,
	xstrview Url,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);



/* 以自定义 GET 请求为基础阻塞建立 WebSocket。 */
XWS_CLIENT_API xwsclientresult* xrtWsConnectRequestSync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);

#endif






XRT_EXTERN_C_END

#endif


#endif


/* ========================================================================== */
/* public: extlibs/xws-client/include/xws-client.h */
/* ========================================================================== */

#ifndef XWS_CLIENT_H
#define XWS_CLIENT_H


#endif

#endif

#if defined(XWS_CLIENT_IMPLEMENTATION) && !defined(XWS_CLIENT_IMPLEMENTATION_ONCE)
#define XWS_CLIENT_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xws-client/src/internal/xrt_websocket_client.h */
/* ========================================================================== */

#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT)
#ifndef XWS_CLIENT_INTERNAL_HTTP_H
#define XWS_CLIENT_INTERNAL_HTTP_H

#include <string.h>

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
#endif


/* ========================================================================== */
/* internal: extlibs/xws-client/src/internal/xrt_websocket_client_future.h */
/* ========================================================================== */

#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)
#ifndef XWS_CLIENT_INTERNAL_FUTURE_H
#define XWS_CLIENT_INTERNAL_FUTURE_H





/* 本库的握手结果用原子槽管理可取走的所有权。 */
#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)

struct xwsclientresult {
	volatile int32 References;
	xatomicptr Connection;
	#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)
		xatomicptr Response;
	#endif
};



/* 中止并释放一个没有进入调用方所有权的 WebSocket Connection。 */
static inline void __xwsClientOpenConnectionDestroy(
	xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return;
	}
	(void)xrtWsConnAbort(pConnection);
	xrtWsConnDestroy(pConnection);
}



#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)

/* 创建 HTTP Future 桥接层自己的稳定握手错误。 */
static inline xerror* __xwsClientOpenErrorCreate(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.websocket.handshake";
	Desc.Code = XWS_HANDSHAKE_ERROR_UPGRADE;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	return xrtErrorBuild(&Desc);
}

#endif


#endif

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xws-client/src/websocket/client.c */
/* ========================================================================== */

#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT)



#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT)

typedef struct __xrt_ws_connect {
	xwsconnconfig Connection;
	xwsconnevents Events;
	xwsconnectproc Proc;
	ptr EventData;
	ptr Data;
	char Key[XWS_KEY_CAPACITY];
	xstrview Protocols;
	#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		xwsdeflate Deflate;
		bool EnableDeflate;
		bool RequireDeflate;
	#endif
	char Storage[];
} __xrt_ws_connect;



#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)

typedef struct __xrt_ws_client_deflate {
	const xwsdeflate* Offer;
	xwsdeflate Response;
	bool Found;
} __xrt_ws_client_deflate;



/* 客户端只接受唯一且确实由本次 offer 允许的 permessage-deflate 响应。 */
static bool __xrtWsClientDeflateVisit(
	const xwsextension* pExtension,
	ptr pData
)
{
	__xrt_ws_client_deflate* pDeflate =
		(__xrt_ws_client_deflate*)pData;

	if ( !xrtWsDeflateIs(pExtension) ) {
		__xwsClientHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-response",
			"WebSocket response selected an unsupported extension"
		);
		return false;
	}
	if ( pDeflate->Found ) {
		__xwsClientHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-response",
			"WebSocket response repeats permessage-deflate"
		);
		return false;
	}
	if ( !xrtWsDeflateResponseParse(
		pExtension,
		&pDeflate->Response
	) || !xrtWsDeflateResponseCheck(
		pDeflate->Offer,
		&pDeflate->Response
	) ) {
		__xwsClientHandshakeWrap(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"check-websocket-response",
			"WebSocket permessage-deflate response violates the offer"
		);
		return false;
	}
	pDeflate->Found = true;
	return true;
}



/* 验证扩展响应并把协商参数复制到独立握手快照。 */
static bool __xrtWsClientDeflateCheck(
	const __xws_client_fields* pFields,
	const xwsclientconfig* pConfig,
	xwsclienthandshake* pHandshake
)
{
	__xrt_ws_client_deflate Deflate;

	if ( !pConfig->EnableDeflate ) {
		if ( pConfig->RequireDeflate ) {
			__xwsClientHandshakeError(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-response",
				"Required WebSocket compression is not enabled"
			);
			return false;
		}
		if ( __xwsClientHttpFieldCount(
			pFields,
			XRT_STR_LITERAL(
				"Sec-WebSocket-Extensions"
			)
		) != 0 ) {
			__xwsClientHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-response",
				"WebSocket response selected an unoffered extension"
			);
			return false;
		}
		return true;
	}
	memset(&Deflate, 0, sizeof(Deflate));
	Deflate.Offer = &pConfig->Deflate;
	if ( !__xwsClientHttpExtensionsVisit(
		pFields,
		__xrtWsClientDeflateVisit,
		&Deflate
	) ) {
		return false;
	}
	if ( !Deflate.Found ) {
		if ( pConfig->RequireDeflate ) {
			__xwsClientHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-response",
				"WebSocket response omitted required compression"
			);
			return false;
		}
		return true;
	}
	pHandshake->Deflate = Deflate.Response;
	pHandshake->DeflateEnabled = true;
	return true;
}

#endif



/* 创建仅把 ws/wss scheme 映射到 HTTP 执行层的基础 GET 请求。 */
XWS_CLIENT_API xhttprequest* xrtWsRequestCreate(
	xstrview Url
)
{
	xurl Parsed;
	xhttprequest* pRequest;
	str sHttpUrl;
	xstrview HttpUrl;
	xstrview Scheme;
	size_t iSize;

	if ( (Url.Size == 0) ||
		!xrtMemRangeValid(Url.Data, Url.Size) ) {
		__xwsClientHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"create-websocket-request",
			"WebSocket URL view is empty or invalid"
		);
		return NULL;
	}
	if ( !xrtUrlParse(Url, &Parsed) ) {
		__xwsClientHandshakeWrap(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"create-websocket-request",
			"WebSocket URL is invalid"
		);
		return NULL;
	}
	if ( (Parsed.Flags &
		(XURL_HAS_USERINFO | XURL_HAS_FRAGMENT)) != 0 ) {
		__xwsClientHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"create-websocket-request",
			"WebSocket URL must not contain userinfo or a fragment"
		);
		return NULL;
	}
	if ( xrtUrlSchemeIs(
		&Parsed,
		XRT_STR_LITERAL("http")
	) || xrtUrlSchemeIs(
		&Parsed,
		XRT_STR_LITERAL("https")
	) ) {
		pRequest = xrtHttpRequestCreate(
			XRT_STR_LITERAL("GET"),
			Url
		);
		if ( pRequest == NULL ) {
			__xwsClientHandshakeWrap(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_ARGUMENT,
				"create-websocket-request",
				"WebSocket URL cannot be used by the HTTP client"
			);
		}
		return pRequest;
	}
	if ( xrtUrlSchemeIs(
		&Parsed,
		XRT_STR_LITERAL("ws")
	) ) {
		Scheme = XRT_STR_LITERAL("http");
	} else if ( xrtUrlSchemeIs(
		&Parsed,
		XRT_STR_LITERAL("wss")
	) ) {
		Scheme = XRT_STR_LITERAL("https");
	} else {
		__xwsClientHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"create-websocket-request",
			"WebSocket URL scheme must be ws, wss, http or https"
		);
		return NULL;
	}
	if ( Url.Size >
		(SIZE_MAX - (Scheme.Size - Parsed.Scheme.Size) - 1u) ) {
		__xwsClientHandshakeError(
			XERR_RANGE,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"create-websocket-request",
			"WebSocket URL is too large"
		);
		return NULL;
	}
	iSize = Url.Size + Scheme.Size - Parsed.Scheme.Size;
	sHttpUrl = (str)xrtMalloc(iSize + 1u);
	if ( sHttpUrl == NULL ) {
		__xwsClientHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"create-websocket-request",
			"WebSocket HTTP URL allocation failed"
		);
		return NULL;
	}
	memcpy(sHttpUrl, Scheme.Data, Scheme.Size);
	memcpy(
		sHttpUrl + Scheme.Size,
		Url.Data + Parsed.Scheme.Size,
		Url.Size - Parsed.Scheme.Size
	);
	sHttpUrl[iSize] = '\0';
	HttpUrl.Data = sHttpUrl;
	HttpUrl.Size = iSize;
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		HttpUrl
	);
	xrtFree(sHttpUrl);
	if ( pRequest == NULL ) {
		__xwsClientHandshakeWrap(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"create-websocket-request",
			"WebSocket URL cannot be used by the HTTP client"
		);
	}
	return pRequest;
}



/* 在私有请求副本上原子构造 RFC 6455 客户端管理字段。 */
static bool __xrtWsClientRequestConfigure(
	xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	char sKey[XWS_KEY_CAPACITY]
)
{
	const xurl* pUrl;
	char sGenerated[XWS_KEY_CAPACITY];
	xstrview Protocols;
	#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		char sExtensions[XWS_DEFLATE_MAX_SIZE];
		size_t iExtensions = 0;
	#endif

	if ( (pRequest == NULL) || (pConfig == NULL) ||
		(sKey == NULL) ||
		((pConfig->Protocols.Data == NULL) &&
		 (pConfig->Protocols.Size != 0)) ) {
		__xwsClientHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"configure-websocket-request",
			"WebSocket request, protocols or key output is invalid"
		);
		return false;
	}

	/* WebSocket URI 不允许 userinfo 或 fragment，包括显式空值。 */
	pUrl = xrtHttpRequestUrl(pRequest);
	if ( (pUrl == NULL) ||
		((pUrl->Flags &
		 (XURL_HAS_USERINFO | XURL_HAS_FRAGMENT)) != 0) ) {
		__xwsClientHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"configure-websocket-request",
			"WebSocket URL must not contain userinfo or a fragment"
		);
		return false;
	}
	Protocols = pConfig->Protocols;
	if ( !xrtWsProtocolsValid(Protocols) ) {
		__xwsClientHandshakeWrap(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"configure-websocket-request",
			"WebSocket client protocol list is invalid"
		);
		return false;
	}
	#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		if ( pConfig->RequireDeflate &&
			!pConfig->EnableDeflate ) {
			__xwsClientHandshakeError(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"configure-websocket-request",
				"Required WebSocket compression is not enabled"
			);
			return false;
		}
		if ( pConfig->EnableDeflate &&
			!xrtWsDeflateOfferWrite(
				&pConfig->Deflate,
				sExtensions,
				sizeof(sExtensions),
				&iExtensions
			) ) {
			__xwsClientHandshakeWrap(
				XERR_VALUE,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"configure-websocket-request",
				"WebSocket compression offer is invalid"
			);
			return false;
		}
	#endif
	if ( !__xwsClientHttpTextEqual(
		xrtHttpRequestMethod(pRequest),
		XRT_STR_LITERAL("GET")
	) ) {
		__xwsClientHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_METHOD,
			"configure-websocket-request",
			"WebSocket client request method must be GET"
		);
		return false;
	}
	if ( (xrtHttpRequestBody(pRequest) != NULL) ||
		(xrtHttpRequestHeader(
			pRequest,
			XRT_STR_LITERAL("Content-Length")
		) != NULL) || (xrtHttpRequestHeader(
			pRequest,
			XRT_STR_LITERAL("Transfer-Encoding")
		) != NULL) ) {
		__xwsClientHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_BODY,
			"configure-websocket-request",
			"WebSocket client request must not carry a body"
		);
		return false;
	}
	if ( xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Sec-WebSocket-Extensions")
	) != NULL ) {
		__xwsClientHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"configure-websocket-request",
			"WebSocket extensions require an explicit extension adapter"
		);
		return false;
	}
	if ( !xrtWsKeyGenerate(
		sGenerated,
		sizeof(sGenerated)
	) ) {
		return false;
	}
	if ( !xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("Upgrade"),
		XRT_STR_LITERAL("websocket")
	) || !xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("Upgrade")
	) || !xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("Sec-WebSocket-Version"),
		XRT_STR_LITERAL("13")
	) || !xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("Sec-WebSocket-Key"),
		(xstrview) { sGenerated, XWS_KEY_SIZE }
	) || ((Protocols.Size != 0) &&
		!xrtHttpRequestSetHeader(
			pRequest,
			XRT_STR_LITERAL("Sec-WebSocket-Protocol"),
			Protocols
		))
	#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		|| (pConfig->EnableDeflate &&
			!xrtHttpRequestSetHeader(
				pRequest,
				XRT_STR_LITERAL(
					"Sec-WebSocket-Extensions"
				),
				(xstrview) {
					sExtensions,
					iExtensions
				}
			))
	#endif
		) {
		__xwsClientHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"configure-websocket-request",
			"WebSocket request fields could not be stored"
		);
		return false;
	}
	if ( Protocols.Size == 0 ) {
		(void)xrtHttpRequestRemoveHeader(
			pRequest,
			XRT_STR_LITERAL("Sec-WebSocket-Protocol")
		);
	}
	memcpy(sKey, sGenerated, sizeof(sGenerated));
	return true;
}



/* 初始化客户端握手与已建立连接的默认配置。 */
XWS_CLIENT_API void xrtWsClientConfigInit(
	xwsclientconfig* pConfig
)
{
	xwsclientconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		__xwsClientHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"config-init-websocket-client",
			"WebSocket client configuration range is invalid"
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtWsConnConfigInit(&Config.Connection);
	Config.Connection.Role = XWS_ROLE_CLIENT;
	xrtHttpCallOptionsInit(&Config.Http);
	#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		xrtWsDeflateInit(&Config.Deflate);
	#else
		Config.Deflate = Config.Connection.Deflate;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		Config.Http.Redirect =
			XHTTP_REDIRECT_MANUAL;
	#endif
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 从 ws/wss 或 HTTP URL 创建完整客户端握手请求。 */
XWS_CLIENT_API xhttprequest* xrtWsClientRequestCreate(
	xstrview Url,
	const xwsclientconfig* pConfig,
	char sKey[XWS_KEY_CAPACITY]
)
{
	xwsclientconfig Config;
	xhttprequest* pRequest;

	if ( !__xwsClientClientConfigSnapshot(
		&Config,
		pConfig,
		"create-websocket-request"
	) || !__xwsClientHttpRangeCheck(
		sKey,
		XWS_KEY_CAPACITY,
		"create-websocket-request",
		"WebSocket key output range is invalid"
	) ) {
		return NULL;
	}
	pRequest = xrtWsRequestCreate(Url);
	if ( pRequest == NULL ) {
		return NULL;
	}
	if ( !__xrtWsClientRequestConfigure(
		pRequest,
		&Config,
		sKey
	) ) {
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	return pRequest;
}



/* 克隆自定义 GET 请求并替换 WebSocket 管理字段。 */
XWS_CLIENT_API xhttprequest* xrtWsClientRequestClone(
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	char sKey[XWS_KEY_CAPACITY]
)
{
	xwsclientconfig Config;
	xhttprequest* pClone;

	if ( !__xwsClientClientConfigSnapshot(
		&Config,
		pConfig,
		"clone-websocket-request"
	) || !__xwsClientHttpObjectCheck(
		pRequest,
		"clone-websocket-request",
		"WebSocket source request is null"
	) || !__xwsClientHttpRangeCheck(
		sKey,
		XWS_KEY_CAPACITY,
		"clone-websocket-request",
		"WebSocket key output range is invalid"
	) ) {
		return NULL;
	}
	pClone = xrtHttpRequestClone(pRequest);
	if ( pClone == NULL ) {
		__xwsClientHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"clone-websocket-request",
			"WebSocket source request could not be cloned"
		);
		return NULL;
	}
	if ( !__xrtWsClientRequestConfigure(
		pClone,
		&Config,
		sKey
	) ) {
		xrtHttpRequestDestroy(pClone);
		return NULL;
	}
	return pClone;
}



/* 严格验证服务器 101 响应和客户端 offer 的绑定关系。 */
XWS_CLIENT_API bool xrtWsClientCheck(
	const xhttpresponse* pResponse,
	xstrview Key,
	const xwsclientconfig* pConfig,
	xwsclienthandshake* pHandshake
)
{
	__xws_client_fields Fields;
	xwsclientconfig Config;
	xwsclienthandshake Handshake;
	const xhttpfield* pAccept;
	const xhttpfield* pProtocol;
	xstrview Selected = { 0 };
	bool bConnection;
	size_t iProtocols;

	if ( !__xwsClientClientConfigSnapshot(
		&Config,
		pConfig,
		"check-websocket-response"
	) || !__xwsClientHttpObjectCheck(
		pResponse,
		"check-websocket-response",
		"WebSocket response is null"
	) || !__xwsClientHttpRangeCheck(
		pHandshake,
		sizeof(Handshake),
		"check-websocket-response",
		"WebSocket client handshake output range is invalid"
	) ) {
		return false;
	}
	if ( !xrtMemRangeValid(Key.Data, Key.Size) ) {
		__xwsClientHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"check-websocket-response",
			"WebSocket key range is invalid"
		);
		return false;
	}
	if ( (Config.Protocols.Data == NULL) &&
		(Config.Protocols.Size != 0) ) {
		__xwsClientHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"check-websocket-response",
			"WebSocket client protocol range is invalid"
		);
		return false;
	}
	if ( (pHandshake == (const void*)pResponse) ||
		((pConfig != NULL) && xrtMemRangesOverlap(
		pHandshake,
		sizeof(Handshake),
		pConfig,
		sizeof(*pConfig)
	)) || xrtMemRangesOverlap(
		pHandshake,
		sizeof(Handshake),
		Key.Data,
		Key.Size
	) ) {
		__xwsClientHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"check-websocket-response",
			"WebSocket client handshake output overlaps an input"
		);
		return false;
	}
	if ( !xrtWsKeyValid(Key) ||
		!xrtWsProtocolsValid(Config.Protocols) ) {
		__xwsClientHandshakeWrap(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"check-websocket-response",
			"WebSocket response check input is invalid"
		);
		return false;
	}
	memset(&Handshake, 0, sizeof(Handshake));
	Fields.Data = xrtHttpHeadersData(
		xrtHttpResponseHeaders(pResponse)
	);
	Fields.Count = xrtHttpResponseHeaderCount(
		pResponse
	);
	if ( xrtHttpResponseVersion(pResponse) !=
		XHTTP_VERSION_1_1 ) {
		__xwsClientHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_VERSION,
			"check-websocket-response",
			"WebSocket response requires HTTP/1.1"
		);
		return false;
	}
	if ( xrtHttpResponseStatus(pResponse) !=
		XHTTP_STATUS_SWITCHING_PROTOCOLS ) {
		__xwsClientHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_STATUS,
			"check-websocket-response",
			"WebSocket response status is not 101"
		);
		return false;
	}
	if ( !__xwsClientHttpUpgradeExact(&Fields) ) {
		__xwsClientHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_UPGRADE,
			"check-websocket-response",
			"WebSocket Upgrade response field is missing or ambiguous"
		);
		return false;
	}
	if ( !__xwsClientHttpTokenHas(
		&Fields,
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("Upgrade"),
		&bConnection
	) || !bConnection ) {
		__xwsClientHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_CONNECTION,
			"check-websocket-response",
			"WebSocket response Connection field does not select Upgrade"
		);
		return false;
	}
	pAccept = __xwsClientHttpFieldUnique(
		&Fields,
		XRT_STR_LITERAL("Sec-WebSocket-Accept")
	);
	if ( (pAccept == NULL) ||
		!xrtWsAcceptValid(Key, pAccept->Value) ) {
		__xwsClientHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_ACCEPT,
			"check-websocket-response",
			"WebSocket response Accept is missing, repeated or invalid"
		);
		return false;
	}
	if ( (__xwsClientHttpFieldCount(
		&Fields,
		XRT_STR_LITERAL("Content-Length")
	) != 0) || (__xwsClientHttpFieldCount(
		&Fields,
		XRT_STR_LITERAL("Transfer-Encoding")
	) != 0) || (xrtHttpResponseBodyBytes(
		pResponse
	) != 0) ) {
		__xwsClientHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_BODY,
			"check-websocket-response",
			"WebSocket 101 response contains an HTTP body"
		);
		return false;
	}
	#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		if ( !__xrtWsClientDeflateCheck(
			&Fields,
			&Config,
			&Handshake
		) ) {
			return false;
		}
	#else
		if ( __xwsClientHttpFieldCount(
			&Fields,
			XRT_STR_LITERAL(
				"Sec-WebSocket-Extensions"
			)
		) != 0 ) {
			__xwsClientHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_EXTENSION,
				"check-websocket-response",
				"Unsolicited WebSocket extensions are not accepted"
			);
			return false;
		}
	#endif
	iProtocols = __xwsClientHttpFieldCount(
		&Fields,
		XRT_STR_LITERAL("Sec-WebSocket-Protocol")
	);
	if ( iProtocols > 1 ) {
		__xwsClientHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"check-websocket-response",
			"WebSocket response repeats the selected protocol"
		);
		return false;
	}
	if ( iProtocols == 1 ) {
		pProtocol = __xwsClientHttpFieldUnique(
			&Fields,
			XRT_STR_LITERAL("Sec-WebSocket-Protocol")
		);
		if ( (pProtocol == NULL) ||
			!xrtHttpTokenValid(pProtocol->Value) ||
			(Config.Protocols.Size == 0) ||
			!xrtWsProtocolsHas(
				Config.Protocols,
				pProtocol->Value
			) ) {
			__xwsClientHandshakeError(
				XERR_PROTOCOL,
				XWS_HANDSHAKE_ERROR_PROTOCOL,
				"check-websocket-response",
				"WebSocket response selected an unoffered protocol"
			);
			return false;
		}
		Selected = pProtocol->Value;
	}
	Handshake.Protocol = Selected;
	memcpy(pHandshake, &Handshake, sizeof(Handshake));
	return true;
}



/* 异常关闭并释放 HTTP Call 转移出的升级传输。 */
static void __xrtWsClientTransportAbort(
	xnetstream* pTcp
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		, xtlsstream* pTls
	#endif
)
{
	if ( pTcp != NULL ) {
		(void)xrtNetStreamAbort(pTcp);
		xrtNetStreamDestroy(pTcp);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pTls != NULL ) {
			(void)xrtTlsStreamAbort(pTls);
			xrtTlsStreamDestroy(pTls);
		}
	#endif
}



/* 把 HTTP Client 的升级终态接管为共享 WebSocket Connection。 */
static void __xrtWsClientDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	__xrt_ws_connect* pConnect =
		(__xrt_ws_connect*)pData;
	xwsconn* pConnection = NULL;
	xerror* pOwnedError = NULL;
	const xerror* pError;
	xnetresult Result;
	xwsclientconfig Config;
	xwsclienthandshake Handshake;
	bool bTransport;

	Result = pResult->Result;
	pError = pResult->Error;
	if ( Result == XNET_RESULT_OK ) {
		xrtWsClientConfigInit(&Config);
		Config.Protocols = pConnect->Protocols;
		#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
			Config.Deflate = pConnect->Deflate;
			Config.EnableDeflate =
				pConnect->EnableDeflate;
			Config.RequireDeflate =
				pConnect->RequireDeflate;
		#endif
		bTransport = pResult->Upgraded &&
			((pResult->Tcp != NULL)
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				!= (pResult->Tls != NULL)
			#endif
			);
		if ( !xrtWsClientCheck(
			pResult->Response,
			(xstrview) {
				pConnect->Key,
				XWS_KEY_SIZE
			},
			&Config,
			&Handshake
		) ) {
			/* xrtWsClientCheck 已经发布准确原因。 */
		} else if ( !bTransport ) {
			__xwsClientHandshakeError(
				XERR_STATE,
				XWS_HANDSHAKE_ERROR_UPGRADE,
				"attach-websocket-client",
				"HTTP client did not return one upgraded transport"
			);
		} else {
			pConnect->Connection.Protocol =
				Handshake.Protocol;
			#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
				pConnect->Connection.DeflateEnabled =
					Handshake.DeflateEnabled;
				if ( Handshake.DeflateEnabled ) {
					pConnect->Connection.Deflate =
						Handshake.Deflate;
				}
			#endif
			if ( pResult->Tcp != NULL ) {
				pConnection = xrtWsConnAttach(
					pResult->Tcp,
					&pConnect->Connection,
					&pConnect->Events,
					pConnect->EventData
				);
			}
			#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_HTTPS)
				else if ( pResult->Tls != NULL ) {
					pConnection = xrtWsConnAttachTls(
						pResult->Tls,
						&pConnect->Connection,
						&pConnect->Events,
						pConnect->EventData
					);
				}
			#endif
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS) && \
				!defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_HTTPS)
				else {
					__xwsClientHandshakeError(
						XERR_UNSUPPORTED,
						XWS_HANDSHAKE_ERROR_UPGRADE,
						"attach-websocket-client",
						"TLS WebSocket adapter is not present in this build"
					);
				}
			#endif
		}
		if ( pConnection == NULL ) {
			pOwnedError = xrtTakeError();
			__xrtWsClientTransportAbort(
				pResult->Tcp
				#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
					, pResult->Tls
				#endif
			);
			Result = XNET_RESULT_ERROR;
			pError = pOwnedError;
		}
	} else {
		__xrtWsClientTransportAbort(
			pResult->Tcp
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				, pResult->Tls
			#endif
		);
	}
	pConnect->Proc(
		pCall,
		Result,
		pConnection,
		pResult->Response,
		pError,
		pConnect->Data
	);
	xrtErrorFree(pOwnedError);
	xrtFree(pConnect);
}



/* 克隆请求、冻结异步上下文并提交 HTTP Client。 */
XWS_CLIENT_API xhttpcall* xrtWsConnectRequest(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsconnectproc pProc,
	ptr pData
)
{
	xwsclientconfig Config;
	xwsconnevents Events;
	xhttpcalloptions Options;
	xhttprequest* pPrepared;
	__xrt_ws_connect* pConnect;
	xhttpcall* pCall;
	char sKey[XWS_KEY_CAPACITY];
	size_t iAllocation;

	if ( !__xwsClientClientConfigSnapshot(
		&Config,
		pConfig,
		"connect-websocket-client"
	) || !__xwsClientConnEventsSnapshot(
		&Events,
		pEvents,
		"connect-websocket-client"
	) || !__xwsClientHttpObjectCheck(
		pClient,
		"connect-websocket-client",
		"HTTP client is null"
	) || !__xwsClientHttpObjectCheck(
		pRequest,
		"connect-websocket-client",
		"HTTP request is null"
	) ) {
		return NULL;
	}
	Config.Connection.Role = XWS_ROLE_CLIENT;
	Config.Connection.Protocol =
		(xstrview) { 0 };
	#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		Config.Connection.DeflateEnabled =
			Config.EnableDeflate;
		if ( Config.EnableDeflate ) {
			/*
				HTTP 提交前还没有服务端响应。
				用无参数响应验证本地编解码配置，实际方向在握手完成后覆盖。
			*/
			xrtWsDeflateInit(
				&Config.Connection.Deflate
			);
		}
	#elif defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		Config.Connection.DeflateEnabled = false;
	#endif
	if ( (pProc == NULL) ||
		((Config.Protocols.Data == NULL) &&
		 (Config.Protocols.Size != 0)) ) {
		__xwsClientHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"connect-websocket-client",
			"WebSocket client, request, protocols or callback is invalid"
		);
		return NULL;
	}
	if ( !xrtWsConnConfigValid(
		&Config.Connection
	) ) {
		__xwsClientHandshakeError(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"connect-websocket-client",
			"WebSocket client connection configuration is invalid"
		);
		return NULL;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		Config.Connection.DeflateEnabled = false;
	#endif
	pPrepared = xrtWsClientRequestClone(
		pRequest,
		&Config,
		sKey
	);
	if ( pPrepared == NULL ) {
		return NULL;
	}
	if ( Config.Protocols.Size >
		(SIZE_MAX - sizeof(*pConnect) - 1u) ) {
		xrtHttpRequestDestroy(pPrepared);
		__xwsClientHandshakeError(
			XERR_RANGE,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"connect-websocket-client",
			"WebSocket client protocol list is too large"
		);
		return NULL;
	}
	iAllocation = sizeof(*pConnect) +
		Config.Protocols.Size + 1u;
	pConnect = (__xrt_ws_connect*)xrtCalloc(
		1,
		iAllocation
	);
	if ( pConnect == NULL ) {
		xrtHttpRequestDestroy(pPrepared);
		__xwsClientHandshakeWrap(
			XERR_MEMORY,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"connect-websocket-client",
			"WebSocket client context allocation failed"
		);
		return NULL;
	}
	pConnect->Connection = Config.Connection;
	pConnect->Events = Events;
	pConnect->Proc = pProc;
	pConnect->EventData = pEventData;
	pConnect->Data = pData;
	memcpy(pConnect->Key, sKey, sizeof(sKey));
	#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		pConnect->Deflate = Config.Deflate;
		pConnect->EnableDeflate =
			Config.EnableDeflate;
		pConnect->RequireDeflate =
			Config.RequireDeflate;
	#endif
	if ( Config.Protocols.Size != 0 ) {
		memcpy(
			pConnect->Storage,
			Config.Protocols.Data,
			Config.Protocols.Size
		);
		pConnect->Protocols.Data =
			pConnect->Storage;
		pConnect->Protocols.Size =
			Config.Protocols.Size;
	}
	Options = Config.Http;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		Options.Redirect = XHTTP_REDIRECT_MANUAL;
	#endif
	pCall = xrtHttpClientDo(
		pClient,
		pPrepared,
		&Options,
		__xrtWsClientDone,
		pConnect
	);
	xrtHttpRequestDestroy(pPrepared);
	if ( pCall == NULL ) {
		xrtFree(pConnect);
	}
	return pCall;
}



/* 从 URL 创建基础请求并提交异步 WebSocket 握手。 */
XWS_CLIENT_API xhttpcall* xrtWsConnect(
	xhttpclient* pClient,
	xstrview Url,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsconnectproc pProc,
	ptr pData
)
{
	xwsclientconfig Config;
	xwsconnevents Events;
	xhttprequest* pRequest;
	xhttpcall* pCall;

	if ( !__xwsClientClientConfigSnapshot(
		&Config,
		pConfig,
		"connect-websocket-client"
	) || !__xwsClientConnEventsSnapshot(
		&Events,
		pEvents,
		"connect-websocket-client"
	) || !__xwsClientHttpObjectCheck(
		pClient,
		"connect-websocket-client",
		"HTTP client is null"
	) ) {
		return NULL;
	}
	if ( pProc == NULL ) {
		__xwsClientHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"connect-websocket-client",
			"WebSocket completion callback is null"
		);
		return NULL;
	}
	pRequest = xrtWsRequestCreate(Url);
	if ( pRequest == NULL ) {
		return NULL;
	}
	pCall = xrtWsConnectRequest(
		pClient,
		pRequest,
		&Config,
		&Events,
		pEventData,
		pProc,
		pData
	);
	xrtHttpRequestDestroy(pRequest);
	return pCall;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xws-client/src/websocket/client_future.c */
/* ========================================================================== */

#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)



#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)

/*
	桥接上下文在成功后原位转为 Future 拥有的结果。
	Result 必须保持首成员，使结果析构能够释放整块存储。
*/
typedef struct xrt_ws_client_future {
	xwsclientresult Result;
	xfuturebridge Bridge;
	xhttpcall* Call;
} xrt_ws_client_future;



/* 验证 Future 入口并安全取得配置中的父取消令牌。 */
static bool __xrtWsClientFutureInputs(
	xhttpclient* pClient,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	cstr sOperation,
	xwsclientconfig* pConfigOutput,
	xwsconnevents* pEventsOutput,
	xcancel** ppParent
)
{
	*ppParent = NULL;
	if ( !__xwsClientClientConfigSnapshot(
		pConfigOutput,
		pConfig,
		sOperation
	) || !__xwsClientConnEventsSnapshot(
		pEventsOutput,
		pEvents,
		sOperation
	) || !__xwsClientHttpObjectCheck(
		pClient,
		sOperation,
		"HTTP client is null"
	) ) {
		return false;
	}
	*ppParent = pConfigOutput->Http.Cancel;
	return true;
}



/* Future 的拥有值析构只释放一个连接建立结果引用。 */
static void __xrtWsClientFutureFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtWsClientResultDestroy((xwsclientresult*)pValue);
}



/* 把 Future 的协作取消转发给完整 HTTP Call。 */
static void __xrtWsClientFutureCancel(ptr pData)
{
	xrt_ws_client_future* pContext =
		(xrt_ws_client_future*)pData;

	(void)xrtHttpCallCancel(pContext->Call);
}



/* 回收回调转移但没有进入成功结果的全部对象。 */
static void __xrtWsClientFutureObjectsDestroy(
	xwsconn* pConnection,
	xhttpresponse* pResponse
)
{
	__xwsClientOpenConnectionDestroy(pConnection);
	xrtHttpResponseDestroy(pResponse);
}



/* 把成功回调转为 Future 拥有的连接建立结果。 */
static void __xrtWsClientFutureResolve(
	xrt_ws_client_future* pContext,
	xwsconn* pConnection,
	xhttpresponse* pResponse
)
{
	xpromise* pPromise = xrtFutureBridgePromise(&pContext->Bridge);
	xhttpcall* pCall = pContext->Call;
	bool bResolved;

	xrtAtomicPtrStore(
		&pContext->Result.Connection,
		pConnection,
		XMEMORY_RELEASE
	);
	xrtAtomicPtrStore(
		&pContext->Result.Response,
		pResponse,
		XMEMORY_RELEASE
	);
	xrtHttpCallDestroy(pCall);
	bResolved = xrtPromiseResolveOwned(
		pPromise,
		&pContext->Result,
		__xrtWsClientFutureFree,
		NULL
	);
	xrtPromiseDestroy(pPromise);
	if ( !bResolved ) {
		xrtWsClientResultDestroy(&pContext->Result);
	}
}



/* 把失败或取消回调转为对应 Future 终态并回收响应。 */
static void __xrtWsClientFutureReject(
	xrt_ws_client_future* pContext,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError
)
{
	xpromise* pPromise = xrtFutureBridgePromise(&pContext->Bridge);
	xhttpcall* pCall = pContext->Call;
	xerror* pFallback = NULL;
	xerror* pFailure = NULL;

	if ( Result != XNET_RESULT_CANCELLED ) {
		if ( pError == NULL ) {
			pFallback = __xwsClientOpenErrorCreate(
				XERR_INTERNAL,
				"complete-websocket-connect",
				"WebSocket connect failed without an error"
			);
			pError = pFallback != NULL ?
				pFallback : xrtGetError();
		}
		pFailure = xrtErrorRef(pError);
	}
	__xrtWsClientFutureObjectsDestroy(
		pConnection,
		pResponse
	);
	xrtHttpCallDestroy(pCall);
	xrtFree(pContext);
	if ( Result == XNET_RESULT_CANCELLED ) {
		(void)xrtPromiseCancel(pPromise);
	} else if ( pFailure != NULL ) {
		(void)xrtPromiseReject(pPromise, pFailure);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtErrorFree(pFailure);
	xrtErrorFree(pFallback);
	xrtPromiseDestroy(pPromise);
}



/* 接收 WebSocket 客户端唯一终态并完成 Future。 */
static void __xrtWsClientFutureDone(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
)
{
	xrt_ws_client_future* pContext =
		(xrt_ws_client_future*)pData;
	bool bReady;

	(void)pCall;
	bReady = xrtFutureBridgeWait(&pContext->Bridge);
	xrtFutureBridgeUnwatch(&pContext->Bridge);
	if ( !bReady ) {
		__xrtWsClientFutureObjectsDestroy(
			pConnection,
			pResponse
		);
		xrtHttpCallDestroy(pContext->Call);
		xrtPromiseDestroy(xrtFutureBridgePromise(&pContext->Bridge));
		xrtFree(pContext);
		return;
	}
	if ( (Result == XNET_RESULT_OK) &&
		(pConnection != NULL) &&
		(pResponse != NULL) ) {
		__xrtWsClientFutureResolve(
			pContext,
			pConnection,
			pResponse
		);
	} else if ( Result == XNET_RESULT_OK ) {
		xerror* pContractError =
			__xwsClientOpenErrorCreate(
				XERR_INTERNAL,
				"complete-websocket-connect",
				"WebSocket connect succeeded without a connection or response"
			);

		__xrtWsClientFutureReject(
			pContext,
			XNET_RESULT_ERROR,
			pConnection,
			pResponse,
			pContractError != NULL ?
				pContractError : xrtGetError()
		);
		xrtErrorFree(pContractError);
	} else {
		__xrtWsClientFutureReject(
			pContext,
			Result,
			pConnection,
			pResponse,
			pError
		);
	}
}



/* 分配 Promise、连接建立结果和统一取消桥。 */
static xrt_ws_client_future* __xrtWsClientFutureCreate(
	xcancel* pParent,
	xfuture** ppFuture
)
{
	xrt_ws_client_future* pContext;

	pContext = (xrt_ws_client_future*)xrtCalloc(
		1,
		sizeof(*pContext)
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Result.References = 1;
	xrtAtomicPtrInit(&pContext->Result.Connection, NULL);
	xrtAtomicPtrInit(&pContext->Result.Response, NULL);
	*ppFuture = xrtFutureBridgeCreate(
		&pContext->Bridge,
		pParent
	);
	if ( *ppFuture == NULL ) {
		xrtFree(pContext);
		return NULL;
	}
	return pContext;
}



/* 完成底层提交后的取消观察器安装。 */
static bool __xrtWsClientFutureWatch(
	xrt_ws_client_future* pContext,
	xfuture* pFuture
)
{
	xerror* pError;

	if ( xrtFutureBridgeWatch(
		&pContext->Bridge,
		__xrtWsClientFutureCancel,
		pContext
	) ) {
		xrtFutureBridgeReady(&pContext->Bridge);
		return true;
	}
	pError = xrtTakeError();
	xrtFutureBridgeFail(&pContext->Bridge);
	(void)xrtHttpCallCancel(pContext->Call);
	xrtFutureDestroy(pFuture);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 使用 URL 提交 WebSocket Client Future。 */
XWS_CLIENT_API xfuture* xrtWsConnectAsync(
	xhttpclient* pClient,
	xstrview Url,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	xrt_ws_client_future* pContext;
	xwsclientconfig Config;
	xwsconnevents Events;
	xcancel* pParent;
	xfuture* pFuture;

	if ( !__xrtWsClientFutureInputs(
		pClient,
		pConfig,
		pEvents,
		"connect-websocket-async",
		&Config,
		&Events,
		&pParent
	) ) {
		return NULL;
	}
	pContext = __xrtWsClientFutureCreate(
		pParent,
		&pFuture
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Call = xrtWsConnect(
		pClient,
		Url,
		&Config,
		&Events,
		pData,
		__xrtWsClientFutureDone,
		pContext
	);
	if ( pContext->Call == NULL ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(xrtFutureBridgePromise(&pContext->Bridge));
		xrtFree(pContext);
		return NULL;
	}
	if ( !__xrtWsClientFutureWatch(
		pContext,
		pFuture
	) ) {
		return NULL;
	}
	return pFuture;
}



/* 使用自定义 GET 请求提交 WebSocket Client Future。 */
XWS_CLIENT_API xfuture* xrtWsConnectRequestAsync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	xrt_ws_client_future* pContext;
	xwsclientconfig Config;
	xwsconnevents Events;
	xcancel* pParent;
	xfuture* pFuture;

	if ( !__xrtWsClientFutureInputs(
		pClient,
		pConfig,
		pEvents,
		"connect-websocket-request-async",
		&Config,
		&Events,
		&pParent
	) ) {
		return NULL;
	}
	if ( !__xwsClientHttpObjectCheck(
		pRequest,
		"connect-websocket-request-async",
		"HTTP request is null"
	) ) {
		return NULL;
	}
	pContext = __xrtWsClientFutureCreate(
		pParent,
		&pFuture
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Call = xrtWsConnectRequest(
		pClient,
		pRequest,
		&Config,
		&Events,
		pData,
		__xrtWsClientFutureDone,
		pContext
	);
	if ( pContext->Call == NULL ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(xrtFutureBridgePromise(&pContext->Bridge));
		xrtFree(pContext);
		return NULL;
	}
	if ( !__xrtWsClientFutureWatch(
		pContext,
		pFuture
	) ) {
		return NULL;
	}
	return pFuture;
}



/* 验证宿主线程可以阻塞等待指定 Client。 */
static bool __xrtWsConnectCanWait(xhttpclient* pClient)
{
	if ( !__xwsClientHttpObjectCheck(
		pClient,
		"wait-websocket-connect",
		"HTTP client is null"
	) ) {
		return false;
	}
	if ( xrtNetEngineCurrent(xrtHttpClientEngine(pClient)) != NULL ) {
		xerror* pError = __xwsClientOpenErrorCreate(
			XERR_STATE,
			"wait-websocket-connect",
			"network Worker cannot block on its own WebSocket connect"
		);

		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return false;
	}
	return true;
}



/* 阻塞等待 Future，并保留一个成功结果引用。 */
static xwsclientresult* __xrtWsConnectWait(
	xfuture* pFuture
)
{
	xfuturestate State;
	xwsclientresult* pResult = NULL;

	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( xrtFutureWait(pFuture) != XWAIT_OK ) {
		xerror* pError = xrtTakeError();

		(void)xrtFutureCancel(pFuture);
		xrtFutureDestroy(pFuture);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	State = xrtFutureState(pFuture);
	if ( State == XFUTURE_RESOLVED ) {
		pResult = xrtWsClientResultRef(
			(xwsclientresult*)xrtFutureValue(pFuture)
		);
	} else if ( State == XFUTURE_FAILED ) {
		(void)xrtFutureValue(pFuture);
	} else {
		xerror* pError = __xwsClientOpenErrorCreate(
			State == XFUTURE_CANCELLED ?
				XERR_CANCELLED : XERR_INTERNAL,
			"wait-websocket-connect",
			State == XFUTURE_CANCELLED ?
				"WebSocket connect was cancelled while waiting" :
				"WebSocket Future completed without a usable result"
		);

		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
	}
	xrtFutureDestroy(pFuture);
	return pResult;
}



/* 在宿主线程使用 URL 阻塞建立 WebSocket。 */
XWS_CLIENT_API xwsclientresult* xrtWsConnectSync(
	xhttpclient* pClient,
	xstrview Url,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	if ( !__xrtWsConnectCanWait(pClient) ) {
		return NULL;
	}
	return __xrtWsConnectWait(
		xrtWsConnectAsync(
			pClient,
			Url,
			pConfig,
			pEvents,
			pData
		)
	);
}



/* 在宿主线程使用自定义 GET 请求阻塞建立 WebSocket。 */
XWS_CLIENT_API xwsclientresult* xrtWsConnectRequestSync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	if ( !__xrtWsConnectCanWait(pClient) ) {
		return NULL;
	}
	return __xrtWsConnectWait(
		xrtWsConnectRequestAsync(
			pClient,
			pRequest,
			pConfig,
			pEvents,
			pData
		)
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xws-client/src/websocket/client_result.c */
/* ========================================================================== */

#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)

#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)




/* 记录连接建立结果的稳定错误上下文。 */
static void __xrtWsClientResultError(
	xerrkind Kind,
	xwsclientresulterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsClientErrorSetDetail(
		Kind,
		"xrt.websocket.open-result",
		(int32)Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* 验证公开连接建立结果覆盖完整固定存储。 */
static bool __xrtWsClientResultValid(
	const xwsclientresult* pResult,
	cstr sOperation
)
{
	if ( xrtMemRangeValid(pResult, sizeof(*pResult)) ) {
		return true;
	}
	__xrtWsClientResultError(
		XERR_ARGUMENT,
		XWS_CLIENT_RESULT_ERROR_ARGUMENT,
		sOperation,
		"WebSocket open result range is invalid"
	);
	return false;
}




/* 增加连接建立结果引用并返回原指针。 */
XWS_CLIENT_API xwsclientresult* xrtWsClientResultRef(
	xwsclientresult* pResult
)
{
	if ( !__xrtWsClientResultValid(
		pResult,
		"retain-websocket-open-result"
	) ) {
		return NULL;
	}
	if ( xrtRefRetain(&pResult->References) < 0 ) {
		__xrtWsClientResultError(
			XERR_STATE,
			XWS_CLIENT_RESULT_ERROR_STATE,
			"retain-websocket-open-result",
			"WebSocket open result cannot be retained"
		);
		return NULL;
	}
	return pResult;
}



/* 释放最后一个结果引用及其尚未取走的全部所有权。 */
XWS_CLIENT_API void xrtWsClientResultDestroy(
	xwsclientresult* pResult
)
{
	xwsconn* pConnection;
	#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)
		xhttpresponse* pResponse;
	#endif

	if ( pResult == NULL ) {
		return;
	}
	if ( !__xrtWsClientResultValid(
		pResult,
		"destroy-websocket-open-result"
	) ) {
		return;
	}
	if ( xrtRefRelease(&pResult->References) != 0 ) {
		return;
	}
	pConnection = (xwsconn*)xrtAtomicPtrExchange(
		&pResult->Connection,
		NULL,
		XMEMORY_ACQ_REL
	);
	__xwsClientOpenConnectionDestroy(pConnection);
	#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)
		pResponse = (xhttpresponse*)xrtAtomicPtrExchange(
			&pResult->Response,
			NULL,
			XMEMORY_ACQ_REL
		);
		xrtHttpResponseDestroy(pResponse);
	#endif
	memset(pResult, 0, sizeof(*pResult));
	xrtFree(pResult);
}



/* 返回结果借用的 WebSocket Connection。 */
XWS_CLIENT_API xwsconn* xrtWsClientResultConnection(
	const xwsclientresult* pResult
)
{
	if ( !__xrtWsClientResultValid(
		pResult,
		"get-websocket-open-connection"
	) ) {
		return NULL;
	}
	return (xwsconn*)xrtAtomicPtrLoad(
		&pResult->Connection,
		XMEMORY_ACQUIRE
	);
}



/* 原子取走结果拥有的 WebSocket Connection。 */
XWS_CLIENT_API xwsconn* xrtWsClientResultTakeConnection(
	xwsclientresult* pResult
)
{
	if ( !__xrtWsClientResultValid(
		pResult,
		"take-websocket-open-connection"
	) ) {
		return NULL;
	}
	return (xwsconn*)xrtAtomicPtrExchange(
		&pResult->Connection,
		NULL,
		XMEMORY_ACQ_REL
	);
}



#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_FUTURE)

/* 返回结果借用的客户端 HTTP 101 Response。 */
XWS_CLIENT_API const xhttpresponse* xrtWsClientResultResponse(
	const xwsclientresult* pResult
)
{
	if ( !__xrtWsClientResultValid(
		pResult,
		"get-websocket-open-response"
	) ) {
		return NULL;
	}
	return (const xhttpresponse*)xrtAtomicPtrLoad(
		&pResult->Response,
		XMEMORY_ACQUIRE
	);
}



/* 原子取走结果拥有的客户端 HTTP 101 Response。 */
XWS_CLIENT_API xhttpresponse* xrtWsClientResultTakeResponse(
	xwsclientresult* pResult
)
{
	if ( !__xrtWsClientResultValid(
		pResult,
		"take-websocket-open-response"
	) ) {
		return NULL;
	}
	return (xhttpresponse*)xrtAtomicPtrExchange(
		&pResult->Response,
		NULL,
		XMEMORY_ACQ_REL
	);
}

#endif


#endif
#endif

#endif
