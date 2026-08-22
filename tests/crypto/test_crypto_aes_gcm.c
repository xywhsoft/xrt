#include "../test.h"
#include "test_crypto_digest.h"

#if defined(XRT_TEST_REQUIRE_ARM_CRYPTO)
	#include "../../src/internal/xrt_crypto.h"
#endif



/* 执行一个 NIST CAVP AES-GCM detached 向量。 */
static void testAesGcmCase(
	const uint8* pKey,
	size_t iKeySize,
	const uint8* pNonce,
	size_t iNonceSize,
	const uint8* pAad,
	size_t iAadSize,
	const uint8* pPlain,
	size_t iPlainSize,
	const char* pCipherHex,
	const char* pTagHex
)
{
	xaesgcm State;
	uint8 Cipher[32];
	uint8 Plain[32];
	uint8 Tag[XRT_AES_GCM_TAG_MAX_SIZE];

	testRequire(iPlainSize <= sizeof(Cipher), "AES-GCM test vector is too large");
	testRequire(xrtAesGcmInit(
			&State, pKey, iKeySize, XRT_AES_GCM_TAG_DEFAULT_SIZE
		), "AES-GCM CAVP initialization failed");
	#if defined(XRT_TEST_REQUIRE_ARM_CRYPTO)
		testRequire(
			(State.Cipher.Backend &
			 (XRT_INTERNAL_AES_BACKEND_ARM_AES |
			  XRT_INTERNAL_AES_BACKEND_ARM_PMULL)) ==
			 (XRT_INTERNAL_AES_BACKEND_ARM_AES |
			  XRT_INTERNAL_AES_BACKEND_ARM_PMULL),
			"AES-GCM did not select ARMv8 AES and PMULL"
		);
	#endif
	testRequire(xrtAesGcmEncrypt(
			&State, pNonce, iNonceSize, pAad, iAadSize,
			pPlain, iPlainSize, Cipher, Tag
		), "AES-GCM CAVP encryption failed");
	testCryptoDigest(Cipher, iPlainSize, pCipherHex,
		"AES-GCM CAVP ciphertext mismatch");
	testCryptoDigest(Tag, sizeof(Tag), pTagHex,
		"AES-GCM CAVP tag mismatch");
	testRequire(xrtAesGcmDecrypt(
			&State, pNonce, iNonceSize, pAad, iAadSize,
			Cipher, iPlainSize, Tag, Plain
		) && xrtConstTimeEqual(Plain, pPlain, iPlainSize),
		"AES-GCM CAVP decryption failed");
	xrtAesGcmClear(&State);
}



