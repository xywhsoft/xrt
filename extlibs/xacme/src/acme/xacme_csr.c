#include "../internal/xacme_csr.h"

#if defined(XACME_FEATURE_ACME_CSR)

#include <xrt/asn1.h>
#include <xrt/buffer.h>
#include <xrt/crypto.h>
#include <xrt/error.h>
#include <xrt/memory.h>
#include <xrt/pem.h>

#include <string.h>

/*
	DER 组装纪律：裸内容只积累在一个缓冲；包装永远是把 TLV
	追加进另一个缓冲（目标为空或已有兄弟 TLV）。禁止把缓冲
	自身的视图再包装回自身——xrtDerAppend 是追加而非替换。
*/

/* 把 AlgIdentifier 内容（两个 OID）包成 SEQ 追加到 pOut。 */
static bool xacmeEcAlgId(xbuffer* pOut)
{
	xbuffer Raw;
	bool bOk;
	xrtBufferInit(&Raw);
	bOk = xrtDerAppendOid(&Raw, XRT_STR_LITERAL("1.2.840.10045.2.1"))
		&& xrtDerAppendOid(&Raw, XRT_STR_LITERAL("1.2.840.10045.3.1.7"))
		&& xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Raw));
	xrtBufferUnit(&Raw);
	return bOk;
}

/* ---------- CSR（PKCS#10，RFC 2986） ---------- */

/* subject: SEQ{ SET{ SEQ{ OID 2.5.4.3, UTF8String } } } */
static bool xacmeCsrSubject(xbuffer* pOut, xstrview sCommonName)
{
	xbuffer Ava;
	xbuffer RdnSeq;
	xbuffer RdnSet;
	bool bOk;
	if((sCommonName.Data == NULL) || (sCommonName.Size == 0u))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.csr",
			XACME_CSR_ERROR_ARGUMENT,
			"acme csr requires common name"
		);
		return false;
	}
	xrtBufferInit(&Ava);
	xrtBufferInit(&RdnSeq);
	xrtBufferInit(&RdnSet);
	bOk = xrtDerAppendOid(&Ava, XRT_STR_LITERAL("2.5.4.3"))
		&& xrtDerAppend(
			&Ava, XASN1_UNIVERSAL, XASN1_UTF8_STRING, false,
			(xbytesview){
				(const uint8*)sCommonName.Data, sCommonName.Size })
		&& xrtDerAppend(
			&RdnSeq, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Ava))
		&& xrtDerAppend(
			&RdnSet, XASN1_UNIVERSAL, XASN1_SET, true,
			xrtBufferView(&RdnSeq))
		&& xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&RdnSet));
	xrtBufferUnit(&Ava);
	xrtBufferUnit(&RdnSeq);
	xrtBufferUnit(&RdnSet);
	return bOk;
}

/* SubjectPublicKeyInfo：SEQ{ AlgId, BIT STRING 点 } */
static bool xacmeCsrSpki(xbuffer* pOut, const xacmees256key* pKey)
{
	xbuffer Spki;
	bool bOk;
	xrtBufferInit(&Spki);
	bOk = xacmeEcAlgId(&Spki)
		&& xrtDerAppendBitString(
			&Spki,
			(xbytesview){ pKey->Public, XRT_P256_PUBLIC_SIZE },
			0u)
		&& xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Spki));
	xrtBufferUnit(&Spki);
	return bOk;
}

