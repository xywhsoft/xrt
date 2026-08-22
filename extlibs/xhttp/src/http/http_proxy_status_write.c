#include "../internal/xrt_http.h"

#include <xrt/http_proxy_status.h>



#if defined(XHTTP_FEATURE_HTTP_PROXY_STATUS_WRITE)

/* 判断参数 key 是否与已知 Proxy-Status 名称相同。 */
static bool __xrtHttpProxyStatusWriteKey(
	xstrview Key,
	const char* sExpected,
	size_t iSize
)
{
	return (Key.Size == iSize) &&
		(memcmp(Key.Data, sExpected, iSize) == 0);
}



/* 验证待写出的 ALPN 标识使用规范要求的最短 Structured 类型。 */
static bool __xrtHttpProxyStatusWriteProtocol(
	const xhttpstructuredvalue* pProtocol
)
{
	if ( !__xrtHttpViewValid(pProtocol->Data) ||
		(pProtocol->Data.Size == 0) ||
		(pProtocol->Data.Size > XHTTP_PROXY_ALPN_MAX) ) {
		return false;
	}
	if ( pProtocol->Type == XHTTP_STRUCTURED_TOKEN ) {
		return xrtHttpStructuredTokenValid(pProtocol->Data);
	}
	if ( pProtocol->Type != XHTTP_STRUCTURED_BYTES ) {
		return false;
	}
	return !xrtHttpStructuredTokenValid(pProtocol->Data);
}



/* 验证一个已知参数使用规范要求的 Structured Fields 类型。 */
static bool __xrtHttpProxyStatusWriteParameter(
	const xhttpstructuredparameterentry* pParameter
)
{
	if ( !__xrtHttpViewValid(pParameter->Key) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtHttpProxyStatusWriteKey(
		pParameter->Key, "error", 5u
	) ) {
		return pParameter->Value.Type == XHTTP_STRUCTURED_TOKEN;
	}
	if ( __xrtHttpProxyStatusWriteKey(
		pParameter->Key, "next-hop", 8u
	) ) {
		return (pParameter->Value.Type == XHTTP_STRUCTURED_STRING) ||
			(pParameter->Value.Type == XHTTP_STRUCTURED_TOKEN);
	}
	if ( __xrtHttpProxyStatusWriteKey(
		pParameter->Key, "next-protocol", 13u
	) ) {
		return __xrtHttpProxyStatusWriteProtocol(
			&pParameter->Value
		);
	}
	if ( __xrtHttpProxyStatusWriteKey(
		pParameter->Key, "received-status", 15u
	) ) {
		return (pParameter->Value.Type == XHTTP_STRUCTURED_INTEGER) &&
			(pParameter->Value.Number >= 100) &&
			(pParameter->Value.Number <= 599);
	}
	if ( __xrtHttpProxyStatusWriteKey(
		pParameter->Key, "details", 7u
	) || __xrtHttpProxyStatusWriteKey(
		pParameter->Key, "next-hop-aliases", 16u
	) ) {
		return pParameter->Value.Type == XHTTP_STRUCTURED_STRING;
	}
	return true;
}



/* 在通用写出前验证 Proxy-Status 已知参数的生产语义。 */
static bool __xrtHttpProxyStatusWriteValid(
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
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iBytes = pStatus->ParameterCount * sizeof(Parameter);
	if ( !__xrtRangeValid(pStatus->Parameters, iBytes) ) {
		return false;
	}
	for ( i = 0; i < pStatus->ParameterCount; i++ ) {
		memcpy(
			&Parameter,
			(const uint8*)pStatus->Parameters +
			(i * sizeof(Parameter)),
			sizeof(Parameter)
		);
		if ( !__xrtHttpProxyStatusWriteParameter(&Parameter) ) {
			return false;
		}
	}
	return true;
}



/* 规范写出一个 Proxy-Status 成员。 */
XRT_API bool xrtHttpProxyStatusWrite(
	const xhttpstructureditemvalue* pStatus,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpstructureditemvalue Status;

	if ( !__xrtRangeValid(pStatus, sizeof(Status)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Status, pStatus, sizeof(Status));
	if ( !__xrtHttpProxyStatusWriteValid(&Status) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetValue();
		}
		return false;
	}
	return xrtHttpStructuredItemWrite(
		&Status, pOutput, iCapacity, pSize
	);
}

#endif
