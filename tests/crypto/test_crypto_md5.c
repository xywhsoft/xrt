#include "../test.h"
#include "test_crypto_digest.h"



typedef struct test_md5_vector {
	cstr Data;
	cstr Digest;
} testmd5vector;



/* 验证 RFC 1321 附录给出的全部标准测试向量。 */
static void testMd5Vectors(void)
{
	static const testmd5vector Vectors[] = {
		{ "", "d41d8cd98f00b204e9800998ecf8427e" },
		{ "a", "0cc175b9c0f1b6a831c399e269772661" },
		{ "abc", "900150983cd24fb0d6963f7d28e17f72" },
		{ "message digest", "f96b697d7cb7938d525a2f31aaf161d0" },
		{ "abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b" },
		{
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
			"d174ab98d277d9f5a5611c2c9f419d9f"
		},
		{
			"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
			"57edf4a22be3c955ac49da2e2107b67a"
		}
	};
	uint8 Digest[XRT_MD5_SIZE];

	for ( size_t i = 0; i < (sizeof(Vectors) / sizeof(Vectors[0])); i++ ) {
		size_t iSize = strlen(Vectors[i].Data);

		testRequire(xrtMd5(Vectors[i].Data, iSize, Digest),
			"MD5 vector calculation failed");
		testCryptoDigest(Digest, sizeof(Digest), Vectors[i].Digest,
			"MD5 vector mismatch");
	}
}



/* 验证每一个块边界分割点都与一次性计算完全一致。 */
static void testMd5Streaming(void)
{
	uint8 Data[257];
	uint8 Expected[XRT_MD5_SIZE];
	uint8 Digest[XRT_MD5_SIZE];
	xmd5 State;

	for ( size_t i = 0; i < sizeof(Data); i++ ) {
		Data[i] = (uint8)i;
	}
	testRequire(xrtMd5(Data, sizeof(Data), Expected),
		"MD5 streaming reference failed");
	for ( size_t i = 0; i <= sizeof(Data); i++ ) {
		xrtMd5Init(&State);
		testRequire(
			xrtMd5Update(&State, Data, i) &&
			xrtMd5Update(&State, Data + i, sizeof(Data) - i) &&
			xrtMd5Final(&State, Digest) &&
			xrtConstTimeEqual(Digest, Expected, sizeof(Digest)),
			"MD5 split update mismatch"
		);
	}
}



/* 验证 Final 只读取快照，可重复调用且不会阻断后续追加。 */
static void testMd5Snapshot(void)
{
	xmd5 State;
	uint8 First[XRT_MD5_SIZE];
	uint8 Second[XRT_MD5_SIZE];
	uint8 Expected[XRT_MD5_SIZE];

	xrtMd5Init(&State);
	testRequire(
		xrtMd5Update(&State, "hello", 5u) &&
		xrtMd5Final(&State, First) &&
		xrtMd5Final(&State, Second) &&
		xrtConstTimeEqual(First, Second, sizeof(First)),
		"MD5 repeatable final failed"
	);
	testRequire(
		xrtMd5Update(&State, " world", 6u) &&
		xrtMd5Final(&State, Second) &&
		xrtMd5("hello world", 11u, Expected) &&
		xrtConstTimeEqual(Second, Expected, sizeof(Second)),
		"MD5 continuation after final failed"
	);
}



/* 参数、状态和长度失败必须在修改状态之前被拒绝。 */
static void testMd5Invalid(void)
{
	xmd5 State;
	xmd5 Before;
	uint8 Digest[XRT_MD5_SIZE];

	xrtClearError();
	xrtMd5Init(NULL);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"MD5 null init error mismatch"
	);
	xrtMd5Init(&State);
	Before = State;
	xrtClearError();
	testRequire(
		!xrtMd5Update(&State, NULL, 1u) &&
		(memcmp(&State, &Before, sizeof(State)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"MD5 null data was not atomic"
	);
	xrtClearError();
	testRequire(
		!xrtMd5Final(&State, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"MD5 null output error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtMd5("x", 1u, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"MD5 one-shot null output error mismatch"
	);

	State.Size = UINT64_MAX >> 3u;
	State.BufferSize = (uint32)(State.Size & 63u);
	Before = State;
	xrtClearError();
	testRequire(
		!xrtMd5Update(&State, "x", 1u) &&
		(memcmp(&State, &Before, sizeof(State)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"MD5 overflow was not atomic"
	);

	xrtMd5Init(&State);
	State.BufferSize = 1u;
	xrtClearError();
	testRequire(
		!xrtMd5Final(&State, Digest) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"MD5 inconsistent tail was accepted"
	);
	memset(&State, 0, sizeof(State));
	xrtClearError();
	testRequire(
		!xrtMd5Final(&State, Digest) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"MD5 uninitialized state was accepted"
	);
	testRequire(xrtCryptoHashSize(XCRYPTO_HASH_MD5) == XRT_MD5_SIZE,
		"MD5 generic digest size mismatch");
}



/* 执行 MD5 标准向量、边界、快照和失败原子性测试。 */
int main(void)
{
	testMd5Vectors();
	testMd5Streaming();
	testMd5Snapshot();
	testMd5Invalid();
	return 0;
}
