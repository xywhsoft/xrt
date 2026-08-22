#include "../internal/xrt_http_server.h"
#include "../internal/xrt_http.h"
#include <xrt/http_trailer.h>
#include <stdio.h>



#if defined(XRT_FEATURE_HTTP_SERVER_RESPONSE)

/* 准备阶段只保存借用输入和本次调用内的临时字段数组。 */
typedef struct xrt_http1_server_response_prepare {
	const xrt_http1_server_response_source* Source;
	xhttpfield* Fields;
	size_t FieldCount;
	size_t FieldCapacity;
	str TrailerNames;
	size_t TrailerNamesSize;
	xhttpbody* Body;
	uint64 BodyLength;
	uint64 Length;
	xhttpversion Version;
	xstrview Method;
	uint32 RequestFlags;
	xhttp1bodymode Mode;
	bool AddLength;
	bool AddTransfer;
	bool AddTrailer;
	bool ConnectionClose;
	bool ConnectionKeepAlive;
	bool ConnectionUpgrade;
	bool Close;
	bool Tunnel;
} xrt_http1_server_response_prepare;



/* 发布响应准备阶段的结构化错误。 */
static bool __xrtHttp1ServerResponsePrepareErrorCause(
	xhttp1serverresponseerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.http.server.response";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 发布没有额外原因链的响应准备错误。 */
static bool __xrtHttp1ServerResponsePrepareError(
	xhttp1serverresponseerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	return __xrtHttp1ServerResponsePrepareErrorCause(
		Code, Kind, sOperation, sMessage, NULL
	);
}



/* 严格扫描一个 Connection 字段并收集标准选项。 */
static bool __xrtHttp1ServerResponseConnection(
	xrt_http1_server_response_prepare* pPrepare,
	xstrview Value
)
{
	xhttpnext Next;
	xstrview Token;
	size_t iOffset = 0;

	for ( ;; ) {
		Next = xrtHttpTokenNext(Value, &iOffset, &Token);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			return true;
		}
		if ( xrtHttpTokenEqual(
			Token, XRT_STR_LITERAL("close")
		) ) {
			pPrepare->ConnectionClose = true;
		} else if ( xrtHttpTokenEqual(
			Token, XRT_STR_LITERAL("keep-alive")
		) ) {
			pPrepare->ConnectionKeepAlive = true;
		} else if ( xrtHttpTokenEqual(
			Token, XRT_STR_LITERAL("upgrade")
		) ) {
			pPrepare->ConnectionUpgrade = true;
		}
	}
}



/* 追加一个只在本次准备调用内借用的输出字段。 */
static bool __xrtHttp1ServerResponseField(
	xrt_http1_server_response_prepare* pPrepare,
	xstrview Name,
	xstrview Value
)
{
	if ( pPrepare->FieldCount >= pPrepare->FieldCapacity ) {
		return __xrtHttp1ServerResponsePrepareError(
			XHTTP1_SERVER_RESPONSE_ERROR_STATE,
			XERR_INTERNAL,
			"prepare-http1-server-response",
			"HTTP server response field capacity is inconsistent"
		);
	}
	pPrepare->Fields[pPrepare->FieldCount].Name = Name;
	pPrepare->Fields[pPrepare->FieldCount].Value = Value;
	pPrepare->FieldCount++;
	return true;
}



