#include "../test_allocator.h"
#include "../fixtures/x509_crl_vectors.h"



/* 验证真实 RSA CRL 的策略验证和状态查询全程不分配堆内存。 */
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

	testRequire(testInstallFailAllocator(),
		"CRL policy failure allocator install failed");
	testRequire(xrtX509Parse(
		X509_CRL_LEGACY_ROOT, sizeof(X509_CRL_LEGACY_ROOT), &Issuer
	), "CRL policy OOM issuer fixture failed to parse");
	testRequire(xrtX509CrlParse(
		X509_CRL_LEGACY_REVOKED, sizeof(X509_CRL_LEGACY_REVOKED), &Crl
	), "CRL policy OOM revoked fixture failed to parse");
	testRequire(xrtDateTime(2026, 4, 9, 0, 0, 0, 0, &iTime),
		"CRL policy OOM validation time construction failed");
	memset(&Config, 0, sizeof(Config));
	Config.Time = iTime;
	testRequire(xrtX509CrlValidate(&Crl, &Issuer, &Config, &Valid),
		"CRL policy validation allocated memory");

	memset(&Certificate, 0, sizeof(Certificate));
	Certificate.Version = X509_VERSION_3;
	Certificate.Issuer = Issuer.Subject;
	Certificate.Serial = (xbytesview) { Serial, sizeof(Serial) };
	testRequire((xrtX509CrlCheck(
		&Valid, &Certificate, &Revocation
	) == X509_VALUE) && (Revocation.State == X509_REVOCATION_REVOKED),
		"CRL policy query allocated memory or returned the wrong state");
	xrtX509RevocationInit(&Check);
	testRequire((xrtX509RevocationUpdate(
		&Check, &Revocation
	) == X509_VALUE) && (xrtX509RevocationResult(
		&Check, &Result
	) == X509_VALUE) && (Result.State == X509_REVOCATION_REVOKED),
		"CRL policy aggregation allocated memory or returned the wrong state");
	printf("[PASS] x509_crl_policy_rsa_oom\n");
	return 0;
}