/* attributes [0] IMPLICIT：extensionRequest 携带 subjectAltName。 */
static bool xacmeCsrAttributes(xbuffer* pOut, const xacmecsrconfig* pConfig)
{
	xbuffer Names;
	xbuffer ExtValue;
	xbuffer Ext;
	xbuffer ExtSeq;
	xbuffer ExtList;
	xbuffer ExtListSeq;
	xbuffer AttrValues;
	xbuffer Attr;
	xbuffer AttrSeq;
	bool bOk = true;
	size_t i;

	if((pConfig->Domains == NULL) || (pConfig->DomainCount == 0u))
	{
		return true; /* 无 SAN：整个 attributes 字段省略。 */
	}
	xrtBufferInit(&Names);
	xrtBufferInit(&ExtValue);
	xrtBufferInit(&Ext);
	xrtBufferInit(&ExtSeq);
	xrtBufferInit(&ExtList);
	xrtBufferInit(&ExtListSeq);
	xrtBufferInit(&AttrValues);
	xrtBufferInit(&Attr);
	xrtBufferInit(&AttrSeq);

	for(i = 0; bOk && (i < pConfig->DomainCount); i++)
	{
		if((pConfig->Domains[i].Data == NULL) ||
			(pConfig->Domains[i].Size == 0u))
		{
			xrtSetErrorInfo(
				XERR_ARGUMENT,
				"xrt.acme.csr",
				XACME_CSR_ERROR_ARGUMENT,
				"acme csr domain entry is empty"
			);
			bOk = false;
			break;
		}
		bOk = xrtDerAppend(
			&Names, XASN1_CONTEXT, 2u, false,
			(xbytesview){
				(const uint8*)pConfig->Domains[i].Data,
				pConfig->Domains[i].Size });
	}
		/* GeneralNames SEQ */
		bOk = bOk
			&& xrtDerAppend(
				&ExtValue, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&Names))
			/* Extension 裸内容 = OID san + OCTET STRING{GeneralNames} */
			&& xrtDerAppendOid(&Ext, XRT_STR_LITERAL("2.5.29.17"))
			&& xrtDerAppend(
				&Ext, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false,
				xrtBufferView(&ExtValue))
			/* Extension SEQ */
			&& xrtDerAppend(
				&ExtSeq, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&Ext))
			/* Extensions SEQUENCE OF Extension（RFC 2986 语义，
			   Go 严格解析要求此层）。 */
			&& xrtBufferAppend(&ExtList, xrtBufferView(&ExtSeq))
			&& xrtDerAppend(
				&ExtListSeq, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&ExtList))
			/* values SET 携带 Extensions SEQUENCE。 */
			&& xrtDerAppend(
				&AttrValues, XASN1_UNIVERSAL, XASN1_SET, true,
				xrtBufferView(&ExtListSeq))
			/* Attribute 裸内容 = OID extReq + values SET TLV。 */
			&& xrtDerAppendOid(&Attr, XRT_STR_LITERAL("1.2.840.113549.1.9.14"))
			&& xrtBufferAppend(&Attr, xrtBufferView(&AttrValues))
			/* [0] IMPLICIT SET OF Attribute = [0]{ Attribute SEQ }。 */
			&& xrtDerAppend(
				&AttrSeq, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&Attr))
			&& xrtDerAppend(
				pOut, XASN1_CONTEXT, 0u, true, xrtBufferView(&AttrSeq));

	xrtBufferUnit(&Names);
	xrtBufferUnit(&ExtValue);
	xrtBufferUnit(&Ext);
	xrtBufferUnit(&ExtSeq);
	xrtBufferUnit(&ExtList);
	xrtBufferUnit(&ExtListSeq);
	xrtBufferUnit(&AttrValues);
	xrtBufferUnit(&Attr);
	xrtBufferUnit(&AttrSeq);
	return bOk;
}

bool xacmeCsrEc(
	const xacmees256key* pKey, const xacmecsrconfig* pConfig, xbuffer* pOut)
{
	xbuffer Cri;
	xbuffer CriTlv;
	xbuffer SigAlgRaw;
	xbuffer SigAlg;
	xbuffer Outer;
	uint8 Digest[XRT_SHA256_SIZE];
	uint8 Signature[XRT_ECDSA_P256_DER_MAX_SIZE];
	size_t iSignatureSize = 0;
	bool bOk;

	if((pKey == NULL) || (pConfig == NULL) || (pOut == NULL) ||
		(pKey->Public[0] != 0x04u))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.csr",
			XACME_CSR_ERROR_ARGUMENT,
			"acme csr requires key, config and output"
		);
		return false;
	}

	xrtBufferInit(&Cri);
	xrtBufferInit(&CriTlv);
	xrtBufferInit(&SigAlgRaw);
	xrtBufferInit(&SigAlg);
	xrtBufferInit(&Outer);
	bOk = xrtDerAppendUInt64(&Cri, 0u) /* version INTEGER 0 */
		&& xacmeCsrSubject(&Cri, pConfig->CommonName)
		&& xacmeCsrSpki(&Cri, pKey)
		&& xacmeCsrAttributes(&Cri, pConfig)
		/* 签名对象是含 SEQ 头的完整 CRI DER（RFC 2986）。 */
		&& xrtDerAppend(
			&CriTlv, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Cri))
		&& xrtSha256(CriTlv.Data, CriTlv.Size, Digest)
		&& xrtEcdsaP256SignDer(
			XCRYPTO_HASH_SHA256, Digest, pKey->Private, Signature,
			sizeof(Signature), &iSignatureSize)
		&& xrtDerAppendOid(
			&SigAlgRaw, XRT_STR_LITERAL("1.2.840.10045.4.3.2"))
		&& xrtDerAppend(
			&SigAlg, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&SigAlgRaw))
		/* CertificationRequest = SEQ{ CRI, sigAlg, BIT STRING sig } */
		&& xrtBufferAppend(&Outer, xrtBufferView(&CriTlv))
		&& xrtBufferAppend(&Outer, xrtBufferView(&SigAlg))
		&& xrtDerAppendBitString(
			&Outer,
			(xbytesview){ Signature, iSignatureSize },
			0u)
		&& xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Outer));

	xrtBufferUnit(&Cri);
	xrtBufferUnit(&CriTlv);
	xrtBufferUnit(&SigAlgRaw);
	xrtBufferUnit(&SigAlg);
	xrtBufferUnit(&Outer);
	if(!bOk)
	{
		xrtSetErrorInfo(
			XERR_INTERNAL,
			"xrt.acme.csr",
			XACME_CSR_ERROR_INTERNAL,
			"acme csr assembly failed"
		);
	}
	return bOk;
}

