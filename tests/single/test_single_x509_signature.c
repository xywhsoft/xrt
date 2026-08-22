#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_signature_vectors.h"

#include <stdio.h>



/* 验证单头文件中的 X.509 PSS 签名算法解析。 */
int main(void)
{
	xx509algorithm Algorithm;
	xx509signature Signature;

	if ( !xrtX509AlgorithmParse(
		(xbytesview) {
			X509_SIGNATURE_PSS_CUSTOM,
			sizeof(X509_SIGNATURE_PSS_CUSTOM)
		}, &Algorithm
	) || (xrtX509SignatureParse(
		&Algorithm, &Signature
	) != X509_VALUE) || (Signature.Type != X509_SIGNATURE_RSA_PSS) ||
		(Signature.Hash != X509_HASH_SHA256) ||
		(Signature.MaskHash != X509_HASH_SHA384) ||
		(Signature.SaltSize != 32u) ) {
		return 1;
	}
	printf("[PASS] single-x509-signature\n");
	return 0;
}
