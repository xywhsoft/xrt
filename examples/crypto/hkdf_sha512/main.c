#include <stdio.h>

#include <xrt.h>



/* 分开执行 SHA-384 HKDF 的 Extract 与 Expand。 */
int main(void)
{
	uint8 arrPrk[XRT_SHA384_SIZE];
	uint8 arrKey[32];

	if ( !xrtHkdfSha384Extract("salt", 4, "ikm", 3, arrPrk) ||
		 !xrtHkdfSha384Expand(
			arrPrk, sizeof(arrPrk), "context", 7, arrKey, sizeof(arrKey)
		 ) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrKey); i++ ) {
		printf("%02x", (unsigned int)arrKey[i]);
	}
	printf("\n");
	xrtSecureZero(arrPrk, sizeof(arrPrk));
	return 0;
}