/* ---------- 证书密钥抽象（ES256 / RSA） ---------- */

/* 大端无符号整数 → DER INTEGER（剥前导零，高位置位补 0x00）。 */
static bool xacmeDerAppendBigUint(
	xbuffer* pOut, const uint8* pData, size_t iSize)
{
	xbuffer Content;
	bool bOk = true;
	while((iSize > 1u) && (pData[0] == 0u))
	{
		pData++;
		iSize--;
	}
	xrtBufferInit(&Content);
	if(iSize == 0u)
	{
		bOk = xrtBufferAppendByte(&Content, 0u);
	}
	else
	{
		if((pData[0] & 0x80u) != 0u)
		{
			bOk = xrtBufferAppendByte(&Content, 0u);
		}
		if(bOk)
		{
			bOk = xrtBufferAppend(
				&Content, (xbytesview){ pData, iSize });
		}
	}
	if(bOk)
	{
		bOk = xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_INTEGER, false,
			xrtBufferView(&Content));
	}
	xrtBufferUnit(&Content);
	return bOk;
}

/* DER INTEGER（正数）剥符号前导零后拷入定长缓冲。 */
static bool xacmeDerIntToBytes(
	const xdervalue* pValue, uint8* pOut, size_t iCap, size_t* pOutSize)
{
	const uint8* pData = pValue->Value.Data;
	size_t iSize = pValue->Value.Size;
	if((pData == NULL) || (iSize == 0u))
	{
		return false;
	}
	if((pData[0] & 0x80u) != 0u)
	{
		return false; /* 负数：密钥分量不可能。 */
	}
	while((iSize > 1u) && (pData[0] == 0u))
	{
		pData++;
		iSize--;
	}
	if(iSize > iCap)
	{
		return false;
	}
	memcpy(pOut, pData, iSize);
	*pOutSize = iSize;
	return true;
}

/* PKCS#1（RSAPrivateKey）：n/e/d 必填，CRT 五参可选成组。 */
static bool xacmeRsaParsePkcs1(xdercursor* pCursor, xacmersakey* pKey)
{
	xdervalue Value;
	uint8 Version[8];
	size_t iVersionSize = 0u;
	struct
	{
		uint8* pData;
		size_t* pSize;
		size_t iCap;
		bool bGot;
	} Tails[5];
	size_t i;
	Tails[0].pData = pKey->Prime1;
	Tails[0].pSize = &pKey->Prime1Size;
	Tails[0].iCap = sizeof(pKey->Prime1);
	Tails[0].bGot = false;
	Tails[1].pData = pKey->Prime2;
	Tails[1].pSize = &pKey->Prime2Size;
	Tails[1].iCap = sizeof(pKey->Prime2);
	Tails[1].bGot = false;
	Tails[2].pData = pKey->Exponent1;
	Tails[2].pSize = &pKey->Exponent1Size;
	Tails[2].iCap = sizeof(pKey->Exponent1);
	Tails[2].bGot = false;
	Tails[3].pData = pKey->Exponent2;
	Tails[3].pSize = &pKey->Exponent2Size;
	Tails[3].iCap = sizeof(pKey->Exponent2);
	Tails[3].bGot = false;
	Tails[4].pData = pKey->Coefficient;
	Tails[4].pSize = &pKey->CoefficientSize;
	Tails[4].iCap = sizeof(pKey->Coefficient);
	Tails[4].bGot = false;
	if(!xrtDerExpect(
			pCursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Value) ||
		!xacmeDerIntToBytes(
			&Value, Version, sizeof(Version), &iVersionSize) ||
		!xrtDerExpect(
			pCursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Value) ||
		!xacmeDerIntToBytes(
			&Value, pKey->Modulus, sizeof(pKey->Modulus),
			&pKey->ModulusSize) ||
		!xrtDerExpect(
			pCursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Value) ||
		!xacmeDerIntToBytes(
			&Value, pKey->Exponent, sizeof(pKey->Exponent),
			&pKey->ExponentSize) ||
		!xrtDerExpect(
			pCursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Value) ||
		!xacmeDerIntToBytes(
			&Value, pKey->PrivateExponent, sizeof(pKey->PrivateExponent),
			&pKey->PrivateExponentSize))
	{
		return false;
	}
	for(i = 0; i < 5u; i++)
	{
		if(xrtDerRead(pCursor, &Value) != XDER_VALUE)
		{
			break;
		}
		if(!xrtDerIs(
				&Value, XASN1_UNIVERSAL, XASN1_INTEGER, false))
		{
			break; /* 允许尾部出现其他可选元素。 */
		}
		if(!xacmeDerIntToBytes(
				&Value, Tails[i].pData, Tails[i].iCap, Tails[i].pSize))
		{
			return false;
		}
		Tails[i].bGot = true;
	}
	for(i = 0; i < 5u; i++)
	{
		if(!Tails[i].bGot)
		{
			*Tails[i].pSize = 0u;
		}
	}
	return (pKey->ModulusSize != 0u) && (pKey->ExponentSize != 0u) &&
		(pKey->PrivateExponentSize != 0u);
}

