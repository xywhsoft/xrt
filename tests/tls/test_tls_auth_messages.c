#include "../test.h"



/* 两个版本的 CertificateRequest 必须发布完整且可遍历的认证选择。 */
static void testTlsCertificateRequests(void)
{
	static const uint8 Tls12[] = {
		2, 1, 64,
		0, 4, 0x04, 0x03, 0x08, 0x04,
		0, 9, 0, 3, 0x30, 1, 0, 0, 2, 0x30, 0
	};
	static const uint8 Tls13[] = {
		2, 0xAA, 0xBB, 0, 34,
		0, 13, 0, 6, 0, 4, 0x04, 0x03, 0x08, 0x04,
		0, 50, 0, 4, 0, 2, 0x08, 0x07,
		0, 47, 0, 7, 0, 5, 0, 3, 0x30, 1, 0,
		0x12, 0x34, 0, 1, 0xA5
	};
	static const uint8 EmptyAuthorities[] = { 0, 0 };
	static const uint8 Tls12EmptyAuthorities[] = {
		1, 1, 0, 2, 0x04, 0x03, 0, 0
	};
	xtls12certificaterequest Request12;
	xtls13certificaterequest Request13;
	xtlsauthoritycursor Cursor;
	xtlsextension Unknown;
	xbytesview Name;
	uint16 iValue;

	testRequire(xrtTls12CertificateRequestParse(
		(xbytesview) { Tls12, sizeof(Tls12) }, &Request12
	) && (Request12.CertificateTypes.Size == 2u) &&
		(Request12.CertificateTypes.Data[1] == XTLS_CERTIFICATE_ECDSA_SIGN) &&
		(xrtTlsIdsCount(&Request12.Signatures) == 2u) &&
		xrtTlsIdsGet(&Request12.Signatures, 1u, &iValue) &&
		(iValue == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256) &&
		xrtTlsAuthorities(Request12.AuthorityData, &Cursor) &&
		(xrtTlsAuthoritiesRead(&Cursor, &Name) == XTLS_ITEM_VALUE) &&
		(Name.Size == 3u) && (Name.Data[0] == 0x30u) &&
		(xrtTlsAuthoritiesRead(&Cursor, &Name) == XTLS_ITEM_VALUE) &&
		(Name.Size == 2u) &&
		(xrtTlsAuthoritiesRead(&Cursor, &Name) == XTLS_ITEM_DONE),
		"TLS 1.2 CertificateRequest view mismatch");

	testRequire(xrtTls13CertificateRequestParse(
		(xbytesview) { Tls13, sizeof(Tls13) }, &Request13
	) && (Request13.RequestContext.Size == 2u) &&
		(Request13.RequestContext.Data[1] == 0xBBu) &&
		(xrtTlsIdsCount(&Request13.Signatures) == 2u) &&
		(xrtTlsIdsCount(&Request13.CertificateSignatures) == 1u) &&
		xrtTlsAuthorities(Request13.AuthorityData, &Cursor) &&
		(xrtTlsAuthoritiesRead(&Cursor, &Name) == XTLS_ITEM_VALUE) &&
		(Name.Size == 3u) &&
		(xrtTlsAuthoritiesRead(&Cursor, &Name) == XTLS_ITEM_DONE) &&
		(xrtTlsExtensionsFind(
			Request13.Extensions, (xtlsextensiontype)0x1234, &Unknown
		) == XTLS_ITEM_VALUE) && (Unknown.Data.Size == 1u) &&
		(Unknown.Data.Data[0] == 0xA5u),
		"TLS 1.3 CertificateRequest view mismatch");

	testRequire(xrtTlsAuthorities(
		(xbytesview) { EmptyAuthorities, sizeof(EmptyAuthorities) }, &Cursor
	) && (xrtTlsAuthoritiesRead(&Cursor, &Name) == XTLS_ITEM_DONE) &&
		xrtTls12CertificateRequestParse(
			(xbytesview) {
				Tls12EmptyAuthorities, sizeof(Tls12EmptyAuthorities)
			}, &Request12
		) && (Request12.AuthorityData.Size == 2u),
		"TLS 1.2 rejected an empty certificate-authority list");
}



