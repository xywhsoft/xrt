#include "../test.h"
#include "../fixtures/x509_crl_vectors.h"



/* 验证 v2 CRL 的固定字段、时间窗口、扩展和签名借用视图。 */
static void testX509CrlV2(void)
{
	static const uint8 CrlNumberOid[] = { 0x55, 0x1D, 0x14 };
	xx509crl Crl;
	xx509ext Extension;
	xtime iThisUpdate;
	xtime iNextUpdate;

	testRequire(xrtX509CrlParse(
		X509_CRL_V2, sizeof(X509_CRL_V2), &Crl
	), "valid v2 CRL parse failed");
	testRequire((Crl.Version == X509_CRL_VERSION_2) &&
		(Crl.Raw.Data == X509_CRL_V2) && (Crl.Tbs.Size != 0) &&
		(Crl.Issuer.Size != 0) && Crl.HasNextUpdate &&
		(Crl.Revoked.Size != 0) && (Crl.Extensions.Size != 0) &&
		(Crl.Signature.Size == 64u), "v2 CRL field views mismatch");
	testRequire(xrtDateTime(2026, 4, 8, 0, 0, 0, 0, &iThisUpdate) &&
		xrtDateTime(2036, 4, 5, 0, 0, 0, 0, &iNextUpdate) &&
		(Crl.ThisUpdate == iThisUpdate) && (Crl.NextUpdate == iNextUpdate) &&
		xrtX509CrlValidAt(&Crl, iThisUpdate) &&
		xrtX509CrlValidAt(&Crl, iNextUpdate) &&
		!xrtX509CrlValidAt(&Crl, iThisUpdate - 1) &&
		!xrtX509CrlValidAt(&Crl, iNextUpdate + 1),
		"v2 CRL time window mismatch");
	testRequire(xrtX509ExtensionListFind(
		Crl.Extensions, CrlNumberOid, sizeof(CrlNumberOid), &Extension
	) && !Extension.Critical && (Extension.Value.Size == 3u) &&
		(memcmp(Extension.Value.Data, "\x02\x01\x07", 3u) == 0),
		"v2 CRL number extension mismatch");
}



/* 验证条目游标、条目扩展、精确查找和证书便捷入口。 */
static void testX509CrlEntries(void)
{
	static const uint8 ReasonOid[] = { 0x55, 0x1D, 0x15 };
	static const uint8 Serial[] = { 0x20, 0x02 };
	static const uint8 Missing[] = { 0x20, 0x03 };
	xx509crl Crl;
	xx509crlcursor Cursor;
	xx509crlentry Entry;
	xx509crlentry Before;
	xx509ext Extension;
	xx509cert Cert;

	testRequire(xrtX509CrlParse(
		X509_CRL_V2, sizeof(X509_CRL_V2), &Crl
	) && xrtX509CrlEntryInit(&Crl, &Cursor) &&
		(xrtX509CrlEntryRead(&Cursor, &Entry) == X509_VALUE),
		"v2 CRL entry traversal failed");
	testRequire((Entry.Serial.Size == sizeof(Serial)) &&
		(memcmp(Entry.Serial.Data, Serial, sizeof(Serial)) == 0) &&
		(Entry.Extensions.Size != 0) && xrtX509ExtensionListFind(
			Entry.Extensions, ReasonOid, sizeof(ReasonOid), &Extension
		) && (Extension.Value.Size == 3u) &&
		(memcmp(Extension.Value.Data, "\x0A\x01\x01", 3u) == 0),
		"v2 CRL entry fields mismatch");
	memset(&Entry, 0xA5, sizeof(Entry));
	Before = Entry;
	testRequire((xrtX509CrlEntryRead(&Cursor, &Entry) == X509_DONE) &&
		(memcmp(&Entry, &Before, sizeof(Entry)) == 0),
		"CRL entry terminal read changed output");
	testRequire(xrtX509CrlFind(
		&Crl, (xbytesview) { Serial, sizeof(Serial) }, &Entry
	) == X509_VALUE, "CRL serial lookup missed a revoked entry");
	memset(&Entry, 0xA5, sizeof(Entry));
	Before = Entry;
	testRequire((xrtX509CrlFind(
		&Crl, (xbytesview) { Missing, sizeof(Missing) }, &Entry
	) == X509_DONE) && (memcmp(&Entry, &Before, sizeof(Entry)) == 0),
		"CRL missing serial changed output or returned an error");
	memset(&Cert, 0, sizeof(Cert));
	Cert.Serial = (xbytesview) { Serial, sizeof(Serial) };
	testRequire(xrtX509CrlRevokes(&Crl, &Cert, NULL) == X509_VALUE,
		"CRL certificate convenience lookup failed");
}



