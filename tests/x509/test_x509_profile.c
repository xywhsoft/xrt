#include "../test.h"
#include "../fixtures/x509_profile_vectors.h"



/* 验证 SAN、KeyUsage、BasicConstraints、EKU 和 SKI 的类型化结果。 */
static void testX509ProfileValues(void)
{
	static const uint8 ServerAuthOid[] = {
		0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01
	};
	static const uint8 ClientAuthOid[] = {
		0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02
	};
	xx509cert Cert;
	xx509gencursor Names;
	xx509genname Name;
	xx509oidcursor Oids;
	xbytesview Oid;
	xbytesview KeyId;
	xx509basicconstraints Constraints;
	uint16 iUsage;

	testRequire(xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Cert
	), "valid X.509 profile certificate parse failed");
	testRequire(xrtX509SubjectAltName(&Cert, &Names) == X509_VALUE,
		"X.509 subjectAltName initialization failed");
	testRequire((xrtX509GeneralNameRead(&Names, &Name) == X509_VALUE) &&
		(Name.Type == X509_NAME_DNS) && (Name.Value.Size == 16u) &&
		(memcmp(Name.Value.Data, "api.example.test", 16u) == 0),
		"X.509 DNS GeneralName mismatch");
	testRequire((xrtX509GeneralNameRead(&Names, &Name) == X509_VALUE) &&
		(Name.Type == X509_NAME_IP) && (Name.Value.Size == 4u) &&
		(memcmp(Name.Value.Data, "\x7F\x00\x00\x01", 4u) == 0) &&
		(xrtX509GeneralNameRead(&Names, &Name) == X509_DONE),
		"X.509 IP GeneralName or cursor end mismatch");

	testRequire((xrtX509KeyUsage(&Cert, &iUsage) == X509_VALUE) &&
		(iUsage == (X509_USAGE_DIGITAL_SIGNATURE |
		 X509_USAGE_KEY_ENCIPHERMENT)), "X.509 KeyUsage mismatch");
	testRequire((xrtX509BasicConstraints(
		&Cert, &Constraints
	) == X509_VALUE) && Constraints.CA && Constraints.HasPathLimit &&
		(Constraints.PathLimit == 2u), "X.509 BasicConstraints mismatch");

	testRequire(xrtX509ExtendedKeyUsage(&Cert, &Oids) == X509_VALUE,
		"X.509 extendedKeyUsage initialization failed");
	testRequire((xrtX509OidRead(&Oids, &Oid) == X509_VALUE) &&
		(Oid.Size == sizeof(ServerAuthOid)) &&
		(memcmp(Oid.Data, ServerAuthOid, sizeof(ServerAuthOid)) == 0),
		"X.509 serverAuth EKU mismatch");
	testRequire((xrtX509OidRead(&Oids, &Oid) == X509_VALUE) &&
		(Oid.Size == sizeof(ClientAuthOid)) &&
		(memcmp(Oid.Data, ClientAuthOid, sizeof(ClientAuthOid)) == 0) &&
		(xrtX509OidRead(&Oids, &Oid) == X509_DONE),
		"X.509 clientAuth EKU or cursor end mismatch");

	testRequire((xrtX509SubjectKeyId(&Cert, &KeyId) == X509_VALUE) &&
		(KeyId.Size == 20u), "X.509 SubjectKeyIdentifier mismatch");
	for ( size_t i = 0; i < KeyId.Size; i++ ) {
		testRequire(KeyId.Data[i] == (uint8)i,
			"X.509 SubjectKeyIdentifier byte mismatch");
	}
}



