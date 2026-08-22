#include "../test.h"
#include "../fixtures/x509_signature_vectors.h"



/* 解析一个独立 AlgorithmIdentifier 并进入签名算法层。 */
static xx509result testX509Signature(
	const uint8* pDer,
	size_t iSize,
	xx509signature* pSignature
)
{
	xx509algorithm Algorithm;

	testRequire(xrtX509AlgorithmParse(
		(xbytesview) { pDer, iSize }, &Algorithm
	), "test signature AlgorithmIdentifier parse failed");
	return xrtX509SignatureParse(&Algorithm, pSignature);
}



/* 验证 RSA、ECDSA、EdDSA 和未知算法的公共分类契约。 */
static void testX509SignatureSchemes(void)
{
	xx509signature Signature;
	xx509signature Before;

	testRequire((testX509Signature(
		X509_SIGNATURE_RSA_SHA256,
		sizeof(X509_SIGNATURE_RSA_SHA256), &Signature
	) == X509_VALUE) &&
		(Signature.Type == X509_SIGNATURE_RSA_PKCS1) &&
		(Signature.Hash == X509_HASH_SHA256),
		"RSA SHA-256 signature classification failed");
	testRequire((testX509Signature(
		X509_SIGNATURE_RSA_SHA384_NULL,
		sizeof(X509_SIGNATURE_RSA_SHA384_NULL), &Signature
	) == X509_VALUE) &&
		(Signature.Type == X509_SIGNATURE_RSA_PKCS1) &&
		(Signature.Hash == X509_HASH_SHA384),
		"RSA SHA-384 NULL parameter compatibility failed");
	testRequire((testX509Signature(
		X509_SIGNATURE_ECDSA_SHA512,
		sizeof(X509_SIGNATURE_ECDSA_SHA512), &Signature
	) == X509_VALUE) && (Signature.Type == X509_SIGNATURE_ECDSA) &&
		(Signature.Hash == X509_HASH_SHA512),
		"ECDSA SHA-512 signature classification failed");
	testRequire((testX509Signature(
		X509_SIGNATURE_ED25519_DER,
		sizeof(X509_SIGNATURE_ED25519_DER), &Signature
	) == X509_VALUE) && (Signature.Type == X509_SIGNATURE_ED25519) &&
		(Signature.Hash == X509_HASH_NONE),
		"Ed25519 signature classification failed");

	memset(&Signature, 0xA5, sizeof(Signature));
	Before = Signature;
	testRequire((testX509Signature(
		X509_SIGNATURE_UNKNOWN,
		sizeof(X509_SIGNATURE_UNKNOWN), &Signature
	) == X509_DONE) &&
		(memcmp(&Signature, &Before, sizeof(Signature)) == 0),
		"unknown signature algorithm did not preserve output");
}



/* 验证 PSS 缺省值、独立 MGF1 摘要和显式默认值兼容。 */
static void testX509SignaturePss(void)
{
	xx509signature Signature;

	testRequire((testX509Signature(
		X509_SIGNATURE_PSS_DEFAULT,
		sizeof(X509_SIGNATURE_PSS_DEFAULT), &Signature
	) == X509_VALUE) && (Signature.Type == X509_SIGNATURE_RSA_PSS) &&
		(Signature.Hash == X509_HASH_SHA1) &&
		(Signature.MaskHash == X509_HASH_SHA1) &&
		(Signature.SaltSize == 20u) && (Signature.Trailer == 1u),
		"default RSA-PSS parameters failed");
	testRequire((testX509Signature(
		X509_SIGNATURE_PSS_CUSTOM,
		sizeof(X509_SIGNATURE_PSS_CUSTOM), &Signature
	) == X509_VALUE) && (Signature.Hash == X509_HASH_SHA256) &&
		(Signature.MaskHash == X509_HASH_SHA384) &&
		(Signature.SaltSize == 32u) && (Signature.Trailer == 1u),
		"custom RSA-PSS parameters failed");
	testRequire((testX509Signature(
		X509_SIGNATURE_PSS_EXPLICIT_DEFAULT,
		sizeof(X509_SIGNATURE_PSS_EXPLICIT_DEFAULT), &Signature
	) == X509_VALUE) && (Signature.Hash == X509_HASH_SHA1) &&
		(Signature.MaskHash == X509_HASH_SHA1) &&
		(Signature.SaltSize == 20u) && (Signature.Trailer == 1u),
		"explicit RSA-PSS defaults were not accepted");
}



typedef struct testx509signaturefailure {
	const uint8* Data;
	size_t Size;
} testx509signaturefailure;



/* 验证已知算法参数错误具有统一错误码和失败原子性。 */
static void testX509SignatureRejects(void)
{
	static const testx509signaturefailure Cases[] = {
		{ X509_SIGNATURE_BAD_RSA_PARAMETER,
			sizeof(X509_SIGNATURE_BAD_RSA_PARAMETER) },
		{ X509_SIGNATURE_BAD_ECDSA_PARAMETER,
			sizeof(X509_SIGNATURE_BAD_ECDSA_PARAMETER) },
		{ X509_SIGNATURE_BAD_ED25519_PARAMETER,
			sizeof(X509_SIGNATURE_BAD_ED25519_PARAMETER) },
		{ X509_SIGNATURE_BAD_PSS_MISSING,
			sizeof(X509_SIGNATURE_BAD_PSS_MISSING) },
		{ X509_SIGNATURE_BAD_PSS_DUPLICATE,
			sizeof(X509_SIGNATURE_BAD_PSS_DUPLICATE) },
		{ X509_SIGNATURE_BAD_PSS_ORDER,
			sizeof(X509_SIGNATURE_BAD_PSS_ORDER) },
		{ X509_SIGNATURE_BAD_PSS_FIELD,
			sizeof(X509_SIGNATURE_BAD_PSS_FIELD) },
		{ X509_SIGNATURE_BAD_PSS_TRAILER,
			sizeof(X509_SIGNATURE_BAD_PSS_TRAILER) },
		{ X509_SIGNATURE_BAD_PSS_MGF,
			sizeof(X509_SIGNATURE_BAD_PSS_MGF) }
	};

	for ( size_t i = 0; i < sizeof(Cases) / sizeof(Cases[0]); i++ ) {
		xx509algorithm Algorithm;
		xx509signature Signature;
		xx509signature Before;
		const xerror* pError;

		testRequire(xrtX509AlgorithmParse(
			(xbytesview) { Cases[i].Data, Cases[i].Size }, &Algorithm
		), "invalid signature fixture is not a valid AlgorithmIdentifier");
		memset(&Signature, 0xA5, sizeof(Signature));
		Before = Signature;
		testRequire((xrtX509SignatureParse(
			&Algorithm, &Signature
		) == X509_ERROR) &&
			(memcmp(&Signature, &Before, sizeof(Signature)) == 0),
			"invalid signature parameters changed failed output");
		pError = xrtGetError();
		testRequire((pError != NULL) &&
			(strcmp(xrtErrorDomain(pError), "xrt.x509") == 0) &&
			(xrtErrorCode(pError) == X509_ERROR_SIGNATURE_ALGORITHM),
			"invalid signature parameters lost structured error");
	}
}



int main(void)
{
	testX509SignatureSchemes();
	testX509SignaturePss();
	testX509SignatureRejects();
	printf("[PASS] x509_signature\n");
	return 0;
}
