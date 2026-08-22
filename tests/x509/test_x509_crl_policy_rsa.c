#include "../test.h"
#include "../fixtures/x509_crl_vectors.h"



/* 小型测试编码器只处理当前边界向量需要的单字节 DER 长度。 */
static size_t testX509CrlPolicyBegin(uint8* pDer, size_t* pSize, uint8 iTag)
{
	size_t iLength = *pSize + 1u;

	pDer[(*pSize)++] = iTag;
	pDer[(*pSize)++] = 0;
	return iLength;
}



/* 结束一项测试 TLV 并回填小于 128 的正文长度。 */
static void testX509CrlPolicyEnd(
	uint8* pDer,
	size_t iLength,
	size_t iSize
)
{
	size_t iContent = iSize - iLength - 1u;

	testRequire(iContent < 128u, "CRL policy test DER length overflow");
	pDer[iLength] = (uint8)iContent;
}



/* 向测试 DER 追加固定字节。 */
static void testX509CrlPolicyWrite(
	uint8* pDer,
	size_t* pSize,
	const void* pData,
	size_t iSize
)
{
	memcpy(pDer + *pSize, pData, iSize);
	*pSize += iSize;
}



/* 构造只用 cRLIssuer 指向指定 CRL Issuer 的证书 CRLDP 扩展。 */
static size_t testX509CrlPolicyCertificatePoints(
	uint8* pDer,
	xbytesview CrlIssuer
)
{
	static const uint8 Oid[] = { 0x06, 0x03, 0x55, 0x1D, 0x1F };
	size_t iSize = 0;
	size_t iExtensions = testX509CrlPolicyBegin(pDer, &iSize, 0x30);
	size_t iExtension = testX509CrlPolicyBegin(pDer, &iSize, 0x30);
	size_t iOctets;
	size_t iPoints;
	size_t iPoint;
	size_t iIssuer;
	size_t iName;

	testX509CrlPolicyWrite(pDer, &iSize, Oid, sizeof(Oid));
	iOctets = testX509CrlPolicyBegin(pDer, &iSize, 0x04);
	iPoints = testX509CrlPolicyBegin(pDer, &iSize, 0x30);
	iPoint = testX509CrlPolicyBegin(pDer, &iSize, 0x30);
	iIssuer = testX509CrlPolicyBegin(pDer, &iSize, 0xA2);
	iName = testX509CrlPolicyBegin(pDer, &iSize, 0xA4);
	testX509CrlPolicyWrite(
		pDer, &iSize, CrlIssuer.Data, CrlIssuer.Size
	);
	testX509CrlPolicyEnd(pDer, iName, iSize);
	testX509CrlPolicyEnd(pDer, iIssuer, iSize);
	testX509CrlPolicyEnd(pDer, iPoint, iSize);
	testX509CrlPolicyEnd(pDer, iPoints, iSize);
	testX509CrlPolicyEnd(pDer, iOctets, iSize);
	testX509CrlPolicyEnd(pDer, iExtension, iSize);
	testX509CrlPolicyEnd(pDer, iExtensions, iSize);
	return iSize;
}



/* 构造一项 critical CertificateIssuer 条目扩展。 */
static size_t testX509CrlPolicyEntryIssuer(
	uint8* pDer,
	xbytesview CertificateIssuer
)
{
	static const uint8 OidAndCritical[] = {
		0x06, 0x03, 0x55, 0x1D, 0x1D, 0x01, 0x01, 0xFF
	};
	size_t iSize = 0;
	size_t iExtensions = testX509CrlPolicyBegin(pDer, &iSize, 0x30);
	size_t iExtension = testX509CrlPolicyBegin(pDer, &iSize, 0x30);
	size_t iOctets;
	size_t iNames;
	size_t iName;

	testX509CrlPolicyWrite(
		pDer, &iSize, OidAndCritical, sizeof(OidAndCritical)
	);
	iOctets = testX509CrlPolicyBegin(pDer, &iSize, 0x04);
	iNames = testX509CrlPolicyBegin(pDer, &iSize, 0x30);
	iName = testX509CrlPolicyBegin(pDer, &iSize, 0xA4);
	testX509CrlPolicyWrite(
		pDer, &iSize, CertificateIssuer.Data, CertificateIssuer.Size
	);
	testX509CrlPolicyEnd(pDer, iName, iSize);
	testX509CrlPolicyEnd(pDer, iNames, iSize);
	testX509CrlPolicyEnd(pDer, iOctets, iSize);
	testX509CrlPolicyEnd(pDer, iExtension, iSize);
	testX509CrlPolicyEnd(pDer, iExtensions, iSize);
	return iSize;
}



