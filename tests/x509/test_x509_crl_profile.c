#include "../test.h"
#include "../fixtures/x509_crl_vectors.h"
#include "../fixtures/x509_crl_profile_vectors.h"
#include "../fixtures/x509_distribution_vectors.h"



/* 用一项独立扩展列表构造只供 profile 便利入口读取的 CRL 视图。 */
static xx509crl testX509CrlProfileCrl(const uint8* pData, size_t iSize)
{
	xx509crl Crl;

	memset(&Crl, 0, sizeof(Crl));
	Crl.Version = X509_CRL_VERSION_2;
	Crl.Extensions = (xbytesview) { pData, iSize };
	return Crl;
}



/* 用一项独立扩展列表构造只供 profile 便利入口读取的条目视图。 */
static xx509crlentry testX509CrlProfileEntry(
	const uint8* pData,
	size_t iSize
)
{
	xx509crlentry Entry;

	memset(&Entry, 0, sizeof(Entry));
	Entry.Extensions = (xbytesview) { pData, iSize };
	return Entry;
}



/* 验证任意精度 CRL Number、delta base 和 AKI 便利入口。 */
static void testX509CrlProfileIdentifiers(void)
{
	xx509crl Crl;
	xx509authoritykeyid Authority;
	xbytesview Number;

	testRequire(xrtX509CrlNumberParse(
		(xbytesview) {
			X509_CRL_PROFILE_LARGE_NUMBER,
			sizeof(X509_CRL_PROFILE_LARGE_NUMBER)
		}, &Number
	) && (Number.Size == 8u) && (Number.Data[0] == UINT8_C(0x80)),
		"arbitrary precision CRL number parse failed");
	testRequire(xrtX509CrlParse(
		X509_CRL_V2, sizeof(X509_CRL_V2), &Crl
	) && (xrtX509CrlNumber(&Crl, &Number) == X509_VALUE) &&
		(Number.Size == 1u) && (Number.Data[0] == 7u),
		"CRLNumber convenience read failed");

	Crl = testX509CrlProfileCrl(
		X509_CRL_PROFILE_DELTA_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_DELTA_EXTENSIONS)
	);
	testRequire((xrtX509CrlDeltaBase(&Crl, &Number) == X509_VALUE) &&
		(Number.Size == 1u) && (Number.Data[0] == 5u),
		"DeltaCRLIndicator convenience read failed");

	Crl = testX509CrlProfileCrl(
		X509_CRL_PROFILE_AUTHORITY_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_AUTHORITY_EXTENSIONS)
	);
	testRequire((xrtX509CrlAuthorityKeyId(&Crl, &Authority) == X509_VALUE) &&
		Authority.HasKeyId && !Authority.HasIssuer && !Authority.HasSerial &&
		(Authority.KeyId.Size == 3u),
		"CRL AuthorityKeyIdentifier convenience read failed");
}



/* 验证 IDP 的完整名称、相对名称、原因掩码和作用域字段。 */
static void testX509CrlProfileIssuingPoint(void)
{
	static const uint8 CaPoint[] = { 0x30, 0x03, 0x82, 0x01, 0xFF };
	static const uint8 AttributePoint[] = { 0x30, 0x03, 0x85, 0x01, 0xFF };
	xx509crl Crl = testX509CrlProfileCrl(
		X509_CRL_PROFILE_ISSUING_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_ISSUING_EXTENSIONS)
	);
	xx509issuingpoint Point;
	xx509genname Name;

	testRequire((xrtX509CrlIssuingPoint(&Crl, &Point) == X509_VALUE) &&
		Point.HasDistributionPoint && Point.OnlyUserCertificates &&
		!Point.OnlyCaCertificates && Point.HasReasons && Point.Indirect &&
		!Point.OnlyAttributeCertificates &&
		(Point.Reasons == (X509_CRL_REASON_FLAG_KEY_COMPROMISE |
		 X509_CRL_REASON_FLAG_CA_COMPROMISE)) &&
		(Point.DistributionPoint.Type == X509_DISTRIBUTION_FULL_NAME) &&
		(xrtX509GeneralNameRead(
			&Point.DistributionPoint.FullNames, &Name
		) == X509_VALUE) && (Name.Type == X509_NAME_URI) &&
		(Name.Value.Size == 10u) &&
		(xrtX509GeneralNameRead(
			&Point.DistributionPoint.FullNames, &Name
		) == X509_DONE), "IssuingDistributionPoint fullName parse failed");

	testRequire(xrtX509IssuingPointParse(
		(xbytesview) {
			X509_CRL_PROFILE_RELATIVE_POINT,
			sizeof(X509_CRL_PROFILE_RELATIVE_POINT)
		}, &Point
	) && Point.HasDistributionPoint &&
		(Point.DistributionPoint.Type == X509_DISTRIBUTION_RELATIVE_NAME) &&
		(Point.DistributionPoint.Value.Size != 0),
		"IssuingDistributionPoint relative name parse failed");
	testRequire(xrtX509IssuingPointParse(
		(xbytesview) { CaPoint, sizeof(CaPoint) }, &Point
	) && Point.OnlyCaCertificates && !Point.OnlyUserCertificates &&
		!Point.OnlyAttributeCertificates,
		"IssuingDistributionPoint CA scope parse failed");
	testRequire(xrtX509IssuingPointParse(
		(xbytesview) { AttributePoint, sizeof(AttributePoint) }, &Point
	) && Point.OnlyAttributeCertificates && !Point.OnlyUserCertificates &&
		!Point.OnlyCaCertificates,
		"IssuingDistributionPoint attribute scope parse failed");
}



