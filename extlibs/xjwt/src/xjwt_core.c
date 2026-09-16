/* Base64URL 编解码 + token 组装/拆解 + 错误设置。 */
#include "xjwt_internal.h"

/* ------------------------------------------------------------------ */
/* Base64URL                                                           */
/* ------------------------------------------------------------------ */

static const char s_B64url_chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

char* xjwt__base64url_encode(const void* pData, size_t iSize)
{
	const unsigned char* p = (const unsigned char*)pData;
	size_t iOutSize = (iSize + 2) / 3 * 4;
	/* 去掉 padding：尾组 1 字节→2 字符，2 字节→3 字符 */
	if ( iSize % 3 == 1 ) iOutSize -= 2;
	else if ( iSize % 3 == 2 ) iOutSize -= 1;
	char* sOut = (char*)xrtMalloc(iOutSize + 1);
	if ( sOut == NULL ) return NULL;
	size_t j = 0;
	for ( size_t i = 0; i < iSize; i += 3 ) {
		uint32_t v = (uint32_t)p[i] << 16;
		if ( i + 1 < iSize ) v |= (uint32_t)p[i+1] << 8;
		if ( i + 2 < iSize ) v |= p[i+2];
		sOut[j++] = s_B64url_chars[(v >> 18) & 63];
		sOut[j++] = s_B64url_chars[(v >> 12) & 63];
		if ( i + 1 < iSize ) sOut[j++] = s_B64url_chars[(v >> 6) & 63];
		if ( i + 2 < iSize ) sOut[j++] = s_B64url_chars[v & 63];
	}
	sOut[j] = 0;
	return sOut;
}

static int b64url_val(char c)
{
	if ( c >= 'A' && c <= 'Z' ) return c - 'A';
	if ( c >= 'a' && c <= 'z' ) return c - 'a' + 26;
	if ( c >= '0' && c <= '9' ) return c - '0' + 52;
	if ( c == '-' ) return 62;
	if ( c == '_' ) return 63;
	return -1;
}

size_t xjwt__base64url_decode_size(const char* sText, size_t iTextSize)
{
	(void)sText;
	if ( iTextSize == 0 ) return 0;
	size_t rem = iTextSize % 4;
	if ( rem == 1 ) return 0;  /* 非法 */
	size_t full = iTextSize / 4 * 3;
	if ( rem == 2 ) return full + 1;
	if ( rem == 3 ) return full + 2;
	return full;
}

unsigned char* xjwt__base64url_decode(const char* sText, size_t iTextSize, size_t* pOutSize)
{
	size_t iOutSize = xjwt__base64url_decode_size(sText, iTextSize);
	if ( iOutSize == 0 && iTextSize > 0 ) {
		xjwt__error(XJWT_ERROR_MALFORMED, "base64url decode: invalid length");
		return NULL;
	}
	unsigned char* pOut = (unsigned char*)xrtMalloc(iOutSize > 0 ? iOutSize : 1);
	if ( pOut == NULL ) return NULL;
	size_t j = 0;
	for ( size_t i = 0; i < iTextSize; i += 4 ) {
		uint32_t v = 0;
		int n = 0;
		for ( int k = 0; k < 4 && i + k < iTextSize; k++ ) {
			int d = b64url_val(sText[i+k]);
			if ( d < 0 ) {
				xrtFree(pOut);
				xjwt__error(XJWT_ERROR_MALFORMED, "base64url decode: invalid character");
				return NULL;
			}
			v = (v << 6) | (uint32_t)d;
			n++;
		}
		v <<= (4 - n) * 6;
		if ( n >= 2 ) pOut[j++] = (unsigned char)(v >> 16);
		if ( n >= 3 ) pOut[j++] = (unsigned char)(v >> 8);
		if ( n >= 4 ) pOut[j++] = (unsigned char)v;
	}
	*pOutSize = j;
	return pOut;
}

/* ------------------------------------------------------------------ */
/* token 拆解 / 组装                                                    */
/* ------------------------------------------------------------------ */

bool xjwt__split(const char* sToken,
                 const char** pHead, size_t* pHeadSize,
                 const char** pClaims, size_t* pClaimsSize,
                 const char** pSig, size_t* pSigSize)
{
	if ( sToken == NULL ) return false;
	const char* p1 = strchr(sToken, '.');
	if ( p1 == NULL ) return false;
	const char* p2 = strchr(p1 + 1, '.');
	if ( p2 == NULL ) return false;
	/* RFC 7515：JWS 精确三段——signature 段内不允许再出现 '.' */
	if ( strchr(p2 + 1, '.') != NULL ) return false;
	*pHead = sToken;
	*pHeadSize = (size_t)(p1 - sToken);
	*pClaims = p1 + 1;
	*pClaimsSize = (size_t)(p2 - p1 - 1);
	*pSig = p2 + 1;
	*pSigSize = strlen(p2 + 1);
	return *pHeadSize > 0 && *pClaimsSize > 0;
}

char* xjwt__join(const char* sHeadJson, const char* sClaimsJson,
                 const void* pSig, size_t iSigSize)
{
	char* sHeadB64 = xjwt__base64url_encode(sHeadJson, strlen(sHeadJson));
	if ( sHeadB64 == NULL ) return NULL;
	char* sClaimsB64 = xjwt__base64url_encode(sClaimsJson, strlen(sClaimsJson));
	if ( sClaimsB64 == NULL ) { xrtFree(sHeadB64); return NULL; }
	char* sSigB64 = xjwt__base64url_encode(pSig, iSigSize);
	if ( sSigB64 == NULL ) { xrtFree(sHeadB64); xrtFree(sClaimsB64); return NULL; }

	size_t n = strlen(sHeadB64) + 1 + strlen(sClaimsB64) + 1 + strlen(sSigB64) + 1;
	char* sOut = (char*)xrtMalloc(n);
	if ( sOut != NULL ) {
		strcpy(sOut, sHeadB64);
		strcat(sOut, ".");
		strcat(sOut, sClaimsB64);
		strcat(sOut, ".");
		strcat(sOut, sSigB64);
	}
	xrtFree(sHeadB64); xrtFree(sClaimsB64); xrtFree(sSigB64);
	return sOut;
}

/* ------------------------------------------------------------------ */
/* 错误                                                                */
/* ------------------------------------------------------------------ */

void xjwt__error(int iCode, const char* sMessage)
{
	xrtSetErrorInfo(XERR_STATE, "xrt.jwt", iCode, sMessage);
}
