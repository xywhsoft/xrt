#ifndef XRT_TEST_TLS_IDENTITY_LEGACY_H
#define XRT_TEST_TLS_IDENTITY_LEGACY_H

#include "x509_legacy_cert.h"



/* 从 ver1 TLS 示例原样迁移的匹配 PKCS#1 RSA 私钥。 */
static const char TLS_IDENTITY_LEGACY_RSA_KEY[] =
	"-----BEGIN RSA PRIVATE KEY-----\n"
	"MIIEogIBAAKCAQEAirK/A/tzcSNyRkRcm/7InueCninfhqbhQyGxyUKGSu1a8Q3e\n"
	"K3FM7gt/7vS13iYA7Wcclhiku+TQtPzYxbJwRxTcUbx/uHAuQNhnUZDVilDtEd/S\n"
	"rPQCr+fwM+f7Ejrf63mlXUdZ6pl8VhndrYtak2b6tm2p1G892cvE5UJwgxzIlLKk\n"
	"MPM44Mhora0ZNyUYMR+OKprUq+EpVYnYxFEK1rg2PMDowRJxjFx02UBhNhSJJVse\n"
	"Txw/NMWTWGWhApI61MO1Ayuuskub69smNW6yeOuAsD1AuY0r3/t7f9MRtHQDltca\n"
	"timzsaDhvQylu7GkvYCTwOYQud3JaCVCjeT8BQIDAQABAoIBAAFrOC8ufpJo94A2\n"
	"rqWC4E2t1m/PQ66Gh6s5OEQei9ikNR1eVAg+PGL6rPhGZROMxc7Skp/fcyn1OoRW\n"
	"H3066NFKqkFPdsADLRnz4hoFwQV38/Y47lhfa6VIhQlxj/zdANGRbkALcjpHOoF3\n"
	"+hpOdg1SzPGaTWszUx7mqdDi59s+J4fMLGZf72xxPdAfmcAFIBq1mPDvnmNjcEOG\n"
	"ydF2nCyOnvXPJgATmH1Ecm5y3+WBDoXNUI1DIibK4idxDzmHA+h0zb4EhtmEhOM4\n"
	"dhGuMra+TJWBSJ8EN0BrGoWmFmJ93QtJl69LBJV/QOOrZDb1qDqaojVqYzhhTubP\n"
	"oVMbVRkCgYEAvoVRpNlj7xHJwSAs7/c4HEMVj8ql2joUweuKjtOYSdJxMnEFGsuA\n"
	"/3Ywgi7Xx7mc6f8OQyhV/L1UNCsBNagbwPYj2knfSolTmDklP3+boUJQVPhDkw0Y\n"
	"pDp97xNwrLXDR86De9neKT8ojPXEU32RR+d8bd/4m8oNJJwJf6xFKS0CgYEAul3m\n"
	"H/CuhKDzStiZcYhl2z/GGAKB8uZ07vwhlmfSw07A0awweC3rF5OJ93qHRT0ZAfGy\n"
	"1aYdc0bGcyHa67/xnb4mWYDXNptyf3Q1TCIi+nOqalJbk0ZfPBV98//1zvXga2ie\n"
	"2yQyYrJtvw7mW1YzQYeAq+PRf3Z4fMjZvpcqtTkCgYA0ub6biZIPgnO8X8Qv8NH1\n"
	"eFdKQQHfP/2ooR/qYQKfQ38SP5bzEGi1yiaokIAlBOg5Fd4DlfEeDeN0wIYILGrp\n"
	"3vSTH6iM/y5ETWRSi2UtnqWOrlo9Iv2zzYA2nsGq+m59u9hFeUjzT0hQol9f37tK\n"
	"E/UqjzZFHwi+HfS/AZTuTQKBgAuSKuiOw/ceGxzph9Vht5k+Q2lYNoNDRb1U0C0L\n"
	"cy2HJTefbj738uG62lUQOXfWDEhvnj/fmXJ/0XByiKocd77ogG8MLdCJJDm/mFOK\n"
	"xwsvxUPmqyLguqb7Wp+co8FeyLlCfKJ0g+BW3bOAFFNVbcdCx31knqxAScjNm59W\n"
	"uWMZAoGAKvrkef9oYB5AAMSQbGIYvIm+vv57T8tZS+tog0qaU8QPYPXNqJ3MFL46\n"
	"zQZCvDFm+8mkLiOhzmJIme4NRrjywITkXEIPg9YSe792dtxgvHE78XPOlZO8uE+W\n"
	"aeku7Q3d+O5bw7M7R6Cq99hQ1j3RlkMKsNE766qAkAXoJBaB89E=\n"
	"-----END RSA PRIVATE KEY-----\n";



/* 解码旧版 PEM 私钥到调用方缓冲，避免测试把 PEM 所有权与身份混在一起。 */
static bool testTlsIdentityLegacyKey(
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xpemblock Block;

	return xrtPemFind(
		TLS_IDENTITY_LEGACY_RSA_KEY,
		sizeof(TLS_IDENTITY_LEGACY_RSA_KEY) - 1u,
		"RSA PRIVATE KEY", &Block
	) && xrtPemDecode(&Block, pOutput, iCapacity, pSize);
}



