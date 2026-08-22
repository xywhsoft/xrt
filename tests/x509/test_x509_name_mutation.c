#include "../test.h"
#include "../fixtures/x509_name_vectors.h"



/* 逐字节扰动有效 Name，验证比较器只返回三态结果且不会越界。 */
int main(void)
{
	uint8 Mutated[sizeof(X509_NAME_UTF8_CAFE)];
	xbytesview Reference = {
		X509_NAME_UTF8_CAFE, sizeof(X509_NAME_UTF8_CAFE)
	};

	for ( size_t i = 0; i < sizeof(Mutated); i++ ) {
		xx509result Result;

		memcpy(Mutated, X509_NAME_UTF8_CAFE, sizeof(Mutated));
		Mutated[i] ^= UINT8_C(0x80);
		Result = xrtX509NameEqual(
			(xbytesview) { Mutated, sizeof(Mutated) }, Reference
		);
		testRequire((Result == X509_ERROR) || (Result == X509_DONE) ||
			(Result == X509_VALUE),
			"mutated X.509 Name returned an invalid tri-state result");
		if ( Result == X509_ERROR ) {
			testRequire(xrtGetError() != NULL,
				"mutated X.509 Name failed without structured error");
		}
	}
	printf("[PASS] x509_name_mutation\n");
	return 0;
}