/* 构造两个序列号并验证 CertificateIssuer 对后续条目的继承。 */
static size_t testX509CrlPolicyIndirectEntries(
	uint8* pDer,
	xbytesview CertificateIssuer
)
{
	static const uint8 Time[] = {
		0x17, 0x0D, 0x32, 0x36, 0x30, 0x34, 0x30, 0x38, 0x30, 0x30,
		0x30, 0x30, 0x30, 0x30, 0x5A
	};
	static const uint8 Serial2002[] = { 0x02, 0x02, 0x20, 0x02 };
	static const uint8 Serial2003[] = { 0x02, 0x02, 0x20, 0x03 };
	uint8 Extensions[96];
	size_t iExtensions = testX509CrlPolicyEntryIssuer(
		Extensions, CertificateIssuer
	);
	size_t iSize = 0;
	size_t iEntries = testX509CrlPolicyBegin(pDer, &iSize, 0x30);
	size_t iEntry = testX509CrlPolicyBegin(pDer, &iSize, 0x30);

	testX509CrlPolicyWrite(pDer, &iSize, Serial2002, sizeof(Serial2002));
	testX509CrlPolicyWrite(pDer, &iSize, Time, sizeof(Time));
	testX509CrlPolicyWrite(pDer, &iSize, Extensions, iExtensions);
	testX509CrlPolicyEnd(pDer, iEntry, iSize);
	iEntry = testX509CrlPolicyBegin(pDer, &iSize, 0x30);
	testX509CrlPolicyWrite(pDer, &iSize, Serial2003, sizeof(Serial2003));
	testX509CrlPolicyWrite(pDer, &iSize, Time, sizeof(Time));
	testX509CrlPolicyEnd(pDer, iEntry, iSize);
	testX509CrlPolicyEnd(pDer, iEntries, iSize);
	return iSize;
}



/* unknown critical 回调接受测试私有扩展。 */
static xx509result testX509CrlPolicyCritical(
	const xx509crl* pCrl,
	const xx509crlentry* pEntry,
	const xx509ext* pExtension,
	ptr pUserData
)
{
	size_t* pCalls = (size_t*)pUserData;

	(void)pCrl;
	(void)pEntry;
	if ( (pExtension->Oid.Size == 3u) &&
		(memcmp(pExtension->Oid.Data, "\x2A\x03\x04", 3u) == 0) ) {
		(*pCalls)++;
		return X509_VALUE;
	}
	return X509_DONE;
}



