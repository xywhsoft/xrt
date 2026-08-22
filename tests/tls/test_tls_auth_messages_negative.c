#include "../test.h"



/* 颁发者向量必须精确消费总长并拒绝空或截断名称。 */
static void testTlsAuthorityFailures(void)
{
	static const uint8 ShortOuter[] = { 0, 2, 0 };
	static const uint8 EmptyName[] = { 0, 2, 0, 0 };
	static const uint8 ShortName[] = { 0, 4, 0, 3, 0x30, 0 };
	xtlsauthoritycursor Cursor;

	testRequire(!xrtTlsAuthorities(
		(xbytesview) { NULL, 0 }, &Cursor
	), "TLS authority parser accepted a missing outer length");
	testRequire(!xrtTlsAuthorities(
		(xbytesview) { ShortOuter, sizeof(ShortOuter) }, &Cursor
	), "TLS authority parser accepted an inconsistent outer length");
	testRequire(!xrtTlsAuthorities(
		(xbytesview) { EmptyName, sizeof(EmptyName) }, &Cursor
	), "TLS authority parser accepted an empty name");
	testRequire(!xrtTlsAuthorities(
		(xbytesview) { ShortName, sizeof(ShortName) }, &Cursor
	), "TLS authority parser accepted a truncated name");
}



/* CertificateRequest 必须拒绝缺失必选扩展、重复值和空 TLS 1.3 CA。 */
static void testTlsCertificateRequestFailures(void)
{
	static const uint8 Tls12NoTypes[] = { 0, 0, 2, 4, 3, 0, 0 };
	static const uint8 Tls12OddSignatures[] = {
		1, 1, 0, 3, 4, 3, 8, 0, 0
	};
	static const uint8 Tls12DuplicateSignatures[] = {
		1, 1, 0, 4, 4, 3, 4, 3, 0, 0
	};
	static const uint8 Tls13MissingSignatures[] = {
		0, 0, 5, 0x12, 0x34, 0, 1, 0
	};
	static const uint8 Tls13DuplicateExtension[] = {
		0, 0, 16,
		0, 13, 0, 4, 0, 2, 4, 3,
		0, 13, 0, 4, 0, 2, 8, 7
	};
	static const uint8 Tls13EmptyAuthority[] = {
		0, 0, 14,
		0, 13, 0, 4, 0, 2, 4, 3,
		0, 47, 0, 2, 0, 0
	};
	xtls12certificaterequest Request12;
	xtls13certificaterequest Request13;

	testRequire(!xrtTls12CertificateRequestParse(
		(xbytesview) { Tls12NoTypes, sizeof(Tls12NoTypes) }, &Request12
	), "TLS 1.2 CertificateRequest accepted no certificate types");
	testRequire(!xrtTls12CertificateRequestParse(
		(xbytesview) { Tls12OddSignatures, sizeof(Tls12OddSignatures) }, &Request12
	), "TLS 1.2 CertificateRequest accepted an odd signature list");
	testRequire(!xrtTls12CertificateRequestParse(
		(xbytesview) {
			Tls12DuplicateSignatures, sizeof(Tls12DuplicateSignatures)
		}, &Request12
	), "TLS 1.2 CertificateRequest accepted duplicate signatures");
	testRequire(!xrtTls13CertificateRequestParse(
		(xbytesview) {
			Tls13MissingSignatures, sizeof(Tls13MissingSignatures)
		}, &Request13
	), "TLS 1.3 CertificateRequest accepted no signature_algorithms");
	testRequire(!xrtTls13CertificateRequestParse(
		(xbytesview) {
			Tls13DuplicateExtension, sizeof(Tls13DuplicateExtension)
		}, &Request13
	), "TLS 1.3 CertificateRequest accepted duplicate extensions");
	testRequire(!xrtTls13CertificateRequestParse(
		(xbytesview) { Tls13EmptyAuthority, sizeof(Tls13EmptyAuthority) },
		&Request13
	), "TLS 1.3 CertificateRequest accepted an empty authority list");
}



