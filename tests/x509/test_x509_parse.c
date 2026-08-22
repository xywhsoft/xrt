#include "../test.h"
#include "../fixtures/x509_legacy_cert.h"
#include "../fixtures/x509_vectors.h"



/* 验证证书骨架、严格时间、签名视图和有效期谓词。 */
static void testX509Certificate(void)
{
	static const uint8 Ed25519Oid[] = { 0x2B, 0x65, 0x70 };
	xx509cert Cert;
	xtime iBefore;
	xtime iAfter;

	testRequire(xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Cert
	), "valid X.509 certificate parse failed");
	testRequire((Cert.Version == X509_VERSION_3) &&
		(Cert.Raw.Data == X509_VALID_ED25519) &&
		(Cert.Raw.Size == sizeof(X509_VALID_ED25519)) &&
		(Cert.Tbs.Size != 0) && (Cert.Serial.Size == 1u) &&
		(Cert.Serial.Data[0] == 1u) &&
		(Cert.TbsSignature.Oid.Size == sizeof(Ed25519Oid)) &&
		(memcmp(
			Cert.TbsSignature.Oid.Data, Ed25519Oid, sizeof(Ed25519Oid)
		) == 0) && !Cert.TbsSignature.HasParameters &&
		(Cert.Signature.Size == 64u),
		"X.509 certificate field views mismatch");

	testRequire(xrtDateTime(2025, 1, 1, 0, 0, 0, 0, &iBefore) &&
		xrtDateTime(2051, 1, 1, 0, 0, 0, 0, &iAfter) &&
		(Cert.NotBefore == iBefore) && (Cert.NotAfter == iAfter),
		"X.509 validity conversion mismatch");
	testRequire(xrtX509ValidAt(&Cert, iBefore) &&
		xrtX509ValidAt(&Cert, iAfter) &&
		!xrtX509ValidAt(&Cert, iBefore - 1) &&
		!xrtX509ValidAt(&Cert, iAfter + 1),
		"X.509 validity boundary mismatch");

	testRequire(xrtX509Parse(
		X509_COMPAT_GENERALIZED_2049,
		sizeof(X509_COMPAT_GENERALIZED_2049),
		&Cert
	) && xrtDateTime(2049, 12, 31, 23, 59, 59, 0, &iAfter) &&
		(Cert.NotAfter == iAfter),
		"X.509 compatible pre-2050 GeneralizedTime parse failed");
}



/* 验证独立时间解析可供证书和 CRL 之外的协议层复用。 */
static void testX509Time(void)
{
	static const uint8 Utc[] = {
		0x17, 0x0D, '2', '5', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5',
		'Z'
	};
	static const uint8 Generalized[] = {
		0x18, 0x0F, '2', '0', '5', '1', '0', '1', '0', '2', '0', '3', '0', '4',
		'0', '5', 'Z'
	};
	static const uint8 Compatible[] = {
		0x18, 0x0F, '2', '0', '4', '9', '0', '1', '0', '2', '0', '3', '0', '4',
		'0', '5', 'Z'
	};
	static const uint8 Bad[] = {
		0x18, 0x0F, '2', '0', '4', '9', '1', '3', '0', '2', '0', '3', '0', '4',
		'0', '5', 'Z'
	};
	xtime iUtc;
	xtime iGeneralized;
	xtime iExpected;
	xtime iBefore = (xtime)INT64_C(0x12345678);
	xtime iOutput = iBefore;

	testRequire(xrtX509TimeParse(
		(xbytesview) { Utc, sizeof(Utc) }, &iUtc
	) && xrtDateTime(2025, 1, 2, 3, 4, 5, 0, &iExpected) &&
		(iUtc == iExpected), "standalone X.509 UTCTime parse failed");
	testRequire(xrtX509TimeParse(
		(xbytesview) { Generalized, sizeof(Generalized) }, &iGeneralized
	) && xrtDateTime(2051, 1, 2, 3, 4, 5, 0, &iExpected) &&
		(iGeneralized == iExpected),
		"standalone X.509 GeneralizedTime parse failed");
	testRequire(xrtX509TimeParse(
		(xbytesview) { Compatible, sizeof(Compatible) }, &iGeneralized
	) && xrtDateTime(2049, 1, 2, 3, 4, 5, 0, &iExpected) &&
		(iGeneralized == iExpected),
		"standalone X.509 compatible GeneralizedTime parse failed");
	testRequire(!xrtX509TimeParse(
		(xbytesview) { Bad, sizeof(Bad) }, &iOutput
	) && (iOutput == iBefore) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_TIME),
		"standalone X.509 time failure was not atomic");
}



