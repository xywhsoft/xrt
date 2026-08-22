#include "../test.h"
#include "test_crypto_nist_api.h"



int main(void)
{
	testCryptoNistApi(
		XRT_P384_PRIVATE_SIZE,
		XRT_P384_PUBLIC_SIZE,
		"ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf"
		"581a0db248b0a77aecec196accc52973",
		"04aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a38"
		"5502f25dbf55296c3a545e3872760ab73617de4a96262c6f5d9e98bf9292dc29f8"
		"f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f",
		"0408d999057ba3d2d969260045c55b97f089025959a6f434d651d207d19fb96e9e"
		"4fe0e86ebe0e64f85b96a9c75295df618e80f1fa5b1b3cedb7bfe8dffd6dba74"
		"b275d875bc6cc43e904e505f256ab4255ffd43e94d39e22d61501e700a940e80",
		"p384-public",
		"p384-multiply",
		"p384-shared",
		xrtP384Valid,
		xrtP384Multiply,
		xrtP384Add,
		xrtP384Public,
		xrtP384Shared
	);
	printf("[PASS] crypto_p384\n");
	return 0;
}
