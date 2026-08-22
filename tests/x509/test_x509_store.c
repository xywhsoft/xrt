#include "../test.h"
#include "../fixtures/x509_path_legacy.h"



/* 向输出尾部追加一个规范 PEM 块。 */
static size_t testX509StorePemAppend(
	char* sOutput,
	size_t iCapacity,
	size_t iOffset,
	cstr sLabel,
	const void* pData,
	size_t iSize
)
{
	size_t iWritten;

	testRequire(iOffset < iCapacity,
		"X.509 store PEM fixture capacity exhausted");
	testRequire(xrtPemEncode(
		sLabel, pData, iSize,
		sOutput + iOffset, iCapacity - iOffset, &iWritten
	), "X.509 store PEM fixture encoding failed");
	return iOffset + iWritten;
}



/* 验证 DER 所有权、精确去重、索引查询和锚视图。 */
static void testX509StoreDer(void)
{
	xx509store* pStore = xrtX509StoreCreate();
	const xx509cert* pCertificate;
	const xx509anchor* pAnchor;

	testRequire(pStore != NULL, "X.509 store creation failed");
	testRequire(xrtX509StoreAdd(
		pStore, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
	) == X509_VALUE, "X.509 store DER insertion failed");
	xrtClearError();
	testRequire((xrtX509StoreAdd(
		pStore, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
	) == X509_DONE) && (xrtGetError() == NULL) &&
		(xrtX509StoreCount(pStore) == 1u),
		"X.509 store exact duplicate handling failed");
	pCertificate = xrtX509StoreCertificate(pStore, 0);
	pAnchor = xrtX509StoreAnchor(pStore, 0);
	testRequire((pCertificate != NULL) && (pAnchor != NULL) &&
		(pCertificate->Raw.Data != X509_PATH_ROOT) &&
		(pCertificate->Raw.Size == sizeof(X509_PATH_ROOT)) &&
		(memcmp(
			pCertificate->Raw.Data, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
		) == 0) && (pAnchor->Certificate.Data == pCertificate->Raw.Data),
		"X.509 store did not own one coherent certificate copy");
	testRequire((xrtX509StoreCertificate(pStore, 1u) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"X.509 store index range error mismatch");
	xrtX509StoreFree(pStore);
}



/* 验证多块 PEM 导入、非证书块跳过和重复计数。 */
static void testX509StorePem(void)
{
	char Text[8192];
	size_t iSize = 0;
	size_t iAdded = SIZE_MAX;
	xx509store* pStore = xrtX509StoreCreate();

	testRequire(pStore != NULL, "X.509 PEM store creation failed");
	iSize = testX509StorePemAppend(
		Text, sizeof(Text), iSize, "DATA", "ignored", 7u
	);
	iSize = testX509StorePemAppend(
		Text, sizeof(Text), iSize, "CERTIFICATE",
		X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
	);
	iSize = testX509StorePemAppend(
		Text, sizeof(Text), iSize, "CERTIFICATE",
		X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE)
	);
	testRequire(xrtX509StoreAddPem(
		pStore, Text, iSize, &iAdded
	) && (iAdded == 2u) && (xrtX509StoreCount(pStore) == 2u),
		"X.509 store multi-certificate PEM import failed");
	iAdded = SIZE_MAX;
	testRequire(xrtX509StoreAddPem(
		pStore, Text, iSize, &iAdded
	) && (iAdded == 0) && (xrtX509StoreCount(pStore) == 2u),
		"X.509 store PEM duplicate accounting failed");
	testRequire(xrtX509StoreAddPem(
		pStore, Text, iSize, NULL
	) && (xrtX509StoreCount(pStore) == 2u),
		"X.509 store PEM import required an optional count output");
	xrtX509StoreFree(pStore);
}



/* 验证 PEM 中途失败回滚全部新增证书且不发布新增数量。 */
static void testX509StorePemRollback(void)
{
	static const char Invalid[] =
		"-----BEGIN CERTIFICATE-----\n"
		"QQ==\n"
		"-----END CERTIFICATE-----\n";
	char Text[8192];
	size_t iSize = 0;
	size_t iAdded = 77;
	xx509store* pStore = xrtX509StoreCreate();

	testRequire(pStore != NULL, "X.509 rollback store creation failed");
	iSize = testX509StorePemAppend(
		Text, sizeof(Text), iSize, "CERTIFICATE",
		X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
	);
	testRequire((sizeof(Invalid) - 1u) <= sizeof(Text) - iSize,
		"X.509 rollback fixture buffer is too small");
	memcpy(Text + iSize, Invalid, sizeof(Invalid) - 1u);
	iSize += sizeof(Invalid) - 1u;
	testRequire(!xrtX509StoreAddPem(
		pStore, Text, iSize, &iAdded
	) && (iAdded == 77) && (xrtX509StoreCount(pStore) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_TRUST_STORE) &&
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(xrtGetError())), "xrt.x509"
		) == 0),
		"X.509 store malformed PEM transaction was not rolled back");
	xrtX509StoreFree(pStore);
}



/* 验证空库、无证书 PEM 和建链源的失败原子性。 */
static void testX509StoreBoundaries(void)
{
	static const char Other[] =
		"-----BEGIN DATA-----\n"
		"TWFu\n"
		"-----END DATA-----\n";
	xx509store* pStore = xrtX509StoreCreate();
	xx509pathsource Source;
	xx509pathsource Before;
	size_t iAdded = 99;

	testRequire(pStore != NULL, "X.509 boundary store creation failed");
	memset(&Source, 0xA5, sizeof(Source));
	Before = Source;
	testRequire(!xrtX509StoreSource(
		pStore, NULL, 0, &Source
	) && (memcmp(&Source, &Before, sizeof(Source)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"empty X.509 store produced a path source");
	testRequire(!xrtX509StoreAddPem(
		pStore, Other, sizeof(Other) - 1u, &iAdded
	) && (iAdded == 99) &&
		(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_TRUST_STORE),
		"PEM without certificates changed the X.509 store");
	testRequire(xrtX509StoreAdd(
		pStore, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
	) == X509_VALUE, "X.509 source fixture insertion failed");
	testRequire(xrtX509StoreSource(
		pStore, NULL, 0, &Source
	) && (Source.Issuers == NULL) && (Source.IssuerCount == 0) &&
		(Source.Anchors == xrtX509StoreAnchor(pStore, 0)) &&
		(Source.AnchorCount == 1u),
		"X.509 store path source mismatch");
	xrtX509StoreFree(pStore);
}



/* 执行信任库 DER、PEM、所有权、事务和边界测试。 */
int main(void)
{
	testX509StoreDer();
	testX509StorePem();
	testX509StorePemRollback();
	testX509StoreBoundaries();
	printf("[PASS] x509_store\n");
	return 0;
}
