#include "../internal/xrt_http_server_static.h"
#include "../internal/xrt_future.h"



#if defined(XRT_FEATURE_HTTP_SERVER_STATIC)

#define XRT_HTTP_STATIC_BOUNDARY_RANDOM 16u
#define XRT_HTTP_STATIC_BOUNDARY_PREFIX "xrt-"
#define XRT_HTTP_STATIC_BOUNDARY_PREFIX_SIZE 4u
#define XRT_HTTP_STATIC_BOUNDARY_SIZE \
	(XRT_HTTP_STATIC_BOUNDARY_PREFIX_SIZE + \
	 (XRT_HTTP_STATIC_BOUNDARY_RANDOM * 2u))

static const xstrview __xrtHttpServerStaticIndexes[] = {
	{ "index.html", sizeof("index.html") - 1u }
};



/* 异步组合上下文独立拥有请求、最终相对路径和全部可变配置文本。 */
typedef struct xrt_http_server_static_context {
	xhttpserverrequest* Request;
	str ContentType;
	str CacheControl;
	str Boundary;
	xhttpstaticreplyconfig Config;
} xrt_http_server_static_context;



/* 静态资源任务独立拥有全部候选路径，任务池和安全根只借用到任务结束。 */
typedef struct xrt_http_server_static_prepare {
	xtaskpool* Pool;
	xroot Root;
	str* Paths;
	size_t PathCount;
} xrt_http_server_static_prepare;



/* 静态资源结果把命中的路径与文件绑定，供 Reply 组装阶段共同消费。 */
typedef struct xrt_http_server_static_resource {
	xhttpstaticfile* File;
	str Path;
	size_t PathSize;
} xrt_http_server_static_resource;



/* 为静态服务阶段建立结构化错误，并接管当前错误作为原因。 */
static void __xrtHttpServerStaticError(
	xerrkind Kind,
	xhttpserverstaticerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ?
		xrtErrorKind(pCause) : Kind;
	Desc.Domain = "http.server.static";
	Desc.Code = (int32)Code;
	Desc.SystemCode = pCause != NULL ?
		xrtErrorSystemCode(pCause) : 0;
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



/* Future 成功值最终销毁完整 Reply。 */
static void __xrtHttpServerStaticReplyFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtHttpReplyDestroy((xhttpreply*)pValue);
}



/* 释放异步组合上下文及其独立请求引用和文本。 */
static void __xrtHttpServerStaticContextFree(
	ptr pValue,
	ptr pData
)
{
	xrt_http_server_static_context* pContext =
		(xrt_http_server_static_context*)pValue;

	(void)pData;
	if ( pContext == NULL ) {
		return;
	}
	xrtHttpServerRequestDestroy(pContext->Request);
	xrtFree(pContext->ContentType);
	xrtFree(pContext->CacheControl);
	xrtFree(pContext->Boundary);
	xrtFree(pContext);
}



/* 用当前错误拒绝 Promise；极端错误分配失败时安全关闭输出。 */
static void __xrtHttpServerStaticReject(xpromise* pOutput)
{
	xerror* pError = xrtTakeError();

	if ( pError == NULL ) {
		__xrtErrorSetInternal();
		pError = xrtTakeError();
	}
	if ( pError != NULL ) {
		(void)xrtPromiseReject(pOutput, pError);
	} else {
		(void)xrtPromiseClose(pOutput);
	}
	xrtErrorFree(pError);
}



/* 把一个完整 Reply 转移给输出 Promise。 */
static bool __xrtHttpServerStaticResolve(
	xpromise* pOutput,
	xhttpreply* pReply
)
{
	if ( (pOutput == NULL) || (pReply == NULL) ) {
		__xrtErrorSetInternal();
		return false;
	}
	return xrtPromiseResolveOwned(
		pOutput,
		pReply,
		__xrtHttpServerStaticReplyFree,
		NULL
	);
}



/* 创建已经以空正文状态码成功完成的 Reply Future。 */
static xfuture* __xrtHttpServerStaticStatusFuture(
	uint16 iStatus
)
{
	xhttpreply* pReply;
	xfuture* pFuture = NULL;
	xpromise* pPromise;

	pReply = xrtHttpReplyCreate(iStatus);
	if ( pReply == NULL ) {
		return NULL;
	}
	pPromise = xrtPromiseCreate(&pFuture, NULL);
	if ( pPromise == NULL ) {
		xrtHttpReplyDestroy(pReply);
		return NULL;
	}
	if ( !__xrtHttpServerStaticResolve(
		pPromise,
		pReply
	) ) {
		xrtHttpReplyDestroy(pReply);
		xrtPromiseDestroy(pPromise);
		xrtFutureDestroy(pFuture);
		return NULL;
	}
	xrtPromiseDestroy(pPromise);
	return pFuture;
}



