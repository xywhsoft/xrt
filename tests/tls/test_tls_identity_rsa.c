#include "../test.h"
#define XRT_TEST_TLS_IDENTITY_LEGACY_CERT_ALGORITHM
#include "../fixtures/tls_identity_legacy.h"



static const uint8 TEST_TLS_IDENTITY_PSS_SHA256[] = {
	0x30, 0x41,
	0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0A,
	0x30, 0x34,
	0xA0, 0x0F,
	0x30, 0x0D, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
	0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00,
	0xA1, 0x1C,
	0x30, 0x1A,
	0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x08,
	0x30, 0x0D, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
	0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00,
	0xA2, 0x03, 0x02, 0x01, 0x20
};



/* 写入当前测试所需的最短规范 DER 长度。 */
static size_t testTlsIdentityDerLength(uint8* pOutput, size_t iSize)
{
	if ( iSize < 128u ) {
		pOutput[0] = (uint8)iSize;
		return 1u;
	}
	if ( iSize <= UINT8_MAX ) {
		pOutput[0] = 0x81;
		pOutput[1] = (uint8)iSize;
		return 2u;
	}
	pOutput[0] = 0x82;
	pOutput[1] = (uint8)(iSize >> 8u);
	pOutput[2] = (uint8)iSize;
	return 3u;
}



/* 把旧版 PKCS#1 资产封装为标准未加密 PKCS#8 PrivateKeyInfo。 */
static bool testTlsIdentityPkcs8(
	xbytesview Pkcs1,
	uint8* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	static const uint8 Algorithm[] = {
		0x30, 0x0D, 0x06, 0x09,
		0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01,
		0x05, 0x00
	};
	uint8 PrivateLength[3];
	uint8 SequenceLength[3];
	size_t iPrivateLength = testTlsIdentityDerLength(
		PrivateLength, Pkcs1.Size
	);
	size_t iBody = 3u + sizeof(Algorithm) + 1u + iPrivateLength + Pkcs1.Size;
	size_t iSequenceLength = testTlsIdentityDerLength(
		SequenceLength, iBody
	);
	size_t iRequired = 1u + iSequenceLength + iBody;
	size_t iOffset = 0;

	if ( iRequired > iCapacity ) {
		return false;
	}
	pOutput[iOffset++] = 0x30;
	memcpy(pOutput + iOffset, SequenceLength, iSequenceLength);
	iOffset += iSequenceLength;
	pOutput[iOffset++] = 0x02;
	pOutput[iOffset++] = 0x01;
	pOutput[iOffset++] = 0x00;
	memcpy(pOutput + iOffset, Algorithm, sizeof(Algorithm));
	iOffset += sizeof(Algorithm);
	pOutput[iOffset++] = 0x04;
	memcpy(pOutput + iOffset, PrivateLength, iPrivateLength);
	iOffset += iPrivateLength;
	memcpy(pOutput + iOffset, Pkcs1.Data, Pkcs1.Size);
	iOffset += Pkcs1.Size;
	*pSize = iOffset;
	return true;
}



/* 使用指定 AlgorithmIdentifier 包装旧版 PKCS#1 私钥。 */
static bool testTlsIdentityPkcs8Algorithm(
	xbytesview Pkcs1,
	xbytesview Algorithm,
	uint8* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	uint8 PrivateLength[3];
	uint8 SequenceLength[3];
	size_t iPrivateLength = testTlsIdentityDerLength(
		PrivateLength, Pkcs1.Size
	);
	size_t iBody = 3u + Algorithm.Size + 1u +
		iPrivateLength + Pkcs1.Size;
	size_t iSequenceLength = testTlsIdentityDerLength(
		SequenceLength, iBody
	);
	size_t iRequired = 1u + iSequenceLength + iBody;
	size_t iOffset = 0;

	if ( (Algorithm.Data == NULL) || (Algorithm.Size == 0) ||
		(iRequired > iCapacity) ) {
		return false;
	}
	pOutput[iOffset++] = 0x30;
	memcpy(pOutput + iOffset, SequenceLength, iSequenceLength);
	iOffset += iSequenceLength;
	pOutput[iOffset++] = 0x02;
	pOutput[iOffset++] = 0x01;
	pOutput[iOffset++] = 0x00;
	memcpy(pOutput + iOffset, Algorithm.Data, Algorithm.Size);
	iOffset += Algorithm.Size;
	pOutput[iOffset++] = 0x04;
	memcpy(pOutput + iOffset, PrivateLength, iPrivateLength);
	iOffset += iPrivateLength;
	memcpy(pOutput + iOffset, Pkcs1.Data, Pkcs1.Size);
	iOffset += Pkcs1.Size;
	*pSize = iOffset;
	return true;
}



