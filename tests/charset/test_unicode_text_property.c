#include "../test.h"



typedef struct test_scalar {
	unsigned char Bytes[4];
	size_t Size;
} test_scalar;



static const test_scalar arrScalar[] = {
	{ { 0x00u, 0, 0, 0 }, 1 },
	{ { 0x41u, 0, 0, 0 }, 1 },
	{ { 0x7Au, 0, 0, 0 }, 1 },
	{ { 0xC3u, 0xA9u, 0, 0 }, 2 },
	{ { 0xE4u, 0xBDu, 0xA0u, 0 }, 3 },
	{ { 0xF0u, 0x9Fu, 0x98u, 0x80u }, 4 }
};



/* 生成可复现的属性测试序列。 */
static uint32 testUnicodeNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 把标量编号序列编码为独立参考 UTF-8。 */
static size_t testUnicodeEncode(const uint8* pIndex, size_t iCount,
	unsigned char* pOutput)
{
	size_t iWrite = 0;

	for ( size_t i = 0; i < iCount; i++ ) {
		const test_scalar* pScalar = &arrScalar[pIndex[i]];

		memcpy(pOutput + iWrite, pScalar->Bytes, pScalar->Size);
		iWrite += pScalar->Size;
	}
	return iWrite;
}



/* 按标量编号集合计算过滤参考结果。 */
static size_t testUnicodeFilterReference(const uint8* pText, size_t iTextCount,
	const uint8* pSet, size_t iSetCount, unsigned char* pOutput)
{
	bool arrRemove[sizeof(arrScalar) / sizeof(arrScalar[0])] = { false };
	size_t iWrite = 0;

	for ( size_t i = 0; i < iSetCount; i++ ) {
		arrRemove[pSet[i]] = true;
	}
	for ( size_t i = 0; i < iTextCount; i++ ) {
		const test_scalar* pScalar = &arrScalar[pText[i]];

		if ( !arrRemove[pText[i]] ) {
			memcpy(pOutput + iWrite, pScalar->Bytes, pScalar->Size);
			iWrite += pScalar->Size;
		}
	}
	return iWrite;
}



/* 按相反的标量顺序计算反转参考结果。 */
static size_t testUnicodeReverseReference(const uint8* pText, size_t iTextCount,
	unsigned char* pOutput)
{
	size_t iWrite = 0;

	for ( size_t i = iTextCount; i != 0; i-- ) {
		const test_scalar* pScalar = &arrScalar[pText[i - 1u]];

		memcpy(pOutput + iWrite, pScalar->Bytes, pScalar->Size);
		iWrite += pScalar->Size;
	}
	return iWrite;
}



/* 独立规范化一个支持负数的标量位置。 */
static size_t testUnicodeNormalize(size_t iCount, int64 iIndex)
{
	if ( iIndex < 0 ) {
		uint64 iBack = (uint64)(-(iIndex + 1)) + 1u;

		return iBack >= (uint64)iCount ? 0 :
			iCount - (size_t)iBack;
	}
	return (uint64)iIndex > (uint64)iCount ?
		iCount : (size_t)iIndex;
}



/* 独立计算带负索引范围的标量边界。 */
static void testUnicodeRangeReference(size_t iSourceCount, int64 iStart,
	int64 iCount, size_t* pStart, size_t* pEnd)
{
	size_t iBegin = testUnicodeNormalize(iSourceCount, iStart);
	size_t iEnd;

	if ( iCount < 0 ) {
		iEnd = iSourceCount;
	} else if ( (uint64)iCount >=
		(uint64)(iSourceCount - iBegin) ) {
		iEnd = iSourceCount;
	} else {
		iEnd = iBegin + (size_t)iCount;
	}
	*pStart = iBegin;
	*pEnd = iEnd;
}



/* 独立查找一个标量编号序列。 */
static size_t testUnicodeFindReference(const uint8* pText, size_t iTextCount,
	const uint8* pPart, size_t iPartCount, size_t iStart)
{
	if ( iPartCount == 0 ) {
		return iStart;
	}
	if ( iPartCount > (iTextCount - iStart) ) {
		return XRT_NPOS;
	}
	for ( size_t i = iStart; i <= (iTextCount - iPartCount); i++ ) {
		if ( memcmp(pText + i, pPart, iPartCount) == 0 ) {
			return i;
		}
	}
	return XRT_NPOS;
}



