/*
 * XRT Example - X25519 / HKDF / ChaCha20-Poly1305 / Ed25519
 * XRT 范例 - X25519 / HKDF / ChaCha20-Poly1305 / Ed25519
 *
 * Description / 说明:
 *   EN: Demonstrates shared-secret agreement, HKDF key derivation,
 *       AEAD round-trip, and Ed25519 sign/verify.
 *   CN: 演示共享秘密协商、HKDF 派生会话密钥、AEAD 加解密往返，
 *       以及 Ed25519 签名与验证。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/crypto_x25519_chacha20poly1305.exe -lws2_32 -lshell32 -liphlpapi
 *   gcc main.c -o ../../bin/crypto_x25519_chacha20poly1305 -lws2_32 -lshell32 -liphlpapi
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include <stdio.h>
#include <string.h>


static void PrintHex(const uint8* pData, size_t iLen)
{
	size_t i;

	for ( i = 0; i < iLen; i++ ) {
		printf("%02x", pData[i]);
	}
}


static bool BufferEqual(const uint8* pLeft, const uint8* pRight, size_t iLen)
{
	return memcmp(pLeft, pRight, iLen) == 0;
}


int main(void)
{
	const char* sPlainText = "{\"kind\":\"ping\",\"seq\":7}";
	const uint8 aucAAD[] = "demo:aead:v1";
	const uint8 aucInfo[] = "xrt/session/chacha20poly1305";
	uint8 aucPrivA[32];
	uint8 aucPubA[32];
	uint8 aucPrivB[32];
	uint8 aucPubB[32];
	uint8 aucSecretA[32];
	uint8 aucSecretB[32];
	uint8 aucPrk[32];
	uint8 aucSessionKey[32];
	uint8 aucNonce[12];
	uint8 aucCipher[256] = {0};
	uint8 aucPlain[256] = {0};
	uint8 aucSeed[32];
	uint8 aucSignPub[32];
	uint8 aucSignature[64];
	size_t iPlainLen = strlen(sPlainText);
	bool bSharedSame = false;
	bool bEnc = false;
	bool bDec = false;
	bool bPlainSame = false;
	bool bSign = false;
	bool bVerify = false;

	xrtInit();

	xrtX25519Keypair(aucPrivA, aucPubA);
	xrtX25519Keypair(aucPrivB, aucPubB);

	xrtX25519SharedSecret(aucSecretA, aucPrivA, aucPubB);
	xrtX25519SharedSecret(aucSecretB, aucPrivB, aucPubA);
	bSharedSame = BufferEqual(aucSecretA, aucSecretB, sizeof(aucSecretA));

	xrtHKDFExtract(aucPrk, NULL, 0, aucSecretA, sizeof(aucSecretA));
	xrtHKDFExpand(aucSessionKey, sizeof(aucSessionKey), aucPrk, sizeof(aucPrk), aucInfo, strlen((const char*)aucInfo));

	xrtRandomBytes(aucNonce, sizeof(aucNonce));
	bEnc = xrtChaCha20Poly1305Encrypt(aucCipher, aucSessionKey, aucNonce, aucAAD, strlen((const char*)aucAAD), (const uint8*)sPlainText, iPlainLen);
	bDec = xrtChaCha20Poly1305Decrypt(aucPlain, aucSessionKey, aucNonce, aucAAD, strlen((const char*)aucAAD), aucCipher, iPlainLen + 16u);
	aucPlain[iPlainLen] = '\0';
	bPlainSame = BufferEqual((const uint8*)sPlainText, aucPlain, iPlainLen);

	xrtEd25519Keypair(aucSeed, aucSignPub);
	bSign = xrtEd25519Sign(aucSignature, (const uint8*)sPlainText, iPlainLen, aucSeed);
	bVerify = xrtEd25519Verify((const uint8*)sPlainText, iPlainLen, aucSignature, aucSignPub);

	printf("shared_secret_equal = %s\n", bSharedSame ? "true" : "false");
	printf("session_key = ");
	PrintHex(aucSessionKey, sizeof(aucSessionKey));
	printf("\n");

	printf("nonce = ");
	PrintHex(aucNonce, sizeof(aucNonce));
	printf("\n");

	printf("encrypt_ok = %s\n", bEnc ? "true" : "false");
	printf("decrypt_ok = %s\n", bDec ? "true" : "false");
	printf("plain_round_trip = %s\n", bPlainSame ? "true" : "false");
	printf("plain_text = %s\n", bDec ? (char*)aucPlain : "(decrypt failed)");

	printf("sign_ok = %s\n", bSign ? "true" : "false");
	printf("verify_ok = %s\n", bVerify ? "true" : "false");

	xrtUnit();
	return (bSharedSame && bEnc && bDec && bPlainSame && bSign && bVerify) ? 0 : 1;
}
