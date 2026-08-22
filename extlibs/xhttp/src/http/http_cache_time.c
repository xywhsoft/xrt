#include "../internal/xrt_http_cache.h"

#include <xrt/http_cache_time.h>



#if defined(XHTTP_FEATURE_HTTP_CACHE_TIME)

#define XRT_HTTP_CACHE_TIME_FLAGS \
	((uint32)XHTTP_CACHE_TIME_DATE | \
	 (uint32)XHTTP_CACHE_TIME_AGE | \
	 (uint32)XHTTP_CACHE_TIME_EXPIRES | \
	 (uint32)XHTTP_CACHE_TIME_DATE_DUPLICATE | \
	 (uint32)XHTTP_CACHE_TIME_AGE_EXTRA | \
	 (uint32)XHTTP_CACHE_TIME_EXPIRES_DUPLICATE | \
	 (uint32)XHTTP_CACHE_TIME_DATE_INVALID | \
	 (uint32)XHTTP_CACHE_TIME_AGE_INVALID | \
	 (uint32)XHTTP_CACHE_TIME_EXPIRES_INVALID)



/* 饱和累加缓存时长，避免任何线路值回绕。 */
uint64 __xrtHttpCacheTimeAdd(
	uint64 iLeft,
	uint64 iRight
)
{
	if ( iLeft > (UINT64_MAX - iRight) ) {
		return UINT64_MAX;
	}
	return iLeft + iRight;
}



/* 把线路秒数转换为微秒，溢出时保持饱和。 */
uint64 __xrtHttpCacheTimeSeconds(uint64 iSeconds)
{
	const uint64 iUnit = (uint64)XRT_TIME_SECOND;

	if ( iSeconds > (UINT64_MAX / iUnit) ) {
		return UINT64_MAX;
	}
	return iSeconds * iUnit;
}



/* 计算已知右值不小于左值的完整无符号时间差。 */
static uint64 __xrtHttpCacheTimeDiff(
	xtime iLeft,
	xtime iRight
)
{
	return (uint64)iRight - (uint64)iLeft;
}