/* 独立反向查找一个标量编号序列。 */
static size_t testUnicodeRFindReference(const uint8* pText, size_t iTextCount,
	const uint8* pPart, size_t iPartCount)
{
	if ( iPartCount == 0 ) {
		return iTextCount;
	}
	if ( iPartCount > iTextCount ) {
		return XRT_NPOS;
	}
	for ( size_t i = (iTextCount - iPartCount) + 1u; i != 0; i-- ) {
		size_t iPosition = i - 1u;

		if ( memcmp(pText + iPosition, pPart, iPartCount) == 0 ) {
			return iPosition;
		}
	}
	return XRT_NPOS;
}



/* 独立计算按标量集合裁剪后的编号范围。 */
static void testUnicodeTrimReference(const uint8* pText, size_t iTextCount,
	const uint8* pSet, size_t iSetCount, size_t* pStart, size_t* pEnd,
	size_t* pRightEnd)
{
	bool arrRemove[sizeof(arrScalar) / sizeof(arrScalar[0])] = { false };
	size_t iBegin = 0;
	size_t iRight = iTextCount;

	for ( size_t i = 0; i < iSetCount; i++ ) {
		arrRemove[pSet[i]] = true;
	}
	while ( (iBegin < iTextCount) && arrRemove[pText[iBegin]] ) {
		iBegin++;
	}
	while ( (iRight != 0) && arrRemove[pText[iRight - 1u]] ) {
		iRight--;
	}
	*pStart = iBegin;
	*pEnd = iRight < iBegin ? iBegin : iRight;
	*pRightEnd = iRight;
}



