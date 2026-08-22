#include "../test.h"
#include "test_crypto_digest.h"



/* 验证 SHA-256 标准向量和跨越单块、双块 padding 的边界。 */
static void testSha256Vectors(void)
{
	static const struct {
		size_t Size;
		cstr Digest;
	} arrVector[] = {
		{0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
		{55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
		{56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
		{63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
		{64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
		{65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"}
	};
	uint8 arrData[65];
	uint8 arrDigest[XRT_SHA256_SIZE];

	memset(arrData, 'a', sizeof(arrData));
	for ( size_t i = 0; i < (sizeof(arrVector) / sizeof(arrVector[0])); i++ ) {
		const void* pData = (arrVector[i].Size == 0) ? NULL : arrData;

		testRequire(xrtSha256(pData, arrVector[i].Size, arrDigest),
			"SHA-256 vector calculation failed");
		testCryptoDigest(arrDigest, sizeof(arrDigest),
			arrVector[i].Digest, "SHA-256 boundary vector mismatch");
	}
	testRequire(xrtSha256("abc", 3, arrDigest),
		"SHA-256 abc calculation failed");
	testCryptoDigest(arrDigest, sizeof(arrDigest),
		"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
		"SHA-256 abc vector mismatch");
}



/* 每个输入分割点和逐字节输入都必须与一次计算得到相同摘要。 */
static void testSha256StreamingBoundaries(void)
{
	xsha256 State;
	uint8 arrData[129];
	uint8 arrDigest[XRT_SHA256_SIZE];
	uint8 arrExpected[XRT_SHA256_SIZE];

	for ( size_t i = 0; i < sizeof(arrData); i++ ) {
		arrData[i] = (uint8)i;
	}
	testRequire(xrtSha256(arrData, sizeof(arrData), arrExpected),
		"SHA-256 streaming reference failed");
	for ( size_t i = 0; i <= sizeof(arrData); i++ ) {
		xrtSha256Init(&State);
		testRequire(xrtSha256Update(&State, arrData, i) &&
			xrtSha256Update(&State, arrData + i, sizeof(arrData) - i) &&
			xrtSha256Final(&State, arrDigest),
			"SHA-256 split update failed");
		testRequire(xrtConstTimeEqual(arrDigest, arrExpected, sizeof(arrDigest)),
			"SHA-256 split update mismatch");
	}
	xrtSha256Init(&State);
	for ( size_t i = 0; i < sizeof(arrData); i++ ) {
		testRequire(xrtSha256Update(&State, arrData + i, 1),
			"SHA-256 byte update failed");
	}
	testRequire(xrtSha256Final(&State, arrDigest) &&
		xrtConstTimeEqual(arrDigest, arrExpected, sizeof(arrDigest)),
		"SHA-256 byte stream mismatch");
}



/* 一百万个 a 的标准向量验证长流和重复整块路径。 */
static void testSha256MillionA(void)
{
	xsha256 State;
	uint8 arrBlock[1000];
	uint8 arrDigest[XRT_SHA256_SIZE];

	memset(arrBlock, 'a', sizeof(arrBlock));
	xrtSha256Init(&State);
	for ( int i = 0; i < 1000; i++ ) {
		testRequire(xrtSha256Update(&State, arrBlock, sizeof(arrBlock)),
			"SHA-256 million-a update failed");
	}
	testRequire(xrtSha256Final(&State, arrDigest),
		"SHA-256 million-a final failed");
	testCryptoDigest(arrDigest, sizeof(arrDigest),
		"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
		"SHA-256 million-a vector mismatch");
}



/* Final 必须可重复且不消耗状态，随后追加数据仍得到正确摘要。 */
static void testSha256Snapshot(void)
{
	xsha256 State;
	uint8 arrFirst[XRT_SHA256_SIZE];
	uint8 arrSecond[XRT_SHA256_SIZE];
	uint8 arrExpected[XRT_SHA256_SIZE];

	xrtSha256Init(&State);
	testRequire(xrtSha256Update(&State, "hello", 5) &&
		xrtSha256Final(&State, arrFirst) &&
		xrtSha256Final(&State, arrSecond),
		"SHA-256 repeatable final failed");
	testRequire(xrtConstTimeEqual(arrFirst, arrSecond, sizeof(arrFirst)),
		"SHA-256 repeated final changed the digest");
	testRequire(xrtSha256Update(&State, " world", 6) &&
		xrtSha256Final(&State, arrSecond) &&
		xrtSha256("hello world", 11, arrExpected),
		"SHA-256 continuation after final failed");
	testRequire(xrtConstTimeEqual(arrSecond, arrExpected, sizeof(arrSecond)),
		"SHA-256 continuation after final mismatch");
}



/* 参数、状态和长度失败必须在修改状态前被拒绝。 */
static void testSha256Invalid(void)
{
	xsha256 State;
	xsha256 Before;
	uint8 arrDigest[XRT_SHA256_SIZE];

	xrtClearError();
	xrtSha256Init(NULL);
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-256 null init reported the wrong error");
	xrtClearError();
	testRequire(!xrtSha256Update(NULL, NULL, 0) &&
		 (xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-256 null state reported the wrong error");

	xrtSha256Init(&State);
	Before = State;
	xrtClearError();
	testRequire(!xrtSha256Final(&State, NULL) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0),
		"SHA-256 null digest modified state");
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-256 null digest reported the wrong error");
	xrtClearError();
	testRequire(!xrtSha256("x", 1, NULL) &&
		 (xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-256 one-shot null digest reported the wrong error");

	Before = State;
	xrtClearError();
	testRequire(!xrtSha256Update(&State, NULL, 1) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0),
		"SHA-256 null update modified state");
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-256 null update reported the wrong error");

	State.Size = UINT64_MAX >> 3u;
	State.BufferSize = (uint32)(State.Size & 63u);
	Before = State;
	xrtClearError();
	testRequire(!xrtSha256Update(&State, "x", 1) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0),
		"SHA-256 overflow modified state");
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"SHA-256 overflow reported the wrong error");

	xrtSha256Init(&State);
	State.BufferSize = 1;
	xrtClearError();
	testRequire(!xrtSha256Final(&State, arrDigest) &&
		 (xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_STATE),
		"SHA-256 inconsistent tail state was not rejected");

	memset(&State, 0, sizeof(State));
	xrtClearError();
	testRequire(!xrtSha256Final(&State, arrDigest),
		"SHA-256 accepted an uninitialized state");
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_STATE),
		"SHA-256 damaged state reported the wrong error");
}



/* 执行 SHA-256 向量、长流、快照和失败原子性测试。 */
int main(void)
{
	testSha256Vectors();
	testSha256StreamingBoundaries();
	testSha256MillionA();
	testSha256Snapshot();
	testSha256Invalid();
	return 0;
}
