/*
 * XRT Example - Key Exchange
 * XRT 范例 - 密钥交换
 *
 * Description / 说明:
 *   EN: Demonstrates X25519 and ECDH secp256r1 shared secret derivation.
 *   CN: 演示 X25519 与 ECDH secp256r1 的共享密钥推导。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/crypto_key_exchange.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/crypto_key_exchange -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


int main()
{
	uint8 arrPrivA[32];
	uint8 arrPubA[32];
	uint8 arrPrivB[32];
	uint8 arrPubB[32];
	uint8 arrSecretA[32];
	uint8 arrSecretB[32];
	uint8 arrEcdhPrivA[32];
	uint8 arrEcdhPubA[65];
	uint8 arrEcdhPrivB[32];
	uint8 arrEcdhPubB[65];
	uint8 arrEcdhSecretA[32];
	uint8 arrEcdhSecretB[32];

	xrtInit();

	xrtX25519Keypair(arrPrivA, arrPubA);
	xrtX25519Keypair(arrPrivB, arrPubB);
	xrtX25519SharedSecret(arrSecretA, arrPrivA, arrPubB);
	xrtX25519SharedSecret(arrSecretB, arrPrivB, arrPubA);

	xrtECDHSecp256r1Keypair(arrEcdhPrivA, arrEcdhPubA);
	xrtECDHSecp256r1Keypair(arrEcdhPrivB, arrEcdhPubB);
	xrtECDHSecp256r1SharedSecret(arrEcdhSecretA, arrEcdhPrivA, arrEcdhPubB);
	xrtECDHSecp256r1SharedSecret(arrEcdhSecretB, arrEcdhPrivB, arrEcdhPubA);

	printf("x25519_match = %s\n", exBoolText(exBytesEqual(arrSecretA, arrSecretB, sizeof(arrSecretA))));
	printf("ecdh_p256_match = %s\n", exBoolText(exBytesEqual(arrEcdhSecretA, arrEcdhSecretB, sizeof(arrEcdhSecretA))));

	xrtUnit();
	return 0;
}
