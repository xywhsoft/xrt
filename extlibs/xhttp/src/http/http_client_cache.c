#include "../internal/xrt_http_client_runtime.h"

#include <stdio.h>



#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)

#define XRT_HTTP_CLIENT_CACHE_COMMIT_RETRIES 8u
#define XRT_HTTP_CLIENT_CACHE_BODY_MIN_CAPACITY 256u



/* 把最终缓存来源发布到并发诊断快照。 */
static void __xrtHttpClientCacheOutcome(
	xhttpcall* pCall,
	xhttpclientcacheoutcome Outcome
)
{
	xrtAtomic32Store(
		&pCall->Info.Cache,
		(uint32)Outcome,
		XMEMORY_RELEASE
	);
}



/* 建立稳定的缓存域错误并标记严格模式回调失败。 */
static bool __xrtHttpClientCacheError(
	xhttpcall* pCall,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	pCall->CacheFailed = true;
	__xrtHttpClientSetError(
		Kind,
		XHTTP_CLIENT_ERROR_CACHE,
		sOperation,
		sMessage,
		pCause
	);
	return false;
}



/* 宽松模式清除存储后端错误，严格模式把错误保留给终态。 */
static bool __xrtHttpClientCacheRecover(
	xhttpcall* pCall,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();

	if ( !pCall->Client->Config.Cache.Strict ) {
		xrtErrorFree(pCause);
		xrtClearError();
		return true;
	}
	(void)__xrtHttpClientCacheError(
		pCall,
		__xrtHttpClientCauseKind(
			pCause,
			XERR_IO
		),
		sOperation,
		sMessage,
		pCause
	);
	xrtErrorFree(pCause);
	return false;
}



/* 复制允许为空的分区文本并追加调试用零字符。 */
static str __xrtHttpClientCacheText(
	xstrview Text
)
{
	str sCopy;

	if ( (Text.Data == NULL) && (Text.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( Text.Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sCopy = (str)xrtMalloc(Text.Size + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	if ( Text.Size != 0 ) {
		memcpy(sCopy, Text.Data, Text.Size);
	}
	sCopy[Text.Size] = '\0';
	return sCopy;
}



/* 返回不含 fragment 的请求目标 URI，保持其他词法形式不变。 */
static xstrview __xrtHttpClientCacheURI(
	const xhttprequest* pRequest
)
{
	xstrview URI = xrtHttpRequestUrlText(pRequest);
	cstr sFragment;

	if ( (URI.Data == NULL) || (URI.Size == 0) ) {
		return (xstrview){ NULL, 0 };
	}
	sFragment = (cstr)memchr(URI.Data, '#', URI.Size);
	if ( sFragment != NULL ) {
		URI.Size = (size_t)(sFragment - URI.Data);
	}
	return URI;
}



/* 释放一跳缓存状态但保留调用级模式和分区。 */
static void __xrtHttpClientCacheReset(
	xhttpcall* pCall
)
{
	xrtHttpCacheRecordRelease(pCall->CacheCandidate);
	xrtHttpHeadersDestroy(pCall->CacheRequestFields);
	xrtHttpHeadersDestroy(pCall->CacheResponseFields);
	xrtFree(pCall->CacheBody);
	xrtFree(pCall->CacheRanges);
	pCall->CacheCandidate = NULL;
	pCall->CacheRequestFields = NULL;
	pCall->CacheResponseFields = NULL;
	pCall->CacheBody = NULL;
	pCall->CacheRanges = NULL;
	pCall->CacheBodySize = 0;
	pCall->CacheBodyCapacity = 0;
	pCall->CacheRangeCount = 0;
	pCall->CacheRequestClock = 0;
	pCall->CacheResponseClock = 0;
	pCall->CacheRangeBodyLength = 0;
	pCall->CacheResponseTime = 0;
	pCall->CacheRange = (xhttpbyterange){ 0, 0 };
	memset(
		pCall->CacheBoundary,
		0,
		sizeof(pCall->CacheBoundary)
	);
	pCall->CacheRangeState =
		__XRT_HTTP_CLIENT_CACHE_RANGE_NONE;
	pCall->CacheReady = false;
	pCall->CacheCapture = false;
	pCall->CacheRangeRequest = false;
	pCall->CacheRangeCovered = false;
	pCall->CacheRangeFill = false;
	pCall->CacheValidating = false;
	pCall->CacheNotModified = false;
	pCall->CacheIfNoneMatch = false;
	pCall->CacheIfModifiedSince = false;
	pCall->CacheIfRange = false;
	pCall->CacheFailed = false;
}



/* 用指定方法构造借用冻结请求字段的完整缓存主键。 */
static bool __xrtHttpClientCacheKeyMethod(
	xhttpcall* pCall,
	xstrview Method,
	xhttpcachekey* pKey
)
{
	xstrview URI = __xrtHttpClientCacheURI(pCall->Request);

	if ( !xrtHttpCacheKeyInit(
		pKey,
		Method,
		URI
	) ) {
		return false;
	}
	pKey->Partition = (xstrview){
		pCall->CachePartitionKey,
		pCall->CachePartitionSize
	};
	pKey->Fields = xrtHttpHeadersData(
		pCall->CacheRequestFields
	);
	pKey->FieldCount = xrtHttpHeadersCount(
		pCall->CacheRequestFields
	);
	return true;
}



/* 用当前请求方法构造完整缓存主键。 */
static bool __xrtHttpClientCacheKey(
	xhttpcall* pCall,
	xhttpcachekey* pKey
)
{
	return __xrtHttpClientCacheKeyMethod(
		pCall,
		xrtHttpRequestMethod(pCall->Request),
		pKey
	);
}



/* 重新读取指定方法的当前匹配记录并替换调用持有的旧快照。 */
static xhttpcachelookup __xrtHttpClientCacheReload(
	xhttpcall* pCall,
	xstrview Method
)
{
	xhttpcacherecord* pRecord = NULL;
	xhttpcachekey Key;
	xhttpcachelookup Lookup;

	if ( !__xrtHttpClientCacheKeyMethod(
		pCall,
		Method,
		&Key
	) ) {
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	Lookup = xrtHttpCacheGet(
		pCall->Client->Cache,
		&Key,
		&pRecord
	);
	if ( Lookup == XHTTP_CACHE_LOOKUP_ERROR ) {
		return Lookup;
	}
	xrtHttpCacheRecordRelease(pCall->CacheCandidate);
	pCall->CacheCandidate = pRecord;
	return Lookup;
}



/* 判断记录是否已经无空洞覆盖完整表示。 */
static bool __xrtHttpClientCacheRecordComplete(
	const xhttpcacherecord* pRecord
)
{
	return (xrtHttpCacheRecordFlags(pRecord) &
		XHTTP_CACHE_RECORD_COMPLETE) != 0;
}



/* 判断当前请求是否为可以由 GET 元数据服务的 HEAD。 */
static bool __xrtHttpClientCacheHeadRequest(
	const xhttpcall* pCall
)
{
	return xrtHttpMethodEqual(
		xrtHttpRequestMethod(pCall->Request),
		XRT_STR_LITERAL("HEAD")
	);
}



/* 判断 HEAD 当前选择的是已有 GET 表示，而不是独立 HEAD 记录。 */
static bool __xrtHttpClientCacheHeadFromGet(
	const xhttpcall* pCall
)
{
	const xhttpcachekey* pKey;

	if ( !__xrtHttpClientCacheHeadRequest(pCall) ||
		(pCall->CacheCandidate == NULL) ) {
		return false;
	}
	pKey = xrtHttpCacheRecordKey(pCall->CacheCandidate);
	return (pKey != NULL) &&
		xrtHttpMethodEqual(
			pKey->Method,
			XRT_STR_LITERAL("GET")
		);
}



/* 判断请求 If-Range 是否与缓存记录的强验证器匹配。 */
static bool __xrtHttpClientCacheIfRangeMatch(
	const xhttpcacherecord* pRecord,
	const xhttpfield* pField,
	bool* pMatch
)
{
	xhttpcacheentry Entry;
	xhttpcacheifrange Plan;
	xhttpcacheifrangekind Kind;
	xhttprepresentation Current;

	*pMatch = pField == NULL;
	if ( pField == NULL ) {
		return true;
	}
	if ( !xrtHttpCacheRecordEntry(
		pRecord,
		&Entry
	) ) {
		return false;
	}
	Kind = xrtHttpCacheIfRangePlan(&Entry, &Plan);
	if ( Kind == XHTTP_CACHE_IF_RANGE_ERROR ) {
		return false;
	}
	if ( Kind == XHTTP_CACHE_IF_RANGE_NONE ) {
		return true;
	}

	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	if ( Kind == XHTTP_CACHE_IF_RANGE_ETAG ) {
		Current.HasETag = true;
		Current.ETag = Plan.ETag;
	} else {
		Current.HasLastModified = true;
		Current.LastModifiedStrong = true;
		Current.LastModified = Plan.Date;
	}
	*pMatch = xrtHttpIfRangeMatch(
		pField->Value,
		&Current
	);
	return true;
}



/* 判断有序正文片段是否连续覆盖请求闭区间。 */
static bool __xrtHttpClientCacheRangeCovered(
	const xhttpcacherecord* pRecord,
	const xhttpbyterange* pRange
)
{
	uint64 iNext = pRange->First;
	size_t i;

	for ( i = 0;
		i < xrtHttpCacheRecordPartCount(pRecord);
		i++ ) {
		const xhttpcachepart* pPart =
			xrtHttpCacheRecordPartAt(pRecord, i);
		uint64 iLast;

		if ( pPart == NULL ) {
			return false;
		}
		iLast = pPart->Offset +
			(uint64)pPart->Data.Size -
			UINT64_C(1);
		if ( iLast < iNext ) {
			continue;
		}
		if ( pPart->Offset > iNext ) {
			return false;
		}
		if ( iLast >= pRange->Last ) {
			return true;
		}
		iNext = iLast + UINT64_C(1);
	}
	return false;
}



/* 返回调用当前规范化范围数组；单范围直接使用内联存储。 */
static const xhttpbyterange* __xrtHttpClientCacheRanges(
	const xhttpcall* pCall
)
{
	if ( pCall->CacheRangeCount == 1u ) {
		return &pCall->CacheRange;
	}
	return pCall->CacheRanges;
}



/* 判断缓存记录是否完整覆盖全部规范化请求范围。 */
static bool __xrtHttpClientCacheRangesCovered(
	const xhttpcacherecord* pRecord,
	const xhttpbyterange* pRanges,
	size_t iRangeCount
)
{
	size_t i;

	for ( i = 0; i < iRangeCount; i++ ) {
		if ( !__xrtHttpClientCacheRangeCovered(
			pRecord,
			&pRanges[i]
		) ) {
			return false;
		}
	}
	return true;
}



/* 返回缓存表示的原始媒体类型，缺失时由 multipart 编码器采用默认值。 */
static xstrview __xrtHttpClientCacheContentType(
	const xhttpcacherecord* pRecord
)
{
	const xhttpfield* pField = xrtHttpCacheRecordField(
		pRecord,
		XRT_STR_LITERAL("Content-Type")
	);

	return pField != NULL ?
		pField->Value :
		(xstrview){ NULL, 0 };
}



/* 为一次本地 multipart/byteranges 重放生成 128 位随机 token boundary。 */
static bool __xrtHttpClientCacheBoundary(
	xhttpcall* pCall
)
{
	uint8 Random[XRT_HTTP_CLIENT_CACHE_BOUNDARY_RANDOM_SIZE];
	size_t iEncoded = 0;

	if ( !xrtSecureRandom(Random, sizeof(Random)) ) {
		memset(Random, 0, sizeof(Random));
		return false;
	}
	memcpy(
		pCall->CacheBoundary,
		XRT_HTTP_CLIENT_CACHE_BOUNDARY_PREFIX,
		XRT_HTTP_CLIENT_CACHE_BOUNDARY_PREFIX_SIZE
	);
	if ( !xrtHexEncode(
			Random,
			sizeof(Random),
			pCall->CacheBoundary +
				XRT_HTTP_CLIENT_CACHE_BOUNDARY_PREFIX_SIZE,
			(sizeof(Random) * 2u) + 1u,
			&iEncoded,
			0
		) || (iEncoded != (sizeof(Random) * 2u)) ) {
		memset(Random, 0, sizeof(Random));
		memset(
			pCall->CacheBoundary,
			0,
			sizeof(pCall->CacheBoundary)
		);
		return false;
	}
	memset(Random, 0, sizeof(Random));
	pCall->CacheBoundary[
		XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE
	] = '\0';
	return true;
}



/*
	把 bytes Range 集合解析为缓存交付状态。
	非法、重复、超限和未知完整长度均保守回源，不改变请求语义。
*/
static bool __xrtHttpClientCacheRangePrepare(
	xhttpcall* pCall,
	bool* pUsable
)
{
	const xhttpfield* pFields = xrtHttpHeadersData(
		pCall->CacheRequestFields
	);
	size_t iCount = xrtHttpHeadersCount(
		pCall->CacheRequestFields
	);
	const xhttpfield* pRange;
	const xhttpfield* pIfRange;
	xstrview Unit;
	xstrview Set;
	xstrview ContentType;
	xhttpbyterange* pResolved;
	xhttprangeresult Result;
	xhttpnext RangeNext;
	xhttpnext IfRangeNext;
	size_t iInputCount = 0;
	size_t iResolved = 0;
	uint32 iFlags = xrtHttpCacheRecordFlags(
		pCall->CacheCandidate
	);
	bool bComplete =
		__xrtHttpClientCacheRecordComplete(
			pCall->CacheCandidate
		);
	bool bIfRange;

	*pUsable = bComplete;
	if ( !xrtHttpMethodEqual(
			xrtHttpRequestMethod(pCall->Request),
			XRT_STR_LITERAL("GET")
		) || (xrtHttpCacheRecordStatus(
			pCall->CacheCandidate
		 ) != XHTTP_STATUS_OK) ) {
		return true;
	}

	RangeNext = xrtHttpFieldGetUnique(
		pFields,
		iCount,
		XRT_STR_LITERAL("Range"),
		&pRange
	);
	if ( RangeNext != XHTTP_NEXT_ITEM ) {
		if ( RangeNext == XHTTP_NEXT_ERROR ) {
			xrtClearError();
			pCall->CacheRangeRequest = true;
			*pUsable = false;
		}
		return true;
	}
	pCall->CacheRangeRequest = true;

	if ( !xrtHttpRangeParse(
			pRange->Value,
			&Unit,
			&Set
		) || !xrtHttpTokenEqual(
			Unit,
			XRT_STR_LITERAL("bytes")
		) || !xrtHttpByteRangeCount(
			Set,
			&iInputCount
		) ) {
		xrtClearError();
		*pUsable = false;
		return true;
	}
	if ( iInputCount >
		pCall->Client->Config.Cache.MaxRanges ) {
		*pUsable = false;
		return true;
	}
	if ( (iFlags &
		  XHTTP_CACHE_RECORD_HAS_LENGTH) == 0 ) {
		*pUsable = false;
		return true;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		if ( pCall->DecompressEnabled ) {
			xhttpcacheentry Entry;
			xhttpcontentencodingplan Encoding;

			if ( !xrtHttpCacheRecordEntry(
				pCall->CacheCandidate,
				&Entry
			) || !xrtHttpContentEncodingPlan(
				Entry.Fields,
				Entry.FieldCount,
				&Encoding
			) ) {
				return false;
			}
			if ( (Encoding.DecoderCount != 0) ||
				(Encoding.UnknownCount != 0) ) {
				*pUsable = false;
				return true;
			}
		}
	#endif

	IfRangeNext = xrtHttpFieldGetUnique(
		pFields,
		iCount,
		XRT_STR_LITERAL("If-Range"),
		&pIfRange
	);
	if ( IfRangeNext == XHTTP_NEXT_ERROR ) {
		xrtClearError();
		bIfRange = false;
	} else if ( !__xrtHttpClientCacheIfRangeMatch(
		pCall->CacheCandidate,
		pIfRange,
		&bIfRange
	) ) {
		return false;
	}
	if ( !bIfRange ) {
		pCall->CacheRangeCovered = bComplete;
		*pUsable = bComplete;
		return true;
	}

	pResolved = iInputCount == 1u ?
		&pCall->CacheRange :
		(xhttpbyterange*)xrtMalloc(
			iInputCount * sizeof(*pResolved)
		);
	if ( pResolved == NULL ) {
		return false;
	}
	Result = xrtHttpByteRangesResolve(
		Set,
		xrtHttpCacheRecordLength(
			pCall->CacheCandidate
		),
		pResolved,
		iInputCount,
		0,
		&iResolved,
		&pCall->CacheRangeBodyLength
	);
	if ( Result == XHTTP_RANGE_ERROR ) {
		if ( pResolved != &pCall->CacheRange ) {
			xrtFree(pResolved);
		}
		return false;
	}
	if ( Result == XHTTP_RANGE_EMPTY ) {
		if ( pResolved != &pCall->CacheRange ) {
			xrtFree(pResolved);
		}
		pCall->CacheRangeCovered = bComplete;
		*pUsable = bComplete;
		return true;
	}
	if ( Result == XHTTP_RANGE_UNSATISFIED ) {
		if ( pResolved != &pCall->CacheRange ) {
			xrtFree(pResolved);
		}
		pCall->CacheRangeState =
			__XRT_HTTP_CLIENT_CACHE_RANGE_UNSATISFIABLE;
		pCall->CacheRangeCovered = bComplete;
		*pUsable = bComplete;
		return true;
	}

	if ( iResolved == 1u ) {
		if ( pResolved != &pCall->CacheRange ) {
			pCall->CacheRange = pResolved[0];
			xrtFree(pResolved);
		}
	} else {
		pCall->CacheRanges = pResolved;
	}
	pCall->CacheRangeCount = iResolved;
	pCall->CacheRangeCovered =
		__xrtHttpClientCacheRangesCovered(
			pCall->CacheCandidate,
			__xrtHttpClientCacheRanges(pCall),
			pCall->CacheRangeCount
		);
	pCall->CacheRangeFill =
		(pCall->CacheRangeCount == 1u) &&
		!pCall->CacheRangeCovered;
	if ( pCall->CacheRangeCovered ) {
		if ( pCall->CacheRangeCount == 1u ) {
			pCall->CacheRangeState =
				__XRT_HTTP_CLIENT_CACHE_RANGE_PARTIAL;
		} else {
			ContentType =
				__xrtHttpClientCacheContentType(
					pCall->CacheCandidate
				);
			if ( !__xrtHttpClientCacheBoundary(pCall) ||
				!xrtHttpRangeMultipartLength(
					__xrtHttpClientCacheRanges(pCall),
					pCall->CacheRangeCount,
					xrtHttpCacheRecordLength(
						pCall->CacheCandidate
					),
					ContentType,
					(xstrview){
						pCall->CacheBoundary,
						XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE
					},
					&pCall->CacheRangeBodyLength
				) ) {
				return false;
			}
			pCall->CacheRangeState =
				__XRT_HTTP_CLIENT_CACHE_RANGE_MULTIPART;
		}
	}
	*pUsable = pCall->CacheRangeCovered;
	return true;
}



/* 解析一组字段中的 Cache-Control，非法值保守地禁用本次缓存路径。 */
static bool __xrtHttpClientCacheControl(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcachecontrol* pControl
)
{
	xrtHttpCacheControlInit(pControl);
	return xrtHttpCacheControlParse(
		pFields,
		iCount,
		pControl
	);
}



/* 从唯一有效 Last-Modified 计算受限启发式新鲜寿命。 */
static bool __xrtHttpClientCacheHeuristic(
	xhttpcall* pCall,
	const xhttpcacheentry* pEntry,
	const xhttpcachetime* pTime,
	uint16 iStatus,
	xhttpcachefreshness* pFreshness
)
{
	const xhttpfield* pModified = NULL;
	xtime iModified;
	xtime iReference;
	uint64 iDifference;
	uint64 iLifetime;
	size_t i;

	if ( !pCall->Client->Config.Cache.Heuristic ||
		!xrtHttpCacheStatusHeuristic(iStatus) ) {
		return false;
	}
	for ( i = 0; i < pEntry->FieldCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pEntry->Fields[i].Name,
			XRT_STR_LITERAL("Last-Modified")
		) ) {
			if ( pModified != NULL ) {
				return false;
			}
			pModified = &pEntry->Fields[i];
		}
	}
	if ( (pModified == NULL) ||
		!xrtTimeParseHTTPDate(
			pModified->Value,
			&iModified
		) ) {
		xrtClearError();
		return false;
	}
	iReference =
		((pTime->Flags & XHTTP_CACHE_TIME_DATE) != 0) ?
			pTime->Date :
			xrtHttpCacheRecordResponseTime(
				pCall->CacheCandidate
			);
	if ( iModified >= iReference ) {
		return false;
	}
	iDifference = (uint64)(iReference - iModified);
	iLifetime =
		(iDifference / 100u) *
			pCall->Client->Config.Cache.HeuristicPercent;
	iLifetime +=
		((iDifference % 100u) *
		 pCall->Client->Config.Cache.HeuristicPercent) / 100u;
	if ( iLifetime >
		pCall->Client->Config.Cache.HeuristicMax ) {
		iLifetime =
			pCall->Client->Config.Cache.HeuristicMax;
	}
	pFreshness->Lifetime = iLifetime;
	pFreshness->Source = XHTTP_CACHE_FRESHNESS_HEURISTIC;
	return true;
}



/* 计算一个候选在当前时刻的年龄、寿命和复用决定。 */
static xhttpcacheusedecision __xrtHttpClientCacheUse(
	xhttpcall* pCall,
	const xhttpcachecontrol* pRequest,
	xhttpcacheuseplan* pPlan
)
{
	xhttpcacheentry Entry;
	xhttpcachecontrol Response;
	xhttpcachetime Time;
	xhttpcacheage Age;
	xhttpcachefreshness Freshness;
	xhttpcacheuseinput Input;
	xhttpcachecalc AgeResult;
	xhttpcachecalc FreshnessResult;
	uint16 iStatus = xrtHttpCacheRecordStatus(
		pCall->CacheCandidate
	);

	if ( !xrtHttpCacheRecordEntry(
		pCall->CacheCandidate,
		&Entry
	) || !__xrtHttpClientCacheControl(
		Entry.Fields,
		Entry.FieldCount,
		&Response
	) || !xrtHttpCacheTimeParse(
		Entry.Fields,
		Entry.FieldCount,
		&Time
	) ) {
		return XHTTP_CACHE_USE_ERROR;
	}
	AgeResult = xrtHttpCacheCurrentAge(
		&Time,
		xrtHttpCacheRecordResponseTime(
			pCall->CacheCandidate
		),
		xrtHttpCacheRecordRequestClock(
			pCall->CacheCandidate
		),
		xrtHttpCacheRecordResponseClock(
			pCall->CacheCandidate
		),
		xrtClock(),
		&Age
	);
	FreshnessResult = xrtHttpCacheFreshness(
		&Response,
		&Time,
		xrtHttpCacheRecordResponseTime(
			pCall->CacheCandidate
		),
		pCall->Client->Config.Cache.Shared,
		&Freshness
	);
	if ( (FreshnessResult == XHTTP_CACHE_CALC_NONE) &&
		__xrtHttpClientCacheHeuristic(
			pCall,
			&Entry,
			&Time,
			iStatus,
			&Freshness
		) ) {
		FreshnessResult = XHTTP_CACHE_CALC_READY;
	}
	if ( (AgeResult != XHTTP_CACHE_CALC_READY) ||
		(FreshnessResult != XHTTP_CACHE_CALC_READY) ||
		!xrtHttpCacheUseInputInit(
			&Input,
			iStatus,
			pCall->Client->Config.Cache.Shared
		) ) {
		memset(pPlan, 0, sizeof(*pPlan));
		pPlan->Decision = XHTTP_CACHE_USE_VALIDATE;
		return pPlan->Decision;
	}
	if ( (xrtHttpCacheRecordFlags(
			pCall->CacheCandidate
		 ) & XHTTP_CACHE_RECORD_COMPLETE) == 0 ) {
		if ( !pCall->CacheRangeCovered ) {
			Input.Flags &=
				~(uint32)XHTTP_CACHE_USE_REPRESENTATION;
		}
	}
	if ( pCall->CacheMode == XHTTP_CLIENT_CACHE_ONLY ) {
		Input.Flags |= XHTTP_CACHE_USE_DISCONNECTED;
	}
	if ( xrtHttpRequestHeader(
		pCall->Request,
		XRT_STR_LITERAL("Authorization")
	) != NULL ) {
		Input.Flags |= XHTTP_CACHE_USE_AUTHORIZATION;
	}
	return xrtHttpCacheUsePlan(
		pRequest,
		&Response,
		&Age,
		&Freshness,
		&Input,
		pPlan
	);
}



/* 移除仅由缓存验证路径添加的条件字段，避免跨重定向泄漏。 */
static void __xrtHttpClientCacheValidatorsRemove(
	xhttpcall* pCall
)
{
	if ( pCall->CacheIfNoneMatch ) {
		(void)xrtHttpRequestRemoveHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-None-Match")
		);
		pCall->CacheIfNoneMatch = false;
	}
	if ( pCall->CacheIfModifiedSince ) {
		(void)xrtHttpRequestRemoveHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-Modified-Since")
		);
		pCall->CacheIfModifiedSince = false;
	}
	if ( pCall->CacheIfRange ) {
		(void)xrtHttpRequestRemoveHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-Range")
		);
		pCall->CacheIfRange = false;
	}
}



/* 为单个候选添加最强可用的条件验证字段。 */
static bool __xrtHttpClientCacheValidate(
	xhttpcall* pCall
)
{
	xhttpcacheentry Entry;
	xhttpcachevalidateplan Plan;
	xhttpcachevalidatedecision Decision;
	char sDate[64];
	size_t iDate;
	str sTags = NULL;
	size_t iTags = 0;

	if ( (xrtHttpRequestHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-None-Match")
		 ) != NULL) ||
		(xrtHttpRequestHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-Modified-Since")
		 ) != NULL) ) {
		return true;
	}
	if ( !xrtHttpCacheRecordEntry(
		pCall->CacheCandidate,
		&Entry
	) ) {
		return false;
	}
	if ( pCall->CacheRangeCovered ) {
		Entry.Flags |=
			XHTTP_CACHE_ENTRY_RANGE_COVERED;
	}
	Decision = xrtHttpCacheValidatePlan(
		&Entry,
		1,
		pCall->CacheRangeRequest,
		&Plan
	);
	if ( Decision == XHTTP_CACHE_VALIDATE_ERROR ) {
		return false;
	}
	if ( Decision != XHTTP_CACHE_VALIDATE_CONDITIONAL ) {
		return true;
	}
	if ( (Plan.Actions &
		XHTTP_CACHE_VALIDATE_IF_NONE_MATCH) != 0 ) {
		if ( Plan.ETagSize == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		sTags = (str)xrtMalloc(Plan.ETagSize + 1u);
		if ( sTags == NULL ) {
			return false;
		}
		if ( !xrtHttpCacheValidateETagsWrite(
			&Entry,
			1,
			pCall->CacheRangeRequest,
			sTags,
			Plan.ETagSize,
			&iTags
		) || (iTags != Plan.ETagSize) ) {
			xrtFree(sTags);
			return false;
		}
		sTags[iTags] = '\0';
		if ( !xrtHttpRequestSetHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-None-Match"),
			(xstrview){ sTags, iTags }
		) ) {
			xrtFree(sTags);
			return false;
		}
		xrtFree(sTags);
		pCall->CacheIfNoneMatch = true;
	}
	if ( (Plan.Actions &
		XHTTP_CACHE_VALIDATE_IF_MODIFIED_SINCE) != 0 ) {
		iDate = xrtTimeWriteHTTPDate(
			sDate,
			sizeof(sDate),
			Plan.LastModified
		);
		if ( iDate == XRT_NPOS ) {
			return false;
		}
		if ( !xrtHttpRequestSetHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-Modified-Since"),
			(xstrview){ sDate, iDate }
		) ) {
			return false;
		}
		pCall->CacheIfModifiedSince = true;
	}
	pCall->CacheValidating =
		pCall->CacheIfNoneMatch ||
		pCall->CacheIfModifiedSince;
	return true;
}



