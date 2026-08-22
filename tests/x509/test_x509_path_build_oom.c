#include "../test_allocator.h"
#include "../fixtures/x509_vectors.h"



/* 验证长路径回溯帧分配失败时不发布部分结果。 */
int main(void)
{
	xx509cert Certificate;
	xx509anchor Anchor;
	xx509pathsource Source;
	xx509pathconfig Config;
	xx509pathresult Result;
	xx509pathresult Before;
	const xx509cert* Path[17];

	testRequire(xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Certificate
	) && xrtX509Anchor(&Certificate, &Anchor),
		"X.509 path builder OOM fixture initialization failed");
	memset(&Source, 0, sizeof(Source));
	Source.Anchors = &Anchor;
	Source.AnchorCount = 1u;
	memset(&Config, 0, sizeof(Config));
	Config.Time = Certificate.NotBefore;
	memset(&Result, 0xA5, sizeof(Result));
	Before = Result;
	testRequire(testInstallFailAllocator(),
		"X.509 path builder failure allocator install failed");
	testRequire(!xrtX509PathBuild(
		&Certificate, &Source, &Config, Path, 17u, &Result
	) && (memcmp(&Result, &Before, sizeof(Result)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"X.509 path builder did not preserve frame allocation failure");
	printf("[PASS] x509_path_build_oom\n");
	return 0;
}