/* 验证旧版真实 RSA CRL 的签名、时间、查询和严格/兼容口径。 */
static void testX509CrlPolicyValidate(void)
{
	static const uint8 Serial2002[] = { 0x20, 0x02 };
	static const uint8 SerialOther[] = { 0x20, 0x03 };
	static const uint8 UnknownCritical[] = {
		0x30, 0x0E, 0x30, 0x0C, 0x06, 0x03, 0x2A, 0x03, 0x04, 0x01, 0x01,
		0xFF, 0x04, 0x02, 0x05, 0x00
	};
	xx509cert Issuer;
	xx509cert Certificate;
	xx509crl Crl;
	xx509crl Empty;
	xx509crl Expired;
	xx509crl Extended;
	xx509crlconfig Config;
	xx509crlvalid Valid;
	xx509crlvalid BeforeValid;
	xx509revocation Revocation;
	xtime iTime;
	size_t iCriticalCalls = 0;

	testRequire(xrtX509Parse(
		X509_CRL_LEGACY_ROOT, sizeof(X509_CRL_LEGACY_ROOT), &Issuer
	), "CRL policy issuer fixture failed to parse");
	testRequire(xrtX509CrlParse(
		X509_CRL_LEGACY_REVOKED, sizeof(X509_CRL_LEGACY_REVOKED), &Crl
	), "CRL policy revoked fixture failed to parse");
	testRequire(xrtX509CrlParse(
		X509_CRL_LEGACY_EMPTY, sizeof(X509_CRL_LEGACY_EMPTY), &Empty
	), "CRL policy empty fixture failed to parse");
	testRequire(xrtX509CrlParse(
		X509_CRL_LEGACY_EXPIRED, sizeof(X509_CRL_LEGACY_EXPIRED), &Expired
	), "CRL policy expired fixture failed to parse");
	testRequire(xrtDateTime(2026, 4, 9, 0, 0, 0, 0, &iTime),
		"CRL policy validation time construction failed");
	memset(&Config, 0, sizeof(Config));
	Config.Time = iTime;
	testRequire(xrtX509CrlValidate(&Crl, &Issuer, &Config, &Valid) &&
		!Valid.Delta && !Valid.HasNumber && !Valid.HasIssuingPoint,
		"legacy CRL compatibility policy validation failed");

	memset(&Certificate, 0, sizeof(Certificate));
	Certificate.Version = X509_VERSION_3;
	Certificate.Issuer = Issuer.Subject;
	Certificate.Serial = (xbytesview) {
		Serial2002, sizeof(Serial2002)
	};
	testRequire((xrtX509CrlCheck(
		&Valid, &Certificate, &Revocation
	) == X509_VALUE) &&
		(Revocation.State == X509_REVOCATION_REVOKED) &&
		!Revocation.HasReason && !Revocation.HasInvalidityDate &&
		(Revocation.CoveredReasons == X509_CRL_REASON_FLAG_ALL),
		"legacy revoked certificate status mismatch");
	Certificate.Serial = (xbytesview) { SerialOther, sizeof(SerialOther) };
	testRequire((xrtX509CrlCheck(
		&Valid, &Certificate, &Revocation
	) == X509_VALUE) && (Revocation.State == X509_REVOCATION_GOOD),
		"legacy unlisted certificate was not good");
	testRequire((xrtX509CrlStatus(
		&Empty, &Issuer, &Certificate, &Config, &Revocation
	) == X509_VALUE) && (Revocation.State == X509_REVOCATION_GOOD),
		"one-shot CRL status failed");

	xrtX509CrlConfigInit(&Config);
	Config.Time = iTime;
	memset(&Valid, 0xA5, sizeof(Valid));
	BeforeValid = Valid;
	testRequire(!xrtX509CrlValidate(&Crl, &Issuer, &Config, &Valid) &&
		(memcmp(&Valid, &BeforeValid, sizeof(Valid)) == 0),
		"strict policy accepted legacy missing fields or changed output");
	memset(&Config, 0, sizeof(Config));
	Config.Time = iTime;
	testRequire(!xrtX509CrlValidate(
		&Expired, &Issuer, &Config, &Valid
	) && (memcmp(&Valid, &BeforeValid, sizeof(Valid)) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_TIME),
		"expired CRL was accepted or changed output");

	Extended = Crl;
	Extended.Extensions = (xbytesview) {
		UnknownCritical, sizeof(UnknownCritical)
	};
	Config.Critical = testX509CrlPolicyCritical;
	Config.UserData = &iCriticalCalls;
	testRequire(xrtX509CrlValidate(
		&Extended, &Issuer, &Config, &Valid
	) && (iCriticalCalls == 1u),
		"custom critical CRL extension was not delegated");
	Config.Critical = NULL;
	memset(&Valid, 0xA5, sizeof(Valid));
	BeforeValid = Valid;
	testRequire(!xrtX509CrlValidate(
		&Extended, &Issuer, &Config, &Valid
	) && (memcmp(&Valid, &BeforeValid, sizeof(Valid)) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_CRITICAL_EXTENSION),
		"unsupported critical CRL extension was accepted");
}



