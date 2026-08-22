#include "../test.h"



/* 默认策略必须稳定覆盖当前协议范围且不拥有调用方内存。 */
static void testTlsPolicyDefault(void)
{
	xtlspolicy First;
	xtlspolicy Second;

	xrtTlsPolicyInit(&First);
	xrtTlsPolicyInit(&Second);
	testRequire(xrtTlsPolicyValid(&First),
		"default TLS policy is invalid");
	testRequire((First.VersionCount == 2u) &&
		(First.Versions[0] == XTLS_VERSION_13) &&
		(First.Versions[1] == XTLS_VERSION_12),
		"default TLS version preference mismatch");
	testRequire((First.CipherCount == 9u) &&
		(First.Ciphers[0] == XTLS_AES_128_GCM_SHA256),
		"default TLS cipher preference mismatch");
	testRequire((First.GroupCount == 4u) &&
		(First.Groups[0] == XTLS_GROUP_X25519) &&
		(First.Groups[1] == XTLS_GROUP_SECP256R1),
		"default TLS group preference mismatch");
	testRequire((First.SignatureCount == 14u) &&
		(First.Signatures[0] == XTLS_SIGNATURE_ED25519),
		"default TLS signature preference mismatch");
	testRequire(First.KeySharePolicy == XTLS_KEY_SHARE_PREFER_READY,
		"default TLS key-share policy mismatch");
	testRequire((First.Versions == Second.Versions) &&
		(First.Ciphers == Second.Ciphers) &&
		(First.Groups == Second.Groups) &&
		(First.Signatures == Second.Signatures),
		"default TLS policy arrays are not process-lifetime constants");
}



/* 自定义策略必须允许纯 TLS 1.3 和无证书恢复场景。 */
static void testTlsPolicyCustom13(void)
{
	static const xtlsversion Versions[] = { XTLS_VERSION_13 };
	static const xtlscipher Ciphers[] = {
		XTLS_CHACHA20_POLY1305_SHA256
	};
	static const xtlssignature Signatures[] = {
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
	};
	xtlspolicy Policy = {
		Versions, 1u,
		Ciphers, 1u,
		NULL, 0u,
		Signatures, 1u,
		XTLS_KEY_SHARE_PREFER_GROUP
	};
	xtlspolicy Before = Policy;

	testRequire(xrtTlsPolicyValid(&Policy),
		"custom TLS 1.3 policy is invalid");
	testRequire(memcmp(&Policy, &Before, sizeof(Policy)) == 0,
		"TLS policy validation modified its input");

	Policy.Signatures = NULL;
	Policy.SignatureCount = 0u;
	testRequire(xrtTlsPolicyValid(&Policy),
		"TLS policy rejected an external authentication path");
}



/* TLS 1.2 策略必须保留 ECDHE、PKCS#1 和自定义本地顺序。 */
static void testTlsPolicyCustom12(void)
{
	static const xtlsversion Versions[] = { XTLS_VERSION_12 };
	static const xtlscipher Ciphers[] = {
		XTLS_ECDHE_RSA_AES_256_GCM_SHA384,
		XTLS_ECDHE_RSA_AES_128_GCM_SHA256
	};
	static const uint16 Groups[] = { XTLS_GROUP_SECP384R1 };
	static const xtlssignature Signatures[] = {
		XTLS_SIGNATURE_RSA_PKCS1_SHA384,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384
	};
	xtlspolicy Policy = {
		Versions, 1u,
		Ciphers, 2u,
		Groups, 1u,
		Signatures, 2u,
		XTLS_KEY_SHARE_PREFER_READY
	};

	testRequire(xrtTlsPolicyValid(&Policy),
		"custom TLS 1.2 policy is invalid");
}



/* 执行 TLS 策略正常路径回归。 */
int main(void)
{
	testTlsPolicyDefault();
	testTlsPolicyCustom13();
	testTlsPolicyCustom12();
	return 0;
}
