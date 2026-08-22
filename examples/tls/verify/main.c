#include <xrt/tls_verify.h>

#include <stdio.h>



/* 示例回调以应用自有的证书固定策略接管信任决策。 */
static xtlsverifydecision exampleTlsVerify(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pContext;
	return pPeer->CertificateCount != 0 ?
		XTLS_VERIFY_ACCEPT : XTLS_VERIFY_REJECT;
}



/* 创建可供多个 TLS 客户端共享的自定义验证器。 */
int main(void)
{
	xtlsverifierconfig Config;
	xtlsverifier* pVerifier;

	xrtTlsVerifierConfigInit(&Config);
	Config.Verify = exampleTlsVerify;
	pVerifier = xrtTlsVerifierCreate(&Config);
	if ( pVerifier == NULL ) {
		return 1;
	}
	printf("TLS verifier is ready\n");
	xrtTlsVerifierRelease(pVerifier);
	return 0;
}