/* 扫描用户 Header 并移除由准备层唯一生成的分帧字段。 */
static bool __xrtHttp1ServerResponseHeaders(
	xrt_http1_server_response_prepare* pPrepare,
	xhttpfield* pLength,
	bool* pHasLength,
	xhttpfield* pTransfer,
	bool* pHasTransfer,
	bool* pTrailerDeclared,
	size_t* pUpgradeCount
)
{
	xhttpfield Field;
	size_t iCount = pPrepare->Source->HeaderCount;
	size_t i;

	*pHasLength = false;
	*pHasTransfer = false;
	*pTrailerDeclared = false;
	*pUpgradeCount = 0;
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(
			pPrepare->Source->Headers, i, &Field
		);
		if ( xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("Content-Length")
		) ) {
			if ( *pHasLength ) {
				return __xrtHttp1ServerResponsePrepareError(
					XHTTP1_SERVER_RESPONSE_ERROR_FRAMING,
					XERR_PROTOCOL,
					"prepare-http1-server-response",
					"HTTP server Reply contains repeated Content-Length fields"
				);
			}
			*pLength = Field;
			*pHasLength = true;
			continue;
		}
		if ( xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("Transfer-Encoding")
		) ) {
			if ( *pHasTransfer ) {
				return __xrtHttp1ServerResponsePrepareError(
					XHTTP1_SERVER_RESPONSE_ERROR_FRAMING,
					XERR_PROTOCOL,
					"prepare-http1-server-response",
					"HTTP server Reply contains repeated Transfer-Encoding fields"
				);
			}
			*pTransfer = Field;
			*pHasTransfer = true;
			continue;
		}
		if ( xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("Trailer")
		) ) {
			*pTrailerDeclared = true;
			continue;
		}
		if ( xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("Connection")
		) && !__xrtHttp1ServerResponseConnection(
			pPrepare, Field.Value
		) ) {
			return __xrtHttp1ServerResponsePrepareError(
				XHTTP1_SERVER_RESPONSE_ERROR_HEADER,
				XERR_PROTOCOL,
				"prepare-http1-server-response",
				"HTTP server Reply Connection field is invalid"
			);
		}
		if ( xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("Upgrade")
		) ) {
			(*pUpgradeCount)++;
		}
		if ( !__xrtHttp1ServerResponseField(
			pPrepare, Field.Name, Field.Value
		) ) {
			return false;
		}
	}
	return true;
}



/* 验证用户提供的唯一分帧字段并返回规范长度。 */
static bool __xrtHttp1ServerResponseFramingFields(
	xrt_http1_server_response_prepare* pPrepare,
	const xhttpfield* pLength,
	const xhttpfield* pTransfer,
	uint64* pDeclared
)
{
	*pDeclared = 0;
	if ( (pLength != NULL) && (pTransfer != NULL) ) {
		return __xrtHttp1ServerResponsePrepareError(
			XHTTP1_SERVER_RESPONSE_ERROR_FRAMING,
			XERR_PROTOCOL,
			"prepare-http1-server-response",
			"HTTP server Reply cannot combine Content-Length and Transfer-Encoding"
		);
	}
	if ( (pLength != NULL) &&
		!xrtHttpContentLengthParse(
			pLength->Value, pDeclared
		) ) {
		return __xrtHttp1ServerResponsePrepareErrorCause(
			XHTTP1_SERVER_RESPONSE_ERROR_FRAMING,
			XERR_PROTOCOL,
			"prepare-http1-server-response",
			"HTTP server Reply Content-Length is invalid",
			xrtGetError()
		);
	}
	if ( (pTransfer != NULL) &&
		!xrtHttpTokenEqual(
			xrtHttpOwsTrim(pTransfer->Value),
			XRT_STR_LITERAL("chunked")
		) ) {
		return __xrtHttp1ServerResponsePrepareError(
			XHTTP1_SERVER_RESPONSE_ERROR_FRAMING,
			XERR_UNSUPPORTED,
			"prepare-http1-server-response",
			"HTTP server response writer only emits final chunked transfer coding"
		);
	}
	if ( (pTransfer != NULL) &&
		(pPrepare->Version != XHTTP_VERSION_1_1) ) {
		return __xrtHttp1ServerResponsePrepareError(
			XHTTP1_SERVER_RESPONSE_ERROR_FRAMING,
			XERR_PROTOCOL,
			"prepare-http1-server-response",
			"HTTP/1.0 response cannot use chunked transfer coding"
		);
	}
	return true;
}



/* 构建实际 Trailer 名称列表并验证每个字段均可在线路末尾发送。 */
static bool __xrtHttp1ServerResponseTrailers(
	xrt_http1_server_response_prepare* pPrepare
)
{
	size_t iCount = pPrepare->Source->TrailerCount;
	const xerror* pCause;
	xerrkind Kind;

	if ( iCount == 0 ) {
		return true;
	}
	pPrepare->TrailerNames = xrtHttpTrailerNamesBuild(
		pPrepare->Source->Trailers,
		iCount,
		&pPrepare->TrailerNamesSize
	);
	if ( pPrepare->TrailerNames == NULL ) {
		pCause = xrtGetError();
		Kind = pCause != NULL ?
			xrtErrorKind(pCause) : XERR_INTERNAL;
		return __xrtHttp1ServerResponsePrepareErrorCause(
			XHTTP1_SERVER_RESPONSE_ERROR_TRAILER,
			Kind,
			"prepare-http1-server-response",
			"HTTP server Trailer declaration could not be built",
			pCause
		);
	}
	return true;
}



