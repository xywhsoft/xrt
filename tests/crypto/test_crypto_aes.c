#include "../test.h"
#include "test_crypto_digest.h"

#if defined(XRT_TEST_REQUIRE_ARM_CRYPTO)
	#include "../../src/internal/xrt_crypto.h"
#endif



/* 核对 FIPS 197 的 AES-128、AES-192 与 AES-256 单块向量。 */
static void testAesVectors(void)
{
	static const uint8 Plain[XRT_AES_BLOCK_SIZE] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
	};
	static const uint8 Key128[XRT_AES128_KEY_SIZE] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
	};
	static const uint8 Key192[XRT_AES192_KEY_SIZE] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
	};
	static const uint8 Key256[XRT_AES256_KEY_SIZE] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
	};
	static const struct {
		const uint8* Key;
		size_t KeySize;
		const char* Cipher;
	} Cases[] = {
		{ Key128, sizeof(Key128), "69c4e0d86a7b0430d8cdb78070b4c55a" },
		{ Key192, sizeof(Key192), "dda97ca4864cdfe06eaf70a0ec0d7191" },
		{ Key256, sizeof(Key256), "8ea2b7ca516745bfeafc49904b496089" }
	};

	for ( size_t i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		xaes State;
		uint8 Cipher[XRT_AES_BLOCK_SIZE];
		uint8 Output[XRT_AES_BLOCK_SIZE];

		testRequire(xrtAesInit(&State, Cases[i].Key, Cases[i].KeySize),
			"AES vector initialization failed");
		#if defined(XRT_TEST_REQUIRE_ARM_CRYPTO)
			testRequire(
				(State.Backend & XRT_INTERNAL_AES_BACKEND_ARM_AES) != 0,
				"AES did not select the ARMv8 hardware backend"
			);
		#endif
		testRequire(xrtAesEncrypt(&State, Plain, Cipher),
			"AES vector encryption failed");
		testCryptoDigest(Cipher, sizeof(Cipher), Cases[i].Cipher,
			"AES FIPS ciphertext mismatch");
		testRequire(xrtAesDecrypt(&State, Cipher, Output) &&
			xrtConstTimeEqual(Output, Plain, sizeof(Output)),
			"AES FIPS decryption failed");
		xrtAesClear(&State);
	}
}



/* 验证块原位路径以及无效初始化不会破坏旧状态。 */
static void testAesState(void)
{
	uint8 Key[XRT_AES256_KEY_SIZE] = { 0 };
	uint8 Block[XRT_AES_BLOCK_SIZE];
	uint8 Plain[XRT_AES_BLOCK_SIZE];
	xaes State;
	xaes Before;

	for ( size_t i = 0; i < sizeof(Block); i++ ) {
		Block[i] = (uint8)i;
	}
	memcpy(Plain, Block, sizeof(Plain));
	testRequire(xrtAesInit(&State, Key, sizeof(Key)),
		"AES state initialization failed");
	testRequire(xrtAesEncrypt(&State, Block, Block) &&
		xrtAesDecrypt(&State, Block, Block) &&
		xrtConstTimeEqual(Block, Plain, sizeof(Block)),
		"AES in-place round trip failed");
	Before = State;
	xrtClearError();
	testRequire(!xrtAesInit(&State, Key, 20u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		xrtConstTimeEqual(&State, &Before, sizeof(State)),
		"AES invalid key length changed the old state");
	xrtAesClear(&State);
	for ( size_t i = 0; i < sizeof(State); i++ ) {
		testRequire(((const uint8*)&State)[i] == 0,
			"AES clear left key material");
	}
}



/* 检查无效状态、部分重叠和状态区重叠均保持输出不变。 */
static void testAesEdges(void)
{
	uint8 Key[XRT_AES128_KEY_SIZE] = { 0 };
	uint8 Buffer[32];
	uint8 Before[sizeof(Buffer)];
	xaes State;
	xaes Invalid;

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Before));
	memset(&Invalid, 0, sizeof(Invalid));
	xrtClearError();
	testRequire(!xrtAesEncrypt(&Invalid, Buffer, Buffer + 16u) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Buffer)),
		"AES invalid state changed output");
	testRequire(xrtAesInit(&State, Key, sizeof(Key)),
		"AES edge initialization failed");
	xrtClearError();
	testRequire(!xrtAesEncrypt(&State, Buffer, Buffer + 1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Buffer)),
		"AES accepted partial block overlap");
	xrtClearError();
	testRequire(!xrtAesEncrypt(
			&State, Buffer, State.RoundKey
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"AES accepted output over its key state");
	xrtAesClear(&State);
}



/* 执行 AES 三种密钥长度、状态和失败原子性测试。 */
int main(void)
{
	testAesVectors();
	testAesState();
	testAesEdges();
	return 0;
}
