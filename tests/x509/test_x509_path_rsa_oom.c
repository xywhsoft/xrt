#include "../test_allocator.h"
#include "../fixtures/x509_legacy_cert.h"
#include "../fixtures/x509_path_legacy.h"



/* 在全失败分配器下验证真实 RSA 证书路径保持零分配。 */
int main(void)
{
	xx509cert Certificate;
	xx509cert Leaf;
	xx509cert Intermediate;
	xx509cert Root;
	xx509anchor Anchor;
	xx509pathconfig Config;
	const xx509cert* Path[2];

	testRequire(testInstallFailAllocator(),
		"X.509 RSA path failure allocator install failed");
	testRequire(xrtX509Parse(
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT), &Certificate
	) && xrtX509Anchor(&Certificate, &Anchor),
		"X.509 RSA path OOM fixture initialization failed");
	memset(&Config, 0, sizeof(Config));
	Config.Time = Certificate.NotBefore;
	Anchor.Certificate = (xbytesview) { NULL, 0 };
	Path[0] = &Certificate;
	testRequire(xrtX509PathValidate(Path, 1u, &Anchor, &Config),
		"valid RSA certificate path required heap allocation");

	testRequire(xrtX509Parse(
		X509_PATH_LEAF, sizeof(X509_PATH_LEAF), &Leaf
	) && xrtX509Parse(
		X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE), &Intermediate
	) && xrtX509Parse(
		X509_PATH_ROOT, sizeof(X509_PATH_ROOT), &Root
	) && xrtX509Anchor(&Root, &Anchor),
		"legacy X.509 RSA path OOM fixture initialization failed");
	memset(&Config, 0, sizeof(Config));
	Config.Time = Leaf.NotBefore;
	Path[0] = &Leaf;
	Path[1] = &Intermediate;
	testRequire(xrtX509PathValidate(Path, 2u, &Anchor, &Config),
		"legacy X.509 RSA path required heap allocation");
	printf("[PASS] x509_path_rsa_oom\n");
	return 0;
}
