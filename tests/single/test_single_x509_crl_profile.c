#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#include "../fixtures/x509_crl_profile_vectors.h"
#include "../fixtures/x509_distribution_vectors.h"



/* 验证单头文件发布包含 CRL profile 的类型化零分配入口。 */
int main(void)
{
	xx509issuingpoint Point;
	xx509distributioncursor Points;
	xx509crl Crl;
	xbytesview Number;

	memset(&Crl, 0, sizeof(Crl));
	Crl.Extensions = (xbytesview) {
		X509_DISTRIBUTION_FRESHEST_EXTENSIONS,
		sizeof(X509_DISTRIBUTION_FRESHEST_EXTENSIONS)
	};

	if ( !xrtX509CrlNumberParse(
		(xbytesview) {
			X509_CRL_PROFILE_LARGE_NUMBER,
			sizeof(X509_CRL_PROFILE_LARGE_NUMBER)
		}, &Number
	) || (Number.Size != 8u) || !xrtX509IssuingPointParse(
		(xbytesview) {
			X509_CRL_PROFILE_ISSUING_POINT,
			sizeof(X509_CRL_PROFILE_ISSUING_POINT)
		}, &Point
	) || !Point.HasDistributionPoint || !Point.Indirect ||
		(xrtX509CrlFreshest(&Crl, &Points) != X509_VALUE) ) {
		return 1;
	}
	return 0;
}