/* 验证证书使用方无损保留零、负数和超长历史序列号。 */
static void testX509LegacySerials(void)
{
	uint8 Negative[sizeof(X509_VALID_ED25519)];
	uint8 Long[sizeof(X509_VALID_ED25519) + 20u];
	xx509cert Cert;

	testRequire(xrtX509Parse(
		X509_SERIAL_ZERO, sizeof(X509_SERIAL_ZERO), &Cert
	) && (Cert.Serial.Size == 1u) && (Cert.Serial.Data[0] == 0),
		"zero X.509 serial was not preserved");

	memcpy(Negative, X509_VALID_ED25519, sizeof(Negative));
	Negative[14] = UINT8_C(0xFF);
	testRequire(xrtX509Parse(
		Negative, sizeof(Negative), &Cert
	) && (Cert.Serial.Size == 1u) &&
		(Cert.Serial.Data[0] == UINT8_C(0xFF)),
		"negative X.509 serial was not preserved");

	memcpy(Long, X509_VALID_ED25519, 12u);
	Long[2] = UINT8_C(0x01);
	Long[3] = UINT8_C(0x39);
	Long[6] = UINT8_C(0xEC);
	Long[12] = UINT8_C(0x02);
	Long[13] = UINT8_C(0x15);
	for ( size_t i = 0; i < 21u; i++ ) {
		Long[14u + i] = (uint8)(i + 1u);
	}
	memcpy(
		Long + 35u, X509_VALID_ED25519 + 15u,
		sizeof(X509_VALID_ED25519) - 15u
	);
	testRequire(xrtX509Parse(
		Long, sizeof(Long), &Cert
	) && (Cert.Serial.Size == 21u) &&
		(Cert.Serial.Data[0] == 1u) && (Cert.Serial.Data[20] == 21u),
		"long X.509 serial was not preserved");
}



/* 验证 Name 游标保留 RDN 边界、值标签和精确 OID 查找。 */
static void testX509Name(void)
{
	static const uint8 OrganizationOid[] = { 0x55, 0x04, 0x0A };
	static const uint8 CommonNameOid[] = { 0x55, 0x04, 0x03 };
	xx509cert Cert;
	xx509namecursor Cursor;
	xx509nameattr Attribute;

	testRequire(xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Cert
	) && xrtX509NameInit(Cert.Issuer, &Cursor),
		"X.509 issuer Name initialization failed");
	testRequire((xrtX509NameRead(&Cursor, &Attribute) == X509_VALUE) &&
		(Attribute.Rdn == 0) &&
		(Attribute.Oid.Size == sizeof(OrganizationOid)) &&
		(memcmp(
			Attribute.Oid.Data, OrganizationOid, sizeof(OrganizationOid)
		) == 0) && (Attribute.ValueTag.Number == XASN1_UTF8_STRING) &&
		(Attribute.Value.Size == 3u) &&
		(memcmp(Attribute.Value.Data, "XRT", 3) == 0),
		"X.509 first issuer attribute mismatch");
	testRequire((xrtX509NameRead(&Cursor, &Attribute) == X509_VALUE) &&
		(Attribute.Rdn == 1) &&
		(Attribute.Oid.Size == sizeof(CommonNameOid)) &&
		(memcmp(Attribute.Value.Data, "XRT Test CA", 11) == 0),
		"X.509 second issuer attribute mismatch");
	testRequire(xrtX509NameRead(&Cursor, &Attribute) == X509_DONE,
		"X.509 Name cursor did not finish exactly");
	testRequire(xrtX509NameFind(
		Cert.Subject, CommonNameOid, sizeof(CommonNameOid), &Attribute
	) && (Attribute.Value.Size == 16u) &&
		(memcmp(Attribute.Value.Data, "api.example.test", 16) == 0),
		"X.509 subject commonName lookup failed");
}