/* RSA PKCS#1 写出（CRT 完整时带五参数）。 */
static bool xacmeRsaWritePkcs1(const xacmersakey* pKey, xbuffer* pOut)
{
	xbuffer Body;
	bool bOk;
	xrtBufferInit(&Body);
	bOk = xrtDerAppendUInt64(&Body, 0u) &&
		xacmeDerAppendBigUint(
			&Body, pKey->Modulus, pKey->ModulusSize) &&
		xacmeDerAppendBigUint(
			&Body, pKey->Exponent, pKey->ExponentSize) &&
		xacmeDerAppendBigUint(
			&Body, pKey->PrivateExponent, pKey->PrivateExponentSize);
	if(bOk && (pKey->Prime1Size != 0u) && (pKey->Prime2Size != 0u) &&
		(pKey->Exponent1Size != 0u) && (pKey->Exponent2Size != 0u) &&
		(pKey->CoefficientSize != 0u))
	{
		bOk = xacmeDerAppendBigUint(
				&Body, pKey->Prime1, pKey->Prime1Size) &&
			xacmeDerAppendBigUint(
				&Body, pKey->Prime2, pKey->Prime2Size) &&
			xacmeDerAppendBigUint(
				&Body, pKey->Exponent1, pKey->Exponent1Size) &&
			xacmeDerAppendBigUint(
				&Body, pKey->Exponent2, pKey->Exponent2Size) &&
			xacmeDerAppendBigUint(
				&Body, pKey->Coefficient, pKey->CoefficientSize);
	}
	if(bOk)
	{
		bOk = xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Body));
	}
	xrtBufferUnit(&Body);
	return bOk;
}

/* rsaEncryption AlgIdentifier（NULL 参数）。 */
static bool xacmeRsaAlgId(xbuffer* pOut)
{
	xbuffer Raw;
	bool bOk;
	xrtBufferInit(&Raw);
	bOk = xrtDerAppendOid(&Raw, XRT_STR_LITERAL("1.2.840.113549.1.1.1")) &&
		xrtDerAppendNull(&Raw) &&
		xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Raw));
	xrtBufferUnit(&Raw);
	return bOk;
}

/* RSA SPKI：SEQ{ rsaEncryption, BIT STRING{ SEQ{ n, e } } }。 */
static bool xacmeCsrSpkiRsa(xbuffer* pOut, const xacmersakey* pKey)
{
	xbuffer Alg;
	xbuffer NeRaw;
	xbuffer Ne;
	xbuffer Spki;
	bool bOk;
	xrtBufferInit(&Alg);
	xrtBufferInit(&NeRaw);
	xrtBufferInit(&Ne);
	xrtBufferInit(&Spki);
	bOk = xacmeRsaAlgId(&Alg) &&
		xacmeDerAppendBigUint(
			&NeRaw, pKey->Modulus, pKey->ModulusSize) &&
		xacmeDerAppendBigUint(
			&NeRaw, pKey->Exponent, pKey->ExponentSize) &&
		xrtDerAppend(
			&Ne, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&NeRaw)) &&
		xrtBufferAppend(&Spki, xrtBufferView(&Alg)) &&
		xrtDerAppendBitString(
			&Spki, xrtBufferView(&Ne), 0u) &&
		xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Spki));
	xrtBufferUnit(&Alg);
	xrtBufferUnit(&NeRaw);
	xrtBufferUnit(&Ne);
	xrtBufferUnit(&Spki);
	return bOk;
}

