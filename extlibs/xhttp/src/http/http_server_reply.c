#include "../internal/xrt_http_server.h"



#if defined(XHTTP_FEATURE_HTTP_SERVER_REPLY)

/* 使网络适配器能检查输出别名而无需知道 Reply 的布局。 */
XHTTP_API bool xrtHttpBridgeReplyOutputValid(
	const xhttpreply* pReply, const void* pOutput, size_t iSize
)
{
	if ( !xrtMemRangeValid(pOutput, iSize) ) {
		return false;
	}
	if ( pReply == NULL || iSize == 0 ) {
		return true;
	}
	return !xrtMemRangesOverlap(pReply, sizeof(*pReply), pOutput, iSize) &&
		!xrtMemRangesOverlap(pReply->Reason.Data, pReply->Reason.Size, pOutput, iSize) &&
		!__xrtHttpHeadersOwnedOverlap(pReply->Headers, pOutput, iSize) &&
		!__xrtHttpHeadersOwnedOverlap(pReply->Trailers, pOutput, iSize);
}

/* 解析默认配置并在创建空 Reply 时完成全部配置校验。 */
static bool __xrtHttpReplyConfigResolve(
	const xhttpreplyconfig* pInput,
	xhttpreplyconfig* pConfig
)
{
	xrtHttpReplyConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !xrtMemRangeValid(pInput, sizeof(*pConfig)) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	return xrtHttpHeadersConfigValid(
		&pConfig->Headers
	) && xrtHttpHeadersConfigValid(
		&pConfig->Trailers
	);
}