/* 验证 v1 和旧版真实 RSA CRL 资产继续由严格新底座接受。 */
static void testX509CrlLegacy(void)
{
	xx509crl V1;
	xx509crl Revoked;
	xx509crl Empty;
	xx509crl Expired;
	xx509crlcursor Cursor;
	xx509crlentry Entry;
	xtime iExpired;

	testRequire(xrtX509CrlParse(
		X509_CRL_V1, sizeof(X509_CRL_V1), &V1
	) && (V1.Version == X509_CRL_VERSION_1) &&
		!V1.HasNextUpdate && (V1.Revoked.Size == 0) &&
		(V1.Extensions.Size == 0) && xrtX509CrlEntryInit(&V1, &Cursor) &&
		(xrtX509CrlEntryRead(&Cursor, &Entry) == X509_DONE),
		"minimal v1 CRL parse failed");
	testRequire(xrtX509CrlParse(
		X509_CRL_LEGACY_REVOKED, sizeof(X509_CRL_LEGACY_REVOKED), &Revoked
	) && xrtX509CrlEntryInit(&Revoked, &Cursor) &&
		(xrtX509CrlEntryRead(&Cursor, &Entry) == X509_VALUE) &&
		(Entry.Serial.Size == 2u) && (Entry.Serial.Data[0] == 0x20) &&
		(Entry.Serial.Data[1] == 0x02),
		"legacy revoked CRL asset failed");
	testRequire(xrtX509CrlParse(
		X509_CRL_LEGACY_EMPTY, sizeof(X509_CRL_LEGACY_EMPTY), &Empty
	) && xrtX509CrlEntryInit(&Empty, &Cursor) &&
		(xrtX509CrlEntryRead(&Cursor, &Entry) == X509_DONE),
		"legacy empty CRL asset failed");
	testRequire(xrtX509CrlParse(
		X509_CRL_LEGACY_EXPIRED, sizeof(X509_CRL_LEGACY_EXPIRED), &Expired
	) && xrtDateTime(2026, 4, 8, 0, 0, 0, 0, &iExpired) &&
		!xrtX509CrlValidAt(&Expired, iExpired),
		"legacy expired CRL window failed");
}



/* 验证版本、时间、序列号、算法和签名的严格拒绝与失败原子性。 */
static void testX509CrlRejectsMalformed(void)
{
	uint8 Mutated[sizeof(X509_CRL_V2)];
	xx509crl Crl;
	xx509crl Before;

	for ( size_t i = 0; i < 5u; i++ ) {
		xx509error Expected;

		memcpy(Mutated, X509_CRL_V2, sizeof(Mutated));
		if ( i == 0 ) {
			Mutated[7] = 2u;
			Expected = X509_ERROR_VERSION;
		} else if ( i == 1 ) {
			Mutated[55] = '2';
			Mutated[56] = '5';
			Expected = X509_ERROR_TIME;
		} else if ( i == 2 ) {
			Mutated[72] = 0x04;
			Expected = X509_ERROR_SERIAL;
		} else if ( i == 3 ) {
			Mutated[127] = 0x71;
			Expected = X509_ERROR_ALGORITHM;
		} else {
			Mutated[130] = 1u;
			Mutated[194] = 0x3E;
			Expected = X509_ERROR_SIGNATURE;
		}
		memset(&Crl, 0xA5, sizeof(Crl));
		Before = Crl;
		testRequire(!xrtX509CrlParse(Mutated, sizeof(Mutated), &Crl) &&
			(memcmp(&Crl, &Before, sizeof(Crl)) == 0) &&
			(xrtErrorCode(xrtGetError()) == (int32)Expected),
			"malformed CRL error or failure atomicity mismatch");
	}
}



/* 执行 CRL 结构、条目、历史资产和严格失败边界测试。 */
int main(void)
{
	testX509CrlV2();
	testX509CrlEntries();
	testX509CrlLegacy();
	testX509CrlRejectsMalformed();
	printf("[PASS] x509_crl\n");
	return 0;
}
