#include "../test.h"
#include "test_crypto_nist_api.h"
#include "test_crypto_nist_keypair.h"



int main(void)
{
	testCryptoNistKeyPair(
		XRT_P384_PRIVATE_SIZE,
		XRT_P384_PUBLIC_SIZE,
		xrtP384KeyPair,
		xrtP384Public,
		xrtP384Shared,
		xrtP384Valid
	);
	printf("[PASS] crypto_p384_keypair\n");
	return 0;
}
