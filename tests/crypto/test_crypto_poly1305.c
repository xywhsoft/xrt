#include "../test.h"
#include "test_crypto_digest.h"



static const uint8 TestPoly1305Key[XRT_POLY1305_KEY_SIZE] = {
	0x85, 0xD6, 0xBE, 0x78, 0x57, 0x55, 0x6D, 0x33,
	0x7F, 0x44, 0x52, 0xFE, 0x42, 0xD5, 0x06, 0xA8,
	0x01, 0x03, 0x80, 0x8A, 0xFB, 0x0D, 0xB2, 0xFD,
	0x4A, 0xBF, 0xF6, 0xAF, 0x41, 0x49, 0xF5, 0x1B
};

static const char TestPoly1305Message[] = "Cryptographic Forum Research Group";



/* 核对 RFC 8439 2.5.2 的一次性 Poly1305 向量。 */
static void testPoly1305Vector(void)
{
	uint8 Tag[XRT_POLY1305_TAG_SIZE];

	testRequire(xrtPoly1305(
			TestPoly1305Key,
			TestPoly1305Message,
			sizeof(TestPoly1305Message) - 1u,
			Tag
		), "Poly1305 vector failed");
	testCryptoDigest(Tag, sizeof(Tag),
		"a8061dc1305136c6c22b8baf0c0127a9",
		"Poly1305 RFC vector mismatch");
}



/* 在每一个切分点以及逐字节输入下核对流式结果。 */
static void testPoly1305Streaming(void)
{
	size_t iSize = sizeof(TestPoly1305Message) - 1u;
	uint8 Expected[XRT_POLY1305_TAG_SIZE];

	testRequire(xrtPoly1305(
			TestPoly1305Key, TestPoly1305Message, iSize, Expected
		), "Poly1305 expected tag failed");
	for ( size_t iSplit = 0; iSplit <= iSize; iSplit++ ) {
		xpoly1305 State;
		uint8 Tag[XRT_POLY1305_TAG_SIZE];

		testRequire(xrtPoly1305Init(&State, TestPoly1305Key) &&
			xrtPoly1305Update(&State, TestPoly1305Message, iSplit) &&
			xrtPoly1305Update(
				&State,
				TestPoly1305Message + iSplit,
				iSize - iSplit
			) && xrtPoly1305Final(&State, Tag) &&
			xrtConstTimeEqual(Tag, Expected, sizeof(Tag)),
			"Poly1305 split result mismatch");
	}
	{
		xpoly1305 State;
		uint8 Tag[XRT_POLY1305_TAG_SIZE];

		testRequire(xrtPoly1305Init(&State, TestPoly1305Key),
			"Poly1305 byte stream init failed");
		for ( size_t i = 0; i < iSize; i++ ) {
			testRequire(xrtPoly1305Update(
					&State, TestPoly1305Message + i, 1
				), "Poly1305 byte stream update failed");
		}
		testRequire(xrtPoly1305Final(&State, Tag) &&
			xrtConstTimeEqual(Tag, Expected, sizeof(Tag)),
			"Poly1305 byte stream mismatch");
	}
}



/* 验证 Final 可重复且不会阻止后续追加。 */
static void testPoly1305Snapshot(void)
{
	xpoly1305 State;
	uint8 First[XRT_POLY1305_TAG_SIZE];
	uint8 Repeat[XRT_POLY1305_TAG_SIZE];
	uint8 Continued[XRT_POLY1305_TAG_SIZE];
	uint8 Expected[XRT_POLY1305_TAG_SIZE];
	char Message[sizeof(TestPoly1305Message)];
	size_t iSize = sizeof(TestPoly1305Message) - 1u;

	memcpy(Message, TestPoly1305Message, iSize);
	Message[iSize] = '!';
	testRequire(xrtPoly1305Init(&State, TestPoly1305Key) &&
		xrtPoly1305Update(&State, TestPoly1305Message, iSize) &&
		xrtPoly1305Final(&State, First) &&
		xrtPoly1305Final(&State, Repeat) &&
		xrtConstTimeEqual(First, Repeat, sizeof(First)),
		"Poly1305 repeated Final mismatch");
	testRequire(xrtPoly1305Update(&State, "!", 1) &&
		xrtPoly1305Final(&State, Continued) &&
		xrtPoly1305(TestPoly1305Key, Message, sizeof(Message), Expected) &&
		xrtConstTimeEqual(Continued, Expected, sizeof(Continued)),
		"Poly1305 continue-after-Final mismatch");
}



/* 检查空消息、损坏状态、空指针和状态内重叠。 */
static void testPoly1305Edges(void)
{
	xpoly1305 State;
	xpoly1305 Before;
	uint8 Tag[XRT_POLY1305_TAG_SIZE];

	testRequire(xrtPoly1305(TestPoly1305Key, NULL, 0, Tag),
		"Poly1305 empty message failed");
	testCryptoDigest(Tag, sizeof(Tag),
		"0103808afb0db2fd4abff6af4149f51b",
		"Poly1305 empty tag mismatch");
	testRequire(xrtPoly1305Init(&State, TestPoly1305Key),
		"Poly1305 edge init failed");
	Before = State;
	xrtClearError();
	testRequire(!xrtPoly1305Update(&State, &State, 1) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(&State, &Before, sizeof(State)) == 0),
		"Poly1305 state overlap changed state");
	xrtClearError();
	testRequire(!xrtPoly1305Final(&State, &State) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Poly1305 accepted tag over state");
	State.Guard = 0;
	xrtClearError();
	testRequire(!xrtPoly1305Update(&State, NULL, 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"Poly1305 accepted damaged state");
	xrtClearError();
	testRequire(!xrtPoly1305Init(NULL, TestPoly1305Key) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Poly1305 accepted null state");
	xrtClearError();
	testRequire(!xrtPoly1305(TestPoly1305Key, NULL, 1, Tag) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Poly1305 accepted null non-empty data");
}



/* 执行 Poly1305 向量、流式快照和参数边界测试。 */
int main(void)
{
	testPoly1305Vector();
	testPoly1305Streaming();
	testPoly1305Snapshot();
	testPoly1305Edges();
	return 0;
}