/* 安全增加公开字段或成员计数。 */
static bool __xrtHttpCacheTimeCount(size_t* pCount)
{
	if ( *pCount == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	(*pCount)++;
	return true;
}



/* 判断字段是否使用指定的缓存时间名称。 */
static bool __xrtHttpCacheTimeField(
	const xhttpfield* pField,
	xstrview Name
)
{
	return xrtHttpFieldNameEqual(pField->Name, Name);
}



/* 合并一个单值 HTTP 日期字段，并保留重复和非法事实。 */
static bool __xrtHttpCacheTimeDateAdd(
	size_t* pCount,
	xtime* pValue,
	uint32* pFlags,
	uint32 iPresent,
	uint32 iDuplicate,
	uint32 iInvalid,
	xstrview Value
)
{
	xtime iTime;
	bool bFirst = *pCount == 0;

	if ( !__xrtHttpCacheTimeCount(pCount) ) {
		return false;
	}
	*pFlags |= iPresent;
	if ( !bFirst ) {
		*pFlags |= iDuplicate;
	}
	Value = xrtHttpOwsTrim(Value);
	if ( !xrtTimeTryParseHTTPDate(Value, &iTime) ) {
		*pFlags |= iInvalid;
		return true;
	}
	if ( bFirst ) {
		*pValue = iTime;
	}
	return true;
}



/* 统计并采用 Age 合并列表中的第一个非空成员。 */
static bool __xrtHttpCacheTimeAgeAdd(
	xhttpcachetime* pTime,
	xstrview Value
)
{
	size_t iOffset = 0;

	if ( !__xrtHttpCacheTimeCount(&pTime->AgeCount) ) {
		return false;
	}
	pTime->Flags |= XHTTP_CACHE_TIME_AGE;
	for ( ;; ) {
		size_t iStart = iOffset;
		xstrview Member;

		while ( (iOffset < Value.Size) &&
			(Value.Data[iOffset] != ',') ) {
			iOffset++;
		}
		Member.Data = iStart == 0 ?
			Value.Data :
			Value.Data + iStart;
		Member.Size = iOffset - iStart;
		Member = xrtHttpOwsTrim(Member);
		if ( Member.Size != 0 ) {
			uint64 iAge;

			if ( !__xrtHttpCacheTimeCount(
				&pTime->AgeMemberCount
			) ) {
				return false;
			}
			if ( pTime->AgeMemberCount == 1 ) {
				if ( __xrtHttpCacheDeltaParse(
					Member, false, false, &iAge
				) ) {
					pTime->Age = iAge;
				} else {
					pTime->Flags |=
						XHTTP_CACHE_TIME_AGE_INVALID;
				}
			} else {
				pTime->Flags |=
					XHTTP_CACHE_TIME_AGE_EXTRA;
			}
		}
		if ( iOffset == Value.Size ) {
			break;
		}
		iOffset++;
	}
	return true;
}



/* 验证年龄公式结果可安全用于新鲜度比较。 */
XRT_API bool xrtHttpCacheAgeValid(
	const xhttpcacheage* pAge
)
{
	uint64 iInitial;
	uint64 iCurrent;
	uint64 iSeconds;

	if ( pAge == NULL ) {
		return false;
	}
	iInitial = pAge->ApparentAge >
		pAge->CorrectedAgeValue ?
		pAge->ApparentAge :
		pAge->CorrectedAgeValue;
	iCurrent = __xrtHttpCacheTimeAdd(
		iInitial, pAge->ResidentTime
	);
	iSeconds = iCurrent / (uint64)XRT_TIME_SECOND;
	if ( iSeconds > XHTTP_CACHE_DELTA_MAX ) {
		iSeconds = XHTTP_CACHE_DELTA_MAX;
	}
	return
		(pAge->CorrectedAgeValue >= pAge->ResponseDelay) &&
		(pAge->CorrectedInitialAge == iInitial) &&
		(pAge->CurrentAge == iCurrent) &&
		(pAge->CurrentAgeSeconds == iSeconds);
}



/* 验证显式、启发式或扩展新鲜寿命可安全用于策略比较。 */
XRT_API bool xrtHttpCacheFreshnessValid(
	const xhttpcachefreshness* pFreshness
)
{
	if ( pFreshness == NULL ) {
		return false;
	}
	if ( pFreshness->Source == XHTTP_CACHE_FRESHNESS_NONE ) {
		return pFreshness->Lifetime == 0;
	}
	return
		(pFreshness->Source >=
		 XHTTP_CACHE_FRESHNESS_S_MAXAGE) &&
		(pFreshness->Source <=
		 XHTTP_CACHE_FRESHNESS_EXTENSION);
}



/* 初始化空缓存时间元数据。 */
XRT_API void xrtHttpCacheTimeInit(
	xhttpcachetime* pTime
)
{
	if ( pTime == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pTime, 0, sizeof(*pTime));
}



/* 验证公开缓存时间元数据的一致性。 */
XRT_API bool xrtHttpCacheTimeValid(
	const xhttpcachetime* pTime
)
{
	bool bDate;
	bool bAge;
	bool bExpires;

	if ( (pTime == NULL) ||
		((pTime->Flags & ~XRT_HTTP_CACHE_TIME_FLAGS) != 0) ||
		(pTime->Age > XHTTP_CACHE_DELTA_MAX) ||
		(pTime->AgeMemberCount > 0 &&
		 pTime->AgeCount == 0) ) {
		return false;
	}
	bDate = pTime->DateCount != 0;
	bAge = pTime->AgeCount != 0;
	bExpires = pTime->ExpiresCount != 0;
	if ( (((pTime->Flags &
		   XHTTP_CACHE_TIME_DATE) != 0) != bDate) ||
		(((pTime->Flags &
		   XHTTP_CACHE_TIME_AGE) != 0) != bAge) ||
		(((pTime->Flags &
		   XHTTP_CACHE_TIME_EXPIRES) != 0) != bExpires) ||
		(((pTime->Flags &
		   XHTTP_CACHE_TIME_DATE_DUPLICATE) != 0) !=
		 (pTime->DateCount > 1)) ||
		(((pTime->Flags &
		   XHTTP_CACHE_TIME_AGE_EXTRA) != 0) !=
		 (pTime->AgeMemberCount > 1)) ||
		(((pTime->Flags &
		   XHTTP_CACHE_TIME_EXPIRES_DUPLICATE) != 0) !=
		 (pTime->ExpiresCount > 1)) ) {
		return false;
	}
	if ( (!bDate &&
		 ((pTime->Date != 0) ||
		  ((pTime->Flags &
		    XHTTP_CACHE_TIME_DATE_INVALID) != 0))) ||
		(!bAge &&
		 ((pTime->Age != 0) ||
		  (pTime->AgeMemberCount != 0) ||
		  ((pTime->Flags & (
			XHTTP_CACHE_TIME_AGE_EXTRA |
			XHTTP_CACHE_TIME_AGE_INVALID
		   )) != 0))) ||
		(!bExpires &&
		 ((pTime->Expires != 0) ||
		  ((pTime->Flags &
		    XHTTP_CACHE_TIME_EXPIRES_INVALID) != 0))) ) {
		return false;
	}
	if ( bAge &&
		(pTime->AgeMemberCount == 0) &&
		((pTime->Flags &
		  XHTTP_CACHE_TIME_AGE_INVALID) == 0) ) {
		return false;
	}
	if ( ((pTime->Flags &
		  XHTTP_CACHE_TIME_AGE_INVALID) != 0) &&
		(pTime->Age != 0) ) {
		return false;
	}
	return true;
}



/* 扫描响应字段并建立缓存时间元数据。 */
XRT_API bool xrtHttpCacheTimeParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcachetime* pTime
)
{
	xhttpcachetime Time;
	size_t i;

	if ( !__xrtHttpFieldArrayValid(
		pFields, iCount
	) || (pTime == NULL) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pTime, sizeof(*pTime)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Time, 0, sizeof(Time));
	for ( i = 0; i < iCount; i++ ) {
		const xhttpfield* pField = &pFields[i];

		if ( __xrtHttpCacheTimeField(
			pField, XRT_STR_LITERAL("Date")
		) ) {
			if ( !__xrtHttpCacheTimeDateAdd(
				&Time.DateCount,
				&Time.Date,
				&Time.Flags,
				XHTTP_CACHE_TIME_DATE,
				XHTTP_CACHE_TIME_DATE_DUPLICATE,
				XHTTP_CACHE_TIME_DATE_INVALID,
				pField->Value
			) ) {
				return false;
			}
		} else if ( __xrtHttpCacheTimeField(
			pField, XRT_STR_LITERAL("Age")
		) ) {
			if ( !__xrtHttpCacheTimeAgeAdd(
				&Time, pField->Value
			) ) {
				return false;
			}
		} else if ( __xrtHttpCacheTimeField(
			pField, XRT_STR_LITERAL("Expires")
		) ) {
			if ( !__xrtHttpCacheTimeDateAdd(
				&Time.ExpiresCount,
				&Time.Expires,
				&Time.Flags,
				XHTTP_CACHE_TIME_EXPIRES,
				XHTTP_CACHE_TIME_EXPIRES_DUPLICATE,
				XHTTP_CACHE_TIME_EXPIRES_INVALID,
				pField->Value
			) ) {
				return false;
			}
		}
	}
	if ( (Time.AgeCount != 0) &&
		(Time.AgeMemberCount == 0) ) {
		Time.Flags |= XHTTP_CACHE_TIME_AGE_INVALID;
	}
	*pTime = Time;
	return true;
}



