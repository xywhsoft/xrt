#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_path_legacy.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件信任库可以终止真实 RSA 自动建链。 */
int main(void)
{
	xx509cert Leaf;
	xx509cert Intermediate;
	xx509store* pStore = NULL;
	xx509pathsource Source;
	xx509pathconfig Config;
	xx509pathresult Result;
	const xx509cert* Issuers[1];
	const xx509cert* Path[2];
	int iResult = 1;

	if ( !xrtX509Parse(
		X509_PATH_LEAF, sizeof(X509_PATH_LEAF), &Leaf
	) || !xrtX509Parse(
		X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE), &Intermediate
	) ) {
		return 1;
	}
	pStore = xrtX509StoreCreate();
	if ( (pStore == NULL) || (xrtX509StoreAdd(
		pStore, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
	) != X509_VALUE) ) {
		goto cleanup;
	}
	Issuers[0] = &Intermediate;
	if ( !xrtX509StoreSource(pStore, Issuers, 1u, &Source) ) {
		goto cleanup;
	}
	memset(&Config, 0, sizeof(Config));
	Config.Time = Leaf.NotBefore;
	if ( xrtX509PathBuild(
		&Leaf, &Source, &Config, Path, 2u, &Result
	) && (Result.Count == 2u) && (Path[1] == &Intermediate) ) {
		iResult = 0;
	}

cleanup:
	xrtX509StoreFree(pStore);
	if ( iResult == 0 ) {
		printf("[PASS] single-x509-store-rsa\n");
	}
	return iResult;
}
