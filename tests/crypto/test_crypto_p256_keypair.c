#include "../test.h"
#include "test_crypto_nist_api.h"
#include "test_crypto_nist_keypair.h"



int main(void)
{
	testCryptoNistKeyPair(
		XRT_P256_PRIVATE_SIZE,
		XRT_P256_PUBLIC_SIZE,
		xrtP256KeyPair,
		xrtP256Public,
		xrtP256Shared,
		xrtP256Valid
	);
	printf("[PASS] crypto_p256_keypair\n");
	return 0;
}
