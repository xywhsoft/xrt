#include "../test.h"
#include "../fixtures/x509_signature_vectors.h"



/* 检查一次变异 AlgorithmIdentifier 的终止性和输出原子性。 */
static void testX509SignatureMutationOne(
	const uint8* pDer,
	size_t iSize,
	size_t* pAlgorithms,
	size_t* pSignatureErrors
)
{
	xx509algorithm Algorithm;
	xx509algorithm BeforeAlgorithm;
	xx509signature Signature;
	xx509signature BeforeSignature;
	xx509result Result;

	memset(&Algorithm, 0xA5, sizeof(Algorithm));
	BeforeAlgorithm = Algorithm;
	if ( !xrtX509AlgorithmParse(
		(xbytesview) { pDer, iSize }, &Algorithm
	) ) {
		testRequire(memcmp(
			&Algorithm, &BeforeAlgorithm, sizeof(Algorithm)
		) == 0, "mutated signature algorithm changed failed output");
		return;
	}
	(*pAlgorithms)++;
	memset(&Signature, 0xA5, sizeof(Signature));
	BeforeSignature = Signature;
	Result = xrtX509SignatureParse(&Algorithm, &Signature);
	if ( Result != X509_VALUE ) {
		testRequire(memcmp(
			&Signature, &BeforeSignature, sizeof(Signature)
		) == 0, "mutated signature parameters changed terminal output");
		if ( Result == X509_ERROR ) {
			(*pSignatureErrors)++;
		}
	}
}



/* 对完整 PSS 参数执行单比特和确定性多字节变异。 */
int main(void)
{
	uint8 Mutated[sizeof(X509_SIGNATURE_PSS_CUSTOM)];
	uint32 iState = UINT32_C(0xA511E9B3);
	size_t iAlgorithms = 0;
	size_t iSignatureErrors = 0;
	size_t iCases = 0;

	for ( size_t i = 0; i < sizeof(Mutated); i++ ) {
		for ( uint8 iBit = 0; iBit < 8u; iBit++ ) {
			memcpy(Mutated, X509_SIGNATURE_PSS_CUSTOM, sizeof(Mutated));
			Mutated[i] ^= (uint8)(UINT8_C(1) << iBit);
			testX509SignatureMutationOne(
				Mutated, sizeof(Mutated), &iAlgorithms, &iSignatureErrors
			);
			iCases++;
		}
	}
	for ( size_t i = 0; i < 4096u; i++ ) {
		memcpy(Mutated, X509_SIGNATURE_PSS_CUSTOM, sizeof(Mutated));
		for ( size_t j = 0; j < 3u; j++ ) {
			iState = (iState * UINT32_C(1664525)) + UINT32_C(1013904223);
			Mutated[iState % sizeof(Mutated)] ^=
				(uint8)((iState >> 24u) | 1u);
		}
		testX509SignatureMutationOne(
			Mutated, sizeof(Mutated), &iAlgorithms, &iSignatureErrors
		);
		iCases++;
	}
	testRequire((iAlgorithms != 0) && (iSignatureErrors != 0),
		"X.509 signature mutation corpus missed result classes");
	printf(
		"[PASS] x509_signature_mutation cases=%zu algorithms=%zu errors=%zu\n",
		iCases, iAlgorithms, iSignatureErrors
	);
	return 0;
}
