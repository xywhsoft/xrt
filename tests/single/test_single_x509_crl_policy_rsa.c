#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_crl_vectors.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的真实 RSA CRL 策略验证和状态查询。 */
int main(void)
{
	static const uint8 Serial[] = { 0x20, 0x02 };
	xx509cert Issuer;
	xx509cert Certificate;
	xx509crl Crl;
	xx509crlconfig Config;
	xx509crlvalid Valid;
	xx509revocationcheck Check;
	xx509revocation Revocation;
	xx509revocation Result;
	xtime iTime;

	if ( !xrtX509Parse(
		X509_CRL_LEGACY_ROOT, sizeof(X509_CRL_LEGACY_ROOT), &Issuer
	) || !xrtX509CrlParse(
		X509_CRL_LEGACY_REVOKED, sizeof(X509_CRL_LEGACY_REVOKED), &Crl
	) || !xrtDateTime(2026, 4, 9, 0, 0, 0, 0, &iTime) ) {
		return 1;
	}
	memset(&Config, 0, sizeof(Config));
	Config.Time = iTime;
	if ( !xrtX509CrlValidate(&Crl, &Issuer, &Config, &Valid) ) {
		return 1;
	}

	memset(&Certificate, 0, sizeof(Certificate));
	Certificate.Version = X509_VERSION_3;
	Certificate.Issuer = Issuer.Subject;
	Certificate.Serial = (xbytesview) { Serial, sizeof(Serial) };
	if ( (xrtX509CrlCheck(
		&Valid, &Certificate, &Revocation
	) != X509_VALUE) ||
		(Revocation.State != X509_REVOCATION_REVOKED) ) {
		return 1;
	}
	xrtX509RevocationInit(&Check);
	if ( (xrtX509RevocationUpdate(&Check, &Revocation) != X509_VALUE) ||
		(xrtX509RevocationResult(&Check, &Result) != X509_VALUE) ||
		(Result.State != X509_REVOCATION_REVOKED) ) {
		return 1;
	}
	printf("[PASS] single-x509-crl-policy-rsa\n");
	return 0;
}
