#include "../test_allocator.h"
#include "../fixtures/x509_crl_profile_vectors.h"
#include "../fixtures/x509_distribution_vectors.h"



/* 验证全部 CRL profile 有效路径保持零分配。 */
int main(void)
{
	xx509crl Crl;
	xx509crlentry Entry;
	xx509issuingpoint Point;
	xx509authoritykeyid Authority;
	xx509gencursor Issuer;
	xx509distributioncursor Points;
	xbytesview Number;
	xx509crlreason Reason;
	xtime iTime;

	memset(&Crl, 0, sizeof(Crl));
	memset(&Entry, 0, sizeof(Entry));
	testRequire(testInstallFailAllocator(),
		"CRL profile failure allocator install failed");

	Crl.Extensions = (xbytesview) {
		X509_CRL_PROFILE_NUMBER_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_NUMBER_EXTENSIONS)
	};
	testRequire(xrtX509CrlNumber(&Crl, &Number) == X509_VALUE,
		"CRLNumber allocated memory");
	Crl.Extensions = (xbytesview) {
		X509_CRL_PROFILE_DELTA_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_DELTA_EXTENSIONS)
	};
	testRequire(xrtX509CrlDeltaBase(&Crl, &Number) == X509_VALUE,
		"DeltaCRLIndicator allocated memory");
	Crl.Extensions = (xbytesview) {
		X509_CRL_PROFILE_AUTHORITY_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_AUTHORITY_EXTENSIONS)
	};
	testRequire(xrtX509CrlAuthorityKeyId(&Crl, &Authority) == X509_VALUE,
		"CRL AuthorityKeyIdentifier allocated memory");
	Crl.Extensions = (xbytesview) {
		X509_CRL_PROFILE_ISSUING_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_ISSUING_EXTENSIONS)
	};
	testRequire(xrtX509CrlIssuingPoint(&Crl, &Point) == X509_VALUE,
		"IssuingDistributionPoint allocated memory");
	Crl.Extensions = (xbytesview) {
		X509_DISTRIBUTION_FRESHEST_EXTENSIONS,
		sizeof(X509_DISTRIBUTION_FRESHEST_EXTENSIONS)
	};
	testRequire(xrtX509CrlFreshest(&Crl, &Points) == X509_VALUE,
		"CRL FreshestCRL allocated memory");

	Entry.Extensions = (xbytesview) {
		X509_CRL_PROFILE_REASON_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_REASON_EXTENSIONS)
	};
	testRequire(xrtX509CrlEntryReason(&Entry, &Reason) == X509_VALUE,
		"CRLReason allocated memory");
	Entry.Extensions = (xbytesview) {
		X509_CRL_PROFILE_INVALIDITY_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_INVALIDITY_EXTENSIONS)
	};
	testRequire(xrtX509CrlEntryInvalidityDate(&Entry, &iTime) == X509_VALUE,
		"InvalidityDate allocated memory");
	Entry.Extensions = (xbytesview) {
		X509_CRL_PROFILE_ISSUER_EXTENSIONS,
		sizeof(X509_CRL_PROFILE_ISSUER_EXTENSIONS)
	};
	testRequire(xrtX509CrlEntryIssuer(&Entry, &Issuer) == X509_VALUE,
		"CertificateIssuer allocated memory");
	printf("[PASS] x509_crl_profile_oom\n");
	return 0;
}
