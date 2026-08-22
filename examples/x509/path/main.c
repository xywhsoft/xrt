#include <xrt.h>

#include "../../../tests/fixtures/x509_path_legacy.h"

#include <stdio.h>
#include <string.h>



/* 验证一条从目标证书经过中间 CA 到独立信任锚的有序路径。 */
int main(void)
{
	xx509cert Leaf;
	xx509cert Intermediate;
	xx509cert Root;
	xx509anchor Anchor;
	xx509pathconfig Config;
	const xx509cert* Path[2];

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
	Path[0] = &Leaf;
	Path[1] = &Intermediate;
	if ( !xrtX509PathValidate(Path, 2u, &Anchor, &Config) ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}
	printf("certificate path is valid\n");
	return 0;
}