/* RSA 私钥视图（借用 xacmersakey 定长缓冲）。 */
static void xacmeRsaView(
	const xacmersakey* pKey, xrsaprivatekey* pView)
{
	memset(pView, 0, sizeof(*pView));
	pView->Public.Modulus = pKey->Modulus;
	pView->Public.ModulusSize = pKey->ModulusSize;
	pView->Public.Exponent = pKey->Exponent;
	pView->Public.ExponentSize = pKey->ExponentSize;
	pView->PrivateExponent = pKey->PrivateExponent;
	pView->PrivateExponentSize = pKey->PrivateExponentSize;
	if((pKey->Prime1Size != 0u) && (pKey->Prime2Size != 0u) &&
		(pKey->Exponent1Size != 0u) && (pKey->Exponent2Size != 0u) &&
		(pKey->CoefficientSize != 0u))
	{
		pView->Prime1 = pKey->Prime1;
		pView->Prime1Size = pKey->Prime1Size;
		pView->Prime2 = pKey->Prime2;
		pView->Prime2Size = pKey->Prime2Size;
		pView->Exponent1 = pKey->Exponent1;
		pView->Exponent1Size = pKey->Exponent1Size;
		pView->Exponent2 = pKey->Exponent2;
		pView->Exponent2Size = pKey->Exponent2Size;
		pView->Coefficient = pKey->Coefficient;
		pView->CoefficientSize = pKey->CoefficientSize;
	}
}

void xacmeCertKeyUnit(xacmecertkey* pKey)
{
	if(pKey == NULL)
	{
		return;
	}
	xrtSecureZero(pKey, sizeof(*pKey));
	pKey->Kind = XACME_CERT_KEY_ES256;
}

bool xacmeCertKeyReadPem(cstr sPem, size_t iSize, xacmecertkey* pKey)
{
	xpemblock Block;
	bool bHavePkcs8;
	bool bHavePkcs1Rsa;

	if((sPem == NULL) || (iSize == 0u) || (pKey == NULL))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.csr", XACME_CSR_ERROR_ARGUMENT,
			"acme cert key read requires pem and key");
		return false;
	}
	xacmeCertKeyUnit(pKey);
	bHavePkcs8 = xrtPemFind(sPem, iSize, "PRIVATE KEY", &Block);
	bHavePkcs1Rsa = xrtPemFind(sPem, iSize, "RSA PRIVATE KEY", &Block);

	/* EC：PKCS#8（非 RSA）或 SEC1。 */
	if(bHavePkcs8 || xrtPemFind(sPem, iSize, "EC PRIVATE KEY", &Block))
	{
		xrtClearError();
		if(xacmeKeyPemRead(sPem, iSize, &pKey->Ec))
		{
			pKey->Kind = XACME_CERT_KEY_ES256;
			return true;
		}
		xrtClearError();
	}

	if(bHavePkcs1Rsa)
	{
		size_t iDerSize = 0u;
		bytes pDer = xrtPemDecodeNew(&Block, &iDerSize);
		xdercursor Cursor;
		xdervalue Value;
		bool bOk;
		if(pDer == NULL)
		{
			return false;
		}
		bOk = xrtDerInit(&Cursor, pDer, iDerSize) &&
			xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, &Value) &&
			xrtDerEnter(&Value, &Cursor) &&
			xacmeRsaParsePkcs1(&Cursor, &pKey->Rsa);
		xrtFree(pDer);
		if(bOk)
		{
			pKey->Kind = XACME_CERT_KEY_RSA;
			return true;
		}
		xacmeCertKeyUnit(pKey);
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.csr", XACME_CSR_ERROR_ARGUMENT,
			"acme cert key rsa pkcs1 invalid");
		return false;
	}

	if(bHavePkcs8)
	{
		size_t iDerSize = 0u;
		bytes pDer = xrtPemDecodeNew(&Block, &iDerSize);
		xdercursor Cursor;
		xdervalue Value;
		xdervalue Algorithm;
		xdercursor AlgCursor;
		xdervalue Oid;
		xdercursor Inner;
		xdervalue InnerSeq;
		bool bIsRsa = false;
		bool bOk;
		if(pDer == NULL)
		{
			return false;
		}
		bOk = xrtDerInit(&Cursor, pDer, iDerSize) &&
			xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, &Value) &&
			xrtDerEnter(&Value, &Cursor) &&
			xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Value) &&
			xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				&Algorithm) &&
			xrtDerEnter(&Algorithm, &AlgCursor) &&
			(xrtDerRead(&AlgCursor, &Oid) == XDER_VALUE) &&
			xrtDerIs(&Oid, XASN1_UNIVERSAL, XASN1_OBJECT_IDENTIFIER, false);
		if(bOk)
		{
			/* rsaEncryption OID 的 DER 内容固定 9 字节。 */
			static const uint8 uRsaOid[9] = {
				0x2a, 0x86, 0x48, 0x86, 0xf7,
				0x0d, 0x01, 0x01, 0x01 };
			bIsRsa = (Oid.Value.Size == sizeof(uRsaOid)) &&
				(memcmp(Oid.Value.Data, uRsaOid, sizeof(uRsaOid)) == 0);
		}
		if(bOk && bIsRsa &&
			xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false,
				&Value))
		{
			bOk = xrtDerInit(
					&Inner, Value.Value.Data, Value.Value.Size) &&
				xrtDerExpect(
					&Inner, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
					&InnerSeq) &&
				xrtDerEnter(&InnerSeq, &Inner) &&
				xacmeRsaParsePkcs1(&Inner, &pKey->Rsa);
			xrtFree(pDer);
			if(bOk)
			{
				pKey->Kind = XACME_CERT_KEY_RSA;
				return true;
			}
			xacmeCertKeyUnit(pKey);
			xrtSetErrorInfo(
				XERR_ARGUMENT, "xrt.acme.csr",
				XACME_CSR_ERROR_ARGUMENT,
				"acme cert key rsa pkcs8 invalid");
			return false;
		}
		xrtFree(pDer);
	}

	xacmeCertKeyUnit(pKey);
	xrtSetErrorInfo(
		XERR_NOT_FOUND, "xrt.acme.csr", XACME_CSR_ERROR_ARGUMENT,
		"acme cert key pem unrecognized");
	return false;
}