/* 为缺失范围回源添加仅属于缓存实现的强 If-Range。 */
static bool __xrtHttpClientCacheIfRange(
	xhttpcall* pCall
)
{
	xhttpcacheentry Entry;
	xhttpcacheifrange Plan;
	xhttpcacheifrangekind Kind;
	char sDate[64];
	str sTag = NULL;
	xstrview Value;
	size_t iSize;

	if ( xrtHttpRequestHeader(
		pCall->Request,
		XRT_STR_LITERAL("If-Range")
	) != NULL ) {
		return true;
	}
	if ( !xrtHttpCacheRecordEntry(
		pCall->CacheCandidate,
		&Entry
	) ) {
		return false;
	}
	Kind = xrtHttpCacheIfRangePlan(&Entry, &Plan);
	if ( Kind == XHTTP_CACHE_IF_RANGE_ERROR ) {
		return false;
	}
	if ( Kind == XHTTP_CACHE_IF_RANGE_NONE ) {
		return true;
	}
	if ( Kind == XHTTP_CACHE_IF_RANGE_ETAG ) {
		sTag = xrtHttpETagBuild(
			&Plan.ETag,
			&iSize
		);
		if ( sTag == NULL ) {
			return false;
		}
		Value = (xstrview){ sTag, iSize };
	} else {
		iSize = xrtTimeWriteHTTPDate(
			sDate,
			sizeof(sDate),
			Plan.Date
		);
		if ( iSize == XRT_NPOS ) {
			return false;
		}
		Value = (xstrview){ sDate, iSize };
	}
	if ( !xrtHttpRequestSetHeader(
		pCall->Request,
		XRT_STR_LITERAL("If-Range"),
		Value
	) ) {
		xrtFree(sTag);
		return false;
	}
	xrtFree(sTag);
	pCall->CacheIfRange = true;
	return true;
}



/* 判断请求控制是否明确禁止任何缓存存取。 */
static bool __xrtHttpClientCacheNoStore(
	const xhttpcachecontrol* pControl
)
{
	return (pControl->Flags & XHTTP_CACHE_NO_STORE) != 0;
}



/* 判断请求控制是否禁止回源。 */
static bool __xrtHttpClientCacheOnly(
	const xhttpcachecontrol* pControl
)
{
	return (pControl->Flags &
		XHTTP_CACHE_ONLY_IF_CACHED) != 0;
}