/* 核对 AES-128、AES-192 与 AES-256 的 NIST CAVP 完整向量。 */
static void testAesGcmVectors(void)
{
	static const uint8 Key128[] = {
		0xC9, 0x39, 0xCC, 0x13, 0x39, 0x7C, 0x1D, 0x37,
		0xDE, 0x6A, 0xE0, 0xE1, 0xCB, 0x7C, 0x42, 0x3C
	};
	static const uint8 Nonce128[] = {
		0xB3, 0xD8, 0xCC, 0x01, 0x7C, 0xBB,
		0x89, 0xB3, 0x9E, 0x0F, 0x67, 0xE2
	};
	static const uint8 Plain128[] = {
		0xC3, 0xB3, 0xC4, 0x1F, 0x11, 0x3A, 0x31, 0xB7,
		0x3D, 0x9A, 0x5C, 0xD4, 0x32, 0x10, 0x30, 0x69
	};
	static const uint8 Aad128[] = {
		0x24, 0x82, 0x56, 0x02, 0xBD, 0x12, 0xA9, 0x84,
		0xE0, 0x09, 0x2D, 0x3E, 0x44, 0x8E, 0xDA, 0x5F
	};
	static const uint8 Key192[] = {
		0x6F, 0x44, 0xF5, 0x2C, 0x2F, 0x62, 0xDA, 0xE4,
		0xE8, 0x68, 0x4B, 0xD2, 0xBC, 0x7D, 0x16, 0xEE,
		0x7C, 0x55, 0x73, 0x30, 0x30, 0x5A, 0x79, 0x0D
	};
	static const uint8 Nonce192[] = {
		0x9A, 0xE3, 0x58, 0x25, 0xD7, 0xC7,
		0xED, 0xC9, 0xA3, 0x9A, 0x07, 0x32
	};
	static const uint8 Plain192[] = {
		0x37, 0x22, 0x2D, 0x30, 0x89, 0x5E, 0xB9, 0x58,
		0x84, 0xBB, 0xBB, 0xAE, 0xE4, 0xD9, 0xCA, 0xE1
	};
	static const uint8 Aad192[] = {
		0x1B, 0x42, 0x36, 0xB8, 0x46, 0xFC, 0x2A, 0x0F,
		0x78, 0x28, 0x81, 0xBA, 0x48, 0xA0, 0x67, 0xE9
	};
	static const uint8 Key256[] = {
		0x92, 0xE1, 0x1D, 0xCD, 0xAA, 0x86, 0x6F, 0x5C,
		0xE7, 0x90, 0xFD, 0x24, 0x50, 0x1F, 0x92, 0x50,
		0x9A, 0xAC, 0xF4, 0xCB, 0x8B, 0x13, 0x39, 0xD5,
		0x0C, 0x9C, 0x12, 0x40, 0x93, 0x5D, 0xD0, 0x8B
	};
	static const uint8 Nonce256[] = {
		0xAC, 0x93, 0xA1, 0xA6, 0x14, 0x52,
		0x99, 0xBD, 0xE9, 0x02, 0xF2, 0x1A
	};
	static const uint8 Plain256[] = {
		0x2D, 0x71, 0xBC, 0xFA, 0x91, 0x4E, 0x4A, 0xC0,
		0x45, 0xB2, 0xAA, 0x60, 0x95, 0x5F, 0xAD, 0x24
	};
	static const uint8 Aad256[] = {
		0x1E, 0x08, 0x89, 0x01, 0x6F, 0x67, 0x60, 0x1C,
		0x8E, 0xBE, 0xA4, 0x94, 0x3B, 0xC2, 0x3A, 0xD6
	};

	testAesGcmCase(
		Key128, sizeof(Key128), Nonce128, sizeof(Nonce128),
		Aad128, sizeof(Aad128), Plain128, sizeof(Plain128),
		"93fe7d9e9bfd10348a5606e5cafa7354",
		"0032a1dc85f1c9786925a2e71d8272dd"
	);
	testAesGcmCase(
		Key192, sizeof(Key192), Nonce192, sizeof(Nonce192),
		Aad192, sizeof(Aad192), Plain192, sizeof(Plain192),
		"a54b5da33fc1196a8ef31a5321bfcaeb",
		"1c198086450ae1834dd6c2636796bce2"
	);
	testAesGcmCase(
		Key256, sizeof(Key256), Nonce256, sizeof(Nonce256),
		Aad256, sizeof(Aad256), Plain256, sizeof(Plain256),
		"8995ae2e6df3dbf96fac7b7137bae67f",
		"eca5aa77d51d4a0a14d9c51e1da474ab"
	);
}



/* 核对通用 IV 的 GHASH 路径，而不是只覆盖 96 位快路径。 */
static void testAesGcmGeneralNonce(void)
{
	static const uint8 Key[] = {
		0x83, 0xF9, 0xD9, 0x7D, 0x4A, 0xB7, 0x59, 0xFD,
		0xDC, 0xC3, 0xEF, 0x54, 0xA0, 0xE2, 0xA8, 0xEC
	};
	static const uint8 Nonce[] = { 0xCF };
	static const uint8 Plain[] = {
		0x77, 0xE6, 0x32, 0x9C, 0xF9, 0x42, 0x4F, 0x71,
		0xC8, 0x08, 0xDF, 0x91, 0x70, 0xBF, 0xD2, 0x98
	};
	static const uint8 Aad[] = {
		0x6D, 0xD4, 0x9E, 0xAE, 0xB4, 0x10, 0x3D, 0xAC,
		0x8F, 0x97, 0xE3, 0x23, 0x49, 0x46, 0xDD, 0x2D
	};

	testAesGcmCase(
		Key, sizeof(Key), Nonce, sizeof(Nonce), Aad, sizeof(Aad),
		Plain, sizeof(Plain),
		"50de86a7a92a8a5ea33db5696b96cd77",
		"aa181e84bc8b4bf5a68927c409d422cb"
	);
}



