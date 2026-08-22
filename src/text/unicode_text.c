#include "../internal/xrt_charset.h"



#if defined(XRT_FEATURE_UNICODE_TEXT)

/* UTF-8 过滤索引用精确 ASCII 位图和非 ASCII 布隆预筛避免无效扫描。 */
typedef struct xrt_utf8_filter_index {
	uint64 Ascii[2];
	uint64 Bloom[4];
} xrt_utf8_filter_index;



/* Unicode 标量填充计划分别记录标量数和实际字节数。 */
typedef struct xrt_utf8_pad_plan {
	xstrview Fill;
	size_t FillCount;
	size_t LeftCount;
	size_t RightCount;
	size_t LeftBytes;
	size_t RightBytes;
	size_t OutputBytes;
} xrt_utf8_pad_plan;



/* 反转一段可写字节。 */
static void __xrtUtf8ReverseRange(char* sText, size_t iSize)
{
	for ( size_t i = 0; i < (iSize / 2u); i++ ) {
		char iByte = sText[i];

		sText[i] = sText[iSize - i - 1u];
		sText[iSize - i - 1u] = iByte;
	}
}



/* 验证需要补零输出的严格 UTF-8 视图。 */
static bool __xrtUtf8TextValid(xstrview Text, cstr sOperation)
{
	size_t iError = XRT_NPOS;

	if ( (Text.Data == NULL) && (Text.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( Text.Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( !xrtUtf8Valid(Text, &iError) ) {
		__xrtUtfSetInvalid(sOperation, iError);
		return false;
	}
	return true;
}



/* 严格统计 UTF-8 标量并保留首个非法字节位置。 */
static bool __xrtUtf8TextCount(xstrview Text, cstr sOperation, size_t* pCount)
{
	size_t iPosition = 0;
	size_t iCount = 0;

	if ( (pCount == NULL) || !__xrtUtf8TextValid(Text, sOperation) ) {
		if ( pCount == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	while ( iPosition < Text.Size ) {
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iPosition,
			Text.Size - iPosition);

		iPosition += Decode.Read;
		iCount++;
	}
	*pCount = iCount;
	return true;
}



/* 把带负数的标量位置钳制到闭区间 [0, iCount]。 */
static size_t __xrtUtf8NormalizeIndex(size_t iCount, int64 iIndex)
{
	if ( iIndex < 0 ) {
		uint64 iBack = (uint64)(-(iIndex + 1)) + 1u;

		return iBack >= (uint64)iCount ? 0 :
			iCount - (size_t)iBack;
	}
	return (uint64)iIndex > (uint64)iCount ?
		iCount : (size_t)iIndex;
}



/* 在已经验证的文本中查找两个标量边界对应的字节位置。 */
static void __xrtUtf8RangeOffsets(xstrview Text, size_t iStart,
	size_t iEnd, size_t* pStartByte, size_t* pEndByte)
{
	size_t iPosition = 0;
	size_t iIndex = 0;
	size_t iStartByte = Text.Size;
	size_t iEndByte = Text.Size;

	while ( iPosition < Text.Size ) {
		xrt_utf_decode Decode;

		if ( iIndex == iStart ) {
			iStartByte = iPosition;
		}
		if ( iIndex == iEnd ) {
			iEndByte = iPosition;
			break;
		}
		Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iPosition,
			Text.Size - iPosition);
		iPosition += Decode.Read;
		iIndex++;
	}
	if ( iIndex == iStart ) {
		iStartByte = iPosition;
	}
	if ( iIndex == iEnd ) {
		iEndByte = iPosition;
	}
	*pStartByte = iStartByte;
	*pEndByte = iEndByte;
}



/* 解析带负索引的标量范围，调用方负责验证输出指针。 */
static bool __xrtUtf8RangeResolve(xstrview Text, int64 iStart,
	int64 iCount, cstr sOperation, xstrview* pRange)
{
	size_t iTextCount;
	size_t iStartIndex;
	size_t iEndIndex;
	size_t iStartByte;
	size_t iEndByte;

	if ( !__xrtUtf8TextCount(Text, sOperation, &iTextCount) ) {
		return false;
	}
	iStartIndex = __xrtUtf8NormalizeIndex(iTextCount, iStart);
	if ( iCount < 0 ) {
		iEndIndex = iTextCount;
	} else if ( (uint64)iCount >=
		(uint64)(iTextCount - iStartIndex) ) {
		iEndIndex = iTextCount;
	} else {
		iEndIndex = iStartIndex + (size_t)iCount;
	}
	__xrtUtf8RangeOffsets(Text, iStartIndex, iEndIndex,
		&iStartByte, &iEndByte);
	pRange->Data = Text.Data != NULL ? Text.Data + iStartByte : NULL;
	pRange->Size = iEndByte - iStartByte;
	return true;
}



/* 把非 ASCII 标量折叠到 256 位布隆预筛。 */
static size_t __xrtUtf8FilterHash(uint32 iScalar)
{
	return (size_t)((iScalar ^ (iScalar >> 8u) ^ (iScalar >> 16u)) & 0xFFu);
}



/* 为已经验证的 UTF-8 标量集合建立无分配索引。 */
static void __xrtUtf8FilterIndexBuild(xstrview Set,
	xrt_utf8_filter_index* pIndex)
{
	size_t iPosition = 0;

	memset(pIndex, 0, sizeof(*pIndex));
	while ( iPosition < Set.Size ) {
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Set.Data + iPosition,
			Set.Size - iPosition);

		if ( Decode.Scalar < 0x80u ) {
			pIndex->Ascii[Decode.Scalar >> 6u] |=
				UINT64_C(1) << (Decode.Scalar & 63u);
		} else {
			size_t iHash = __xrtUtf8FilterHash(Decode.Scalar);

			pIndex->Bloom[iHash >> 6u] |= UINT64_C(1) << (iHash & 63u);
		}
		iPosition += Decode.Read;
	}
}



/* 判断严格 UTF-8 集合中是否存在指定标量。 */
static bool __xrtUtf8FilterContains(xstrview Set,
	const xrt_utf8_filter_index* pIndex, uint32 iScalar)
{
	size_t iPosition = 0;

	if ( iScalar < 0x80u ) {
		return (pIndex->Ascii[iScalar >> 6u] &
			(UINT64_C(1) << (iScalar & 63u))) != 0;
	}
	{
		size_t iHash = __xrtUtf8FilterHash(iScalar);

		if ( (pIndex->Bloom[iHash >> 6u] &
				(UINT64_C(1) << (iHash & 63u))) == 0 ) {
			return false;
		}
	}
	while ( iPosition < Set.Size ) {
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Set.Data + iPosition,
			Set.Size - iPosition);

		if ( Decode.Scalar == iScalar ) {
			return true;
		}
		iPosition += Decode.Read;
	}
	return false;
}



/* 用已经验证的标量集合裁剪文本两侧并返回借用视图。 */
static void __xrtUtf8TrimBody(xstrview Text, xstrview Set,
	const xrt_utf8_filter_index* pIndex, bool bLeft, bool bRight,
	xstrview* pResult)
{
	size_t iPosition = 0;
	size_t iBegin = 0;
	size_t iEnd = bRight ? 0 : Text.Size;
	bool bLeading = bLeft;

	while ( iPosition < Text.Size ) {
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iPosition,
			Text.Size - iPosition);
		bool bMember = __xrtUtf8FilterContains(
			Set, pIndex, Decode.Scalar);

		if ( bLeading && bMember ) {
			iBegin = iPosition + Decode.Read;
		} else {
			bLeading = false;
		}
		if ( bRight && !bMember ) {
			iEnd = iPosition + Decode.Read;
		}
		iPosition += Decode.Read;
	}
	if ( bLeading ) {
		iEnd = iBegin;
	}
	pResult->Data = Text.Data != NULL ? Text.Data + iBegin : NULL;
	pResult->Size = iEnd - iBegin;
}



