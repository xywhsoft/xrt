#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_legacy_cert.h"

#include <stdio.h>



/* 单头文件自定义验证器明确接管信任决策。 */
static xtlsverifydecision testSingleTlsVerify(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pContext;
	return (pPeer != NULL) && (pPeer->CertificateCount == 1u) ?
		XTLS_VERIFY_ACCEPT : XTLS_VERIFY_REJECT;
}



/* 验证单头文件中的共享 TLS 验证器。 */
int main(void)
{
	xtlsverifierconfig Config;
	xtlsverifier* pVerifier;
	xx509cert Certificate;
	bool bVerified;

	if ( !xrtX509Parse(
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT), &Certificate
	) ) {
		return 1;
	}
	xrtTlsVerifierConfigInit(&Config);
	Config.Verify = testSingleTlsVerify;
	pVerifier = xrtTlsVerifierCreate(&Config);
	if ( pVerifier == NULL ) {
		return 1;
	}
	bVerified = xrtTlsVerifierVerify(
		pVerifier, XTLS_SERVER, XRT_STR_LITERAL("localhost"),
		&Certificate, 1u
	);
	xrtTlsVerifierRelease(pVerifier);
	if ( !bVerified ) {
		return 1;
	}
	printf("[PASS] single-tls-verify\n");
	return 0;
}
