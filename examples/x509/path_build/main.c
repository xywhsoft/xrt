#include <xrt.h>

#include "../../../tests/fixtures/x509_path_legacy.h"

#include <stdio.h>
#include <string.h>



/* 从无序候选证书中构建并验证一条到独立信任锚的认证路径。 */
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
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
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
	) ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}
	printf("certificate path contains %zu certificate(s)\n", Result.Count);
	return 0;
}
