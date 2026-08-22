#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xssh.h"

static int xssh_crypto_test_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static int xssh_crypto_expect_mem(const char* sName, const void* pA, const void* pB, size_t iLen)
{
	if ( memcmp(pA, pB, iLen) != 0 ) {
		return xssh_crypto_test_fail(sName, "memory mismatch");
	}
	return 0;
}

static int xssh_crypto_test_hmac_sha2_vectors(void)
{
	static const uint8 arrExpected256[XSSH_SHA256_SIZE] = {
		0xb0u, 0x34u, 0x4cu, 0x61u, 0xd8u, 0xdbu, 0x38u, 0x53u,
		0x5cu, 0xa8u, 0xafu, 0xceu, 0xafu, 0x0bu, 0xf1u, 0x2bu,
		0x88u, 0x1du, 0xc2u, 0x00u, 0xc9u, 0x83u, 0x3du, 0xa7u,
		0x26u, 0xe9u, 0x37u, 0x6cu, 0x2eu, 0x32u, 0xcfu, 0xf7u
	};
	static const uint8 arrExpected512[XSSH_SHA512_SIZE] = {
		0x87u, 0xaau, 0x7cu, 0xdeu, 0xa5u, 0xefu, 0x61u, 0x9du,
		0x4fu, 0xf0u, 0xb4u, 0x24u, 0x1au, 0x1du, 0x6cu, 0xb0u,
		0x23u, 0x79u, 0xf4u, 0xe2u, 0xceu, 0x4eu, 0xc2u, 0x78u,
		0x7au, 0xd0u, 0xb3u, 0x05u, 0x45u, 0xe1u, 0x7cu, 0xdeu,
		0xdau, 0xa8u, 0x33u, 0xb7u, 0xd6u, 0xb8u, 0xa7u, 0x02u,
		0x03u, 0x8bu, 0x27u, 0x4eu, 0xaeu, 0xa3u, 0xf4u, 0xe4u,
		0xbeu, 0x9du, 0x91u, 0x4eu, 0xebu, 0x61u, 0xf1u, 0x70u,
		0x2eu, 0x69u, 0x6cu, 0x20u, 0x3au, 0x12u, 0x68u, 0x54u
	};
	uint8 arrKey[20];
	uint8 arrOut256[XSSH_SHA256_SIZE];
	uint8 arrOut512[XSSH_SHA512_SIZE];
	const uint8* pMsg = (const uint8*)"Hi There";

	memset(arrKey, 0x0bu, sizeof(arrKey));
	if ( xsshCryptoHMACSHA256(arrKey, sizeof(arrKey), pMsg, strlen((const char*)pMsg), arrOut256) != XSSH_OK ||
		xsshCryptoHMACSHA512(arrKey, sizeof(arrKey), pMsg, strlen((const char*)pMsg), arrOut512) != XSSH_OK ) {
		return xssh_crypto_test_fail("hmac_sha2_vectors", "hmac failed");
	}
	if ( xssh_crypto_expect_mem("hmac_sha2_vectors", arrOut256, arrExpected256, sizeof(arrExpected256)) != 0 ||
		xssh_crypto_expect_mem("hmac_sha2_vectors", arrOut512, arrExpected512, sizeof(arrExpected512)) != 0 ) {
		return 1;
	}
	return 0;
}

