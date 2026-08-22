#ifndef XRT_TEST_CRYPTO_NIST_KEYPAIR_H
#define XRT_TEST_CRYPTO_NIST_KEYPAIR_H

#include "test_crypto_nist_api.h"



typedef bool (*test_nist_keypair_fn)(void* pPrivate, void* pPublic);



/* 验证随机密钥对、公钥派生、协商、重叠与空参数契约。 */
static inline void testCryptoNistKeyPair(
	size_t iPrivateSize,
	size_t iPublicSize,
	test_nist_keypair_fn pKeyPair,
	test_nist_public_fn pPublic,
	test_nist_binary_fn pShared,
	test_nist_valid_fn pValid
)
{
	uint8 PrivateA[48];
	uint8 PrivateB[48];
	uint8 PublicA[97];
	uint8 PublicB[97];
	uint8 Derived[97];
	uint8 SharedA[48];
	uint8 SharedB[48];
	uint8 Buffer[145];
	uint8 Before[145];

	testRequire(pKeyPair(PrivateA, PublicA), "NIST first key pair failed");
	testRequire(pKeyPair(PrivateB, PublicB), "NIST second key pair failed");
	testRequire(pValid(PublicA) && pValid(PublicB),
		"NIST generated an invalid public point");
	testRequire(pPublic(PrivateA, Derived) &&
		xrtConstTimeEqual(Derived, PublicA, iPublicSize),
		"NIST generated public key does not match its private key");
	testRequire(pShared(PrivateA, PublicB, SharedA) &&
		pShared(PrivateB, PublicA, SharedB) &&
		xrtConstTimeEqual(SharedA, SharedB, iPrivateSize),
		"NIST generated key agreement mismatch");

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Before));
	xrtClearError();
	testRequire(!pKeyPair(Buffer, Buffer + (iPrivateSize / 2u)) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Buffer)) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"NIST overlapping key-pair outputs changed data or error contract");
	testRequire(!pKeyPair(NULL, PublicA),
		"NIST key pair accepted a null private output");
	testRequire(!pKeyPair(PrivateA, NULL),
		"NIST key pair accepted a null public output");
}

#endif