/* 初始化禁用存储的安全客户端缓存默认值。 */
XRT_API void xrtHttpClientCacheConfigInit(
	xhttpclientcacheconfig* pConfig
)
{
	xhttpclientcacheconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-client-cache-config",
			"HTTP client cache config storage is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.MaxBody = XHTTP_CLIENT_CACHE_BODY_DEFAULT;
	Config.HeuristicMax =
		XHTTP_CLIENT_CACHE_HEURISTIC_MAX_DEFAULT;
	Config.HeuristicPercent =
		XHTTP_CLIENT_CACHE_HEURISTIC_PERCENT_DEFAULT;
	Config.MaxRanges =
		XHTTP_CLIENT_CACHE_MAX_RANGES_DEFAULT;
	Config.Heuristic = true;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化继承 Client 且未分区的调用级缓存选项。 */
XRT_API void xrtHttpClientCacheOptionsInit(
	xhttpclientcacheoptions* pOptions
)
{
	const xhttpclientcacheoptions Options = {
		XHTTP_CLIENT_CACHE_DEFAULT,
		{ NULL, 0 }
	};

	if ( !__xrtRangeValid(pOptions, sizeof(Options)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-client-cache-options",
			"HTTP client cache options storage is invalid",
			NULL
		);
		return;
	}
	memcpy(pOptions, &Options, sizeof(Options));
}



/* 验证配置并让 Client 保留统一 Cache 句柄。 */
bool __xrtHttpClientCacheOpen(xhttpclient* pClient)
{
	const xhttpclientcacheconfig* pConfig =
		&pClient->Config.Cache;

	if ( (pConfig->MaxBody == 0) ||
		(pConfig->MaxRanges == 0) ||
		(pConfig->MaxRanges >
		 (SIZE_MAX / sizeof(xhttpbyterange))) ||
		(pConfig->HeuristicPercent > 100u) ||
		(pConfig->Heuristic &&
		 (pConfig->HeuristicMax == 0)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_CONFIG,
			"configure-http-client-cache",
			"HTTP client cache limits are invalid",
			NULL
		);
		return false;
	}
	if ( pConfig->Store == NULL ) {
		return true;
	}
	pClient->Cache = xrtHttpCacheRetain(pConfig->Store);
	if ( pClient->Cache == NULL ) {
		__xrtHttpClientSetError(
			XERR_STATE,
			XHTTP_CLIENT_ERROR_CONFIG,
			"configure-http-client-cache",
			"HTTP client cache could not be retained",
			xrtGetError()
		);
		return false;
	}
	return true;
}



/* 释放 Client 持有的统一 Cache 句柄。 */
void __xrtHttpClientCacheClose(xhttpclient* pClient)
{
	if ( pClient == NULL ) {
		return;
	}
	xrtHttpCacheRelease(pClient->Cache);
	pClient->Cache = NULL;
}



/* 返回 Client 借用的统一缓存句柄。 */
XRT_API xhttpcache* xrtHttpClientCache(
	const xhttpclient* pClient
)
{
	return pClient != NULL ? pClient->Cache : NULL;
}



/* 冻结调用级缓存模式和分区文本。 */
bool __xrtHttpClientCacheInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
)
{
	xstrview Partition = pOptions->Cache.PartitionKey;

	if ( (pOptions->Cache.Mode <
		  XHTTP_CLIENT_CACHE_DEFAULT) ||
		(pOptions->Cache.Mode >
		  XHTTP_CLIENT_CACHE_ONLY) ||
		((Partition.Data == NULL) &&
		 (Partition.Size != 0)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_CACHE,
			"configure-http-call-cache",
			"HTTP call cache options are invalid",
			NULL
		);
		return false;
	}
	pCall->CachePartitionKey =
		__xrtHttpClientCacheText(Partition);
	if ( pCall->CachePartitionKey == NULL ) {
		return false;
	}
	pCall->CachePartitionSize = Partition.Size;
	pCall->CacheMode = pOptions->Cache.Mode;
	pCall->CacheEnabled =
		(pCall->Client->Cache != NULL) &&
		(pCall->CacheMode !=
		 XHTTP_CLIENT_CACHE_DISABLED);
	if ( !pCall->CacheEnabled ) {
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_BYPASS
		);
	}
	return true;
}



/* 释放 Call 的全部缓存状态。 */
void __xrtHttpClientCacheUnit(xhttpcall* pCall)
{
	if ( pCall == NULL ) {
		return;
	}
	__xrtHttpClientCacheReset(pCall);
	xrtFree(pCall->CachePartitionKey);
	pCall->CachePartitionKey = NULL;
	pCall->CachePartitionSize = 0;
}



/* 冻结有效请求字段并选择命中、验证或回源路径。 */
bool __xrtHttpClientCachePrepare(xhttpcall* pCall)
{
	xhttpcachecontrol Request;
	xhttpcacheuseplan Plan;
	xhttpcacheusedecision Decision;
	xhttpcachelookup Lookup;
	const xhttpheaders* pHeaders;
	bool bHead;
	bool bOnly;
	bool bUsable;

	/* 重试可能发生在响应 Header 到达前，先撤销上一跳自动条件字段。 */
	__xrtHttpClientCacheValidatorsRemove(pCall);
	__xrtHttpClientCacheReset(pCall);
	if ( !pCall->CacheEnabled ) {
		return true;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
		if ( xrtHttpRequestTrailerCount(
			pCall->Request
		) != 0 ) {
			pCall->CacheCapture = false;
			if ( pCall->CacheMode ==
				XHTTP_CLIENT_CACHE_ONLY ) {
				pCall->CacheReady = true;
				__xrtHttpClientCacheOutcome(
					pCall,
					XHTTP_CLIENT_CACHE_ONLY_MISS
				);
			} else {
				__xrtHttpClientCacheOutcome(
					pCall,
					XHTTP_CLIENT_CACHE_BYPASS
				);
			}
			return true;
		}
	#endif
	pHeaders = xrtHttpRequestHeaders(pCall->Request);
	pCall->CacheRequestFields = xrtHttpHeadersClone(pHeaders);
	if ( pCall->CacheRequestFields == NULL ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"snapshot-http-cache-request",
			"HTTP cache request fields could not be copied"
		);
	}
	if ( !__xrtHttpClientCacheControl(
		xrtHttpHeadersData(pCall->CacheRequestFields),
		xrtHttpHeadersCount(pCall->CacheRequestFields),
		&Request
	) ) {
		if ( !__xrtHttpClientCacheRecover(
			pCall,
			"parse-http-cache-request",
			"HTTP request Cache-Control is invalid"
		) ) {
			return false;
		}
		pCall->CacheCapture = false;
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_BYPASS
		);
		return true;
	}
	if ( __xrtHttpClientCacheNoStore(&Request) ) {
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_BYPASS
		);
		return true;
	}
	bOnly =
		(pCall->CacheMode == XHTTP_CLIENT_CACHE_ONLY) ||
		__xrtHttpClientCacheOnly(&Request);
	pCall->CacheCapture =
		pCall->CacheMode != XHTTP_CLIENT_CACHE_ONLY;
	bHead = __xrtHttpClientCacheHeadRequest(pCall);
	Lookup = __xrtHttpClientCacheReload(
		pCall,
		bHead ?
			XRT_STR_LITERAL("GET") :
			xrtHttpRequestMethod(pCall->Request)
	);
	if ( Lookup == XHTTP_CACHE_LOOKUP_ERROR ) {
		if ( !__xrtHttpClientCacheRecover(
			pCall,
			"lookup-http-cache",
			"HTTP cache lookup failed"
		) ) {
			return false;
		}
		Lookup = XHTTP_CACHE_LOOKUP_MISS;
	}
	if ( bHead &&
		(Lookup == XHTTP_CACHE_LOOKUP_MISS) ) {
		Lookup = __xrtHttpClientCacheReload(
			pCall,
			XRT_STR_LITERAL("HEAD")
		);
		if ( Lookup == XHTTP_CACHE_LOOKUP_ERROR ) {
			if ( !__xrtHttpClientCacheRecover(
				pCall,
				"lookup-http-head-cache",
				"HTTP HEAD cache lookup failed"
			) ) {
				return false;
			}
			Lookup = XHTTP_CACHE_LOOKUP_MISS;
		}
	}
	if ( Lookup == XHTTP_CACHE_LOOKUP_MISS ) {
		if ( bOnly ) {
			pCall->CacheReady = true;
			__xrtHttpClientCacheOutcome(
				pCall,
				XHTTP_CLIENT_CACHE_ONLY_MISS
			);
		} else {
			__xrtHttpClientCacheOutcome(
				pCall,
				XHTTP_CLIENT_CACHE_MISS
			);
		}
		return true;
	}
	if ( !__xrtHttpClientCacheRangePrepare(
		pCall,
		&bUsable
	) ) {
		if ( !__xrtHttpClientCacheRecover(
			pCall,
			"plan-http-cache-range",
			"HTTP cache range selection failed"
		) ) {
			return false;
		}
		bUsable = false;
	}
	if ( !bUsable ) {
		if ( bOnly ) {
			xrtHttpCacheRecordRelease(
				pCall->CacheCandidate
			);
			pCall->CacheCandidate = NULL;
			pCall->CacheReady = true;
			__xrtHttpClientCacheOutcome(
				pCall,
				XHTTP_CLIENT_CACHE_ONLY_MISS
			);
			return true;
		}
		if ( pCall->CacheRangeFill &&
			!__xrtHttpClientCacheIfRange(pCall) ) {
			if ( !__xrtHttpClientCacheRecover(
				pCall,
				"prepare-http-cache-if-range",
				"HTTP cache If-Range could not be prepared"
			) ) {
				return false;
			}
		}
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_MISS
		);
		return true;
	}
	Decision = __xrtHttpClientCacheUse(
		pCall,
		&Request,
		&Plan
	);
	if ( Decision == XHTTP_CACHE_USE_ERROR ) {
		if ( !__xrtHttpClientCacheRecover(
			pCall,
			"plan-http-cache-use",
			"HTTP cache reuse plan failed"
		) ) {
			return false;
		}
		Decision = XHTTP_CACHE_USE_FORWARD;
	}
	if ( (Decision == XHTTP_CACHE_USE_STORED) &&
		(pCall->CacheMode !=
		 XHTTP_CLIENT_CACHE_RELOAD) ) {
		pCall->CacheReady = true;
		__xrtHttpClientCacheOutcome(
			pCall,
			((Plan.Actions &
			  XHTTP_CACHE_USE_STALE) != 0) ?
				XHTTP_CLIENT_CACHE_STALE :
				XHTTP_CLIENT_CACHE_HIT
		);
		return true;
	}
	if ( (Decision ==
		  XHTTP_CACHE_USE_GATEWAY_TIMEOUT) ||
		(bOnly &&
		 (Decision != XHTTP_CACHE_USE_STORED)) ) {
		xrtHttpCacheRecordRelease(
			pCall->CacheCandidate
		);
		pCall->CacheCandidate = NULL;
		pCall->CacheReady = true;
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_ONLY_MISS
		);
		return true;
	}
	if ( (Decision == XHTTP_CACHE_USE_VALIDATE) ||
		(pCall->CacheMode ==
		 XHTTP_CLIENT_CACHE_RELOAD) ) {
		if ( !__xrtHttpClientCacheValidate(pCall) ) {
			/* SetHeader 可能已提交第一个验证器，宽松回源前必须回滚。 */
			__xrtHttpClientCacheValidatorsRemove(pCall);
			pCall->CacheValidating = false;
			return __xrtHttpClientCacheRecover(
				pCall,
				"prepare-http-cache-validation",
				"HTTP cache validators could not be prepared"
			);
		}
	}
	return true;
}



/* 为未知长度正文增长自适应暂存，不使用固定对象缓冲。 */
static bool __xrtHttpClientCacheBodyAppend(
	xhttpcall* pCall,
	xbytesview Data
)
{
	size_t iRequired;
	size_t iCapacity;
	bytes pBody;

	if ( Data.Size == 0 ) {
		return true;
	}
	if ( (pCall->CacheBodySize >
		  (SIZE_MAX - Data.Size)) ||
		((uint64)Data.Size >
		 (pCall->Client->Config.Cache.MaxBody -
		  (uint64)pCall->CacheBodySize)) ) {
		__xrtErrorSetRange();
		return false;
	}
	iRequired = pCall->CacheBodySize + Data.Size;
	iCapacity = pCall->CacheBodyCapacity;
	if ( iRequired > iCapacity ) {
		if ( iCapacity == 0 ) {
			iCapacity =
				iRequired >
				XRT_HTTP_CLIENT_CACHE_BODY_MIN_CAPACITY ?
					iRequired :
					XRT_HTTP_CLIENT_CACHE_BODY_MIN_CAPACITY;
		}
		while ( iCapacity < iRequired ) {
			size_t iNext = iCapacity +
				(iCapacity >> 1u);

			if ( iNext <= iCapacity ) {
				iCapacity = iRequired;
				break;
			}
			iCapacity = iNext;
		}
		if ( (uint64)iCapacity >
			pCall->Client->Config.Cache.MaxBody ) {
			iCapacity = (size_t)
				pCall->Client->Config.Cache.MaxBody;
		}
		pBody = (bytes)xrtRealloc(
			pCall->CacheBody,
			iCapacity
		);
		if ( pBody == NULL ) {
			return false;
		}
		pCall->CacheBody = pBody;
		pCall->CacheBodyCapacity = iCapacity;
	}
	memcpy(
		pCall->CacheBody + pCall->CacheBodySize,
		Data.Data,
		Data.Size
	);
	pCall->CacheBodySize = iRequired;
	return true;
}



/* 缓存观察器透传信息响应，不把它们保存为表示。 */
static bool __xrtHttpClientCacheInformational(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->CacheNext.Informational == NULL ) {
		return true;
	}
	return pCall->CacheNext.Informational(
		pResponse,
		pCall->CacheNext.Data
	);
}



/* 在下游改写前快照原始响应 Header，并吞掉自有验证产生的 304。 */
static bool __xrtHttpClientCacheHeaders(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	const xhttpheaders* pHeaders =
		xrtHttpResponseHeaders(pResponse);

	pCall->CacheResponseClock = xrtClock();
	pCall->CacheResponseTime = xrtNow();
	if ( pCall->CacheValidating &&
		(xrtHttpResponseStatus(pResponse) ==
		 XHTTP_STATUS_NOT_MODIFIED) ) {
		pCall->CacheResponseFields =
			xrtHttpHeadersClone(pHeaders);
		if ( pCall->CacheResponseFields == NULL ) {
			return __xrtHttpClientCacheError(
				pCall,
				XERR_MEMORY,
				"copy-http-cache-validation",
				"HTTP 304 fields could not be copied",
				xrtGetError()
			);
		}
		pCall->CacheNotModified = true;
		pCall->CacheCapture = false;
		__xrtHttpClientCacheValidatorsRemove(pCall);
		return true;
	}
	__xrtHttpClientCacheValidatorsRemove(pCall);
	if ( pCall->CacheCapture ) {
		pCall->CacheResponseFields =
			xrtHttpHeadersClone(pHeaders);
		if ( pCall->CacheResponseFields == NULL ) {
			if ( !__xrtHttpClientCacheRecover(
				pCall,
				"copy-http-cache-response",
				"HTTP cache response fields could not be copied"
			) ) {
				return false;
			}
			pCall->CacheCapture = false;
		}
	}
	if ( pCall->CacheNext.Headers == NULL ) {
		return true;
	}
	return pCall->CacheNext.Headers(
		pResponse,
		pCall->CacheNext.Data
	);
}



/* 暂存原始编码正文后按原顺序转发给重定向和解压链。 */
static bool __xrtHttpClientCacheBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->CacheCapture &&
		!__xrtHttpClientCacheBodyAppend(
			pCall,
			Data
		) ) {
		if ( !__xrtHttpClientCacheRecover(
			pCall,
			"buffer-http-cache-response",
			"HTTP cache response body exceeded its limit or allocation failed"
		) ) {
			return false;
		}
		pCall->CacheCapture = false;
		xrtHttpHeadersDestroy(
			pCall->CacheResponseFields
		);
		pCall->CacheResponseFields = NULL;
		xrtFree(pCall->CacheBody);
		pCall->CacheBody = NULL;
		pCall->CacheBodySize = 0;
		pCall->CacheBodyCapacity = 0;
	}
	if ( pCall->CacheNext.Body == NULL ) {
		return true;
	}
	return pCall->CacheNext.Body(
		pResponse,
		Data,
		pCall->CacheNext.Data
	);
}