/* 初始化静态 Reply 的独立默认策略。 */
XRT_API void xrtHttpStaticReplyConfigInit(
	xhttpstaticreplyconfig* pConfig
)
{
	xhttpstaticreplyconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtHttpStaticPlanConfigInit(&Config.Plan);
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化安全根挂载和常用目录索引。 */
XRT_API void xrtHttpStaticServeConfigInit(
	xhttpstaticserveconfig* pConfig
)
{
	xhttpstaticserveconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtHttpStaticPathConfigInit(&Config.Path);
	xrtHttpStaticReplyConfigInit(&Config.Reply);
	Config.Indexes = __xrtHttpServerStaticIndexes;
	Config.IndexCount = sizeof(__xrtHttpServerStaticIndexes) /
		sizeof(__xrtHttpServerStaticIndexes[0]);
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 验证静态 Reply 配置并填充缺省值。 */
static bool __xrtHttpStaticReplyConfigResolve(
	const xhttpstaticreplyconfig* pInput,
	xhttpstaticreplyconfig* pConfig
)
{
	xhttpstaticreplyconfig Config;

	xrtHttpStaticReplyConfigInit(&Config);
	if ( pInput != NULL ) {
		if ( !__xrtRangeValid(pInput, sizeof(Config)) ) {
			__xrtErrorSetInvalidArgument();
			__xrtHttpServerStaticError(
				XERR_ARGUMENT,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"invalid static Reply configuration range"
			);
			return false;
		}
		memcpy(&Config, pInput, sizeof(Config));
	}
	if ( !__xrtHttpViewValid(Config.ContentType) ||
		!__xrtHttpViewValid(Config.CacheControl) ||
		!__xrtHttpViewValid(Config.Boundary) ||
		(Config.Plan.MaxRanges >
		 (SIZE_MAX / sizeof(xhttpbyterange))) ) {
		__xrtErrorSetInvalidArgument();
		__xrtHttpServerStaticError(
			XERR_ARGUMENT,
			XHTTP_SERVER_STATIC_ERROR_CONFIG,
			"config",
			"invalid static Reply configuration"
		);
		return false;
	}
	memcpy(pConfig, &Config, sizeof(Config));
	return true;
}



/* 验证一站式服务配置，包括挂载点、索引名称和 Reply 策略。 */
static bool __xrtHttpStaticServeConfigResolve(
	const xhttpstaticserveconfig* pInput,
	xhttpstaticserveconfig* pConfig
)
{
	xstrview Index;
	size_t iMapped = 0;
	size_t i;
	bool bTrailing = false;

	xrtHttpStaticServeConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !__xrtRangeValid(pInput, sizeof(*pConfig)) ) {
			__xrtErrorSetInvalidArgument();
			__xrtHttpServerStaticError(
				XERR_ARGUMENT,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"invalid static service configuration range"
			);
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	if ( !__xrtHttpStaticReplyConfigResolve(
		&pConfig->Reply,
		&pConfig->Reply
	) ) {
		return false;
	}
	if ( (pConfig->IndexCount >
		 (SIZE_MAX / sizeof(xstrview))) ||
		((pConfig->IndexCount != 0) &&
		 !__xrtRangeValid(
			pConfig->Indexes,
			pConfig->IndexCount * sizeof(xstrview)
		 )) ) {
		__xrtErrorSetInvalidArgument();
		__xrtHttpServerStaticError(
			XERR_ARGUMENT,
			XHTTP_SERVER_STATIC_ERROR_CONFIG,
			"config",
			"invalid static directory index array"
		);
		return false;
	}

	/* 用挂载点自身执行一次无分配映射，统一复用路径模块的完整配置校验。 */
	if ( xrtHttpStaticPathWrite(
		pConfig->Path.Mount,
		&pConfig->Path,
		NULL,
		0,
		&iMapped,
		&bTrailing
	) == XHTTP_STATIC_PATH_ERROR ) {
		__xrtHttpServerStaticError(
			XERR_ARGUMENT,
			XHTTP_SERVER_STATIC_ERROR_CONFIG,
			"config",
			"invalid static path configuration"
		);
		return false;
	}
	for ( i = 0; i < pConfig->IndexCount; i++ ) {
		memcpy(
			&Index,
			pConfig->Indexes + i,
			sizeof(Index)
		);
		if ( !__xrtHttpViewValid(Index) ) {
			__xrtErrorSetInvalidArgument();
			__xrtHttpServerStaticError(
				XERR_ARGUMENT,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"invalid static directory index view"
			);
			return false;
		}
		if ( Index.Size == 0 ) {
			__xrtErrorSetValue();
			__xrtHttpServerStaticError(
				XERR_VALUE,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"empty static directory index"
			);
			return false;
		}
		if ( (memchr(Index.Data, '/', Index.Size) != NULL) ||
			(memchr(Index.Data, '\\', Index.Size) != NULL) ) {
			__xrtErrorSetValue();
			__xrtHttpServerStaticError(
				XERR_VALUE,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"static directory index contains a path separator"
			);
			return false;
		}
		if ( !xrtPathIsSafeEntry(Index, false) ) {
			__xrtErrorSetValue();
			__xrtHttpServerStaticError(
				XERR_VALUE,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"static directory index is not a portable file name"
			);
			return false;
		}
		if ( ((pConfig->Path.Flags &
			XHTTP_STATIC_PATH_ALLOW_HIDDEN) == 0u) &&
			(Index.Data[0] == '.') ) {
			__xrtErrorSetValue();
			__xrtHttpServerStaticError(
				XERR_VALUE,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"hidden static directory index is disabled"
			);
			return false;
		}
	}
	return true;
}



/* 复制可选文本，并让目标视图指向独立存储。 */
static bool __xrtHttpServerStaticViewCopy(
	xstrview Source,
	str* ppStorage,
	xstrview* pTarget
)
{
	if ( Source.Size == 0 ) {
		*ppStorage = NULL;
		*pTarget = (xstrview){ NULL, 0 };
		return true;
	}
	*ppStorage = xrtStrDupView(Source);
	if ( *ppStorage == NULL ) {
		return false;
	}
	*pTarget = (xstrview){ *ppStorage, Source.Size };
	return true;
}



/* 为 multipart/byteranges 生成 128 位随机 token boundary。 */
static bool __xrtHttpServerStaticBoundary(
	char sBoundary[XRT_HTTP_STATIC_BOUNDARY_SIZE + 1u],
	xstrview* pBoundary
)
{
	uint8 Random[XRT_HTTP_STATIC_BOUNDARY_RANDOM];
	size_t iSize = 0;

	if ( !xrtSecureRandom(Random, sizeof(Random)) ) {
		__xrtHttpServerStaticError(
			XERR_IO,
			XHTTP_SERVER_STATIC_ERROR_BOUNDARY,
			"boundary",
			"failed to generate a multipart boundary"
		);
		return false;
	}
	memcpy(
		sBoundary,
		XRT_HTTP_STATIC_BOUNDARY_PREFIX,
		XRT_HTTP_STATIC_BOUNDARY_PREFIX_SIZE
	);
	if ( !xrtHexEncode(
		Random,
		sizeof(Random),
		sBoundary + XRT_HTTP_STATIC_BOUNDARY_PREFIX_SIZE,
		sizeof(Random) * 2u + 1u,
		&iSize,
		0
	) || (iSize != (sizeof(Random) * 2u)) ) {
		memset(Random, 0, sizeof(Random));
		__xrtHttpServerStaticError(
			XERR_INTERNAL,
			XHTTP_SERVER_STATIC_ERROR_BOUNDARY,
			"boundary",
			"failed to encode a multipart boundary"
		);
		return false;
	}
	memset(Random, 0, sizeof(Random));
	pBoundary->Data = sBoundary;
	pBoundary->Size = XRT_HTTP_STATIC_BOUNDARY_SIZE;
	return true;
}



/* 只复制静态响应状态和字段，正文由上层在最后一步安装。 */
/* 快照并验证纯静态响应描述符及其全部借用字段。 */
static bool __xrtHttpServerStaticResponseRead(
	const xhttpstaticresponse* pInput,
	xhttpstaticresponse* pResponse
)
{
	size_t i;

	if ( !__xrtRangeValid(pInput, sizeof(*pResponse)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pResponse, pInput, sizeof(*pResponse));
	if ( (pResponse->FieldCount >
		 XHTTP_STATIC_RESPONSE_MAX_FIELDS) ||
		!__xrtHttpViewValid(pResponse->ContentType) ||
		!__xrtHttpViewValid(pResponse->Boundary) ||
		!__xrtHttpFieldArrayValid(
			pResponse->Fields,
			pResponse->FieldCount
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < pResponse->FieldCount; i++ ) {
		if ( !xrtHttpTokenValid(
				pResponse->Fields[i].Name
			) || !xrtHttpFieldValueValid(
				pResponse->Fields[i].Value
			) ) {
			__xrtErrorSetValue();
			return false;
		}
	}
	if ( ((pResponse->ContentType.Size != 0) &&
		 !xrtHttpFieldValueValid(pResponse->ContentType)) ||
		(pResponse->Multipart &&
		 ((pResponse->Boundary.Size == 0) ||
		  !xrtHttpTokenValid(pResponse->Boundary))) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



static xhttpreply* __xrtHttpReplyStaticFields(
	const xhttpstaticresponse* pResponse
)
{
	xhttpreply* pReply;
	size_t i;

	pReply = xrtHttpReplyCreate(pResponse->Status);
	if ( pReply == NULL ) {
		return NULL;
	}
	for ( i = 0; i < pResponse->FieldCount; i++ ) {
		if ( !xrtHttpReplyAddHeader(
			pReply,
			pResponse->Fields[i].Name,
			pResponse->Fields[i].Value
		) ) {
			xrtHttpReplyDestroy(pReply);
			return NULL;
		}
	}
	return pReply;
}



/* 把静态协议描述桥接为拥有型 Reply。 */
XRT_API xhttpreply* xrtHttpReplyFromStatic(
	const xhttpstaticresponse* pResponse,
	xhttpbody* pBody
)
{
	xhttpstaticresponse Response;
	xhttpreply* pReply;
	uint64 iLength;

	if ( !__xrtHttpServerStaticResponseRead(
		pResponse,
		&Response
	) ) {
		return NULL;
	}
	if ( Response.SendBody != (pBody != NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pBody != NULL ) {
		iLength = xrtHttpBodyLength(pBody);
		if ( (iLength == XHTTP_BODY_UNKNOWN) ||
			(iLength != Response.BodyLength) ) {
			__xrtErrorSetRange();
			return NULL;
		}
	}
	pReply = __xrtHttpReplyStaticFields(&Response);
	if ( (pReply == NULL) ||
		((pBody != NULL) &&
		 !xrtHttpReplySetBody(pReply, pBody)) ) {
		xrtHttpReplyDestroy(pReply);
		return NULL;
	}
	return pReply;
}



/* 根据最终资源路径选择显式或内置媒体类型。 */
static xstrview __xrtHttpServerStaticContentType(
	xstrview ResourcePath,
	const xhttpstaticreplyconfig* pConfig
)
{
	xstrview ContentType;

	if ( pConfig->ContentType.Size != 0 ) {
		return pConfig->ContentType;
	}
	ContentType = xrtMimeByPath(ResourcePath);
	return ContentType.Size != 0 ?
		ContentType :
		XRT_STR_LITERAL("application/octet-stream");
}



/* 按静态计划从同一个安全文件资源取走准确正文。 */
static xhttpbody* __xrtHttpServerStaticBody(
	xhttpstaticfile* pFile,
	const xhttpstaticplan* pPlan,
	const xhttpbyterange* pRanges,
	xstrview ContentType,
	xstrview Boundary
)
{
	uint64 iLength;

	if ( pPlan->RangeCount == 0 ) {
		return xrtHttpStaticFileTakeBodyAll(pFile);
	}
	if ( pRanges == NULL ) {
		__xrtErrorSetInternal();
		return NULL;
	}
	if ( pPlan->RangeCount == 1u ) {
		iLength =
			(pRanges[0].Last - pRanges[0].First) +
			UINT64_C(1);
		return xrtHttpStaticFileTakeBody(
			pFile,
			pRanges[0].First,
			iLength
		);
	}
	return xrtHttpStaticFileTakeMultipartBody(
		pFile,
		pRanges,
		pPlan->RangeCount,
		ContentType,
		Boundary
	);
}



/* 构建请求对应的完整静态文件 Reply。 */
XRT_API xhttpreply* xrtHttpReplyStatic(
	const xhttpserverrequest* pRequest,
	xhttpstaticfile* pFile,
	xstrview ResourcePath,
	const xhttpstaticreplyconfig* pInput
)
{
	xhttpstaticreplyconfig Config;
	xhttpstaticresponseconfig ResponseConfig;
	const xhttprepresentation* pCurrent;
	xhttpstaticresponse Response;
	xhttpstaticplan Plan;
	xhttpbyterange* pRanges = NULL;
	xhttpreply* pReply = NULL;
	xhttpbody* pBody = NULL;
	char* sWorkspace = NULL;
	char sBoundary[XRT_HTTP_STATIC_BOUNDARY_SIZE + 1u];
	xstrview ContentType;
	xstrview Boundary;
	size_t iWorkspace = 0;
	size_t iRangeBytes;

	if ( (pRequest == NULL) || (pFile == NULL) ||
		!__xrtHttpViewValid(ResourcePath) ||
		!__xrtHttpStaticReplyConfigResolve(
			pInput,
			&Config
		) ) {
		if ( (pRequest == NULL) || (pFile == NULL) ||
			!__xrtHttpViewValid(ResourcePath) ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	iRangeBytes =
		Config.Plan.MaxRanges * sizeof(xhttpbyterange);
	if ( iRangeBytes != 0 ) {
		pRanges = (xhttpbyterange*)xrtMalloc(iRangeBytes);
		if ( pRanges == NULL ) {
			return NULL;
		}
	}
	pCurrent = xrtHttpStaticFileRepresentation(pFile);
	if ( (pCurrent == NULL) ||
		!xrtHttpStaticPlanBuild(
			pRequest->Method,
			pRequest->Fields,
			pRequest->FieldCount,
			pCurrent,
			xrtHttpStaticFileSize(pFile),
			pRanges,
			Config.Plan.MaxRanges,
			&Config.Plan,
			&Plan
		) ) {
		__xrtHttpServerStaticError(
			XERR_PROTOCOL,
			XHTTP_SERVER_STATIC_ERROR_PLAN,
			"plan",
			"failed to build the static response plan"
		);
		xrtFree(pRanges);
		return NULL;
	}
	ContentType = __xrtHttpServerStaticContentType(
		ResourcePath,
		&Config
	);
	Boundary = Config.Boundary;
	if ( (Plan.RangeCount > 1u) &&
		(Boundary.Size == 0) &&
		!__xrtHttpServerStaticBoundary(
			sBoundary,
			&Boundary
		) ) {
		xrtFree(pRanges);
		return NULL;
	}
	xrtHttpStaticResponseConfigInit(&ResponseConfig);
	ResponseConfig.ContentType = ContentType;
	ResponseConfig.CacheControl = Config.CacheControl;
	ResponseConfig.Boundary = Boundary;
	if ( !xrtHttpStaticResponseBuild(
		&Plan,
		pRanges,
		pCurrent,
		&ResponseConfig,
		NULL,
		0,
		&iWorkspace,
		NULL
	) ) {
		__xrtHttpServerStaticError(
			XERR_PROTOCOL,
			XHTTP_SERVER_STATIC_ERROR_RESPONSE,
			"response",
			"failed to measure the static response"
		);
		xrtFree(pRanges);
		return NULL;
	}
	if ( iWorkspace != 0 ) {
		sWorkspace = (char*)xrtMalloc(iWorkspace);
		if ( sWorkspace == NULL ) {
			xrtFree(pRanges);
			return NULL;
		}
	}
	if ( !xrtHttpStaticResponseBuild(
		&Plan,
		pRanges,
		pCurrent,
		&ResponseConfig,
		sWorkspace,
		iWorkspace,
		&iWorkspace,
		&Response
	) ) {
		__xrtHttpServerStaticError(
			XERR_PROTOCOL,
			XHTTP_SERVER_STATIC_ERROR_RESPONSE,
			"response",
			"failed to build the static response"
		);
		xrtFree(sWorkspace);
		xrtFree(pRanges);
		return NULL;
	}
	pReply = __xrtHttpReplyStaticFields(&Response);
	if ( pReply == NULL ) {
		__xrtHttpServerStaticError(
			XERR_MEMORY,
			XHTTP_SERVER_STATIC_ERROR_REPLY,
			"reply",
			"failed to create the static Reply"
		);
		xrtFree(sWorkspace);
		xrtFree(pRanges);
		return NULL;
	}

	/* 最后才取走文件正文，前面的可失败分配不会消耗静态文件。 */
	if ( Response.SendBody ) {
		pBody = __xrtHttpServerStaticBody(
			pFile,
			&Plan,
			pRanges,
			ContentType,
			Boundary
		);
		if ( (pBody == NULL) ||
			!xrtHttpReplySetBody(pReply, pBody) ) {
			__xrtHttpServerStaticError(
				XERR_IO,
				XHTTP_SERVER_STATIC_ERROR_BODY,
				"body",
				"failed to create the static response body"
			);
			xrtHttpBodyDestroy(pBody);
			xrtHttpReplyDestroy(pReply);
			xrtFree(sWorkspace);
			xrtFree(pRanges);
			return NULL;
		}
		xrtHttpBodyDestroy(pBody);
	}
	xrtFree(sWorkspace);
	xrtFree(pRanges);
	return pReply;
}



/* 判断打开失败是否应按静态资源不可见处理。 */
static bool __xrtHttpServerStaticNotFound(
	const xerror* pError
)
{
	xerrkind Kind;

	if ( pError == NULL ) {
		return false;
	}
	Kind = xrtErrorKind(pError);
	return (Kind == XERR_NOT_FOUND) ||
		(Kind == XERR_PERMISSION) ||
		(Kind == XERR_TYPE);
}



/* 把源 Future 的非成功终态原样发布到输出 Promise。 */
static void __xrtHttpServerStaticForward(
	const xfutureresult* pInput,
	xpromise* pOutput
)
{
	if ( (pInput == NULL) || (pOutput == NULL) ) {
		if ( pOutput != NULL ) {
			(void)xrtPromiseClose(pOutput);
		}
		return;
	}
	switch ( pInput->State ) {
		case XFUTURE_FAILED:
			if ( pInput->Error != NULL ) {
				(void)xrtPromiseReject(
					pOutput,
					pInput->Error
				);
			} else {
				(void)xrtPromiseClose(pOutput);
			}
			break;

		case XFUTURE_CANCELLED:
			(void)xrtPromiseCancel(pOutput);
			break;

		case XFUTURE_CLOSED:
			(void)xrtPromiseClose(pOutput);
			break;

		default:
			(void)xrtPromiseClose(pOutput);
			break;
	}
}



/* 文件打开终态转换为静态 Reply，同时把不可见资源规范化为 404。 */
static void __xrtHttpServerStaticFileReady(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	xrt_http_server_static_context* pContext =
		(xrt_http_server_static_context*)pData;
	xrt_http_server_static_resource* pResource;
	xhttpreply* pReply;

	if ( (pInput != NULL) &&
		(pInput->State == XFUTURE_FAILED) &&
		__xrtHttpServerStaticNotFound(pInput->Error) ) {
		pReply = xrtHttpReplyCreate(
			XHTTP_STATUS_NOT_FOUND
		);
		if ( pReply == NULL ) {
			__xrtHttpServerStaticReject(pOutput);
		} else if ( !__xrtHttpServerStaticResolve(
			pOutput,
			pReply
		) ) {
			xrtHttpReplyDestroy(pReply);
		}
		return;
	}
	if ( (pInput == NULL) ||
		(pInput->State != XFUTURE_RESOLVED) ) {
		__xrtHttpServerStaticForward(
			pInput,
			pOutput
		);
		return;
	}
	pResource = (xrt_http_server_static_resource*)
		pInput->Value;
	if ( (pContext == NULL) ||
		(pContext->Request == NULL) ||
		(pResource == NULL) ||
		(pResource->File == NULL) ||
		(pResource->Path == NULL) ) {
		__xrtErrorSetInternal();
		__xrtHttpServerStaticReject(pOutput);
		return;
	}
	pReply = xrtHttpReplyStatic(
		pContext->Request,
		pResource->File,
		(xstrview){
			pResource->Path,
			pResource->PathSize
		},
		&pContext->Config
	);
	if ( pReply == NULL ) {
		__xrtHttpServerStaticReject(pOutput);
		return;
	}
	if ( !__xrtHttpServerStaticResolve(
		pOutput,
		pReply
	) ) {
		xrtHttpReplyDestroy(pReply);
	}
}



/* 为异步回调复制必要的请求引用和 Reply 配置文本。 */
static xrt_http_server_static_context*
	__xrtHttpServerStaticContextCreate(
		const xhttpserverrequest* pRequest,
		const xhttpstaticreplyconfig* pConfig
	)
{
	xrt_http_server_static_context* pContext;

	pContext = (xrt_http_server_static_context*)xrtCalloc(
		1,
		sizeof(xrt_http_server_static_context)
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Request = xrtHttpServerRequestRef(
		(xhttpserverrequest*)pRequest
	);
	if ( pContext->Request == NULL ) {
		__xrtHttpServerStaticContextFree(
			pContext,
			NULL
		);
		return NULL;
	}
	pContext->Config = *pConfig;
	if ( !__xrtHttpServerStaticViewCopy(
		pConfig->ContentType,
		&pContext->ContentType,
		&pContext->Config.ContentType
	) || !__xrtHttpServerStaticViewCopy(
		pConfig->CacheControl,
		&pContext->CacheControl,
		&pContext->Config.CacheControl
	) || !__xrtHttpServerStaticViewCopy(
		pConfig->Boundary,
		&pContext->Boundary,
		&pContext->Config.Boundary
	) ) {
		__xrtHttpServerStaticContextFree(
			pContext,
			NULL
		);
		return NULL;
	}
	return pContext;
}



/* 释放任务持有的全部候选路径。 */
static void __xrtHttpServerStaticPrepareFree(
	ptr pValue,
	ptr pData
)
{
	xrt_http_server_static_prepare* pPrepare =
		(xrt_http_server_static_prepare*)pValue;
	size_t i;

	(void)pData;
	if ( pPrepare == NULL ) {
		return;
	}
	for ( i = 0; i < pPrepare->PathCount; i++ ) {
		xrtFree(pPrepare->Paths[i]);
	}
	xrtFree(pPrepare->Paths);
	xrtFree(pPrepare);
}



/* 释放已命中的静态资源和尚未被 Reply 正文取走的文件。 */
static void __xrtHttpServerStaticResourceFree(
	ptr pValue,
	ptr pData
)
{
	xrt_http_server_static_resource* pResource =
		(xrt_http_server_static_resource*)pValue;

	(void)pData;
	if ( pResource == NULL ) {
		return;
	}
	xrtHttpStaticFileDestroy(pResource->File);
	xrtFree(pResource->Path);
	xrtFree(pResource);
}



/* 为普通文件或尾随斜杠目录构造有序候选路径。 */
static xrt_http_server_static_prepare*
	__xrtHttpServerStaticPrepareCreate(
		xtaskpool* pPool,
		xroot Root,
		const xhttpstaticpath* pPath,
		const xstrview* pIndexes,
		size_t iIndexCount
	)
{
	xrt_http_server_static_prepare* pPrepare;
	xstrview Index;
	str sIndex;
	size_t iCount = pPath->TrailingSlash ?
		iIndexCount : 1u;
	size_t i;

	if ( (iCount == 0) ||
		(iCount > (SIZE_MAX / sizeof(str))) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pPrepare = (xrt_http_server_static_prepare*)xrtCalloc(
		1,
		sizeof(xrt_http_server_static_prepare)
	);
	if ( pPrepare == NULL ) {
		return NULL;
	}
	pPrepare->Paths = (str*)xrtCalloc(iCount, sizeof(str));
	if ( pPrepare->Paths == NULL ) {
		xrtFree(pPrepare);
		return NULL;
	}
	pPrepare->Pool = pPool;
	pPrepare->Root = Root;
	pPrepare->PathCount = iCount;
	if ( !pPath->TrailingSlash ) {
		pPrepare->Paths[0] = xrtStrDup(pPath->Path);
		if ( pPrepare->Paths[0] == NULL ) {
			__xrtHttpServerStaticPrepareFree(pPrepare, NULL);
			return NULL;
		}
		return pPrepare;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(&Index, pIndexes + i, sizeof(Index));
		sIndex = xrtStrDupView(Index);
		if ( sIndex != NULL ) {
			pPrepare->Paths[i] = xrtPathJoin(
				pPath->Path,
				sIndex
			);
			xrtFree(sIndex);
		}
		if ( pPrepare->Paths[i] == NULL ) {
			__xrtHttpServerStaticPrepareFree(pPrepare, NULL);
			return NULL;
		}
	}
	return pPrepare;
}



/* 在同一任务池工作线程内按顺序查找首个可见目录索引。 */
static xtaskoutcome __xrtHttpServerStaticPrepareTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	xrt_http_server_static_prepare* pPrepare =
		(xrt_http_server_static_prepare*)pData;
	xrt_http_server_static_resource* pResource;
	xhttpstaticfile* pFile;
	const xerror* pError;
	size_t i;

	if ( (pPrepare == NULL) || (pResult == NULL) ) {
		__xrtErrorSetInternal();
		return XTASK_FAILED;
	}
	for ( i = 0; i < pPrepare->PathCount; i++ ) {
		if ( xrtCancelRequested(pCancel) ) {
			return XTASK_CANCELLED;
		}
		pFile = xrtHttpStaticFileOpen(
			pPrepare->Pool,
			pPrepare->Root,
			pPrepare->Paths[i]
		);
		if ( pFile != NULL ) {
			if ( xrtCancelRequested(pCancel) ) {
				xrtHttpStaticFileDestroy(pFile);
				return XTASK_CANCELLED;
			}
			pResource =
				(xrt_http_server_static_resource*)xrtCalloc(
					1,
					sizeof(xrt_http_server_static_resource)
				);
			if ( pResource == NULL ) {
				xrtHttpStaticFileDestroy(pFile);
				return XTASK_FAILED;
			}
			pResource->File = pFile;
			pResource->Path = pPrepare->Paths[i];
			pResource->PathSize = strlen(pResource->Path);
			pPrepare->Paths[i] = NULL;
			pResult->Value = pResource;
			pResult->Destroy =
				__xrtHttpServerStaticResourceFree;
			return XTASK_SUCCESS;
		}
		pError = xrtGetError();
		if ( !__xrtHttpServerStaticNotFound(pError) ||
			((i + 1u) == pPrepare->PathCount) ) {
			return xrtCancelRequested(pCancel) ?
				XTASK_CANCELLED : XTASK_FAILED;
		}
		xrtClearError();
	}
	__xrtErrorSetInternal();
	return XTASK_FAILED;
}



/* 提交一次普通文件打开或有序目录索引查找。 */
static xfuture* __xrtHttpServerStaticPrepareFuture(
	xtaskpool* pPool,
	xroot Root,
	const xhttpstaticpath* pPath,
	const xstrview* pIndexes,
	size_t iIndexCount
)
{
	xrt_http_server_static_prepare* pPrepare;
	xtaskargs Args;
	xfuture* pFuture;

	pPrepare = __xrtHttpServerStaticPrepareCreate(
		pPool,
		Root,
		pPath,
		pIndexes,
		iIndexCount
	);
	if ( pPrepare == NULL ) {
		return NULL;
	}
	memset(&Args, 0, sizeof(Args));
	Args.Destroy = __xrtHttpServerStaticPrepareFree;
	pFuture = xrtTaskSubmit(
		pPool,
		__xrtHttpServerStaticPrepareTask,
		pPrepare,
		&Args
	);
	if ( pFuture == NULL ) {
		__xrtHttpServerStaticPrepareFree(pPrepare, NULL);
	}
	return pFuture;
}



/* 映射请求并启动根内文件准备 Future。 */
XRT_API xfuture* xrtHttpReplyStaticFuture(
	xtaskpool* pPool,
	xroot Root,
	const xhttpserverrequest* pRequest,
	const xhttpstaticserveconfig* pInput
)
{
	xhttpstaticserveconfig Config;
	xrt_http_server_static_context* pContext;
	xhttpstaticpath Path;
	xhttptarget Target;
	xfuture* pFileFuture;
	xfuture* pReplyFuture;

	if ( (pPool == NULL) || (Root == NULL) ||
		(pRequest == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtHttpStaticServeConfigResolve(
		pInput,
		&Config
	) ) {
		return NULL;
	}
	if ( !xrtHttpServerRequestParseTarget(
		pRequest,
		&Target
	) ) {
		xrtClearError();
		return __xrtHttpServerStaticStatusFuture(
			XHTTP_STATUS_BAD_REQUEST
		);
	}
	if ( (Target.Form != XHTTP_TARGET_ORIGIN) &&
		(Target.Form != XHTTP_TARGET_ABSOLUTE) ) {
		return __xrtHttpServerStaticStatusFuture(
			XHTTP_STATUS_NOT_FOUND
		);
	}
	xrtHttpStaticPathInit(&Path);
	if ( !xrtHttpStaticPathMap(
		Target.Uri.Path,
		&Config.Path,
		&Path
	) ) {
		const xerror* pError = xrtGetError();

		if ( (pError != NULL) &&
			(xrtErrorKind(pError) == XERR_VALUE) ) {
			xrtClearError();
			return __xrtHttpServerStaticStatusFuture(
				XHTTP_STATUS_NOT_FOUND
			);
		}
		__xrtHttpServerStaticError(
			XERR_IO,
			XHTTP_SERVER_STATIC_ERROR_PATH,
			"path",
			"failed to map the static request path"
		);
		return NULL;
	}
	if ( !Path.Matched ) {
		xrtHttpStaticPathFree(&Path);
		return __xrtHttpServerStaticStatusFuture(
			XHTTP_STATUS_NOT_FOUND
		);
	}
	if ( Path.TrailingSlash &&
		(Config.IndexCount == 0) ) {
		xrtHttpStaticPathFree(&Path);
		return __xrtHttpServerStaticStatusFuture(
			XHTTP_STATUS_NOT_FOUND
		);
	}
	pContext = __xrtHttpServerStaticContextCreate(
		pRequest,
		&Config.Reply
	);
	if ( pContext == NULL ) {
		xrtHttpStaticPathFree(&Path);
		return NULL;
	}
	pFileFuture = __xrtHttpServerStaticPrepareFuture(
		pPool,
		Root,
		&Path,
		Config.Indexes,
		Config.IndexCount
	);
	xrtHttpStaticPathFree(&Path);
	if ( pFileFuture == NULL ) {
		__xrtHttpServerStaticContextFree(
			pContext,
			NULL
		);
		return NULL;
	}
	pReplyFuture = __xrtFutureContinueOwnedCancelSource(
		pFileFuture,
		__xrtHttpServerStaticFileReady,
		pContext,
		__xrtHttpServerStaticContextFree,
		NULL
	);
	xrtFutureDestroy(pFileFuture);
	if ( pReplyFuture == NULL ) {
		__xrtHttpServerStaticContextFree(
			pContext,
			NULL
		);
	}
	return pReplyFuture;
}



/* 把当前请求的一站式静态 Reply Future 交给 Connection。 */
XRT_API bool xrtHttpConnStatic(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	xroot Root,
	const xhttpstaticserveconfig* pConfig
)
{
	const xhttpserverrequest* pRequest;
	xfuture* pFuture;
	bool bResult;

	if ( pConnection == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pRequest = xrtHttpConnRequest(pConnection);
	if ( pRequest == NULL ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pFuture = xrtHttpReplyStaticFuture(
		pPool,
		Root,
		pRequest,
		pConfig
	);
	if ( pFuture == NULL ) {
		return false;
	}
	bResult = xrtHttpConnRespondFuture(
		pConnection,
		pFuture
	);
	xrtFutureDestroy(pFuture);
	return bResult;
}

#endif