/* 验证完整 CRL 的 FreshestCRL 位置列表和专属字段约束。 */
static void testX509CrlProfileFreshest(void)
{
	xx509crl Crl;
	xx509distributioncursor Cursor;
	xx509distributioncursor BeforeCursor;
	xx509distributionpoint Point;

	Crl = testX509CrlProfileCrl(
		X509_DISTRIBUTION_FRESHEST_EXTENSIONS,
		sizeof(X509_DISTRIBUTION_FRESHEST_EXTENSIONS)
	);
	testRequire((xrtX509CrlFreshest(&Crl, &Cursor) == X509_VALUE) &&
		(xrtX509DistributionRead(&Cursor, &Point) == X509_VALUE) &&
		Point.HasName && !Point.HasReasons && !Point.HasIssuer &&
		(xrtX509DistributionRead(&Cursor, &Point) == X509_DONE),
		"complete CRL FreshestCRL read failed");

	memset(&Cursor, 0xA5, sizeof(Cursor));
	BeforeCursor = Cursor;
	Crl = testX509CrlProfileCrl(
		X509_CRL_PROFILE_BAD_FRESHEST_FIELDS,
		sizeof(X509_CRL_PROFILE_BAD_FRESHEST_FIELDS)
	);
	testRequire((xrtX509CrlFreshest(&Crl, &Cursor) == X509_ERROR) &&
		(memcmp(&Cursor, &BeforeCursor, sizeof(Cursor)) == 0),
		"CRL FreshestCRL accepted reasons or changed output");

	Crl = testX509CrlProfileCrl(
		X509_CRL_PROFILE_DELTA_FRESHEST_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_DELTA_FRESHEST_EXTENSIONS)
	);
	testRequire((xrtX509CrlFreshest(&Crl, &Cursor) == X509_ERROR) &&
		(memcmp(&Cursor, &BeforeCursor, sizeof(Cursor)) == 0),
		"delta CRL accepted FreshestCRL or changed output");

	memset(&Crl.Extensions, 0, sizeof(Crl.Extensions));
	testRequire((xrtX509CrlFreshest(&Crl, &Cursor) == X509_DONE) &&
		(memcmp(&Cursor, &BeforeCursor, sizeof(Cursor)) == 0),
		"absent CRL FreshestCRL changed output");
}



/* 验证条目 Reason、Invalidity Date 和 Certificate Issuer。 */
static void testX509CrlProfileEntries(void)
{
	static const uint8 ValidReasons[] = { 0, 1, 2, 3, 4, 5, 6, 8, 9, 10 };
	uint8 ReasonDer[] = { 0x0A, 0x01, 0x00 };
	xx509crlentry Entry;
	xx509crlreason Reason;
	xx509gencursor Issuer;
	xx509genname Name;
	xtime iTime;
	xtime iExpected;

	Entry = testX509CrlProfileEntry(
		X509_CRL_PROFILE_REASON_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_REASON_EXTENSIONS)
	);
	testRequire((xrtX509CrlEntryReason(&Entry, &Reason) == X509_VALUE) &&
		(Reason == X509_CRL_REASON_KEY_COMPROMISE),
		"CRL entry Reason Code read failed");
	for ( size_t i = 0; i < sizeof(ValidReasons); i++ ) {
		ReasonDer[2] = ValidReasons[i];
		testRequire(xrtX509CrlReasonParse(
			(xbytesview) { ReasonDer, sizeof(ReasonDer) }, &Reason
		) && ((uint8)Reason == ValidReasons[i]),
			"defined CRLReason value was rejected");
	}

	Entry = testX509CrlProfileEntry(
		X509_CRL_PROFILE_INVALIDITY_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_INVALIDITY_EXTENSIONS)
	);
	testRequire(xrtDateTime(2026, 4, 8, 0, 0, 0, 0, &iExpected) &&
		(xrtX509CrlEntryInvalidityDate(&Entry, &iTime) == X509_VALUE) &&
		(iTime == iExpected), "CRL entry Invalidity Date read failed");

	Entry = testX509CrlProfileEntry(
		X509_CRL_PROFILE_ISSUER_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_ISSUER_EXTENSIONS)
	);
	testRequire((xrtX509CrlEntryIssuer(&Entry, &Issuer) == X509_VALUE) &&
		(xrtX509GeneralNameRead(&Issuer, &Name) == X509_VALUE) &&
		(Name.Type == X509_NAME_URI) && (Name.Value.Size == 10u) &&
		(xrtX509GeneralNameRead(&Issuer, &Name) == X509_DONE),
		"CRL entry Certificate Issuer read failed");
}