/* 解析无正文状态、CONNECT 与 Upgrade 的线路终止语义。 */
static bool __xrtHttp1ServerResponseStatus(
	xrt_http1_server_response_prepare* pPrepare,
	uint16 iStatus,
	size_t iUpgradeCount
)
{
	xhttpfield Upgrade;
	size_t iUpgrade = xrtHttpFieldFind(
		pPrepare->Source->Headers,
		pPrepare->Source->HeaderCount,
		XRT_STR_LITERAL("Upgrade"),
		0
	);
	bool bConnect = xrtHttpMethodEqual(
		pPrepare->Method, XRT_STR_LITERAL("CONNECT")
	);
	bool bUpgrade = iUpgrade != XRT_NPOS;

	if ( bUpgrade ) {
		__xrtHttpFieldLoad(
			pPrepare->Source->Headers,
			iUpgrade,
			&Upgrade
		);
	}

	if ( (iStatus < 200) && (iStatus != 101) ) {
		return __xrtHttp1ServerResponsePrepareError(
			XHTTP1_SERVER_RESPONSE_ERROR_STATUS,
			XERR_VALUE,
			"prepare-http1-server-response",
			"informational responses require the dedicated server path"
		);
	}
	if ( iStatus == 101 ) {
		if ( (pPrepare->Version != XHTTP_VERSION_1_1) ||
			((pPrepare->RequestFlags &
			  XHTTP_SERVER_REQUEST_UPGRADE) == 0) ||
			(iUpgradeCount != 1) ||
			!bUpgrade ||
			(xrtHttpOwsTrim(Upgrade.Value).Size == 0) ) {
			return __xrtHttp1ServerResponsePrepareError(
				XHTTP1_SERVER_RESPONSE_ERROR_STATUS,
				XERR_PROTOCOL,
				"prepare-http1-server-response",
				"101 response requires one accepted HTTP/1.1 Upgrade request"
			);
		}
		pPrepare->Tunnel = true;
	}
	if ( bConnect && (iStatus >= 200) && (iStatus < 300) ) {
		pPrepare->Tunnel = true;
	}
	return true;
}



