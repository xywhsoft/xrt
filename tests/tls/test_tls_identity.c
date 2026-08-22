#include "../test.h"
#include "../fixtures/x509_legacy_cert.h"



typedef struct test_tls_identity_signer {
	size_t Queries;
	size_t Signs;
	size_t Releases;
} test_tls_identity_signer;



/* 自定义签名器只声明一个 TLS 1.3 RSA-PSS 方案。 */
static bool testTlsIdentitySupports(
	ptr pContext,
	xtlsversion Version,
	xtlssignature Signature
)
{
	(void)pContext;
	return (Version == XTLS_VERSION_13) &&
		(Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256);
}



/* 自定义签名器使用固定长度输出验证核心容量和转发契约。 */
static bool testTlsIdentitySign(
	ptr pContext,
	xtlsversion Version,
	xtlssignature Signature,
	xbytesview Message,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	test_tls_identity_signer* pSigner =
		(test_tls_identity_signer*)pContext;

	if ( (Version != XTLS_VERSION_13) ||
		(Signature != XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256) ||
		(pSize == NULL) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		pSigner->Queries++;
		*pSize = 16u;
		return true;
	}
	if ( iCapacity < 16u ) {
		return false;
	}
	pSigner->Signs++;
	memset(pOutput, (int)(Message.Size & 0xFFu), 16u);
	*pSize = 16u;
	return true;
}



/* 记录身份最后一个引用释放外部上下文的准确时机。 */
static void testTlsIdentityRelease(ptr pContext)
{
	((test_tls_identity_signer*)pContext)->Releases++;
}



/* 身份必须深复制证书，并稳定发布类型、证书和叶公钥。 */
static void testTlsIdentitySnapshot(void)
{
	uint8 Certificate[sizeof(X509_LEGACY_RSA_CERT)];
	uint8 Original[sizeof(X509_LEGACY_RSA_CERT)];
	xbytesview Chain;
	xtlsidentityconfig Config;
	test_tls_identity_signer Signer = { 0 };
	xtlsidentity* pIdentity;
	xbytesview Stored;
	xx509pubkey PublicKey;

	memcpy(Certificate, X509_LEGACY_RSA_CERT, sizeof(Certificate));
	memcpy(Original, X509_LEGACY_RSA_CERT, sizeof(Original));
	Chain = (xbytesview) { Certificate, sizeof(Certificate) };
	memset(&Config, 0, sizeof(Config));
	Config.Certificates = &Chain;
	Config.CertificateCount = 1u;
	Config.Type = XTLS_IDENTITY_RSA;
	Config.Supports = testTlsIdentitySupports;
	Config.Sign = testTlsIdentitySign;
	Config.Release = testTlsIdentityRelease;
	Config.Context = &Signer;
	pIdentity = xrtTlsIdentityCreate(&Config);
	testRequire(pIdentity != NULL, "TLS identity snapshot creation failed");
	memset(Certificate, 0, sizeof(Certificate));
	testRequire((xrtTlsIdentityType(pIdentity) == XTLS_IDENTITY_RSA) &&
		(xrtTlsIdentityCertificateCount(pIdentity) == 1u) &&
		xrtTlsIdentityCertificate(pIdentity, 0, &Stored) &&
		(Stored.Data != Certificate) &&
		(Stored.Size == sizeof(Original)) &&
		(memcmp(Stored.Data, Original, sizeof(Original)) == 0) &&
		xrtTlsIdentityPublicKey(pIdentity, &PublicKey) &&
		(PublicKey.Type == X509_KEY_RSA) &&
		(PublicKey.Modulus.Size == 256u),
		"TLS identity did not preserve its immutable certificate snapshot");
	xrtTlsIdentityRelease(pIdentity);
	testRequire(Signer.Releases == 1u,
		"TLS identity did not release its custom signer exactly once");
}



/* 查询、容量失败、方案筛选和实际签名必须保持清晰口径。 */
static void testTlsIdentitySigning(void)
{
	static const uint8 Message[] = { 1, 2, 3, 4, 5 };
	xbytesview Chain = {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	xtlsidentityconfig Config;
	test_tls_identity_signer Signer = { 0 };
	xtlsidentity* pIdentity;
	uint8 Output[16];
	uint8 Original[16];
	size_t iSize = 77u;

	memset(&Config, 0, sizeof(Config));
	Config.Certificates = &Chain;
	Config.CertificateCount = 1u;
	Config.Type = XTLS_IDENTITY_RSA;
	Config.Supports = testTlsIdentitySupports;
	Config.Sign = testTlsIdentitySign;
	Config.Release = testTlsIdentityRelease;
	Config.Context = &Signer;
	pIdentity = xrtTlsIdentityCreate(&Config);
	testRequire(pIdentity != NULL, "TLS identity signing setup failed");
	testRequire(xrtTlsIdentityCanSign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
	) && !xrtTlsIdentityCanSign(
		pIdentity, XTLS_VERSION_12,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
	) && !xrtTlsIdentityCanSign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384
	), "TLS identity custom capability filter mismatch");
	testRequire(xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		(xbytesview) { Message, sizeof(Message) },
		NULL, 0, &iSize
	) && (iSize == sizeof(Output)) && (Signer.Queries == 1u),
		"TLS identity signature sizing failed");
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Original, Output, sizeof(Output));
	iSize = 77u;
	testRequire(!xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		(xbytesview) { Message, sizeof(Message) },
		Output, sizeof(Output) - 1u, &iSize
	) && (iSize == 77u) &&
		(memcmp(Output, Original, sizeof(Output)) == 0) &&
		(Signer.Signs == 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"TLS identity capacity failure was not atomic");
	xrtClearError();
	testRequire(xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		(xbytesview) { Message, sizeof(Message) },
		Output, sizeof(Output), &iSize
	) && (iSize == sizeof(Output)) && (Output[0] == sizeof(Message)) &&
		(Signer.Signs == 1u),
		"TLS identity custom signing failed");
	testRequire(xrtTlsIdentityRetain(pIdentity) == pIdentity,
		"TLS identity retain changed object identity");
	xrtTlsIdentityRelease(pIdentity);
	testRequire(Signer.Releases == 0u,
		"TLS identity released custom signer before its last reference");
	xrtTlsIdentityRelease(pIdentity);
	testRequire(Signer.Releases == 1u,
		"TLS identity final release did not release custom signer");
}



/* 执行 TLS 身份核心正常路径回归。 */
int main(void)
{
	testTlsIdentitySnapshot();
	testTlsIdentitySigning();
	return 0;
}
