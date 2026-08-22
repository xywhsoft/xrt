#include "../test.h"
#include "test_crypto_digest.h"



/* 验证 SHA-1 标准向量和跨越单块、双块 padding 的边界。 */
static void testSha1Vectors(void)
{
	static const struct {
		size_t Size;
		cstr Digest;
	} arrVector[] = {
		{0, "da39a3ee5e6b4b0d3255bfef95601890afd80709"},
		{55, "c1c8bbdc22796e28c0e15163d20899b65621d65a"},
		{56, "c2db330f6083854c99d4b5bfb6e8f29f201be699"},
		{63, "03f09f5b158a7a8cdad920bddc29b81c18a551f5"},
		{64, "0098ba824b5c16427bd7a1122a5a442a25ec644d"},
		{65, "11655326c708d70319be2610e8a57d9a5b959d3b"}
	};
	uint8 arrData[65];
	uint8 arrDigest[XRT_SHA1_SIZE];

	memset(arrData, 'a', sizeof(arrData));
	for ( size_t i = 0; i < (sizeof(arrVector) / sizeof(arrVector[0])); i++ ) {
		const void* pData = (arrVector[i].Size == 0) ? NULL : arrData;

		testRequire(xrtSha1(pData, arrVector[i].Size, arrDigest),
			"SHA-1 vector calculation failed");
		testCryptoDigest(arrDigest, sizeof(arrDigest),
			arrVector[i].Digest, "SHA-1 boundary vector mismatch");
	}
	testRequire(xrtSha1("abc", 3, arrDigest),
		"SHA-1 abc calculation failed");
	testCryptoDigest(arrDigest, sizeof(arrDigest),
		"a9993e364706816aba3e25717850c26c9cd0d89d",
		"SHA-1 abc vector mismatch");
}



/* 每个输入分割点和逐字节输入都必须与一次计算得到相同摘要。 */
static void testSha1StreamingBoundaries(void)
{
	xsha1 State;
	uint8 arrData[129];
	uint8 arrDigest[XRT_SHA1_SIZE];
	uint8 arrExpected[XRT_SHA1_SIZE];

	for ( size_t i = 0; i < sizeof(arrData); i++ ) {
		arrData[i] = (uint8)i;
	}
	testRequire(xrtSha1(arrData, sizeof(arrData), arrExpected),
		"SHA-1 streaming reference failed");
	for ( size_t i = 0; i <= sizeof(arrData); i++ ) {
		xrtSha1Init(&State);
		testRequire(xrtSha1Update(&State, arrData, i) &&
			xrtSha1Update(&State, arrData + i, sizeof(arrData) - i) &&
			xrtSha1Final(&State, arrDigest),
			"SHA-1 split update failed");
		testRequire(xrtConstTimeEqual(arrDigest, arrExpected, sizeof(arrDigest)),
			"SHA-1 split update mismatch");
	}
	xrtSha1Init(&State);
	for ( size_t i = 0; i < sizeof(arrData); i++ ) {
		testRequire(xrtSha1Update(&State, arrData + i, 1),
			"SHA-1 byte update failed");
	}
	testRequire(xrtSha1Final(&State, arrDigest) &&
		xrtConstTimeEqual(arrDigest, arrExpected, sizeof(arrDigest)),
		"SHA-1 byte stream mismatch");
}



/* 一百万个 a 的标准向量验证长流和重复整块路径。 */
static void testSha1MillionA(void)
{
	xsha1 State;
	uint8 arrBlock[1000];
	uint8 arrDigest[XRT_SHA1_SIZE];

	memset(arrBlock, 'a', sizeof(arrBlock));
	xrtSha1Init(&State);
	for ( int i = 0; i < 1000; i++ ) {
		testRequire(xrtSha1Update(&State, arrBlock, sizeof(arrBlock)),
			"SHA-1 million-a update failed");
	}
	testRequire(xrtSha1Final(&State, arrDigest),
		"SHA-1 million-a final failed");
	testCryptoDigest(arrDigest, sizeof(arrDigest),
		"34aa973cd4c4daa4f61eeb2bdbad27316534016f",
		"SHA-1 million-a vector mismatch");
}



/* Final 必须可重复且不消耗状态，随后追加数据仍得到正确摘要。 */
static void testSha1Snapshot(void)
{
	xsha1 State;
	uint8 arrFirst[XRT_SHA1_SIZE];
	uint8 arrSecond[XRT_SHA1_SIZE];
	uint8 arrExpected[XRT_SHA1_SIZE];

	xrtSha1Init(&State);
	testRequire(xrtSha1Update(&State, "hello", 5) &&
		xrtSha1Final(&State, arrFirst) &&
		xrtSha1Final(&State, arrSecond),
		"SHA-1 repeatable final failed");
	testRequire(xrtConstTimeEqual(arrFirst, arrSecond, sizeof(arrFirst)),
		"SHA-1 repeated final changed the digest");
	testRequire(xrtSha1Update(&State, " world", 6) &&
		xrtSha1Final(&State, arrSecond) &&
		xrtSha1("hello world", 11, arrExpected),
		"SHA-1 continuation after final failed");
	testRequire(xrtConstTimeEqual(arrSecond, arrExpected, sizeof(arrSecond)),
		"SHA-1 continuation after final mismatch");
}



/* 参数、状态和长度失败必须在修改状态前被拒绝。 */
static void testSha1Invalid(void)
{
	xsha1 State;
	xsha1 Before;
	uint8 arrDigest[XRT_SHA1_SIZE];

	xrtClearError();
	xrtSha1Init(NULL);
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-1 null init reported the wrong error");
	xrtClearError();
	testRequire(!xrtSha1Update(NULL, NULL, 0) &&
		 (xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-1 null state reported the wrong error");

	xrtSha1Init(&State);
	Before = State;
	xrtClearError();
	testRequire(!xrtSha1Final(&State, NULL) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0),
		"SHA-1 null digest modified state");
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-1 null digest reported the wrong error");
	xrtClearError();
	testRequire(!xrtSha1("x", 1, NULL) &&
		 (xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-1 one-shot null digest reported the wrong error");

	Before = State;
	xrtClearError();
	testRequire(!xrtSha1Update(&State, NULL, 1) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0),
		"SHA-1 null update modified state");
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-1 null update reported the wrong error");

	State.Size = UINT64_MAX >> 3u;
	State.BufferSize = (uint32)(State.Size & 63u);
	Before = State;
	xrtClearError();
	testRequire(!xrtSha1Update(&State, "x", 1) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0),
		"SHA-1 overflow modified state");
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"SHA-1 overflow reported the wrong error");

	xrtSha1Init(&State);
	State.BufferSize = 1;
	xrtClearError();
	testRequire(!xrtSha1Final(&State, arrDigest) &&
		 (xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_STATE),
		"SHA-1 inconsistent tail state was not rejected");

	memset(&State, 0, sizeof(State));
	xrtClearError();
	testRequire(!xrtSha1Final(&State, arrDigest),
		"SHA-1 accepted an uninitialized state");
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_STATE),
		"SHA-1 damaged state reported the wrong error");
}



/* 执行 SHA-1 向量、长流、快照和失败原子性测试。 */
int main(void)
{
	testSha1Vectors();
	testSha1StreamingBoundaries();
	testSha1MillionA();
	testSha1Snapshot();
	testSha1Invalid();
	return 0;
}
