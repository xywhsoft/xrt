#include "../internal/xrt_http_server.h"

#include <xrt/http_compress.h>



#if defined(XHTTP_FEATURE_HTTP_REPLY_COMPRESS)

#define XRT_HTTP_REPLY_COMPRESS_CODINGS \
	((uint32)XHTTP_CODING_GZIP | \
	 (uint32)XHTTP_CODING_DEFLATE)

#define XRT_HTTP_REPLY_COMPRESS_FLAGS \
	((uint32)XHTTP_REPLY_COMPRESS_ALLOW_ABSENT | \
	 (uint32)XHTTP_REPLY_COMPRESS_ALLOW_UNKNOWN_LENGTH | \
	 (uint32)XHTTP_REPLY_COMPRESS_ALLOW_ANY_TYPE | \
	 (uint32)XHTTP_REPLY_COMPRESS_IGNORE_NO_TRANSFORM | \
	 (uint32)XHTTP_REPLY_COMPRESS_KEEP_LARGER)



/* 保存一组与压缩选择相关的响应字段事实。 */
typedef struct xrt_http_reply_compress_fields {
	bool VaryCoding;
	bool VaryAny;
	bool KeepETag;
} xrt_http_reply_compress_fields;



/* 建立 Reply 压缩域错误。 */
void __xrtHttpReplyCompressError(
	xerrkind Kind,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = iCode;
	Desc.Domain = "http.reply.compress";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 向可能未对齐的公开输出槽一次性发布 Reply 指针。 */
static void __xrtHttpReplyCompressPublish(
	xhttpreply** ppOutput,
	xhttpreply* pOutput
)
{
	memcpy(ppOutput, &pOutput, sizeof(pOutput));
}



/* 把公开配置解析成完整默认值并一次验证全部边界。 */
static bool __xrtHttpReplyCompressConfigResolve(
	const xhttpreplycompressconfig* pInput,
	xhttpreplycompressconfig* pConfig
)
{
	xdeflateconfig Deflate;

	xrtHttpReplyCompressConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !__xrtRangeValid(
			pInput, sizeof(*pInput)
		) ) {
			__xrtHttpReplyCompressError(
				XERR_ARGUMENT,
				XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
				"config",
				"HTTP Reply compression config range is invalid"
			);
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	if ( ((pConfig->Codings &
		  ~XRT_HTTP_REPLY_COMPRESS_CODINGS) != 0) ||
		((pConfig->Preferred != XHTTP_CODING_NONE) &&
		 (pConfig->Preferred != XHTTP_CODING_GZIP) &&
		 (pConfig->Preferred != XHTTP_CODING_DEFLATE)) ||
		((pConfig->Preferred != XHTTP_CODING_NONE) &&
		 ((pConfig->Codings &
		   (uint32)pConfig->Preferred) == 0)) ||
		(pConfig->MinimumSize > pConfig->MaximumSize) ||
		(pConfig->ReadSize == 0) ||
		((pConfig->Flags &
		  ~XRT_HTTP_REPLY_COMPRESS_FLAGS) != 0) ) {
		__xrtHttpReplyCompressError(
			XERR_VALUE,
			XHTTP_REPLY_COMPRESS_ERROR_CONFIG,
			"config",
			"HTTP Reply compression configuration is invalid"
		);
		return false;
	}
	xrtDeflateConfigInit(&Deflate);
	Deflate.Format = XDEFLATE_GZIP;
	Deflate.Level = pConfig->Level;
	Deflate.Strategy = pConfig->Strategy;
	Deflate.OutputLimit = pConfig->OutputLimit;
	return xrtDeflateConfigValid(&Deflate);
}



/* 统计一个响应字段，并借出唯一字段。 */
static size_t __xrtHttpReplyCompressField(
	const xhttpreply* pReply,
	xstrview Name,
	const xhttpfield** ppField
)
{
	const xhttpfield* pOnly = NULL;
	size_t iCount = 0;
	size_t i;

	for ( i = 0; i < xrtHttpReplyHeaderCount(pReply); i++ ) {
		const xhttpfield* pField =
			xrtHttpReplyHeaderAt(pReply, i);

		if ( (pField != NULL) &&
			xrtHttpFieldNameEqual(pField->Name, Name) ) {
			pOnly = pField;
			iCount++;
		}
	}
	if ( ppField != NULL ) {
		*ppField = iCount == 1 ? pOnly : NULL;
	}
	return iCount;
}



/* 严格扫描全部 Cache-Control 字段并识别 no-transform。 */
static bool __xrtHttpReplyCompressNoTransform(
	const xhttpreply* pReply,
	bool* pNoTransform
)
{
	xhttpcachecontrol Cache;
	size_t i;

	xrtHttpCacheControlInit(&Cache);
	for ( i = 0; i < xrtHttpReplyHeaderCount(pReply); i++ ) {
		const xhttpfield* pField =
			xrtHttpReplyHeaderAt(pReply, i);

		if ( (pField != NULL) &&
			xrtHttpFieldNameEqual(
				pField->Name,
				XRT_STR_LITERAL("Cache-Control")
			) && !xrtHttpCacheControlAdd(
				&Cache, pField->Value
			) ) {
			__xrtHttpReplyCompressError(
				XERR_PROTOCOL,
				XHTTP_REPLY_COMPRESS_ERROR_HEADER,
				"cache-control",
				"HTTP Cache-Control field is malformed"
			);
			return false;
		}
	}
	if ( (Cache.Flags & XHTTP_CACHE_INVALID) != 0 ) {
		__xrtHttpReplyCompressError(
			XERR_PROTOCOL,
			XHTTP_REPLY_COMPRESS_ERROR_HEADER,
			"cache-control",
			"HTTP Cache-Control directive argument is invalid"
		);
		return false;
	}
	*pNoTransform =
		(Cache.Flags & XHTTP_CACHE_NO_TRANSFORM) != 0;
	return true;
}



/* 读取完整 Vary 计划和已有 Accept-Encoding 选择维度。 */
static bool __xrtHttpReplyCompressVary(
	const xhttpreply* pReply,
	xrt_http_reply_compress_fields* pFields
)
{
	const xhttpfield* pHeaders =
		pReply->Headers != NULL ?
			xrtHttpHeadersData(pReply->Headers) : NULL;
	size_t iCount = xrtHttpReplyHeaderCount(pReply);
	xhttpvaryplan Plan;
	xhttpvaryitem Item;
	xhttpnext Next;

	if ( !xrtHttpVaryPlan(
		pHeaders, iCount, &Plan
	) ) {
		__xrtHttpReplyCompressError(
			XERR_PROTOCOL,
			XHTTP_REPLY_COMPRESS_ERROR_HEADER,
			"vary",
			"HTTP Vary field is malformed"
		);
		return false;
	}
	Next = xrtHttpVaryFind(
		pHeaders,
		iCount,
		XRT_STR_LITERAL("Accept-Encoding"),
		&Item
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		__xrtHttpReplyCompressError(
			XERR_PROTOCOL,
			XHTTP_REPLY_COMPRESS_ERROR_HEADER,
			"vary",
			"HTTP Vary field is malformed"
		);
		return false;
	}
	pFields->VaryAny =
		(Plan.Flags & XHTTP_VARY_WILDCARD) != 0;
	pFields->VaryCoding = Next == XHTTP_NEXT_ITEM;
	return true;
}



/* 只让一个语法正确的弱 ETag 跨越表示编码变换。 */
static bool __xrtHttpReplyCompressKeepETag(
	const xhttpreply* pReply
)
{
	const xhttpfield* pField;
	xhttpetag Tag;
	xerror* pPrevious;
	bool bParsed;

	if ( __xrtHttpReplyCompressField(
		pReply, XRT_STR_LITERAL("ETag"), &pField
	) != 1 ) {
		return false;
	}
	pPrevious = xrtTakeError();
	bParsed = xrtHttpETagParse(pField->Value, &Tag);
	xrtClearError();
	if ( pPrevious != NULL ) {
		xrtSetError(pPrevious);
		xrtErrorFree(pPrevious);
	}
	return bParsed && Tag.Weak;
}



/* 判断状态、方法和已有表示元数据是否禁止自动变换。 */
static bool __xrtHttpReplyCompressHardSkip(
	xstrview Method,
	const xhttpreply* pReply,
	xhttpbody** ppBody,
	uint64* pLength
)
{
	uint16 iStatus = xrtHttpReplyStatus(pReply);
	xhttpbody* pBody = xrtHttpReplyBody(pReply);
	bool bHead = xrtHttpMethodEqual(
		Method, XRT_STR_LITERAL("HEAD")
	);

	if ( (!xrtHttpResponseContentAllowed(Method, iStatus) &&
		  !bHead) ||
		(iStatus == 206) ||
		(pBody == NULL) ||
		(xrtHttpReplyHeader(
			pReply, XRT_STR_LITERAL("Content-Encoding")
		 ) != NULL) ||
		(xrtHttpReplyHeader(
			pReply, XRT_STR_LITERAL("Content-Range")
		 ) != NULL) ) {
		return true;
	}
	*pLength = xrtHttpBodyLength(pBody);
	if ( *pLength == 0 ) {
		return true;
	}
	*ppBody = pBody;
	return false;
}



/* 判断未编码表示是否满足自动压缩策略。 */
static int __xrtHttpReplyCompressEligible(
	const xhttpreply* pReply,
	uint64 iLength,
	const xhttpreplycompressconfig* pConfig
)
{
	const xhttpfield* pContentType;
	xmediatype MediaType;
	bool bNoTransform;
	size_t iContentTypes;

	if ( !__xrtHttpReplyCompressNoTransform(
		pReply, &bNoTransform
	) ) {
		return -1;
	}
	if ( bNoTransform &&
		((pConfig->Flags &
		  XHTTP_REPLY_COMPRESS_IGNORE_NO_TRANSFORM) == 0) ) {
		return 0;
	}
	if ( iLength == XHTTP_BODY_UNKNOWN ) {
		if ( (pConfig->Flags &
			  XHTTP_REPLY_COMPRESS_ALLOW_UNKNOWN_LENGTH) == 0 ) {
			return 0;
		}
	} else if ( (iLength < pConfig->MinimumSize) ||
		(iLength > pConfig->MaximumSize) ) {
		return 0;
	}
	if ( pConfig->Codings == 0 ) {
		return 0;
	}
	if ( (pConfig->Flags &
		  XHTTP_REPLY_COMPRESS_ALLOW_ANY_TYPE) != 0 ) {
		return 1;
	}
	iContentTypes = __xrtHttpReplyCompressField(
		pReply,
		XRT_STR_LITERAL("Content-Type"),
		&pContentType
	);
	if ( iContentTypes == 0 ) {
		return 0;
	}
	if ( iContentTypes != 1 ) {
		__xrtHttpReplyCompressError(
			XERR_PROTOCOL,
			XHTTP_REPLY_COMPRESS_ERROR_RESPONSE,
			"content-type",
			"HTTP Reply has multiple Content-Type fields"
		);
		return -1;
	}
	if ( !xrtHttpMediaTypeParse(
		pContentType->Value, &MediaType
	) ) {
		return -1;
	}
	return xrtHttpMediaTypeCompressible(&MediaType) ? 1 : 0;
}



/* 根据协商编码建立 Deflate 配置。 */
static void __xrtHttpReplyCompressDeflate(
	const xhttpreplycompressconfig* pConfig,
	xhttpcoding Coding,
	xdeflateconfig* pDeflate
)
{
	xrtDeflateConfigInit(pDeflate);
	pDeflate->Format = Coding == XHTTP_CODING_GZIP ?
		XDEFLATE_GZIP : XDEFLATE_ZLIB;
	pDeflate->Level = pConfig->Level;
	pDeflate->Strategy = pConfig->Strategy;
	pDeflate->OutputLimit = pConfig->OutputLimit;
}



/* 创建一次性压缩正文；当更大的结果可以回退时返回空正文和 Identity。 */
static xhttpbody* __xrtHttpReplyCompressEager(
	xhttpbody* pSource,
	const xhttpacceptencoding* pAccept,
	const xhttpreplycompressconfig* pConfig,
	xhttpcoding Coding,
	bool* pIdentity
)
{
	xdeflateconfig Deflate;
	xbytesview Input;
	bytes pOutput;
	xhttpbody* pBody;
	size_t iOutput;

	*pIdentity = false;
	if ( !xrtHttpBodyView(pSource, &Input) ||
		(Input.Size > pConfig->EagerLimit) ) {
		return NULL;
	}
	__xrtHttpReplyCompressDeflate(
		pConfig, Coding, &Deflate
	);
	pOutput = xrtDeflateAll(Input, &Deflate, &iOutput);
	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( (iOutput >= Input.Size) &&
		((pConfig->Flags &
		  XHTTP_REPLY_COMPRESS_KEEP_LARGER) == 0) &&
		(xrtHttpAcceptEncodingQuality(
			pAccept, XHTTP_CODING_IDENTITY
		 ) != 0) ) {
		xrtFree(pOutput);
		*pIdentity = true;
		return NULL;
	}
	pBody = xrtHttpBodyTake(pOutput, iOutput);
	if ( pBody == NULL ) {
		xrtFree(pOutput);
	}
	return pBody;
}



/* 创建延迟压缩正文，不读取来源。 */
static xhttpbody* __xrtHttpReplyCompressStream(
	xhttpbody* pSource,
	const xhttpreplycompressconfig* pConfig,
	xhttpcoding Coding
)
{
	xhttpbodydeflateconfig Body;

	memset(&Body, 0, sizeof(Body));
	__xrtHttpReplyCompressDeflate(
		pConfig, Coding, &Body.Deflate
	);
	Body.ReadSize = pConfig->ReadSize;
	Body.QueueLimit = pConfig->QueueLimit;
	return xrtHttpBodyDeflate(pSource, &Body);
}



/* 克隆 identity Reply，并按需补充 Accept-Encoding 的缓存维度。 */
static xhttpreply* __xrtHttpReplyCompressIdentity(
	const xhttpreply* pReply,
	const xrt_http_reply_compress_fields* pFields
)
{
	xhttpreply* pOutput = xrtHttpReplyClone(pReply);

	if ( (pOutput != NULL) &&
		!pFields->VaryAny &&
		!pFields->VaryCoding &&
		!xrtHttpReplyAddHeader(
			pOutput,
			XRT_STR_LITERAL("Vary"),
			XRT_STR_LITERAL("Accept-Encoding")
		) ) {
		xrtHttpReplyDestroy(pOutput);
		return NULL;
	}
	return pOutput;
}



/* 删除内容编码变换后不再成立的表示和线路元数据。 */
static void __xrtHttpReplyCompressRemoveStale(
	xhttpreply* pReply,
	bool bKeepETag
)
{
	static const xstrview Headers[] = {
		XRT_STR_INIT("Content-Length"),
		XRT_STR_INIT("Transfer-Encoding"),
		XRT_STR_INIT("Content-MD5"),
		XRT_STR_INIT("Digest"),
		XRT_STR_INIT("Content-Digest"),
		XRT_STR_INIT("Repr-Digest"),
		XRT_STR_INIT("Accept-Ranges"),
		XRT_STR_INIT("Trailer")
	};
	static const xstrview Trailers[] = {
		XRT_STR_INIT("Content-MD5"),
		XRT_STR_INIT("Digest"),
		XRT_STR_INIT("Content-Digest"),
		XRT_STR_INIT("Repr-Digest")
	};
	size_t i;

	for ( i = 0;
		  i < (sizeof(Headers) / sizeof(Headers[0]));
		  i++ ) {
		(void)xrtHttpReplyRemoveHeader(
			pReply, Headers[i]
		);
	}
	for ( i = 0;
		  i < (sizeof(Trailers) / sizeof(Trailers[0]));
		  i++ ) {
		(void)xrtHttpReplyRemoveTrailer(
			pReply, Trailers[i]
		);
	}
	if ( !bKeepETag ) {
		(void)xrtHttpReplyRemoveHeader(
			pReply, XRT_STR_LITERAL("ETag")
		);
	}
}



/* 克隆 Reply、替换正文并提交内容编码元数据。 */
static xhttpreply* __xrtHttpReplyCompressApply(
	const xhttpreply* pReply,
	xhttpbody* pBody,
	xhttpcoding Coding,
	const xrt_http_reply_compress_fields* pFields
)
{
	xhttpreply* pOutput = __xrtHttpReplyCompressIdentity(
		pReply, pFields
	);

	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpReplySetBody(pOutput, pBody) ||
		!xrtHttpReplySetHeader(
			pOutput,
			XRT_STR_LITERAL("Content-Encoding"),
			xrtHttpCodingName(Coding)
		) ) {
		xrtHttpReplyDestroy(pOutput);
		return NULL;
	}
	__xrtHttpReplyCompressRemoveStale(
		pOutput, pFields->KeepETag
	);
	return pOutput;
}



