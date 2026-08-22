#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_path_legacy.h"

#include <stdio.h>



/* 验证单头文件中的 DER 文件自动装载、所有权与新增计数。 */
int main(void)
{
	str sPath = NULL;
	xfile File = xrtFileTemp(NULL, "xrt-single-x509-", ".der", &sPath);
	xx509store* pStore;
	size_t iAdded = 0;
	int iResult = 1;

	if ( (File == NULL) || (sPath == NULL) || !xrtClose(File) ||
		!xrtFileWriteAll(
			sPath, (xbytesview){ X509_PATH_ROOT, sizeof(X509_PATH_ROOT) }
		) ) {
		goto cleanup_file;
	}
	pStore = xrtX509StoreCreate();
	if ( (pStore != NULL) && xrtX509StoreAddFile(
		pStore, sPath, &iAdded
	) && (iAdded == 1u) && (xrtX509StoreCount(pStore) == 1u) ) {
		iResult = 0;
	}
	xrtX509StoreFree(pStore);

cleanup_file:
	if ( sPath != NULL ) {
		(void)xrtFileDelete(sPath);
		xrtFree(sPath);
	}
	if ( iResult == 0 ) {
		printf("[PASS] single-x509-store-file\n");
	}
	return iResult;
}