static int xssh_crypto_test_aes_gcm_zero_vectors(void)
{
	static const uint8 arrAES128Tag[XSSH_AEAD_TAG_SIZE] = {
		0x58u, 0xe2u, 0xfcu, 0xceu, 0xfau, 0x7eu, 0x30u, 0x61u,
		0x36u, 0x7fu, 0x1du, 0x57u, 0xa4u, 0xe7u, 0x45u, 0x5au
	};
	static const uint8 arrAES256Tag[XSSH_AEAD_TAG_SIZE] = {
		0x53u, 0x0fu, 0x8au, 0xfbu, 0xc7u, 0x45u, 0x36u, 0xb9u,
		0xa9u, 0x63u, 0xb4u, 0xf1u, 0xc4u, 0xcbu, 0x73u, 0x8bu
	};
	uint8 arrKey128[XSSH_AES128_KEY_SIZE];
	uint8 arrKey256[XSSH_AES256_KEY_SIZE];
	uint8 arrNonce[XSSH_GCM_NONCE_SIZE];
	uint8 arrEmpty[1] = {0};
	uint8 arrOut[XSSH_AEAD_TAG_SIZE];
	uint8 arrPlain[1];

	memset(arrKey128, 0, sizeof(arrKey128));
	memset(arrKey256, 0, sizeof(arrKey256));
	memset(arrNonce, 0, sizeof(arrNonce));

	if ( xsshCryptoAES128GCMEncrypt(arrOut, sizeof(arrOut), arrKey128, arrNonce, sizeof(arrNonce), NULL, 0u, arrEmpty, 0u) != XSSH_OK ||
		xssh_crypto_expect_mem("aes128_gcm_zero", arrOut, arrAES128Tag, sizeof(arrAES128Tag)) != 0 ||
		xsshCryptoAES128GCMDecrypt(arrPlain, 0u, arrKey128, arrNonce, sizeof(arrNonce), NULL, 0u, arrOut, sizeof(arrOut)) != XSSH_OK ) {
		return xssh_crypto_test_fail("aes128_gcm_zero", "vector mismatch");
	}

	if ( xsshCryptoAES256GCMEncrypt(arrOut, sizeof(arrOut), arrKey256, arrNonce, sizeof(arrNonce), NULL, 0u, arrEmpty, 0u) != XSSH_OK ||
		xssh_crypto_expect_mem("aes256_gcm_zero", arrOut, arrAES256Tag, sizeof(arrAES256Tag)) != 0 ||
		xsshCryptoAES256GCMDecrypt(arrPlain, 0u, arrKey256, arrNonce, sizeof(arrNonce), NULL, 0u, arrOut, sizeof(arrOut)) != XSSH_OK ) {
		return xssh_crypto_test_fail("aes256_gcm_zero", "vector mismatch");
	}
	return 0;
}

