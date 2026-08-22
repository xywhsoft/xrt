#include "../test_allocator.h"
#include "../fixtures/x509_name_vectors.h"



/* 验证常见名称零分配，长名称分配失败时保留结构化错误。 */
int main(void)
{
	testRequire(testInstallFailAllocator(),
		"X.509 Name failure allocator install failed");
	testRequire(xrtX509NameEqual(
		(xbytesview) { X509_NAME_PRINTABLE_SPACE,
			sizeof(X509_NAME_PRINTABLE_SPACE) },
		(xbytesview) { X509_NAME_UTF8_SPACE,
			sizeof(X509_NAME_UTF8_SPACE) }
	) == X509_VALUE, "short X.509 Name comparison allocated memory");
	testRequire(xrtX509NameEqual(
		(xbytesview) { X509_NAME_LONG, sizeof(X509_NAME_LONG) },
		(xbytesview) { X509_NAME_LONG, sizeof(X509_NAME_LONG) }
	) == X509_ERROR && (xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"long X.509 Name allocation failure was not reported");
	printf("[PASS] x509_name_oom\n");
	return 0;
}
