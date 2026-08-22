#include "../test_allocator.h"



/* 验证错误对象分配失败时保留标准内存不足错误。 */
int main(void)
{
	static const uint8 Byte = 1;
	xx509signature Scheme;
	xx509pubkey PublicKey;

	memset(&Scheme, 0, sizeof(Scheme));
	memset(&PublicKey, 0, sizeof(PublicKey));
	Scheme.Type = X509_SIGNATURE_RSA_PKCS1;
	Scheme.Hash = X509_HASH_SHA256;
	PublicKey.Type = X509_KEY_RSA;
	testRequire(testInstallFailAllocator(),
		"X.509 verify failure allocator install failed");
	testRequire(!xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) { NULL, 0 },
		(xbytesview) { &Byte, 1u },
		&PublicKey
	) && (xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"X.509 verification lost allocation failure");
	printf("[PASS] x509_verify_oom\n");
	return 0;
}