/* 初始化稳定的自动压缩默认策略。 */
XRT_API void xrtHttpReplyCompressConfigInit(
	xhttpreplycompressconfig* pConfig
)
{
	xhttpreplycompressconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.Codings =
		XHTTP_CODING_GZIP |
		XHTTP_CODING_DEFLATE;
	Config.Preferred = XHTTP_CODING_GZIP;
	Config.MinimumSize =
		XHTTP_REPLY_COMPRESS_MIN_DEFAULT;
	Config.MaximumSize = UINT64_MAX;
	Config.EagerLimit =
		XHTTP_REPLY_COMPRESS_EAGER_DEFAULT;
	Config.ReadSize =
		XHTTP_BODY_DEFLATE_READ_DEFAULT;
	Config.QueueLimit =
		XHTTP_BODY_DEFLATE_QUEUE_DEFAULT;
	Config.Level = XDEFLATE_LEVEL_DEFAULT;
	Config.Strategy = XDEFLATE_STRATEGY_DEFAULT;
	Config.OutputLimit = XDEFLATE_OUTPUT_UNLIMITED;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 选择内容编码并失败原子地生成独立 Reply。 */
XRT_API xhttpreplycompressstatus xrtHttpReplyCompress(
	const xhttpacceptencoding* pAccept,
	xstrview Method,
	const xhttpreply* pReply,
	const xhttpreplycompressconfig* pConfig,
	xhttpreply** ppOutput
)
{
	xhttpacceptencoding Accept;
	xhttpreplycompressconfig Config;
	xrt_http_reply_compress_fields Fields;
	xhttpcoding Coding;
	xhttpbody* pSource = NULL;
	xhttpbody* pBody = NULL;
	xhttpreply* pOutput;
	uint64 iLength = XHTTP_BODY_UNKNOWN;
	int iEligible;
	bool bIdentity = false;

	if ( !__xrtRangeValid(ppOutput, sizeof(*ppOutput)) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress",
			"HTTP Reply compression output is invalid"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	if ( __xrtRangesOverlap(
			ppOutput, sizeof(*ppOutput),
			pAccept, sizeof(*pAccept)
		) ||
		__xrtRangesOverlap(
			ppOutput, sizeof(*ppOutput),
			pReply, sizeof(*pReply)
		) ||
		((pConfig != NULL) &&
		 __xrtRangesOverlap(
			ppOutput, sizeof(*ppOutput),
			pConfig, sizeof(*pConfig)
		 )) ||
		__xrtRangesOverlap(
			ppOutput, sizeof(*ppOutput),
			Method.Data, Method.Size
		) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress",
			"HTTP Reply compression arguments are invalid"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	__xrtHttpReplyCompressPublish(ppOutput, NULL);
	if ( !__xrtRangeValid(pAccept, sizeof(*pAccept)) ||
		(pReply == NULL) ||
		!xrtHttpTokenValid(Method) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress",
			"HTTP Reply compression arguments are invalid"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	memcpy(&Accept, pAccept, sizeof(Accept));
	if ( !xrtHttpAcceptEncodingValid(&Accept) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress",
			"HTTP Reply compression negotiation is invalid"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	if ( !__xrtHttpReplyCompressConfigResolve(
		pConfig, &Config
	) ) {
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	if ( __xrtHttpReplyCompressHardSkip(
		Method, pReply, &pSource, &iLength
	) ) {
		return XHTTP_REPLY_COMPRESS_SKIP;
	}
	iEligible = __xrtHttpReplyCompressEligible(
		pReply, iLength, &Config
	);
	if ( iEligible < 0 ) {
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	if ( iEligible == 0 ) {
		Coding = xrtHttpAcceptEncodingSelect(
			&Accept,
			XHTTP_CODING_IDENTITY,
			XHTTP_CODING_IDENTITY
		);
		return Coding == XHTTP_CODING_NONE ?
			XHTTP_REPLY_COMPRESS_NOT_ACCEPTABLE :
			XHTTP_REPLY_COMPRESS_SKIP;
	}
	memset(&Fields, 0, sizeof(Fields));
	if ( !__xrtHttpReplyCompressVary(
		pReply, &Fields
	) ) {
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	Fields.KeepETag =
		__xrtHttpReplyCompressKeepETag(pReply);
	if ( ((Accept.Flags &
		  XHTTP_ACCEPT_ENCODING_PRESENT) == 0) &&
		((Config.Flags &
		  XHTTP_REPLY_COMPRESS_ALLOW_ABSENT) == 0) ) {
		Coding = XHTTP_CODING_IDENTITY;
	} else {
		Coding = xrtHttpAcceptEncodingSelect(
			&Accept,
			Config.Codings | XHTTP_CODING_IDENTITY,
			Config.Preferred
		);
	}
	if ( Coding == XHTTP_CODING_NONE ) {
		return XHTTP_REPLY_COMPRESS_NOT_ACCEPTABLE;
	}
	if ( Coding == XHTTP_CODING_IDENTITY ) {
		pOutput = __xrtHttpReplyCompressIdentity(
			pReply, &Fields
		);
		if ( pOutput == NULL ) {
			return XHTTP_REPLY_COMPRESS_ERROR;
		}
		__xrtHttpReplyCompressPublish(ppOutput, pOutput);
		return XHTTP_REPLY_COMPRESS_IDENTITY;
	}
	pBody = __xrtHttpReplyCompressEager(
		pSource, &Accept, &Config, Coding, &bIdentity
	);
	if ( bIdentity ) {
		pOutput = __xrtHttpReplyCompressIdentity(
			pReply, &Fields
		);
		if ( pOutput == NULL ) {
			return XHTTP_REPLY_COMPRESS_ERROR;
		}
		__xrtHttpReplyCompressPublish(ppOutput, pOutput);
		return XHTTP_REPLY_COMPRESS_IDENTITY;
	}
	if ( pBody == NULL ) {
		xbytesview View;

		if ( xrtHttpBodyView(pSource, &View) &&
			(View.Size <= Config.EagerLimit) ) {
			return XHTTP_REPLY_COMPRESS_ERROR;
		}
		pBody = __xrtHttpReplyCompressStream(
			pSource, &Config, Coding
		);
		if ( pBody == NULL ) {
			return XHTTP_REPLY_COMPRESS_ERROR;
		}
	}
	pOutput = __xrtHttpReplyCompressApply(
		pReply, pBody, Coding, &Fields
	);
	xrtHttpBodyDestroy(pBody);
	if ( pOutput == NULL ) {
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	__xrtHttpReplyCompressPublish(ppOutput, pOutput);
	return XHTTP_REPLY_COMPRESS_APPLIED;
}

#endif
