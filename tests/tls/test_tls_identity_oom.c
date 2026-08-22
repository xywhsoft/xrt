#include "../test.h"
#include "../test_allocator.h"
#include "../fixtures/x509_legacy_cert.h"



/* OOM 测试签名器只提供合法的长度查询。 */
static bool testTlsIdentityOomSign(
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
	(void)iCapacity;
	if ( (pOutput == NULL) && (pSize != NULL) ) {
		*pSize = 1u;
		return true;
	}
	return false;
}



/* 身份单块分配失败不得接管签名上下文或发布半成品。 */
int main(void)
{
	xbytesview Chain = {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	xtlsidentityconfig Config;

	memset(&Config, 0, sizeof(Config));
	Config.Certificates = &Chain;
	Config.CertificateCount = 1u;
	Config.Type = XTLS_IDENTITY_RSA;
	Config.Sign = testTlsIdentityOomSign;
	testRequire(testInstallFailAllocator(),
		"TLS identity OOM allocator install failed");
	testRequire((xrtTlsIdentityCreate(&Config) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"TLS identity unexpectedly survived forced OOM");
	return 0;
}