/* 验证七种标准标签长度都使用完整标签的左侧前缀。 */
static void testAesGcmTagSizes(void)
{
	static const size_t Sizes[] = { 4u, 8u, 12u, 13u, 14u, 15u, 16u };
	static const uint8 Key[] = {
		0xC9, 0x39, 0xCC, 0x13, 0x39, 0x7C, 0x1D, 0x37,
		0xDE, 0x6A, 0xE0, 0xE1, 0xCB, 0x7C, 0x42, 0x3C
	};
	static const uint8 Nonce[] = {
		0xB3, 0xD8, 0xCC, 0x01, 0x7C, 0xBB,
		0x89, 0xB3, 0x9E, 0x0F, 0x67, 0xE2
	};
	static const uint8 Plain[] = {
		0xC3, 0xB3, 0xC4, 0x1F, 0x11, 0x3A, 0x31, 0xB7,
		0x3D, 0x9A, 0x5C, 0xD4, 0x32, 0x10, 0x30, 0x69
	};
	static const uint8 Aad[] = {
		0x24, 0x82, 0x56, 0x02, 0xBD, 0x12, 0xA9, 0x84,
		0xE0, 0x09, 0x2D, 0x3E, 0x44, 0x8E, 0xDA, 0x5F
	};
	static const uint8 ExpectedTag[] = {
		0x00, 0x32, 0xA1, 0xDC, 0x85, 0xF1, 0xC9, 0x78,
		0x69, 0x25, 0xA2, 0xE7, 0x1D, 0x82, 0x72, 0xDD
	};
	uint8 Tag[XRT_AES_GCM_TAG_MAX_SIZE];

	for ( size_t i = 0; i < (sizeof(Sizes) / sizeof(Sizes[0])); i++ ) {
		xaesgcm State;
		uint8 Cipher[sizeof(Plain)];

		memset(Tag, 0xA5, sizeof(Tag));
		testRequire(xrtAesGcmInit(&State, Key, sizeof(Key), Sizes[i]) &&
			(xrtAesGcmTagSize(&State) == Sizes[i]) &&
			xrtAesGcmEncrypt(
				&State, Nonce, sizeof(Nonce), Aad, sizeof(Aad),
				Plain, sizeof(Plain), Cipher, Tag
			), "AES-GCM standard tag size failed");
		testRequire(xrtConstTimeEqual(Tag, ExpectedTag, Sizes[i]),
			"AES-GCM truncated tag prefix mismatch");
		for ( size_t j = Sizes[i]; j < sizeof(Tag); j++ ) {
			testRequire(Tag[j] == 0xA5,
				"AES-GCM wrote past the configured tag length");
		}
		xrtAesGcmClear(&State);
	}
}



