#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_path_legacy.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件可以从真实 RSA 候选集合自动构建认证路径。 */
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

	if ( !xrtX509Parse(
		X509_PATH_LEAF, sizeof(X509_PATH_LEAF), &Leaf
	) || !xrtX509Parse(
		X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE), &Intermediate
	) || !xrtX509Parse(
		X509_PATH_ROOT, sizeof(X509_PATH_ROOT), &Root
	) || !xrtX509Anchor(&Root, &Anchor) ) {
		return 1;
	}
	memset(&Config, 0, sizeof(Config));
	Config.Time = Leaf.NotBefore;
	Issuers[0] = &Root;
	Issuers[1] = &Intermediate;
	Source.Issuers = Issuers;
	Source.IssuerCount = 2u;
	Source.Anchors = &Anchor;
	Source.AnchorCount = 1u;
	if ( !xrtX509PathBuild(
		&Leaf, &Source, &Config, Path, 3u, &Result
	) || (Result.Count != 2u) || (Result.Anchor != &Anchor) ||
		(Path[1] != &Intermediate) ) {
		return 1;
	}
	printf("[PASS] single-x509-path-build-rsa\n");
	return 0;
}