/* 随机验证严格 UTF-8 标量过滤和反转与独立参考序列一致。 */
int main(void)
{
	unsigned char arrActual[132];
	unsigned char arrExpected[128];
	unsigned char arrInPlace[132];
	unsigned char arrSetText[32];
	unsigned char arrSourceText[128];
	uint8 arrSet[8];
	uint8 arrSource[32];
	uint32 iState = UINT32_C(0x13579BDF);

	for ( size_t iCase = 0; iCase < 100000u; iCase++ ) {
		size_t iSourceCount = (size_t)(testUnicodeNext(&iState) % 33u);
		size_t iSetCount = (size_t)(testUnicodeNext(&iState) % 9u);
		size_t iSourceSize;
		size_t iSetSize;
		size_t iExpected;
		size_t iActual = SIZE_MAX;
		size_t iRangeStart;
		size_t iRangeEnd;
		size_t iSearchStart;
		size_t iFindExpected;
		size_t iTrimStart;
		size_t iTrimEnd;
		size_t iRightTrimEnd;
		int64 iSignedStart;
		int64 iSignedCount;
		xstrview Range;
		xstrview Trimmed;
		str sCopy;

		for ( size_t i = 0; i < iSourceCount; i++ ) {
			arrSource[i] = (uint8)(testUnicodeNext(&iState) %
				(sizeof(arrScalar) / sizeof(arrScalar[0])));
		}
		for ( size_t i = 0; i < iSetCount; i++ ) {
			arrSet[i] = (uint8)(testUnicodeNext(&iState) %
				(sizeof(arrScalar) / sizeof(arrScalar[0])));
		}
		iSourceSize = testUnicodeEncode(arrSource, iSourceCount, arrSourceText);
		iSetSize = testUnicodeEncode(arrSet, iSetCount, arrSetText);

		iSignedStart = (int64)(testUnicodeNext(&iState) % 81u) - 40;
		iSignedCount = (testUnicodeNext(&iState) & 3u) == 0 ?
			-1 : (int64)(testUnicodeNext(&iState) % 41u);
		testUnicodeRangeReference(iSourceCount, iSignedStart, iSignedCount,
			&iRangeStart, &iRangeEnd);
		iExpected = testUnicodeEncode(arrSource + iRangeStart,
			iRangeEnd - iRangeStart, arrExpected);
		testRequire(xrtUtf8Range(
			(xstrview){ (cstr)arrSourceText, iSourceSize },
			iSignedStart, iSignedCount, &Range) &&
			(Range.Size == iExpected) &&
			(memcmp(Range.Data, arrExpected, iExpected) == 0),
			"Unicode range property mismatch");
		if ( (iCase & 15u) == 0 ) {
			sCopy = xrtUtf8Substr(
				(xstrview){ (cstr)arrSourceText, iSourceSize },
				iSignedStart, iSignedCount);
			testRequire((sCopy != NULL) &&
				(memcmp(sCopy, arrExpected, iExpected) == 0) &&
				(sCopy[iExpected] == 0),
				"Unicode substring property mismatch");
			xrtFree(sCopy);
		}

		iSearchStart = iSourceCount == 0 ? 0 :
			(size_t)(testUnicodeNext(&iState) % (iSourceCount + 1u));
		iFindExpected = testUnicodeFindReference(arrSource, iSourceCount,
			arrSet, iSetCount, iSearchStart);
		testRequire(xrtUtf8Find(
			(xstrview){ (cstr)arrSourceText, iSourceSize },
			(xstrview){ (cstr)arrSetText, iSetSize },
			iSearchStart) == iFindExpected,
			"Unicode find property mismatch");
		iFindExpected = testUnicodeRFindReference(arrSource, iSourceCount,
			arrSet, iSetCount);
		testRequire(xrtUtf8RFind(
			(xstrview){ (cstr)arrSourceText, iSourceSize },
			(xstrview){ (cstr)arrSetText, iSetSize }) == iFindExpected,
			"Unicode reverse find property mismatch");

		testUnicodeTrimReference(arrSource, iSourceCount,
			arrSet, iSetCount, &iTrimStart, &iTrimEnd, &iRightTrimEnd);
		iExpected = testUnicodeEncode(arrSource + iTrimStart,
			iTrimEnd - iTrimStart, arrExpected);
		testRequire(xrtUtf8TrimSet(
			(xstrview){ (cstr)arrSourceText, iSourceSize },
			(xstrview){ (cstr)arrSetText, iSetSize }, &Trimmed) &&
			(Trimmed.Size == iExpected) &&
			(memcmp(Trimmed.Data, arrExpected, iExpected) == 0),
			"Unicode trim property mismatch");
		iExpected = testUnicodeEncode(arrSource + iTrimStart,
			iSourceCount - iTrimStart, arrExpected);
		testRequire(xrtUtf8TrimLeftSet(
			(xstrview){ (cstr)arrSourceText, iSourceSize },
			(xstrview){ (cstr)arrSetText, iSetSize }, &Trimmed) &&
			(Trimmed.Size == iExpected) &&
			(memcmp(Trimmed.Data, arrExpected, iExpected) == 0),
			"Unicode left trim property mismatch");
		iExpected = testUnicodeEncode(arrSource, iRightTrimEnd, arrExpected);
		testRequire(xrtUtf8TrimRightSet(
			(xstrview){ (cstr)arrSourceText, iSourceSize },
			(xstrview){ (cstr)arrSetText, iSetSize }, &Trimmed) &&
			(Trimmed.Size == iExpected) &&
			(memcmp(Trimmed.Data, arrExpected, iExpected) == 0),
			"Unicode right trim property mismatch");

		iExpected = testUnicodeFilterReference(arrSource, iSourceCount,
			arrSet, iSetCount, arrExpected);

		testRequire(xrtUtf8FilterTo(
			(xstrview){ (cstr)arrSourceText, iSourceSize },
			(xstrview){ (cstr)arrSetText, iSetSize },
			NULL, 0, &iActual) && (iActual == iExpected),
			"Unicode filter property query mismatch");
		testRequire(xrtUtf8FilterTo(
			(xstrview){ (cstr)arrSourceText, iSourceSize },
			(xstrview){ (cstr)arrSetText, iSetSize },
			(char*)arrActual, sizeof(arrActual), &iActual) &&
			(iActual == iExpected) &&
			(memcmp(arrActual, arrExpected, iExpected) == 0) &&
			(arrActual[iExpected] == 0),
			"Unicode filter property output mismatch");

		memcpy(arrInPlace, arrSourceText, iSourceSize);
		testRequire(xrtUtf8FilterTo(
			(xstrview){ (cstr)arrInPlace, iSourceSize },
			(xstrview){ (cstr)arrSetText, iSetSize },
			(char*)arrInPlace, sizeof(arrInPlace), &iActual) &&
			(iActual == iExpected) &&
			(memcmp(arrInPlace, arrExpected, iExpected) == 0) &&
			(arrInPlace[iExpected] == 0),
			"Unicode filter property in-place mismatch");

		iExpected = testUnicodeReverseReference(arrSource,
			iSourceCount, arrExpected);
		testRequire(xrtUtf8ReverseTo(
			(xstrview){ (cstr)arrSourceText, iSourceSize },
			(char*)arrActual, sizeof(arrActual)) &&
			(iExpected == iSourceSize) &&
			(memcmp(arrActual, arrExpected, iExpected) == 0) &&
			(arrActual[iExpected] == 0),
			"Unicode reverse property output mismatch");
		memcpy(arrInPlace, arrSourceText, iSourceSize);
		testRequire(xrtUtf8ReverseTo(
			(xstrview){ (cstr)arrInPlace, iSourceSize },
			(char*)arrInPlace, sizeof(arrInPlace)) &&
			(memcmp(arrInPlace, arrExpected, iExpected) == 0) &&
			(arrInPlace[iExpected] == 0),
			"Unicode reverse property in-place mismatch");
	}
	printf("[PASS] unicode-text-property\n");
	return 0;
}
