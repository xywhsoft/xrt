#include "../test.h"
#include "../fixtures/x509_distribution_vectors.h"



/* 验证独立分发点解析和列表游标的全部字段。 */
static void testX509DistributionValues(void)
{
	xx509distributionpoint Point;
	xx509distributioncursor Cursor;
	xx509genname Name;

	testRequire(xrtX509DistributionPointParse(
		(xbytesview) {
			X509_DISTRIBUTION_POINT, sizeof(X509_DISTRIBUTION_POINT)
		}, &Point
	) && Point.HasName && Point.HasReasons && !Point.HasIssuer &&
		(Point.Name.Type == X509_DISTRIBUTION_FULL_NAME) &&
		(Point.Reasons == (X509_CRL_REASON_FLAG_KEY_COMPROMISE |
		 X509_CRL_REASON_FLAG_CA_COMPROMISE)) &&
		(xrtX509GeneralNameRead(&Point.Name.FullNames, &Name) == X509_VALUE) &&
		(Name.Type == X509_NAME_URI) && (Name.Value.Size == 10u) &&
		(xrtX509GeneralNameRead(&Point.Name.FullNames, &Name) == X509_DONE),
		"standalone DistributionPoint parse failed");

	testRequire(xrtX509DistributionInit(
		(xbytesview) {
			X509_DISTRIBUTION_POINTS, sizeof(X509_DISTRIBUTION_POINTS)
		}, &Cursor
	) && (xrtX509DistributionRead(&Cursor, &Point) == X509_VALUE) &&
		Point.HasName && Point.HasReasons && !Point.HasIssuer &&
		(xrtX509DistributionRead(&Cursor, &Point) == X509_VALUE) &&
		!Point.HasName && !Point.HasReasons && Point.HasIssuer &&
		(xrtX509GeneralNameRead(&Point.Issuer, &Name) == X509_VALUE) &&
		(Name.Type == X509_NAME_URI) &&
		(xrtX509GeneralNameRead(&Point.Issuer, &Name) == X509_DONE) &&
		(xrtX509DistributionRead(&Cursor, &Point) == X509_DONE),
		"CRLDistributionPoints traversal failed");

	testRequire(xrtX509DistributionPointParse(
		(xbytesview) {
			X509_DISTRIBUTION_RELATIVE_POINT,
			sizeof(X509_DISTRIBUTION_RELATIVE_POINT)
		}, &Point
	) && Point.HasName && !Point.HasReasons && !Point.HasIssuer &&
		(Point.Name.Type == X509_DISTRIBUTION_RELATIVE_NAME) &&
		(Point.Name.Value.Size != 0),
		"relative DistributionPointName parse failed");
}



/* 验证证书 CRLDistributionPoints 和 FreshestCRL 便利入口。 */
static void testX509DistributionCertificate(void)
{
	xx509cert Cert;
	xx509distributioncursor Cursor;
	xx509distributionpoint Point;

	memset(&Cert, 0, sizeof(Cert));
	Cert.Extensions = (xbytesview) {
		X509_DISTRIBUTION_CRL_EXTENSIONS,
		sizeof(X509_DISTRIBUTION_CRL_EXTENSIONS)
	};
	testRequire((xrtX509CrlPoints(&Cert, &Cursor) == X509_VALUE) &&
		(xrtX509DistributionRead(&Cursor, &Point) == X509_VALUE) &&
		Point.HasName && !Point.HasReasons && !Point.HasIssuer &&
		(xrtX509DistributionRead(&Cursor, &Point) == X509_DONE),
		"certificate CRLDistributionPoints read failed");

	Cert.Extensions = (xbytesview) {
		X509_DISTRIBUTION_FRESHEST_EXTENSIONS,
		sizeof(X509_DISTRIBUTION_FRESHEST_EXTENSIONS)
	};
	testRequire((xrtX509FreshestCrl(&Cert, &Cursor) == X509_VALUE) &&
		(xrtX509DistributionRead(&Cursor, &Point) == X509_VALUE) &&
		Point.HasName, "certificate FreshestCRL read failed");
}



