#ifndef XACME_SIGV4_H
#define XACME_SIGV4_H

/*
	AWS SigV4 家族签名公共件（TC3 / AWS4 / SDK-HMAC-SHA256 共用骨架）：
	三方差异只在 StringToSign 前缀与派生密钥链长度，canonical request
	形状一致（method\nuri\nquery\nheaders\nsignedheaders\npayloadhash）。
	全部 static 实现，仅内部使用。
*/

#include <xrt/core.h>
#include <xrt/crypto.h>

#include <stdio.h>
#include <string.h>

#define XACME_SIG_HASH_TEXT 65u

#if defined(__GNUC__)
__attribute__((unused))
#endif
static void xacmeSigHex(const uint8* pData, size_t iSize, char* sOut)
{
	size_t i;
	for(i = 0; i < iSize; i++)
	{
		sprintf(sOut + i * 2u, "%02x", pData[i]);
	}
	sOut[iSize * 2u] = '\0';
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeSigSha256Hex(
	const void* pData, size_t iSize, char* sOut)
{
	uint8 Digest[XRT_SHA256_SIZE];
	if(!xrtSha256(pData, iSize, Digest))
	{
		return false;
	}
	xacmeSigHex(Digest, sizeof(Digest), sOut);
	return true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeSigHmac(
	const uint8* pKey, size_t iKeySize, const void* pData, size_t iSize,
	uint8 pOut[XRT_SHA256_SIZE])
{
	return xrtHmacSha256(pKey, iKeySize, pData, iSize, pOut);
}

/*
	组装 canonical request。canonHeaders 形如
	"content-type:v\nhost:h\n"（键小写、按字典序、值裁剪首尾空白），
	sSignedHeaders 形如 "content-type;host"。
*/
#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeSigCanonical(
	char* sOut, size_t iCapacity, cstr sMethod, cstr sUri, cstr sQuery,
	cstr sCanonHeaders, cstr sSignedHeaders, cstr sPayloadHashHex)
{
	int iWritten = snprintf(
		sOut, iCapacity, "%s\n%s\n%s\n%s\n%s\n%s", sMethod, sUri,
		((sQuery != NULL) ? sQuery : ""), sCanonHeaders, sSignedHeaders,
		sPayloadHashHex);
	return (iWritten > 0) && ((size_t)iWritten < iCapacity);
}

#endif
