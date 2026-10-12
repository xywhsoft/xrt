#include "../internal/xrt_http.h"

#include <xrt/http_cache_status.h>



#if defined(XHTTP_FEATURE_HTTP_CACHE_STATUS_WRITE)

/* 判断参数 key 是否与已知 Cache-Status 名称相同。 */
static bool __xrtHttpCacheStatusWriteKey(
	xstrview Key,
	const char* sExpected,
	size_t iSize
)
{
	return (Key.Size == iSize) &&
		(memcmp(Key.Data, sExpected, iSize) == 0);
}



/* 验证一个已知参数使用规范要求的 Structured Fields 类型。 */
static bool __xrtHttpCacheStatusWriteParameter(
	const xhttpstructuredparameterentry* pParameter
)
{
	if ( !__xhttpViewValid(pParameter->Key) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( __xrtHttpCacheStatusWriteKey(
		pParameter->Key, "hit", 3u
	) || __xrtHttpCacheStatusWriteKey(
		pParameter->Key, "stored", 6u
	) || __xrtHttpCacheStatusWriteKey(
		pParameter->Key, "collapsed", 9u
	) ) {
		return (pParameter->Value.Type ==
			XHTTP_STRUCTURED_BOOLEAN) &&
			((pParameter->Value.Number == 0) ||
			 (pParameter->Value.Number == 1));
	}
	if ( __xrtHttpCacheStatusWriteKey(
		pParameter->Key, "fwd", 3u
	) ) {
		return pParameter->Value.Type == XHTTP_STRUCTURED_TOKEN;
	}
	if ( __xrtHttpCacheStatusWriteKey(
		pParameter->Key, "fwd-status", 10u
	) ) {
		return (pParameter->Value.Type == XHTTP_STRUCTURED_INTEGER) &&
			(pParameter->Value.Number >= 100) &&
			(pParameter->Value.Number <= 599);
	}
	if ( __xrtHttpCacheStatusWriteKey(
		pParameter->Key, "ttl", 3u
	) ) {
		return pParameter->Value.Type == XHTTP_STRUCTURED_INTEGER;
	}
	if ( __xrtHttpCacheStatusWriteKey(
		pParameter->Key, "key", 3u
	) ) {
		return pParameter->Value.Type == XHTTP_STRUCTURED_STRING;
	}
	if ( __xrtHttpCacheStatusWriteKey(
		pParameter->Key, "detail", 6u
	) ) {
		return (pParameter->Value.Type == XHTTP_STRUCTURED_STRING) ||
			(pParameter->Value.Type == XHTTP_STRUCTURED_TOKEN);
	}
	return true;
}



/* 在通用写出前验证 Cache-Status 已知参数的生产语义。 */
static bool __xrtHttpCacheStatusWriteValid(
	const xhttpstructureditemvalue* pStatus
)
{
	xhttpstructuredparameterentry Parameter;
	size_t iBytes;
	size_t i;

	if ( (pStatus->Bare.Type != XHTTP_STRUCTURED_STRING) &&
		(pStatus->Bare.Type != XHTTP_STRUCTURED_TOKEN) ) {
		return false;
	}
	if ( pStatus->ParameterCount >
		(SIZE_MAX / sizeof(Parameter)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	iBytes = pStatus->ParameterCount * sizeof(Parameter);
	if ( !xrtMemRangeValid(pStatus->Parameters, iBytes) ) {
		return false;
	}
	for ( i = 0; i < pStatus->ParameterCount; i++ ) {
		memcpy(
			&Parameter,
			(const uint8*)pStatus->Parameters +
			(i * sizeof(Parameter)),
			sizeof(Parameter)
		);
		if ( !__xrtHttpCacheStatusWriteParameter(&Parameter) ) {
			return false;
		}
	}
	return true;
}



/* 规范写出一个 Cache-Status 成员。 */
XHTTP_API bool xrtHttpCacheStatusWrite(
	const xhttpstructureditemvalue* pStatus,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpstructureditemvalue Status;

	if ( !xrtMemRangeValid(pStatus, sizeof(Status)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(&Status, pStatus, sizeof(Status));
	if ( !__xrtHttpCacheStatusWriteValid(&Status) ) {
		if ( xrtGetError() == NULL ) {
			xrtSetErrorKind(XERR_VALUE);
		}
		return false;
	}
	return xrtHttpStructuredItemWrite(
		&Status, pOutput, iCapacity, pSize
	);
}

#endif