/* 计算指定数量的重复填充标量需要多少字节。 */
static bool __xrtUtf8PadBytes(xstrview Fill, size_t iFillCount,
	size_t iCount, size_t* pBytes)
{
	size_t iWhole = iCount / iFillCount;
	size_t iRest = iCount % iFillCount;
	size_t iRestBytes = 0;

	if ( iWhole > (SIZE_MAX / Fill.Size) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( iRest != 0 ) {
		iRestBytes = xrtUtf8Offset(Fill, iRest);
		if ( iRestBytes == XRT_NPOS ) {
			return false;
		}
	}
	if ( iRestBytes > (SIZE_MAX - (iWhole * Fill.Size)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pBytes = (iWhole * Fill.Size) + iRestBytes;
	return true;
}



/* 为标量宽度填充验证输入并计算精确输出布局。 */
static bool __xrtUtf8PadPlan(xstrview Text, size_t iWidth,
	xstrview Fill, int iMode, xrt_utf8_pad_plan* pPlan)
{
	static const char sSpace[] = " ";
	size_t iTextCount;
	size_t iNeed;

	if ( !__xrtUtf8TextCount(Text, "utf8-pad-text", &iTextCount) ||
		 !__xrtUtf8TextCount(Fill, "utf8-pad-fill", &pPlan->FillCount) ) {
		return false;
	}
	pPlan->Fill = Fill;
	if ( Fill.Size == 0 ) {
		pPlan->Fill.Data = sSpace;
		pPlan->Fill.Size = 1;
		pPlan->FillCount = 1;
	}
	if ( iWidth <= iTextCount ) {
		pPlan->LeftCount = 0;
		pPlan->RightCount = 0;
		pPlan->LeftBytes = 0;
		pPlan->RightBytes = 0;
		pPlan->OutputBytes = Text.Size;
		return true;
	}
	iNeed = iWidth - iTextCount;
	pPlan->LeftCount = iMode < 0 ? iNeed :
		(iMode == 0 ? iNeed / 2u : 0u);
	pPlan->RightCount = iNeed - pPlan->LeftCount;
	if ( !__xrtUtf8PadBytes(pPlan->Fill, pPlan->FillCount,
			pPlan->LeftCount, &pPlan->LeftBytes) ||
		 !__xrtUtf8PadBytes(pPlan->Fill, pPlan->FillCount,
			pPlan->RightCount, &pPlan->RightBytes) ) {
		return false;
	}
	if ( (pPlan->LeftBytes > (SIZE_MAX - Text.Size)) ||
		 (pPlan->RightBytes >
			(SIZE_MAX - Text.Size - pPlan->LeftBytes)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pPlan->OutputBytes =
		pPlan->LeftBytes + Text.Size + pPlan->RightBytes;
	if ( pPlan->OutputBytes == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	return true;
}



/* 高效写出从填充文本开头循环的指定字节数。 */
static void __xrtUtf8PadWrite(char* sOutput, size_t iBytes, xstrview Fill)
{
	size_t iWholeBytes = iBytes - (iBytes % Fill.Size);
	size_t iWritten = 0;

	if ( iWholeBytes != 0 ) {
		memcpy(sOutput, Fill.Data, Fill.Size);
		iWritten = Fill.Size;
		while ( iWritten < iWholeBytes ) {
			size_t iTake = iWritten < (iWholeBytes - iWritten) ?
				iWritten : iWholeBytes - iWritten;

			memcpy(sOutput + iWritten, sOutput, iTake);
			iWritten += iTake;
		}
	}
	if ( iWritten < iBytes ) {
		memcpy(sOutput + iWritten, Fill.Data, iBytes - iWritten);
	}
}



/* 按标量宽度创建填充后的 UTF-8 字符串。 */
static str __xrtUtf8Pad(xstrview Text, size_t iWidth,
	xstrview Fill, int iMode)
{
	xrt_utf8_pad_plan Plan;
	str sResult;

	if ( !__xrtUtf8PadPlan(Text, iWidth, Fill, iMode, &Plan) ) {
		return NULL;
	}
	sResult = (str)xrtMalloc(Plan.OutputBytes + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	__xrtUtf8PadWrite(sResult, Plan.LeftBytes, Plan.Fill);
	if ( Text.Size != 0 ) {
		memcpy(sResult + Plan.LeftBytes, Text.Data, Text.Size);
	}
	__xrtUtf8PadWrite(sResult + Plan.LeftBytes + Text.Size,
		Plan.RightBytes, Plan.Fill);
	sResult[Plan.OutputBytes] = 0;
	return sResult;
}



/* 验证文本和集合并计算 UTF-8 标量过滤后的字节数。 */
static bool __xrtUtf8FilterMeasure(xstrview Text, xstrview Set,
	xrt_utf8_filter_index* pIndex, size_t* pSize)
{
	size_t iPosition = 0;
	size_t iSize = 0;

	if ( !__xrtUtf8TextValid(Text, "utf8-filter-text") ||
		 !__xrtUtf8TextValid(Set, "utf8-filter-set") ) {
		return false;
	}
	__xrtUtf8FilterIndexBuild(Set, pIndex);
	while ( iPosition < Text.Size ) {
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iPosition,
			Text.Size - iPosition);

		if ( !__xrtUtf8FilterContains(Set, pIndex, Decode.Scalar) ) {
			iSize += Decode.Read;
		}
		iPosition += Decode.Read;
	}
	*pSize = iSize;
	return true;
}



/* 在已经验证的输入上执行向前原地安全 UTF-8 标量过滤。 */
static void __xrtUtf8FilterBody(xstrview Text, xstrview Set,
	const xrt_utf8_filter_index* pIndex, char* sOutput)
{
	size_t iRead = 0;
	size_t iWrite = 0;

	while ( iRead < Text.Size ) {
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iRead, Text.Size - iRead);

		if ( !__xrtUtf8FilterContains(Set, pIndex, Decode.Scalar) ) {
			memmove(sOutput + iWrite, Text.Data + iRead, Decode.Read);
			iWrite += Decode.Read;
		}
		iRead += Decode.Read;
	}
	sOutput[iWrite] = 0;
}



/* 在已经验证的输入上执行分离缓冲区或原地 UTF-8 标量反转。 */
static void __xrtUtf8ReverseBody(xstrview Text, char* sOutput)
{
	size_t iRead = 0;

	if ( (const void*)sOutput == (const void*)Text.Data ) {
		while ( iRead < Text.Size ) {
			xrt_utf_decode Decode = __xrtUtf8Decode(
				(const unsigned char*)sOutput + iRead,
				Text.Size - iRead);

			__xrtUtf8ReverseRange(sOutput + iRead, Decode.Read);
			iRead += Decode.Read;
		}
		__xrtUtf8ReverseRange(sOutput, Text.Size);
		sOutput[Text.Size] = 0;
		return;
	}
	{
		size_t iWrite = Text.Size;

		while ( iRead < Text.Size ) {
			xrt_utf_decode Decode = __xrtUtf8Decode(
				(const unsigned char*)Text.Data + iRead,
				Text.Size - iRead);

			iWrite -= Decode.Read;
			memcpy(sOutput + iWrite, Text.Data + iRead, Decode.Read);
			iRead += Decode.Read;
		}
	}
	sOutput[Text.Size] = 0;
}



/* 按 Unicode 标量解析带负索引的借用范围。 */
XRT_API bool xrtUtf8Range(xstrview Text, int64 iStart, int64 iCount,
	xstrview* pRange)
{
	if ( pRange == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pRange->Data = NULL;
	pRange->Size = 0;
	return __xrtUtf8RangeResolve(
		Text, iStart, iCount, "utf8-range", pRange);
}



/* 按 Unicode 标量复制带负索引的范围。 */
XRT_API str xrtUtf8Substr(xstrview Text, int64 iStart, int64 iCount)
{
	xstrview Range;

	if ( !__xrtUtf8RangeResolve(
			Text, iStart, iCount, "utf8-substr", &Range) ) {
		return NULL;
	}
	return xrtStrDupView(Range);
}



/* 使用字节搜索实现严格 UTF-8 标量索引搜索。 */
static size_t __xrtUtf8Find(xstrview Text, xstrview Part,
	size_t iStart, bool bCaseInsensitive)
{
	size_t iTextCount;
	size_t iStartByte;
	size_t iFound;

	if ( !__xrtUtf8TextCount(Text, "utf8-find-text", &iTextCount) ||
		 !__xrtUtf8TextValid(Part, "utf8-find-part") ) {
		return XRT_NPOS;
	}
	if ( iStart > iTextCount ) {
		__xrtErrorSetRange();
		return XRT_NPOS;
	}
	iStartByte = xrtUtf8Offset(Text, iStart);
	if ( iStartByte == XRT_NPOS ) {
		return XRT_NPOS;
	}
	iFound = bCaseInsensitive ?
		xrtStrCaseFind(Text, Part, iStartByte) :
		xrtStrFind(Text, Part, iStartByte);
	if ( iFound == XRT_NPOS ) {
		return XRT_NPOS;
	}
	return xrtUtf8Index(Text, iFound);
}



/* 从指定标量索引查找严格 UTF-8 子串。 */
XRT_API size_t xrtUtf8Find(xstrview Text, xstrview Part, size_t iStart)
{
	return __xrtUtf8Find(Text, Part, iStart, false);
}



/* 按 ASCII 大小写不敏感规则查找严格 UTF-8 子串。 */
XRT_API size_t xrtUtf8CaseFind(xstrview Text, xstrview Part, size_t iStart)
{
	return __xrtUtf8Find(Text, Part, iStart, true);
}



/* 使用字节搜索实现严格 UTF-8 反向标量索引搜索。 */
static size_t __xrtUtf8RFind(xstrview Text, xstrview Part,
	bool bCaseInsensitive)
{
	size_t iFound;

	if ( !__xrtUtf8TextValid(Text, "utf8-rfind-text") ||
		 !__xrtUtf8TextValid(Part, "utf8-rfind-part") ) {
		return XRT_NPOS;
	}
	iFound = bCaseInsensitive ?
		xrtStrCaseRFind(Text, Part) : xrtStrRFind(Text, Part);
	if ( iFound == XRT_NPOS ) {
		return XRT_NPOS;
	}
	return xrtUtf8Index(Text, iFound);
}



/* 从右侧查找严格 UTF-8 子串并返回标量索引。 */
XRT_API size_t xrtUtf8RFind(xstrview Text, xstrview Part)
{
	return __xrtUtf8RFind(Text, Part, false);
}



/* 按 ASCII 大小写不敏感规则从右侧查找严格 UTF-8 子串。 */
XRT_API size_t xrtUtf8CaseRFind(xstrview Text, xstrview Part)
{
	return __xrtUtf8RFind(Text, Part, true);
}



/* 判断文本是否包含集合中的任意 Unicode 标量。 */
XRT_API bool xrtUtf8ContainsAny(xstrview Text, xstrview Set)
{
	xrt_utf8_filter_index Index;
	size_t iPosition = 0;

	if ( !__xrtUtf8TextValid(Text, "utf8-contains-any-text") ||
		 !__xrtUtf8TextValid(Set, "utf8-contains-any-set") ) {
		return false;
	}
	__xrtUtf8FilterIndexBuild(Set, &Index);
	while ( iPosition < Text.Size ) {
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iPosition,
			Text.Size - iPosition);

		if ( __xrtUtf8FilterContains(Set, &Index, Decode.Scalar) ) {
			return true;
		}
		iPosition += Decode.Read;
	}
	return false;
}



/* 执行一个方向组合的 Unicode 标量集合裁剪。 */
static bool __xrtUtf8TrimSet(xstrview Text, xstrview Set,
	bool bLeft, bool bRight, xstrview* pResult)
{
	xrt_utf8_filter_index Index;

	if ( pResult == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pResult->Data = NULL;
	pResult->Size = 0;
	if ( !__xrtUtf8TextValid(Text, "utf8-trim-text") ||
		 !__xrtUtf8TextValid(Set, "utf8-trim-set") ) {
		return false;
	}
	__xrtUtf8FilterIndexBuild(Set, &Index);
	__xrtUtf8TrimBody(Text, Set, &Index, bLeft, bRight, pResult);
	return true;
}



/* 删除左侧属于指定 Unicode 标量集合的内容。 */
XRT_API bool xrtUtf8TrimLeftSet(xstrview Text, xstrview Set,
	xstrview* pResult)
{
	return __xrtUtf8TrimSet(Text, Set, true, false, pResult);
}



/* 删除右侧属于指定 Unicode 标量集合的内容。 */
XRT_API bool xrtUtf8TrimRightSet(xstrview Text, xstrview Set,
	xstrview* pResult)
{
	return __xrtUtf8TrimSet(Text, Set, false, true, pResult);
}



/* 删除两侧属于指定 Unicode 标量集合的内容。 */
XRT_API bool xrtUtf8TrimSet(xstrview Text, xstrview Set,
	xstrview* pResult)
{
	return __xrtUtf8TrimSet(Text, Set, true, true, pResult);
}



/* 按 Unicode 标量位置插入严格 UTF-8 子串。 */
XRT_API str xrtUtf8Insert(xstrview Text, int64 iPosition, xstrview Part)
{
	xstrview Position;
	size_t iByte;

	if ( !__xrtUtf8RangeResolve(
			Text, iPosition, 0, "utf8-insert-text", &Position) ||
		 !__xrtUtf8TextValid(Part, "utf8-insert-part") ) {
		return NULL;
	}
	iByte = Text.Data != NULL ?
		(size_t)(Position.Data - Text.Data) : 0;
	return xrtStrInsert(Text, iByte, Part);
}



/* 按 Unicode 标量范围删除内容。 */
XRT_API str xrtUtf8Remove(xstrview Text, int64 iStart, int64 iCount)
{
	xstrview Range;
	size_t iByte;

	if ( !__xrtUtf8RangeResolve(
			Text, iStart, iCount, "utf8-remove", &Range) ) {
		return NULL;
	}
	iByte = Text.Data != NULL ?
		(size_t)(Range.Data - Text.Data) : 0;
	return xrtStrRemove(Text, iByte, Range.Size);
}



/* 按 Unicode 标量宽度在左侧重复填充文本。 */
XRT_API str xrtUtf8PadLeft(xstrview Text, size_t iWidth, xstrview Fill)
{
	return __xrtUtf8Pad(Text, iWidth, Fill, -1);
}



/* 按 Unicode 标量宽度在右侧重复填充文本。 */
XRT_API str xrtUtf8PadRight(xstrview Text, size_t iWidth, xstrview Fill)
{
	return __xrtUtf8Pad(Text, iWidth, Fill, 1);
}



/* 按 Unicode 标量宽度在两侧重复填充文本。 */
XRT_API str xrtUtf8PadCenter(xstrview Text, size_t iWidth, xstrview Fill)
{
	return __xrtUtf8Pad(Text, iWidth, Fill, 0);
}



/* 按 Unicode 标量反转严格 UTF-8 文本到调用方缓冲区。 */
XRT_API bool xrtUtf8ReverseTo(xstrview Text, char* sOutput, size_t iCapacity)
{
	if ( !__xrtUtf8TextValid(Text, "utf8-reverse") ) {
		return false;
	}
	if ( sOutput == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity <= Text.Size ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( __xrtRangesOverlap(sOutput, Text.Size + 1u,
			Text.Data, Text.Size) &&
		 ((const void*)sOutput != (const void*)Text.Data) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtUtf8ReverseBody(Text, sOutput);
	return true;
}



/* 按 Unicode 标量反转严格 UTF-8 文本并创建独立字符串。 */
XRT_API str xrtUtf8Reverse(xstrview Text)
{
	str sResult;

	if ( !__xrtUtf8TextValid(Text, "utf8-reverse") ) {
		return NULL;
	}
	sResult = (str)xrtMalloc(Text.Size + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	__xrtUtf8ReverseBody(Text, sResult);
	return sResult;
}



/* 按 Unicode 标量集合过滤严格 UTF-8 文本到调用方缓冲区。 */
XRT_API bool xrtUtf8FilterTo(xstrview Text, xstrview Set,
	char* sOutput, size_t iCapacity, size_t* pOutputSize)
{
	xrt_utf8_filter_index Index;
	size_t iRequired;

	if ( (pOutputSize == NULL) ||
		 ((sOutput == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtUtf8FilterMeasure(Text, Set, &Index, &iRequired) ) {
		return false;
	}
	if ( __xrtRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			Text.Data, Text.Size) ||
		 __xrtRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			Set.Data, Set.Size) ||
		 ((sOutput != NULL) && __xrtRangesOverlap(
			pOutputSize, sizeof(*pOutputSize), sOutput, iRequired + 1u)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtErrorSetRange();
		return false;
	}
	if ( (__xrtRangesOverlap(sOutput, iRequired + 1u,
			Text.Data, Text.Size) &&
		 ((const void*)sOutput != (const void*)Text.Data)) ||
		 __xrtRangesOverlap(sOutput, iRequired + 1u,
			Set.Data, Set.Size) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtUtf8FilterBody(Text, Set, &Index, sOutput);
	*pOutputSize = iRequired;
	return true;
}



/* 按 Unicode 标量集合过滤严格 UTF-8 文本并创建独立字符串。 */
XRT_API str xrtUtf8Filter(xstrview Text, xstrview Set)
{
	xrt_utf8_filter_index Index;
	size_t iSize;
	str sResult;

	if ( !__xrtUtf8FilterMeasure(Text, Set, &Index, &iSize) ) {
		return NULL;
	}
	sResult = (str)xrtMalloc(iSize + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	__xrtUtf8FilterBody(Text, Set, &Index, sResult);
	return sResult;
}

#endif
