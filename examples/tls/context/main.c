#include <xrt/tls.h>

#include <stdio.h>



/* 创建一份可由多个连接共享的自有策略与限制快照。 */
int main(void)
{
	static const xtlsversion Versions[] = { XTLS_VERSION_13 };
	static const xtlscipher Ciphers[] = {
		XTLS_AES_128_GCM_SHA256,
		XTLS_CHACHA20_POLY1305_SHA256
	};
	static const xtlssignature Signatures[] = {
		XTLS_SIGNATURE_ED25519,
		XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
	};
	xtlspolicy Policy;
	xtlscontextconfig Config;
	xtlscontext* pContext;

	xrtTlsPolicyInit(&Policy);
	Policy.Versions = Versions;
	Policy.VersionCount = 1u;
	Policy.Ciphers = Ciphers;
	Policy.CipherCount = 2u;
	Policy.Signatures = Signatures;
	Policy.SignatureCount = 3u;

	xrtTlsContextConfigInit(&Config);
	Config.Policy = &Policy;
	Config.Limits.PlainLimit = 512u * 1024u;
	pContext = xrtTlsContextCreate(&Config);
	if ( pContext == NULL ) {
		return 1;
	}
	printf(
		"versions=%zu plain-limit=%zu\n",
		xrtTlsContextPolicy(pContext)->VersionCount,
		xrtTlsContextLimits(pContext)->PlainLimit
	);
	xrtTlsContextRelease(pContext);
	return 0;
}
