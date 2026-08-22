#include "../test.h"



/* 验证失败必须落在统一的 TLS 协商错误域。 */
static void testTlsPolicyError(
	const xtlspolicy* pPolicy,
	xtlserror Code,
	cstr sMessage
)
{
	xrtClearError();
	testRequire(!xrtTlsPolicyValid(pPolicy), sMessage);
	testRequire((xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.tls") == 0) &&
		(xrtErrorCode(xrtGetError()) == (int32)Code),
		"TLS policy error metadata mismatch");
}



/* 版本与套件列表必须非空、唯一、已知且相互覆盖。 */
static void testTlsPolicyVersionCipherErrors(void)
{
	static const xtlsversion BadVersion[] = { (xtlsversion)0x7777 };
	static const xtlsversion DuplicateVersions[] = {
		XTLS_VERSION_13, XTLS_VERSION_13
	};
	static const xtlsversion Version13[] = { XTLS_VERSION_13 };
	static const xtlscipher BadCipher[] = { (xtlscipher)0x7777 };
	static const xtlscipher DuplicateCiphers[] = {
		XTLS_AES_128_GCM_SHA256,
		XTLS_AES_128_GCM_SHA256
	};
	static const xtlscipher Cipher12[] = {
		XTLS_ECDHE_RSA_AES_128_GCM_SHA256
	};
	xtlspolicy Policy;

	testTlsPolicyError(
		NULL, XTLS_ERROR_ARGUMENT, "null TLS policy was accepted"
	);
	xrtClearError();
	xrtTlsPolicyInit(NULL);
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XTLS_ERROR_ARGUMENT),
		"null TLS policy initialization did not set an error");

	xrtTlsPolicyInit(&Policy);
	Policy.Versions = NULL;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"null TLS version list was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.VersionCount = 0u;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"empty TLS version list was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.Versions = BadVersion;
	Policy.VersionCount = 1u;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"unknown TLS version was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.Versions = DuplicateVersions;
	Policy.VersionCount = 2u;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"duplicate TLS version was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.VersionCount = SIZE_MAX;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"oversized TLS version count was accepted"
	);

	xrtTlsPolicyInit(&Policy);
	Policy.Ciphers = NULL;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"null TLS cipher list was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.CipherCount = 0u;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"empty TLS cipher list was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.Ciphers = BadCipher;
	Policy.CipherCount = 1u;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"unknown TLS cipher was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.Ciphers = DuplicateCiphers;
	Policy.CipherCount = 2u;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"duplicate TLS cipher was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.CipherCount = SIZE_MAX;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"oversized TLS cipher count was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.Versions = Version13;
	Policy.VersionCount = 1u;
	Policy.Ciphers = Cipher12;
	Policy.CipherCount = 1u;
	testTlsPolicyError(&Policy, XTLS_ERROR_NEGOTIATION,
		"TLS cipher without an enabled version was accepted");
}



/* 组与签名列表必须保持指针一致、唯一和版本可用。 */
static void testTlsPolicyGroupSignatureErrors(void)
{
	static const uint16 BadGroup[] = { 0x7777u };
	static const uint16 DuplicateGroups[] = {
		XTLS_GROUP_X25519, XTLS_GROUP_X25519
	};
	static const xtlsversion Version13[] = { XTLS_VERSION_13 };
	static const xtlscipher Cipher13[] = { XTLS_AES_128_GCM_SHA256 };
	static const xtlssignature BadSignature[] = {
		(xtlssignature)0x7777
	};
	static const xtlssignature DuplicateSignatures[] = {
		XTLS_SIGNATURE_ED25519, XTLS_SIGNATURE_ED25519
	};
	static const xtlssignature Signature12[] = {
		XTLS_SIGNATURE_RSA_PKCS1_SHA256
	};
	xtlspolicy Policy;

	xrtTlsPolicyInit(&Policy);
	Policy.Groups = NULL;
	Policy.GroupCount = 1u;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_ARGUMENT,
		"null TLS group list was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.Groups = BadGroup;
	Policy.GroupCount = 1u;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"unknown TLS group was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.Groups = DuplicateGroups;
	Policy.GroupCount = 2u;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"duplicate TLS group was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.GroupCount = SIZE_MAX;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"oversized TLS group count was accepted"
	);

	xrtTlsPolicyInit(&Policy);
	Policy.Signatures = NULL;
	Policy.SignatureCount = 1u;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_ARGUMENT,
		"null TLS signature list was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.Signatures = BadSignature;
	Policy.SignatureCount = 1u;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"unknown TLS signature was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.Signatures = DuplicateSignatures;
	Policy.SignatureCount = 2u;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"duplicate TLS signature was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.SignatureCount = SIZE_MAX;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"oversized TLS signature count was accepted"
	);
	xrtTlsPolicyInit(&Policy);
	Policy.Versions = Version13;
	Policy.VersionCount = 1u;
	Policy.Ciphers = Cipher13;
	Policy.CipherCount = 1u;
	Policy.Signatures = Signature12;
	Policy.SignatureCount = 1u;
	testTlsPolicyError(&Policy, XTLS_ERROR_NEGOTIATION,
		"TLS signature without an enabled version was accepted");

	xrtTlsPolicyInit(&Policy);
	Policy.KeySharePolicy = (xtlskeysharepolicy)9;
	testTlsPolicyError(
		&Policy, XTLS_ERROR_NEGOTIATION,
		"unknown TLS key-share policy was accepted"
	);
}



/* 执行 TLS 策略错误路径回归。 */
int main(void)
{
	testTlsPolicyVersionCipherErrors();
	testTlsPolicyGroupSignatureErrors();
	return 0;
}
