#include "../test.h"



/* 验证系统信任库快照、重复装载、建链源和便捷创建入口。 */
int main(void)
{
	xx509store* pStore = xrtX509StoreCreate();
	xx509store* pSystem;
	xx509pathsource Source;
	size_t iAdded = 0;
	size_t iCount;

	testRequire(pStore != NULL, "system X.509 store creation failed");
	if ( !xrtX509StoreAddSystem(pStore, &iAdded) ) {
		const xerror* pError = xrtGetError();

		while ( pError != NULL ) {
			fprintf(stderr,
				"[x509-system-error] kind=%d domain=%s code=%d system=%d operation=%s message=%s data=%s\n",
				(int)xrtErrorKind(pError), xrtErrorDomain(pError),
				(int)xrtErrorCode(pError), (int)xrtErrorSystemCode(pError),
				xrtErrorOperation(pError), xrtErrorMessage(pError),
				xrtErrorData(pError));
			pError = xrtErrorCause(pError);
		}
		testRequire(false, "system X.509 trust import failed");
	}
	testRequire(
		(iAdded != 0) && (xrtX509StoreCount(pStore) == iAdded),
		"system X.509 trust import count mismatch");
	iCount = xrtX509StoreCount(pStore);
	testRequire(xrtX509StoreSource(
		pStore, NULL, 0, &Source
	) && (Source.AnchorCount == iCount) &&
		(Source.Anchors == xrtX509StoreAnchor(pStore, 0)),
		"system X.509 store did not produce a path source");
	xrtClearError();
	iAdded = SIZE_MAX;
	testRequire(xrtX509StoreAddSystem(pStore, &iAdded) &&
		(iAdded == 0) && (xrtX509StoreCount(pStore) == iCount) &&
		(xrtGetError() == NULL),
		"system X.509 trust duplicate import changed the store");
	xrtX509StoreFree(pStore);

	pSystem = xrtX509StoreSystem();
	testRequire((pSystem != NULL) && (xrtX509StoreCount(pSystem) != 0),
		"system X.509 store convenience creation failed");
	xrtX509StoreFree(pSystem);
	printf("[PASS] x509_store_system\n");
	return 0;
}