/* 按墙钟和单调时钟计算当前年龄。 */
XRT_API xhttpcachecalc xrtHttpCacheCurrentAge(
	const xhttpcachetime* pTime,
	xtime ResponseTime,
	uint64 RequestClock,
	uint64 ResponseClock,
	uint64 NowClock,
	xhttpcacheage* pAge
)
{
	xhttpcacheage Age;
	xtime iDate;
	uint64 iAgeValue;

	if ( !xrtHttpCacheTimeValid(pTime) ||
		(pAge == NULL) ||
		(RequestClock > ResponseClock) ||
		(ResponseClock > NowClock) ||
		__xrtRangesOverlap(
			pAge, sizeof(*pAge),
			pTime, sizeof(*pTime)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_CALC_ERROR;
	}
	if ( (pTime->Flags &
		  XHTTP_CACHE_TIME_DATE_DUPLICATE) != 0 ) {
		return XHTTP_CACHE_CALC_INVALID;
	}
	iDate =
		((pTime->Flags & XHTTP_CACHE_TIME_DATE) != 0) &&
		((pTime->Flags &
		  XHTTP_CACHE_TIME_DATE_INVALID) == 0) ?
		pTime->Date :
		ResponseTime;
	iAgeValue =
		(pTime->Flags &
		 XHTTP_CACHE_TIME_AGE_INVALID) != 0 ?
		0 :
		__xrtHttpCacheTimeSeconds(pTime->Age);
	memset(&Age, 0, sizeof(Age));
	if ( ResponseTime > iDate ) {
		Age.ApparentAge = __xrtHttpCacheTimeDiff(
			iDate, ResponseTime
		);
	}
	Age.ResponseDelay = ResponseClock - RequestClock;
	Age.CorrectedAgeValue = __xrtHttpCacheTimeAdd(
		iAgeValue, Age.ResponseDelay
	);
	Age.CorrectedInitialAge =
		Age.ApparentAge > Age.CorrectedAgeValue ?
		Age.ApparentAge :
		Age.CorrectedAgeValue;
	Age.ResidentTime = NowClock - ResponseClock;
	Age.CurrentAge = __xrtHttpCacheTimeAdd(
		Age.CorrectedInitialAge, Age.ResidentTime
	);
	Age.CurrentAgeSeconds =
		Age.CurrentAge / (uint64)XRT_TIME_SECOND;
	if ( Age.CurrentAgeSeconds >
		XHTTP_CACHE_DELTA_MAX ) {
		Age.CurrentAgeSeconds = XHTTP_CACHE_DELTA_MAX;
	}
	*pAge = Age;
	return XHTTP_CACHE_CALC_READY;
}



/* 计算共享或私有缓存的显式新鲜寿命。 */
XRT_API xhttpcachecalc xrtHttpCacheFreshness(
	const xhttpcachecontrol* pControl,
	const xhttpcachetime* pTime,
	xtime ResponseTime,
	bool Shared,
	xhttpcachefreshness* pFreshness
)
{
	xhttpcachefreshness Freshness;
	xhttpcachedirective Directive = XHTTP_CACHE_UNKNOWN;

	if ( !xrtHttpCacheControlValid(pControl) ||
		!xrtHttpCacheTimeValid(pTime) ||
		(pFreshness == NULL) ||
		__xrtRangesOverlap(
			pFreshness, sizeof(*pFreshness),
			pControl, sizeof(*pControl)
		) ||
		__xrtRangesOverlap(
			pFreshness, sizeof(*pFreshness),
			pTime, sizeof(*pTime)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_CALC_ERROR;
	}
	if ( Shared &&
		((pControl->Flags & XHTTP_CACHE_S_MAXAGE) != 0) ) {
		Directive = XHTTP_CACHE_S_MAXAGE;
		Freshness.Lifetime =
			__xrtHttpCacheTimeSeconds(pControl->SMaxAge);
		Freshness.Source =
			XHTTP_CACHE_FRESHNESS_S_MAXAGE;
	} else if ( (pControl->Flags &
		XHTTP_CACHE_MAX_AGE) != 0 ) {
		Directive = XHTTP_CACHE_MAX_AGE;
		Freshness.Lifetime =
			__xrtHttpCacheTimeSeconds(pControl->MaxAge);
		Freshness.Source =
			XHTTP_CACHE_FRESHNESS_MAX_AGE;
	} else if ( (pTime->Flags &
		XHTTP_CACHE_TIME_EXPIRES) != 0 ) {
		xtime iDate;

		if ( (pTime->Flags & (
			XHTTP_CACHE_TIME_EXPIRES_DUPLICATE |
			XHTTP_CACHE_TIME_DATE_DUPLICATE
		 )) != 0 ) {
			return XHTTP_CACHE_CALC_INVALID;
		}
		Freshness.Source =
			XHTTP_CACHE_FRESHNESS_EXPIRES;
		if ( (pTime->Flags &
			  XHTTP_CACHE_TIME_EXPIRES_INVALID) != 0 ) {
			Freshness.Lifetime = 0;
			*pFreshness = Freshness;
			return XHTTP_CACHE_CALC_READY;
		}
		iDate =
			((pTime->Flags &
			  XHTTP_CACHE_TIME_DATE) != 0) &&
			((pTime->Flags &
			  XHTTP_CACHE_TIME_DATE_INVALID) == 0) ?
			pTime->Date :
			ResponseTime;
		Freshness.Lifetime =
			pTime->Expires > iDate ?
			__xrtHttpCacheTimeDiff(
				iDate, pTime->Expires
			) :
			0;
		*pFreshness = Freshness;
		return XHTTP_CACHE_CALC_READY;
	} else {
		return XHTTP_CACHE_CALC_NONE;
	}
	if ( ((pControl->DuplicateDirectives &
		  (uint32)Directive) != 0) ||
		((pControl->InvalidDirectives &
		  (uint32)Directive) != 0) ) {
		return XHTTP_CACHE_CALC_INVALID;
	}
	*pFreshness = Freshness;
	return XHTTP_CACHE_CALC_READY;
}



/* 比较已计算的显式新鲜寿命和当前年龄。 */
XRT_API bool xrtHttpCacheFresh(
	const xhttpcacheage* pAge,
	const xhttpcachefreshness* pFreshness
)
{
	if ( !xrtHttpCacheAgeValid(pAge) ||
		!xrtHttpCacheFreshnessValid(pFreshness) ||
		(pFreshness->Source ==
		 XHTTP_CACHE_FRESHNESS_NONE) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return pFreshness->Lifetime > pAge->CurrentAge;
}

#endif