/* 验证缺省扩展、critical 约束和畸形正文保持输出不变。 */
static void testX509CrlProfileRejects(void)
{
	static const uint8 CriticalNumber[] = {
		0x30, 0x0F, 0x30, 0x0D, 0x06, 0x03, 0x55, 0x1D, 0x14, 0x01, 0x01,
		0xFF, 0x04, 0x03, 0x02, 0x01, 0x07
	};
	static const uint8 NonCriticalDelta[] = {
		0x30, 0x0C, 0x30, 0x0A, 0x06, 0x03, 0x55, 0x1D, 0x1B, 0x04, 0x03,
		0x02, 0x01, 0x05
	};
	static const uint8 CriticalAuthority[] = {
		0x30, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x1D, 0x23, 0x01, 0x01,
		0xFF, 0x04, 0x07, 0x30, 0x05, 0x80, 0x03, 0x01, 0x02, 0x03
	};
	static const uint8 NonCriticalIssuing[] = {
		0x30, 0x25, 0x30, 0x23, 0x06, 0x03, 0x55, 0x1D, 0x1C, 0x04, 0x1C,
		0x30, 0x1A, 0xA0, 0x0E, 0xA0, 0x0C, 0x86, 0x0A, 0x68, 0x74, 0x74,
		0x70, 0x3A, 0x2F, 0x2F, 0x63, 0x72, 0x6C, 0x81, 0x01, 0xFF, 0x83,
		0x02, 0x05, 0x60, 0x84, 0x01, 0xFF
	};
	static const uint8 CriticalReason[] = {
		0x30, 0x0F, 0x30, 0x0D, 0x06, 0x03, 0x55, 0x1D, 0x15, 0x01, 0x01,
		0xFF, 0x04, 0x03, 0x0A, 0x01, 0x01
	};
	static const uint8 CriticalInvalidity[] = {
		0x30, 0x1D, 0x30, 0x1B, 0x06, 0x03, 0x55, 0x1D, 0x18, 0x01, 0x01,
		0xFF, 0x04, 0x11, 0x18, 0x0F, 0x32, 0x30, 0x32, 0x36, 0x30, 0x34,
		0x30, 0x38, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5A
	};
	static const uint8 NonCriticalIssuer[] = {
		0x30, 0x17, 0x30, 0x15, 0x06, 0x03, 0x55, 0x1D, 0x1D, 0x04, 0x0E,
		0x30, 0x0C, 0x86, 0x0A, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F,
		0x63, 0x72, 0x6C
	};
	static const uint8 BadNumbers[][3] = {
		{ 0x02, 0x01, 0xFF },
		{ 0x0A, 0x01, 0x01 }
	};
	static const uint8 BadPoints[][8] = {
		{ 0x30, 0x00 },
		{ 0x30, 0x06, 0x81, 0x01, 0xFF, 0x82, 0x01, 0xFF },
		{ 0x30, 0x03, 0x81, 0x01, 0x00 },
		{ 0x30, 0x04, 0x83, 0x02, 0x05, 0x40 }
	};
	static const size_t BadPointSizes[] = { 2u, 8u, 5u, 6u };
	static const uint8 BadReason[] = { 0x0A, 0x01, 0x07 };
	static const uint8 UtcInvalidity[] = {
		0x17, 0x0D, 0x32, 0x36, 0x30, 0x34, 0x30, 0x38, 0x30, 0x30, 0x30,
		0x30, 0x30, 0x30, 0x5A
	};
	xx509crl Crl;
	xx509issuingpoint Point;
	xx509issuingpoint BeforePoint;
	xx509authoritykeyid Authority;
	xx509authoritykeyid BeforeAuthority;
	xx509crlentry Entry;
	xx509gencursor Issuer;
	xx509gencursor BeforeIssuer;
	xbytesview Number;
	xbytesview BeforeNumber;
	xx509crlreason Reason;
	xtime iTime;

	memset(&Crl, 0, sizeof(Crl));
	memset(&Number, 0xA5, sizeof(Number));
	BeforeNumber = Number;
	testRequire((xrtX509CrlNumber(&Crl, &Number) == X509_DONE) &&
		(memcmp(&Number, &BeforeNumber, sizeof(Number)) == 0),
		"absent CRLNumber changed output");

	Crl = testX509CrlProfileCrl(CriticalNumber, sizeof(CriticalNumber));
	testRequire((xrtX509CrlNumber(&Crl, &Number) == X509_ERROR) &&
		(memcmp(&Number, &BeforeNumber, sizeof(Number)) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_CRL_NUMBER),
		"critical CRLNumber was accepted or changed output");
	Crl = testX509CrlProfileCrl(
		NonCriticalDelta, sizeof(NonCriticalDelta)
	);
	testRequire((xrtX509CrlDeltaBase(&Crl, &Number) == X509_ERROR) &&
		(memcmp(&Number, &BeforeNumber, sizeof(Number)) == 0),
		"non-critical DeltaCRLIndicator was accepted");

	Crl = testX509CrlProfileCrl(
		CriticalAuthority, sizeof(CriticalAuthority)
	);
	memset(&Authority, 0xA5, sizeof(Authority));
	BeforeAuthority = Authority;
	testRequire((xrtX509CrlAuthorityKeyId(
		&Crl, &Authority
	) == X509_ERROR) && (memcmp(
		&Authority, &BeforeAuthority, sizeof(Authority)
	) == 0), "critical CRL AuthorityKeyIdentifier was accepted");
	Crl = testX509CrlProfileCrl(
		NonCriticalIssuing, sizeof(NonCriticalIssuing)
	);
	memset(&Point, 0xA5, sizeof(Point));
	BeforePoint = Point;
	testRequire((xrtX509CrlIssuingPoint(&Crl, &Point) == X509_ERROR) &&
		(memcmp(&Point, &BeforePoint, sizeof(Point)) == 0),
		"non-critical IssuingDistributionPoint was accepted");

	Entry = testX509CrlProfileEntry(CriticalReason, sizeof(CriticalReason));
	Reason = X509_CRL_REASON_AA_COMPROMISE;
	testRequire((xrtX509CrlEntryReason(&Entry, &Reason) == X509_ERROR) &&
		(Reason == X509_CRL_REASON_AA_COMPROMISE),
		"critical Reason Code was accepted");
	Entry = testX509CrlProfileEntry(
		CriticalInvalidity, sizeof(CriticalInvalidity)
	);
	iTime = INT64_C(0x1122334455667788);
	testRequire((xrtX509CrlEntryInvalidityDate(
		&Entry, &iTime
	) == X509_ERROR) && (iTime == INT64_C(0x1122334455667788)),
		"critical Invalidity Date was accepted");
	Entry = testX509CrlProfileEntry(
		NonCriticalIssuer, sizeof(NonCriticalIssuer)
	);
	memset(&Issuer, 0xA5, sizeof(Issuer));
	BeforeIssuer = Issuer;
	testRequire((xrtX509CrlEntryIssuer(&Entry, &Issuer) == X509_ERROR) &&
		(memcmp(&Issuer, &BeforeIssuer, sizeof(Issuer)) == 0),
		"non-critical Certificate Issuer was accepted");

	for ( size_t i = 0; i < 2u; i++ ) {
		testRequire(!xrtX509CrlNumberParse(
			(xbytesview) { BadNumbers[i], sizeof(BadNumbers[i]) }, &Number
		) && (memcmp(&Number, &BeforeNumber, sizeof(Number)) == 0),
			"invalid CRL number changed output");
	}
	for ( size_t i = 0; i < 4u; i++ ) {
		memset(&Point, 0xA5, sizeof(Point));
		BeforePoint = Point;
		testRequire(!xrtX509IssuingPointParse(
			(xbytesview) { BadPoints[i], BadPointSizes[i] }, &Point
		) && (memcmp(&Point, &BeforePoint, sizeof(Point)) == 0),
			"invalid IssuingDistributionPoint changed output");
	}

	Reason = X509_CRL_REASON_AA_COMPROMISE;
	testRequire(!xrtX509CrlReasonParse(
		(xbytesview) { BadReason, sizeof(BadReason) }, &Reason
	) && (Reason == X509_CRL_REASON_AA_COMPROMISE),
		"undefined CRLReason changed output");
	iTime = INT64_C(0x1122334455667788);
	testRequire(!xrtX509CrlInvalidityDateParse(
		(xbytesview) { UtcInvalidity, sizeof(UtcInvalidity) }, &iTime
	) && (iTime == INT64_C(0x1122334455667788)),
		"UTC InvalidityDate changed output");
}



/* 执行 CRL profile 的全部协议与失败原子性测试。 */
int main(void)
{
	testX509CrlProfileIdentifiers();
	testX509CrlProfileIssuingPoint();
	testX509CrlProfileFreshest();
	testX509CrlProfileEntries();
	testX509CrlProfileRejects();
	printf("[PASS] x509_crl_profile\n");
	return 0;
}