/* KeyExchange、CertificateStatus 与压缩证书必须采用精确长度。 */
static void testTlsAuthenticationPayloadFailures(void)
{
	static const uint8 BadCurveType[] = {
		2, 0, 29, 1, 0, 8, 7, 0, 1, 0
	};
	static const uint8 EmptyServerKey[] = {
		3, 0, 29, 0, 8, 7, 0, 1, 0
	};
	static const uint8 ClientTrailing[] = { 1, 0xAA, 0xBB };
	static const uint8 UnknownStatus[] = { 2, 0, 0, 1, 0 };
	static const uint8 StatusTrailing[] = { 1, 0, 0, 1, 0, 1 };
	static const uint8 EmptyCompressed[] = { 0, 1, 0, 0, 1 };
	static const uint8 ZeroUncompressed[] = { 0, 1, 0, 0, 0, 1 };
	xtls12serverkeyexchange Exchange;
	xtlscertificatestatusmessage Status;
	xtlscompressedcertificate Certificate;
	xbytesview PublicKey;

	testRequire(!xrtTls12ServerKeyExchangeParse(
		(xbytesview) { BadCurveType, sizeof(BadCurveType) }, &Exchange
	), "TLS ServerKeyExchange accepted a non-named curve type");
	testRequire(!xrtTls12ServerKeyExchangeParse(
		(xbytesview) { EmptyServerKey, sizeof(EmptyServerKey) }, &Exchange
	), "TLS ServerKeyExchange accepted an empty public key");
	testRequire(!xrtTls12ClientKeyExchangeParse(
		(xbytesview) { ClientTrailing, sizeof(ClientTrailing) }, &PublicKey
	), "TLS ClientKeyExchange accepted trailing bytes");
	testRequire(!xrtTlsCertificateStatusParse(
		(xbytesview) { UnknownStatus, sizeof(UnknownStatus) }, &Status
	), "TLS CertificateStatus accepted an unknown response shape");
	testRequire(!xrtTlsCertificateStatusParse(
		(xbytesview) { StatusTrailing, sizeof(StatusTrailing) }, &Status
	), "TLS CertificateStatus accepted trailing bytes");
	testRequire(!xrtTlsCompressedCertificateParse(
		(xbytesview) { EmptyCompressed, sizeof(EmptyCompressed) }, &Certificate
	), "TLS CompressedCertificate accepted no compressed data");
	testRequire(!xrtTlsCompressedCertificateParse(
		(xbytesview) { ZeroUncompressed, sizeof(ZeroUncompressed) }, &Certificate
	), "TLS CompressedCertificate accepted a zero output length");
}



/* 解析失败不得发布部分填充的输出对象。 */
static void testTlsAuthenticationParseAtomicity(void)
{
	static const uint8 BadRequest[] = { 0, 0, 0 };
	static const uint8 BadStatus[] = { 1, 0, 0, 0 };
	xtls13certificaterequest Request;
	xtlscertificatestatusmessage Status;
	xtls13certificaterequest RequestBefore;
	xtlscertificatestatusmessage StatusBefore;

	memset(&Request, 0xA5, sizeof(Request));
	memset(&Status, 0x5A, sizeof(Status));
	RequestBefore = Request;
	StatusBefore = Status;
	testRequire(!xrtTls13CertificateRequestParse(
		(xbytesview) { BadRequest, sizeof(BadRequest) }, &Request
	) && (memcmp(&Request, &RequestBefore, sizeof(Request)) == 0),
		"failed CertificateRequest parse changed its output");
	testRequire(!xrtTlsCertificateStatusParse(
		(xbytesview) { BadStatus, sizeof(BadStatus) }, &Status
	) && (memcmp(&Status, &StatusBefore, sizeof(Status)) == 0),
		"failed CertificateStatus parse changed its output");
}



/* 执行 TLS 认证消息负向回归。 */
int main(void)
{
	testTlsAuthorityFailures();
	testTlsCertificateRequestFailures();
	testTlsAuthenticationPayloadFailures();
	testTlsAuthenticationParseAtomicity();
	return 0;
}
