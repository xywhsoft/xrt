#include "../test.h"



/* 每个有效认证消息的所有真前缀和尾随变体都必须被拒绝。 */
static void testTlsAuthenticationMessageTruncation(void)
{
	static const uint8 Request12[] = {
		1, 1, 0, 2, 4, 3, 0, 0
	};
	static const uint8 Request13[] = {
		0, 0, 8, 0, 13, 0, 4, 0, 2, 4, 3
	};
	static const uint8 ServerKey[] = {
		3, 0, 29, 1, 0xAA, 8, 7, 0, 1, 0xBB
	};
	static const uint8 ClientKey[] = { 1, 0xAA };
	static const uint8 Status[] = { 1, 0, 0, 1, 0xAA };
	static const uint8 Compressed[] = { 0, 1, 0, 0, 1, 0xAA };
	xtls12certificaterequest Parsed12;
	xtls13certificaterequest Parsed13;
	xtls12serverkeyexchange Exchange;
	xtlscertificatestatusmessage ParsedStatus;
	xtlscompressedcertificate ParsedCompressed;
	xbytesview PublicKey;
	uint8 Extra[32];

	for ( size_t i = 0; i < sizeof(Request12); i++ ) {
		testRequire(!xrtTls12CertificateRequestParse(
			(xbytesview) { Request12, i }, &Parsed12
		), "TLS 1.2 CertificateRequest accepted a truncated prefix");
	}
	for ( size_t i = 0; i < sizeof(Request13); i++ ) {
		testRequire(!xrtTls13CertificateRequestParse(
			(xbytesview) { Request13, i }, &Parsed13
		), "TLS 1.3 CertificateRequest accepted a truncated prefix");
	}
	for ( size_t i = 0; i < sizeof(ServerKey); i++ ) {
		testRequire(!xrtTls12ServerKeyExchangeParse(
			(xbytesview) { ServerKey, i }, &Exchange
		), "TLS ServerKeyExchange accepted a truncated prefix");
	}
	for ( size_t i = 0; i < sizeof(ClientKey); i++ ) {
		testRequire(!xrtTls12ClientKeyExchangeParse(
			(xbytesview) { ClientKey, i }, &PublicKey
		), "TLS ClientKeyExchange accepted a truncated prefix");
	}
	for ( size_t i = 0; i < sizeof(Status); i++ ) {
		testRequire(!xrtTlsCertificateStatusParse(
			(xbytesview) { Status, i }, &ParsedStatus
		), "TLS CertificateStatus accepted a truncated prefix");
	}
	for ( size_t i = 0; i < sizeof(Compressed); i++ ) {
		testRequire(!xrtTlsCompressedCertificateParse(
			(xbytesview) { Compressed, i }, &ParsedCompressed
		), "TLS CompressedCertificate accepted a truncated prefix");
	}

	memcpy(Extra, Request12, sizeof(Request12));
	Extra[sizeof(Request12)] = 0;
	testRequire(!xrtTls12CertificateRequestParse(
		(xbytesview) { Extra, sizeof(Request12) + 1u }, &Parsed12
	), "TLS 1.2 CertificateRequest accepted trailing data");
	memcpy(Extra, Request13, sizeof(Request13));
	Extra[sizeof(Request13)] = 0;
	testRequire(!xrtTls13CertificateRequestParse(
		(xbytesview) { Extra, sizeof(Request13) + 1u }, &Parsed13
	), "TLS 1.3 CertificateRequest accepted trailing data");
	memcpy(Extra, ServerKey, sizeof(ServerKey));
	Extra[sizeof(ServerKey)] = 0;
	testRequire(!xrtTls12ServerKeyExchangeParse(
		(xbytesview) { Extra, sizeof(ServerKey) + 1u }, &Exchange
	), "TLS ServerKeyExchange accepted trailing data");
}



/* 执行 TLS 认证消息长度变异回归。 */
int main(void)
{
	testTlsAuthenticationMessageTruncation();
	return 0;
}
