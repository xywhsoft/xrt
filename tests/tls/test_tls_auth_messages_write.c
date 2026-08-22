#include "../test.h"



/* 颁发者向量与两个 CertificateRequest writer 必须完成严格 round-trip。 */
static void testTlsCertificateRequestWriters(void)
{
	static const uint8 Name1[] = { 0x30, 1, 1 };
	static const uint8 Name2[] = { 0x30, 1, 2 };
	static const uint8 Types[] = {
		XTLS_CERTIFICATE_RSA_SIGN, XTLS_CERTIFICATE_ECDSA_SIGN
	};
	static const uint8 SignatureIds[] = {
		0x04, 0x03, 0x08, 0x04
	};
	static const uint8 Extensions[] = {
		0, 13, 0, 6, 0, 4, 0x04, 0x03, 0x08, 0x04,
		0, 47, 0, 7, 0, 5, 0, 3, 0x30, 1, 1
	};
	xbytesview Names[2];
	uint8 Authorities[32];
	uint8 Body[96];
	xtls12certificaterequest Request12;
	xtls12certificaterequest Parsed12;
	xtls13certificaterequest Parsed13;
	xtlsauthoritycursor Cursor;
	xbytesview Name;
	size_t iAuthoritySize;
	size_t iBodySize;

	Names[0] = (xbytesview) { Name1, sizeof(Name1) };
	Names[1] = (xbytesview) { Name2, sizeof(Name2) };
	iAuthoritySize = xrtTlsAuthoritiesSize(Names, 2u);
	testRequire((iAuthoritySize == 12u) && xrtTlsAuthoritiesEncode(
		Names, 2u, Authorities, sizeof(Authorities)
	) && xrtTlsAuthorities(
		(xbytesview) { Authorities, iAuthoritySize }, &Cursor
	) && (xrtTlsAuthoritiesRead(&Cursor, &Name) == XTLS_ITEM_VALUE) &&
		(Name.Data[2] == 1u) &&
		(xrtTlsAuthoritiesRead(&Cursor, &Name) == XTLS_ITEM_VALUE) &&
		(Name.Data[2] == 2u),
		"TLS certificate-authority writer round-trip mismatch");

	memset(&Request12, 0, sizeof(Request12));
	Request12.CertificateTypes = (xbytesview) { Types, sizeof(Types) };
	Request12.Signatures.Data = (xbytesview) {
		SignatureIds, sizeof(SignatureIds)
	};
	Request12.AuthorityData = (xbytesview) {
		Authorities, iAuthoritySize
	};
	iBodySize = xrtTls12CertificateRequestSize(&Request12);
	testRequire((iBodySize != 0) && xrtTls12CertificateRequestEncode(
		&Request12, Body, sizeof(Body)
	) && xrtTls12CertificateRequestParse(
		(xbytesview) { Body, iBodySize }, &Parsed12
	) && (Parsed12.CertificateTypes.Size == 2u) &&
		(xrtTlsIdsCount(&Parsed12.Signatures) == 2u),
		"TLS 1.2 CertificateRequest writer round-trip mismatch");

	iBodySize = xrtTls13CertificateRequestSize(
		XRT_BYTES_LITERAL("ctx"),
		(xbytesview) { Extensions, sizeof(Extensions) }
	);
	testRequire((iBodySize != 0) && xrtTls13CertificateRequestEncode(
		XRT_BYTES_LITERAL("ctx"),
		(xbytesview) { Extensions, sizeof(Extensions) },
		Body, sizeof(Body)
	) && xrtTls13CertificateRequestParse(
		(xbytesview) { Body, iBodySize }, &Parsed13
	) && (Parsed13.RequestContext.Size == 3u) &&
		(xrtTlsIdsCount(&Parsed13.Signatures) == 2u) &&
		(Parsed13.AuthorityData.Size == 7u),
		"TLS 1.3 CertificateRequest writer round-trip mismatch");
}



