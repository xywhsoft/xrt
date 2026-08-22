#include "../test_allocator.h"
#include "../fixtures/x509_vectors.h"



/* 验证有效证书、Name、Extension 和 SPKI 路径完全不依赖动态分配。 */
int main(void)
{
	xx509cert Cert;
	xx509namecursor NameCursor;
	xx509nameattr Attribute;
	xx509extcursor ExtensionCursor;
	xx509ext Extension;
	xx509pubkey PublicKey;

	testRequire(testInstallFailAllocator(),
		"X.509 failure allocator install failed");
	testRequire(xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Cert
	), "valid X.509 parse allocated memory");
	testRequire(xrtX509NameInit(Cert.Subject, &NameCursor) &&
		(xrtX509NameRead(&NameCursor, &Attribute) == X509_VALUE) &&
		(xrtX509NameRead(&NameCursor, &Attribute) == X509_DONE),
		"X.509 Name traversal allocated memory");
	testRequire(xrtX509ExtensionInit(&Cert, &ExtensionCursor) &&
		(xrtX509ExtensionRead(&ExtensionCursor, &Extension) == X509_VALUE) &&
		xrtX509PublicKey(&Cert, &PublicKey),
		"X.509 extension or public key traversal allocated memory");
	printf("[PASS] x509_parse_oom\n");
	return 0;
}