/* 验证扩展不存在是正常结果，且调用方输出保持不变。 */
static void testX509ProfileAbsent(void)
{
	xx509cert Cert;
	xx509gencursor Names;
	xx509gencursor BeforeNames;
	xx509oidcursor Oids;
	xx509oidcursor BeforeOids;
	xx509basicconstraints Constraints;
	xx509basicconstraints BeforeConstraints;
	xx509authoritykeyid Authority;
	xx509authoritykeyid BeforeAuthority;
	xbytesview KeyId;
	xbytesview BeforeKeyId;
	uint16 iUsage = UINT16_C(0xA55A);

	testRequire(xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Cert
	), "X.509 absent-profile fixture parse failed");
	memset(&Cert.Extensions, 0, sizeof(Cert.Extensions));
	memset(&Names, 0xA5, sizeof(Names));
	BeforeNames = Names;
	memset(&Oids, 0xA5, sizeof(Oids));
	BeforeOids = Oids;
	memset(&Constraints, 0xA5, sizeof(Constraints));
	BeforeConstraints = Constraints;
	memset(&KeyId, 0xA5, sizeof(KeyId));
	BeforeKeyId = KeyId;
	memset(&Authority, 0xA5, sizeof(Authority));
	BeforeAuthority = Authority;

	testRequire((xrtX509SubjectAltName(&Cert, &Names) == X509_DONE) &&
		(memcmp(&Names, &BeforeNames, sizeof(Names)) == 0) &&
		(xrtX509IssuerAltName(&Cert, &Names) == X509_DONE) &&
		(memcmp(&Names, &BeforeNames, sizeof(Names)) == 0) &&
		(xrtX509KeyUsage(&Cert, &iUsage) == X509_DONE) &&
		(iUsage == UINT16_C(0xA55A)) &&
		(xrtX509BasicConstraints(&Cert, &Constraints) == X509_DONE) &&
		(memcmp(
			&Constraints, &BeforeConstraints, sizeof(Constraints)
		) == 0) &&
		(xrtX509ExtendedKeyUsage(&Cert, &Oids) == X509_DONE) &&
		(memcmp(&Oids, &BeforeOids, sizeof(Oids)) == 0) &&
		(xrtX509SubjectKeyId(&Cert, &KeyId) == X509_DONE) &&
		(memcmp(&KeyId, &BeforeKeyId, sizeof(KeyId)) == 0) &&
		(xrtX509AuthorityKeyId(&Cert, &Authority) == X509_DONE) &&
		(memcmp(&Authority, &BeforeAuthority, sizeof(Authority)) == 0),
		"absent X.509 profile extension changed output");
}



/* 验证 IssuerAltName 与 SubjectAltName 使用相同的公开借用游标。 */
static void testX509IssuerAltName(void)
{
	static const uint8 Extensions[] = {
		0x30, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x1D, 0x12, 0x04,
		0x09, 0x30, 0x07, 0x86, 0x05, 0x63, 0x61, 0x3A, 0x2F, 0x2F
	};
	xx509cert Cert;
	xx509gencursor Names;
	xx509genname Name;

	memset(&Cert, 0, sizeof(Cert));
	Cert.Extensions = (xbytesview) { Extensions, sizeof(Extensions) };
	testRequire((xrtX509IssuerAltName(&Cert, &Names) == X509_VALUE) &&
		(xrtX509GeneralNameRead(&Names, &Name) == X509_VALUE) &&
		(Name.Type == X509_NAME_URI) && (Name.Value.Size == 5u) &&
		(memcmp(Name.Value.Data, "ca://", 5u) == 0) &&
		(xrtX509GeneralNameRead(&Names, &Name) == X509_DONE),
		"X.509 issuerAltName traversal failed");
}



