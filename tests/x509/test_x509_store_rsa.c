#include "../test.h"
#include "../fixtures/x509_path_legacy.h"



/* 验证信任库锚与服务端乱序候选可以直接完成真实 RSA 建链。 */
int main(void)
{
	xx509cert Leaf;
	xx509cert Intermediate;
	xx509cert Root;
	xx509store* pStore;
	xx509pathsource Source;
	xx509pathconfig Config;
	xx509pathresult Result;
	const xx509cert* Issuers[2];
	const xx509cert* Path[3];

	testRequire(xrtX509Parse(
		X509_PATH_LEAF, sizeof(X509_PATH_LEAF), &Leaf
	) && xrtX509Parse(
		X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE), &Intermediate
	) && xrtX509Parse(
		X509_PATH_ROOT, sizeof(X509_PATH_ROOT), &Root
	), "X.509 RSA store fixture initialization failed");
	pStore = xrtX509StoreCreate();
	testRequire((pStore != NULL) && (xrtX509StoreAdd(
		pStore, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
	) == X509_VALUE), "X.509 RSA root store initialization failed");
	Issuers[0] = &Root;
	Issuers[1] = &Intermediate;
	testRequire(xrtX509StoreSource(
		pStore, Issuers, 2u, &Source
	), "X.509 RSA store source creation failed");
	memset(&Config, 0, sizeof(Config));
	Config.Time = Leaf.NotBefore;
	testRequire(xrtX509PathBuild(
		&Leaf, &Source, &Config, Path, 3u, &Result
	) && (Result.Count == 2u) &&
		(Result.Anchor == xrtX509StoreAnchor(pStore, 0)) &&
		(Path[0] == &Leaf) && (Path[1] == &Intermediate),
		"X.509 trust store did not terminate the real RSA path");
	xrtX509StoreFree(pStore);
	printf("[PASS] x509_store_rsa\n");
	return 0;
}
