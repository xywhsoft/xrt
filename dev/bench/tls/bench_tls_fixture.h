#ifndef XRT_BENCH_TLS_FIXTURE_H
#define XRT_BENCH_TLS_FIXTURE_H

#include <xrt.h>

#include "../../../tests/fixtures/tls_identity_legacy.h"



/* 基准验证器只接管信任决策，握手签名和 Finished 仍由 XRT 严格验证。 */
static xtlsverifydecision benchTlsFixtureAccept(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pContext;
	return (
		(pPeer != NULL) &&
		(pPeer->Role == XTLS_SERVER) &&
		(pPeer->CertificateCount != 0)
	) ? XTLS_VERIFY_ACCEPT : XTLS_VERIFY_REJECT;
}



/* 创建只包含当前基准路径的 TLS 1.3 策略快照。 */
static xtlscontext* benchTlsFixtureContext(void)
{
	static const xtlsversion Versions[] = { XTLS_VERSION_13 };
	static const xtlscipher Ciphers[] = {
		XTLS_AES_128_GCM_SHA256
	};
	static const uint16 Groups[] = { XTLS_GROUP_X25519 };
	static const xtlssignature Signatures[] = {
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
	};
	xtlspolicy Policy;
	xtlscontextconfig Config;

	xrtTlsPolicyInit(&Policy);
	Policy.Versions = Versions;
	Policy.VersionCount = sizeof(Versions) / sizeof(Versions[0]);
	Policy.Ciphers = Ciphers;
	Policy.CipherCount = sizeof(Ciphers) / sizeof(Ciphers[0]);
	Policy.Groups = Groups;
	Policy.GroupCount = sizeof(Groups) / sizeof(Groups[0]);
	Policy.Signatures = Signatures;
	Policy.SignatureCount = sizeof(Signatures) / sizeof(Signatures[0]);
	xrtTlsContextConfigInit(&Config);
	Config.Policy = &Policy;
	Config.Limits.RecordBudget = 4u;
	Config.Limits.HandshakeBudget = 4u;
	return xrtTlsContextCreate(&Config);
}



/* 从历史真实证书与匹配私钥创建共享 RSA 服务端身份。 */
static xtlsidentity* benchTlsFixtureIdentity(void)
{
	uint8 PrivateKey[2048];
	xbytesview Certificate = {
		X509_LEGACY_RSA_CERT,
		sizeof(X509_LEGACY_RSA_CERT)
	};
	size_t iPrivateKeySize = 0;
	xtlsidentity* pIdentity = NULL;

	if ( testTlsIdentityLegacyKey(
		PrivateKey,
		sizeof(PrivateKey),
		&iPrivateKeySize
	) ) {
		pIdentity = xrtTlsIdentityRsa(
			&Certificate,
			1u,
			(xbytesview) { PrivateKey, iPrivateKeySize }
		);
	}
	xrtSecureZero(PrivateKey, sizeof(PrivateKey));
	return pIdentity;
}

#endif
