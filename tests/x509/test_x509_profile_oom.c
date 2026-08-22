#include "../test_allocator.h"
#include "../fixtures/x509_profile_vectors.h"



/* 验证全部 X.509 profile 借用视图在有效路径中不分配内存。 */
int main(void)
{
	xx509cert Cert;
	xx509gencursor Names;
	xx509genname Name;
	xx509oidcursor Oids;
	xbytesview Oid;
	xbytesview KeyId;
	xx509basicconstraints Constraints;
	uint16 iUsage;

	testRequire(testInstallFailAllocator(),
		"X.509 profile failure allocator install failed");
	testRequire(xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Cert
	), "valid X.509 profile certificate allocated memory");
	testRequire((xrtX509SubjectAltName(&Cert, &Names) == X509_VALUE) &&
		(xrtX509GeneralNameRead(&Names, &Name) == X509_VALUE) &&
		(xrtX509GeneralNameRead(&Names, &Name) == X509_VALUE) &&
		(xrtX509GeneralNameRead(&Names, &Name) == X509_DONE),
		"X.509 GeneralNames traversal allocated memory");
	testRequire((xrtX509KeyUsage(&Cert, &iUsage) == X509_VALUE) &&
		(xrtX509BasicConstraints(&Cert, &Constraints) == X509_VALUE),
		"X.509 usage profile allocated memory");
	testRequire((xrtX509ExtendedKeyUsage(&Cert, &Oids) == X509_VALUE) &&
		(xrtX509OidRead(&Oids, &Oid) == X509_VALUE) &&
		(xrtX509OidRead(&Oids, &Oid) == X509_VALUE) &&
		(xrtX509OidRead(&Oids, &Oid) == X509_DONE) &&
		(xrtX509SubjectKeyId(&Cert, &KeyId) == X509_VALUE),
		"X.509 identifier profile allocated memory");
	printf("[PASS] x509_profile_oom\n");
	return 0;
}