/* 验证公开 GeneralNames 和 OID 序列游标的独立使用路径。 */
static void testX509ProfileCursors(void)
{
	static const uint8 GeneralNames[] = {
		0x30, 0x08, 0x81, 0x03, 0x61, 0x40, 0x62, 0x88, 0x01, 0x2A
	};
	static const uint8 BadRegisteredId[] = {
		0x30, 0x03, 0x88, 0x01, 0x80
	};
	static const uint8 OidList[] = {
		0x30, 0x06, 0x06, 0x01, 0x2A, 0x06, 0x01, 0x2B
	};
	static const uint8 DuplicateOids[] = {
		0x30, 0x06, 0x06, 0x01, 0x2A, 0x06, 0x01, 0x2A
	};
	static const uint8 EmptySequence[] = { 0x30, 0x00 };
	xx509gencursor Names;
	xx509gencursor BeforeNames;
	xx509genname Name;
	xx509oidcursor Oids;
	xx509oidcursor BeforeOids;
	xbytesview Oid;

	testRequire(xrtX509GeneralNameInit(
		(xbytesview) { GeneralNames, sizeof(GeneralNames) }, &Names
	) && (xrtX509GeneralNameRead(&Names, &Name) == X509_VALUE) &&
		(Name.Type == X509_NAME_EMAIL) && (Name.Value.Size == 3u) &&
		(xrtX509GeneralNameRead(&Names, &Name) == X509_VALUE) &&
		(Name.Type == X509_NAME_REGISTERED_ID) &&
		(xrtX509GeneralNameRead(&Names, &Name) == X509_DONE),
		"standalone X.509 GeneralNames traversal failed");

	testRequire(xrtX509OidInit(
		(xbytesview) { OidList, sizeof(OidList) }, &Oids
	) && (xrtX509OidRead(&Oids, &Oid) == X509_VALUE) &&
		(Oid.Size == 1u) && (Oid.Data[0] == 0x2A) &&
		(xrtX509OidRead(&Oids, &Oid) == X509_VALUE) &&
		(Oid.Size == 1u) && (Oid.Data[0] == 0x2B) &&
		(xrtX509OidRead(&Oids, &Oid) == X509_DONE),
		"standalone X.509 OID traversal failed");

	memset(&Names, 0xA5, sizeof(Names));
	BeforeNames = Names;
	testRequire(!xrtX509GeneralNameInit(
		(xbytesview) { BadRegisteredId, sizeof(BadRegisteredId) }, &Names
	) && (memcmp(&Names, &BeforeNames, sizeof(Names)) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_GENERAL_NAME),
		"invalid registeredID changed GeneralNames output");
	testRequire(!xrtX509GeneralNameInit(
		(xbytesview) { EmptySequence, sizeof(EmptySequence) }, &Names
	) && (memcmp(&Names, &BeforeNames, sizeof(Names)) == 0),
		"empty GeneralNames was accepted");

	memset(&Oids, 0xA5, sizeof(Oids));
	BeforeOids = Oids;
	testRequire(!xrtX509OidInit(
		(xbytesview) { DuplicateOids, sizeof(DuplicateOids) }, &Oids
	) && (memcmp(&Oids, &BeforeOids, sizeof(Oids)) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_OID_LIST),
		"duplicate X.509 OID changed output");
	testRequire(!xrtX509OidInit(
		(xbytesview) { EmptySequence, sizeof(EmptySequence) }, &Oids
	) && (memcmp(&Oids, &BeforeOids, sizeof(Oids)) == 0),
		"empty X.509 OID sequence was accepted");
}