/* 验证 complete/delta 编号条件、更新优先级和 removeFromCRL。 */
static void testX509CrlPolicyDelta(void)
{
	static const uint8 Number5[] = { 5 };
	static const uint8 Number7[] = { 7 };
	static const uint8 Authority1[] = { 0x30, 0x00 };
	static const uint8 Authority2[] = { 0x30, 0x01, 0x00 };
	static const uint8 Serial2002[] = { 0x20, 0x02 };
	static const uint8 SerialOther[] = { 0x20, 0x03 };
	static const uint8 RemovedEntries[] = {
		0x30, 0x23, 0x30, 0x21, 0x02, 0x02, 0x20, 0x02, 0x17, 0x0D,
		0x32, 0x36, 0x30, 0x34, 0x30, 0x38, 0x30, 0x30, 0x30, 0x30,
		0x30, 0x30, 0x5A, 0x30, 0x0C, 0x30, 0x0A, 0x06, 0x03, 0x55,
		0x1D, 0x15, 0x04, 0x03, 0x0A, 0x01, 0x08
	};
	xx509cert Issuer;
	xx509cert Certificate;
	xx509crl BaseCrl;
	xx509crl DeltaCrl;
	xx509crl RemovedCrl;
	xx509crlconfig Config;
	xx509crlvalid Base;
	xx509crlvalid Delta;
	xx509crlvalid Removed;
	xx509crlset Set;
	xx509revocation Revocation;
	xtime iTime;

	testRequire(xrtX509Parse(
		X509_CRL_LEGACY_ROOT, sizeof(X509_CRL_LEGACY_ROOT), &Issuer
	), "delta policy issuer fixture failed to parse");
	testRequire(xrtX509CrlParse(
		X509_CRL_LEGACY_EMPTY, sizeof(X509_CRL_LEGACY_EMPTY), &BaseCrl
	), "delta policy base fixture failed to parse");
	testRequire(xrtX509CrlParse(
		X509_CRL_LEGACY_REVOKED, sizeof(X509_CRL_LEGACY_REVOKED), &DeltaCrl
	), "delta policy delta fixture failed to parse");
	testRequire(xrtDateTime(2026, 4, 9, 0, 0, 0, 0, &iTime),
		"delta policy validation time construction failed");
	memset(&Config, 0, sizeof(Config));
	Config.Time = iTime;
	testRequire(xrtX509CrlValidate(
		&BaseCrl, &Issuer, &Config, &Base),
		"complete CRL source validation failed");
	testRequire(xrtX509CrlValidate(
		&DeltaCrl, &Issuer, &Config, &Delta),
		"delta CRL source validation failed");
	Base.HasNumber = true;
	Base.Number = (xbytesview) { Number5, sizeof(Number5) };
	Delta.Delta = true;
	Delta.HasNumber = true;
	Delta.Number = (xbytesview) { Number7, sizeof(Number7) };
	Delta.BaseNumber = (xbytesview) { Number5, sizeof(Number5) };
	testRequire(xrtX509CrlSetInit(&Set, &Base, &Delta),
		"compatible complete/delta CRLs did not combine");
	Base.HasAuthorityKeyId = true;
	Base.AuthorityKeyIdDer = (xbytesview) {
		Authority1, sizeof(Authority1)
	};
	testRequire(!xrtX509CrlSetInit(&Set, &Base, &Delta),
		"complete/delta CRLs with different AKI presence combined");
	Delta.HasAuthorityKeyId = true;
	Delta.AuthorityKeyIdDer = (xbytesview) {
		Authority2, sizeof(Authority2)
	};
	testRequire(!xrtX509CrlSetInit(&Set, &Base, &Delta),
		"complete/delta CRLs with different AKI values combined");
	Delta.AuthorityKeyIdDer = Base.AuthorityKeyIdDer;
	testRequire(xrtX509CrlSetInit(&Set, &Base, &Delta),
		"complete/delta CRLs with equal AKI did not combine");

	memset(&Certificate, 0, sizeof(Certificate));
	Certificate.Version = X509_VERSION_3;
	Certificate.Issuer = Issuer.Subject;
	Certificate.Serial = (xbytesview) {
		Serial2002, sizeof(Serial2002)
	};
	testRequire((xrtX509CrlSetCheck(
		&Set, &Certificate, &Revocation
	) == X509_VALUE) &&
		(Revocation.State == X509_REVOCATION_REVOKED),
		"delta revocation did not override complete CRL");
	Certificate.Serial = (xbytesview) { SerialOther, sizeof(SerialOther) };
	testRequire((xrtX509CrlSetCheck(
		&Set, &Certificate, &Revocation
	) == X509_VALUE) && (Revocation.State == X509_REVOCATION_GOOD),
		"delta no-change did not fall back to complete CRL");

	RemovedCrl = DeltaCrl;
	RemovedCrl.Version = X509_CRL_VERSION_2;
	RemovedCrl.Revoked = (xbytesview) {
		RemovedEntries, sizeof(RemovedEntries)
	};
	Removed = Delta;
	Removed.Crl = &RemovedCrl;
	testRequire(xrtX509CrlSetInit(&Set, &Base, &Removed),
		"removeFromCRL delta did not combine");
	Certificate.Serial = (xbytesview) {
		Serial2002, sizeof(Serial2002)
	};
	testRequire((xrtX509CrlCheck(
		&Removed, &Certificate, &Revocation
	) == X509_VALUE) &&
		(Revocation.State == X509_REVOCATION_REMOVED) &&
		(xrtX509CrlSetCheck(
			&Set, &Certificate, &Revocation
		) == X509_VALUE) && (Revocation.State == X509_REVOCATION_GOOD),
		"removeFromCRL did not clear combined status");

	Delta.BaseNumber = (xbytesview) { Number7, sizeof(Number7) };
	testRequire(!xrtX509CrlSetInit(&Set, &Base, &Delta) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_CRL_DELTA),
		"incompatible delta numbering was accepted");
}



