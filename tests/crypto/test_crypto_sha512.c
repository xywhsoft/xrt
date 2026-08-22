#include "../test.h"
#include "test_crypto_digest.h"



typedef void (*test_sha512_init_fn)(xsha512* pState);
typedef bool (*test_sha512_update_fn)(xsha512* pState, const void* pData, size_t iSize);
typedef bool (*test_sha512_final_fn)(const xsha512* pState, void* pDigest);
typedef bool (*test_sha512_fn)(const void* pData, size_t iSize, void* pDigest);



/* 对一组共享输入长度验证 SHA-384 或 SHA-512 标准向量。 */
static void testSha512FamilyVectors(
	test_sha512_fn pHash,
	size_t iDigestSize,
	cstr sAbc,
	const cstr* arrExpected
)
{
	static const size_t arrSize[] = {0, 111, 112, 127, 128, 129};
	uint8 arrData[129];
	uint8 arrDigest[XRT_SHA512_SIZE];

	memset(arrData, 'a', sizeof(arrData));
	for ( size_t i = 0; i < (sizeof(arrSize) / sizeof(arrSize[0])); i++ ) {
		const void* pData = (arrSize[i] == 0) ? NULL : arrData;

		testRequire(pHash(pData, arrSize[i], arrDigest),
			"SHA-384/512 vector calculation failed");
		testCryptoDigest(arrDigest, iDigestSize, arrExpected[i],
			"SHA-384/512 boundary vector mismatch");
	}
	testRequire(pHash("abc", 3, arrDigest),
		"SHA-384/512 abc calculation failed");
	testCryptoDigest(arrDigest, iDigestSize, sAbc,
		"SHA-384/512 abc vector mismatch");
}



/* 验证 SHA-384 和 SHA-512 的标准向量及 padding 边界。 */
static void testSha512Vectors(void)
{
	static const cstr arrSha384[] = {
		"38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b",
		"3c37955051cb5c3026f94d551d5b5e2ac38d572ae4e07172085fed81f8466b8f90dc23a8ffcdea0b8d8e58e8fdacc80a",
		"187d4e07cb306103c69967bf544d0dfbe9042577599c73c330abc0cb64c61236d5ed565ee19119d8c31779a38f791fcd",
		"9bd06b1763c2cf7aef40e795dc65bc96d59c41b537f3ad72ebdefd485476b5717c1aeb37c327fe9c1831b12b9efd08ae",
		"edb12730a366098b3b2beac75a3bef1b0969b15c48e2163c23d96994f8d1bef760c7e27f3c464d3829f56c0d53808b0b",
		"39b6f5a7b0e781dbc419f72e49b30eaac10f2c98c4403bc610da31067fd1b48f324138c8615d2b496d08d73d5e865326"
	};
	static const cstr arrSha512[] = {
		"cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",
		"fa9121c7b32b9e01733d034cfc78cbf67f926c7ed83e82200ef86818196921760b4beff48404df811b953828274461673c68d04e297b0eb7b2b4d60fc6b566a2",
		"c01d080efd492776a1c43bd23dd99d0a2e626d481e16782e75d54c2503b5dc32bd05f0f1ba33e568b88fd2d970929b719ecbb152f58f130a407c8830604b70ca",
		"828613968b501dc00a97e08c73b118aa8876c26b8aac93df128502ab360f91bab50a51e088769a5c1eff4782ace147dce3642554199876374291f5d921629502",
		"b73d1929aa615934e61a871596b3f3b33359f42b8175602e89f7e06e5f658a243667807ed300314b95cacdd579f3e33abdfbe351909519a846d465c59582f321",
		"4f681e0bd53cda4b5a2041cc8a06f2eabde44fb16c951fbd5b87702f07aeab611565b19c47fde30587177ebb852e3971bbd8d3fd30da18d71037dfbd98420429"
	};

	testSha512FamilyVectors(
		xrtSha384, XRT_SHA384_SIZE,
		"cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7",
		arrSha384
	);
	testSha512FamilyVectors(
		xrtSha512, XRT_SHA512_SIZE,
		"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
		arrSha512
	);
}



/* 每个输入分割点和逐字节输入都必须与一次计算一致。 */
static void testSha512FamilyStreaming(
	test_sha512_init_fn pInit,
	test_sha512_update_fn pUpdate,
	test_sha512_final_fn pFinal,
	test_sha512_fn pHash,
	size_t iDigestSize
)
{
	xsha512 State;
	uint8 arrData[257];
	uint8 arrDigest[XRT_SHA512_SIZE];
	uint8 arrExpected[XRT_SHA512_SIZE];

	for ( size_t i = 0; i < sizeof(arrData); i++ ) {
		arrData[i] = (uint8)i;
	}
	testRequire(pHash(arrData, sizeof(arrData), arrExpected),
		"SHA-384/512 streaming reference failed");
	for ( size_t i = 0; i <= sizeof(arrData); i++ ) {
		pInit(&State);
		testRequire(pUpdate(&State, arrData, i) &&
			pUpdate(&State, arrData + i, sizeof(arrData) - i) &&
			pFinal(&State, arrDigest),
			"SHA-384/512 split update failed");
		testRequire(xrtConstTimeEqual(arrDigest, arrExpected, iDigestSize),
			"SHA-384/512 split update mismatch");
	}
	pInit(&State);
	for ( size_t i = 0; i < sizeof(arrData); i++ ) {
		testRequire(pUpdate(&State, arrData + i, 1),
			"SHA-384/512 byte update failed");
	}
	testRequire(pFinal(&State, arrDigest) &&
		xrtConstTimeEqual(arrDigest, arrExpected, iDigestSize),
		"SHA-384/512 byte stream mismatch");
}



