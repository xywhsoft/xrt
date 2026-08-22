#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST)

/* 把单个十六进制字节转换为数值。 */
int __xrtHttpDigestHexValue(uint8 iByte)
{
	if ( (iByte >= (uint8)'0') && (iByte <= (uint8)'9') ) {
		return (int)(iByte - (uint8)'0');
	}
	if ( (iByte >= (uint8)'A') && (iByte <= (uint8)'F') ) {
		return (int)(iByte - (uint8)'A') + 10;
	}
	if ( (iByte >= (uint8)'a') && (iByte <= (uint8)'f') ) {
		return (int)(iByte - (uint8)'a') + 10;
	}
	return -1;
}



/* 验证线路参数中未转义的十六进制值。 */
bool __xrtHttpDigestHexParamValid(
	const xhttpparam* pParam,
	size_t* pSize
)
{
	if ( pParam->Value.Size == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < pParam->Value.Size; i++ ) {
		if ( __xrtHttpDigestHexValue(
			(uint8)pParam->Value.Data[i]
		) < 0 ) {
			return false;
		}
	}
	*pSize = pParam->Value.Size;
	return true;
}



/* 验证借用视图中的十六进制文本。 */
bool __xrtHttpDigestHexViewValid(xstrview Text)
{
	if ( !__xrtHttpViewValid(Text) || (Text.Size == 0) ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( __xrtHttpDigestHexValue((uint8)Text.Data[i]) < 0 ) {
			return false;
		}
	}
	return true;
}



/* 严格读取八位 nonce count。 */
bool __xrtHttpDigestNonceCountRead(
	const xhttpparam* pParam,
	uint32* pCount
)
{
	uint32 iCount = 0;

	if ( ((pParam->Flags & XHTTP_PARAM_QUOTED) != 0) ||
		(pParam->Value.Size != 8u) ) {
		return false;
	}
	for ( size_t i = 0; i < 8u; i++ ) {
		int iValue = __xrtHttpDigestHexValue(
			(uint8)pParam->Value.Data[i]
		);

		if ( iValue < 0 ) {
			return false;
		}
		iCount = (iCount << 4u) | (uint32)iValue;
	}
	*pCount = iCount;
	return true;
}



/* 规范写出八位小写 nonce count。 */
void __xrtHttpDigestNonceCountWrite(
	uint32 iCount,
	char sOutput[8]
)
{
	static const char Hex[] = "0123456789abcdef";

	for ( size_t i = 8u; i != 0; i-- ) {
		sOutput[i - 1u] = Hex[iCount & UINT32_C(0x0F)];
		iCount >>= 4u;
	}
}