/* 使用叶证书公钥验证身份生成的 RSA-PKCS1 与 RSA-PSS 签名。 */
static void testTlsIdentityRsaSign(xtlsidentity* pIdentity)
{
	static const uint8 Message[] = "xrt TLS identity legacy asset";
	xx509pubkey PublicKey;
	xrsapublickey Rsa;
	uint8 Hash[32];
	uint8 Signature[XRT_RSA_MODULUS_MAX_SIZE];
	size_t iSize = 0;

	testRequire(xrtTlsIdentityPublicKey(pIdentity, &PublicKey),
		"RSA identity public key query failed");
	Rsa.Modulus = PublicKey.Modulus.Data;
	Rsa.ModulusSize = PublicKey.Modulus.Size;
	Rsa.Exponent = PublicKey.Exponent.Data;
	Rsa.ExponentSize = PublicKey.Exponent.Size;
	testRequire(xrtTlsIdentityCanSign(
		pIdentity, XTLS_VERSION_12,
		XTLS_SIGNATURE_RSA_PKCS1_SHA256
	) && !xrtTlsIdentityCanSign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_RSA_PKCS1_SHA256
	) && xrtTlsIdentityCanSign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
	), "RSA identity TLS signature compatibility mismatch");
	testRequire(xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_12,
		XTLS_SIGNATURE_RSA_PKCS1_SHA256,
		(xbytesview) { Message, sizeof(Message) - 1u },
		NULL, 0, &iSize
	) && (iSize == PublicKey.Modulus.Size),
		"RSA identity signature size query failed");
	testRequire(xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_12,
		XTLS_SIGNATURE_RSA_PKCS1_SHA256,
		(xbytesview) { Message, sizeof(Message) - 1u },
		Signature, sizeof(Signature), &iSize
	) && xrtSha256(Message, sizeof(Message) - 1u, Hash) &&
		xrtRsaPkcs1Verify(
			&Rsa, XCRYPTO_HASH_SHA256, Hash, Signature, iSize
		), "RSA identity PKCS#1 signature verification failed");
	testRequire(xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		(xbytesview) { Message, sizeof(Message) - 1u },
		Signature, sizeof(Signature), &iSize
	) && xrtRsaPssVerify(
		&Rsa, XCRYPTO_HASH_SHA256, XCRYPTO_HASH_SHA256,
		XRT_RSA_PSS_SALT_ANY, Hash, Signature, iSize
	), "RSA identity PSS signature verification failed");
}



