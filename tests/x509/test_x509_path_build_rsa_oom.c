#include "../test_allocator.h"
#include "../fixtures/x509_path_legacy.h"



/* 在全失败分配器下验证常见真实 RSA 自动建链保持零分配。 */
int main(void)
{
	xx509cert Leaf;
	xx509cert Intermediate;
	xx509cert Root;
	xx509anchor Anchor;
	xx509pathsource Source;
	xx509pathconfig Config;
	xx509pathresult Result;
	const xx509cert* Issuers[2];
	const xx509cert* Path[3];

	testRequire(testInstallFailAllocator(),
		"X.509 RSA builder failure allocator install failed");
	testRequire(xrtX509Parse(
		X509_PATH_LEAF, sizeof(X509_PATH_LEAF), &Leaf
	) && xrtX509Parse(
		X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE), &Intermediate
	) && xrtX509Parse(
		X509_PATH_ROOT, sizeof(X509_PATH_ROOT), &Root
	) && xrtX509Anchor(&Root, &Anchor),
		"X.509 RSA builder OOM fixture initialization failed");
	memset(&Config, 0, sizeof(Config));
	Config.Time = Leaf.NotBefore;
	Issuers[0] = &Root;
	Issuers[1] = &Intermediate;
	Source.Issuers = Issuers;
	Source.IssuerCount = 2u;
	Source.Anchors = &Anchor;
	Source.AnchorCount = 1u;
	testRequire(xrtX509PathBuild(
		&Leaf, &Source, &Config, Path, 3u, &Result
	) && (Result.Count == 2u) && (Result.Anchor == &Anchor) &&
		(Path[1] == &Intermediate),
		"common X.509 RSA path building required heap allocation");
	printf("[PASS] x509_path_build_rsa_oom\n");
	return 0;
}