/* 验证 packed 原位路径、空消息以及 GMAC 生成和验证。 */
static void testAesGcmConvenience(void)
{
	static const uint8 GmacKey[] = {
		0x77, 0xBE, 0x63, 0x70, 0x89, 0x71, 0xC4, 0xE2,
		0x40, 0xD1, 0xCB, 0x79, 0xE8, 0xD7, 0x7F, 0xEB
	};
	static const uint8 GmacNonce[] = {
		0xE0, 0xE0, 0x0F, 0x19, 0xFE, 0xD7,
		0xBA, 0x01, 0x36, 0xA7, 0x97, 0xF3
	};
	static const uint8 GmacData[] = {
		0x7A, 0x43, 0xEC, 0x1D, 0x9C, 0x0A, 0x5A, 0x78,
		0xA0, 0xB1, 0x65, 0x33, 0xA6, 0x21, 0x3C, 0xAB
	};
	uint8 Key[XRT_AES256_KEY_SIZE] = { 0 };
	uint8 Nonce[XRT_AES_GCM_NONCE_DEFAULT_SIZE] = { 0 };
	uint8 Buffer[5u + XRT_AES_GCM_TAG_DEFAULT_SIZE] = {
		'h', 'e', 'l', 'l', 'o'
	};
	uint8 Empty[XRT_AES_GCM_TAG_DEFAULT_SIZE];
	uint8 Tag[XRT_AES_GCM_TAG_DEFAULT_SIZE];
	xaesgcm State;

	testRequire(xrtAesGcmInit(
			&State, Key, sizeof(Key), XRT_AES_GCM_TAG_DEFAULT_SIZE
		), "AES-GCM packed initialization failed");
	testRequire(xrtAesGcmSeal(
			&State, Nonce, sizeof(Nonce), NULL, 0,
			Buffer, 5u, Buffer, sizeof(Buffer)
		) && xrtAesGcmOpen(
			&State, Nonce, sizeof(Nonce), NULL, 0,
			Buffer, sizeof(Buffer), Buffer, sizeof(Buffer)
		) && (memcmp(Buffer, "hello", 5u) == 0),
		"AES-GCM in-place packed round trip failed");
	testRequire(xrtAesGcmSeal(
			&State, Nonce, sizeof(Nonce), NULL, 0,
			NULL, 0, Empty, sizeof(Empty)
		) && xrtAesGcmOpen(
			&State, Nonce, sizeof(Nonce), NULL, 0,
			Empty, sizeof(Empty), NULL, 0
		), "AES-GCM empty packed message failed");
	xrtAesGcmClear(&State);
	testRequire(xrtAesGcmInit(
			&State, GmacKey, sizeof(GmacKey), sizeof(Tag)
		) && xrtAesGmac(
			&State, GmacNonce, sizeof(GmacNonce),
			GmacData, sizeof(GmacData), Tag
		), "AES-GMAC CAVP generation failed");
	testCryptoDigest(Tag, sizeof(Tag),
		"209fcc8d3675ed938e9c7166709dd946",
		"AES-GMAC CAVP tag mismatch");
	testRequire(xrtAesGmacVerify(
			&State, GmacNonce, sizeof(GmacNonce),
			GmacData, sizeof(GmacData), Tag
		), "AES-GMAC verification failed");
	Tag[0] ^= 1u;
	xrtClearError();
	testRequire(!xrtAesGmacVerify(
			&State, GmacNonce, sizeof(GmacNonce),
			GmacData, sizeof(GmacData), Tag
		) && (xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_AUTHENTICATION),
		"AES-GMAC accepted a changed tag");
	xrtAesGcmClear(&State);
}



/* 篡改密文、标签或 AAD 时必须返回结构化错误且不写明文。 */
static void testAesGcmAuthentication(void)
{
	uint8 Key[XRT_AES128_KEY_SIZE] = { 0 };
	uint8 Nonce[XRT_AES_GCM_NONCE_DEFAULT_SIZE] = { 0 };
	uint8 Aad[5] = { 1, 2, 3, 4, 5 };
	uint8 Cipher[23];
	uint8 Tag[XRT_AES_GCM_TAG_DEFAULT_SIZE];
	uint8 Plain[sizeof(Cipher)];
	uint8 Before[sizeof(Plain)];
	xaesgcm State;

	for ( size_t i = 0; i < sizeof(Cipher); i++ ) {
		Plain[i] = (uint8)i;
	}
	testRequire(xrtAesGcmInit(
			&State, Key, sizeof(Key), sizeof(Tag)
		) && xrtAesGcmEncrypt(
			&State, Nonce, sizeof(Nonce), Aad, sizeof(Aad),
			Plain, sizeof(Plain), Cipher, Tag
		), "AES-GCM tamper setup failed");
	memset(Plain, 0xA5, sizeof(Plain));
	memcpy(Before, Plain, sizeof(Before));
	Cipher[0] ^= 1u;
	xrtClearError();
	testRequire(!xrtAesGcmDecrypt(
			&State, Nonce, sizeof(Nonce), Aad, sizeof(Aad),
			Cipher, sizeof(Cipher), Tag, Plain
		) && (xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_AUTHENTICATION) &&
		xrtConstTimeEqual(Plain, Before, sizeof(Plain)),
		"AES-GCM ciphertext tamper changed output");
	Cipher[0] ^= 1u;
	Tag[15] ^= 1u;
	xrtClearError();
	testRequire(!xrtAesGcmDecrypt(
			&State, Nonce, sizeof(Nonce), Aad, sizeof(Aad),
			Cipher, sizeof(Cipher), Tag, Plain
		) && xrtConstTimeEqual(Plain, Before, sizeof(Plain)),
		"AES-GCM tag tamper changed output");
	Tag[15] ^= 1u;
	Aad[0] ^= 1u;
	xrtClearError();
	testRequire(!xrtAesGcmDecrypt(
			&State, Nonce, sizeof(Nonce), Aad, sizeof(Aad),
			Cipher, sizeof(Cipher), Tag, Plain
		) && xrtConstTimeEqual(Plain, Before, sizeof(Plain)),
		"AES-GCM AAD tamper changed output");
	xrtAesGcmClear(&State);
}