/* 构造保存原始表示的逐跳事件观察器。 */
const xhttp1exchangeevents* __xrtHttpClientCacheEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
)
{
	if ( !pCall->CacheEnabled ) {
		return pNext;
	}
	pCall->CacheNext = *pNext;
	memset(
		&pCall->CacheEvents,
		0,
		sizeof(pCall->CacheEvents)
	);
	pCall->CacheEvents.Informational =
		__xrtHttpClientCacheInformational;
	pCall->CacheEvents.Headers =
		__xrtHttpClientCacheHeaders;
	pCall->CacheEvents.Body =
		__xrtHttpClientCacheBody;
	pCall->CacheEvents.Data = pCall;
	return &pCall->CacheEvents;
}



/* 构造只包含 StorePlan 允许字段的借用数组。 */
static xhttpfield* __xrtHttpClientCacheStoredFields(
	const xhttpheaders* pHeaders,
	bool Shared,
	uint32 iActions,
	size_t* pCount
)
{
	const xhttpfield* pFields = xrtHttpHeadersData(pHeaders);
	size_t iCount = xrtHttpHeadersCount(pHeaders);
	xhttpfield* pStored;
	size_t iStored = 0;
	size_t i;

	*pCount = 0;
	if ( iCount == 0 ) {
		return NULL;
	}
	if ( iCount > (SIZE_MAX / sizeof(*pStored)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pStored = (xhttpfield*)xrtMalloc(
		iCount * sizeof(*pStored)
	);
	if ( pStored == NULL ) {
		return NULL;
	}
	for ( i = 0; i < iCount; i++ ) {
		xhttpcachefieldstore Result =
			xrtHttpCacheFieldStore(
				pFields,
				iCount,
				i,
				Shared,
				iActions
			);

		if ( Result == XHTTP_CACHE_FIELD_STORE_ERROR ) {
			xrtFree(pStored);
			return NULL;
		}
		if ( Result == XHTTP_CACHE_FIELD_STORE_KEEP ) {
			pStored[iStored] = pFields[i];
			iStored++;
		}
	}
	*pCount = iStored;
	return pStored;
}



/* 把借用字段数组复制到可规范化的 Header 容器。 */
static xhttpheaders* __xrtHttpClientCacheHeadersCreate(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	size_t i;

	if ( pHeaders == NULL ) {
		return NULL;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !xrtHttpHeadersAdd(
			pHeaders,
			pFields[i].Name,
			pFields[i].Value
		) ) {
			xrtHttpHeadersDestroy(pHeaders);
			return NULL;
		}
	}
	return pHeaders;
}



/* 用十进制完整长度设置规范化 Content-Length。 */
static bool __xrtHttpClientCacheContentLength(
	xhttpheaders* pHeaders,
	uint64 iLength
)
{
	char sLength[32];
	int iSize = snprintf(
		sLength,
		sizeof(sLength),
		"%llu",
		(unsigned long long)iLength
	);

	if ( (iSize < 0) ||
		((size_t)iSize >= sizeof(sLength)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	return xrtHttpHeadersSet(
		pHeaders,
		XRT_STR_LITERAL("Content-Length"),
		(xstrview){ sLength, (size_t)iSize }
	);
}



/* 按已经解码的计划事实规范化表示 Header。 */
static bool __xrtHttpClientCacheNormalizeHeaders(
	xhttpheaders* pHeaders,
	bool bRemoveRange,
	bool bRemoveLength,
	bool bSetLength,
	uint64 iLength
)
{
	if ( bRemoveRange ) {
		(void)xrtHttpHeadersRemove(
			pHeaders,
			XRT_STR_LITERAL("Content-Range")
		);
	}
	if ( bRemoveLength ) {
		(void)xrtHttpHeadersRemove(
			pHeaders,
			XRT_STR_LITERAL("Content-Length")
		);
	}
	if ( bSetLength ) {
		return __xrtHttpClientCacheContentLength(
			pHeaders,
			iLength
		);
	}
	return true;
}



/* 把外层 multipart framing Header 还原为被分片表示的 Header。 */
static bool __xrtHttpClientCacheMultipartHeaders(
	xhttpheaders* pHeaders,
	const __xrt_http_client_cache_multipart* pPlan
)
{
	(void)xrtHttpHeadersRemove(
		pHeaders,
		XRT_STR_LITERAL("Content-Range")
	);
	(void)xrtHttpHeadersRemove(
		pHeaders,
		XRT_STR_LITERAL("Content-Length")
	);
	(void)xrtHttpHeadersRemove(
		pHeaders,
		XRT_STR_LITERAL("Content-Type")
	);
	if ( ((pPlan->Flags &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_CONTENT_TYPE) != 0) &&
		!xrtHttpHeadersSet(
			pHeaders,
			XRT_STR_LITERAL("Content-Type"),
			pPlan->ContentType
		) ) {
		return false;
	}
	if ( (pPlan->Flags &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_COMPLETE) != 0 ) {
		return __xrtHttpClientCacheContentLength(
			pHeaders,
			pPlan->Length
		);
	}
	return true;
}



/* 删除 unsafe 成功响应影响的目标与同源位置条目。 */
static bool __xrtHttpClientCacheInvalidate(
	xhttpcall* pCall,
	const xhttpresponse* pResponse
)
{
	const xhttpheaders* pHeaders =
		xrtHttpResponseHeaders(pResponse);
	const xhttpfield* pFields =
		xrtHttpHeadersData(pHeaders);
	size_t iFieldCount =
		xrtHttpHeadersCount(pHeaders);
	xhttpcacheinvalidatecursor Cursor;
	xhttpcacheinvalidateitem Item;
	xhttpnext Next;
	xstrview Target =
		__xrtHttpClientCacheURI(pCall->Request);
	xstrview Partition = {
		pCall->CachePartitionKey,
		pCall->CachePartitionSize
	};

	xrtHttpCacheInvalidationCursorInit(&Cursor);
	for ( ;; ) {
		char sLocal[256];
		str sURI = sLocal;
		size_t iSize = 0;

		Next = xrtHttpCacheInvalidationNext(
			xrtHttpRequestMethod(pCall->Request),
			xrtHttpResponseStatus(pResponse),
			Target,
			pFields,
			iFieldCount,
			&Cursor,
			&Item
		);
		if ( Next == XHTTP_NEXT_END ) {
			return true;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"plan-http-cache-invalidation",
				"HTTP cache invalidation candidates are invalid"
			);
		}
		if ( !xrtHttpCacheInvalidationWrite(
			Target,
			&Item,
			NULL,
			0,
			&iSize
		) ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"resolve-http-cache-invalidation",
				"HTTP cache invalidation URI could not be resolved"
			);
		}
		if ( iSize > sizeof(sLocal) ) {
			sURI = (str)xrtMalloc(iSize);
			if ( sURI == NULL ) {
				return __xrtHttpClientCacheRecover(
					pCall,
					"allocate-http-cache-invalidation",
					"HTTP cache invalidation URI could not be allocated"
				);
			}
		}
		if ( !xrtHttpCacheInvalidationWrite(
			Target,
			&Item,
			sURI,
			iSize,
			&iSize
		) ) {
			if ( sURI != sLocal ) {
				xrtFree(sURI);
			}
			return __xrtHttpClientCacheRecover(
				pCall,
				"write-http-cache-invalidation",
				"HTTP cache invalidation URI could not be written"
			);
		}
		xrtClearError();
		if ( !xrtHttpCacheRemoveURI(
			pCall->Client->Cache,
			(xstrview){ sURI, iSize },
			Partition,
			NULL
		) ) {
			if ( sURI != sLocal ) {
				xrtFree(sURI);
			}
			return __xrtHttpClientCacheRecover(
				pCall,
				"remove-http-cache-invalidation",
				"HTTP cache backend could not invalidate the response URI"
			);
		}
		if ( sURI != sLocal ) {
			xrtFree(sURI);
		}
		if ( xrtGetError() != NULL ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"remove-http-cache-invalidation",
				"HTTP cache backend could not invalidate a URI"
			);
		}
	}
}



/* 创建保存字段的可修改副本。 */
static xhttpheaders* __xrtHttpClientCacheRecordHeaders(
	const xhttpcacherecord* pRecord
);



