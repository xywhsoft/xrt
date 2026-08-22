#include "../test.h"
#include "../fixtures/x509_path_legacy.h"



/* 创建一个已关闭、由调用方负责删除的排他临时文件。 */
static str testX509StoreFilePath(void)
{
	str sPath = NULL;
	xfile File = xrtFileTemp(NULL, "xrt-x509-store-", ".tmp", &sPath);

	testRequire((File != NULL) && (sPath != NULL),
		"X.509 store temporary file creation failed");
	testRequire(xrtClose(File), "X.509 store temporary file close failed");
	return sPath;
}



/* 写入一张 DER 和一张 PEM 证书，并验证自动识别、去重与可选输出。 */
static void testX509StoreFileFormats(cstr sPath)
{
	xx509store* pStore = xrtX509StoreCreate();
	str sPem;
	size_t iAdded = SIZE_MAX;

	testRequire(pStore != NULL, "X.509 file store creation failed");
	testRequire(xrtFileWriteAll(
		sPath, (xbytesview){ X509_PATH_ROOT, sizeof(X509_PATH_ROOT) }
	), "X.509 DER file write failed");
	testRequire(xrtX509StoreAddFile(pStore, sPath, &iAdded) &&
		(iAdded == 1u) && (xrtX509StoreCount(pStore) == 1u),
		"X.509 DER file import failed");
	iAdded = SIZE_MAX;
	testRequire(xrtX509StoreAddFile(pStore, sPath, &iAdded) &&
		(iAdded == 0) && (xrtX509StoreCount(pStore) == 1u),
		"X.509 DER file duplicate accounting failed");
	testRequire(xrtX509StoreAddFile(pStore, sPath, NULL) &&
		(xrtX509StoreCount(pStore) == 1u),
		"X.509 file import required an optional count output");

	sPem = xrtPemEncodeNew(
		"CERTIFICATE", X509_PATH_INTERMEDIATE,
		sizeof(X509_PATH_INTERMEDIATE)
	);
	testRequire(sPem != NULL, "X.509 PEM file fixture encoding failed");
	testRequire(xrtFileWriteAll(
		sPath, (xbytesview){ (const unsigned char*)sPem, strlen(sPem) }
	), "X.509 PEM file write failed");
	iAdded = SIZE_MAX;
	testRequire(xrtX509StoreAddFile(pStore, sPath, &iAdded) &&
		(iAdded == 1u) && (xrtX509StoreCount(pStore) == 2u),
		"X.509 PEM file import failed");
	xrtFree(sPem);
	xrtX509StoreFree(pStore);
}



/* 文件内容失败必须保留信任库与新增数量，并公开文件层原因链。 */
static void testX509StoreFileFailure(cstr sPath)
{
	static const char Invalid[] =
		"-----BEGIN CERTIFICATE-----\n"
		"QQ==\n"
		"-----END CERTIFICATE-----\n";
	xx509store* pStore = xrtX509StoreCreate();
	size_t iAdded = 77u;

	testRequire(pStore != NULL, "X.509 failing file store creation failed");
	testRequire(xrtX509StoreAdd(
		pStore, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
	) == X509_VALUE, "X.509 failing file store seed failed");
	testRequire(xrtFileWriteAll(
		sPath, (xbytesview){
			(const unsigned char*)Invalid, sizeof(Invalid) - 1u
		}
	), "X.509 malformed PEM file write failed");
	testRequire(!xrtX509StoreAddFile(pStore, sPath, &iAdded) &&
		(iAdded == 77u) && (xrtX509StoreCount(pStore) == 1u) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_TRUST_STORE_FILE) &&
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(xrtErrorCode(xrtErrorCause(xrtGetError())) ==
			X509_ERROR_TRUST_STORE),
		"X.509 malformed file did not preserve the store or cause chain");

	testRequire(xrtFileWriteAll(sPath, (xbytesview){ NULL, 0 }),
		"X.509 empty file write failed");
	iAdded = 91u;
	testRequire(!xrtX509StoreAddFile(pStore, sPath, &iAdded) &&
		(iAdded == 91u) && (xrtX509StoreCount(pStore) == 1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_TRUST_STORE_FILE),
		"X.509 empty file failure contract mismatch");
	iAdded = 103u;
	testRequire(!xrtX509StoreAddFile(
		pStore, "xrt-x509-store-file-does-not-exist.pem", &iAdded
	) && (iAdded == 103u) && (xrtX509StoreCount(pStore) == 1u) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_TRUST_STORE_FILE) &&
		(xrtErrorCause(xrtGetError()) != NULL),
		"X.509 missing file did not preserve its file-system cause");
	xrtX509StoreFree(pStore);
}



/* 执行信任库文件格式、事务、错误与输出原子性测试。 */
int main(void)
{
	str sPath = testX509StoreFilePath();

	testX509StoreFileFormats(sPath);
	testX509StoreFileFailure(sPath);
	testRequire(xrtFileDelete(sPath), "X.509 store test file cleanup failed");
	xrtFree(sPath);
	printf("[PASS] x509_store_file\n");
	return 0;
}