str xacmeCertKeyPemWrite(const xacmecertkey* pKey)
{
	if((pKey == NULL) ||
		((pKey->Kind == XACME_CERT_KEY_ES256) &&
			(pKey->Ec.Public[0] != 0x04u)) ||
		((pKey->Kind == XACME_CERT_KEY_RSA) &&
			((pKey->Rsa.ModulusSize == 0u) ||
				(pKey->Rsa.PrivateExponentSize == 0u))))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.csr", XACME_CSR_ERROR_ARGUMENT,
			"acme cert key write requires valid key");
		return NULL;
	}
	if(pKey->Kind == XACME_CERT_KEY_ES256)
	{
		return xacmeKeyPemWrite(&pKey->Ec);
	}
	{
		xbuffer Alg;
		xbuffer Pkcs1;
		xbuffer Body;
		xbuffer Pkcs8;
		str sPem = NULL;
		bool bOk;
		xrtBufferInit(&Alg);
		xrtBufferInit(&Pkcs1);
		xrtBufferInit(&Body);
		xrtBufferInit(&Pkcs8);
		bOk = xacmeRsaAlgId(&Alg) &&
			xacmeRsaWritePkcs1(&pKey->Rsa, &Pkcs1) &&
			xrtDerAppendUInt64(&Body, 0u) &&
			xrtBufferAppend(&Body, xrtBufferView(&Alg)) &&
			xrtDerAppend(
				&Body, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false,
				xrtBufferView(&Pkcs1)) &&
			xrtDerAppend(
				&Pkcs8, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&Body));
		if(bOk && (Pkcs8.Data != NULL))
		{
			sPem = xrtPemEncodeNew(
				"PRIVATE KEY", Pkcs8.Data, Pkcs8.Size);
		}
		xrtBufferUnit(&Alg);
		xrtBufferUnit(&Pkcs1);
		xrtBufferUnit(&Body);
		xrtBufferUnit(&Pkcs8);
		return sPem;
	}
}