/* 把记录的每个正文片段投影为范围组合协议视图。 */
static xhttpcachefragment* __xrtHttpClientCacheFragments(
	const xhttpcacherecord* pRecord,
	size_t* pCount
)
{
	xhttpcacheentry Entry;
	xhttpcachefragment* pFragments;
	size_t iPartCount =
		xrtHttpCacheRecordPartCount(pRecord);
	size_t iCount = 0;
	uint32 iRecordFlags = xrtHttpCacheRecordFlags(pRecord);
	size_t i;

	*pCount = 0;
	if ( iPartCount == 0 ) {
		return NULL;
	}
	if ( iPartCount >
		(SIZE_MAX / sizeof(*pFragments)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	if ( !xrtHttpCacheRecordEntry(pRecord, &Entry) ) {
		return NULL;
	}
	pFragments = (xhttpcachefragment*)xrtMalloc(
		iPartCount * sizeof(*pFragments)
	);
	if ( pFragments == NULL ) {
		return NULL;
	}
	for ( i = 0; i < iPartCount; i++ ) {
		const xhttpcachepart* pPart =
			xrtHttpCacheRecordPartAt(pRecord, i);
		xhttpcachefragment* pFragment;
		uint64 iLast;

		if ( (pPart == NULL) ||
			(pPart->Data.Size == 0) ||
			(((uint64)pPart->Data.Size - UINT64_C(1)) >
			 (UINT64_MAX - pPart->Offset)) ) {
			xrtFree(pFragments);
			__xrtErrorSetInternal();
			return NULL;
		}
		iLast = pPart->Offset +
			(uint64)pPart->Data.Size -
			UINT64_C(1);
		if ( (iCount != 0) &&
			(pFragments[iCount - 1u].Range.Last !=
			 UINT64_MAX) &&
			(pPart->Offset ==
			 (pFragments[iCount - 1u].Range.Last +
			  UINT64_C(1))) ) {
			pFragments[iCount - 1u].Range.Last =
				iLast;
			continue;
		}
		pFragment = &pFragments[iCount++];
		memset(pFragment, 0, sizeof(*pFragment));
		pFragment->Entry = Entry;
		pFragment->Entry.Flags &=
			~(uint32)XHTTP_CACHE_ENTRY_RANGE_COVERED;
		pFragment->Range.First = pPart->Offset;
		pFragment->Range.Last = iLast;
		pFragment->Flags =
			XHTTP_CACHE_FRAGMENT_HAS_RANGE;
		if ( (iRecordFlags &
			  XHTTP_CACHE_RECORD_HAS_LENGTH) != 0 ) {
			pFragment->Flags |=
				XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
			pFragment->Length =
				xrtHttpCacheRecordLength(pRecord);
		}
	}
	for ( i = 0; i < iCount; i++ ) {
		bool bComplete =
			(iCount == 1) &&
			((iRecordFlags &
			  XHTTP_CACHE_RECORD_COMPLETE) != 0);

		if ( bComplete ) {
			pFragments[i].Entry.Flags &=
				~(uint32)XHTTP_CACHE_ENTRY_PARTIAL;
			pFragments[i].SourceStatus =
				XHTTP_STATUS_OK;
		} else {
			pFragments[i].Entry.Flags |=
				XHTTP_CACHE_ENTRY_PARTIAL;
			pFragments[i].SourceStatus =
				XHTTP_STATUS_PARTIAL_CONTENT;
		}
	}
	*pCount = iCount;
	return pFragments;
}



/* 按字段名用新记录字段覆盖已有 Header，保持同名字段的线路顺序。 */
static bool __xrtHttpClientCacheHeadersOverlay(
	xhttpheaders* pHeaders,
	const xhttpcacherecord* pIncoming,
	bool bShared
)
{
	xhttpcacheentry Entry;
	const xhttpfield* pFields;
	size_t iCount;
	size_t i;

	if ( !xrtHttpCacheRecordEntry(
		pIncoming,
		&Entry
	) ) {
		return false;
	}
	pFields = Entry.Fields;
	iCount = Entry.FieldCount;
	for ( i = 0; i < iCount; i++ ) {
		const xhttpfield* pField = &pFields[i];
		xhttpcachefieldupdate Update;
		size_t j;

		for ( j = 0; j < i; j++ ) {
			if ( xrtHttpFieldNameEqual(
					pFields[j].Name,
					pField->Name
				) ) {
				break;
			}
		}
		if ( j != i ) {
			continue;
		}
		Update = xrtHttpCacheFieldUpdate(
			pFields,
			iCount,
			i,
			bShared,
			XHTTP_CACHE_UPDATE_FIELD_NONE
		);
		if ( Update == XHTTP_CACHE_FIELD_UPDATE_ERROR ) {
			return false;
		}
		if ( Update == XHTTP_CACHE_FIELD_UPDATE_SKIP ) {
			continue;
		}
		(void)xrtHttpHeadersRemove(
			pHeaders,
			pField->Name
		);
		for ( j = i; j < iCount; j++ ) {
			const xhttpfield* pValue = &pFields[j];

			if ( xrtHttpFieldNameEqual(
					pValue->Name,
					pField->Name
				) && !xrtHttpHeadersAdd(
					pHeaders,
					pValue->Name,
					pValue->Value
				) ) {
				return false;
			}
		}
	}
	return true;
}



/* 在规范覆盖数组上原地应用一个片段组合计划。 */
static bool __xrtHttpClientCacheCombineApply(
	xhttpcachefragment* pFragments,
	size_t* pCount,
	size_t iCapacity,
	const xhttpcachefragment* pIncoming,
	const xhttpcachecombineplan* pPlan
)
{
	xhttpcachefragment Fragment = *pIncoming;
	size_t iTail;
	size_t iTailCount;
	size_t iResult;

	if ( (pPlan->Index > *pCount) ||
		(pPlan->RemoveCount >
		 (*pCount - pPlan->Index)) ) {
		__xrtErrorSetInternal();
		return false;
	}
	iResult =
		*pCount - pPlan->RemoveCount + 1u;
	if ( (iResult != pPlan->ResultCount) ||
		(iResult > iCapacity) ) {
		__xrtErrorSetInternal();
		return false;
	}
	iTail = pPlan->Index +
		pPlan->RemoveCount;
	iTailCount = *pCount - iTail;
	memmove(
		&pFragments[pPlan->Index + 1u],
		&pFragments[iTail],
		iTailCount * sizeof(*pFragments)
	);

	Fragment.Range = pPlan->Range;
	Fragment.Length = pPlan->Length;
	Fragment.Flags =
		XHTTP_CACHE_FRAGMENT_HAS_RANGE;
	if ( pPlan->HasLength ) {
		Fragment.Flags |=
			XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
	}
	Fragment.Entry.Flags &=
		~(uint32)XHTTP_CACHE_ENTRY_RANGE_COVERED;
	if ( pPlan->Complete ) {
		Fragment.Entry.Flags &=
			~(uint32)XHTTP_CACHE_ENTRY_PARTIAL;
	} else {
		Fragment.Entry.Flags |=
			XHTTP_CACHE_ENTRY_PARTIAL;
	}
	pFragments[pPlan->Index] = Fragment;
	*pCount = iResult;
	return true;
}



/* 为最终覆盖区间计算一次性正文布局。 */
static bool __xrtHttpClientCacheCombineLayout(
	const xhttpcachefragment* pFragments,
	size_t iCount,
	xhttpcachepart* pParts,
	uint64 iMaxBody,
	size_t* pBodySize
)
{
	size_t iTotal = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		uint64 iSpan64 =
			pFragments[i].Range.Last -
			pFragments[i].Range.First;
		size_t iSpan;

		if ( iSpan64 == UINT64_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iSpan64++;
		if ( (uint64)(size_t)iSpan64 != iSpan64 ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iSpan = (size_t)iSpan64;
		if ( ((uint64)iTotal > iMaxBody) ||
			(iSpan64 >
			 (iMaxBody - (uint64)iTotal)) ) {
			__xrtErrorSetRange();
			return false;
		}
		pParts[i].Offset =
			pFragments[i].Range.First;
		pParts[i].Data.Data = NULL;
		pParts[i].Data.Size = iSpan;
		iTotal += iSpan;
	}
	*pBodySize = iTotal;
	return true;
}



/* 把一个不可变记录的相交字节复制到最终覆盖布局。 */
static bool __xrtHttpClientCacheCombineCopy(
	const xhttpcacherecord* pRecord,
	xhttpcachepart* pParts,
	size_t iPartCount
)
{
	size_t iSourceCount =
		xrtHttpCacheRecordPartCount(pRecord);
	size_t iSource = 0;
	size_t iTarget = 0;

	while ( (iSource < iSourceCount) &&
		(iTarget < iPartCount) ) {
		const xhttpcachepart* pSource =
			xrtHttpCacheRecordPartAt(
				pRecord,
				iSource
			);
		xhttpcachepart* pTarget =
			&pParts[iTarget];
		uint64 iSourceLast;
		uint64 iTargetLast;
		uint64 iFirst;
		uint64 iLast;
		size_t iSize;

		if ( (pSource == NULL) ||
			(pSource->Data.Size == 0) ||
			(pTarget->Data.Size == 0) ||
			(((uint64)pSource->Data.Size -
			  UINT64_C(1)) >
			 (UINT64_MAX - pSource->Offset)) ||
			(((uint64)pTarget->Data.Size -
			  UINT64_C(1)) >
			 (UINT64_MAX - pTarget->Offset)) ) {
			__xrtErrorSetInternal();
			return false;
		}
		iSourceLast =
			pSource->Offset +
			(uint64)pSource->Data.Size -
			UINT64_C(1);
		iTargetLast =
			pTarget->Offset +
			(uint64)pTarget->Data.Size -
			UINT64_C(1);
		if ( iSourceLast < pTarget->Offset ) {
			iSource++;
			continue;
		}
		if ( iTargetLast < pSource->Offset ) {
			iTarget++;
			continue;
		}
		iFirst = pSource->Offset >
			pTarget->Offset ?
				pSource->Offset :
				pTarget->Offset;
		iLast = iSourceLast < iTargetLast ?
			iSourceLast :
			iTargetLast;
		iSize = (size_t)(
			(iLast - iFirst) +
			UINT64_C(1)
		);
		memcpy(
			(bytes)pTarget->Data.Data +
				(size_t)(iFirst -
				 pTarget->Offset),
			pSource->Data.Data +
				(size_t)(iFirst -
				 pSource->Offset),
			iSize
		);
		if ( iSourceLast <= iTargetLast ) {
			iSource++;
		}
		if ( iTargetLast <= iSourceLast ) {
			iTarget++;
		}
	}
	return true;
}



/* 依据全部已应用计划一次创建最终组合记录。 */
static xhttpcacherecord* __xrtHttpClientCacheCombineRecord(
	xhttpcall* pCall,
	const xhttpcacherecord* pIncoming,
	const xhttpcachefragment* pFragments,
	size_t iCount,
	uint64 iLength,
	bool bHasLength,
	bool bComplete
)
{
	const xhttpcacherecord* pStored =
		pCall->CacheCandidate;
	xhttpcachekey Key;
	xhttpcacherecordinput Input;
	xhttpcachepart* pParts = NULL;
	xhttpheaders* pHeaders = NULL;
	xhttpcacherecord* pRecord = NULL;
	bytes pBody = NULL;
	size_t iBodySize = 0;
	size_t iOffset = 0;
	size_t i;

	if ( iCount >
		(SIZE_MAX / sizeof(*pParts)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pParts = (xhttpcachepart*)xrtMalloc(
		iCount * sizeof(*pParts)
	);
	if ( pParts == NULL ) {
		return NULL;
	}
	if ( !__xrtHttpClientCacheCombineLayout(
		pFragments,
		iCount,
		pParts,
		pCall->Client->Config.Cache.MaxBody,
		&iBodySize
	) ) {
		goto cleanup;
	}
	pBody = (bytes)xrtMalloc(iBodySize);
	if ( pBody == NULL ) {
		goto cleanup;
	}
	for ( i = 0; i < iCount; i++ ) {
		pParts[i].Data.Data =
			pBody + iOffset;
		iOffset += pParts[i].Data.Size;
	}
	if ( (iOffset != iBodySize) ||
		!__xrtHttpClientCacheCombineCopy(
			pStored,
			pParts,
			iCount
		) || !__xrtHttpClientCacheCombineCopy(
			pIncoming,
			pParts,
			iCount
		) ) {
		goto cleanup;
	}

	pHeaders = __xrtHttpClientCacheRecordHeaders(
		pStored
	);
	if ( (pHeaders == NULL) ||
		!__xrtHttpClientCacheHeadersOverlay(
			pHeaders,
			pIncoming,
			pCall->Client->Config.Cache.Shared
		) || !__xrtHttpClientCacheNormalizeHeaders(
			pHeaders,
			true,
			true,
			bComplete,
			iLength
		) || !__xrtHttpClientCacheKey(
			pCall,
			&Key
		) || !xrtHttpCacheRecordInputInit(
			&Input,
			&Key,
			XHTTP_STATUS_OK
		) ) {
		goto cleanup;
	}
	Input.Version =
		xrtHttpCacheRecordVersion(pIncoming);
	Input.Reason = XRT_STR_LITERAL("OK");
	Input.Fields = xrtHttpHeadersData(pHeaders);
	Input.FieldCount =
		xrtHttpHeadersCount(pHeaders);
	Input.Parts = pParts;
	Input.PartCount = iCount;
	Input.Length = iLength;
	if ( bHasLength ) {
		Input.Flags |=
			XHTTP_CACHE_RECORD_HAS_LENGTH;
	}
	if ( bComplete ) {
		Input.Flags |=
			XHTTP_CACHE_RECORD_COMPLETE;
	}
	Input.ResponseTime =
		pCall->CacheResponseTime;
	Input.RequestClock =
		pCall->CacheRequestClock;
	Input.ResponseClock =
		pCall->CacheResponseClock;
	pRecord = xrtHttpCacheRecordCreate(&Input);

cleanup:
	xrtHttpHeadersDestroy(pHeaders);
	xrtFree(pBody);
	xrtFree(pParts);
	return pRecord;
}



/* 尝试把一个或多个新 206 片段原子并入同一 Vary 变体。 */
static xhttpcacherecord* __xrtHttpClientCacheCombine(
	xhttpcall* pCall,
	xhttpcacherecord* pIncoming
)
{
	xhttpcachefragment* pStored = NULL;
	xhttpcachefragment* pNew = NULL;
	xhttpcachefragment* pWorking = NULL;
	xhttpcachecombineplan Plan;
	xhttpcachecombinedecision Decision;
	xhttpcacherecord* pRecord = NULL;
	size_t iStoredCount = 0;
	size_t iNewCount = 0;
	size_t iCapacity;
	size_t iCount;
	size_t i;

	if ( (pCall->CacheCandidate == NULL) ||
		(xrtHttpCacheRecordStatus(
			pCall->CacheCandidate
		 ) != XHTTP_STATUS_OK) ||
		((xrtHttpCacheRecordFlags(pIncoming) &
		  XHTTP_CACHE_RECORD_COMPLETE) != 0) ) {
		return xrtHttpCacheRecordRetain(pIncoming);
	}
	if ( xrtHttpCacheRecordPartCount(
		pCall->CacheCandidate
	) == 0 ) {
		return xrtHttpCacheRecordRetain(pIncoming);
	}
	pStored = __xrtHttpClientCacheFragments(
		pCall->CacheCandidate,
		&iStoredCount
	);
	pNew = __xrtHttpClientCacheFragments(
		pIncoming,
		&iNewCount
	);
	if ( (pStored == NULL) || (pNew == NULL) ||
		(iStoredCount == 0) || (iNewCount == 0) ) {
		goto cleanup;
	}
	if ( iNewCount >
		(SIZE_MAX - iStoredCount) ) {
		__xrtErrorSetSizeOverflow();
		goto cleanup;
	}
	iCapacity = iStoredCount + iNewCount;
	if ( iCapacity >
		(SIZE_MAX / sizeof(*pWorking)) ) {
		__xrtErrorSetSizeOverflow();
		goto cleanup;
	}
	pWorking = (xhttpcachefragment*)xrtMalloc(
		iCapacity * sizeof(*pWorking)
	);
	if ( pWorking == NULL ) {
		goto cleanup;
	}
	memcpy(
		pWorking,
		pStored,
		iStoredCount * sizeof(*pWorking)
	);
	iCount = iStoredCount;

	for ( i = 0; i < iNewCount; i++ ) {
		Decision = xrtHttpCacheCombinePlan(
			pWorking,
			iCount,
			&pNew[i],
			&Plan
		);
		if ( Decision == XHTTP_CACHE_COMBINE_ERROR ) {
			goto cleanup;
		}
		if ( Decision != XHTTP_CACHE_COMBINE_APPLY ) {
			pRecord =
				xrtHttpCacheRecordRetain(pIncoming);
			goto cleanup;
		}
		if ( !__xrtHttpClientCacheCombineApply(
			pWorking,
			&iCount,
			iCapacity,
			&pNew[i],
			&Plan
		) ) {
			goto cleanup;
		}
	}
	if ( iCount >
		pCall->Client->Config.Cache.MaxRanges ) {
		pRecord =
			xrtHttpCacheRecordRetain(pIncoming);
		goto cleanup;
	}
	pRecord = __xrtHttpClientCacheCombineRecord(
		pCall,
		pIncoming,
		pWorking,
		iCount,
		Plan.Length,
		Plan.HasLength,
		Plan.Complete
	);

cleanup:
	xrtFree(pWorking);
	xrtFree(pNew);
	xrtFree(pStored);
	return pRecord;
}



/*
	以条件插入或条件替换提交不完整表示。
	并发冲突后重新读取当前记录并重新执行片段规划，避免丢失覆盖。
*/
static bool __xrtHttpClientCacheFragmentCommit(
	xhttpcall* pCall,
	xhttpcacherecord* pIncoming
)
{
	xstrview Method = xrtHttpRequestMethod(pCall->Request);
	xhttpcacherecord* pCombined;
	xhttpcachelookup Lookup;
	xhttpcacheput Put;
	size_t iAttempt;

	for ( iAttempt = 0;
		iAttempt < XRT_HTTP_CLIENT_CACHE_COMMIT_RETRIES;
		iAttempt++ ) {
		if ( pCall->CacheCandidate == NULL ) {
			Put = xrtHttpCacheInsert(
				pCall->Client->Cache,
				pIncoming
			);
		} else {
			pCombined = __xrtHttpClientCacheCombine(
				pCall,
				pIncoming
			);
			if ( pCombined == NULL ) {
				return __xrtHttpClientCacheRecover(
					pCall,
					"combine-http-cache-fragment",
					"HTTP cache response fragment could not be combined"
				);
			}
			Put = xrtHttpCacheReplace(
				pCall->Client->Cache,
				pCall->CacheCandidate,
				pCombined
			);
			xrtHttpCacheRecordRelease(pCombined);
		}
		if ( Put == XHTTP_CACHE_PUT_ERROR ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"store-http-cache-fragment",
				"HTTP cache backend could not commit the response fragment"
			);
		}
		if ( Put == XHTTP_CACHE_PUT_REJECTED ) {
			return true;
		}
		if ( Put != XHTTP_CACHE_PUT_CONFLICT ) {
			__xrtHttpClientCacheOutcome(
				pCall,
				XHTTP_CLIENT_CACHE_UPDATED
			);
			return true;
		}
		Lookup = __xrtHttpClientCacheReload(
			pCall,
			Method
		);
		if ( Lookup == XHTTP_CACHE_LOOKUP_ERROR ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"reload-http-cache-fragment",
				"HTTP cache fragment conflict could not be reloaded"
			);
		}
	}

	__xrtErrorSetAgain();
	return __xrtHttpClientCacheRecover(
		pCall,
		"retry-http-cache-fragment",
		"HTTP cache fragment commit remained contended"
	);
}



/* 把成功网络响应规范化为完整或部分不可变记录。 */
static bool __xrtHttpClientCacheStore(
	xhttpcall* pCall,
	const xhttpresponse* pResponse
)
{
	const xhttpfield* pRequestFields =
		xrtHttpHeadersData(pCall->CacheRequestFields);
	size_t iRequestCount =
		xrtHttpHeadersCount(pCall->CacheRequestFields);
	const xhttpfield* pResponseFields =
		xrtHttpHeadersData(pCall->CacheResponseFields);
	size_t iResponseCount =
		xrtHttpHeadersCount(pCall->CacheResponseFields);
	xhttpcachecontrol Request;
	xhttpcachecontrol Response;
	xhttpcachetime Time;
	xhttpcachestoreinput StoreInput;
	xhttpcachestoreplan StorePlan;
	xhttpcachefragmentinput FragmentInput;
	xhttpcachefragmentplan FragmentPlan;
	xhttpcachefragmentdecision FragmentDecision;
	__xrt_http_client_cache_multipart Multipart;
	__xrt_http_client_cache_multipart_decision
		MultipartDecision =
			__XRT_HTTP_CLIENT_CACHE_MULTIPART_NONE;
	xhttpcacherecordinput Input;
	xhttpcachekey Key;
	xhttpcachepart Part;
	const xhttpcachepart* pRecordParts;
	xhttpcacherecord* pRecord;
	xhttpheaders* pNormalized;
	xhttpfield* pStored;
	const xhttpfield* pContentType;
	size_t iStored;
	size_t iRecordPartCount;
	xhttpcacheput Put;
	xhttpnext ContentTypeNext;
	uint16 iSourceStatus =
		xrtHttpResponseStatus(pResponse);
	uint16 iStoredStatus = iSourceStatus;
	bool bFragment = false;
	bool bMultipart = false;
	bool bComplete = false;

	memset(&Multipart, 0, sizeof(Multipart));
	if ( !pCall->CacheCapture ||
		(pCall->CacheResponseFields == NULL) ) {
		return true;
	}
	if ( (xrtHttpRequestHeader(
			pCall->Request,
			XRT_STR_LITERAL("Range")
		 ) != NULL) &&
		(iSourceStatus != XHTTP_STATUS_OK) &&
		(iSourceStatus !=
		 XHTTP_STATUS_PARTIAL_CONTENT) ) {
		return true;
	}
	if ( !__xrtHttpClientCacheControl(
		pRequestFields,
		iRequestCount,
		&Request
	) || !__xrtHttpClientCacheControl(
		pResponseFields,
		iResponseCount,
		&Response
	) || !xrtHttpCacheTimeParse(
		pResponseFields,
		iResponseCount,
		&Time
	) || !xrtHttpCacheStoreInputInit(
		&StoreInput,
		xrtHttpRequestMethod(pCall->Request),
		xrtHttpResponseStatus(pResponse),
		pCall->Client->Config.Cache.Shared
	) ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"plan-http-cache-store",
			"HTTP cache response metadata is invalid"
		);
	}
	StoreInput.Flags |=
		XHTTP_CACHE_STORE_RANGE_SUPPORTED;
	if ( xrtHttpCacheStorePlan(
		&Request,
		&Response,
		&Time,
		&StoreInput,
		&StorePlan
	) != XHTTP_CACHE_STORE_KEEP ) {
		if ( StorePlan.Decision == XHTTP_CACHE_STORE_ERROR ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"plan-http-cache-store",
				"HTTP cache store plan failed"
			);
		}
		return true;
	}
	if ( xrtHttpMethodEqual(
			xrtHttpRequestMethod(pCall->Request),
			XRT_STR_LITERAL("GET")
		) && ((iSourceStatus == XHTTP_STATUS_OK) ||
		 (iSourceStatus ==
		  XHTTP_STATUS_PARTIAL_CONTENT)) ) {
		xrtHttpCacheFragmentInputInit(&FragmentInput);
		FragmentInput.Method =
			xrtHttpRequestMethod(pCall->Request);
		FragmentInput.Fields = pResponseFields;
		FragmentInput.FieldCount = iResponseCount;
		FragmentInput.ResponseTime =
			pCall->CacheResponseTime;
		FragmentInput.BodySize =
			(uint64)pCall->CacheBodySize;
		FragmentInput.Status = iSourceStatus;
		FragmentInput.Flags =
			XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
			XHTTP_CACHE_FRAGMENT_BODY_COMPLETE;
		if ( iSourceStatus ==
			XHTTP_STATUS_PARTIAL_CONTENT ) {
			ContentTypeNext = xrtHttpFieldGetUnique(
				pResponseFields,
				iResponseCount,
				XRT_STR_LITERAL("Content-Type"),
				&pContentType
			);
			if ( ContentTypeNext == XHTTP_NEXT_ITEM ) {
				MultipartDecision =
					__xrtHttpClientCacheMultipartPlan(
						&FragmentInput,
						pContentType->Value,
						(xbytesview){
							pCall->CacheBody,
							pCall->CacheBodySize
						},
						pCall->Client->Config.Cache.MaxRanges,
						&Multipart
					);
			} else if ( ContentTypeNext == XHTTP_NEXT_ERROR ) {
				xrtClearError();
			}
			if ( (MultipartDecision ==
				  __XRT_HTTP_CLIENT_CACHE_MULTIPART_ERROR) ||
				(MultipartDecision ==
				  __XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP) ) {
				bool bResult =
					__xrtHttpClientCacheRecover(
						pCall,
						"plan-http-cache-multipart",
						"HTTP multipart range response could not be cached"
					);

				__xrtHttpClientCacheMultipartUnit(
					&Multipart
				);
				return bResult;
			}
			if ( MultipartDecision ==
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_STORE ) {
				bMultipart = true;
				bFragment = true;
				iStoredStatus = XHTTP_STATUS_OK;
			}
		}
		if ( !bMultipart ) {
			FragmentDecision =
				xrtHttpCacheFragmentPlan(
					&FragmentInput,
					&FragmentPlan
				);
			if ( FragmentDecision ==
				XHTTP_CACHE_FRAGMENT_ERROR ) {
				return __xrtHttpClientCacheRecover(
					pCall,
					"plan-http-cache-fragment",
					"HTTP cache response fragment is invalid"
				);
			}
			if ( FragmentDecision !=
				XHTTP_CACHE_FRAGMENT_STORE ) {
				return true;
			}
			bFragment = true;
			if ( (FragmentPlan.Actions &
				  XHTTP_CACHE_FRAGMENT_AS_200) != 0 ) {
				iStoredStatus = XHTTP_STATUS_OK;
			}
		}
	}
	pStored = __xrtHttpClientCacheStoredFields(
		pCall->CacheResponseFields,
		pCall->Client->Config.Cache.Shared,
		StorePlan.Actions,
		&iStored
	);
	if ( (pStored == NULL) && (iResponseCount != 0) ) {
		__xrtHttpClientCacheMultipartUnit(
			&Multipart
		);
		return __xrtHttpClientCacheRecover(
			pCall,
			"filter-http-cache-fields",
			"HTTP cache response fields could not be filtered"
		);
	}
	pNormalized = __xrtHttpClientCacheHeadersCreate(
		pStored,
		iStored
	);
	xrtFree(pStored);
	if ( pNormalized == NULL ) {
		__xrtHttpClientCacheMultipartUnit(
			&Multipart
		);
		return __xrtHttpClientCacheRecover(
			pCall,
			"copy-http-cache-fields",
			"HTTP cache response fields could not be copied"
		);
	}
	if ( bMultipart &&
		!__xrtHttpClientCacheMultipartHeaders(
			pNormalized,
			&Multipart
		) ) {
		xrtHttpHeadersDestroy(pNormalized);
		__xrtHttpClientCacheMultipartUnit(
			&Multipart
		);
		return __xrtHttpClientCacheRecover(
			pCall,
			"normalize-http-cache-multipart",
			"HTTP multipart range fields could not be normalized"
		);
	}
	if ( !bMultipart && bFragment &&
		!__xrtHttpClientCacheNormalizeHeaders(
			pNormalized,
			(FragmentPlan.Actions &
			 XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_RANGE) != 0,
			(FragmentPlan.Actions &
			 XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_LENGTH) != 0,
			(FragmentPlan.Actions &
			 XHTTP_CACHE_FRAGMENT_SET_CONTENT_LENGTH) != 0,
			FragmentPlan.Fragment.Length
		) ) {
		xrtHttpHeadersDestroy(pNormalized);
		__xrtHttpClientCacheMultipartUnit(
			&Multipart
		);
		return __xrtHttpClientCacheRecover(
			pCall,
			"normalize-http-cache-fields",
			"HTTP cache response fields could not be normalized"
		);
	}
	if ( !__xrtHttpClientCacheKey(pCall, &Key) ||
		!xrtHttpCacheRecordInputInit(
			&Input,
			&Key,
			iStoredStatus
		) ) {
		xrtHttpHeadersDestroy(pNormalized);
		__xrtHttpClientCacheMultipartUnit(
			&Multipart
		);
		return false;
	}
	Part.Offset = !bMultipart && bFragment &&
		((FragmentPlan.Fragment.Flags &
		  XHTTP_CACHE_FRAGMENT_HAS_RANGE) != 0) ?
			FragmentPlan.Fragment.Range.First : 0;
	Part.Data = (xbytesview){
		pCall->CacheBody,
		pCall->CacheBodySize
	};
	pRecordParts = bMultipart ?
		Multipart.Parts :
		(pCall->CacheBodySize != 0 ? &Part : NULL);
	iRecordPartCount = bMultipart ?
		Multipart.PartCount :
		(pCall->CacheBodySize != 0 ? 1u : 0u);
	Input.Version = xrtHttpResponseVersion(pResponse);
	Input.Reason = iStoredStatus == iSourceStatus ?
		xrtHttpResponseReason(pResponse) :
		XRT_STR_LITERAL("OK");
	Input.Fields = xrtHttpHeadersData(pNormalized);
	Input.FieldCount = xrtHttpHeadersCount(pNormalized);
	if ( iSourceStatus !=
		XHTTP_STATUS_PARTIAL_CONTENT ) {
		Input.Trailers = xrtHttpResponseTrailerData(pResponse);
		Input.TrailerCount =
			xrtHttpResponseTrailerCount(pResponse);
	}
	Input.Parts = pRecordParts;
	Input.PartCount = iRecordPartCount;
	if ( bMultipart ) {
		if ( (Multipart.Flags &
			  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_LENGTH) != 0 ) {
			Input.Flags |=
				XHTTP_CACHE_RECORD_HAS_LENGTH;
			Input.Length = Multipart.Length;
		}
		if ( (Multipart.Flags &
			  __XRT_HTTP_CLIENT_CACHE_MULTIPART_COMPLETE) != 0 ) {
			Input.Flags |=
				XHTTP_CACHE_RECORD_COMPLETE;
			bComplete = true;
		}
	} else if ( bFragment ) {
		if ( (FragmentPlan.Fragment.Flags &
			  XHTTP_CACHE_FRAGMENT_HAS_LENGTH) != 0 ) {
			Input.Flags |=
				XHTTP_CACHE_RECORD_HAS_LENGTH;
			Input.Length =
				FragmentPlan.Fragment.Length;
		}
		if ( xrtHttpCacheFragmentComplete(
			&FragmentPlan.Fragment
		) ) {
			Input.Flags |=
				XHTTP_CACHE_RECORD_COMPLETE;
			bComplete = true;
		}
	} else {
		Input.Length =
			(uint64)pCall->CacheBodySize;
		Input.Flags =
			XHTTP_CACHE_RECORD_HAS_LENGTH |
			XHTTP_CACHE_RECORD_COMPLETE;
		bComplete = true;
	}
	Input.ResponseTime = pCall->CacheResponseTime;
	Input.RequestClock = pCall->CacheRequestClock;
	Input.ResponseClock = pCall->CacheResponseClock;
	pRecord = xrtHttpCacheRecordCreate(&Input);
	xrtHttpHeadersDestroy(pNormalized);
	__xrtHttpClientCacheMultipartUnit(&Multipart);
	if ( pRecord == NULL ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"create-http-cache-record",
			"HTTP cache record could not be created"
		);
	}
	if ( bFragment &&
		(iSourceStatus ==
		 XHTTP_STATUS_PARTIAL_CONTENT) &&
		!bComplete ) {
		bool bResult =
			__xrtHttpClientCacheFragmentCommit(
				pCall,
				pRecord
			);

		xrtHttpCacheRecordRelease(pRecord);
		return bResult;
	}
	Put = xrtHttpCachePut(
		pCall->Client->Cache,
		pRecord
	);
	xrtHttpCacheRecordRelease(pRecord);
	if ( Put == XHTTP_CACHE_PUT_ERROR ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"store-http-cache-record",
			"HTTP cache backend could not store the response"
		);
	}
	if ( Put > XHTTP_CACHE_PUT_REJECTED ) {
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_UPDATED
		);
	}
	return true;
}