/* 检查标签集合、容量、部分重叠和标准长度上限。 */
static void testAesGcmEdges(void)
{
	static const size_t InvalidTagSizes[] = {
		0u, 1u, 2u, 3u, 5u, 6u, 7u, 9u, 10u, 11u, 17u
	};
	uint8 Key[XRT_AES128_KEY_SIZE] = { 0 };
	uint8 Nonce[XRT_AES_GCM_NONCE_DEFAULT_SIZE] = { 0 };
	uint8 Buffer[64];
	uint8 Tag[XRT_AES_GCM_TAG_DEFAULT_SIZE];
	uint8 One = 0;
	xaesgcm State;
	xaesgcm Before;

	testRequire(xrtAesGcmInit(
			&State, Key, sizeof(Key), sizeof(Tag)
		), "AES-GCM edge initialization failed");
	Before = State;
	for ( size_t i = 0;
		i < (sizeof(InvalidTagSizes) / sizeof(InvalidTagSizes[0])); i++ ) {
		xrtClearError();
		testRequire(!xrtAesGcmInit(
				&State, Key, sizeof(Key), InvalidTagSizes[i]
			) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
			xrtConstTimeEqual(&State, &Before, sizeof(State)),
			"AES-GCM accepted an invalid tag length");
	}
	memset(Buffer, 0xA5, sizeof(Buffer));
	xrtClearError();
	testRequire(!xrtAesGcmSeal(
			&State, Nonce, sizeof(Nonce), NULL, 0,
			"x", 1u, Buffer, sizeof(Tag)
		) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"AES-GCM accepted a short seal buffer");
	xrtClearError();
	testRequire(!xrtAesGcmOpen(
			&State, Nonce, sizeof(Nonce), NULL, 0,
			Buffer, sizeof(Tag) - 1u, NULL, 0
		) && (xrtErrorKind(xrtGetError()) == XERR_PROTOCOL),
		"AES-GCM accepted packed input without a full tag");
	xrtClearError();
	testRequire(!xrtAesGcmEncrypt(
			&State, Nonce, sizeof(Nonce), NULL, 0,
			Buffer, 16u, Buffer + 1u, Tag
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"AES-GCM accepted partial plaintext overlap");
	xrtClearError();
	testRequire(!xrtAesGcmEncrypt(
			&State, Nonce, sizeof(Nonce), Buffer, 8u,
			Buffer + 16u, 8u, Buffer, Tag
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"AES-GCM accepted output over AAD");
	xrtClearError();
	testRequire(!xrtAesGcmEncrypt(
			&State, Nonce, sizeof(Nonce), NULL, 0,
			Buffer, 16u, Buffer, Buffer + 8u
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"AES-GCM accepted tag over ciphertext");
	xrtClearError();
	testRequire(!xrtAesGcmEncrypt(
			&State, NULL, 0, NULL, 0, NULL, 0, NULL, Tag
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"AES-GCM accepted an empty nonce");
	#if SIZE_MAX > UINT32_MAX
		xrtClearError();
		testRequire(!xrtAesGcmEncrypt(
				&State, Nonce, sizeof(Nonce), NULL, 0,
				&One, (size_t)(XRT_AES_GCM_MAX_SIZE + 1u), &One, Tag
			) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
			"AES-GCM accepted plaintext past P_MAX");
		xrtClearError();
		testRequire(!xrtAesGcmEncrypt(
				&State, &One, (size_t)((UINT64_MAX / 8u) + 1u),
				NULL, 0, NULL, 0, NULL, Tag
			) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
			"AES-GCM accepted an IV bit length overflow");
	#else
		(void)One;
	#endif
	xrtAesGcmClear(&State);
	for ( size_t i = 0; i < sizeof(State); i++ ) {
		testRequire(((const uint8*)&State)[i] == 0,
			"AES-GCM clear left key material");
	}
}



/* 执行 AES-GCM 官方向量、通用路径、便利层和失败边界测试。 */
int main(void)
{
	testAesGcmVectors();
	testAesGcmGeneralNonce();
	testAesGcmTagSizes();
	testAesGcmConvenience();
	testAesGcmAuthentication();
	testAesGcmEdges();
	return 0;
}