/* 验证状态码能够写成三位 HTTP 状态。 */
static bool __xrtHttpReplyStatusValid(uint16 iStatus)
{
	if ( (iStatus < 100) || (iStatus > 999) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	return true;
}



/* 为首次字段修改创建容器，失败时不改变 Reply 可见状态。 */
static xhttpheaders* __xrtHttpReplyFieldsCreate(
	xhttpheaders** ppFields,
	const xhttpheadersconfig* pConfig
)
{
	xhttpheaders* pFields;

	if ( *ppFields != NULL ) {
		return *ppFields;
	}
	pFields = xrtHttpHeadersCreate(pConfig);
	if ( pFields == NULL ) {
		return NULL;
	}
	*ppFields = pFields;
	return pFields;
}



/* 首次添加字段时把创建与修改作为一个提交单元。 */
static bool __xrtHttpReplyFieldAdd(
	xhttpheaders** ppFields,
	const xhttpheadersconfig* pConfig,
	xstrview Name,
	xstrview Value
)
{
	xhttpheaders* pFields;

	if ( *ppFields != NULL ) {
		return xrtHttpHeadersAdd(*ppFields, Name, Value);
	}
	pFields = xrtHttpHeadersCreate(pConfig);
	if ( pFields == NULL ) {
		return false;
	}
	if ( !xrtHttpHeadersAdd(pFields, Name, Value) ) {
		xrtHttpHeadersDestroy(pFields);
		return false;
	}
	*ppFields = pFields;
	return true;
}



/* 首次设置字段时把创建与修改作为一个提交单元。 */
static bool __xrtHttpReplyFieldSet(
	xhttpheaders** ppFields,
	const xhttpheadersconfig* pConfig,
	xstrview Name,
	xstrview Value
)
{
	xhttpheaders* pFields;

	if ( *ppFields != NULL ) {
		return xrtHttpHeadersSet(*ppFields, Name, Value);
	}
	pFields = xrtHttpHeadersCreate(pConfig);
	if ( pFields == NULL ) {
		return false;
	}
	if ( !xrtHttpHeadersSet(pFields, Name, Value) ) {
		xrtHttpHeadersDestroy(pFields);
		return false;
	}
	*ppFields = pFields;
	return true;
}



/* 没有字段容器时仍严格验证删除名称。 */
static bool __xrtHttpReplyFieldNameValid(xstrview Name)
{
	if ( (Name.Data == NULL) && (Name.Size != 0) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !xrtHttpTokenValid(Name) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	return true;
}



/* 查询按需字段容器，并让字段名校验不依赖容器是否已经创建。 */
static const xhttpfield* __xrtHttpReplyFieldGet(
	const xhttpheaders* pFields,
	xstrview Name
)
{
	if ( pFields != NULL ) {
		return xrtHttpHeadersGet(pFields, Name);
	}
	(void)__xrtHttpReplyFieldNameValid(Name);
	return NULL;
}



/* 初始化不预留字段内存的服务端 Reply 配置。 */
XHTTP_API void xrtHttpReplyConfigInit(xhttpreplyconfig* pConfig)
{
	xhttpreplyconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtHttpHeadersConfigInit(&Config.Headers);
	xrtHttpHeadersConfigInit(&Config.Trailers);
	Config.Headers.InitialFields = 0;
	Config.Headers.InitialBytes = 0;
	Config.Trailers.InitialFields = 0;
	Config.Trailers.InitialBytes = 0;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 使用已解析配置创建一个空正文 Reply。 */
XHTTP_API xhttpreply* xrtHttpReplyCreateWithConfig(
	uint16 iStatus,
	const xhttpreplyconfig* pConfig
)
{
	xhttpreplyconfig Config;
	xhttpreply* pReply;

	if ( !__xrtHttpReplyStatusValid(iStatus) ||
		!__xrtHttpReplyConfigResolve(pConfig, &Config) ) {
		return NULL;
	}
	pReply = (xhttpreply*)xrtCalloc(1, sizeof(*pReply));
	if ( pReply == NULL ) {
		return NULL;
	}
	pReply->Config = Config;
	pReply->Status = iStatus;
	pReply->Reason = xrtHttpStatusText(iStatus);
	return pReply;
}



/* 使用默认按需分配配置创建 Reply。 */
XHTTP_API xhttpreply* xrtHttpReplyCreate(uint16 iStatus)
{
	return xrtHttpReplyCreateWithConfig(iStatus, NULL);
}



/* 克隆 Reply 的拥有型状态，并共享不可变正文来源。 */
XHTTP_API xhttpreply* xrtHttpReplyClone(const xhttpreply* pReply)
{
	xhttpreply* pClone;

	if ( pReply == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pClone = xrtHttpReplyCreateWithConfig(
		pReply->Status, &pReply->Config
	);
	if ( pClone == NULL ) {
		return NULL;
	}
	if ( pReply->CustomReason &&
		!xrtHttpReplySetReason(pClone, pReply->Reason) ) {
		xrtHttpReplyDestroy(pClone);
		return NULL;
	}
	if ( pReply->Headers != NULL ) {
		pClone->Headers = xrtHttpHeadersClone(pReply->Headers);
		if ( pClone->Headers == NULL ) {
			xrtHttpReplyDestroy(pClone);
			return NULL;
		}
	}
	if ( pReply->Trailers != NULL ) {
		pClone->Trailers = xrtHttpHeadersClone(pReply->Trailers);
		if ( pClone->Trailers == NULL ) {
			xrtHttpReplyDestroy(pClone);
			return NULL;
		}
	}
	if ( pReply->Body != NULL ) {
		pClone->Body = xrtHttpBodyRef(pReply->Body);
		if ( pClone->Body == NULL ) {
			xrtHttpReplyDestroy(pClone);
			return NULL;
		}
	}
	return pClone;
}



/* 销毁 Reply 的全部按需资产。 */
XHTTP_API void xrtHttpReplyDestroy(xhttpreply* pReply)
{
	if ( pReply == NULL ) {
		return;
	}
	xrtHttpBodyDestroy(pReply->Body);
	xrtHttpHeadersDestroy(pReply->Trailers);
	xrtHttpHeadersDestroy(pReply->Headers);
	xrtFree(pReply->ReasonStorage);
	memset(pReply, 0, sizeof(*pReply));
	xrtFree(pReply);
}



/* 设置状态码并恢复静态标准原因短语。 */
XHTTP_API bool xrtHttpReplySetStatus(
	xhttpreply* pReply,
	uint16 iStatus
)
{
	if ( (pReply == NULL) ||
		!__xrtHttpReplyStatusValid(iStatus) ) {
		if ( pReply == NULL ) {
			xrtSetErrorKind(XERR_ARGUMENT);
		}
		return false;
	}
	xrtFree(pReply->ReasonStorage);
	pReply->ReasonStorage = NULL;
	pReply->Status = iStatus;
	pReply->CustomReason = false;
	pReply->Reason = xrtHttpStatusText(iStatus);
	return true;
}



/* 返回当前状态码。 */
XHTTP_API uint16 xrtHttpReplyStatus(const xhttpreply* pReply)
{
	return pReply != NULL ? pReply->Status : 0;
}



/* 复制自定义原因短语并在成功后一次替换。 */
XHTTP_API bool xrtHttpReplySetReason(
	xhttpreply* pReply,
	xstrview Reason
)
{
	str sReason = NULL;

	if ( (pReply == NULL) ||
		!xrtHttpFieldValueValid(Reason) ) {
		if ( pReply == NULL ) {
			xrtSetErrorKind(XERR_ARGUMENT);
		}
		return false;
	}
	if ( Reason.Size != 0 ) {
		sReason = (str)xrtMalloc(Reason.Size);
		if ( sReason == NULL ) {
			return false;
		}
		memcpy(sReason, Reason.Data, Reason.Size);
	}
	xrtFree(pReply->ReasonStorage);
	pReply->ReasonStorage = sReason;
	pReply->CustomReason = true;
	pReply->Reason.Data = sReason;
	pReply->Reason.Size = Reason.Size;
	return true;
}



/* 返回当前有效原因短语。 */
XHTTP_API xstrview xrtHttpReplyReason(const xhttpreply* pReply)
{
	return pReply != NULL ?
		pReply->Reason : (xstrview){ NULL, 0 };
}



/* 追加拥有型 Header。 */
XHTTP_API bool xrtHttpReplyAddHeader(
	xhttpreply* pReply,
	xstrview Name,
	xstrview Value
)
{
	if ( pReply == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return __xrtHttpReplyFieldAdd(
		&pReply->Headers,
		&pReply->Config.Headers,
		Name,
		Value
	);
}



/* 设置并折叠同名 Header。 */
XHTTP_API bool xrtHttpReplySetHeader(
	xhttpreply* pReply,
	xstrview Name,
	xstrview Value
)
{
	if ( pReply == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return __xrtHttpReplyFieldSet(
		&pReply->Headers,
		&pReply->Config.Headers,
		Name,
		Value
	);
}



/* 删除全部同名 Header。 */
XHTTP_API size_t xrtHttpReplyRemoveHeader(
	xhttpreply* pReply,
	xstrview Name
)
{
	if ( pReply == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return 0;
	}
	if ( pReply->Headers == NULL ) {
		(void)__xrtHttpReplyFieldNameValid(Name);
		return 0;
	}
	return xrtHttpHeadersRemove(pReply->Headers, Name);
}



/* 查找首个同名 Header。 */
XHTTP_API const xhttpfield* xrtHttpReplyHeader(
	const xhttpreply* pReply,
	xstrview Name
)
{
	return pReply != NULL ?
		__xrtHttpReplyFieldGet(pReply->Headers, Name) : NULL;
}



/* 返回 Header 数量。 */
XHTTP_API size_t xrtHttpReplyHeaderCount(const xhttpreply* pReply)
{
	return (pReply != NULL) && (pReply->Headers != NULL) ?
		xrtHttpHeadersCount(pReply->Headers) : 0;
}



/* 返回 Reply 拥有的连续只读 Header 数组。 */
XHTTP_API const xhttpfield* xrtHttpReplyHeaderData(
	const xhttpreply* pReply
)
{
	return (pReply != NULL) && (pReply->Headers != NULL) ?
		xrtHttpHeadersData(pReply->Headers) : NULL;
}



/* 返回指定位置的 Header。 */
XHTTP_API const xhttpfield* xrtHttpReplyHeaderAt(
	const xhttpreply* pReply,
	size_t iIndex
)
{
	return (pReply != NULL) && (pReply->Headers != NULL) ?
		xrtHttpHeadersAt(pReply->Headers, iIndex) : NULL;
}



/* 返回只读 Header 容器而不触发创建。 */
XHTTP_API const xhttpheaders* xrtHttpReplyHeaders(
	const xhttpreply* pReply
)
{
	return pReply != NULL ? pReply->Headers : NULL;
}



/* 按需创建并返回可修改 Header 容器。 */
XHTTP_API xhttpheaders* xrtHttpReplyEditHeaders(xhttpreply* pReply)
{
	if ( pReply == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return __xrtHttpReplyFieldsCreate(
		&pReply->Headers, &pReply->Config.Headers
	);
}



/* 追加拥有型 Trailer。 */
XHTTP_API bool xrtHttpReplyAddTrailer(
	xhttpreply* pReply,
	xstrview Name,
	xstrview Value
)
{
	if ( pReply == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return __xrtHttpReplyFieldAdd(
		&pReply->Trailers,
		&pReply->Config.Trailers,
		Name,
		Value
	);
}



/* 设置并折叠同名 Trailer。 */
XHTTP_API bool xrtHttpReplySetTrailer(
	xhttpreply* pReply,
	xstrview Name,
	xstrview Value
)
{
	if ( pReply == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return __xrtHttpReplyFieldSet(
		&pReply->Trailers,
		&pReply->Config.Trailers,
		Name,
		Value
	);
}



/* 删除全部同名 Trailer。 */
XHTTP_API size_t xrtHttpReplyRemoveTrailer(
	xhttpreply* pReply,
	xstrview Name
)
{
	if ( pReply == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return 0;
	}
	if ( pReply->Trailers == NULL ) {
		(void)__xrtHttpReplyFieldNameValid(Name);
		return 0;
	}
	return xrtHttpHeadersRemove(pReply->Trailers, Name);
}



/* 查找首个同名 Trailer。 */
XHTTP_API const xhttpfield* xrtHttpReplyTrailer(
	const xhttpreply* pReply,
	xstrview Name
)
{
	return pReply != NULL ?
		__xrtHttpReplyFieldGet(pReply->Trailers, Name) : NULL;
}



/* 返回 Trailer 数量。 */
XHTTP_API size_t xrtHttpReplyTrailerCount(const xhttpreply* pReply)
{
	return (pReply != NULL) && (pReply->Trailers != NULL) ?
		xrtHttpHeadersCount(pReply->Trailers) : 0;
}



/* 返回 Reply 拥有的连续只读 Trailer 数组。 */
XHTTP_API const xhttpfield* xrtHttpReplyTrailerData(
	const xhttpreply* pReply
)
{
	return (pReply != NULL) && (pReply->Trailers != NULL) ?
		xrtHttpHeadersData(pReply->Trailers) : NULL;
}



/* 返回指定位置的 Trailer。 */
XHTTP_API const xhttpfield* xrtHttpReplyTrailerAt(
	const xhttpreply* pReply,
	size_t iIndex
)
{
	return (pReply != NULL) && (pReply->Trailers != NULL) ?
		xrtHttpHeadersAt(pReply->Trailers, iIndex) : NULL;
}



/* 返回只读 Trailer 容器而不触发创建。 */
XHTTP_API const xhttpheaders* xrtHttpReplyTrailers(
	const xhttpreply* pReply
)
{
	return pReply != NULL ? pReply->Trailers : NULL;
}



/* 按需创建并返回可修改 Trailer 容器。 */
XHTTP_API xhttpheaders* xrtHttpReplyEditTrailers(xhttpreply* pReply)
{
	if ( pReply == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return __xrtHttpReplyFieldsCreate(
		&pReply->Trailers, &pReply->Config.Trailers
	);
}



/* 替换正文并保留独立引用。 */
XHTTP_API bool xrtHttpReplySetBody(
	xhttpreply* pReply,
	xhttpbody* pBody
)
{
	xhttpbody* pNext = NULL;

	if ( pReply == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( pBody != NULL ) {
		pNext = xrtHttpBodyRef(pBody);
		if ( pNext == NULL ) {
			return false;
		}
	}
	xrtHttpBodyDestroy(pReply->Body);
	pReply->Body = pNext;
	return true;
}



/* 复制常用完整正文，并可同时设置 Content-Type。 */
XHTTP_API bool xrtHttpReplySetBytes(
	xhttpreply* pReply,
	xbytesview Data,
	xstrview ContentType
)
{
	xhttpbody* pBody;

	if ( pReply == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	pBody = xrtHttpBodyCopy(Data);
	if ( pBody == NULL ) {
		return false;
	}
	if ( (ContentType.Size != 0) &&
		!xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Content-Type"),
			ContentType
		) ) {
		xrtHttpBodyDestroy(pBody);
		return false;
	}
	xrtHttpBodyDestroy(pReply->Body);
	pReply->Body = pBody;
	return true;
}



/* 返回借用正文对象。 */
XHTTP_API xhttpbody* xrtHttpReplyBody(const xhttpreply* pReply)
{
	return pReply != NULL ? pReply->Body : NULL;
}

#endif
