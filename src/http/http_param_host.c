#include "../internal/xrt_http.h"



#if defined(XRT_FEATURE_HTTP_PARAM_HOST)

#define XRT_HTTP_IPV6_TEXT_MAX 45u



/* 参数语义读取器跳过 quoted-pair 的反斜线。 */
typedef struct xrt_http_param_host_reader {
	xhttpparam Param;
	size_t Offset;
} xrt_http_param_host_reader;



/* 读取参数解码后的下一个语义字节。 */
static bool __xrtHttpParamHostRead(
	xrt_http_param_host_reader* pReader,
	uint8* pByte
)
{
	return __xrtHttpParamSemanticNext(
		&pReader->Param, &pReader->Offset, pByte
	);
}



/* 验证 IP-literal 或 reg-name 后面的可选十进制端口。 */
static bool __xrtHttpParamHostPort(
	xrt_http_param_host_reader* pReader
)
{
	uint8 iByte;

	while ( __xrtHttpParamHostRead(pReader, &iByte) ) {
		if ( (iByte < (uint8)'0') || (iByte > (uint8)'9') ) {
			return false;
		}
	}
	return true;
}



/* 验证右方括号之后只剩省略端口或十进制端口。 */
static bool __xrtHttpParamHostLiteralTail(
	xrt_http_param_host_reader* pReader
)
{
	uint8 iByte;

	if ( !__xrtHttpParamHostRead(pReader, &iByte) ) {
		return true;
	}
	return (iByte == (uint8)':') &&
		__xrtHttpParamHostPort(pReader);
}



/* 流式验证长度不受限的 IPvFuture 字面地址。 */
static bool __xrtHttpParamHostIpvFuture(
	xrt_http_param_host_reader* pReader
)
{
	bool bVersion = false;
	bool bDot = false;
	bool bAddress = false;
	uint8 iByte;

	while ( __xrtHttpParamHostRead(pReader, &iByte) ) {
		if ( iByte == (uint8)']' ) {
			return bVersion && bDot && bAddress &&
				__xrtHttpParamHostLiteralTail(pReader);
		}
		if ( !bDot ) {
			if ( __xrtHttpAuthorityHex(iByte) >= 0 ) {
				bVersion = true;
				continue;
			}
			if ( (iByte != (uint8)'.') || !bVersion ) {
				return false;
			}
			bDot = true;
			continue;
		}
		if ( !__xrtHttpAuthorityNameByte(iByte) &&
			(iByte != (uint8)':') ) {
			return false;
		}
		bAddress = true;
	}
	return false;
}



/* 收集有协议上限的 IPv6 文本并交给共享 Host 语法核心验证。 */
static bool __xrtHttpParamHostIpv6(
	xrt_http_param_host_reader* pReader,
	uint8 iFirst
)
{
	char sText[XRT_HTTP_IPV6_TEXT_MAX];
	size_t iSize = 0;
	uint8 iByte = iFirst;

	for ( ;; ) {
		if ( iByte == (uint8)']' ) {
			return (iSize != 0) && __xrtHttpAuthorityIpv6(
				(xstrview){ sText, iSize }
			) && __xrtHttpParamHostLiteralTail(pReader);
		}
		if ( iSize == sizeof(sText) ) {
			return false;
		}
		sText[iSize++] = (char)iByte;
		if ( !__xrtHttpParamHostRead(pReader, &iByte) ) {
			return false;
		}
	}
}



/* 验证方括号包围的 IPv6 或 IPvFuture。 */
static bool __xrtHttpParamHostLiteral(
	xrt_http_param_host_reader* pReader
)
{
	uint8 iByte;

	if ( !__xrtHttpParamHostRead(pReader, &iByte) ||
		(iByte == (uint8)']') ) {
		return false;
	}
	if ( (iByte == (uint8)'v') || (iByte == (uint8)'V') ) {
		return __xrtHttpParamHostIpvFuture(pReader);
	}
	return __xrtHttpParamHostIpv6(pReader, iByte);
}



/* 流式验证任意长度 reg-name 和可选端口。 */
static bool __xrtHttpParamHostName(
	xrt_http_param_host_reader* pReader,
	uint8 iByte
)
{
	for ( ;; ) {
		if ( iByte == (uint8)':' ) {
			return __xrtHttpParamHostPort(pReader);
		}
		if ( iByte == (uint8)'%' ) {
			uint8 iHigh;
			uint8 iLow;

			if ( !__xrtHttpParamHostRead(pReader, &iHigh) ||
				!__xrtHttpParamHostRead(pReader, &iLow) ||
				(__xrtHttpAuthorityHex(iHigh) < 0) ||
				(__xrtHttpAuthorityHex(iLow) < 0) ) {
				return false;
			}
		} else if ( !__xrtHttpAuthorityNameByte(iByte) ) {
			return false;
		}
		if ( !__xrtHttpParamHostRead(pReader, &iByte) ) {
			return true;
		}
	}
}



/* 无分配验证参数解码后的语义值是 HTTP Host authority。 */
XRT_API bool xrtHttpParamHostValid(const xhttpparam* pParam)
{
	xrt_http_param_host_reader Reader;
	size_t iSize;
	uint8 iByte;
	bool bValid;

	if ( !xrtHttpParamValueWrite(
		pParam, NULL, 0, &iSize
	) ) {
		return false;
	}
	memcpy(&Reader.Param, pParam, sizeof(Reader.Param));
	Reader.Offset = 0;
	if ( iSize == 0 ) {
		return true;
	}
	if ( !__xrtHttpParamHostRead(&Reader, &iByte) ) {
		__xrtErrorSetValue();
		return false;
	}
	bValid = (iByte == (uint8)'[') ?
		__xrtHttpParamHostLiteral(&Reader) :
		__xrtHttpParamHostName(&Reader, iByte);
	if ( !bValid ) {
		__xrtErrorSetValue();
	}
	return bValid;
}

#endif
