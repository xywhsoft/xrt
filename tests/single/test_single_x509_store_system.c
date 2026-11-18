#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>

#if !defined(_WIN32) && !defined(_WIN64) && !defined(__APPLE__)
#include "../fixtures/x509_path_legacy.h"

static bool testPosixSystemDirectories(void)
{
	static const uint8 Invalid[] = "-----BEGIN CERTIFICATE-----\nQQ==\n-----END CERTIFICATE-----\n";
	char sDirectory[] = "./xrt-system-ca-XXXXXX";
	xx509store* pStore = xrtX509StoreCreate();
	char sInvalid[sizeof(sDirectory) + 11u];
	char sValid[sizeof(sDirectory) + 11u];
	const char* Paths[] = {sDirectory};
	bool bFound = false;
	bool bSuccess = false;

	if ( (pStore == NULL) || (mkdtemp(sDirectory) == NULL) ) {
		xrtX509StoreFree(pStore);
		return false;
	}
	snprintf(sInvalid, sizeof(sInvalid), "%s/00000000.0", sDirectory);
	snprintf(sValid, sizeof(sValid), "%s/00000001.0", sDirectory);
	bSuccess =
		xrtFileWriteAll(sInvalid, (xbytesview){Invalid, sizeof(Invalid) - 1u}) &&
		!__xrtX509StoreSystemDirectories(pStore, Paths, 1, &bFound) &&
		!bFound && (xrtX509StoreCount(pStore) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_TRUST_STORE_SYSTEM) &&
		(xrtErrorFind(xrtGetError(), "xrt.x509", X509_ERROR_TRUST_STORE_FILE) != NULL);
	xrtClearError();
	bSuccess = bSuccess &&
		!__xrtX509StoreSystemDirectoryList(pStore, sDirectory, &bFound) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_TRUST_STORE_FILE) &&
		!bFound && (xrtX509StoreCount(pStore) == 0);
	xrtClearError();
	bSuccess = bSuccess &&
		xrtFileWriteAll(sValid, (xbytesview){X509_PATH_ROOT, sizeof(X509_PATH_ROOT)}) &&
		__xrtX509StoreSystemDirectories(pStore, Paths, 1, &bFound) &&
		bFound && (xrtX509StoreCount(pStore) == 1) && (xrtGetError() == NULL);
	remove(sInvalid);
	remove(sValid);
	xrtDirRemove(sDirectory);
	xrtX509StoreFree(pStore);
	return bSuccess;
}
#endif



/* 验证单头文件可创建一份拥有式系统信任库快照。 */
int main(void)
{
	xx509store* pStore = xrtX509StoreSystem();

	if ( (pStore == NULL) || (xrtX509StoreCount(pStore) == 0) ) {
		xrtX509StoreFree(pStore);
		return 1;
	}
	xrtX509StoreFree(pStore);
#if !defined(_WIN32) && !defined(_WIN64) && !defined(__APPLE__)
	if ( !testPosixSystemDirectories() ) {
		fprintf(stderr, "[FAIL] POSIX system trust directory isolation\n");
		return 1;
	}
#endif
	printf("[PASS] single-x509-store-system\n");
	return 0;
}