#if defined(XRT_TEST_TLS_IDENTITY_LEGACY_CERT_ALGORITHM)

/* 写入历史证书重构夹具所需的规范 DER 标签和长度。 */
static size_t testTlsIdentityLegacyDerHeader(
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



/* 保留旧证书全部字段，只结构化替换叶 SPKI 的 AlgorithmIdentifier。 */
static bool testTlsIdentityLegacyCertificateAlgorithm(
	xbytesview Algorithm,
	uint8* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xx509cert Certificate;
	xdercursor Cursor;
	xdercursor SpkiFields;
	xdervalue Outer;
	xdervalue Tbs;
	xdervalue Spki;
	xdervalue OldAlgorithm;
	xdervalue Key;
	uint8 NewSpki[512];
	size_t iSpkiBody;
	size_t iSpkiSize;
	size_t iTbsPrefix;
	size_t iTbsSuffix;
	size_t iTbsBody;
	size_t iTbsSize;
	size_t iOuterPrefix;
	size_t iOuterSuffix;
	size_t iOuterBody;
	size_t iRequired;
	size_t iOffset;

	if ( (Algorithm.Data == NULL) || (Algorithm.Size == 0) ||
		(pOutput == NULL) || (pSize == NULL) ||
		!xrtX509Parse(
			X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT),
			&Certificate
		) || !xrtDerInit(
			&Cursor, Certificate.Raw.Data, Certificate.Raw.Size
		) || (xrtDerRead(&Cursor, &Outer) != XDER_VALUE) ||
		!xrtDerDone(&Cursor) || !xrtDerInit(
			&Cursor, Certificate.Tbs.Data, Certificate.Tbs.Size
		) || (xrtDerRead(&Cursor, &Tbs) != XDER_VALUE) ||
		!xrtDerDone(&Cursor) || !xrtDerInit(
			&Cursor, Certificate.SubjectPublicKeyInfo.Data,
			Certificate.SubjectPublicKeyInfo.Size
		) || (xrtDerRead(&Cursor, &Spki) != XDER_VALUE) ||
		!xrtDerDone(&Cursor) || !xrtDerEnter(&Spki, &SpkiFields) ||
		(xrtDerRead(&SpkiFields, &OldAlgorithm) != XDER_VALUE) ||
		(xrtDerRead(&SpkiFields, &Key) != XDER_VALUE) ||
		!xrtDerDone(&SpkiFields) ) {
		return false;
	}
	(void)OldAlgorithm;
	iSpkiBody = Algorithm.Size + Key.Raw.Size;
	iSpkiSize = testTlsIdentityLegacyDerHeader(
		NewSpki, 0x30, iSpkiBody
	);
	if ( (iSpkiSize + iSpkiBody) > sizeof(NewSpki) ) {
		return false;
	}
	memcpy(NewSpki + iSpkiSize, Algorithm.Data, Algorithm.Size);
	iSpkiSize += Algorithm.Size;
	memcpy(NewSpki + iSpkiSize, Key.Raw.Data, Key.Raw.Size);
	iSpkiSize += Key.Raw.Size;
	iTbsPrefix = (size_t)(
		Certificate.SubjectPublicKeyInfo.Data - Tbs.Value.Data
	);
	if ( iTbsPrefix > Tbs.Value.Size ) {
		return false;
	}
	iTbsSuffix = Tbs.Value.Size - iTbsPrefix -
		Certificate.SubjectPublicKeyInfo.Size;
	iTbsBody = iTbsPrefix + iSpkiSize + iTbsSuffix;
	iTbsSize = (iTbsBody < 128u ? 2u :
		(iTbsBody <= UINT8_MAX ? 3u : 4u)) + iTbsBody;
	iOuterPrefix = (size_t)(Certificate.Tbs.Data - Outer.Value.Data);
	if ( iOuterPrefix > Outer.Value.Size ) {
		return false;
	}
	iOuterSuffix = Outer.Value.Size - iOuterPrefix - Certificate.Tbs.Size;
	iOuterBody = iOuterPrefix + iTbsSize + iOuterSuffix;
	iRequired = (iOuterBody < 128u ? 2u :
		(iOuterBody <= UINT8_MAX ? 3u : 4u)) + iOuterBody;
	if ( iRequired > iCapacity ) {
		return false;
	}
	iOffset = testTlsIdentityLegacyDerHeader(pOutput, 0x30, iOuterBody);
	memcpy(pOutput + iOffset, Outer.Value.Data, iOuterPrefix);
	iOffset += iOuterPrefix;
	iOffset += testTlsIdentityLegacyDerHeader(
		pOutput + iOffset, 0x30, iTbsBody
	);
	memcpy(pOutput + iOffset, Tbs.Value.Data, iTbsPrefix);
	iOffset += iTbsPrefix;
	memcpy(pOutput + iOffset, NewSpki, iSpkiSize);
	iOffset += iSpkiSize;
	memcpy(
		pOutput + iOffset,
		Certificate.SubjectPublicKeyInfo.Data +
			Certificate.SubjectPublicKeyInfo.Size,
		iTbsSuffix
	);
	iOffset += iTbsSuffix;
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

#endif
