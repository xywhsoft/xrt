#include "../../src/internal/xrt_mail_net.h"
#include "../test.h"



/* 测试验证器明确接管信任决策，不允许 TLS 配置静默跳过验证。 */
static xtlsverifydecision testMailNetTlsVerify(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pPeer;
	(void)pContext;
	return XTLS_VERIFY_ACCEPT;
}



/* 验证 TLS 可选层的配置闭包和安全默认值。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xtlsverifierconfig VerifierConfig;
	xmailnetconfig Config;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xtlsverifier* pVerifier;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine != NULL,
		"mail TLS engine creation failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"mail TLS resolver creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testMailNetTlsVerify;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"mail TLS verifier creation failed");

	xrtMailNetConfigInit(&Config);
	Config.Engine = pEngine;
	Config.Resolver = pResolver;
	Config.Host = "mail.test";
	Config.Port = 465u;
	Config.Security = XMAIL_SECURITY_TLS;
	testRequire(!xrtMailNetConfigValid(&Config),
		"mail TLS accepted a missing verifier");
	xrtClearError();
	Config.Tls.Verifier = pVerifier;
	testRequire(xrtMailNetConfigValid(&Config),
		"mail TLS rejected a verified configuration");

	xrtTlsVerifierRelease(pVerifier);
	testRequire(xrtNetResolverDestroy(pResolver),
		"mail TLS resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"mail TLS engine destroy failed");
	return 0;
}