/* 收敛正文对象、用户字段和请求语义为唯一发送模式。 */
static bool __xrtHttp1ServerResponseBody(
	xrt_http1_server_response_prepare* pPrepare,
	const xhttpfield* pLength,
	const xhttpfield* pTransfer,
	bool bTrailerDeclared
)
{
	uint16 iStatus = pPrepare->Source->Status;
	size_t iTrailers = pPrepare->Source->TrailerCount;
	uint64 iDeclared;
	bool bHead = xrtHttpMethodEqual(
		pPrepare->Method, XRT_STR_LITERAL("HEAD")
	);
	bool bMetadata = bHead || (iStatus == 304);
	bool bSuppressed = pPrepare->Tunnel ||
		!xrtHttpResponseContentAllowed(
			pPrepare->Method, iStatus
		);

	if ( (!bSuppressed || bMetadata) &&
		!__xrtHttp1ServerResponseFramingFields(
			pPrepare, pLength, pTransfer, &iDeclared
		) ) {
		return false;
	}
	if ( bSuppressed && !bMetadata ) {
		iDeclared = 0;
	}
	if ( bTrailerDeclared && (iTrailers == 0) ) {
		return __xrtHttp1ServerResponsePrepareError(
			XHTTP1_SERVER_RESPONSE_ERROR_TRAILER,
			XERR_VALUE,
			"prepare-http1-server-response",
			"HTTP server Reply declares Trailer without actual Trailer fields"
		);
	}
	if ( bSuppressed && (iTrailers != 0) ) {
		return __xrtHttp1ServerResponsePrepareError(
			XHTTP1_SERVER_RESPONSE_ERROR_TRAILER,
			XERR_PROTOCOL,
			"prepare-http1-server-response",
			"this HTTP response cannot carry Trailer fields"
		);
	}
	pPrepare->Body = pPrepare->Source->Body;
	pPrepare->BodyLength = pPrepare->Source->BodyLength;

	if ( bSuppressed ) {
		pPrepare->Mode = pPrepare->Tunnel ?
			XHTTP1_BODY_TUNNEL : XHTTP1_BODY_NONE;
		if ( bMetadata ) {
			if ( pLength != NULL ) {
				if ( (pPrepare->Body != NULL) &&
					(pPrepare->BodyLength !=
					 XHTTP_BODY_UNKNOWN) &&
					(iDeclared != pPrepare->BodyLength) ) {
					return __xrtHttp1ServerResponsePrepareError(
						XHTTP1_SERVER_RESPONSE_ERROR_LENGTH,
						XERR_PROTOCOL,
						"prepare-http1-server-response",
						"metadata response Content-Length does not match its representation"
					);
				}
				pPrepare->AddLength = true;
				pPrepare->Length = iDeclared;
			} else if ( (pTransfer != NULL) &&
				(pPrepare->Version == XHTTP_VERSION_1_1) ) {
				pPrepare->AddTransfer = true;
			} else if (
				pPrepare->BodyLength != XHTTP_BODY_UNKNOWN
			) {
				if ( bHead || (pPrepare->Body != NULL) ) {
					pPrepare->AddLength = true;
					pPrepare->Length =
						pPrepare->BodyLength;
				}
			}
		} else if ( iStatus == 205 ) {
			pPrepare->AddLength = true;
			pPrepare->Length = 0;
		}
		return true;
	}

	if ( iTrailers != 0 ) {
		if ( (pPrepare->Version != XHTTP_VERSION_1_1) ||
			(pLength != NULL) ) {
			return __xrtHttp1ServerResponsePrepareError(
				XHTTP1_SERVER_RESPONSE_ERROR_TRAILER,
				XERR_PROTOCOL,
				"prepare-http1-server-response",
				"HTTP server Trailer fields require chunked HTTP/1.1 framing"
			);
		}
		pPrepare->Mode = XHTTP1_BODY_CHUNKED;
		pPrepare->AddTransfer = true;
		pPrepare->AddTrailer = true;
		return true;
	}
	if ( pTransfer != NULL ) {
		pPrepare->Mode = XHTTP1_BODY_CHUNKED;
		pPrepare->AddTransfer = true;
		return true;
	}
	if ( pLength != NULL ) {
		if ( (pPrepare->BodyLength == XHTTP_BODY_UNKNOWN) ||
			(iDeclared != pPrepare->BodyLength) ) {
			return __xrtHttp1ServerResponsePrepareError(
				XHTTP1_SERVER_RESPONSE_ERROR_LENGTH,
				XERR_PROTOCOL,
				"prepare-http1-server-response",
				"HTTP server Reply Content-Length does not match its body"
			);
		}
		pPrepare->Length = iDeclared;
		pPrepare->AddLength = true;
		pPrepare->Mode = iDeclared == 0 ?
			XHTTP1_BODY_NONE : XHTTP1_BODY_FIXED;
		return true;
	}
	if ( pPrepare->BodyLength == XHTTP_BODY_UNKNOWN ) {
		pPrepare->Mode =
			(pPrepare->Version == XHTTP_VERSION_1_1) ?
				XHTTP1_BODY_CHUNKED :
				XHTTP1_BODY_CLOSE;
		pPrepare->AddTransfer =
			pPrepare->Mode == XHTTP1_BODY_CHUNKED;
		return true;
	}
	pPrepare->Length = pPrepare->BodyLength;
	pPrepare->AddLength = true;
	pPrepare->Mode = pPrepare->BodyLength == 0 ?
		XHTTP1_BODY_NONE : XHTTP1_BODY_FIXED;
	return true;
}