/* 以 quoted-string 形式写出规范小写十六进制值。 */
bool __xrtHttpDigestWriterHex(
	xrt_http_param_writer* pWriter,
	xstrview Name,
	xstrview Value,
	bool bFirst
)
{
	if ( !__xrtHttpParamWriterName(pWriter, Name, bFirst) ) {
		return false;
	}
	if ( (Value.Size > (SIZE_MAX - 2u)) ||
		(pWriter->Size > (SIZE_MAX - Value.Size - 2u)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( pWriter->Output != NULL ) {
		pWriter->Output[pWriter->Size] = (uint8)'"';
		for ( size_t i = 0; i < Value.Size; i++ ) {
			pWriter->Output[pWriter->Size + i + 1u] =
				(uint8)__xrtHttpAsciiLower(
					(uint8)Value.Data[i]
				);
		}
		pWriter->Output[pWriter->Size + Value.Size + 1u] =
			(uint8)'"';
	}
	pWriter->Size += Value.Size + 2u;
	return true;
}



/* 解析 Digest 算法名称，不把正常的未知扩展算法当成错误。 */
XRT_API xhttpdigestalgorithm xrtHttpDigestAlgorithmParse(xstrview Name)
{
	if ( xrtHttpTokenEqual(Name, XRT_STR_LITERAL("MD5")) ) {
		return XHTTP_DIGEST_ALGORITHM_MD5;
	}
	if ( xrtHttpTokenEqual(Name, XRT_STR_LITERAL("MD5-sess")) ) {
		return XHTTP_DIGEST_ALGORITHM_MD5_SESSION;
	}
	if ( xrtHttpTokenEqual(Name, XRT_STR_LITERAL("SHA-256")) ) {
		return XHTTP_DIGEST_ALGORITHM_SHA256;
	}
	if ( xrtHttpTokenEqual(Name, XRT_STR_LITERAL("SHA-256-sess")) ) {
		return XHTTP_DIGEST_ALGORITHM_SHA256_SESSION;
	}
	if ( xrtHttpTokenEqual(Name, XRT_STR_LITERAL("SHA-512-256")) ) {
		return XHTTP_DIGEST_ALGORITHM_SHA512_256;
	}
	if ( xrtHttpTokenEqual(
		Name, XRT_STR_LITERAL("SHA-512-256-sess")
	) ) {
		return XHTTP_DIGEST_ALGORITHM_SHA512_256_SESSION;
	}
	return XHTTP_DIGEST_ALGORITHM_UNKNOWN;
}



/* 返回已知算法的规范线路名称。 */
XRT_API xstrview xrtHttpDigestAlgorithmName(
	xhttpdigestalgorithm Algorithm
)
{
	switch ( Algorithm ) {
		case XHTTP_DIGEST_ALGORITHM_MD5:
			return XRT_STR_LITERAL("MD5");
		case XHTTP_DIGEST_ALGORITHM_MD5_SESSION:
			return XRT_STR_LITERAL("MD5-sess");
		case XHTTP_DIGEST_ALGORITHM_SHA256:
			return XRT_STR_LITERAL("SHA-256");
		case XHTTP_DIGEST_ALGORITHM_SHA256_SESSION:
			return XRT_STR_LITERAL("SHA-256-sess");
		case XHTTP_DIGEST_ALGORITHM_SHA512_256:
			return XRT_STR_LITERAL("SHA-512-256");
		case XHTTP_DIGEST_ALGORITHM_SHA512_256_SESSION:
			return XRT_STR_LITERAL("SHA-512-256-sess");
		default:
			return (xstrview){ NULL, 0 };
	}
}



/* 返回 RFC 7616 算法的二进制摘要长度。 */
XRT_API size_t xrtHttpDigestSize(xhttpdigestalgorithm Algorithm)
{
	switch ( Algorithm ) {
		case XHTTP_DIGEST_ALGORITHM_MD5:
		case XHTTP_DIGEST_ALGORITHM_MD5_SESSION:
			return 16u;
		case XHTTP_DIGEST_ALGORITHM_SHA256:
		case XHTTP_DIGEST_ALGORITHM_SHA256_SESSION:
		case XHTTP_DIGEST_ALGORITHM_SHA512_256:
		case XHTTP_DIGEST_ALGORITHM_SHA512_256_SESSION:
			return 32u;
		default:
			return 0;
	}
}



/* 判断算法是否使用 RFC 7616 session A1。 */
XRT_API bool xrtHttpDigestAlgorithmSession(
	xhttpdigestalgorithm Algorithm
)
{
	return (Algorithm == XHTTP_DIGEST_ALGORITHM_MD5_SESSION) ||
		(Algorithm == XHTTP_DIGEST_ALGORITHM_SHA256_SESSION) ||
		(Algorithm == XHTTP_DIGEST_ALGORITHM_SHA512_256_SESSION);
}



/* 按当前裁剪闭包报告可执行的摘要后端。 */
XRT_API bool xrtHttpDigestAlgorithmSupported(
	xhttpdigestalgorithm Algorithm
)
{
	#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SHA2)
	if ( (Algorithm == XHTTP_DIGEST_ALGORITHM_SHA256) ||
		(Algorithm == XHTTP_DIGEST_ALGORITHM_SHA256_SESSION) ||
		(Algorithm == XHTTP_DIGEST_ALGORITHM_SHA512_256) ||
		(Algorithm == XHTTP_DIGEST_ALGORITHM_SHA512_256_SESSION) ) {
		return true;
	}
	#endif

	#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_MD5)
	if ( (Algorithm == XHTTP_DIGEST_ALGORITHM_MD5) ||
		(Algorithm == XHTTP_DIGEST_ALGORITHM_MD5_SESSION) ) {
		return true;
	}
	#endif

	(void)Algorithm;
	return false;
}



/* 解析单个 Digest qop token。 */
XRT_API xhttpdigestqop xrtHttpDigestQopParse(xstrview Name)
{
	if ( xrtHttpTokenEqual(Name, XRT_STR_LITERAL("auth")) ) {
		return XHTTP_DIGEST_QOP_AUTH;
	}
	if ( xrtHttpTokenEqual(Name, XRT_STR_LITERAL("auth-int")) ) {
		return XHTTP_DIGEST_QOP_AUTH_INT;
	}
	return XHTTP_DIGEST_QOP_NONE;
}



/* 返回 qop 的规范线路名称。 */
XRT_API xstrview xrtHttpDigestQopName(xhttpdigestqop Qop)
{
	if ( Qop == XHTTP_DIGEST_QOP_AUTH ) {
		return XRT_STR_LITERAL("auth");
	}
	if ( Qop == XHTTP_DIGEST_QOP_AUTH_INT ) {
		return XRT_STR_LITERAL("auth-int");
	}
	return (xstrview){ NULL, 0 };
}

#endif
