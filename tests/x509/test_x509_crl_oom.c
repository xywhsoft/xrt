#include "../test_allocator.h"
#include "../fixtures/x509_crl_vectors.h"



/* 验证 CRL 解析、条目和扩展遍历的成功路径完全不分配内存。 */
int main(void)
{
	static const uint8 CrlNumberOid[] = { 0x55, 0x1D, 0x14 };
	xx509crl Crl;
	xx509crlcursor Cursor;
	xx509crlentry Entry;
	xx509ext Extension;

	testRequire(testInstallFailAllocator(),
		"CRL failure allocator install failed");
	testRequire(xrtX509CrlParse(
		X509_CRL_V2, sizeof(X509_CRL_V2), &Crl
	), "valid CRL parse allocated memory");
	testRequire(xrtX509CrlEntryInit(&Crl, &Cursor) &&
		(xrtX509CrlEntryRead(&Cursor, &Entry) == X509_VALUE) &&
		(xrtX509CrlEntryRead(&Cursor, &Entry) == X509_DONE) &&
		xrtX509ExtensionListFind(
			Crl.Extensions, CrlNumberOid, sizeof(CrlNumberOid), &Extension
		), "CRL traversal allocated memory");
	printf("[PASS] x509_crl_oom\n");
	return 0;
}
