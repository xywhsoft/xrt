#include "../test_allocator.h"
#include "../fixtures/x509_distribution_vectors.h"



/* 验证分发点的独立、列表和证书便利路径保持零分配。 */
int main(void)
{
	xx509distributionpoint Point;
	xx509distributioncursor Cursor;
	xx509cert Cert;

	testRequire(testInstallFailAllocator(),
		"X.509 distribution failure allocator install failed");
	testRequire(xrtX509DistributionPointParse(
		(xbytesview) {
			X509_DISTRIBUTION_POINT, sizeof(X509_DISTRIBUTION_POINT)
		}, &Point
	) && xrtX509DistributionInit(
		(xbytesview) {
			X509_DISTRIBUTION_POINTS, sizeof(X509_DISTRIBUTION_POINTS)
		}, &Cursor
	) && (xrtX509DistributionRead(&Cursor, &Point) == X509_VALUE) &&
		(xrtX509DistributionRead(&Cursor, &Point) == X509_VALUE) &&
		(xrtX509DistributionRead(&Cursor, &Point) == X509_DONE),
		"X.509 distribution traversal allocated memory");

	memset(&Cert, 0, sizeof(Cert));
	Cert.Extensions = (xbytesview) {
		X509_DISTRIBUTION_CRL_EXTENSIONS,
		sizeof(X509_DISTRIBUTION_CRL_EXTENSIONS)
	};
	testRequire(xrtX509CrlPoints(&Cert, &Cursor) == X509_VALUE,
		"certificate CRLDistributionPoints allocated memory");
	Cert.Extensions = (xbytesview) {
		X509_DISTRIBUTION_FRESHEST_EXTENSIONS,
		sizeof(X509_DISTRIBUTION_FRESHEST_EXTENSIONS)
	};
	testRequire(xrtX509FreshestCrl(&Cert, &Cursor) == X509_VALUE,
		"certificate FreshestCRL allocated memory");
	printf("[PASS] x509_distribution_oom\n");
	return 0;
}
