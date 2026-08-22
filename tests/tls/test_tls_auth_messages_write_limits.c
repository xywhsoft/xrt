#include "../test.h"



/* 颁发者向量必须覆盖完整 16 位总长且不限制名称数量。 */
static void testTlsAuthorityLimits(void)
{
	uint8* pName;
	uint8* pOutput;
	xbytesview Name;
	xtlsauthoritycursor Cursor;
	xbytesview Parsed;
	size_t iRequired;

	pName = (uint8*)malloc(65533u);
	testRequire(pName != NULL, "TLS authority limit allocation failed");
	memset(pName, 0x5A, 65533u);
	Name = (xbytesview) { pName, 65533u };
	iRequired = xrtTlsAuthoritiesSize(&Name, 1u);
	pOutput = (uint8*)malloc(iRequired);
	testRequire((iRequired == 65537u) && (pOutput != NULL) &&
		xrtTlsAuthoritiesEncode(&Name, 1u, pOutput, iRequired) &&
		xrtTlsAuthorities(
			(xbytesview) { pOutput, iRequired }, &Cursor
		) && (xrtTlsAuthoritiesRead(&Cursor, &Parsed) == XTLS_ITEM_VALUE) &&
		(Parsed.Size == 65533u) &&
		(xrtTlsAuthoritiesRead(&Cursor, &Parsed) == XTLS_ITEM_DONE),
		"TLS authority writer retained a smaller-than-wire limit");
	free(pOutput);
	free(pName);
}



/* ECPoint 与签名必须支持各自完整线路上限。 */
static void testTlsKeyExchangeLimits(void)
{
	uint8 PublicKey[UINT8_MAX];
	uint8* pSignature;
	uint8* pBody;
	xtlscertificateverify Verify;
	xtls12serverkeyexchange Parsed;
	xbytesview ClientKey;
	size_t iRequired;

	memset(PublicKey, 0xA5, sizeof(PublicKey));
	pSignature = (uint8*)malloc(UINT16_MAX);
	testRequire(pSignature != NULL,
		"TLS key-exchange signature limit allocation failed");
	memset(pSignature, 0x5A, UINT16_MAX);
	Verify.Scheme = UINT16_C(0xFE01);
	Verify.Signature = (xbytesview) { pSignature, UINT16_MAX };
	iRequired = xrtTls12ServerKeyExchangeSize(
		UINT16_C(0xFE02),
		(xbytesview) { PublicKey, sizeof(PublicKey) }, &Verify
	);
	pBody = (uint8*)malloc(iRequired);
	testRequire((iRequired == 65798u) && (pBody != NULL) &&
		xrtTls12ServerKeyExchangeEncode(
			UINT16_C(0xFE02),
			(xbytesview) { PublicKey, sizeof(PublicKey) }, &Verify,
			pBody, iRequired
		) && xrtTls12ServerKeyExchangeParse(
			(xbytesview) { pBody, iRequired }, &Parsed
		) && (Parsed.PublicKey.Size == UINT8_MAX) &&
		(Parsed.Verify.Signature.Size == UINT16_MAX),
		"TLS ServerKeyExchange rejected an exact wire limit");
	free(pBody);
	free(pSignature);

	iRequired = xrtTls12ClientKeyExchangeSize(
		(xbytesview) { PublicKey, sizeof(PublicKey) }
	);
	pBody = (uint8*)malloc(iRequired);
	testRequire((pBody != NULL) && xrtTls12ClientKeyExchangeEncode(
		(xbytesview) { PublicKey, sizeof(PublicKey) }, pBody, iRequired
	) && xrtTls12ClientKeyExchangeParse(
		(xbytesview) { pBody, iRequired }, &ClientKey
	) && (ClientKey.Size == UINT8_MAX),
		"TLS ClientKeyExchange rejected a 255-byte ECPoint");
	free(pBody);
}