/* TLS 1.2 ECDHE writer 必须从字段构造可直接验签的参数切片。 */
static void testTls12KeyExchangeWriters(void)
{
	uint8 PublicKey[32];
	static const uint8 Signature[] = { 1, 2, 3 };
	uint8 Body[64];
	xtlscertificateverify Verify;
	xtls12serverkeyexchange Parsed;
	xbytesview ParsedKey;
	size_t iBodySize;

	memset(PublicKey, 0xA5, sizeof(PublicKey));
	Verify.Scheme = XTLS_SIGNATURE_ED25519;
	Verify.Signature = (xbytesview) { Signature, sizeof(Signature) };
	iBodySize = xrtTls12ServerKeyExchangeSize(
		XTLS_GROUP_X25519,
		(xbytesview) { PublicKey, sizeof(PublicKey) }, &Verify
	);
	testRequire((iBodySize == 43u) && xrtTls12ServerKeyExchangeEncode(
		XTLS_GROUP_X25519,
		(xbytesview) { PublicKey, sizeof(PublicKey) }, &Verify,
		Body, sizeof(Body)
	) && xrtTls12ServerKeyExchangeParse(
		(xbytesview) { Body, iBodySize }, &Parsed
	) && (Parsed.Group == XTLS_GROUP_X25519) &&
		(Parsed.Parameters.Size == 36u) &&
		(Parsed.Verify.Signature.Size == sizeof(Signature)),
		"TLS 1.2 ServerKeyExchange writer round-trip mismatch");

	memcpy(Body + 8u, PublicKey, sizeof(PublicKey));
	iBodySize = xrtTls12ClientKeyExchangeSize(
		(xbytesview) { Body + 8u, sizeof(PublicKey) }
	);
	testRequire((iBodySize == 33u) && xrtTls12ClientKeyExchangeEncode(
		(xbytesview) { Body + 8u, sizeof(PublicKey) }, Body, sizeof(Body)
	) && xrtTls12ClientKeyExchangeParse(
		(xbytesview) { Body, iBodySize }, &ParsedKey
	) && (ParsedKey.Size == sizeof(PublicKey)) &&
		(memcmp(ParsedKey.Data, PublicKey, sizeof(PublicKey)) == 0),
		"TLS 1.2 ClientKeyExchange in-place writer mismatch");
}



/* 单负载认证消息 writer 必须支持原位移动并严格 round-trip。 */
static void testTlsCertificatePayloadWriters(void)
{
	uint8 Body[64];
	uint8 Expected[8];
	xtlscertificatestatusmessage Status;
	xtlscertificatestatusmessage ParsedStatus;
	xtlscompressedcertificate Certificate;
	xtlscompressedcertificate ParsedCertificate;
	size_t iBodySize;

	for ( size_t i = 0; i < sizeof(Expected); i++ ) {
		Expected[i] = (uint8)(0xA0u + i);
	}
	memcpy(Body + 3u, Expected, sizeof(Expected));
	Status.Type = XTLS_CERTIFICATE_STATUS_OCSP;
	Status.Response = (xbytesview) { Body + 3u, sizeof(Expected) };
	iBodySize = xrtTlsCertificateStatusSize(&Status);
	testRequire((iBodySize == 12u) && xrtTlsCertificateStatusEncode(
		&Status, Body, sizeof(Body)
	) && xrtTlsCertificateStatusParse(
		(xbytesview) { Body, iBodySize }, &ParsedStatus
	) && (memcmp(
		ParsedStatus.Response.Data, Expected, sizeof(Expected)
	) == 0), "TLS CertificateStatus in-place writer mismatch");

	memcpy(Body + 2u, Expected, sizeof(Expected));
	Certificate.Algorithm = XTLS_CERTIFICATE_COMPRESSION_ZSTD;
	Certificate.UncompressedSize = 4096u;
	Certificate.Data = (xbytesview) { Body + 2u, sizeof(Expected) };
	iBodySize = xrtTlsCompressedCertificateSize(&Certificate);
	testRequire((iBodySize == 13u) && xrtTlsCompressedCertificateEncode(
		&Certificate, Body, sizeof(Body)
	) && xrtTlsCompressedCertificateParse(
		(xbytesview) { Body, iBodySize }, &ParsedCertificate
	) && (ParsedCertificate.UncompressedSize == 4096u) &&
		(memcmp(
			ParsedCertificate.Data.Data, Expected, sizeof(Expected)
		) == 0), "TLS CompressedCertificate in-place writer mismatch");
}



/* 执行 TLS 认证消息 writer 正向回归。 */
int main(void)
{
	testTlsCertificateRequestWriters();
	testTls12KeyExchangeWriters();
	testTlsCertificatePayloadWriters();
	return 0;
}
