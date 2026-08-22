#include "../bench_common.h"

#define XRT_MODULE_X509_VERIFY_RSA
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"
#include "../../../tests/fixtures/x509_legacy_cert.h"



/* 测量真实 DER 证书的零分配解析与 RSA 自签名验签公开路径。 */
int main(int argc, char** argv)
{
	uint32 iParseCount = xbenchArgU32(argc, argv, 1, 100000u);
	uint32 iVerifyCount = xbenchArgU32(argc, argv, 2, 2000u);
	xx509cert Certificate;
	xx509pubkey PublicKey;
	xbenchtimer Timer;
	uint64 iParseElapsed;
	uint64 iVerifyElapsed;

	if ( (iParseCount == 0u) || (iVerifyCount == 0u) ) {
		fprintf(stderr, "invalid X.509 benchmark arguments.\n");
		return 1;
	}
	xbenchApplyCpuPinFromEnv();
	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iParseCount; i++ ) {
		if ( !xrtX509Parse(
			X509_LEGACY_RSA_CERT,
			sizeof(X509_LEGACY_RSA_CERT),
			&Certificate
		) ) {
			return 2;
		}
	}
	xbenchTimerStop(&Timer);
	iParseElapsed = xbenchTimerElapsedNs(&Timer);

	if ( !xrtX509PublicKey(&Certificate, &PublicKey) ) {
		return 3;
	}
	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iVerifyCount; i++ ) {
		if ( !xrtX509CertificateVerifyKey(&Certificate, &PublicKey) ) {
			return 4;
		}
	}
	xbenchTimerStop(&Timer);
	iVerifyElapsed = xbenchTimerElapsedNs(&Timer);

	printf(
		"x509_parse_ops_per_sec: %.3f\n",
		xbenchSafeRate(iParseCount, iParseElapsed)
	);
	printf(
		"x509_rsa_verify_ops_per_sec: %.3f\n",
		xbenchSafeRate(iVerifyCount, iVerifyElapsed)
	);
	return 0;
}