/* 创建保存字段的可修改副本。 */
static xhttpheaders* __xrtHttpClientCacheRecordHeaders(
	const xhttpcacherecord* pRecord
)
{
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	size_t i;

	if ( pHeaders == NULL ) {
		return NULL;
	}
	for ( i = 0;
		i < xrtHttpCacheRecordFieldCount(pRecord);
		i++ ) {
		const xhttpfield* pField =
			xrtHttpCacheRecordFieldAt(pRecord, i);

		if ( (pField == NULL) ||
			!xrtHttpHeadersAdd(
				pHeaders,
				pField->Name,
				pField->Value
			) ) {
			xrtHttpHeadersDestroy(pHeaders);
			return NULL;
		}
	}
	return pHeaders;
}



/* 判断字段名是否已经在当前输入前缀出现。 */
static bool __xrtHttpClientCacheFieldEarlier(
	const xhttpfield* pFields,
	size_t iIndex
)
{
	size_t i;

	for ( i = 0; i < iIndex; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pFields[i].Name,
			pFields[iIndex].Name
		) ) {
			return true;
		}
	}
	return false;
}



/* 按验证或 HEAD 元数据建立保留原正文的不可变记录。 */
static xhttpcacherecord* __xrtHttpClientCacheUpdateRecord(
	xhttpcall* pCall,
	bool bSelect304
)
{
	const xhttpcachekey* pCandidateKey =
		xrtHttpCacheRecordKey(pCall->CacheCandidate);
	const xhttpfield* pFields = xrtHttpHeadersData(
		pCall->CacheResponseFields
	);
	size_t iCount = xrtHttpHeadersCount(
		pCall->CacheResponseFields
	);
	xhttpcacheentry Entry;
	xhttpcacheupdatematch Match;
	xhttpheaders* pMerged;
	xhttpcachekey Key;
	xhttpcacherecordinput Input;
	xhttpcachepart* pParts = NULL;
	xhttpfield* pTrailers = NULL;
	xhttpcacherecord* pRecord = NULL;
	size_t iPartCount;
	size_t iTrailerCount;
	size_t iSelected;
	size_t i;

	if ( (pCandidateKey == NULL) ||
		!xrtHttpCacheRecordEntry(
		pCall->CacheCandidate,
		&Entry
	) ) {
		return NULL;
	}
	if ( bSelect304 ) {
		Match = xrtHttpCache304Select(
			pFields,
			iCount,
			&Entry,
			1,
			NULL,
			0,
			&iSelected
		);
		if ( (Match == XHTTP_CACHE_UPDATE_MATCH_ERROR) ||
			(Match == XHTTP_CACHE_UPDATE_MATCH_NONE) ) {
			if ( Match ==
				XHTTP_CACHE_UPDATE_MATCH_NONE ) {
				__xrtErrorSetInvalidState();
			}
			return NULL;
		}
	}
	pMerged = __xrtHttpClientCacheRecordHeaders(
		pCall->CacheCandidate
	);
	if ( pMerged == NULL ) {
		return NULL;
	}
	for ( i = 0; i < iCount; i++ ) {
		xhttpcachefieldupdate Update;
		size_t j;

		if ( __xrtHttpClientCacheFieldEarlier(
			pFields,
			i
		) ) {
			continue;
		}
		Update = xrtHttpCacheFieldUpdate(
			pFields,
			iCount,
			i,
			pCall->Client->Config.Cache.Shared,
			xrtHttpFieldNameEqual(
				pFields[i].Name,
				XRT_STR_LITERAL("Content-Range")
			) ?
				XHTTP_CACHE_UPDATE_FIELD_PROCESSED :
				XHTTP_CACHE_UPDATE_FIELD_NONE
		);
		if ( Update == XHTTP_CACHE_FIELD_UPDATE_ERROR ) {
			xrtHttpHeadersDestroy(pMerged);
			return NULL;
		}
		if ( Update == XHTTP_CACHE_FIELD_UPDATE_SKIP ) {
			continue;
		}
		(void)xrtHttpHeadersRemove(
			pMerged,
			pFields[i].Name
		);
		for ( j = i; j < iCount; j++ ) {
			if ( xrtHttpFieldNameEqual(
				pFields[j].Name,
				pFields[i].Name
			) && !xrtHttpHeadersAdd(
				pMerged,
				pFields[j].Name,
				pFields[j].Value
			) ) {
				xrtHttpHeadersDestroy(pMerged);
				return NULL;
			}
		}
	}
	iPartCount = xrtHttpCacheRecordPartCount(
		pCall->CacheCandidate
	);
	iTrailerCount = xrtHttpCacheRecordTrailerCount(
		pCall->CacheCandidate
	);
	if ( iPartCount != 0 ) {
		if ( iPartCount >
			(SIZE_MAX / sizeof(*pParts)) ) {
			xrtHttpHeadersDestroy(pMerged);
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		pParts = (xhttpcachepart*)xrtMalloc(
			iPartCount * sizeof(*pParts)
		);
		if ( pParts == NULL ) {
			xrtHttpHeadersDestroy(pMerged);
			return NULL;
		}
		for ( i = 0; i < iPartCount; i++ ) {
			const xhttpcachepart* pPart =
				xrtHttpCacheRecordPartAt(
					pCall->CacheCandidate,
					i
				);

			if ( pPart == NULL ) {
				xrtFree(pParts);
				xrtHttpHeadersDestroy(pMerged);
				__xrtErrorSetInternal();
				return NULL;
			}
			pParts[i] = *pPart;
		}
	}
	if ( iTrailerCount != 0 ) {
		if ( iTrailerCount >
			(SIZE_MAX / sizeof(*pTrailers)) ) {
			xrtFree(pParts);
			xrtHttpHeadersDestroy(pMerged);
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		pTrailers = (xhttpfield*)xrtMalloc(
			iTrailerCount * sizeof(*pTrailers)
		);
		if ( pTrailers == NULL ) {
			xrtFree(pParts);
			xrtHttpHeadersDestroy(pMerged);
			return NULL;
		}
		for ( i = 0; i < iTrailerCount; i++ ) {
			const xhttpfield* pTrailer =
				xrtHttpCacheRecordTrailerAt(
					pCall->CacheCandidate,
					i
				);

			if ( pTrailer == NULL ) {
				xrtFree(pTrailers);
				xrtFree(pParts);
				xrtHttpHeadersDestroy(pMerged);
				__xrtErrorSetInternal();
				return NULL;
			}
			pTrailers[i] = *pTrailer;
		}
	}
	if ( __xrtHttpClientCacheKeyMethod(
			pCall,
			pCandidateKey->Method,
			&Key
		) &&
		xrtHttpCacheRecordInputInit(
			&Input,
			&Key,
			xrtHttpCacheRecordStatus(
				pCall->CacheCandidate
			)
		) ) {
		Input.Version = xrtHttpCacheRecordVersion(
			pCall->CacheCandidate
		);
		Input.Flags = xrtHttpCacheRecordFlags(
			pCall->CacheCandidate
		);
		Input.Reason = xrtHttpCacheRecordReason(
			pCall->CacheCandidate
		);
		Input.Fields = xrtHttpHeadersData(pMerged);
		Input.FieldCount = xrtHttpHeadersCount(pMerged);
		Input.Trailers = pTrailers;
		Input.TrailerCount = iTrailerCount;
		Input.Parts = pParts;
		Input.PartCount = iPartCount;
		Input.Length = xrtHttpCacheRecordLength(
			pCall->CacheCandidate
		);
		Input.ResponseTime = pCall->CacheResponseTime;
		Input.RequestClock = pCall->CacheRequestClock;
		Input.ResponseClock = pCall->CacheResponseClock;
		pRecord = xrtHttpCacheRecordCreate(&Input);
	}
	xrtFree(pTrailers);
	xrtFree(pParts);
	xrtHttpHeadersDestroy(pMerged);
	return pRecord;
}



/*
	用 200 HEAD 更新或条件删除当前选择的 GET 表示。
	更新冲突表示已有更新结果胜出，不再额外保存重复 HEAD 记录。
*/
static bool __xrtHttpClientCacheHeadFreshen(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	bool* pHandled
)
{
	xhttpcacheentry Entry;
	xhttpcacheheaddecision Decision;
	xhttpcacherecord* pRecord;
	xhttpcacheput Put;
	xhttpcachechange Change;

	*pHandled = false;
	if ( !__xrtHttpClientCacheHeadFromGet(pCall) ||
		(xrtHttpResponseStatus(pResponse) !=
		 XHTTP_STATUS_OK) ||
		(pCall->CacheResponseFields == NULL) ) {
		return true;
	}
	if ( !xrtHttpCacheRecordEntry(
		pCall->CacheCandidate,
		&Entry
	) ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"prepare-http-cache-head",
			"HTTP HEAD cache candidate is invalid"
		);
	}
	Decision = xrtHttpCacheHeadPlan(
		xrtHttpResponseStatus(pResponse),
		&Entry,
		xrtHttpHeadersData(pCall->CacheResponseFields),
		xrtHttpHeadersCount(pCall->CacheResponseFields)
	);
	if ( Decision == XHTTP_CACHE_HEAD_ERROR ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"plan-http-cache-head",
			"HTTP HEAD cache metadata could not be compared"
		);
	}
	if ( Decision == XHTTP_CACHE_HEAD_IGNORE ) {
		return true;
	}
	if ( Decision == XHTTP_CACHE_HEAD_STALE ) {
		Change = xrtHttpCacheRemoveRecord(
			pCall->Client->Cache,
			pCall->CacheCandidate
		);
		if ( Change == XHTTP_CACHE_CHANGE_ERROR ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"invalidate-http-cache-head",
				"HTTP HEAD could not invalidate the stale GET response"
			);
		}
		if ( Change == XHTTP_CACHE_CHANGE_CONFLICT ) {
			*pHandled = true;
		}
		return true;
	}

	pRecord = __xrtHttpClientCacheUpdateRecord(
		pCall,
		false
	);
	if ( pRecord == NULL ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"update-http-cache-head",
			"HTTP HEAD metadata could not update the GET response"
		);
	}
	Put = xrtHttpCacheReplace(
		pCall->Client->Cache,
		pCall->CacheCandidate,
		pRecord
	);
	xrtHttpCacheRecordRelease(pRecord);
	*pHandled = true;
	if ( Put == XHTTP_CACHE_PUT_ERROR ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"store-http-cache-head",
			"HTTP cache backend could not store HEAD metadata"
		);
	}
	if ( Put == XHTTP_CACHE_PUT_REPLACED ) {
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_UPDATED
		);
	}
	return true;
}



