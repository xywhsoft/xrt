#include "../test.h"
#include "test_crypto_nist_api.h"



int main(void)
{
	testCryptoNistApi(
		XRT_P256_PRIVATE_SIZE,
		XRT_P256_PUBLIC_SIZE,
		"ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551",
		"046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"
		"4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5",
		"047cf27b188d034f7e8a52380304b51ac3c08969e277f21b35a60b48fc47669978"
		"07775510db8ed040293d9ac69f7430dbba7dade63ce982299e04b79d227873d1",
		"p256-public",
		"p256-multiply",
		"p256-shared",
		xrtP256Valid,
		xrtP256Multiply,
		xrtP256Add,
		xrtP256Public,
		xrtP256Shared
	);
	printf("[PASS] crypto_p256\n");
	return 0;
}
