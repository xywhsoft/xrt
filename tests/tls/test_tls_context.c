#include "../test.h"



/* 默认上下文应拥有完整策略与限制快照。 */
static void testTlsContextDefault(void)
{
	xtlscontext* pContext = xrtTlsContextCreate(NULL);
	const xtlspolicy* pPolicy;
	const xtlslimits* pLimits;

	testRequire(pContext != NULL, "default TLS context creation failed");
	pPolicy = xrtTlsContextPolicy(pContext);
	pLimits = xrtTlsContextLimits(pContext);
	testRequire((pPolicy != NULL) && xrtTlsPolicyValid(pPolicy) &&
		(pPolicy->VersionCount == 2u) &&
		(pPolicy->CipherCount == 9u),
		"default TLS context policy mismatch");
	testRequire((pLimits != NULL) && xrtTlsLimitsValid(pLimits) &&
		(pLimits->FeedLimit == XTLS_FEED_LIMIT_DEFAULT) &&
		(pLimits->HandshakeLimit == XTLS_HANDSHAKE_LIMIT_DEFAULT) &&
		(pLimits->RecordBudget == XTLS_DRIVE_RECORD_BUDGET_DEFAULT),
		"default TLS context limits mismatch");
	xrtTlsContextRelease(pContext);
}



/* 上下文必须深拷贝调用方策略数组与限制。 */
static void testTlsContextSnapshot(void)
{
	xtlsversion Versions[] = { XTLS_VERSION_13 };
	xtlscipher Ciphers[] = { XTLS_AES_128_GCM_SHA256 };
	uint16 Groups[] = { XTLS_GROUP_X25519 };
	xtlssignature Signatures[] = { XTLS_SIGNATURE_ED25519 };
	xtlspolicy Policy = {
		Versions, 1u,
		Ciphers, 1u,
		Groups, 1u,
		Signatures, 1u,
		XTLS_KEY_SHARE_PREFER_GROUP
	};
	xtlscontextconfig Config;
	xtlscontext* pContext;
	const xtlspolicy* pSnapshot;
	const xtlslimits* pLimits;

	xrtTlsContextConfigInit(&Config);
	Config.Policy = &Policy;
	Config.Limits.FeedLimit = 131072u;
	Config.Limits.RecordBudget = 7u;
	pContext = xrtTlsContextCreate(&Config);
	testRequire(pContext != NULL, "custom TLS context creation failed");

	Versions[0] = XTLS_VERSION_12;
	Ciphers[0] = XTLS_ECDHE_RSA_AES_128_GCM_SHA256;
	Groups[0] = XTLS_GROUP_X448;
	Signatures[0] = XTLS_SIGNATURE_RSA_PKCS1_SHA256;
	Config.Limits.FeedLimit = XTLS_FEED_LIMIT_DEFAULT;
	Config.Limits.RecordBudget = XTLS_DRIVE_RECORD_BUDGET_DEFAULT;

	pSnapshot = xrtTlsContextPolicy(pContext);
	pLimits = xrtTlsContextLimits(pContext);
	testRequire((pSnapshot->Versions != Versions) &&
		(pSnapshot->Ciphers != Ciphers) &&
		(pSnapshot->Groups != Groups) &&
		(pSnapshot->Signatures != Signatures) &&
		(pSnapshot->Versions[0] == XTLS_VERSION_13) &&
		(pSnapshot->Ciphers[0] == XTLS_AES_128_GCM_SHA256) &&
		(pSnapshot->Groups[0] == XTLS_GROUP_X25519) &&
		(pSnapshot->Signatures[0] == XTLS_SIGNATURE_ED25519),
		"TLS context borrowed mutable policy storage");
	testRequire((pLimits->FeedLimit == 131072u) &&
		(pLimits->RecordBudget == 7u),
		"TLS context borrowed mutable limits");
	xrtTlsContextRelease(pContext);
}



/* 引用计数必须让同一只读上下文跨所有者存活。 */
static void testTlsContextReferences(void)
{
	xtlscontext* pContext = xrtTlsContextCreate(NULL);
	xtlscontext* pRetained;

	testRequire(pContext != NULL, "TLS context reference setup failed");
	pRetained = xrtTlsContextRetain(pContext);
	testRequire(pRetained == pContext, "TLS context retain changed identity");
	xrtTlsContextRelease(pContext);
	testRequire((xrtTlsContextPolicy(pRetained) != NULL) &&
		(xrtTlsContextLimits(pRetained) != NULL),
		"retained TLS context did not remain alive");
	xrtTlsContextRelease(pRetained);
	xrtTlsContextRelease(NULL);
}



/* 执行 TLS 上下文正常路径回归。 */
int main(void)
{
	testTlsContextDefault();
	testTlsContextSnapshot();
	testTlsContextReferences();
	return 0;
}