/* 按缓存范围交付状态重写 Content-Range 与 Content-Length。 */
static bool __xrtHttpClientCacheResponseRange(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	const xhttpcacherecord* pRecord
)
{
	static const char sMultipartPrefix[] =
		"multipart/byteranges; boundary=";
	xhttpheaders* pHeaders = (xhttpheaders*)
		xrtHttpResponseHeaders(pResponse);
	xhttpcontentrange ContentRange;
	char sLength[32];
	char sType[
		(sizeof(sMultipartPrefix) - 1u) +
		XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE
	];
	str sRange;
	size_t iRangeSize;
	uint64 iLength;
	int iWritten;

	/*
		HEAD 使用 GET 记录的原始表示元数据，但不执行 Range，
		也不能把无正文误写成长度为零的范围响应。
	*/
	if ( __xrtHttpClientCacheHeadRequest(pCall) ||
		(pCall->CacheRangeState ==
		 __XRT_HTTP_CLIENT_CACHE_RANGE_NONE) ) {
		return true;
	}
	(void)xrtHttpHeadersRemove(
		pHeaders,
		XRT_STR_LITERAL("Content-Range")
	);
	(void)xrtHttpHeadersRemove(
		pHeaders,
		XRT_STR_LITERAL("Content-Length")
	);

	if ( pCall->CacheRangeState ==
		__XRT_HTTP_CLIENT_CACHE_RANGE_MULTIPART ) {
		(void)xrtHttpHeadersRemove(
			pHeaders,
			XRT_STR_LITERAL("Content-Type")
		);
		memcpy(
			sType,
			sMultipartPrefix,
			sizeof(sMultipartPrefix) - 1u
		);
		memcpy(
			sType + sizeof(sMultipartPrefix) - 1u,
			pCall->CacheBoundary,
			XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE
		);
		iWritten = snprintf(
			sLength,
			sizeof(sLength),
			"%llu",
			(unsigned long long)
				pCall->CacheRangeBodyLength
		);
		return (iWritten >= 0) &&
			((size_t)iWritten < sizeof(sLength)) &&
			xrtHttpHeadersSet(
				pHeaders,
				XRT_STR_LITERAL("Content-Type"),
				(xstrview){ sType, sizeof(sType) }
			) &&
			xrtHttpHeadersSet(
				pHeaders,
				XRT_STR_LITERAL("Content-Length"),
				(xstrview){
					sLength,
					(size_t)iWritten
				}
			);
	}

	memset(&ContentRange, 0, sizeof(ContentRange));
	ContentRange.HasLength = true;
	ContentRange.Length =
		xrtHttpCacheRecordLength(pRecord);
	if ( pCall->CacheRangeState ==
		__XRT_HTTP_CLIENT_CACHE_RANGE_PARTIAL ) {
		ContentRange.Satisfied = true;
		ContentRange.First =
			pCall->CacheRange.First;
		ContentRange.Last =
			pCall->CacheRange.Last;
		iLength =
			(pCall->CacheRange.Last -
			 pCall->CacheRange.First) +
			UINT64_C(1);
	} else {
		iLength = 0;
	}

	sRange = xrtHttpContentRangeBuild(
		&ContentRange,
		&iRangeSize
	);
	if ( sRange == NULL ) {
		return false;
	}
	iWritten = snprintf(
		sLength,
		sizeof(sLength),
		"%llu",
		(unsigned long long)iLength
	);
	if ( (iWritten < 0) ||
		((size_t)iWritten >= sizeof(sLength)) ||
		!xrtHttpHeadersSet(
			pHeaders,
			XRT_STR_LITERAL("Content-Range"),
			(xstrview){ sRange, iRangeSize }
		) || !xrtHttpHeadersSet(
			pHeaders,
			XRT_STR_LITERAL("Content-Length"),
			(xstrview){
				sLength,
				(size_t)iWritten
			}
		) ) {
		xrtFree(sRange);
		return false;
	}
	xrtFree(sRange);
	return true;
}



/* 重建一个拥有型响应并为复用路径写入当前 Age。 */
static xhttpresponse* __xrtHttpClientCacheResponse(
	xhttpcall* pCall,
	const xhttpcacherecord* pRecord
)
{
	xhttpresponse* pResponse;
	xhttpcachetime Time;
	xhttpcacheage Age;
	xstrview Reason = xrtHttpCacheRecordReason(pRecord);
	char sAge[32];
	int iAge;
	uint16 iStatus = xrtHttpCacheRecordStatus(pRecord);
	size_t i;

	if ( (pCall->CacheRangeState ==
		  __XRT_HTTP_CLIENT_CACHE_RANGE_PARTIAL) ||
		(pCall->CacheRangeState ==
		 __XRT_HTTP_CLIENT_CACHE_RANGE_MULTIPART) ) {
		iStatus = XHTTP_STATUS_PARTIAL_CONTENT;
		Reason = XRT_STR_LITERAL("Partial Content");
	} else if ( pCall->CacheRangeState ==
		__XRT_HTTP_CLIENT_CACHE_RANGE_UNSATISFIABLE ) {
		iStatus = XHTTP_STATUS_RANGE_NOT_SATISFIABLE;
		Reason = XRT_STR_LITERAL("Range Not Satisfiable");
	}
	pResponse = __xrtHttpResponseCreate(
		xrtHttpCacheRecordVersion(pRecord),
		iStatus,
		Reason,
		&pCall->Client->Config.Exchange.Headers
	);
	if ( pResponse == NULL ) {
		return NULL;
	}
	for ( i = 0;
		i < xrtHttpCacheRecordFieldCount(pRecord);
		i++ ) {
		const xhttpfield* pField =
			xrtHttpCacheRecordFieldAt(pRecord, i);

		if ( (pField == NULL) ||
			!__xrtHttpResponseAddHeader(
				pResponse,
				pField->Name,
				pField->Value
			) ) {
			xrtHttpResponseDestroy(pResponse);
			return NULL;
		}
	}
	if ( (pCall->CacheRangeState ==
		  __XRT_HTTP_CLIENT_CACHE_RANGE_NONE) &&
		!__xrtHttpClientCacheHeadRequest(pCall) ) {
		for ( i = 0;
			i < xrtHttpCacheRecordTrailerCount(pRecord);
			i++ ) {
			const xhttpfield* pField =
				xrtHttpCacheRecordTrailerAt(pRecord, i);

			if ( (pField == NULL) ||
				!__xrtHttpResponseAddTrailer(
					pResponse,
					&pCall->Client->Config.Exchange.Trailers,
					pField->Name,
					pField->Value
				) ) {
				xrtHttpResponseDestroy(pResponse);
				return NULL;
			}
		}
	}
	if ( !__xrtHttpResponseSetUrl(
		pResponse,
		xrtHttpRequestUrlText(pCall->Request)
	) ) {
		xrtHttpResponseDestroy(pResponse);
		return NULL;
	}
	__xrtHttpResponseSetFlags(
		pResponse,
		XHTTP_RESPONSE_STREAMED
	);
	if ( xrtHttpCacheTimeParse(
		xrtHttpHeadersData(
			xrtHttpResponseHeaders(pResponse)
		),
		xrtHttpHeadersCount(
			xrtHttpResponseHeaders(pResponse)
		),
		&Time
	) && (xrtHttpCacheCurrentAge(
		&Time,
		xrtHttpCacheRecordResponseTime(pRecord),
		xrtHttpCacheRecordRequestClock(pRecord),
		xrtHttpCacheRecordResponseClock(pRecord),
		xrtClock(),
		&Age
	) == XHTTP_CACHE_CALC_READY) ) {
		iAge = snprintf(
			sAge,
			sizeof(sAge),
			"%llu",
			(unsigned long long)Age.CurrentAgeSeconds
		);
		if ( (iAge < 0) ||
			((size_t)iAge >= sizeof(sAge)) ||
			!xrtHttpHeadersSet(
				(xhttpheaders*)xrtHttpResponseHeaders(
					pResponse
				),
				XRT_STR_LITERAL("Age"),
				(xstrview){ sAge, (size_t)iAge }
			) ) {
			xrtHttpResponseDestroy(pResponse);
			return NULL;
		}
	} else {
		xrtClearError();
	}
	if ( !__xrtHttpClientCacheResponseRange(
		pCall,
		pResponse,
		pRecord
	) ) {
		xrtHttpResponseDestroy(pResponse);
		return NULL;
	}
	return pResponse;
}