/* 验证扩展遍历、critical 语义、正文 DER 和无扩展路径。 */
static void testX509Extensions(void)
{
	static const uint8 SanOid[] = { 0x55, 0x1D, 0x11 };
	static const uint8 BasicConstraintsOid[] = { 0x55, 0x1D, 0x13 };
	xx509cert Cert;
	xx509cert Empty;
	xx509extcursor Cursor;
	xx509ext Extension;

	testRequire(xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Cert
	) && xrtX509ExtensionInit(&Cert, &Cursor),
		"X.509 extension cursor initialization failed");
	testRequire((xrtX509ExtensionRead(&Cursor, &Extension) == X509_VALUE) &&
		Extension.Critical && (Extension.Oid.Size == sizeof(SanOid)) &&
		(memcmp(Extension.Oid.Data, SanOid, sizeof(SanOid)) == 0) &&
		xrtDerValidate(Extension.Value.Data, Extension.Value.Size),
		"X.509 SAN extension mismatch");
	testRequire((xrtX509ExtensionRead(&Cursor, &Extension) == X509_VALUE) &&
		Extension.Critical &&
		(Extension.Oid.Size == sizeof(BasicConstraintsOid)) &&
		(memcmp(
			Extension.Oid.Data,
			BasicConstraintsOid, sizeof(BasicConstraintsOid)
		) == 0), "X.509 basicConstraints extension mismatch");
	testRequire(xrtX509ExtensionRead(&Cursor, &Extension) == X509_DONE,
		"X.509 extension cursor did not finish exactly");
	testRequire(xrtX509ExtensionFind(
		&Cert, SanOid, sizeof(SanOid), &Extension
	) && Extension.Critical, "X.509 extension find failed");
	testRequire(xrtX509ExtensionListInit(Cert.Extensions, &Cursor) &&
		(xrtX509ExtensionRead(&Cursor, &Extension) == X509_VALUE) &&
		xrtX509ExtensionListFind(
			Cert.Extensions, BasicConstraintsOid,
			sizeof(BasicConstraintsOid), &Extension
		) && Extension.Critical,
		"generic X.509 extension list API failed");

	Empty = Cert;
	memset(&Empty.Extensions, 0, sizeof(Empty.Extensions));
	testRequire(xrtX509ExtensionInit(&Empty, &Cursor) &&
		(xrtX509ExtensionRead(&Cursor, &Extension) == X509_DONE),
		"X.509 empty extension cursor failed");
}



/* 验证已知 OKP 和未知未来算法都通过同一 SPKI 视图公开。 */
static void testX509PublicKey(void)
{
	xx509cert Cert;
	xx509pubkey PublicKey;

	testRequire(xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Cert
	) && xrtX509PublicKey(&Cert, &PublicKey) &&
		(PublicKey.Type == X509_KEY_ED25519) &&
		(PublicKey.Curve == X509_CURVE_UNKNOWN) &&
		(PublicKey.Key.Size == 32u) &&
		(PublicKey.Key.Data[0] == 0) && (PublicKey.Key.Data[31] == 31),
		"X.509 Ed25519 public key view mismatch");

	testRequire(xrtX509Parse(
		X509_VALID_UNKNOWN_KEY, sizeof(X509_VALID_UNKNOWN_KEY), &Cert
	) && xrtX509PublicKey(&Cert, &PublicKey) &&
		(PublicKey.Type == X509_KEY_UNKNOWN) &&
		(PublicKey.Key.Size == 11u) &&
		(memcmp(PublicKey.Key.Data, "unknown-key", 11) == 0),
		"X.509 unknown public key was not preserved");
}



/* 验证旧版 TLS 示例中的真实 RSA 证书继续通过新解析底座。 */
static void testX509LegacyCertificate(void)
{
	static const uint8 SanOid[] = { 0x55, 0x1D, 0x11 };
	xx509cert Cert;
	xx509pubkey PublicKey;
	xx509ext San;
	xtime iBefore;
	xtime iAfter;

	testRequire(xrtX509Parse(
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT), &Cert
	) && xrtX509PublicKey(&Cert, &PublicKey) &&
		(PublicKey.Type == X509_KEY_RSA) &&
		(PublicKey.Modulus.Size == 256u) &&
		(PublicKey.Exponent.Size == 3u) &&
		(memcmp(PublicKey.Exponent.Data, "\x01\x00\x01", 3) == 0),
		"legacy XRT RSA certificate or SPKI parse failed");
	testRequire(xrtX509ExtensionFind(
		&Cert, SanOid, sizeof(SanOid), &San
	) && !San.Critical, "legacy XRT SAN extension parse failed");
	testRequire(xrtDateTime(2026, 4, 1, 4, 35, 47, 0, &iBefore) &&
		xrtDateTime(2036, 3, 30, 4, 35, 47, 0, &iAfter) &&
		(Cert.NotBefore == iBefore) && (Cert.NotAfter == iAfter),
		"legacy XRT certificate validity mismatch");
}



typedef struct testx509failure {
	const uint8* Data;
	size_t Size;
	xx509error Code;
} testx509failure;



