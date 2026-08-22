#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_legacy_cert.h"



/* 单头文件签名器只验证身份核心能够独立组合。 */
static bool testSingleTlsIdentitySign(
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
	if ( (pSize == NULL) || ((pOutput != NULL) && (iCapacity < 1u)) ) {
		return false;
	}
	if ( pOutput != NULL ) {
		((bytes)pOutput)[0] = 0xA5;
	}
	*pSize = 1u;
	return true;
}



/* 验证单头文件中的不可变 TLS 身份核心。 */
int main(void)
{
	xbytesview Chain = {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	xtlsidentityconfig Config;
	xtlsidentity* pIdentity;

	memset(&Config, 0, sizeof(Config));
	Config.Certificates = &Chain;
	Config.CertificateCount = 1u;
	Config.Type = XTLS_IDENTITY_RSA;
	Config.Sign = testSingleTlsIdentitySign;
	pIdentity = xrtTlsIdentityCreate(&Config);
	if ( (pIdentity == NULL) ||
		(xrtTlsIdentityType(pIdentity) != XTLS_IDENTITY_RSA) ) {
		return 1;
	}
	xrtTlsIdentityRelease(pIdentity);
	return 0;
}
