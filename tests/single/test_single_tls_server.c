#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_legacy_cert.h"



/* 单头文件服务端只在真正构建首航时才调用外部签名器。 */
static bool testSingleTlsServerSign(
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
		((bytes)pOutput)[0] = 0xA5u;
	}
	*pSize = 1u;
	return true;
}



/* 验证单头文件服务端、恢复查询和票据状态契约可独立组合。 */
int main(void)
{
	xbytesview Certificate = {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	xtlsidentityconfig IdentityConfig;
	xtlsidentity* pIdentity;
	xtlsserverconfig ServerConfig;
	xtlssession* pServer;
	xtlsresume* pResume = (xtlsresume*)(uintptr_t)1u;
	bool bResult;

	memset(&IdentityConfig, 0, sizeof(IdentityConfig));
	IdentityConfig.Certificates = &Certificate;
	IdentityConfig.CertificateCount = 1u;
	IdentityConfig.Type = XTLS_IDENTITY_RSA;
	IdentityConfig.Sign = testSingleTlsServerSign;
	pIdentity = xrtTlsIdentityCreate(&IdentityConfig);
	if ( pIdentity == NULL ) {
		return 1;
	}
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Identity = pIdentity;
	pServer = xrtTlsServerCreate(&ServerConfig, NULL);
	xrtTlsIdentityRelease(pIdentity);
	if ( pServer == NULL ) {
		return 1;
	}
	bResult = (xrtTlsSessionRole(pServer) == XTLS_SERVER) &&
		(xrtTlsSessionState(pServer) == XTLS_STATE_HANDSHAKE) &&
		((xrtTlsSessionWait(pServer) & XTLS_WAIT_INPUT) != 0) &&
		!xrtTlsServerResumed(pServer) &&
		(xrtTlsServerTicketNew(pServer, &pResume) == XTLS_ERROR) &&
		(pResume == NULL) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_STATE);
	xrtTlsSessionDestroy(pServer);
	return bResult ? 0 : 1;
}
