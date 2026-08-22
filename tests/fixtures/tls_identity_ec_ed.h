#ifndef XRT_TEST_TLS_IDENTITY_EC_ED_H
#define XRT_TEST_TLS_IDENTITY_EC_ED_H

#include "x509_vectors.h"



#if defined(TEST_TLS_IDENTITY_FIXTURE_EC)

/* 写入测试夹具使用的规范 DER 标签和长度。 */
static size_t testTlsIdentityDerHeader(
	uint8* pOutput,
	uint8 iTag,
	size_t iSize
)
{
	pOutput[0] = iTag;
	if ( iSize < 128u ) {
		pOutput[1] = (uint8)iSize;
		return 2u;
	}
	if ( iSize <= UINT8_MAX ) {
		pOutput[1] = 0x81;
		pOutput[2] = (uint8)iSize;
		return 3u;
	}
	pOutput[1] = 0x82;
	pOutput[2] = (uint8)(iSize >> 8u);
	pOutput[3] = (uint8)iSize;
	return 4u;
}



/* 以现有严格证书向量为骨架，结构化替换 P-256 或 P-384 SPKI。 */
static bool testTlsIdentityEcCertificate(
	const uint8* pPublic,
	size_t iPublicSize,
	uint8* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	static const uint8 P256Algorithm[] = {
		0x30, 0x13,
		0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
		0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07
	};
	static const uint8 P384Algorithm[] = {
		0x30, 0x10,
		0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
		0x06, 0x05, 0x2B, 0x81, 0x04, 0x00, 0x22
	};
	const uint8* pAlgorithm;
	size_t iAlgorithmSize;
	xx509cert Certificate;
	xdercursor Cursor;
	xdervalue Tbs;
	uint8 Spki[128];
	size_t iSpkiBody;
	size_t iSpkiSize;
	size_t iSpkiPrefix;
	size_t iSpkiSuffix;
	size_t iTbsBody;
	size_t iTbsHeader;
	size_t iTbsSize;
	size_t iOuterSuffix;
	size_t iOuterBody;
	size_t iOuterHeader;
	size_t iRequired;
	size_t iOffset;

	if ( iPublicSize == 65u ) {
		pAlgorithm = P256Algorithm;
		iAlgorithmSize = sizeof(P256Algorithm);
	} else if ( iPublicSize == 97u ) {
		pAlgorithm = P384Algorithm;
		iAlgorithmSize = sizeof(P384Algorithm);
	} else {
		return false;
	}
	if ( !xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Certificate
	) || !xrtDerInit(
		&Cursor, Certificate.Tbs.Data, Certificate.Tbs.Size
	) || (xrtDerRead(&Cursor, &Tbs) != XDER_VALUE) ||
		!xrtDerDone(&Cursor) ||
		(Certificate.SubjectPublicKeyInfo.Data < Tbs.Value.Data) ||
		((Certificate.SubjectPublicKeyInfo.Data +
		  Certificate.SubjectPublicKeyInfo.Size) >
		 (Tbs.Value.Data + Tbs.Value.Size)) ||
		(Certificate.Tbs.Data !=
		 (X509_VALID_ED25519 + 4u)) ) {
		return false;
	}
	iSpkiBody = iAlgorithmSize + 3u + iPublicSize;
	iSpkiSize = testTlsIdentityDerHeader(Spki, 0x30, iSpkiBody);
	memcpy(Spki + iSpkiSize, pAlgorithm, iAlgorithmSize);
	iSpkiSize += iAlgorithmSize;
	iSpkiSize += testTlsIdentityDerHeader(
		Spki + iSpkiSize, 0x03, iPublicSize + 1u
	);
	Spki[iSpkiSize++] = 0;
	memcpy(Spki + iSpkiSize, pPublic, iPublicSize);
	iSpkiSize += iPublicSize;
	iSpkiPrefix = (size_t)(
		Certificate.SubjectPublicKeyInfo.Data - Tbs.Value.Data
	);
	iSpkiSuffix = Tbs.Value.Size - iSpkiPrefix -
		Certificate.SubjectPublicKeyInfo.Size;
	iTbsBody = iSpkiPrefix + iSpkiSize + iSpkiSuffix;
	iTbsHeader = iTbsBody < 128u ? 2u : (iTbsBody <= UINT8_MAX ? 3u : 4u);
	iTbsSize = iTbsHeader + iTbsBody;
	iOuterSuffix = sizeof(X509_VALID_ED25519) - 4u - Certificate.Tbs.Size;
	iOuterBody = iTbsSize + iOuterSuffix;
	iOuterHeader = iOuterBody < 128u ? 2u :
		(iOuterBody <= UINT8_MAX ? 3u : 4u);
	iRequired = iOuterHeader + iOuterBody;
	if ( (pOutput == NULL) || (pSize == NULL) ||
		(iRequired > iCapacity) ) {
		return false;
	}
	iOffset = testTlsIdentityDerHeader(pOutput, 0x30, iOuterBody);
	iOffset += testTlsIdentityDerHeader(
		pOutput + iOffset, 0x30, iTbsBody
	);
	memcpy(pOutput + iOffset, Tbs.Value.Data, iSpkiPrefix);
	iOffset += iSpkiPrefix;
	memcpy(pOutput + iOffset, Spki, iSpkiSize);
	iOffset += iSpkiSize;
	memcpy(
		pOutput + iOffset,
		Certificate.SubjectPublicKeyInfo.Data +
			Certificate.SubjectPublicKeyInfo.Size,
		iSpkiSuffix
	);
	iOffset += iSpkiSuffix;
	memcpy(
		pOutput + iOffset,
		Certificate.Tbs.Data + Certificate.Tbs.Size,
		iOuterSuffix
	);
	iOffset += iOuterSuffix;
	*pSize = iOffset;
	return iOffset == iRequired;
}

