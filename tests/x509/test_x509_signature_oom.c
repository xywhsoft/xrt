#include "../test_allocator.h"
#include "../fixtures/x509_signature_vectors.h"



/* 验证签名算法有效路径只借用 DER 且不分配内存。 */
int main(void)
{
	xx509algorithm Algorithm;
	xx509signature Signature;

	testRequire(testInstallFailAllocator(),
		"X.509 signature failure allocator install failed");
	testRequire(xrtX509AlgorithmParse(
		(xbytesview) {
			X509_SIGNATURE_PSS_CUSTOM,
			sizeof(X509_SIGNATURE_PSS_CUSTOM)
		}, &Algorithm
	) && (xrtX509SignatureParse(
		&Algorithm, &Signature
	) == X509_VALUE) &&
		(Signature.Type == X509_SIGNATURE_RSA_PSS) &&
		(Signature.Hash == X509_HASH_SHA256) &&
		(Signature.MaskHash == X509_HASH_SHA384) &&
		(Signature.SaltSize == 32u),
		"X.509 signature parsing allocated memory");
	printf("[PASS] x509_signature_oom\n");
	return 0;
}