static int xssh_crypto_test_aes_gcm_roundtrip_and_tamper(void)
{
	static const uint8 arrAAD[] = { 's', 's', 'h', '-', 'p', 'k', 't' };
	static const uint8 arrPlain[] = { 0x14u, 'x', 's', 's', 'h' };
	uint8 arrKey128[XSSH_AES128_KEY_SIZE];
	uint8 arrKey256[XSSH_AES256_KEY_SIZE];
	uint8 arrNonce[XSSH_GCM_NONCE_SIZE];
	uint8 arrCipher[sizeof(arrPlain) + XSSH_AEAD_TAG_SIZE];
	uint8 arrDec[sizeof(arrPlain)];
	size_t i;

	for ( i = 0u; i < sizeof(arrKey128); ++i ) arrKey128[i] = (uint8)i;
	for ( i = 0u; i < sizeof(arrKey256); ++i ) arrKey256[i] = (uint8)(0x80u + i);
	for ( i = 0u; i < sizeof(arrNonce); ++i ) arrNonce[i] = (uint8)(0xa0u + i);

	if ( xsshCryptoAES128GCMEncrypt(arrCipher, sizeof(arrCipher), arrKey128, arrNonce, sizeof(arrNonce), arrAAD, sizeof(arrAAD), arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshCryptoAES128GCMDecrypt(arrDec, sizeof(arrDec), arrKey128, arrNonce, sizeof(arrNonce), arrAAD, sizeof(arrAAD), arrCipher, sizeof(arrCipher)) != XSSH_OK ||
		xssh_crypto_expect_mem("aes128_gcm_roundtrip", arrDec, arrPlain, sizeof(arrPlain)) != 0 ) {
		return xssh_crypto_test_fail("aes128_gcm_roundtrip", "roundtrip failed");
	}
	arrCipher[sizeof(arrCipher) - 1u] ^= 0x01u;
	if ( xsshCryptoAES128GCMDecrypt(arrDec, sizeof(arrDec), arrKey128, arrNonce, sizeof(arrNonce), arrAAD, sizeof(arrAAD), arrCipher, sizeof(arrCipher)) != XSSH_ERR_BAD_PACKET ) {
		return xssh_crypto_test_fail("aes128_gcm_tamper", "tampered tag accepted");
	}

	if ( xsshCryptoAES256GCMEncrypt(arrCipher, sizeof(arrCipher), arrKey256, arrNonce, sizeof(arrNonce), arrAAD, sizeof(arrAAD), arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		xsshCryptoAES256GCMDecrypt(arrDec, sizeof(arrDec), arrKey256, arrNonce, sizeof(arrNonce), arrAAD, sizeof(arrAAD), arrCipher, sizeof(arrCipher)) != XSSH_OK ||
		xssh_crypto_expect_mem("aes256_gcm_roundtrip", arrDec, arrPlain, sizeof(arrPlain)) != 0 ) {
		return xssh_crypto_test_fail("aes256_gcm_roundtrip", "roundtrip failed");
	}
	arrCipher[sizeof(arrCipher) - 1u] ^= 0x01u;
	if ( xsshCryptoAES256GCMDecrypt(arrDec, sizeof(arrDec), arrKey256, arrNonce, sizeof(arrNonce), arrAAD, sizeof(arrAAD), arrCipher, sizeof(arrCipher)) != XSSH_ERR_BAD_PACKET ) {
		return xssh_crypto_test_fail("aes256_gcm_tamper", "tampered tag accepted");
	}
	return 0;
}

static int xssh_crypto_test_chacha20_poly1305_vector(void)
{
	static const uint8 arrKey[XSSH_CHACHA20_KEY_SIZE] = {
		0x80u, 0x81u, 0x82u, 0x83u, 0x84u, 0x85u, 0x86u, 0x87u,
		0x88u, 0x89u, 0x8au, 0x8bu, 0x8cu, 0x8du, 0x8eu, 0x8fu,
		0x90u, 0x91u, 0x92u, 0x93u, 0x94u, 0x95u, 0x96u, 0x97u,
		0x98u, 0x99u, 0x9au, 0x9bu, 0x9cu, 0x9du, 0x9eu, 0x9fu
	};
	static const uint8 arrNonce[XSSH_CHACHA20_NONCE_SIZE] = {
		0x07u, 0x00u, 0x00u, 0x00u, 0x40u, 0x41u, 0x42u, 0x43u,
		0x44u, 0x45u, 0x46u, 0x47u
	};
	static const uint8 arrAAD[] = {
		0x50u, 0x51u, 0x52u, 0x53u, 0xc0u, 0xc1u, 0xc2u, 0xc3u,
		0xc4u, 0xc5u, 0xc6u, 0xc7u
	};
	static const uint8 arrExpected[] = {
		0xd3u, 0x1au, 0x8du, 0x34u, 0x64u, 0x8eu, 0x60u, 0xdbu,
		0x7bu, 0x86u, 0xafu, 0xbcu, 0x53u, 0xefu, 0x7eu, 0xc2u,
		0xa4u, 0xadu, 0xedu, 0x51u, 0x29u, 0x6eu, 0x08u, 0xfeu,
		0xa9u, 0xe2u, 0xb5u, 0xa7u, 0x36u, 0xeeu, 0x62u, 0xd6u,
		0x3du, 0xbeu, 0xa4u, 0x5eu, 0x8cu, 0xa9u, 0x67u, 0x12u,
		0x82u, 0xfau, 0xfbu, 0x69u, 0xdau, 0x92u, 0x72u, 0x8bu,
		0x1au, 0x71u, 0xdeu, 0x0au, 0x9eu, 0x06u, 0x0bu, 0x29u,
		0x05u, 0xd6u, 0xa5u, 0xb6u, 0x7eu, 0xcdu, 0x3bu, 0x36u,
		0x92u, 0xddu, 0xbdu, 0x7fu, 0x2du, 0x77u, 0x8bu, 0x8cu,
		0x98u, 0x03u, 0xaeu, 0xe3u, 0x28u, 0x09u, 0x1bu, 0x58u,
		0xfau, 0xb3u, 0x24u, 0xe4u, 0xfau, 0xd6u, 0x75u, 0x94u,
		0x55u, 0x85u, 0x80u, 0x8bu, 0x48u, 0x31u, 0xd7u, 0xbcu,
		0x3fu, 0xf4u, 0xdeu, 0xf0u, 0x8eu, 0x4bu, 0x7au, 0x9du,
		0xe5u, 0x76u, 0xd2u, 0x65u, 0x86u, 0xceu, 0xc6u, 0x4bu,
		0x61u, 0x16u, 0x1au, 0xe1u, 0x0bu, 0x59u, 0x4fu, 0x09u,
		0xe2u, 0x6au, 0x7eu, 0x90u, 0x2eu, 0xcbu, 0xd0u, 0x60u,
		0x06u, 0x91u
	};
	const uint8* pPlain = (const uint8*)"Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
	size_t iPlainLen = strlen((const char*)pPlain);
	uint8 arrCipher[sizeof(arrExpected)];
	uint8 arrDec[sizeof(arrExpected)];

	/* 该向量验证的是标准 RFC 8439 AEAD；OpenSSH 专用 chacha 封包测试后续单独补。 */
	if ( iPlainLen + XSSH_AEAD_TAG_SIZE != sizeof(arrExpected) ) {
		return xssh_crypto_test_fail("chacha20_poly1305_vector", "fixture length mismatch");
	}
	if ( xsshCryptoChaCha20Poly1305Encrypt(arrCipher, sizeof(arrCipher), arrKey, arrNonce, arrAAD, sizeof(arrAAD), pPlain, iPlainLen) != XSSH_OK ||
		xssh_crypto_expect_mem("chacha20_poly1305_vector", arrCipher, arrExpected, sizeof(arrExpected)) != 0 ||
		xsshCryptoChaCha20Poly1305Decrypt(arrDec, iPlainLen, arrKey, arrNonce, arrAAD, sizeof(arrAAD), arrCipher, sizeof(arrCipher)) != XSSH_OK ||
		xssh_crypto_expect_mem("chacha20_poly1305_vector", arrDec, pPlain, iPlainLen) != 0 ) {
		return xssh_crypto_test_fail("chacha20_poly1305_vector", "vector mismatch");
	}
	arrCipher[sizeof(arrCipher) - 1u] ^= 0x01u;
	if ( xsshCryptoChaCha20Poly1305Decrypt(arrDec, iPlainLen, arrKey, arrNonce, arrAAD, sizeof(arrAAD), arrCipher, sizeof(arrCipher)) != XSSH_ERR_BAD_PACKET ) {
		return xssh_crypto_test_fail("chacha20_poly1305_tamper", "tampered tag accepted");
	}
	return 0;
}

int main(void)
{
	if ( xssh_crypto_test_hmac_sha2_vectors() != 0 ) return 1;
	if ( xssh_crypto_test_aes_gcm_zero_vectors() != 0 ) return 1;
	if ( xssh_crypto_test_aes_gcm_roundtrip_and_tamper() != 0 ) return 1;
	if ( xssh_crypto_test_chacha20_poly1305_vector() != 0 ) return 1;

	printf("xssh_crypto_test: PASS\n");
	return 0;
}
