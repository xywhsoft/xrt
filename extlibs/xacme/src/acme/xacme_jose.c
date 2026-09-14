#include "../internal/xacme_jose.h"

#if defined(XACME_FEATURE_ACME_JOSE)

#include <xrt/buffer.h>
#include <xrt/codec.h>
#include <xrt/error.h>
#include <xrt/random.h>

#include <string.h>

/* JOSE 全部编码统一 base64url 无填充。 */
static const xbase64config __xacmeB64Url = {
	NULL,
	XBASE64_URL | XBASE64_NO_PADDING
};

/* 把借用文本按 JSON 字符串 token（含两侧引号）转义追加；控制字符用 \u。 */
static bool xacmeJsonQuoteAppend(xbuffer* pBuffer, xstrview sText)
{
	size_t i;
	if(!xrtBufferAppendByte(pBuffer, '"'))
	{
		return false;
	}
	for(i = 0; i < sText.Size; i++)
	{
		char c = sText.Data[i];
		bool bEscaped = false;
		char sEscape[6];
		switch(c)
		{
		case '"':
			bEscaped = xrtBufferAppend(
				pBuffer, XRT_BYTES_LITERAL("\\\""));
			break;
		case '\\':
			bEscaped = xrtBufferAppend(
				pBuffer, XRT_BYTES_LITERAL("\\\\"));
			break;
		default:
			if((unsigned char)c < 0x20u)
			{
				sEscape[0] = '\\';
				sEscape[1] = 'u';
				sEscape[2] = '0';
				sEscape[3] = '0';
				sEscape[4] = "0123456789ABCDEF"
					[((unsigned char)c >> 4u) & 0x0Fu];
				sEscape[5] = "0123456789ABCDEF"
					[(unsigned char)c & 0x0Fu];
				bEscaped = xrtBufferAppend(
					pBuffer,
					(xbytesview){ (const uint8*)sEscape, 6u }
				);
			}
			else
			{
				bEscaped = (xrtBufferAppendByte(pBuffer, (uint8)c));
			}
			break;
		}
		if(!bEscaped)
		{
			return false;
		}
	}
	return xrtBufferAppendByte(pBuffer, '"');
}

/* 32 字节标量 → 43 字符 base64url 文本 + 末尾零，写入 44 字节输出。 */
static bool xacmeScalarText(const uint8* pScalar, char* sOut)
{
	size_t iSize = 0;
	if(!xrtBase64Encode(
		pScalar, 32u, sOut, 44u, &iSize, &__xacmeB64Url) || iSize != 43u)
	{
		xrtSetErrorInfo(
			XERR_INTERNAL,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_INTERNAL,
			"acme jose scalar base64url size mismatch"
		);
		return false;
	}
	sOut[43] = '\0';
	return true;
}

bool xacmeEs256Generate(xacmees256key* pKey)
{
	if(pKey == NULL)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_ARGUMENT, "acme jose key required");
		return false;
	}
	if(!xrtP256KeyPair(pKey->Private, pKey->Public))
	{
		return false;
	}
	return true;
}

bool xacmeEs256FromPrivate(xacmees256key* pKey)
{
	if(pKey == NULL)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_ARGUMENT, "acme jose key required");
		return false;
	}
	return xrtP256Public(pKey->Private, pKey->Public);
}

str xacmeJwkEcJson(const xacmees256key* pKey)
{
	char sX[44];
	char sY[44];
	str sJson;
	size_t iSize;
	if(pKey == NULL || pKey->Public[0] != 0x04u)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_ARGUMENT,
			"acme jose jwk requires key with uncompressed public point"
		);
		return NULL;
	}
	if(!xacmeScalarText(&pKey->Public[1], sX) ||
		!xacmeScalarText(&pKey->Public[33], sY))
	{
		return NULL;
	}
	/* {"crv":"P-256","kty":"EC","x":"","y":""} 固定形状 = 46 + 86。 */
	iSize = 46u + 43u + 43u;
	sJson = (str)xrtMalloc(iSize + 1u);
	if(sJson == NULL)
	{
		return NULL;
	}
	memcpy(sJson, "{\"crv\":\"P-256\",\"kty\":\"EC\",\"x\":\"", 31u);
	memcpy(sJson + 31u, sX, 43u);
	memcpy(sJson + 74u, "\",\"y\":\"", 7u);
	memcpy(sJson + 81u, sY, 43u);
	memcpy(sJson + 124u, "\"}", 2u);
	sJson[126] = '\0';
	return sJson;
}

bool xacmeJwkThumbprint(xstrview sCanonicalJson, char* sOut)
{
	uint8 Digest[XRT_SHA256_SIZE];
	size_t iSize = 0;
	if(sCanonicalJson.Data == NULL || sCanonicalJson.Size == 0 || sOut == NULL)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_ARGUMENT,
			"acme jose thumbprint requires json and output"
		);
		return false;
	}
	if(!xrtSha256(sCanonicalJson.Data, sCanonicalJson.Size, Digest))
	{
		return false;
	}
	if(!xrtBase64Encode(
		Digest, sizeof(Digest), sOut, 44u, &iSize, &__xacmeB64Url) ||
		iSize != 43u)
	{
		return false;
	}
	sOut[43] = '\0';
	return true;
}

bool xacmeJwkEcThumbprint(const xacmees256key* pKey, char* sOut)
{
	str sJson = xacmeJwkEcJson(pKey);
	bool bOk;
	if(sJson == NULL)
	{
		return false;
	}
	bOk = xacmeJwkThumbprint(
		(xstrview){ sJson, 126u }, sOut);
	xrtFree(sJson);
	return bOk;
}

