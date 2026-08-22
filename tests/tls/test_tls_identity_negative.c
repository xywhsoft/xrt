#include "../test.h"
#include "../fixtures/x509_legacy_cert.h"



/* 负向签名器仅用于确保错误在进入回调前被核心拒绝。 */
static bool testTlsIdentityNegativeSign(
	ptr pContext,
	xtlsversion Version,
	xtlssignature Signature,
	xbytesview Message,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	(void)pContext;
	(void)Version;
	(void)Signature;
	(void)Message;
	(void)pOutput;
	(void)iCapacity;
	(void)pSize;
	return false;
}



/* 身份核心拒绝空链、错误类型、损坏证书和非法查询。 */
int main(void)
{
	uint8 Broken[sizeof(X509_LEGACY_RSA_CERT)];
	xbytesview Chain = {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	xtlsidentityconfig Config;
	xtlsidentity* pIdentity;
	xbytesview Certificate = { (cbytes)1, 77u };
	size_t iSize = 77u;

	memset(&Config, 0, sizeof(Config));
	Config.Certificates = &Chain;
	Config.CertificateCount = 1u;
	Config.Type = XTLS_IDENTITY_ECDSA_P256;
	Config.Sign = testTlsIdentityNegativeSign;
	testRequire(xrtTlsIdentityCreate(&Config) == NULL,
		"TLS identity accepted a leaf type mismatch");
	xrtClearError();
	Config.Type = XTLS_IDENTITY_RSA;
	Config.CertificateCount = 0;
	testRequire(xrtTlsIdentityCreate(&Config) == NULL,
		"TLS identity accepted an empty certificate chain");
	xrtClearError();
	memcpy(Broken, X509_LEGACY_RSA_CERT, sizeof(Broken));
	Broken[0] = 0x31;
	Chain = (xbytesview) { Broken, sizeof(Broken) };
	Config.CertificateCount = 1u;
	testRequire(xrtTlsIdentityCreate(&Config) == NULL,
		"TLS identity accepted malformed certificate DER");
	xrtClearError();
	Chain = (xbytesview) {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	pIdentity = xrtTlsIdentityCreate(&Config);
	testRequire(pIdentity != NULL, "TLS identity negative setup failed");
	testRequire(!xrtTlsIdentityCertificate(
		pIdentity, 1u, &Certificate
	) && (Certificate.Data == (cbytes)1) && (Certificate.Size == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"TLS identity certificate range failure changed output");
	xrtClearError();
	testRequire(!xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_RSA_PKCS1_SHA256,
		(xbytesview) { NULL, 0 }, NULL, 0, &iSize
	) && (iSize == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"TLS identity accepted a version-incompatible signature");
	xrtTlsIdentityRelease(pIdentity);
	return 0;
}
