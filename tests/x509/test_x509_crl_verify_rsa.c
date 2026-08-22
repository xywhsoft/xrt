#include "../test.h"
#include "../fixtures/x509_crl_vectors.h"
#include "../fixtures/x509_legacy_cert.h"



/* 验证旧版真实 CRL 的 RSA 签名、错误签发者和损坏签名。 */
int main(void)
{
	static const struct {
		const uint8* Data;
		size_t Size;
	} Cases[] = {
		{ X509_CRL_LEGACY_REVOKED, sizeof(X509_CRL_LEGACY_REVOKED) },
		{ X509_CRL_LEGACY_EMPTY, sizeof(X509_CRL_LEGACY_EMPTY) },
		{ X509_CRL_LEGACY_EXPIRED, sizeof(X509_CRL_LEGACY_EXPIRED) }
	};
	uint8 Damaged[sizeof(X509_CRL_LEGACY_REVOKED)];
	xx509cert Issuer;
	xx509cert WrongIssuer;
	xx509pubkey PublicKey;
	xx509crl Crl;

	testRequire(xrtX509Parse(
		X509_CRL_LEGACY_ROOT, sizeof(X509_CRL_LEGACY_ROOT), &Issuer
	) && xrtX509PublicKey(&Issuer, &PublicKey),
		"legacy CRL issuer certificate parse failed");
	for ( size_t i = 0; i < sizeof(Cases) / sizeof(Cases[0]); i++ ) {
		testRequire(xrtX509CrlParse(
			Cases[i].Data, Cases[i].Size, &Crl
		) && xrtX509CrlVerifyKey(&Crl, &PublicKey) &&
			xrtX509CrlVerify(&Crl, &Issuer),
			"legacy RSA CRL signature verification failed");
	}
	testRequire(xrtX509Parse(
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT), &WrongIssuer
	) && !xrtX509CrlVerify(&Crl, &WrongIssuer),
		"CRL signature accepted an unrelated issuer key");
	memcpy(Damaged, X509_CRL_LEGACY_REVOKED, sizeof(Damaged));
	Damaged[sizeof(Damaged) - 1u] ^= UINT8_C(1);
	testRequire(xrtX509CrlParse(Damaged, sizeof(Damaged), &Crl) &&
		!xrtX509CrlVerify(&Crl, &Issuer) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_SIGNATURE) &&
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(xrtGetError())), "xrt.crypto"
		) == 0), "damaged CRL signature or cause chain mismatch");
	printf("[PASS] x509_crl_verify_rsa\n");
	return 0;
}