/* 多字段 writer 的容量与重叠失败必须保持输出完全不变。 */
static void testTlsAuthenticationWriteAtomicity(void)
{
	static const uint8 Types[] = { XTLS_CERTIFICATE_RSA_SIGN };
	static const uint8 Signatures[] = { 4, 3 };
	static const uint8 EmptyAuthorities[] = { 0, 0 };
	static const uint8 Extensions[] = {
		0, 13, 0, 4, 0, 2, 4, 3
	};
	static const uint8 Signature[] = { 1 };
	uint8 Output[64];
	uint8 Before[64];
	xtls12certificaterequest Request;
	xtlscertificateverify Verify;
	size_t iRequired;

	memset(&Request, 0, sizeof(Request));
	Request.CertificateTypes = (xbytesview) { Types, sizeof(Types) };
	Request.Signatures.Data = (xbytesview) {
		Signatures, sizeof(Signatures)
	};
	Request.AuthorityData = (xbytesview) {
		EmptyAuthorities, sizeof(EmptyAuthorities)
	};
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	iRequired = xrtTls12CertificateRequestSize(&Request);
	testRequire(!xrtTls12CertificateRequestEncode(
		&Request, Output, iRequired - 1u
	) && (memcmp(Output, Before, sizeof(Output)) == 0),
		"short TLS 1.2 CertificateRequest output changed its buffer");

	iRequired = xrtTls13CertificateRequestSize(
		(xbytesview) { NULL, 0 },
		(xbytesview) { Extensions, sizeof(Extensions) }
	);
	memcpy(Output + 4u, Extensions, sizeof(Extensions));
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtTls13CertificateRequestEncode(
		(xbytesview) { NULL, 0 },
		(xbytesview) { Output + 4u, sizeof(Extensions) },
		Output, iRequired
	) && (memcmp(Output, Before, sizeof(Output)) == 0),
		"overlapping TLS 1.3 CertificateRequest changed its output");

	Verify.Scheme = XTLS_SIGNATURE_ED25519;
	Verify.Signature = (xbytesview) { Signature, sizeof(Signature) };
	testRequire(!xrtTls12ServerKeyExchangeEncode(
		XTLS_GROUP_X25519, (xbytesview) { Output + 4u, 1u },
		&Verify, Output, sizeof(Output)
	) && (memcmp(Output, Before, sizeof(Output)) == 0),
		"overlapping TLS ServerKeyExchange changed its output");
}



/* 单负载 writer 必须先快照位于输出中的描述结构。 */
static void testTlsAuthenticationDescriptorAlias(void)
{
	static const uint8 Payload[] = { 1, 2, 3 };
	union {
		xtlscertificatestatusmessage Status;
		uint8 Data[64];
	} StatusStorage;
	union {
		xtlscompressedcertificate Certificate;
		uint8 Data[64];
	} CertificateStorage;
	xtlscertificatestatusmessage ParsedStatus;
	xtlscompressedcertificate ParsedCertificate;
	size_t iRequired;

	memset(&StatusStorage, 0, sizeof(StatusStorage));
	StatusStorage.Status.Type = XTLS_CERTIFICATE_STATUS_OCSP;
	StatusStorage.Status.Response = (xbytesview) {
		Payload, sizeof(Payload)
	};
	iRequired = xrtTlsCertificateStatusSize(&StatusStorage.Status);
	testRequire(xrtTlsCertificateStatusEncode(
		&StatusStorage.Status, StatusStorage.Data, sizeof(StatusStorage.Data)
	) && xrtTlsCertificateStatusParse(
		(xbytesview) { StatusStorage.Data, iRequired }, &ParsedStatus
	) && (ParsedStatus.Response.Size == sizeof(Payload)),
		"TLS CertificateStatus descriptor alias corrupted metadata");

	memset(&CertificateStorage, 0, sizeof(CertificateStorage));
	CertificateStorage.Certificate.Algorithm =
		XTLS_CERTIFICATE_COMPRESSION_ZLIB;
	CertificateStorage.Certificate.UncompressedSize = 10u;
	CertificateStorage.Certificate.Data = (xbytesview) {
		Payload, sizeof(Payload)
	};
	iRequired = xrtTlsCompressedCertificateSize(
		&CertificateStorage.Certificate
	);
	testRequire(xrtTlsCompressedCertificateEncode(
		&CertificateStorage.Certificate, CertificateStorage.Data,
		sizeof(CertificateStorage.Data)
	) && xrtTlsCompressedCertificateParse(
		(xbytesview) { CertificateStorage.Data, iRequired }, &ParsedCertificate
	) && (ParsedCertificate.UncompressedSize == 10u) &&
		(ParsedCertificate.Data.Size == sizeof(Payload)),
		"TLS CompressedCertificate descriptor alias corrupted metadata");
}



/* 执行 TLS 认证消息 writer 边界回归。 */
int main(void)
{
	testTlsAuthorityLimits();
	testTlsKeyExchangeLimits();
	testTlsAuthenticationWriteAtomicity();
	testTlsAuthenticationDescriptorAlias();
	return 0;
}