/* 验证 SKI/AKI 的独立解析、证书便利入口和隐式字段边界。 */
static void testX509KeyIdentifiers(void)
{
	static const uint8 SubjectKeyId[] = {
		0x04, 0x03, 0xAA, 0xBB, 0xCC
	};
	static const uint8 AuthorityKeyId[] = {
		0x30, 0x10,
		0x80, 0x03, 0x01, 0x02, 0x03,
		0xA1, 0x05, 0x86, 0x03, 0x63, 0x61, 0x3A,
		0x82, 0x02, 0x00, 0x80
	};
	static const uint8 ZeroAuthorityKeyId[] = {
		0x30, 0x0F,
		0x80, 0x03, 0x01, 0x02, 0x03,
		0xA1, 0x05, 0x86, 0x03, 0x63, 0x61, 0x3A,
		0x82, 0x01, 0x00
	};
	static const uint8 NegativeAuthorityKeyId[] = {
		0x30, 0x0F,
		0x80, 0x03, 0x01, 0x02, 0x03,
		0xA1, 0x05, 0x86, 0x03, 0x63, 0x61, 0x3A,
		0x82, 0x01, 0xFF
	};
	static const uint8 AuthorityExtension[] = {
		0x30, 0x1B,
		0x30, 0x19,
		0x06, 0x03, 0x55, 0x1D, 0x23,
		0x04, 0x12,
		0x30, 0x10,
		0x80, 0x03, 0x01, 0x02, 0x03,
		0xA1, 0x05, 0x86, 0x03, 0x63, 0x61, 0x3A,
		0x82, 0x02, 0x00, 0x80
	};
	static const uint8 CriticalAuthorityExtension[] = {
		0x30, 0x1E,
		0x30, 0x1C,
		0x06, 0x03, 0x55, 0x1D, 0x23,
		0x01, 0x01, 0xFF,
		0x04, 0x12,
		0x30, 0x10,
		0x80, 0x03, 0x01, 0x02, 0x03,
		0xA1, 0x05, 0x86, 0x03, 0x63, 0x61, 0x3A,
		0x82, 0x02, 0x00, 0x80
	};
	static const uint8 BadAuthority[][13] = {
		{ 0x30, 0x00 },
		{ 0x30, 0x07, 0xA1, 0x05, 0x86, 0x03, 0x63, 0x61, 0x3A },
		{ 0x30, 0x09, 0x82, 0x02, 0x00, 0x80,
		  0x80, 0x03, 0x01, 0x02, 0x03 },
		{ 0x30, 0x0B, 0xA1, 0x05, 0x86, 0x03, 0x63, 0x61, 0x3A,
		  0x82, 0x02, 0x00, 0x01 }
	};
	static const size_t BadAuthoritySize[] = { 2u, 9u, 11u, 13u };
	xx509authoritykeyid Authority;
	xx509authoritykeyid BeforeAuthority;
	xx509genname Name;
	xx509cert Cert;
	xbytesview KeyId;
	xbytesview BeforeKeyId;

	testRequire(xrtX509SubjectKeyIdParse(
		(xbytesview) { SubjectKeyId, sizeof(SubjectKeyId) }, &KeyId
	) && (KeyId.Size == 3u) &&
		(memcmp(KeyId.Data, "\xAA\xBB\xCC", 3u) == 0),
		"standalone SubjectKeyIdentifier parse failed");
	testRequire(xrtX509AuthorityKeyIdParse(
		(xbytesview) { AuthorityKeyId, sizeof(AuthorityKeyId) }, &Authority
	) && Authority.HasKeyId && Authority.HasIssuer && Authority.HasSerial &&
		(Authority.KeyId.Size == 3u) && (Authority.Serial.Size == 2u) &&
		(Authority.Serial.Data[0] == 0) &&
		(Authority.Serial.Data[1] == UINT8_C(0x80)) &&
		(xrtX509GeneralNameRead(&Authority.Issuer, &Name) == X509_VALUE) &&
		(Name.Type == X509_NAME_URI) && (Name.Value.Size == 3u) &&
		(memcmp(Name.Value.Data, "ca:", 3u) == 0) &&
		(xrtX509GeneralNameRead(&Authority.Issuer, &Name) == X509_DONE),
		"standalone AuthorityKeyIdentifier parse failed");
	testRequire(xrtX509AuthorityKeyIdParse(
		(xbytesview) {
			ZeroAuthorityKeyId, sizeof(ZeroAuthorityKeyId)
		}, &Authority
	) && Authority.HasSerial && (Authority.Serial.Size == 1u) &&
		(Authority.Serial.Data[0] == 0),
		"zero AuthorityKeyIdentifier serial was not preserved");
	testRequire(xrtX509AuthorityKeyIdParse(
		(xbytesview) {
			NegativeAuthorityKeyId, sizeof(NegativeAuthorityKeyId)
		}, &Authority
	) && Authority.HasSerial && (Authority.Serial.Size == 1u) &&
		(Authority.Serial.Data[0] == UINT8_C(0xFF)),
		"negative AuthorityKeyIdentifier serial was not preserved");

	memset(&Cert, 0, sizeof(Cert));
	Cert.Extensions = (xbytesview) {
		AuthorityExtension, sizeof(AuthorityExtension)
	};
	testRequire((xrtX509AuthorityKeyId(&Cert, &Authority) == X509_VALUE) &&
		Authority.HasKeyId && Authority.HasIssuer && Authority.HasSerial,
		"certificate AuthorityKeyIdentifier read failed");

	Cert.Extensions = (xbytesview) {
		CriticalAuthorityExtension, sizeof(CriticalAuthorityExtension)
	};
	memset(&Authority, 0xA5, sizeof(Authority));
	BeforeAuthority = Authority;
	testRequire((xrtX509AuthorityKeyId(&Cert, &Authority) == X509_ERROR) &&
		(memcmp(&Authority, &BeforeAuthority, sizeof(Authority)) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_KEY_IDENTIFIER),
		"critical AuthorityKeyIdentifier was accepted or changed output");

	for ( size_t i = 0; i < sizeof(BadAuthoritySize) /
		sizeof(BadAuthoritySize[0]); i++ ) {
		memset(&Authority, 0xA5, sizeof(Authority));
		BeforeAuthority = Authority;
		testRequire(!xrtX509AuthorityKeyIdParse(
			(xbytesview) { BadAuthority[i], BadAuthoritySize[i] }, &Authority
		) && (memcmp(
			&Authority, &BeforeAuthority, sizeof(Authority)
		) == 0) &&
			(xrtErrorCode(xrtGetError()) == X509_ERROR_KEY_IDENTIFIER),
			"malformed AuthorityKeyIdentifier was accepted or changed output");
	}

	memset(&KeyId, 0xA5, sizeof(KeyId));
	BeforeKeyId = KeyId;
	testRequire(!xrtX509SubjectKeyIdParse(
		(xbytesview) { SubjectKeyId, 2u }, &KeyId
	) && (memcmp(&KeyId, &BeforeKeyId, sizeof(KeyId)) == 0),
		"malformed SubjectKeyIdentifier changed output");
}