/* 通用 PKCS#10 组装：骨架共享，SPKI/签名算法/签名按密钥算法分派。 */
bool xacmeCsrBuild(
	const xacmecertkey* pKey, const xacmecsrconfig* pConfig, xbuffer* pOut)
{
	xbuffer Cri;
	xbuffer CriTlv;
	xbuffer SigAlgRaw;
	xbuffer SigAlg;
	xbuffer Outer;
	uint8 Digest[XRT_SHA256_SIZE];
	uint8 Signature[XRT_RSA_MAX_MODULUS_SIZE];
	size_t iSignatureSize = 0u;
	bool bOk;

	if((pKey == NULL) || (pConfig == NULL) || (pOut == NULL) ||
		(pConfig->CommonName.Data == NULL) ||
		(pConfig->CommonName.Size == 0u))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.csr", XACME_CSR_ERROR_ARGUMENT,
			"acme csr build requires key, config and output");
		return false;
	}
	xrtBufferInit(&Cri);
	xrtBufferInit(&CriTlv);
	xrtBufferInit(&SigAlgRaw);
	xrtBufferInit(&SigAlg);
	xrtBufferInit(&Outer);
	bOk = xrtDerAppendUInt64(&Cri, 0u) &&
		xacmeCsrSubject(&Cri, pConfig->CommonName) &&
		((pKey->Kind == XACME_CERT_KEY_ES256) ?
			xacmeCsrSpki(&Cri, &pKey->Ec) :
			xacmeCsrSpkiRsa(&Cri, &pKey->Rsa)) &&
		xacmeCsrAttributes(&Cri, pConfig) &&
		xrtDerAppend(
			&CriTlv, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Cri)) &&
		xrtSha256(CriTlv.Data, xrtBufferView(&CriTlv).Size, Digest);
	if(bOk && (pKey->Kind == XACME_CERT_KEY_ES256))
	{
		bOk = xrtEcdsaP256SignDer(
			XCRYPTO_HASH_SHA256, Digest, pKey->Ec.Private, Signature,
			sizeof(Signature), &iSignatureSize);
	}
	else if(bOk)
	{
		xrsaprivatekey View;
		xacmeRsaView(&pKey->Rsa, &View);
		iSignatureSize = pKey->Rsa.ModulusSize;
		bOk = (iSignatureSize <= sizeof(Signature)) &&
			xrtRsaPkcs1Sign(
				&View, XCRYPTO_HASH_SHA256, Digest, Signature);
	}
	if(bOk)
	{
		bOk = xrtDerAppendOid(
				&SigAlgRaw,
				(pKey->Kind == XACME_CERT_KEY_ES256) ?
					XRT_STR_LITERAL("1.2.840.10045.4.3.2") :
					XRT_STR_LITERAL("1.2.840.113549.1.1.11"));
		if(bOk && (pKey->Kind == XACME_CERT_KEY_RSA))
		{
			bOk = xrtDerAppendNull(&SigAlgRaw);
		}
		bOk = bOk &&
			xrtDerAppend(
				&SigAlg, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&SigAlgRaw)) &&
			xrtBufferAppend(&Outer, xrtBufferView(&CriTlv)) &&
			xrtBufferAppend(&Outer, xrtBufferView(&SigAlg)) &&
			xrtDerAppendBitString(
				&Outer, (xbytesview){ Signature, iSignatureSize }, 0u) &&
			xrtDerAppend(
				pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&Outer));
	}
	xrtBufferUnit(&Cri);
	xrtBufferUnit(&CriTlv);
	xrtBufferUnit(&SigAlgRaw);
	xrtBufferUnit(&SigAlg);
	xrtBufferUnit(&Outer);
	if(!bOk)
	{
		xrtSetErrorInfo(
			XERR_INTERNAL, "xrt.acme.csr", XACME_CSR_ERROR_INTERNAL,
			"acme csr build assembly failed");
	}
	return bOk;
}

/* ---------- 私钥 PEM 序列化 ---------- */
/* ---------- 私钥 PEM 序列化 ---------- */

/* SEC1：SEQ{ INT 1, OCTET d, [0]{曲线 OID}, [1]{BIT STRING pub} } */
static bool xacmeSec1Der(const xacmees256key* pKey, xbuffer* pOut)
{
	xbuffer Curve;
	xbuffer Pub;
	xbuffer Sec1;
	bool bOk;
	xrtBufferInit(&Curve);
	xrtBufferInit(&Pub);
	xrtBufferInit(&Sec1);
	bOk = xrtDerAppendOid(&Curve, XRT_STR_LITERAL("1.2.840.10045.3.1.7"))
		&& xrtDerAppendUInt64(&Sec1, 1u)
		&& xrtDerAppend(
			&Sec1, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false,
			(xbytesview){ pKey->Private, XRT_P256_PRIVATE_SIZE })
		&& xrtDerAppend(
			&Sec1, XASN1_CONTEXT, 0u, true, xrtBufferView(&Curve))
		&& xrtDerAppendBitString(
			&Pub,
			(xbytesview){ pKey->Public, XRT_P256_PUBLIC_SIZE },
			0u)
		&& xrtDerAppend(
			&Sec1, XASN1_CONTEXT, 1u, true, xrtBufferView(&Pub))
		&& xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Sec1));
	xrtBufferUnit(&Curve);
	xrtBufferUnit(&Pub);
	xrtBufferUnit(&Sec1);
	return bOk;
}

str xacmeKeyPemWrite(const xacmees256key* pKey)
{
	xbuffer AlgId;
	xbuffer Sec1;
	xbuffer Body;
	xbuffer Pkcs8;
	str sPem = NULL;
	bool bOk;

	if((pKey == NULL) || (pKey->Public[0] != 0x04u))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.csr",
			XACME_CSR_ERROR_ARGUMENT,
			"acme key pem write requires key with public point"
		);
		return NULL;
	}
	xrtBufferInit(&AlgId);
	xrtBufferInit(&Sec1);
	xrtBufferInit(&Body);
	xrtBufferInit(&Pkcs8);
	bOk = xrtDerAppendUInt64(&Body, 0u) /* PKCS#8 version 0 */
		&& xacmeEcAlgId(&AlgId)
		&& xrtBufferAppend(&Body, xrtBufferView(&AlgId))
		&& xacmeSec1Der(pKey, &Sec1)
		&& xrtDerAppend(
			&Body, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false,
			xrtBufferView(&Sec1))
		&& xrtDerAppend(
			&Pkcs8, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Body));
	if(bOk)
	{
		sPem = xrtPemEncodeNew("PRIVATE KEY", Pkcs8.Data, Pkcs8.Size);
	}
	xrtBufferUnit(&AlgId);
	xrtBufferUnit(&Sec1);
	xrtBufferUnit(&Body);
	xrtBufferUnit(&Pkcs8);
	return sPem;
}

