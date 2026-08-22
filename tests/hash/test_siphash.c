#include "../test.h"



/* SipHash 论文参考实现对 0 到 63 字节消息给出的标准向量。 */
static const uint64 __arrVectors[64] = {
	UINT64_C(0x726FDB47DD0E0E31), UINT64_C(0x74F839C593DC67FD),
	UINT64_C(0x0D6C8009D9A94F5A), UINT64_C(0x85676696D7FB7E2D),
	UINT64_C(0xCF2794E0277187B7), UINT64_C(0x18765564CD99A68D),
	UINT64_C(0xCBC9466E58FEE3CE), UINT64_C(0xAB0200F58B01D137),
	UINT64_C(0x93F5F5799A932462), UINT64_C(0x9E0082DF0BA9E4B0),
	UINT64_C(0x7A5DBBC594DDB9F3), UINT64_C(0xF4B32F46226BADA7),
	UINT64_C(0x751E8FBC860EE5FB), UINT64_C(0x14EA5627C0843D90),
	UINT64_C(0xF723CA908E7AF2EE), UINT64_C(0xA129CA6149BE45E5),
	UINT64_C(0x3F2ACC7F57C29BDB), UINT64_C(0x699AE9F52CBE4794),
	UINT64_C(0x4BC1B3F0968DD39C), UINT64_C(0xBB6DC91DA77961BD),
	UINT64_C(0xBED65CF21AA2EE98), UINT64_C(0xD0F2CBB02E3B67C7),
	UINT64_C(0x93536795E3A33E88), UINT64_C(0xA80C038CCD5CCEC8),
	UINT64_C(0xB8AD50C6F649AF94), UINT64_C(0xBCE192DE8A85B8EA),
	UINT64_C(0x17D835B85BBB15F3), UINT64_C(0x2F2E6163076BCFAD),
	UINT64_C(0xDE4DAAACA71DC9A5), UINT64_C(0xA6A2506687956571),
	UINT64_C(0xAD87A3535C49EF28), UINT64_C(0x32D892FAD841C342),
	UINT64_C(0x7127512F72F27CCE), UINT64_C(0xA7F32346F95978E3),
	UINT64_C(0x12E0B01ABB051238), UINT64_C(0x15E034D40FA197AE),
	UINT64_C(0x314DFFBE0815A3B4), UINT64_C(0x027990F029623981),
	UINT64_C(0xCADCD4E59EF40C4D), UINT64_C(0x9ABFD8766A33735C),
	UINT64_C(0x0E3EA96B5304A7D0), UINT64_C(0xAD0C42D6FC585992),
	UINT64_C(0x187306C89BC215A9), UINT64_C(0xD4A60ABCF3792B95),
	UINT64_C(0xF935451DE4F21DF2), UINT64_C(0xA9538F0419755787),
	UINT64_C(0xDB9ACDDFF56CA510), UINT64_C(0xD06C98CD5C0975EB),
	UINT64_C(0xE612A3CB9ECBA951), UINT64_C(0xC766E62CFCADAF96),
	UINT64_C(0xEE64435A9752FE72), UINT64_C(0xA192D576B245165A),
	UINT64_C(0x0A8787BF8ECB74B2), UINT64_C(0x81B3E73D20B49B6F),
	UINT64_C(0x7FA8220BA3B2ECEA), UINT64_C(0x245731C13CA42499),
	UINT64_C(0xB78DBFAF3A8D83BD), UINT64_C(0xEA1AD565322A1A0B),
	UINT64_C(0x60E61C23A3795013), UINT64_C(0x6606D7E446282B93),
	UINT64_C(0x6CA4ECB15C5F91E1), UINT64_C(0x9F626DA15C9625F3),
	UINT64_C(0xE51B38608EF25F57), UINT64_C(0x958A324CEB064572)
};