/* 验证畸形 profile 正文由对应类型层拒绝，并保持输出不变。 */
static void testX509ProfileRejectsMalformed(void)
{
	xx509cert Cert;
	xx509gencursor Names;
	xx509gencursor BeforeNames;
	xx509oidcursor Oids;
	xx509oidcursor BeforeOids;
	xx509basicconstraints Constraints;
	xx509basicconstraints BeforeConstraints;
	xbytesview KeyId;
	xbytesview BeforeKeyId;
	uint16 iUsage;

	testRequire(xrtX509Parse(
		X509_PROFILE_BAD_SAN, sizeof(X509_PROFILE_BAD_SAN), &Cert
	), "bad SAN fixture failed base X.509 parse");
	memset(&Names, 0xA5, sizeof(Names));
	BeforeNames = Names;
	testRequire((xrtX509SubjectAltName(&Cert, &Names) == X509_ERROR) &&
		(memcmp(&Names, &BeforeNames, sizeof(Names)) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_GENERAL_NAME),
		"bad SAN was accepted or changed output");

	for ( size_t i = 0; i < 2u; i++ ) {
		const uint8* pDer = (i == 0) ? X509_PROFILE_BAD_KU_MINIMAL :
			X509_PROFILE_BAD_KU_AGREEMENT;
		size_t iSize = (i == 0) ? sizeof(X509_PROFILE_BAD_KU_MINIMAL) :
			sizeof(X509_PROFILE_BAD_KU_AGREEMENT);

		testRequire(xrtX509Parse(pDer, iSize, &Cert),
			"bad KeyUsage fixture failed base X.509 parse");
		iUsage = UINT16_C(0xA55A);
		testRequire((xrtX509KeyUsage(&Cert, &iUsage) == X509_ERROR) &&
			(iUsage == UINT16_C(0xA55A)) &&
			(xrtErrorCode(xrtGetError()) == X509_ERROR_KEY_USAGE),
			"bad KeyUsage was accepted or changed output");
	}

	testRequire(xrtX509Parse(
		X509_PROFILE_BAD_BC, sizeof(X509_PROFILE_BAD_BC), &Cert
	), "bad BasicConstraints fixture failed base X.509 parse");
	memset(&Constraints, 0xA5, sizeof(Constraints));
	BeforeConstraints = Constraints;
	testRequire((xrtX509BasicConstraints(
		&Cert, &Constraints
	) == X509_ERROR) && (memcmp(
		&Constraints, &BeforeConstraints, sizeof(Constraints)
	) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_BASIC_CONSTRAINTS),
		"bad BasicConstraints was accepted or changed output");

	testRequire(xrtX509Parse(
		X509_PROFILE_BAD_EKU, sizeof(X509_PROFILE_BAD_EKU), &Cert
	), "bad EKU fixture failed base X.509 parse");
	memset(&Oids, 0xA5, sizeof(Oids));
	BeforeOids = Oids;
	testRequire((xrtX509ExtendedKeyUsage(&Cert, &Oids) == X509_ERROR) &&
		(memcmp(&Oids, &BeforeOids, sizeof(Oids)) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_EXTENDED_KEY_USAGE) &&
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(xrtErrorCode(xrtErrorCause(xrtGetError())) == X509_ERROR_OID_LIST),
		"bad EKU was accepted or changed output");

	testRequire(xrtX509Parse(
		X509_PROFILE_BAD_SKI, sizeof(X509_PROFILE_BAD_SKI), &Cert
	), "bad SKI fixture failed base X.509 parse");
	memset(&KeyId, 0xA5, sizeof(KeyId));
	BeforeKeyId = KeyId;
	testRequire((xrtX509SubjectKeyId(&Cert, &KeyId) == X509_ERROR) &&
		(memcmp(&KeyId, &BeforeKeyId, sizeof(KeyId)) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_KEY_IDENTIFIER),
		"bad SKI was accepted or changed output");
}



/* 执行 X.509 profile 类型化扩展和严格失败边界测试。 */
int main(void)
{
	testX509ProfileValues();
	testX509ProfileAbsent();
	testX509IssuerAltName();
	testX509ProfileCursors();
	testX509KeyIdentifiers();
	testX509ProfileRejectsMalformed();
	printf("[PASS] x509_profile\n");
	return 0;
}
