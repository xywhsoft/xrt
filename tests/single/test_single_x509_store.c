#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_path_legacy.h"

#include <stdio.h>



/* 验证单头文件中的拥有式信任库、去重和锚查询。 */
int main(void)
{
	xx509store* pStore = xrtX509StoreCreate();
	const xx509anchor* pAnchor;

	if ( (pStore == NULL) || (xrtX509StoreAdd(
		pStore, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
	) != X509_VALUE) || (xrtX509StoreAdd(
		pStore, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
	) != X509_DONE) || (xrtX509StoreCount(pStore) != 1u) ) {
		xrtX509StoreFree(pStore);
		return 1;
	}
	pAnchor = xrtX509StoreAnchor(pStore, 0);
	if ( (pAnchor == NULL) ||
		(pAnchor->Certificate.Size != sizeof(X509_PATH_ROOT)) ) {
		xrtX509StoreFree(pStore);
		return 1;
	}
	xrtX509StoreFree(pStore);
	printf("[PASS] single-x509-store\n");
	return 0;
}