/* 验证空列表、reasons-only、字段顺序和 critical profile 边界。 */
static void testX509DistributionRejects(void)
{
	static const uint8 EmptyList[] = { 0x30, 0x00 };
	static const uint8 EmptyPoint[] = { 0x30, 0x00 };
	static const uint8 ReasonsOnly[] = {
		0x30, 0x04, 0x81, 0x02, 0x06, 0x40
	};
	static const uint8 OutOfOrder[] = {
		0x30, 0x14, 0x81, 0x02, 0x06, 0x40, 0xA0, 0x0E, 0xA0, 0x0C,
		0x86, 0x0A, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x63,
		0x72, 0x6C
	};
	static const uint8 BadReason[] = {
		0x30, 0x14, 0xA0, 0x0E, 0xA0, 0x0C, 0x86, 0x0A, 0x68, 0x74,
		0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x63, 0x72, 0x6C, 0x81, 0x02,
		0x05, 0x40
	};
	const xbytesview BadPoints[] = {
		{ EmptyPoint, sizeof(EmptyPoint) },
		{ ReasonsOnly, sizeof(ReasonsOnly) },
		{ OutOfOrder, sizeof(OutOfOrder) },
		{ BadReason, sizeof(BadReason) }
	};
	xx509distributionpoint Point;
	xx509distributionpoint BeforePoint;
	xx509distributioncursor Cursor;
	xx509distributioncursor BeforeCursor;
	xx509cert Cert;

	memset(&Cursor, 0xA5, sizeof(Cursor));
	BeforeCursor = Cursor;
	testRequire(!xrtX509DistributionInit(
		(xbytesview) { EmptyList, sizeof(EmptyList) }, &Cursor
	) && (memcmp(&Cursor, &BeforeCursor, sizeof(Cursor)) == 0),
		"empty CRLDistributionPoints changed output");

	for ( size_t i = 0; i < sizeof(BadPoints) / sizeof(BadPoints[0]); i++ ) {
		memset(&Point, 0xA5, sizeof(Point));
		BeforePoint = Point;
		testRequire(!xrtX509DistributionPointParse(
			BadPoints[i], &Point
		) && (memcmp(&Point, &BeforePoint, sizeof(Point)) == 0) &&
			(xrtErrorCode(xrtGetError()) == X509_ERROR_DISTRIBUTION_POINT),
			"invalid DistributionPoint was accepted or changed output");
	}

	memset(&Cert, 0, sizeof(Cert));
	Cert.Extensions = (xbytesview) {
		X509_DISTRIBUTION_CRITICAL_FRESHEST_EXTENSIONS,
		sizeof(X509_DISTRIBUTION_CRITICAL_FRESHEST_EXTENSIONS)
	};
	memset(&Cursor, 0xA5, sizeof(Cursor));
	BeforeCursor = Cursor;
	testRequire((xrtX509FreshestCrl(&Cert, &Cursor) == X509_ERROR) &&
		(memcmp(&Cursor, &BeforeCursor, sizeof(Cursor)) == 0),
		"critical FreshestCRL was accepted or changed output");

	memset(&Cert.Extensions, 0, sizeof(Cert.Extensions));
	testRequire((xrtX509CrlPoints(&Cert, &Cursor) == X509_DONE) &&
		(memcmp(&Cursor, &BeforeCursor, sizeof(Cursor)) == 0) &&
		(xrtX509FreshestCrl(&Cert, &Cursor) == X509_DONE) &&
		(memcmp(&Cursor, &BeforeCursor, sizeof(Cursor)) == 0),
		"absent distribution extension changed output");
}



/* 执行 X.509 分发点协议层测试。 */
int main(void)
{
	testX509DistributionValues();
	testX509DistributionCertificate();
	testX509DistributionRejects();
	printf("[PASS] x509_distribution\n");
	return 0;
}