/* 解析 SEC1 内容（已进入最外层 SEQ）：INT ver, OCTET d, [0]?, [1]? */
static bool xacmeSec1Parse(xdercursor* pCursor, xacmees256key* pKey)
{
	xdervalue Version;
	xdervalue Private;
	xdervalue Field;
	bool bHasPublic = false;

	if(!xrtDerExpect(
		pCursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Version) ||
		!xrtDerExpect(
			pCursor, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false, &Private))
	{
		return false;
	}
	if(Private.Value.Size != XRT_P256_PRIVATE_SIZE)
	{
		return false;
	}
	memcpy(pKey->Private, Private.Value.Data, XRT_P256_PRIVATE_SIZE);
	while(xrtDerRead(pCursor, &Field) == XDER_VALUE)
	{
		if((Field.Tag.Class == XASN1_CONTEXT) &&
			(Field.Tag.Number == 1u))
		{
			xdercursor Pub;
			xdervalue Point;
			xbytesview Key;
			uint8 iUnusedBits = 0;
			if(!xrtDerEnter(&Field, &Pub) ||
				(xrtDerRead(&Pub, &Point) != XDER_VALUE) ||
				!xrtDerIs(
					&Point, XASN1_UNIVERSAL, XASN1_BIT_STRING, false) ||
				!xrtDerBitString(&Point, &Key, &iUnusedBits) ||
				(iUnusedBits != 0u) ||
				(Key.Size != XRT_P256_PUBLIC_SIZE))
			{
				return false;
			}
			memcpy(pKey->Public, Key.Data, XRT_P256_PUBLIC_SIZE);
			bHasPublic = true;
		}
	}
	if(!bHasPublic)
	{
		return xacmeEs256FromPrivate(pKey);
	}
	return true;
}

bool xacmeKeyPemRead(cstr sPem, size_t iSize, xacmees256key* pKey)
{
	xpemblock Block;

	if((sPem == NULL) || (iSize == 0u) || (pKey == NULL))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.csr",
			XACME_CSR_ERROR_ARGUMENT,
			"acme key pem read requires pem and key"
		);
		return false;
	}

	/* PKCS#8 "PRIVATE KEY"。 */
	if(xrtPemFind(sPem, iSize, "PRIVATE KEY", &Block))
	{
		size_t iDerSize = 0;
		bytes pDer;
		xdercursor Cursor;
		xdervalue Value;
		bool bOk;
		if(!xrtPemDecode(&Block, NULL, 0u, &iDerSize))
		{
			return false;
		}
		pDer = xrtPemDecodeNew(&Block, &iDerSize);
		if(pDer == NULL)
		{
			return false;
		}
		bOk = xrtDerInit(&Cursor, pDer, iDerSize)
			&& xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, &Value)
			&& xrtDerEnter(&Value, &Cursor)
			&& xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Value)
			&& xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, &Value)
			&& xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false,
				&Value)
			/* privateKey 是 primitive OCTET STRING，内容即完整 SEC1。 */
			&& xrtDerInit(
				&Cursor, Value.Value.Data, Value.Value.Size)
			&& xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, &Value)
			&& xrtDerEnter(&Value, &Cursor)
			&& xacmeSec1Parse(&Cursor, pKey);
		xrtFree(pDer);
		return bOk;
	}

	/* 传统 SEC1 "EC PRIVATE KEY"。 */
	if(xrtPemFind(sPem, iSize, "EC PRIVATE KEY", &Block))
	{
		size_t iDerSize = 0;
		bytes pDer;
		xdercursor Cursor;
		xdervalue Value;
		bool bOk;
		if(!xrtPemDecode(&Block, NULL, 0u, &iDerSize))
		{
			return false;
		}
		pDer = xrtPemDecodeNew(&Block, &iDerSize);
		if(pDer == NULL)
		{
			return false;
		}
		bOk = xrtDerInit(&Cursor, pDer, iDerSize)
			&& xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, &Value)
			&& xrtDerEnter(&Value, &Cursor)
			&& xacmeSec1Parse(&Cursor, pKey);
		xrtFree(pDer);
		return bOk;
	}

	xrtSetErrorInfo(
		XERR_NOT_FOUND,
		"xrt.acme.csr",
		XACME_CSR_ERROR_ARGUMENT,
		"acme key pem read found no ec private key block"
	);
	return false;
}

#endif