/* 把缓存重建过程中的回调失败提升为唯一高层终态。 */
static void __xrtHttpClientCacheCallbackFail(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();

	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( __xrtHttpRedirectFail(pCall, pCause) ) {
			xrtHttpResponseDestroy(pResponse);
			xrtErrorFree(pCause);
			return;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		if ( __xrtHttpDecompressFail(pCall, pCause) ) {
			xrtHttpResponseDestroy(pResponse);
			xrtErrorFree(pCause);
			return;
		}
	#endif
	xrtHttpResponseDestroy(pResponse);
	__xrtHttpCallFail(
		pCall,
		XNET_RESULT_ERROR,
		XHTTP_CLIENT_ERROR_CALLBACK,
		__xrtHttpClientCauseKind(
			pCause,
			XERR_CANCELLED
		),
		"deliver-http-cache-response",
		sMessage,
		pCause
	);
	xrtErrorFree(pCause);
}



/* 缓存重放交付区分继续、回调已终止和内部错误。 */
typedef enum __xrt_http_client_cache_delivery {
	__XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR = -1,
	__XRT_HTTP_CLIENT_CACHE_DELIVERY_STOP = 0,
	__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE = 1
} __xrt_http_client_cache_delivery;



/* 以响应限额终止缓存重放，保持与网络 Exchange 相同的高层错误分类。 */
static __xrt_http_client_cache_delivery
__xrtHttpClientCacheBodyLimitFail(
	xhttpcall* pCall,
	xhttpresponse* pResponse
)
{
	xerror* pCause;

	__xrtErrorSetRange();
	pCause = xrtTakeError();
	xrtHttpResponseDestroy(pResponse);
	__xrtHttpCallFail(
		pCall,
		XNET_RESULT_ERROR,
		XHTTP_CLIENT_ERROR_RESPONSE,
		XERR_RANGE,
		"receive-http-response",
		"HTTP response exceeds its configured body limit",
		pCause
	);
	xrtErrorFree(pCause);
	return __XRT_HTTP_CLIENT_CACHE_DELIVERY_STOP;
}



/* 截取一个缓存片段与指定范围的交集；空范围表示完整片段。 */
static xbytesview __xrtHttpClientCachePartView(
	const xhttpcachepart* pPart,
	const xhttpbyterange* pRange
)
{
	xbytesview Data = pPart->Data;
	uint64 iPartLast;
	uint64 iFirst;
	uint64 iLast;
	size_t iOffset;
	size_t iSize;

	if ( pRange == NULL ) {
		return Data;
	}
	if ( Data.Size == 0 ) {
		return (xbytesview){ NULL, 0 };
	}

	iPartLast = pPart->Offset +
		(uint64)pPart->Data.Size -
		UINT64_C(1);
	if ( (iPartLast < pRange->First) ||
		(pPart->Offset > pRange->Last) ) {
		return (xbytesview){ NULL, 0 };
	}
	iFirst = pPart->Offset >
		pRange->First ?
			pPart->Offset :
			pRange->First;
	iLast = iPartLast <
		pRange->Last ?
			iPartLast :
			pRange->Last;
	iOffset = (size_t)(iFirst - pPart->Offset);
	iSize = (size_t)(
		(iLast - iFirst) + UINT64_C(1)
	);
	return (xbytesview){
		pPart->Data.Data + iOffset,
		iSize
	};
}



/* 把一段缓存或 framing 字节交给缓冲响应或用户 Body 回调。 */
static __xrt_http_client_cache_delivery
__xrtHttpClientCacheDeliver(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	xbytesview Data
)
{
	uint64 iDelivered;
	bool bAccepted;

	if ( Data.Size == 0 ) {
		return __XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE;
	}
	iDelivered = xrtHttpResponseWireBodyBytes(pResponse);
	if ( (iDelivered > pCall->ResponseBodyLimit) ||
		((uint64)Data.Size >
		 (pCall->ResponseBodyLimit - iDelivered)) ) {
		return __xrtHttpClientCacheBodyLimitFail(
			pCall,
			pResponse
		);
	}
	/* 与网络 Exchange 一致，回调可见计数必须包含当前线路块。 */
	if ( !__xrtHttpResponseAddWireBody(
		pResponse,
		(uint64)Data.Size
	) ) {
		__xrtHttpClientCacheCallbackFail(
			pCall,
			pResponse,
			"HTTP cache response body byte count overflowed"
		);
		return __XRT_HTTP_CLIENT_CACHE_DELIVERY_STOP;
	}
	/* Header 回调留下的非失败错误不能污染下一次 Body 回调。 */
	xrtClearError();
	if ( pCall->CacheNext.Body == NULL ) {
		bAccepted =
			__xrtHttpResponseBufferDeliveredBody(
				pResponse,
				Data
			);
	} else {
		bAccepted = pCall->CacheNext.Body(
			pResponse,
			Data,
			pCall->CacheNext.Data
		);
	}
	if ( !bAccepted ||
		!__xrtHttpResponseDeliverBody(
			pResponse,
			(uint64)Data.Size
		) ) {
		__xrtHttpClientCacheCallbackFail(
			pCall,
			pResponse,
			"HTTP cache response body callback stopped delivery"
		);
		return __XRT_HTTP_CLIENT_CACHE_DELIVERY_STOP;
	}
	return __XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE;
}



/* 从有序缓存片段流式交付一个完整或选定范围。 */
static __xrt_http_client_cache_delivery
__xrtHttpClientCacheDeliverRange(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	const xhttpcacherecord* pRecord,
	const xhttpbyterange* pRange,
	uint64* pDelivered
)
{
	uint64 iDelivered = 0;
	size_t i;

	for ( i = 0;
		i < xrtHttpCacheRecordPartCount(pRecord);
		i++ ) {
		const xhttpcachepart* pPart =
			xrtHttpCacheRecordPartAt(pRecord, i);
		xbytesview Data;
		__xrt_http_client_cache_delivery Delivery;

		if ( pPart == NULL ) {
			__xrtErrorSetInternal();
			return __XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR;
		}
		Data = __xrtHttpClientCachePartView(
			pPart,
			pRange
		);
		if ( Data.Size == 0 ) {
			continue;
		}
		Delivery = __xrtHttpClientCacheDeliver(
			pCall,
			pResponse,
			Data
		);
		if ( Delivery !=
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE ) {
			return Delivery;
		}
		if ( iDelivered >
			(UINT64_MAX - (uint64)Data.Size) ) {
			__xrtErrorSetSizeOverflow();
			return __XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR;
		}
		iDelivered += (uint64)Data.Size;
	}
	if ( pDelivered != NULL ) {
		*pDelivered = iDelivered;
	}
	return __XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE;
}



/* 流式交付 multipart/byteranges framing 与缓存正文切片。 */
static __xrt_http_client_cache_delivery
__xrtHttpClientCacheDeliverMultipart(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	const xhttpcacherecord* pRecord
)
{
	const xhttpbyterange* pRanges =
		__xrtHttpClientCacheRanges(pCall);
	xstrview ContentType =
		__xrtHttpClientCacheContentType(pRecord);
	xstrview Boundary = {
		pCall->CacheBoundary,
		XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE
	};
	bytes pHead = NULL;
	size_t iHeadCapacity = 0;
	uint8 sEnd[2];
	uint8 sClose[XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE + 6u];
	uint64 iCompleteLength =
		xrtHttpCacheRecordLength(pRecord);
	uint64 iDelivered = 0;
	size_t iEndSize;
	size_t iCloseSize;
	size_t i;
	__xrt_http_client_cache_delivery Delivery =
		__XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR;

	if ( (pRanges == NULL) ||
		(pCall->CacheRangeCount < 2u) ) {
		__xrtErrorSetInvalidState();
		return Delivery;
	}
	if ( !xrtHttpRangeMultipartEndWrite(
			sEnd,
			sizeof(sEnd),
			&iEndSize
		) || !xrtHttpRangeMultipartCloseWrite(
			Boundary,
			sClose,
			sizeof(sClose),
			&iCloseSize
		) ) {
		return Delivery;
	}

	for ( i = 0; i < pCall->CacheRangeCount; i++ ) {
		bytes pNext;
		size_t iHeadSize;
		size_t iWritten;
		uint64 iPayload;

		if ( !xrtHttpRangeMultipartHeadWrite(
			&pRanges[i],
			iCompleteLength,
			ContentType,
			Boundary,
			NULL,
			0,
			&iHeadSize
		) ) {
			goto cleanup;
		}
		if ( iHeadSize > iHeadCapacity ) {
			pNext = (bytes)xrtRealloc(
				pHead,
				iHeadSize
			);
			if ( pNext == NULL ) {
				goto cleanup;
			}
			pHead = pNext;
			iHeadCapacity = iHeadSize;
		}
		if ( !xrtHttpRangeMultipartHeadWrite(
			&pRanges[i],
			iCompleteLength,
			ContentType,
			Boundary,
			pHead,
			iHeadCapacity,
			&iWritten
		) || (iWritten != iHeadSize) ) {
			goto cleanup;
		}
		Delivery = __xrtHttpClientCacheDeliver(
			pCall,
			pResponse,
			(xbytesview){ pHead, iHeadSize }
		);
		if ( Delivery !=
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE ) {
			goto cleanup;
		}
		iDelivered += (uint64)iHeadSize;

		Delivery = __xrtHttpClientCacheDeliverRange(
			pCall,
			pResponse,
			pRecord,
			&pRanges[i],
			&iPayload
		);
		if ( Delivery !=
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE ) {
			goto cleanup;
		}
		if ( iPayload !=
			((pRanges[i].Last - pRanges[i].First) +
			 UINT64_C(1)) ) {
			__xrtErrorSetInternal();
			Delivery =
				__XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR;
			goto cleanup;
		}
		iDelivered += iPayload;

		Delivery = __xrtHttpClientCacheDeliver(
			pCall,
			pResponse,
			(xbytesview){ sEnd, iEndSize }
		);
		if ( Delivery !=
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE ) {
			goto cleanup;
		}
		iDelivered += (uint64)iEndSize;
	}

	Delivery = __xrtHttpClientCacheDeliver(
		pCall,
		pResponse,
		(xbytesview){ sClose, iCloseSize }
	);
	if ( Delivery !=
		__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE ) {
		goto cleanup;
	}
	iDelivered += (uint64)iCloseSize;
	if ( iDelivered != pCall->CacheRangeBodyLength ) {
		__xrtErrorSetInternal();
		Delivery = __XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR;
	}

cleanup:
	xrtFree(pHead);
	return Delivery;
}



/* 交付完整缓存记录并复用重定向、解压和用户事件链。 */
static bool __xrtHttpClientCacheReplay(
	xhttpcall* pCall,
	xhttpcacherecord* pRecord
)
{
	xhttpresponse* pResponse =
		__xrtHttpClientCacheResponse(
			pCall,
			pRecord
		);

	if ( pResponse == NULL ) {
		return false;
	}
	if ( (pCall->CacheNext.Headers != NULL) &&
		!pCall->CacheNext.Headers(
			pResponse,
			pCall->CacheNext.Data
		) ) {
		__xrtHttpClientCacheCallbackFail(
			pCall,
			pResponse,
			"HTTP cache response Header callback stopped delivery"
		);
		return true;
	}
	{
		__xrt_http_client_cache_delivery Delivery =
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE;
		const xhttpbyterange* pRange = NULL;

		if ( __xrtHttpClientCacheHeadRequest(pCall) ) {
			Delivery =
				__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE;
		} else if ( pCall->CacheRangeState ==
			__XRT_HTTP_CLIENT_CACHE_RANGE_MULTIPART ) {
			Delivery =
				__xrtHttpClientCacheDeliverMultipart(
					pCall,
					pResponse,
					pRecord
				);
		} else if ( pCall->CacheRangeState ==
			__XRT_HTTP_CLIENT_CACHE_RANGE_UNSATISFIABLE ) {
			Delivery =
				__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE;
		} else {
			if ( pCall->CacheRangeState ==
				__XRT_HTTP_CLIENT_CACHE_RANGE_PARTIAL ) {
				pRange = &pCall->CacheRange;
			}
			Delivery = __xrtHttpClientCacheDeliverRange(
				pCall,
				pResponse,
				pRecord,
				pRange,
				NULL
			);
		}
		if ( Delivery ==
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_STOP ) {
			return true;
		}
		if ( Delivery ==
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR ) {
			xrtHttpResponseDestroy(pResponse);
			return false;
		}
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( pCall->RedirectPending ) {
			xerror* pCause;

			xrtHttpResponseDestroy(pResponse);
			if ( !__xrtHttpRedirectAdvance(pCall) ) {
				pCause = xrtTakeError();
				(void)__xrtHttpRedirectFail(
					pCall,
					pCause
				);
				xrtErrorFree(pCause);
				return true;
			}
			__xrtHttpCallStartHop(pCall);
			return true;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		if ( !__xrtHttpDecompressFinish(
			pCall,
			pResponse
		) ) {
			__xrtHttpClientCacheCallbackFail(
				pCall,
				pResponse,
				"HTTP cache response final decoding failed"
			);
			return true;
		}
	#endif
	__xrtHttpCallSucceed(
		pCall,
		pResponse,
		NULL,
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			NULL,
		#endif
		0,
		false
	);
	return true;
}



/* 构造 only-if-cached 未命中的 504 空响应。 */
static xhttpresponse* __xrtHttpClientCacheGatewayTimeout(
	xhttpcall* pCall
)
{
	xhttpresponse* pResponse = __xrtHttpResponseCreate(
		XHTTP_VERSION_1_1,
		XHTTP_STATUS_GATEWAY_TIMEOUT,
		XRT_STR_LITERAL("Gateway Timeout"),
		&pCall->Client->Config.Exchange.Headers
	);

	if ( (pResponse == NULL) ||
		!__xrtHttpResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL("Content-Length"),
			XRT_STR_LITERAL("0")
		) || !__xrtHttpResponseSetUrl(
			pResponse,
			xrtHttpRequestUrlText(pCall->Request)
		) ) {
		xrtHttpResponseDestroy(pResponse);
		return NULL;
	}
	__xrtHttpResponseSetFlags(
		pResponse,
		XHTTP_RESPONSE_STREAMED
	);
	return pResponse;
}



/* 在连接池前交付 ready 命中或合成 504。 */
bool __xrtHttpClientCacheStart(
	xhttpcall* pCall,
	bool* pHandled
)
{
	xhttpresponse* pResponse;

	if ( (pCall == NULL) || (pHandled == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pHandled = false;
	if ( !pCall->CacheEnabled || !pCall->CacheReady ) {
		pCall->CacheRequestClock = xrtClock();
		return true;
	}
	*pHandled = true;
	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_CACHE
	);
	if ( pCall->CacheCandidate != NULL ) {
		return __xrtHttpClientCacheReplay(
			pCall,
			pCall->CacheCandidate
		);
	}
	pResponse = __xrtHttpClientCacheGatewayTimeout(pCall);
	if ( pResponse == NULL ) {
		return false;
	}
	if ( (pCall->CacheNext.Headers != NULL) &&
		!pCall->CacheNext.Headers(
			pResponse,
			pCall->CacheNext.Data
		) ) {
		__xrtHttpClientCacheCallbackFail(
			pCall,
			pResponse,
			"HTTP cache 504 Header callback stopped delivery"
		);
		return true;
	}
	__xrtHttpCallSucceed(
		pCall,
		pResponse,
		NULL,
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			NULL,
		#endif
		0,
		false
	);
	return true;
}



/* 保存完整响应，或把自有 304 合并为待重放记录。 */
bool __xrtHttpClientCacheDone(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	bool* pReplayed
)
{
	xhttpcacherecord* pRecord;
	xhttpcacheput Put;
	bool bHeadHandled;

	if ( (pCall == NULL) ||
		(pResponse == NULL) ||
		(pReplayed == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pReplayed = false;
	if ( !pCall->CacheEnabled ) {
		return true;
	}
	if ( !__xrtHttpClientCacheInvalidate(
		pCall,
		pResponse
	) ) {
		return false;
	}
	if ( !pCall->CacheNotModified ) {
		if ( !__xrtHttpClientCacheHeadFreshen(
			pCall,
			pResponse,
			&bHeadHandled
		) ) {
			return false;
		}
		if ( bHeadHandled ) {
			return true;
		}
		return __xrtHttpClientCacheStore(
			pCall,
			pResponse
		);
	}
	pRecord = __xrtHttpClientCacheUpdateRecord(
		pCall,
		true
	);
	if ( pRecord == NULL ) {
		return __xrtHttpClientCacheError(
			pCall,
			__xrtHttpClientCauseKind(
				xrtGetError(),
				XERR_PROTOCOL
			),
			"update-http-cache-validation",
			"HTTP 304 could not update the selected cache record",
			xrtGetError()
		);
	}
	Put = xrtHttpCacheReplace(
		pCall->Client->Cache,
		pCall->CacheCandidate,
		pRecord
	);
	if ( Put == XHTTP_CACHE_PUT_ERROR ) {
		if ( !__xrtHttpClientCacheRecover(
			pCall,
			"store-http-cache-validation",
			"HTTP cache backend could not store the validated response"
		) ) {
			xrtHttpCacheRecordRelease(pRecord);
			return false;
		}
	}
	xrtHttpCacheRecordRelease(pCall->CacheCandidate);
	pCall->CacheCandidate = pRecord;
	pCall->CacheReady = true;
	pCall->CacheNotModified = false;
	__xrtHttpClientCacheOutcome(
		pCall,
		XHTTP_CLIENT_CACHE_REVALIDATED
	);
	*pReplayed = true;
	return true;
}



/* 把严格模式缓存回调错误提升为稳定客户端错误。 */
bool __xrtHttpClientCacheFail(
	xhttpcall* pCall,
	const xerror* pCause
)
{
	if ( (pCall == NULL) || !pCall->CacheFailed ) {
		return false;
	}
	__xrtHttpCallFail(
		pCall,
		XNET_RESULT_ERROR,
		XHTTP_CLIENT_ERROR_CACHE,
		__xrtHttpClientCauseKind(
			pCause,
			XERR_IO
		),
		"process-http-cache",
		"HTTP client cache processing failed",
		pCause
	);
	return true;
}

#endif