#endif



#if defined(TEST_TLS_IDENTITY_FIXTURE_ED25519)

/* 复制 Ed25519 证书骨架并替换固定长度公钥。 */
static bool testTlsIdentityEdCertificate(
	const uint8* pPublic,
	uint8* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xx509cert Certificate;
	xx509pubkey PublicKey;
	size_t iOffset;

	if ( (pPublic == NULL) || (pOutput == NULL) || (pSize == NULL) ||
		(iCapacity < sizeof(X509_VALID_ED25519)) ) {
		return false;
	}
	memcpy(pOutput, X509_VALID_ED25519, sizeof(X509_VALID_ED25519));
	if ( !xrtX509Parse(
		pOutput, sizeof(X509_VALID_ED25519), &Certificate
	) || !xrtX509PublicKey(&Certificate, &PublicKey) ||
		(PublicKey.Type != X509_KEY_ED25519) ||
		(PublicKey.Key.Size != XRT_ED25519_PUBLIC_SIZE) ) {
		return false;
	}
	iOffset = (size_t)(PublicKey.Key.Data - pOutput);
	memcpy(pOutput + iOffset, pPublic, XRT_ED25519_PUBLIC_SIZE);
	*pSize = sizeof(X509_VALID_ED25519);
	return true;
}

#endif



#if defined(TEST_TLS_IDENTITY_FIXTURE_EC)

/* 构造带命名曲线和可选公钥的规范 RFC 5915 ECPrivateKey。 */
static bool testTlsIdentityEcSec1(
	const uint8* pPrivate,
	size_t iPrivateSize,
	const uint8* pPublic,
	uint8* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	static const uint8 P256Oid[] = {
		0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07
	};
	static const uint8 P384Oid[] = {
		0x06, 0x05, 0x2B, 0x81, 0x04, 0x00, 0x22
	};
	const uint8* pOid;
	size_t iOidSize;
	size_t iPublicSize;
	size_t iBody;
	size_t iRequired;
	size_t iOffset;

	if ( iPrivateSize == 32u ) {
		pOid = P256Oid;
		iOidSize = sizeof(P256Oid);
		iPublicSize = 65u;
	} else if ( iPrivateSize == 48u ) {
		pOid = P384Oid;
		iOidSize = sizeof(P384Oid);
		iPublicSize = 97u;
	} else {
		return false;
	}
	iBody = 3u + 2u + iPrivateSize + 2u + iOidSize +
		2u + 2u + 1u + iPublicSize;
	iRequired = (iBody < 128u ? 2u : 3u) + iBody;
	if ( (pPrivate == NULL) || (pPublic == NULL) ||
		(pOutput == NULL) || (pSize == NULL) ||
		(iRequired > iCapacity) ) {
		return false;
	}
	iOffset = testTlsIdentityDerHeader(pOutput, 0x30, iBody);
	pOutput[iOffset++] = 0x02;
	pOutput[iOffset++] = 0x01;
	pOutput[iOffset++] = 0x01;
	iOffset += testTlsIdentityDerHeader(
		pOutput + iOffset, 0x04, iPrivateSize
	);
	memcpy(pOutput + iOffset, pPrivate, iPrivateSize);
	iOffset += iPrivateSize;
	iOffset += testTlsIdentityDerHeader(
		pOutput + iOffset, 0xA0, iOidSize
	);
	memcpy(pOutput + iOffset, pOid, iOidSize);
	iOffset += iOidSize;
	{
		size_t iBitSize = 2u + 1u + iPublicSize;

		iOffset += testTlsIdentityDerHeader(
			pOutput + iOffset, 0xA1, iBitSize
		);
		iOffset += testTlsIdentityDerHeader(
			pOutput + iOffset, 0x03, iPublicSize + 1u
		);
	}
	pOutput[iOffset++] = 0;
	memcpy(pOutput + iOffset, pPublic, iPublicSize);
	iOffset += iPublicSize;
	*pSize = iOffset;
	return iOffset == iRequired;
}



