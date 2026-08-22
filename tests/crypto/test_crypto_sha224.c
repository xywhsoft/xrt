#include "../test.h"
#include "test_crypto_digest.h"



/* 验证 SHA-224 标准向量及单块、双块 padding 边界。 */
static void testSha224Vectors(void)
{
	static const struct {
		size_t Size;
		cstr Digest;
	} Vectors[] = {
		{ 0, "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f" },
		{ 55, "fb0bd626a70c28541dfa781bb5cc4d7d7f56622a58f01a0b1ddd646f" },
		{ 56, "d40854fc9caf172067136f2e29e1380b14626bf6f0dd06779f820dcd" },
		{ 63, "1d4e051f4d6fed2a63fd2421e65834cec00d64456553de3496ae8b1d" },
		{ 64, "a88cd5cde6d6fe9136a4e58b49167461ea95d388ca2bdb7afdc3cbf4" },
		{ 65, "ff8716f600af42959d0efb52e1f21b01bb328733009344d511c299fb" }
	};
	uint8 Data[65];
	uint8 Digest[XRT_SHA224_SIZE];

	memset(Data, 'a', sizeof(Data));
	for ( size_t i = 0; i < sizeof(Vectors) / sizeof(Vectors[0]); i++ ) {
		testRequire(xrtSha224(
			Vectors[i].Size != 0 ? Data : NULL, Vectors[i].Size, Digest
		), "SHA-224 boundary vector calculation failed");
		testCryptoDigest(
			Digest, sizeof(Digest), Vectors[i].Digest,
			"SHA-224 boundary vector mismatch"
		);
	}
	testRequire(xrtSha224("abc", 3, Digest),
		"SHA-224 abc calculation failed");
	testCryptoDigest(
		Digest, sizeof(Digest),
		"23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7",
		"SHA-224 abc vector mismatch"
	);
}



/* 验证所有分割点、快照 Final 和继续追加保持一致。 */
static void testSha224Streaming(void)
{
	xsha224 State;
	uint8 Data[129];
	uint8 Digest[XRT_SHA224_SIZE];
	uint8 Expected[XRT_SHA224_SIZE];

	for ( size_t i = 0; i < sizeof(Data); i++ ) {
		Data[i] = (uint8)i;
	}
	testRequire(xrtSha224(Data, sizeof(Data), Expected),
		"SHA-224 streaming reference failed");
	for ( size_t i = 0; i <= sizeof(Data); i++ ) {
		xrtSha224Init(&State);
		testRequire(xrtSha224Update(&State, Data, i) &&
			xrtSha224Update(&State, Data + i, sizeof(Data) - i) &&
			xrtSha224Final(&State, Digest) &&
			xrtConstTimeEqual(Digest, Expected, sizeof(Digest)),
			"SHA-224 split update mismatch");
	}
	xrtSha224Init(&State);
	testRequire(xrtSha224Update(&State, "hello", 5) &&
		xrtSha224Final(&State, Digest) &&
		xrtSha224Update(&State, " world", 6) &&
		xrtSha224Final(&State, Digest) &&
		xrtSha224("hello world", 11, Expected) &&
		xrtConstTimeEqual(Digest, Expected, sizeof(Digest)),
		"SHA-224 snapshot continuation failed");
}



/* 验证百万 a 长流向量和 SHA-224/SHA-256 状态不可混用。 */
static void testSha224LongAndInvalid(void)
{
	xsha224 State;
	xsha224 Before;
	uint8 Block[1000];
	uint8 Digest[XRT_SHA224_SIZE];

	memset(Block, 'a', sizeof(Block));
	xrtSha224Init(&State);
	for ( size_t i = 0; i < 1000u; i++ ) {
		testRequire(xrtSha224Update(&State, Block, sizeof(Block)),
			"SHA-224 million-a update failed");
	}
	testRequire(xrtSha224Final(&State, Digest),
		"SHA-224 million-a final failed");
	testCryptoDigest(
		Digest, sizeof(Digest),
		"20794655980c91d8bbb4c1ea97618a4bf03f42581948b2ee4ee7ad67",
		"SHA-224 million-a vector mismatch"
	);

	xrtSha224Init(&State);
	Before = State;
	testRequire(!xrtSha256Update((xsha256*)&State, "x", 1) &&
		(memcmp(&State, &Before, sizeof(State)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"SHA-256 accepted a SHA-224 state");
	xrtSha256Init((xsha256*)&State);
	Before = State;
	testRequire(!xrtSha224Update(&State, "x", 1) &&
		(memcmp(&State, &Before, sizeof(State)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"SHA-224 accepted a SHA-256 state");
}



int main(void)
{
	testSha224Vectors();
	testSha224Streaming();
	testSha224LongAndInvalid();
	printf("[PASS] crypto_sha224\n");
	return 0;
}