/* 加入规范分帧、Trailer 声明和必要连接字段。 */
static bool __xrtHttp1ServerResponseGeneratedFields(
	xrt_http1_server_response_prepare* pPrepare,
	char* sLength
)
{
	int iWritten;

	if ( pPrepare->AddLength ) {
		iWritten = snprintf(
			sLength,
			21,
			"%llu",
			(unsigned long long)pPrepare->Length
		);
		if ( (iWritten <= 0) || (iWritten >= 21) ||
			!__xrtHttp1ServerResponseField(
				pPrepare,
				XRT_STR_LITERAL("Content-Length"),
				(xstrview){ sLength, (size_t)iWritten }
			) ) {
			return false;
		}
	}
	if ( pPrepare->AddTransfer &&
		!__xrtHttp1ServerResponseField(
			pPrepare,
			XRT_STR_LITERAL("Transfer-Encoding"),
			XRT_STR_LITERAL("chunked")
		) ) {
		return false;
	}
	if ( pPrepare->AddTrailer &&
		!__xrtHttp1ServerResponseField(
			pPrepare,
			XRT_STR_LITERAL("Trailer"),
			(xstrview){
				pPrepare->TrailerNames,
				pPrepare->TrailerNamesSize
			}
		) ) {
		return false;
	}

	pPrepare->Close =
		((pPrepare->RequestFlags &
		  XHTTP_SERVER_REQUEST_KEEP_ALIVE) == 0) ||
		pPrepare->ConnectionClose ||
		(pPrepare->Mode == XHTTP1_BODY_CLOSE);
	if ( pPrepare->Tunnel ) {
		if ( pPrepare->ConnectionClose ) {
			return __xrtHttp1ServerResponsePrepareError(
				XHTTP1_SERVER_RESPONSE_ERROR_HEADER,
				XERR_PROTOCOL,
				"prepare-http1-server-response",
				"tunnel response cannot also request connection close"
			);
		}
		pPrepare->Close = false;
		if ( !pPrepare->ConnectionUpgrade &&
			!xrtHttpMethodEqual(
				pPrepare->Method,
				XRT_STR_LITERAL("CONNECT")
			) && !__xrtHttp1ServerResponseField(
				pPrepare,
				XRT_STR_LITERAL("Connection"),
				XRT_STR_LITERAL("Upgrade")
			) ) {
			return false;
		}
	} else if ( pPrepare->Close ) {
		if ( !pPrepare->ConnectionClose &&
			!__xrtHttp1ServerResponseField(
				pPrepare,
				XRT_STR_LITERAL("Connection"),
				XRT_STR_LITERAL("close")
			) ) {
			return false;
		}
	} else if (
		(pPrepare->Version == XHTTP_VERSION_1_0) &&
		!pPrepare->ConnectionKeepAlive &&
		!__xrtHttp1ServerResponseField(
			pPrepare,
			XRT_STR_LITERAL("Connection"),
			XRT_STR_LITERAL("keep-alive")
		)
	) {
		return false;
	}
	return true;
}