/* 旧版 PKCS#1 私钥必须可深复制、签名并保持引用生命周期。 */
static void testTlsIdentityRsaPkcs1(void)
{
	uint8 PrivateDer[2048];
	size_t iPrivateSize = 0;
	xbytesview Chain = {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	xtlsidentity* pIdentity;

	testRequire(testTlsIdentityLegacyKey(
		PrivateDer, sizeof(PrivateDer), &iPrivateSize
	), "legacy RSA identity key decode failed");
	pIdentity = xrtTlsIdentityRsa(
		&Chain, 1u, (xbytesview) { PrivateDer, iPrivateSize }
	);
	testRequire(pIdentity != NULL,
		"legacy PKCS#1 RSA identity creation failed");
	memset(PrivateDer, 0, sizeof(PrivateDer));
	testTlsIdentityRsaSign(pIdentity);
	xrtTlsIdentityRelease(pIdentity);
}



/* 同一旧私钥封装为 PKCS#8 后必须进入完全相同的身份与签名路径。 */
static void testTlsIdentityRsaPkcs8(void)
{
	uint8 PrivateDer[2048];
	uint8 Pkcs8[2304];
	size_t iPrivateSize = 0;
	size_t iPkcs8Size = 0;
	xbytesview Chain = {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	xtlsidentity* pIdentity;

	testRequire(testTlsIdentityLegacyKey(
		PrivateDer, sizeof(PrivateDer), &iPrivateSize
	) && testTlsIdentityPkcs8(
		(xbytesview) { PrivateDer, iPrivateSize },
		Pkcs8, sizeof(Pkcs8), &iPkcs8Size
	), "PKCS#8 RSA identity fixture construction failed");
	pIdentity = xrtTlsIdentityRsa(
		&Chain, 1u, (xbytesview) { Pkcs8, iPkcs8Size }
	);
	testRequire(pIdentity != NULL,
		"PKCS#8 RSA identity creation failed");
	testTlsIdentityRsaSign(pIdentity);
	xrtTlsIdentityRelease(pIdentity);
}



/* 受限 RSA-PSS 证书和私钥只能发布共同允许的 TLS PSS 方案。 */
static void testTlsIdentityRsaPss(void)
{
	static const uint8 Message[] = "xrt restricted rsa pss identity";
	uint8 PrivateDer[2048];
	uint8 Pkcs8[2304];
	uint8 Certificate[2048];
	uint8 Signature[XRT_RSA_MODULUS_MAX_SIZE];
	uint8 Hash[32];
	size_t iPrivateSize = 0;
	size_t iPkcs8Size = 0;
	size_t iCertificateSize = 0;
	size_t iSignatureSize = 0;
	xbytesview Algorithm = {
		TEST_TLS_IDENTITY_PSS_SHA256,
		sizeof(TEST_TLS_IDENTITY_PSS_SHA256)
	};
	xbytesview Chain;
	xtlsidentity* pIdentity;
	xx509pubkey PublicKey;
	xrsapublickey Rsa;

	testRequire(testTlsIdentityLegacyKey(
		PrivateDer, sizeof(PrivateDer), &iPrivateSize
	) && testTlsIdentityPkcs8Algorithm(
		(xbytesview) { PrivateDer, iPrivateSize }, Algorithm,
		Pkcs8, sizeof(Pkcs8), &iPkcs8Size
	) && testTlsIdentityLegacyCertificateAlgorithm(
		Algorithm, Certificate, sizeof(Certificate), &iCertificateSize
	), "restricted RSA-PSS identity fixtures failed");
	Chain = (xbytesview) { Certificate, iCertificateSize };
	pIdentity = xrtTlsIdentityRsa(
		&Chain, 1u, (xbytesview) { Pkcs8, iPkcs8Size }
	);
	testRequire((pIdentity != NULL) &&
		(xrtTlsIdentityType(pIdentity) == XTLS_IDENTITY_RSA_PSS) &&
		xrtTlsIdentityCanSign(
			pIdentity, XTLS_VERSION_13,
			XTLS_SIGNATURE_RSA_PSS_PSS_SHA256
		) && !xrtTlsIdentityCanSign(
			pIdentity, XTLS_VERSION_13,
			XTLS_SIGNATURE_RSA_PSS_PSS_SHA384
		) && !xrtTlsIdentityCanSign(
			pIdentity, XTLS_VERSION_13,
			XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
		), "restricted RSA-PSS identity capability mismatch");
	testRequire(xrtTlsIdentityPublicKey(pIdentity, &PublicKey) &&
		xrtTlsIdentitySign(
			pIdentity, XTLS_VERSION_13,
			XTLS_SIGNATURE_RSA_PSS_PSS_SHA256,
			(xbytesview) { Message, sizeof(Message) - 1u },
			Signature, sizeof(Signature), &iSignatureSize
		) && xrtSha256(Message, sizeof(Message) - 1u, Hash),
		"restricted RSA-PSS identity signing failed");
	Rsa.Modulus = PublicKey.Modulus.Data;
	Rsa.ModulusSize = PublicKey.Modulus.Size;
	Rsa.Exponent = PublicKey.Exponent.Data;
	Rsa.ExponentSize = PublicKey.Exponent.Size;
	testRequire(xrtRsaPssVerify(
		&Rsa, XCRYPTO_HASH_SHA256, XCRYPTO_HASH_SHA256,
		32u, Hash, Signature, iSignatureSize
	), "restricted RSA-PSS identity signature verification failed");
	xrtTlsIdentityRelease(pIdentity);
}



/* 执行 RSA 身份历史资产和协议签名回归。 */
int main(void)
{
	testTlsIdentityRsaPkcs1();
	testTlsIdentityRsaPkcs8();
	testTlsIdentityRsaPss();
	return 0;
}