/* TLS 1.2 ECDHE 参数必须保留精确验签切片和不受支持组的线路值。 */
static void testTls12KeyExchanges(void)
{
	uint8 Server[43];
	uint8 Client[33];
	xtls12serverkeyexchange Exchange;
	xbytesview PublicKey;

	memset(Server, 0, sizeof(Server));
	Server[0] = 3u;
	Server[1] = 0u;
	Server[2] = 29u;
	Server[3] = 32u;
	for ( size_t i = 0; i < 32u; i++ ) {
		Server[4u + i] = (uint8)i;
	}
	Server[36] = 0x08u;
	Server[37] = 0x07u;
	Server[38] = 0u;
	Server[39] = 3u;
	Server[40] = 0xA1u;
	Server[41] = 0xA2u;
	Server[42] = 0xA3u;

	testRequire(xrtTls12ServerKeyExchangeParse(
		(xbytesview) { Server, sizeof(Server) }, &Exchange
	) && (Exchange.Group == XTLS_GROUP_X25519) &&
		(Exchange.PublicKey.Size == 32u) &&
		(Exchange.Parameters.Data == Server) &&
		(Exchange.Parameters.Size == 36u) &&
		(Exchange.Verify.Scheme == XTLS_SIGNATURE_ED25519) &&
		(Exchange.Verify.Signature.Size == 3u),
		"TLS 1.2 ServerKeyExchange view mismatch");

	Server[1] = 0xFEu;
	Server[2] = 0x01u;
	testRequire(xrtTls12ServerKeyExchangeParse(
		(xbytesview) { Server, sizeof(Server) }, &Exchange
	) && (Exchange.Group == UINT16_C(0xFE01)),
		"TLS 1.2 parser prematurely rejected an unknown named group");

	Client[0] = 32u;
	memcpy(Client + 1u, Server + 4u, 32u);
	testRequire(xrtTls12ClientKeyExchangeParse(
		(xbytesview) { Client, sizeof(Client) }, &PublicKey
	) && (PublicKey.Data == Client + 1u) && (PublicKey.Size == 32u),
		"TLS 1.2 ClientKeyExchange view mismatch");
}



/* OCSP 与压缩证书消息必须发布不透明负载并保留算法线路值。 */
static void testTlsCertificatePayloads(void)
{
	static const uint8 StatusBody[] = {
		1, 0, 0, 3, 1, 2, 3
	};
	static const uint8 CompressedBody[] = {
		0, 2, 0, 0, 9, 0xAA, 0xBB
	};
	xtlscertificatestatusmessage Status;
	xtlscompressedcertificate Certificate;
	uint8 UnknownCompressed[sizeof(CompressedBody)];

	testRequire(xrtTlsCertificateStatusParse(
		(xbytesview) { StatusBody, sizeof(StatusBody) }, &Status
	) && (Status.Type == XTLS_CERTIFICATE_STATUS_OCSP) &&
		(Status.Response.Size == 3u) && (Status.Response.Data[2] == 3u),
		"TLS CertificateStatus view mismatch");
	testRequire(xrtTlsCompressedCertificateParse(
		(xbytesview) { CompressedBody, sizeof(CompressedBody) }, &Certificate
	) && (Certificate.Algorithm == XTLS_CERTIFICATE_COMPRESSION_BROTLI) &&
		(Certificate.UncompressedSize == 9u) &&
		(Certificate.Data.Size == 2u) && (Certificate.Data.Data[1] == 0xBBu),
		"TLS CompressedCertificate view mismatch");

	memcpy(UnknownCompressed, CompressedBody, sizeof(UnknownCompressed));
	UnknownCompressed[0] = 0xFEu;
	UnknownCompressed[1] = 0x01u;
	testRequire(xrtTlsCompressedCertificateParse(
		(xbytesview) { UnknownCompressed, sizeof(UnknownCompressed) },
		&Certificate
	) && (Certificate.Algorithm == UINT16_C(0xFE01)),
		"TLS CompressedCertificate prematurely rejected an unknown algorithm");
}



/* 执行 TLS 认证消息正向回归。 */
int main(void)
{
	testTlsCertificateRequests();
	testTls12KeyExchanges();
	testTlsCertificatePayloads();
	return 0;
}