/* 安全累计最终 Response 尾随存储长度。 */
static bool __xrtHttp1ServerResponseStorageAdd(
	size_t* pSize,
	size_t iAdd
)
{
	if ( *pSize > (SIZE_MAX - iAdd) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 分配紧凑响应计划并直接写入 Header 与终止 chunk。 */
static xhttp1serverresponse* __xrtHttp1ServerResponsePlan(
	xrt_http1_server_response_prepare* pPrepare
)
{
	const xhttpfield* pTrailers =
		pPrepare->Source->Trailers;
	size_t iTrailerCount =
		pPrepare->Source->TrailerCount;
	xhttp1serverresponse* pResponse;
	xhttpbody* pBody = NULL;
	size_t iHeadSize;
	size_t iEndSize = 0;
	size_t iStorage = 0;
	bytes pStorage;

	if ( !xrtHttp1ResponseWrite(
		pPrepare->Version,
		pPrepare->Source->Status,
		pPrepare->Source->Reason,
		pPrepare->Fields,
		pPrepare->FieldCount,
		NULL,
		0,
		&iHeadSize
	) || ((pPrepare->Mode == XHTTP1_BODY_CHUNKED) &&
		!xrtHttp1ChunkEndWrite(
			pTrailers,
			iTrailerCount,
			NULL,
			0,
			&iEndSize
		)) || !__xrtHttp1ServerResponseStorageAdd(
			&iStorage, iHeadSize
		) || !__xrtHttp1ServerResponseStorageAdd(
			&iStorage, iEndSize
		) || (sizeof(*pResponse) > (SIZE_MAX - iStorage)) ) {
		return NULL;
	}
	if ( (pPrepare->Mode == XHTTP1_BODY_FIXED) ||
		(pPrepare->Mode == XHTTP1_BODY_CHUNKED) ||
		(pPrepare->Mode == XHTTP1_BODY_CLOSE) ) {
		if ( pPrepare->Body != NULL ) {
			pBody = xrtHttpBodyRef(pPrepare->Body);
			if ( pBody == NULL ) {
				return NULL;
			}
		}
	}
	pResponse = (xhttp1serverresponse*)xrtMalloc(
		sizeof(*pResponse) + iStorage
	);
	if ( pResponse == NULL ) {
		xrtHttpBodyDestroy(pBody);
		return NULL;
	}
	memset(pResponse, 0, sizeof(*pResponse));
	pStorage = (bytes)(pResponse + 1);
	pResponse->Head = pStorage;
	pResponse->HeadSize = iHeadSize;
	pResponse->End = pStorage + iHeadSize;
	pResponse->EndSize = iEndSize;
	pResponse->Body = pBody;
	pResponse->BodyLength = pPrepare->BodyLength;
	pResponse->BodyRemaining =
		pPrepare->Mode == XHTTP1_BODY_FIXED ?
			pPrepare->BodyLength : 0;
	pResponse->Mode = pPrepare->Mode;
	pResponse->State = XRT_HTTP_SERVER_RESPONSE_HEAD;
	pResponse->Close = pPrepare->Close;
	pResponse->Tunnel = pPrepare->Tunnel;
	if ( !xrtHttp1ResponseWrite(
		pPrepare->Version,
		pPrepare->Source->Status,
		pPrepare->Source->Reason,
		pPrepare->Fields,
		pPrepare->FieldCount,
		pResponse->Head,
		pResponse->HeadSize,
		&pResponse->HeadSize
	) || ((pPrepare->Mode == XHTTP1_BODY_CHUNKED) &&
		!xrtHttp1ChunkEndWrite(
			pTrailers,
			iTrailerCount,
			pResponse->End,
			pResponse->EndSize,
			&pResponse->EndSize
		)) ) {
		xrtHttp1ServerResponseDestroy(pResponse);
		return NULL;
	}
	return pResponse;
}



/* 释放准备阶段临时字段与 Trailer 名称。 */
static void __xrtHttp1ServerResponsePrepareUnit(
	xrt_http1_server_response_prepare* pPrepare
)
{
	xrtFree(pPrepare->TrailerNames);
	xrtFree(pPrepare->Fields);
	pPrepare->TrailerNames = NULL;
	pPrepare->Fields = NULL;
}



/* 判断信息响应字段是否会引入被协议禁止的正文分帧。 */
static bool __xrtHttp1ServerResponseInformField(
	const xhttpfield* pField
)
{
	return !xrtHttpFieldNameEqual(
		pField->Name,
		XRT_STR_LITERAL("Content-Length")
	) && !xrtHttpFieldNameEqual(
		pField->Name,
		XRT_STR_LITERAL("Transfer-Encoding")
	) && !xrtHttpFieldNameEqual(
		pField->Name,
		XRT_STR_LITERAL("Trailer")
	);
}



/* 冻结一条没有正文且不结束当前请求的 HTTP/1.1 信息响应。 */
XRT_API xhttp1serverresponse* xrtHttp1ServerResponseInform(
	xhttpversion Version,
	const xhttpreply* pReply
)
{
	const xhttpheaders* pHeaders;
	const xhttpfield* pFields = NULL;
	const xhttpfield* pField;
	xhttp1serverresponse* pResponse;
	size_t iFieldCount;
	size_t iHeadSize;
	size_t i;

	if ( (pReply == NULL) ||
		((Version != XHTTP_VERSION_1_0) &&
		 (Version != XHTTP_VERSION_1_1)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (Version != XHTTP_VERSION_1_1) ||
		(xrtHttpReplyStatus(pReply) < 100) ||
		(xrtHttpReplyStatus(pReply) >= 200) ||
		(xrtHttpReplyStatus(pReply) == 101) ) {
		__xrtHttp1ServerResponsePrepareError(
			XHTTP1_SERVER_RESPONSE_ERROR_STATUS,
			XERR_PROTOCOL,
			"prepare-http1-server-information",
			"HTTP information requires HTTP/1.1 status 100..199 except 101"
		);
		return NULL;
	}
	if ( xrtHttpReplyBody(pReply) != NULL ) {
		__xrtHttp1ServerResponsePrepareError(
			XHTTP1_SERVER_RESPONSE_ERROR_BODY,
			XERR_PROTOCOL,
			"prepare-http1-server-information",
			"HTTP information cannot carry a body"
		);
		return NULL;
	}
	if ( xrtHttpReplyTrailerCount(pReply) != 0 ) {
		__xrtHttp1ServerResponsePrepareError(
			XHTTP1_SERVER_RESPONSE_ERROR_TRAILER,
			XERR_PROTOCOL,
			"prepare-http1-server-information",
			"HTTP information cannot carry Trailer fields"
		);
		return NULL;
	}

	iFieldCount = xrtHttpReplyHeaderCount(pReply);
	for ( i = 0; i < iFieldCount; i++ ) {
		pField = xrtHttpReplyHeaderAt(pReply, i);
		if ( (pField == NULL) ||
			!__xrtHttp1ServerResponseInformField(pField) ) {
			__xrtHttp1ServerResponsePrepareError(
				XHTTP1_SERVER_RESPONSE_ERROR_FRAMING,
				XERR_PROTOCOL,
				"prepare-http1-server-information",
				"HTTP information contains a forbidden framing field"
			);
			return NULL;
		}
	}
	pHeaders = xrtHttpReplyHeaders(pReply);
	if ( pHeaders != NULL ) {
		pFields = xrtHttpHeadersData(pHeaders);
	}
	if ( !xrtHttp1ResponseWrite(
		Version,
		xrtHttpReplyStatus(pReply),
		xrtHttpReplyReason(pReply),
		pFields,
		iFieldCount,
		NULL,
		0,
		&iHeadSize
	) ) {
		return NULL;
	}
	if ( sizeof(*pResponse) > (SIZE_MAX - iHeadSize) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pResponse = (xhttp1serverresponse*)xrtMalloc(
		sizeof(*pResponse) + iHeadSize
	);
	if ( pResponse == NULL ) {
		return NULL;
	}
	memset(pResponse, 0, sizeof(*pResponse));
	pResponse->Head = (bytes)(pResponse + 1);
	pResponse->HeadSize = iHeadSize;
	pResponse->Mode = XHTTP1_BODY_NONE;
	pResponse->State = XRT_HTTP_SERVER_RESPONSE_HEAD;
	pResponse->Informational = true;
	if ( !xrtHttp1ResponseWrite(
		Version,
		xrtHttpReplyStatus(pReply),
		xrtHttpReplyReason(pReply),
		pFields,
		iFieldCount,
		pResponse->Head,
		pResponse->HeadSize,
		&pResponse->HeadSize
	) ) {
		xrtHttp1ServerResponseDestroy(pResponse);
		return NULL;
	}
	return pResponse;
}



/* 按固定请求事实冻结借用型响应来源。 */
xhttp1serverresponse* __xrtHttp1ServerResponsePrepareSource(
	xhttpversion Version,
	xstrview Method,
	uint32 iRequestFlags,
	const xrt_http1_server_response_source* pSource
)
{
	xrt_http1_server_response_prepare Prepare;
	xhttpfield Length;
	xhttpfield Transfer;
	size_t iHeaders;
	size_t iUpgradeCount;
	size_t iCapacity;
	bool bTrailerDeclared;
	bool bHasLength;
	bool bHasTransfer;
	char sLength[21];
	xhttp1serverresponse* pResponse = NULL;
	uint32 iKnownFlags =
		XHTTP_SERVER_REQUEST_KEEP_ALIVE |
		XHTTP_SERVER_REQUEST_UPGRADE |
		XHTTP_SERVER_REQUEST_EXPECT_CONTINUE |
		XHTTP_SERVER_REQUEST_STREAMED |
		XHTTP_SERVER_REQUEST_COMPLETE |
		XHTTP_SERVER_REQUEST_DISCARDED;

	if ( (pSource == NULL) ||
		((Version != XHTTP_VERSION_1_0) &&
		 (Version != XHTTP_VERSION_1_1)) ||
		!xrtHttpTokenValid(Method) ||
		(pSource->Status < 100) ||
		(pSource->Status > 999) ||
		!xrtHttpFieldValueValid(pSource->Reason) ||
		!__xrtHttpFieldArrayValid(
			pSource->Headers,
			pSource->HeaderCount
		) || !__xrtHttpFieldArrayValid(
			pSource->Trailers,
			pSource->TrailerCount
		) || ((pSource->Body == NULL) &&
		 (pSource->BodyLength != 0)) ||
		((pSource->Body != NULL) &&
		 (xrtHttpBodyLength(pSource->Body) !=
		  pSource->BodyLength)) ||
		((iRequestFlags & ~iKnownFlags) != 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	memset(&Prepare, 0, sizeof(Prepare));
	Prepare.Source = pSource;
	Prepare.Version = Version;
	Prepare.Method = Method;
	Prepare.RequestFlags = iRequestFlags;
	iHeaders = pSource->HeaderCount;
	if ( (iHeaders > (SIZE_MAX - 4u)) ||
		((iHeaders + 4u) >
		 (SIZE_MAX / sizeof(xhttpfield))) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iCapacity = iHeaders + 4u;
	Prepare.Fields = (xhttpfield*)xrtMalloc(
		iCapacity * sizeof(xhttpfield)
	);
	if ( Prepare.Fields == NULL ) {
		return NULL;
	}
	Prepare.FieldCapacity = iCapacity;
	if ( !__xrtHttp1ServerResponseHeaders(
		&Prepare,
		&Length,
		&bHasLength,
		&Transfer,
		&bHasTransfer,
		&bTrailerDeclared,
		&iUpgradeCount
	) || !__xrtHttp1ServerResponseTrailers(
		&Prepare
	) || !__xrtHttp1ServerResponseStatus(
		&Prepare,
		pSource->Status,
		iUpgradeCount
	) || !__xrtHttp1ServerResponseBody(
		&Prepare,
		bHasLength ? &Length : NULL,
		bHasTransfer ? &Transfer : NULL,
		bTrailerDeclared
	) || !__xrtHttp1ServerResponseGeneratedFields(
		&Prepare, sLength
	) ) {
		__xrtHttp1ServerResponsePrepareUnit(&Prepare);
		return NULL;
	}
	pResponse = __xrtHttp1ServerResponsePlan(&Prepare);
	__xrtHttp1ServerResponsePrepareUnit(&Prepare);
	return pResponse;
}



/* 把拥有型 Reply 适配为借用来源并复用唯一冻结内核。 */
XRT_API xhttp1serverresponse* xrtHttp1ServerResponsePrepare(
	xhttpversion Version,
	xstrview Method,
	uint32 iRequestFlags,
	const xhttpreply* pReply
)
{
	xrt_http1_server_response_source Source;
	const xhttpheaders* pHeaders;
	const xhttpheaders* pTrailers;

	if ( pReply == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	memset(&Source, 0, sizeof(Source));
	pHeaders = xrtHttpReplyHeaders(pReply);
	pTrailers = xrtHttpReplyTrailers(pReply);
	Source.Status = xrtHttpReplyStatus(pReply);
	Source.Reason = xrtHttpReplyReason(pReply);
	Source.Headers = pHeaders != NULL ?
		xrtHttpHeadersData(pHeaders) : NULL;
	Source.HeaderCount = xrtHttpReplyHeaderCount(pReply);
	Source.Trailers = pTrailers != NULL ?
		xrtHttpHeadersData(pTrailers) : NULL;
	Source.TrailerCount = xrtHttpReplyTrailerCount(pReply);
	Source.Body = xrtHttpReplyBody(pReply);
	Source.BodyLength = Source.Body != NULL ?
		xrtHttpBodyLength(Source.Body) : 0;
	return __xrtHttp1ServerResponsePrepareSource(
		Version,
		Method,
		iRequestFlags,
		&Source
	);
}



/* 从拥有型请求快照提取请求事实并准备响应。 */
XRT_API xhttp1serverresponse* xrtHttp1ServerResponseCreate(
	const xhttpserverrequest* pRequest,
	const xhttpreply* pReply
)
{
	if ( (pRequest == NULL) || (pReply == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return xrtHttp1ServerResponsePrepare(
		xrtHttpServerRequestVersion(pRequest),
		xrtHttpServerRequestMethod(pRequest),
		xrtHttpServerRequestFlags(pRequest),
		pReply
	);
}

#endif
