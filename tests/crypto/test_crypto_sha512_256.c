#include "../test.h"
#include "test_crypto_digest.h"



/* 验证 FIPS SHA-512/256 固定向量和分块等价性。 */
static void testSha512_256Vectors(void)
{
	uint8 Data[257];
	uint8 Digest[XRT_SHA512_256_SIZE];
	uint8 Expected[XRT_SHA512_256_SIZE];
	xsha512_256 State;

	testRequire(xrtSha512_256(NULL, 0, Digest),
		"SHA-512/256 empty vector failed");
	testCryptoDigest(
		Digest,
		sizeof(Digest),
		"c672b8d1ef56ed28ab87c3622c5114069bdd3ad7b8f9737498d0c01ecef0967a",
		"SHA-512/256 empty vector mismatch"
	);
	testRequire(xrtSha512_256("abc", 3u, Digest),
		"SHA-512/256 abc vector failed");
	testCryptoDigest(
		Digest,
		sizeof(Digest),
		"53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23",
		"SHA-512/256 abc vector mismatch"
	);
	for ( size_t i = 0; i < sizeof(Data); i++ ) {
		Data[i] = (uint8)i;
	}
	testRequire(xrtSha512_256(Data, sizeof(Data), Expected),
		"SHA-512/256 streaming reference failed");
	for ( size_t i = 0; i <= sizeof(Data); i++ ) {
		xrtSha512_256Init(&State);
		testRequire(
			xrtSha512_256Update(&State, Data, i) &&
			xrtSha512_256Update(
				&State, Data + i, sizeof(Data) - i
			) && xrtSha512_256Final(&State, Digest) &&
			xrtConstTimeEqual(
				Digest, Expected, sizeof(Digest)
			),
			"SHA-512/256 split update mismatch"
		);
	}
}



/* 验证算法状态隔离、空参数和失败原子性。 */
static void testSha512_256Invalid(void)
{
	xsha512_256 State;
	xsha512_256 Before;

	xrtClearError();
	xrtSha512_256Init(NULL);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-512/256 null init error mismatch"
	);
	xrtSha512Init(&State);
	Before = State;
	xrtClearError();
	testRequire(
		!xrtSha512_256Update(&State, NULL, 0) &&
		(memcmp(&State, &Before, sizeof(State)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"SHA-512/256 accepted SHA-512 state"
	);
	xrtSha512_256Init(&State);
	Before = State;
	xrtClearError();
	testRequire(
		!xrtSha512_256Update(&State, NULL, 1u) &&
		(memcmp(&State, &Before, sizeof(State)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-512/256 null data was not atomic"
	);
	xrtClearError();
	testRequire(
		!xrtSha512_256Final(&State, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-512/256 null output error mismatch"
	);
	testRequire(
		xrtCryptoHashSize(XCRYPTO_HASH_SHA512_256) ==
			XRT_SHA512_256_SIZE,
		"SHA-512/256 generic digest size mismatch"
	);
}



/* 执行 SHA-512/256 固定向量、流式和失败边界测试。 */
int main(void)
{
	testSha512_256Vectors();
	testSha512_256Invalid();
	return 0;
}