str xacmeJwsEs256(
	const xacmees256key* pKey, const xacmejwsheader* pHeader,
	xstrview sPayload)
{
	xbuffer Header;
	xbuffer Token;
	str sResult = NULL;
	str sHeaderB64;
	str sPayloadB64;
	str sSignatureB64;
	uint8 Digest[XRT_SHA256_SIZE];
	uint8 Signature[XRT_ECDSA_P256_SIGNATURE_SIZE];
	size_t iSize = 0;
	bool bOk;

	if(pKey == NULL || pHeader == NULL || pHeader->Url.Data == NULL ||
		pHeader->Url.Size == 0 || sPayload.Data == NULL)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_ARGUMENT,
			"acme jose jws requires key, header url and payload"
		);
		return NULL;
	}

	xrtBufferInit(&Header);
	xrtBufferInit(&Token);
	sHeaderB64 = NULL;
	sPayloadB64 = NULL;
	sSignatureB64 = NULL;

	bOk = xrtBufferAppend(&Header, XRT_BYTES_LITERAL("{\"alg\":\"ES256\""));
	if(bOk && (pHeader->Nonce.Data != NULL) && (pHeader->Nonce.Size > 0u))
	{
		bOk = xrtBufferAppend(&Header, XRT_BYTES_LITERAL(",\"nonce\":"))
			&& xacmeJsonQuoteAppend(&Header, pHeader->Nonce);
	}
	if(bOk)
	{
		bOk = xrtBufferAppend(&Header, XRT_BYTES_LITERAL(",\"url\":"))
			&& xacmeJsonQuoteAppend(&Header, pHeader->Url);
	}
	if(bOk && (pHeader->Kid.Data != NULL) && (pHeader->Kid.Size > 0u))
	{
		bOk = xrtBufferAppend(&Header, XRT_BYTES_LITERAL(",\"kid\":"))
			&& xacmeJsonQuoteAppend(&Header, pHeader->Kid);
	}
	else if(bOk)
	{
		str sJwk = xacmeJwkEcJson(pKey);
		if(sJwk == NULL)
		{
			bOk = false;
		}
		else
		{
			bOk = xrtBufferAppend(&Header, XRT_BYTES_LITERAL(",\"jwk\":"))
				&& xrtBufferAppend(
					&Header, (xbytesview){ (const uint8*)sJwk, 126u });
			xrtFree(sJwk);
		}
	}
	if(bOk)
	{
		bOk = xrtBufferAppend(&Header, XRT_BYTES_LITERAL("}"));
	}
	if(!bOk)
	{
		goto Done;
	}

	sHeaderB64 = xrtBase64EncodeNew(
		Header.Data == NULL ? "" : (cstr)Header.Data,
		xrtBufferView(&Header).Size,
		&__xacmeB64Url);
	sPayloadB64 = xrtBase64EncodeNew(
		sPayload.Data, sPayload.Size, &__xacmeB64Url);
	if((sHeaderB64 == NULL) || (sPayloadB64 == NULL))
	{
		goto Done;
	}

	/* 签名输入是 ASCII 的 header.payload 拼接。 */
	bOk = xrtBufferAppend(
		&Token, (xbytesview){ (const uint8*)sHeaderB64, strlen(sHeaderB64) })
		&& xrtBufferAppendByte(&Token, (uint8)'.')
		&& xrtBufferAppend(
			&Token,
			(xbytesview){ (const uint8*)sPayloadB64, strlen(sPayloadB64) });
	if(!bOk || !xrtSha256(Token.Data, xrtBufferView(&Token).Size, Digest))
	{
		goto Done;
	}
	if(!xrtEcdsaP256Sign(
		XCRYPTO_HASH_SHA256, Digest, pKey->Private, Signature))
	{
		goto Done;
	}
	sSignatureB64 = xrtBase64EncodeNew(
		Signature, sizeof(Signature), &__xacmeB64Url);
	if(sSignatureB64 == NULL)
	{
		goto Done;
	}

	/* ACME 使用 JWS 扁平 JSON 序列化（RFC 8555 §6.2）。 */
	{
		xbuffer Flat;
		bool bFlat;
		xrtBufferInit(&Flat);
		bFlat = xrtBufferAppend(&Flat, XRT_BYTES_LITERAL("{\"protected\":\""))
			&& xrtBufferAppend(
				&Flat,
				(xbytesview){
					(const uint8*)sHeaderB64, strlen(sHeaderB64) })
			&& xrtBufferAppend(&Flat, XRT_BYTES_LITERAL("\",\"payload\":\""))
			&& xrtBufferAppend(
				&Flat,
				(xbytesview){
					(const uint8*)sPayloadB64, strlen(sPayloadB64) })
			&& xrtBufferAppend(&Flat, XRT_BYTES_LITERAL("\",\"signature\":\""))
			&& xrtBufferAppend(
				&Flat,
				(xbytesview){
					(const uint8*)sSignatureB64, strlen(sSignatureB64) })
			&& xrtBufferAppend(&Flat, XRT_BYTES_LITERAL("\"}"))
			&& xrtBufferAppendByte(&Flat, 0u);
		if(bFlat && (Flat.Data != NULL))
		{
			iSize = Flat.Size - 1u;
			sResult = (str)xrtMalloc(iSize + 1u);
			if(sResult != NULL)
			{
				memcpy(sResult, Flat.Data, iSize + 1u);
			}
		}
		xrtBufferUnit(&Flat);
	}

Done:
	xrtFree(sHeaderB64);
	xrtFree(sPayloadB64);
	xrtFree(sSignatureB64);
	xrtBufferUnit(&Header);
	xrtBufferUnit(&Token);
	if(sResult == NULL)
	{
		xrtSetErrorInfo(
			XERR_MEMORY,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_INTERNAL,
			"acme jose jws assembly failed"
		);
	}
	return sResult;
}

#endif