/* 验证间接 CRL 的 cRLIssuer 作用域和 CertificateIssuer 继承。 */
static void testX509CrlPolicyIndirect(void)
{
	static const uint8 Serial2002[] = { 0x20, 0x02 };
	static const uint8 Serial2003[] = { 0x20, 0x03 };
	uint8 CertificateExtensions[128];
	uint8 Entries[128];
	xx509cert Issuer;
	xx509cert Certificate;
	xx509crl Crl;
	xx509crl Foreign;
	xx509crlvalid Valid;
	xx509revocation Revocation;
	xx509distributioncursor Points;
	xx509distributionpoint Point;
	size_t iCertificateExtensions;
	size_t iEntries;

	testRequire(xrtX509Parse(
		X509_CRL_LEGACY_ROOT, sizeof(X509_CRL_LEGACY_ROOT), &Issuer
	) && xrtX509CrlParse(
		X509_CRL_LEGACY_EMPTY, sizeof(X509_CRL_LEGACY_EMPTY), &Crl
	) && xrtX509CrlParse(
		X509_CRL_V2, sizeof(X509_CRL_V2), &Foreign
	), "indirect CRL fixtures failed to parse");
	iCertificateExtensions = testX509CrlPolicyCertificatePoints(
		CertificateExtensions, Crl.Issuer
	);
	iEntries = testX509CrlPolicyIndirectEntries(Entries, Foreign.Issuer);
	Crl.Version = X509_CRL_VERSION_2;
	Crl.Revoked = (xbytesview) { Entries, iEntries };
	memset(&Valid, 0, sizeof(Valid));
	Valid.Crl = &Crl;
	Valid.Issuer = &Issuer;
	Valid.HasIssuingPoint = true;
	Valid.IssuingPoint.Indirect = true;
	memset(&Certificate, 0, sizeof(Certificate));
	Certificate.Version = X509_VERSION_3;
	Certificate.Issuer = Foreign.Issuer;
	Certificate.Extensions = (xbytesview) {
		CertificateExtensions, iCertificateExtensions
	};
	Certificate.Serial = (xbytesview) {
		Serial2002, sizeof(Serial2002)
	};
	testRequire((xrtX509CrlPoints(&Certificate, &Points) == X509_VALUE) &&
		(xrtX509DistributionRead(&Points, &Point) == X509_VALUE),
		"indirect CRL distribution point failed to parse");
	Valid.IssuingPoint.HasDistributionPoint = true;
	Valid.IssuingPoint.DistributionPoint.Type = X509_DISTRIBUTION_FULL_NAME;
	Valid.IssuingPoint.DistributionPoint.FullNames = Point.Issuer;
	testRequire((xrtX509CrlCheck(
		&Valid, &Certificate, &Revocation
	) == X509_VALUE) &&
		(Revocation.State == X509_REVOCATION_REVOKED),
		"IDP name did not match an omitted DP name through cRLIssuer");
	Certificate.Serial = (xbytesview) {
		Serial2003, sizeof(Serial2003)
	};
	testRequire((xrtX509CrlCheck(
		&Valid, &Certificate, &Revocation
	) == X509_VALUE) &&
		(Revocation.State == X509_REVOCATION_REVOKED),
		"indirect CRL CertificateIssuer inheritance failed");
	Valid.IssuingPoint.Indirect = false;
	testRequire(xrtX509CrlCheck(
		&Valid, &Certificate, &Revocation
	) == X509_DONE, "cRLIssuer applied without indirectCRL");
	Valid.IssuingPoint.Indirect = true;

	memset(&Certificate.Extensions, 0, sizeof(Certificate.Extensions));
	testRequire(xrtX509CrlCheck(
		&Valid, &Certificate, &Revocation
	) == X509_DONE, "indirect CRL applied without certificate cRLIssuer");
}