/* 验证两种算法全部输入分割位置。 */
static void testSha512StreamingBoundaries(void)
{
	testSha512FamilyStreaming(
		xrtSha384Init, xrtSha384Update, xrtSha384Final,
		xrtSha384, XRT_SHA384_SIZE
	);
	testSha512FamilyStreaming(
		xrtSha512Init, xrtSha512Update, xrtSha512Final,
		xrtSha512, XRT_SHA512_SIZE
	);
}



/* 一百万个 a 验证长流、128 字节整块和计数累加路径。 */
static void testSha512MillionA(void)
{
	uint8 arrBlock[1000];
	uint8 arrDigest[XRT_SHA512_SIZE];
	xsha384 Sha384;
	xsha512 Sha512;

	memset(arrBlock, 'a', sizeof(arrBlock));
	xrtSha384Init(&Sha384);
	xrtSha512Init(&Sha512);
	for ( int i = 0; i < 1000; i++ ) {
		testRequire(xrtSha384Update(&Sha384, arrBlock, sizeof(arrBlock)) &&
			xrtSha512Update(&Sha512, arrBlock, sizeof(arrBlock)),
			"SHA-384/512 million-a update failed");
	}
	testRequire(xrtSha384Final(&Sha384, arrDigest),
		"SHA-384 million-a final failed");
	testCryptoDigest(arrDigest, XRT_SHA384_SIZE,
		"9d0e1809716474cb086e834e310a4a1ced149e9c00f248527972cec5704c2a5b07b8b3dc38ecc4ebae97ddd87f3d8985",
		"SHA-384 million-a vector mismatch");
	testRequire(xrtSha512Final(&Sha512, arrDigest),
		"SHA-512 million-a final failed");
	testCryptoDigest(arrDigest, XRT_SHA512_SIZE,
		"e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973ebde0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b",
		"SHA-512 million-a vector mismatch");
}



/* Final 必须可重复且不消耗状态，随后追加数据仍得到正确摘要。 */
static void testSha512Snapshot(void)
{
	xsha512 State;
	uint8 arrFirst[XRT_SHA512_SIZE];
	uint8 arrSecond[XRT_SHA512_SIZE];
	uint8 arrExpected[XRT_SHA512_SIZE];

	xrtSha512Init(&State);
	testRequire(xrtSha512Update(&State, "hello", 5) &&
		xrtSha512Final(&State, arrFirst) &&
		xrtSha512Final(&State, arrSecond),
		"SHA-512 repeatable final failed");
	testRequire(xrtConstTimeEqual(arrFirst, arrSecond, sizeof(arrFirst)),
		"SHA-512 repeated final changed the digest");
	testRequire(xrtSha512Update(&State, " world", 6) &&
		xrtSha512Final(&State, arrSecond) &&
		xrtSha512("hello world", 11, arrExpected),
		"SHA-512 continuation after final failed");
	testRequire(xrtConstTimeEqual(arrSecond, arrExpected, sizeof(arrSecond)),
		"SHA-512 continuation after final mismatch");
}



/* 参数、算法标识、尾部状态和 128 位长度失败必须被准确拒绝。 */
static void testSha512Invalid(void)
{
	xsha512 State;
	xsha512 Before;
	uint8 arrDigest[XRT_SHA512_SIZE];

	xrtClearError();
	xrtSha384Init(NULL);
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-384 null init reported the wrong error");
	xrtClearError();
	xrtSha512Init(NULL);
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-512 null init reported the wrong error");

	xrtSha384Init(&State);
	Before = State;
	xrtClearError();
	testRequire(!xrtSha512Update(&State, NULL, 0) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0) &&
		 (xrtErrorKind(xrtGetError()) == XERR_STATE),
		"SHA-512 accepted SHA-384 state");
	xrtClearError();
	testRequire(!xrtSha384Final(&State, NULL) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-384 null digest contract failed");
	xrtClearError();
	testRequire(!xrtSha384(NULL, 0, NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-384 one-shot null digest contract failed");

	xrtSha512Init(&State);
	Before = State;
	xrtClearError();
	testRequire(!xrtSha512Update(&State, NULL, 1) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SHA-512 null update contract failed");

	State.SizeHigh = UINT64_MAX >> 3u;
	State.SizeLow = UINT64_MAX;
	State.BufferSize = 127;
	Before = State;
	xrtClearError();
	testRequire(!xrtSha512Update(&State, "x", 1) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0) &&
		 (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"SHA-512 length overflow contract failed");

	xrtSha512Init(&State);
	State.SizeLow = UINT64_MAX;
	State.BufferSize = 127;
	testRequire(xrtSha512Update(&State, "x", 1) &&
		 (State.SizeHigh == 1) && (State.SizeLow == 0) &&
		 (State.BufferSize == 0),
		"SHA-512 128-bit byte counter did not carry");

	xrtSha512Init(&State);
	State.BufferSize = 1;
	xrtClearError();
	testRequire(!xrtSha512Final(&State, arrDigest) &&
		 (xrtErrorKind(xrtGetError()) == XERR_STATE),
		"SHA-512 inconsistent tail state was not rejected");
}



/* 执行 SHA-384/512 向量、流式、长消息、快照和失败测试。 */
int main(void)
{
	testSha512Vectors();
	testSha512StreamingBoundaries();
	testSha512MillionA();
	testSha512Snapshot();
	testSha512Invalid();
	return 0;
}