/* 把 SEC1 ECPrivateKey 包装为标准未加密 PKCS#8。 */
static bool testTlsIdentityEcPkcs8(
	const uint8* pSec1,
	size_t iSec1Size,
	size_t iPrivateSize,
	uint8* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	static const uint8 P256Algorithm[] = {
		0x30, 0x13,
		0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
		0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07
	};
	static const uint8 P384Algorithm[] = {
		0x30, 0x10,
		0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
		0x06, 0x05, 0x2B, 0x81, 0x04, 0x00, 0x22
	};
	const uint8* pAlgorithm;
	size_t iAlgorithmSize;
	size_t iPrivateHeader;
	size_t iBody;
	size_t iRequired;
	size_t iOffset;

	if ( iPrivateSize == 32u ) {
		pAlgorithm = P256Algorithm;
		iAlgorithmSize = sizeof(P256Algorithm);
	} else if ( iPrivateSize == 48u ) {
		pAlgorithm = P384Algorithm;
		iAlgorithmSize = sizeof(P384Algorithm);
	} else {
		return false;
	}
	iPrivateHeader = iSec1Size < 128u ? 2u : 3u;
	iBody = 3u + iAlgorithmSize + iPrivateHeader + iSec1Size;
	iRequired = (iBody < 128u ? 2u : 3u) + iBody;
	if ( (pSec1 == NULL) || (pOutput == NULL) || (pSize == NULL) ||
		(iRequired > iCapacity) ) {
		return false;
	}
	iOffset = testTlsIdentityDerHeader(pOutput, 0x30, iBody);
	pOutput[iOffset++] = 0x02;
	pOutput[iOffset++] = 0x01;
	pOutput[iOffset++] = 0x00;
	memcpy(pOutput + iOffset, pAlgorithm, iAlgorithmSize);
	iOffset += iAlgorithmSize;
	iOffset += testTlsIdentityDerHeader(
		pOutput + iOffset, 0x04, iSec1Size
	);
	memcpy(pOutput + iOffset, pSec1, iSec1Size);
	iOffset += iSec1Size;
	*pSize = iOffset;
	return iOffset == iRequired;
}

#endif



#if defined(TEST_TLS_IDENTITY_FIXTURE_ED25519)

/* 构造 RFC 8410 Ed25519 PrivateKeyInfo。 */
static bool testTlsIdentityEdPkcs8(
	const uint8* pSeed,
	uint8* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	static const uint8 Prefix[] = {
		0x30, 0x2E, 0x02, 0x01, 0x00,
		0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,
		0x04, 0x22, 0x04, 0x20
	};

	if ( (pSeed == NULL) || (pOutput == NULL) || (pSize == NULL) ||
		(iCapacity < (sizeof(Prefix) + XRT_ED25519_SEED_SIZE)) ) {
		return false;
	}
	memcpy(pOutput, Prefix, sizeof(Prefix));
	memcpy(
		pOutput + sizeof(Prefix), pSeed, XRT_ED25519_SEED_SIZE
	);
	*pSize = sizeof(Prefix) + XRT_ED25519_SEED_SIZE;
	return true;
}

#endif

#endif