/* 验证版本、序列号、时间、算法、扩展和 SPKI 的严格拒绝边界。 */
static void testX509RejectsMalformed(void)
{
	static const testx509failure Cases[] = {
		{ X509_BAD_EXPLICIT_V1, sizeof(X509_BAD_EXPLICIT_V1),
			X509_ERROR_VERSION },
		{ X509_BAD_TIME_MONTH, sizeof(X509_BAD_TIME_MONTH),
			X509_ERROR_TIME },
		{ X509_BAD_ALGORITHM_MISMATCH,
			sizeof(X509_BAD_ALGORITHM_MISMATCH), X509_ERROR_ALGORITHM },
		{ X509_BAD_DUPLICATE_EXTENSION,
			sizeof(X509_BAD_DUPLICATE_EXTENSION),
			X509_ERROR_DUPLICATE_EXTENSION },
		{ X509_BAD_CRITICAL_FALSE, sizeof(X509_BAD_CRITICAL_FALSE),
			X509_ERROR_EXTENSION },
		{ X509_BAD_EXTENSION_DER, sizeof(X509_BAD_EXTENSION_DER),
			X509_ERROR_EXTENSION },
		{ X509_BAD_EMPTY_SUBJECT, sizeof(X509_BAD_EMPTY_SUBJECT),
			X509_ERROR_NAME },
		{ X509_BAD_V2_EXTENSIONS, sizeof(X509_BAD_V2_EXTENSIONS),
			X509_ERROR_VERSION },
		{ X509_BAD_OKP_PARAMETERS, sizeof(X509_BAD_OKP_PARAMETERS),
			X509_ERROR_PUBLIC_KEY }
	};

	for ( size_t i = 0; i < sizeof(Cases) / sizeof(Cases[0]); i++ ) {
		xx509cert Cert;
		xx509cert Before;
		const xerror* pError;

		memset(&Cert, 0xA5, sizeof(Cert));
		Before = Cert;
		testRequire(!xrtX509Parse(Cases[i].Data, Cases[i].Size, &Cert) &&
			(memcmp(&Cert, &Before, sizeof(Cert)) == 0),
			"malformed X.509 certificate changed output");
		pError = xrtGetError();
		testRequire((pError != NULL) &&
			(strcmp(xrtErrorDomain(pError), "xrt.x509") == 0) &&
			(xrtErrorCode(pError) == (int32)Cases[i].Code),
			"malformed X.509 certificate error mismatch");
	}
}



/* 验证 DER 原因链、AlgorithmIdentifier 和便捷查找的失败原子性。 */
static void testX509FailureAtomicity(void)
{
	static const uint8 Algorithm[] = {
		0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70
	};
	static const uint8 BadAlgorithm[] = {
		0x30, 0x09, 0x06, 0x03, 0x2B, 0x65, 0x70,
		0x05, 0x00, 0x05, 0x00
	};
	static const uint8 MissingOid[] = { 0x55, 0x04, 0x7F };
	xx509cert Cert;
	xx509algorithm Parsed;
	xx509algorithm BeforeAlgorithm;
	xx509nameattr Attribute;
	xx509nameattr BeforeAttribute;
	const xerror* pError;

	testRequire(xrtX509AlgorithmParse(
		(xbytesview) { Algorithm, sizeof(Algorithm) }, &Parsed
	) && !Parsed.HasParameters && (Parsed.Oid.Size == 3u),
		"standalone X.509 AlgorithmIdentifier parse failed");
	memset(&Parsed, 0xA5, sizeof(Parsed));
	BeforeAlgorithm = Parsed;
	testRequire(!xrtX509AlgorithmParse(
		(xbytesview) { BadAlgorithm, sizeof(BadAlgorithm) }, &Parsed
	) && (memcmp(&Parsed, &BeforeAlgorithm, sizeof(Parsed)) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_ALGORITHM),
		"bad AlgorithmIdentifier changed output");

	testRequire(!xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519) - 1u, &Cert
	), "truncated X.509 certificate was accepted");
	pError = xrtGetError();
	testRequire((pError != NULL) &&
		(xrtErrorCode(pError) == X509_ERROR_DER) &&
		(xrtErrorCause(pError) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(pError)), "xrt.asn1"
		) == 0), "X.509 DER cause chain mismatch");

	testRequire(xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Cert
	), "X.509 find fixture parse failed");
	memset(&Attribute, 0xA5, sizeof(Attribute));
	BeforeAttribute = Attribute;
	testRequire(!xrtX509NameFind(
		Cert.Subject, MissingOid, sizeof(MissingOid), &Attribute
	) && (memcmp(&Attribute, &BeforeAttribute, sizeof(Attribute)) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_NOT_FOUND),
		"X.509 Name not-found changed output");
}



/* 执行 X.509 证书、Name、Extension、SPKI 和严格失败边界测试。 */
int main(void)
{
	testX509Certificate();
	testX509Time();
	testX509LegacySerials();
	testX509Name();
	testX509Extensions();
	testX509PublicKey();
	testX509LegacyCertificate();
	testX509RejectsMalformed();
	testX509FailureAtomicity();
	printf("[PASS] x509_parse\n");
	return 0;
}