/* 验证 onlyContainsUserCerts 接受没有 BasicConstraints 的旧版终端证书。 */
static void testX509CrlPolicyUserScope(void)
{
	static const uint8 Name[] = {
		0x30, 0x0E, 0x31, 0x0C, 0x30, 0x0A, 0x06, 0x03, 0x55, 0x04,
		0x03, 0x0C, 0x03, 0x43, 0x41, 0x31
	};
	static const uint8 FullNames[] = {
		0x30, 0x12, 0xA4, 0x10,
		0x30, 0x0E, 0x31, 0x0C, 0x30, 0x0A, 0x06, 0x03, 0x55, 0x04,
		0x03, 0x0C, 0x03, 0x43, 0x41, 0x31
	};
	static const uint8 Serial[] = { 1 };
	xx509cert Issuer;
	xx509cert Certificate;
	xx509crl Crl;
	xx509crlvalid Valid;
	xx509revocation Revocation;

	memset(&Issuer, 0, sizeof(Issuer));
	memset(&Certificate, 0, sizeof(Certificate));
	memset(&Crl, 0, sizeof(Crl));
	memset(&Valid, 0, sizeof(Valid));
	Issuer.Subject = (xbytesview) { Name, sizeof(Name) };
	Certificate.Version = X509_VERSION_1;
	Certificate.Issuer = Issuer.Subject;
	Certificate.Serial = (xbytesview) { Serial, sizeof(Serial) };
	Crl.Version = X509_CRL_VERSION_2;
	Crl.Issuer = Issuer.Subject;
	Valid.Crl = &Crl;
	Valid.Issuer = &Issuer;
	Valid.HasIssuingPoint = true;
	Valid.IssuingPoint.OnlyUserCertificates = true;
	Valid.IssuingPoint.HasDistributionPoint = true;
	Valid.IssuingPoint.DistributionPoint.Type = X509_DISTRIBUTION_FULL_NAME;
	testRequire(xrtX509GeneralNameInit(
		(xbytesview) { FullNames, sizeof(FullNames) },
		&Valid.IssuingPoint.DistributionPoint.FullNames
	), "default distribution point name failed to parse");
	testRequire((xrtX509CrlCheck(
		&Valid, &Certificate, &Revocation
	) == X509_VALUE) && (Revocation.State == X509_REVOCATION_GOOD),
		"v1 user certificate or default distribution fallback failed");
}



/* 验证多份原因分段只在覆盖完整或确认撤销后产生最终状态。 */
static void testX509CrlPolicyAggregate(void)
{
	xx509revocationcheck Check;
	xx509revocation Status;
	xx509revocation Result;
	xx509revocation Before;

	xrtX509RevocationInit(&Check);
	memset(&Status, 0, sizeof(Status));
	Status.State = X509_REVOCATION_GOOD;
	Status.CoveredReasons = X509_CRL_REASON_FLAG_KEY_COMPROMISE;
	memset(&Result, 0xA5, sizeof(Result));
	Before = Result;
	testRequire((xrtX509RevocationUpdate(&Check, &Status) == X509_DONE) &&
		(xrtX509RevocationResult(&Check, &Result) == X509_DONE) &&
		(memcmp(&Result, &Before, sizeof(Result)) == 0),
		"partial revocation reasons produced a final status");
	Status.CoveredReasons = (uint16)(
		X509_CRL_REASON_FLAG_ALL & ~X509_CRL_REASON_FLAG_KEY_COMPROMISE
	);
	testRequire((xrtX509RevocationUpdate(&Check, &Status) == X509_VALUE) &&
		(xrtX509RevocationResult(&Check, &Result) == X509_VALUE) &&
		(Result.State == X509_REVOCATION_GOOD) &&
		(Result.CoveredReasons == X509_CRL_REASON_FLAG_ALL),
		"complete revocation reason coverage did not produce GOOD");

	xrtX509RevocationInit(&Check);
	Status.State = X509_REVOCATION_REVOKED;
	Status.CoveredReasons = X509_CRL_REASON_FLAG_CA_COMPROMISE;
	testRequire((xrtX509RevocationUpdate(&Check, &Status) == X509_VALUE) &&
		(xrtX509RevocationResult(&Check, &Result) == X509_VALUE) &&
		(Result.State == X509_REVOCATION_REVOKED),
		"partial reason revocation did not determine final status");
	xrtX509RevocationInit(&Check);
	Status.State = X509_REVOCATION_REMOVED;
	testRequire((xrtX509RevocationUpdate(&Check, &Status) == X509_ERROR) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_REVOCATION),
		"uncombined removeFromCRL entered the status aggregator");
}



/* 执行 CRL policy 的真实签名和组合语义测试。 */
int main(void)
{
	testX509CrlPolicyValidate();
	testX509CrlPolicyDelta();
	testX509CrlPolicyIndirect();
	testX509CrlPolicyUserScope();
	testX509CrlPolicyAggregate();
	printf("[PASS] x509_crl_policy_rsa\n");
	return 0;
}