/* 一次性和所有二分位置的流式结果都必须匹配标准向量。 */
static void testVectorsAndSplits(void)
{
	unsigned char arrMessage[64];
	xsipkey Key = xrtSipKey(UINT64_C(0x0706050403020100),
		UINT64_C(0x0F0E0D0C0B0A0908));

	for ( size_t i = 0; i < sizeof(arrMessage); i++ ) {
		arrMessage[i] = (unsigned char)i;
	}

	for ( size_t iSize = 0; iSize < sizeof(arrMessage); iSize++ ) {
		testRequire(xrtSipHash(arrMessage, iSize, Key) == __arrVectors[iSize],
			"SipHash reference vector changed");

		for ( size_t iSplit = 0; iSplit <= iSize; iSplit++ ) {
			xsiphash State;

			xrtSipHashInit(&State, Key);
			testRequire(xrtSipHashUpdate(&State, arrMessage, iSplit),
				"SipHash first stream chunk failed");
			testRequire(xrtSipHashUpdate(&State, arrMessage + iSplit,
				iSize - iSplit), "SipHash second stream chunk failed");
			testRequire(xrtSipHashFinal(&State) == __arrVectors[iSize],
				"SipHash stream split changed the result");
			testRequire(xrtSipHashFinal(&State) == __arrVectors[iSize],
				"SipHash final changed the state");
		}
	}
}



/* 逐字节追加后仍允许先终结观察，再继续追加。 */
static void testContinueAfterFinal(void)
{
	unsigned char arrMessage[64];
	xsipkey Key = xrtSipKey(UINT64_C(0x0706050403020100),
		UINT64_C(0x0F0E0D0C0B0A0908));
	xsiphash State;

	for ( size_t i = 0; i < sizeof(arrMessage); i++ ) {
		arrMessage[i] = (unsigned char)i;
	}
	xrtSipHashInit(&State, Key);
	for ( size_t i = 0; i < 17u; i++ ) {
		testRequire(xrtSipHashUpdate(&State, arrMessage + i, 1),
			"SipHash byte update failed");
	}
	testRequire(xrtSipHashFinal(&State) == __arrVectors[17],
		"SipHash byte stream result is wrong");
	testRequire(xrtSipHashUpdate(&State, arrMessage + 17, 14),
		"SipHash update after final failed");
	testRequire(xrtSipHashFinal(&State) == __arrVectors[31],
		"SipHash update after final changed semantics");
}



/* 参数和累计长度溢出必须在修改状态之前失败。 */
static void testErrors(void)
{
	xsiphash State;
	xsiphash Before;
	xsipkey Key = xrtSipKey(1, 2);
	unsigned char iByte = 1;

	memset(&State, 0, sizeof(State));
	xrtClearError();
	testRequire(!xrtSipHashUpdate(&State, NULL, 0),
		"SipHash accepted an uninitialized state");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"SipHash uninitialized state reported the wrong error");

	xrtSipHashInit(&State, Key);
	xrtClearError();
	testRequire(!xrtSipHashUpdate(&State, NULL, 1),
		"SipHash accepted missing input");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SipHash invalid input reported the wrong error");

	Before = State;
	xrtClearError();
	testRequire(!xrtSipHashUpdate(&State, &State, 1),
		"SipHash accepted input overlapping its state");
	testRequire(memcmp(&State, &Before, sizeof(State)) == 0,
		"SipHash overlap failure modified the state");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SipHash overlap reported the wrong error");

	State.Total = UINT64_MAX;
	State.TailSize = 7;
	Before = State;
	xrtClearError();
	testRequire(!xrtSipHashUpdate(&State, &iByte, 1),
		"SipHash accepted total length overflow");
	testRequire(memcmp(&State, &Before, sizeof(State)) == 0,
		"SipHash overflow modified the state");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"SipHash overflow reported the wrong error");
}



/* 执行标准向量、分块等价和错误原子性测试。 */
int main(void)
{
	testVectorsAndSplits();
	testContinueAfterFinal();
	testErrors();
	return 0;
}
