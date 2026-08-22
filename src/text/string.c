#include "../internal/xrt_string.h"



#if defined(XRT_FEATURE_STRING)

/* 判断一个字节是否存在于指定集合。 */
static bool __xrtStrByteInSet(unsigned char iByte, xstrview Set)
{
	return (Set.Size != 0) && (memchr(Set.Data, iByte, Set.Size) != NULL);
}



/* 判断两个明确长度片段是否按 ASCII 大小写不敏感规则相等。 */
static bool __xrtStrCaseBytesEqual(cstr sLeft, cstr sRight, size_t iSize)
{
	for ( size_t i = 0; i < iSize; i++ ) {
		if ( __xrtStrAsciiLower((unsigned char)sLeft[i]) !=
			 __xrtStrAsciiLower((unsigned char)sRight[i]) ) {
			return false;
		}
	}
	return true;
}



/* 反转一段可写字节，不读取范围之外的数据。 */
static void __xrtStrReverseRange(char* sText, size_t iSize)
{
	for ( size_t i = 0; i < (iSize / 2u); i++ ) {
		char iByte = sText[i];

		sText[i] = sText[iSize - i - 1u];
		sText[iSize - i - 1u] = iByte;
	}
}



/* 在已经验证的等长缓冲区之间执行 ASCII 小写转换。 */
static void __xrtStrLowerBody(xstrview Text, char* sOutput)
{
	for ( size_t i = 0; i < Text.Size; i++ ) {
		sOutput[i] = (char)__xrtStrAsciiLower((unsigned char)Text.Data[i]);
	}
	sOutput[Text.Size] = 0;
}



/* 在已经验证的等长缓冲区之间执行 ASCII 大写转换。 */
static void __xrtStrUpperBody(xstrview Text, char* sOutput)
{
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z') ) {
			iByte = (unsigned char)(iByte -
				((unsigned char)'a' - (unsigned char)'A'));
		}
		sOutput[i] = (char)iByte;
	}
	sOutput[Text.Size] = 0;
}



/* 验证等长字符串变换的目标缓冲区和重叠关系。 */
static bool __xrtStrTransformTargetValid(xstrview Text,
	char* sOutput, size_t iCapacity)
{
	if ( !__xrtStrViewValid(Text) ) {
		return false;
	}
	if ( sOutput == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( Text.Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
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
	return true;
}



/* 建立字节集合位图并计算过滤后的长度。 */
static bool __xrtStrFilterMeasure(xstrview Text, xstrview Set,
	uint64 arrRemove[4], size_t* pSize)
{
	size_t iSize = 0;

	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Set) ) {
		return false;
	}
	if ( Text.Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	memset(arrRemove, 0, sizeof(uint64) * 4u);
	for ( size_t i = 0; i < Set.Size; i++ ) {
		uint8 iByte = (uint8)Set.Data[i];

		arrRemove[iByte >> 6u] |= UINT64_C(1) << (iByte & 63u);
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		uint8 iByte = (uint8)Text.Data[i];

		if ( (arrRemove[iByte >> 6u] &
				(UINT64_C(1) << (iByte & 63u))) == 0 ) {
			iSize++;
		}
	}
	*pSize = iSize;
	return true;
}



/* 使用已经建立的位图执行向前原地安全过滤。 */
static void __xrtStrFilterBody(xstrview Text, const uint64 arrRemove[4],
	char* sOutput)
{
	size_t iWrite = 0;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		uint8 iByte = (uint8)Text.Data[i];

		if ( (arrRemove[iByte >> 6u] &
				(UINT64_C(1) << (iByte & 63u))) == 0 ) {
			sOutput[iWrite++] = Text.Data[i];
		}
	}
	sOutput[iWrite] = 0;
}



/* 复制一个已经验证的字符串视图。 */
static str __xrtStrCopyView(xstrview Text)
{
	str sResult;

	if ( Text.Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sResult = (str)xrtMalloc(Text.Size + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	if ( Text.Size != 0 ) {
		memcpy(sResult, Text.Data, Text.Size);
	}
	sResult[Text.Size] = 0;
	return sResult;
}



/* 向输出地址重复写入指定填充视图。 */
static void __xrtStrWriteFill(str sOutput, size_t iSize, xstrview Fill)
{
	size_t iWritten;

	iWritten = Fill.Size < iSize ? Fill.Size : iSize;
	if ( iWritten != 0 ) {
		memcpy(sOutput, Fill.Data, iWritten);
	}
	while ( iWritten < iSize ) {
		size_t iTake = iWritten < (iSize - iWritten) ?
			iWritten : iSize - iWritten;

		memcpy(sOutput + iWritten, sOutput, iTake);
		iWritten += iTake;
	}
}



/* 创建按字节宽度填充的字符串。 */
static str __xrtStrPad(xstrview Text, size_t iWidth, xstrview Fill, int iMode)
{
	static const char sSpace[] = " ";
	size_t iNeed;
	size_t iLeft;
	size_t iRight;
	str sResult;

	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Fill) ) {
		return NULL;
	}
	if ( iWidth <= Text.Size ) {
		return __xrtStrCopyView(Text);
	}
	if ( Fill.Size == 0 ) {
		Fill.Data = sSpace;
		Fill.Size = 1;
	}
	iNeed = iWidth - Text.Size;
	iLeft = iMode < 0 ? iNeed : (iMode == 0 ? iNeed / 2u : 0u);
	iRight = iNeed - iLeft;
	if ( iWidth == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sResult = (str)xrtMalloc(iWidth + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	__xrtStrWriteFill(sResult, iLeft, Fill);
	if ( Text.Size != 0 ) {
		memcpy(sResult + iLeft, Text.Data, Text.Size);
	}
	__xrtStrWriteFill(sResult + iLeft + Text.Size, iRight, Fill);
	sResult[iWidth] = 0;
	return sResult;
}



/* 从零结尾字符串创建借用视图，空指针视为空字符串。 */
XRT_API xstrview xrtStrView(cstr sText)
{
	xstrview Text;

	Text.Data = sText;
	Text.Size = sText != NULL ? strlen(sText) : 0;
	return Text;
}



/* 从明确长度创建借用视图。 */
XRT_API xstrview xrtStrViewN(cstr sText, size_t iSize)
{
	xstrview Text;

	Text.Data = sText;
	Text.Size = iSize;
	return Text;
}



/* 判断字符串视图是否为空。 */
XRT_API bool xrtStrEmpty(xstrview Text)
{
	if ( !__xrtStrViewValid(Text) ) {
		return true;
	}
	return Text.Size == 0;
}



/* 按无符号字节进行词典序比较。 */
XRT_API int xrtStrCompare(xstrview Left, xstrview Right)
{
	size_t iCommon;
	int iResult;

	if ( !__xrtStrViewValid(Left) || !__xrtStrViewValid(Right) ) {
		return 0;
	}
	iCommon = Left.Size < Right.Size ? Left.Size : Right.Size;
	iResult = iCommon != 0 ? memcmp(Left.Data, Right.Data, iCommon) : 0;
	if ( iResult != 0 ) {
		return iResult;
	}
	if ( Left.Size == Right.Size ) {
		return 0;
	}
	return Left.Size < Right.Size ? -1 : 1;
}



/* 按 ASCII 大小写不敏感规则进行词典序比较。 */
XRT_API int xrtStrCaseCompare(xstrview Left, xstrview Right)
{
	size_t iCommon;

	if ( !__xrtStrViewValid(Left) || !__xrtStrViewValid(Right) ) {
		return 0;
	}
	iCommon = Left.Size < Right.Size ? Left.Size : Right.Size;
	for ( size_t i = 0; i < iCommon; i++ ) {
		unsigned char iLeft = __xrtStrAsciiLower((unsigned char)Left.Data[i]);
		unsigned char iRight = __xrtStrAsciiLower((unsigned char)Right.Data[i]);

		if ( iLeft != iRight ) {
			return iLeft < iRight ? -1 : 1;
		}
	}
	if ( Left.Size == Right.Size ) {
		return 0;
	}
	return Left.Size < Right.Size ? -1 : 1;
}



/* 判断两个字符串视图是否完全相等。 */
XRT_API bool xrtStrEqual(xstrview Left, xstrview Right)
{
	if ( !__xrtStrViewValid(Left) || !__xrtStrViewValid(Right) ) {
		return false;
	}
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) || (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 按 ASCII 大小写不敏感规则判断相等。 */
XRT_API bool xrtStrCaseEqual(xstrview Left, xstrview Right)
{
	if ( !__xrtStrViewValid(Left) || !__xrtStrViewValid(Right) ) {
		return false;
	}
	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( size_t i = 0; i < Left.Size; i++ ) {
		if ( __xrtStrAsciiLower((unsigned char)Left.Data[i]) !=
			 __xrtStrAsciiLower((unsigned char)Right.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 从指定字节位置查找子串，未找到返回 XRT_NPOS。 */
XRT_API size_t xrtStrFind(xstrview Text, xstrview Part, size_t iStart)
{
	size_t iLimit;

	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Part) || (iStart > Text.Size) ) {
		return XRT_NPOS;
	}
	if ( Part.Size == 0 ) {
		return iStart;
	}
	if ( Part.Size > (Text.Size - iStart) ) {
		return XRT_NPOS;
	}
	if ( Part.Size == 1 ) {
		cstr sFound = (cstr)memchr(Text.Data + iStart, (unsigned char)Part.Data[0], Text.Size - iStart);

		return sFound != NULL ? (size_t)(sFound - Text.Data) : XRT_NPOS;
	}

	/* 短模式使用首字节过滤，避免每次查询初始化跳转表。 */
	iLimit = Text.Size - Part.Size;
	if ( Part.Size < 8u ) {
		size_t iPosition = iStart;

		while ( iPosition <= iLimit ) {
			cstr sFound = (cstr)memchr(Text.Data + iPosition,
				(unsigned char)Part.Data[0], (iLimit - iPosition) + 1u);

			if ( sFound == NULL ) {
				return XRT_NPOS;
			}
			iPosition = (size_t)(sFound - Text.Data);
			if ( memcmp(Text.Data + iPosition, Part.Data, Part.Size) == 0 ) {
				return iPosition;
			}
			if ( iPosition == iLimit ) {
				return XRT_NPOS;
			}
			iPosition++;
		}
		return XRT_NPOS;
	}

	/* 长模式复用旧版已经验证的 Boyer-Moore-Horspool 跳转策略。 */
	{
		size_t arrShift[256];
		size_t iPosition = iStart;
		unsigned char iLastPart = (unsigned char)Part.Data[Part.Size - 1u];

		for ( size_t i = 0; i < 256u; i++ ) {
			arrShift[i] = Part.Size;
		}
		for ( size_t i = 0; (i + 1u) < Part.Size; i++ ) {
			arrShift[(unsigned char)Part.Data[i]] = Part.Size - i - 1u;
		}
		while ( iPosition <= iLimit ) {
			unsigned char iLastText = (unsigned char)Text.Data[iPosition + Part.Size - 1u];

			if ( (iLastText == iLastPart) &&
				 (memcmp(Text.Data + iPosition, Part.Data, Part.Size - 1u) == 0) ) {
				return iPosition;
			}
			if ( arrShift[iLastText] > (iLimit - iPosition) ) {
				break;
			}
			iPosition += arrShift[iLastText];
		}
	}
	return XRT_NPOS;
}



/* 按 ASCII 大小写不敏感规则查找子串。 */
XRT_API size_t xrtStrCaseFind(xstrview Text, xstrview Part, size_t iStart)
{
	size_t iLimit;

	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Part) || (iStart > Text.Size) ) {
		return XRT_NPOS;
	}
	if ( Part.Size == 0 ) {
		return iStart;
	}
	if ( Part.Size > (Text.Size - iStart) ) {
		return XRT_NPOS;
	}
	iLimit = Text.Size - Part.Size;

	/* 短模式避免初始化跳转表，长模式按折叠后的尾字节跳过无效位置。 */
	if ( Part.Size < 8u ) {
		size_t iPosition = iStart;

		for ( ;; ) {
			size_t j = 0;

			while ( (j < Part.Size) &&
				(__xrtStrAsciiLower((unsigned char)Text.Data[iPosition + j]) ==
				 __xrtStrAsciiLower((unsigned char)Part.Data[j])) ) {
				j++;
			}
			if ( j == Part.Size ) {
				return iPosition;
			}
			if ( iPosition == iLimit ) {
				return XRT_NPOS;
			}
			iPosition++;
		}
	}

	{
		size_t arrShift[256];
		size_t iPosition = iStart;
		unsigned char iLastPart = __xrtStrAsciiLower(
			(unsigned char)Part.Data[Part.Size - 1u]);

		for ( size_t i = 0; i < 256u; i++ ) {
			arrShift[i] = Part.Size;
		}
		for ( size_t i = 0; (i + 1u) < Part.Size; i++ ) {
			arrShift[__xrtStrAsciiLower(
				(unsigned char)Part.Data[i])] = Part.Size - i - 1u;
		}
		while ( iPosition <= iLimit ) {
			unsigned char iLastText = __xrtStrAsciiLower(
				(unsigned char)Text.Data[iPosition + Part.Size - 1u]);

			if ( (iLastText == iLastPart) && __xrtStrCaseBytesEqual(
				Text.Data + iPosition, Part.Data, Part.Size - 1u) ) {
				return iPosition;
			}
			if ( arrShift[iLastText] > (iLimit - iPosition) ) {
				break;
			}
			iPosition += arrShift[iLastText];
		}
	}
	return XRT_NPOS;
}



/* 从指定字节位置查找单个字节。 */
XRT_API size_t xrtStrFindByte(xstrview Text, unsigned char iByte, size_t iStart)
{
	cstr sFound;

	if ( !__xrtStrViewValid(Text) || (iStart > Text.Size) ) {
		return XRT_NPOS;
	}
	if ( iStart == Text.Size ) {
		return XRT_NPOS;
	}
	sFound = (cstr)memchr(Text.Data + iStart, iByte, Text.Size - iStart);
	return sFound != NULL ? (size_t)(sFound - Text.Data) : XRT_NPOS;
}



/* 从指定字节位置查找属于集合的首个字节。 */
XRT_API size_t xrtStrFindAny(xstrview Text, xstrview Set, size_t iStart)
{
	unsigned char arrMember[256];

	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Set) ||
		 (iStart > Text.Size) ) {
		return XRT_NPOS;
	}
	if ( Set.Size == 0 ) {
		return XRT_NPOS;
	}
	if ( Set.Size == 1 ) {
		return xrtStrFindByte(Text, (unsigned char)Set.Data[0], iStart);
	}
	memset(arrMember, 0, sizeof(arrMember));
	for ( size_t i = 0; i < Set.Size; i++ ) {
		arrMember[(unsigned char)Set.Data[i]] = 1;
	}
	for ( size_t i = iStart; i < Text.Size; i++ ) {
		if ( arrMember[(unsigned char)Text.Data[i]] != 0 ) {
			return i;
		}
	}
	return XRT_NPOS;
}



/* 从右侧查找最后一个子串，未找到返回 XRT_NPOS。 */
XRT_API size_t xrtStrRFind(xstrview Text, xstrview Part)
{
	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Part) ) {
		return XRT_NPOS;
	}
	if ( Part.Size == 0 ) {
		return Text.Size;
	}
	if ( Part.Size > Text.Size ) {
		return XRT_NPOS;
	}
	for ( size_t i = (Text.Size - Part.Size) + 1u; i > 0; i-- ) {
		size_t iPosition = i - 1u;

		if ( memcmp(Text.Data + iPosition, Part.Data, Part.Size) == 0 ) {
			return iPosition;
		}
	}
	return XRT_NPOS;
}



/* 按 ASCII 大小写不敏感规则从右侧查找最后一个子串。 */
XRT_API size_t xrtStrCaseRFind(xstrview Text, xstrview Part)
{
	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Part) ) {
		return XRT_NPOS;
	}
	if ( Part.Size == 0 ) {
		return Text.Size;
	}
	if ( Part.Size > Text.Size ) {
		return XRT_NPOS;
	}
	for ( size_t i = (Text.Size - Part.Size) + 1u; i > 0; i-- ) {
		size_t iPosition = i - 1u;

		if ( __xrtStrCaseBytesEqual(Text.Data + iPosition, Part.Data, Part.Size) ) {
			return iPosition;
		}
	}
	return XRT_NPOS;
}



/* 统计不重叠子串数量，空子串返回零。 */
XRT_API size_t xrtStrCount(xstrview Text, xstrview Part)
{
	size_t iCount = 0;
	size_t iPosition = 0;

	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Part) || (Part.Size == 0) ) {
		return 0;
	}
	while ( iPosition <= Text.Size ) {
		iPosition = xrtStrFind(Text, Part, iPosition);
		if ( iPosition == XRT_NPOS ) {
			break;
		}
		iCount++;
		iPosition += Part.Size;
	}
	return iCount;
}



/* 按 ASCII 大小写不敏感规则统计不重叠子串数量。 */
XRT_API size_t xrtStrCaseCount(xstrview Text, xstrview Part)
{
	size_t iCount = 0;
	size_t iPosition = 0;

	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Part) || (Part.Size == 0) ) {
		return 0;
	}
	while ( iPosition <= Text.Size ) {
		iPosition = xrtStrCaseFind(Text, Part, iPosition);
		if ( iPosition == XRT_NPOS ) {
			break;
		}
		iCount++;
		iPosition += Part.Size;
	}
	return iCount;
}



/* 判断字符串是否包含指定子串。 */
XRT_API bool xrtStrContains(xstrview Text, xstrview Part)
{
	return xrtStrFind(Text, Part, 0) != XRT_NPOS;
}



/* 按 ASCII 大小写不敏感规则判断是否包含子串。 */
XRT_API bool xrtStrCaseContains(xstrview Text, xstrview Part)
{
	return xrtStrCaseFind(Text, Part, 0) != XRT_NPOS;
}



/* 判断字符串是否包含集合中的任意字节。 */
XRT_API bool xrtStrContainsAny(xstrview Text, xstrview Set)
{
	return xrtStrFindAny(Text, Set, 0) != XRT_NPOS;
}



/* 判断字符串是否以指定子串开始。 */
XRT_API bool xrtStrStarts(xstrview Text, xstrview Part)
{
	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Part) || (Part.Size > Text.Size) ) {
		return false;
	}
	return (Part.Size == 0) || (memcmp(Text.Data, Part.Data, Part.Size) == 0);
}



/* 按 ASCII 大小写不敏感规则判断是否以子串开始。 */
XRT_API bool xrtStrCaseStarts(xstrview Text, xstrview Part)
{
	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Part) || (Part.Size > Text.Size) ) {
		return false;
	}
	return __xrtStrCaseBytesEqual(Text.Data, Part.Data, Part.Size);
}



/* 判断字符串是否以指定子串结束。 */
XRT_API bool xrtStrEnds(xstrview Text, xstrview Part)
{
	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Part) || (Part.Size > Text.Size) ) {
		return false;
	}
	return (Part.Size == 0) ||
		(memcmp(Text.Data + Text.Size - Part.Size, Part.Data, Part.Size) == 0);
}



/* 按 ASCII 大小写不敏感规则判断是否以子串结束。 */
XRT_API bool xrtStrCaseEnds(xstrview Text, xstrview Part)
{
	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Part) || (Part.Size > Text.Size) ) {
		return false;
	}
	if ( Part.Size == 0 ) {
		return true;
	}
	return __xrtStrCaseBytesEqual(Text.Data + Text.Size - Part.Size,
		Part.Data, Part.Size);
}



/* 围绕首个分隔符切分借用视图，未找到时 Before 返回完整输入。 */
XRT_API bool xrtStrCut(xstrview Text, xstrview Separator,
	xstrview* pBefore, xstrview* pAfter)
{
	size_t iFound;

	if ( pBefore != NULL ) {
		*pBefore = xrtStrViewN(NULL, 0);
	}
	if ( pAfter != NULL ) {
		*pAfter = xrtStrViewN(NULL, 0);
	}
	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Separator) ) {
		return false;
	}
	iFound = xrtStrFind(Text, Separator, 0);
	if ( iFound == XRT_NPOS ) {
		if ( pBefore != NULL ) {
			*pBefore = Text;
		}
		return false;
	}
	if ( pBefore != NULL ) {
		*pBefore = xrtStrSlice(Text, 0, iFound);
	}
	if ( pAfter != NULL ) {
		*pAfter = xrtStrSlice(Text, iFound + Separator.Size, XRT_NPOS);
	}
	return true;
}



/* 围绕最后一个分隔符切分借用视图，未找到时 Before 返回完整输入。 */
XRT_API bool xrtStrRCut(xstrview Text, xstrview Separator,
	xstrview* pBefore, xstrview* pAfter)
{
	size_t iFound;

	if ( pBefore != NULL ) {
		*pBefore = xrtStrViewN(NULL, 0);
	}
	if ( pAfter != NULL ) {
		*pAfter = xrtStrViewN(NULL, 0);
	}
	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Separator) ) {
		return false;
	}
	iFound = xrtStrRFind(Text, Separator);
	if ( iFound == XRT_NPOS ) {
		if ( pBefore != NULL ) {
			*pBefore = Text;
		}
		return false;
	}
	if ( pBefore != NULL ) {
		*pBefore = xrtStrSlice(Text, 0, iFound);
	}
	if ( pAfter != NULL ) {
		*pAfter = xrtStrSlice(Text, iFound + Separator.Size, XRT_NPOS);
	}
	return true;
}



/* 删除匹配的前缀并返回剩余借用视图。 */
XRT_API bool xrtStrCutPrefix(xstrview Text, xstrview Prefix, xstrview* pRest)
{
	if ( pRest != NULL ) {
		*pRest = xrtStrViewN(NULL, 0);
	}
	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Prefix) ) {
		return false;
	}
	if ( !xrtStrStarts(Text, Prefix) ) {
		if ( pRest != NULL ) {
			*pRest = Text;
		}
		return false;
	}
	if ( pRest != NULL ) {
		*pRest = xrtStrSlice(Text, Prefix.Size, XRT_NPOS);
	}
	return true;
}



/* 删除匹配的后缀并返回剩余借用视图。 */
XRT_API bool xrtStrCutSuffix(xstrview Text, xstrview Suffix, xstrview* pRest)
{
	if ( pRest != NULL ) {
		*pRest = xrtStrViewN(NULL, 0);
	}
	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Suffix) ) {
		return false;
	}
	if ( !xrtStrEnds(Text, Suffix) ) {
		if ( pRest != NULL ) {
			*pRest = Text;
		}
		return false;
	}
	if ( pRest != NULL ) {
		*pRest = xrtStrSlice(Text, 0, Text.Size - Suffix.Size);
	}
	return true;
}



/* 按字节截取借用视图，范围会钳制到源字符串。 */
XRT_API xstrview xrtStrSlice(xstrview Text, size_t iStart, size_t iCount)
{
	xstrview Result;

	if ( !__xrtStrViewValid(Text) ) {
		Result.Data = NULL;
		Result.Size = 0;
		return Result;
	}
	if ( iStart > Text.Size ) {
		iStart = Text.Size;
	}
	if ( iCount > (Text.Size - iStart) ) {
		iCount = Text.Size - iStart;
	}
	Result.Data = Text.Data != NULL ? Text.Data + iStart : NULL;
	Result.Size = iCount;
	return Result;
}



/* 删除左侧 ASCII 空白并返回借用视图。 */
XRT_API xstrview xrtStrTrimLeft(xstrview Text)
{
	if ( !__xrtStrViewValid(Text) ) {
		return xrtStrViewN(NULL, 0);
	}
	while ( (Text.Size != 0) && __xrtStrAsciiSpace((unsigned char)Text.Data[0]) ) {
		Text.Data++;
		Text.Size--;
	}
	return Text;
}



/* 删除右侧 ASCII 空白并返回借用视图。 */
XRT_API xstrview xrtStrTrimRight(xstrview Text)
{
	if ( !__xrtStrViewValid(Text) ) {
		return xrtStrViewN(NULL, 0);
	}
	while ( (Text.Size != 0) && __xrtStrAsciiSpace((unsigned char)Text.Data[Text.Size - 1u]) ) {
		Text.Size--;
	}
	return Text;
}



/* 删除两侧 ASCII 空白并返回借用视图。 */
XRT_API xstrview xrtStrTrim(xstrview Text)
{
	return xrtStrTrimRight(xrtStrTrimLeft(Text));
}



/* 删除左侧属于指定字节集合的内容。 */
XRT_API xstrview xrtStrTrimLeftSet(xstrview Text, xstrview Set)
{
	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Set) ) {
		return xrtStrViewN(NULL, 0);
	}
	while ( (Text.Size != 0) && __xrtStrByteInSet((unsigned char)Text.Data[0], Set) ) {
		Text.Data++;
		Text.Size--;
	}
	return Text;
}



/* 删除右侧属于指定字节集合的内容。 */
XRT_API xstrview xrtStrTrimRightSet(xstrview Text, xstrview Set)
{
	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Set) ) {
		return xrtStrViewN(NULL, 0);
	}
	while ( (Text.Size != 0) && __xrtStrByteInSet((unsigned char)Text.Data[Text.Size - 1u], Set) ) {
		Text.Size--;
	}
	return Text;
}



/* 删除两侧属于指定字节集合的内容。 */
XRT_API xstrview xrtStrTrimSet(xstrview Text, xstrview Set)
{
	return xrtStrTrimRightSet(xrtStrTrimLeftSet(Text, Set), Set);
}



/* 判断字符串是否只包含 ASCII 空白。 */
XRT_API bool xrtStrBlank(xstrview Text)
{
	if ( !__xrtStrViewValid(Text) ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( !__xrtStrAsciiSpace((unsigned char)Text.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 复制零结尾字符串，返回值始终由 xrtFree 释放。 */
XRT_API str xrtStrDup(cstr sText)
{
	return __xrtStrCopyView(xrtStrView(sText));
}



/* 复制明确长度字符串并追加零结尾。 */
XRT_API str xrtStrDupN(cstr sText, size_t iSize)
{
	xstrview Text = xrtStrViewN(sText, iSize);

	if ( !__xrtStrViewValid(Text) ) {
		return NULL;
	}
	return __xrtStrCopyView(Text);
}



/* 复制字符串视图并追加零结尾。 */
XRT_API str xrtStrDupView(xstrview Text)
{
	if ( !__xrtStrViewValid(Text) ) {
		return NULL;
	}
	return __xrtStrCopyView(Text);
}



/* 连接两个字符串视图。 */
XRT_API str xrtStrConcat(xstrview Left, xstrview Right)
{
	size_t iSize;
	str sResult;

	if ( !__xrtStrViewValid(Left) || !__xrtStrViewValid(Right) ) {
		return NULL;
	}
	if ( Left.Size > (SIZE_MAX - Right.Size) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iSize = Left.Size + Right.Size;
	if ( iSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sResult = (str)xrtMalloc(iSize + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	if ( Left.Size != 0 ) {
		memcpy(sResult, Left.Data, Left.Size);
	}
	if ( Right.Size != 0 ) {
		memcpy(sResult + Left.Size, Right.Data, Right.Size);
	}
	sResult[iSize] = 0;
	return sResult;
}



/* 使用分隔符连接一组字符串视图。 */
XRT_API str xrtStrJoin(xstrview Separator, const xstrview* arrText, size_t iCount)
{
	size_t iSize = 0;
	size_t iPosition = 0;
	str sResult;

	if ( !__xrtStrViewValid(Separator) || ((arrText == NULL) && (iCount != 0)) ) {
		if ( (arrText == NULL) && (iCount != 0) ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !__xrtStrViewValid(arrText[i]) || (arrText[i].Size > (SIZE_MAX - iSize)) ) {
			if ( arrText[i].Size > (SIZE_MAX - iSize) ) {
				__xrtErrorSetSizeOverflow();
			}
			return NULL;
		}
		iSize += arrText[i].Size;
		if ( (i != 0) && (Separator.Size > (SIZE_MAX - iSize)) ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		if ( i != 0 ) {
			iSize += Separator.Size;
		}
	}
	if ( iSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sResult = (str)xrtMalloc(iSize + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( (i != 0) && (Separator.Size != 0) ) {
			memcpy(sResult + iPosition, Separator.Data, Separator.Size);
			iPosition += Separator.Size;
		}
		if ( arrText[i].Size != 0 ) {
			memcpy(sResult + iPosition, arrText[i].Data, arrText[i].Size);
			iPosition += arrText[i].Size;
		}
	}
	sResult[iPosition] = 0;
	return sResult;
}



/* 重复字符串指定次数。 */
XRT_API str xrtStrRepeat(xstrview Text, size_t iCount)
{
	size_t iSize;
	size_t iWritten;
	str sResult;

	if ( !__xrtStrViewValid(Text) ) {
		return NULL;
	}
	if ( (Text.Size != 0) && (iCount > (SIZE_MAX / Text.Size)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iSize = Text.Size * iCount;
	if ( iSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sResult = (str)xrtMalloc(iSize + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	iWritten = Text.Size;
	if ( iWritten != 0 ) {
		memcpy(sResult, Text.Data, iWritten);
	}
	while ( iWritten < iSize ) {
		size_t iTake = iWritten < (iSize - iWritten) ?
			iWritten : iSize - iWritten;

		memcpy(sResult + iWritten, sResult, iTake);
		iWritten += iTake;
	}
	sResult[iSize] = 0;
	return sResult;
}



/* 替换所有不重叠子串。 */
XRT_API str xrtStrReplace(xstrview Text, xstrview Part, xstrview Replacement)
{
	size_t iMatchCount;
	size_t iResultSize;
	size_t iRead = 0;
	size_t iWrite = 0;
	str sResult;

	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Part) ||
		 !__xrtStrViewValid(Replacement) ) {
		return NULL;
	}
	if ( Part.Size == 0 ) {
		return __xrtStrCopyView(Text);
	}
	iMatchCount = xrtStrCount(Text, Part);
	iResultSize = Text.Size;
	if ( Replacement.Size >= Part.Size ) {
		size_t iGrowth = Replacement.Size - Part.Size;

		if ( (iGrowth != 0) && (iMatchCount > ((SIZE_MAX - iResultSize) / iGrowth)) ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iResultSize += iMatchCount * iGrowth;
	} else {
		iResultSize -= iMatchCount * (Part.Size - Replacement.Size);
	}
	if ( iResultSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sResult = (str)xrtMalloc(iResultSize + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	while ( iRead < Text.Size ) {
		size_t iFound = xrtStrFind(Text, Part, iRead);

		if ( iFound == XRT_NPOS ) {
			size_t iTail = Text.Size - iRead;

			if ( iTail != 0 ) {
				memcpy(sResult + iWrite, Text.Data + iRead, iTail);
				iWrite += iTail;
			}
			break;
		}
		if ( iFound != iRead ) {
			size_t iPrefix = iFound - iRead;

			memcpy(sResult + iWrite, Text.Data + iRead, iPrefix);
			iWrite += iPrefix;
		}
		if ( Replacement.Size != 0 ) {
			memcpy(sResult + iWrite, Replacement.Data, Replacement.Size);
			iWrite += Replacement.Size;
		}
		iRead = iFound + Part.Size;
	}
	sResult[iWrite] = 0;
	return sResult;
}



/* 按字节位置插入子串。 */
XRT_API str xrtStrInsert(xstrview Text, size_t iPosition, xstrview Part)
{
	size_t iSize;
	str sResult;

	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Part) ) {
		return NULL;
	}
	if ( iPosition > Text.Size ) {
		iPosition = Text.Size;
	}
	if ( Part.Size > (SIZE_MAX - Text.Size) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iSize = Text.Size + Part.Size;
	if ( iSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sResult = (str)xrtMalloc(iSize + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	if ( iPosition != 0 ) {
		memcpy(sResult, Text.Data, iPosition);
	}
	if ( Part.Size != 0 ) {
		memcpy(sResult + iPosition, Part.Data, Part.Size);
	}
	if ( iPosition != Text.Size ) {
		memcpy(sResult + iPosition + Part.Size, Text.Data + iPosition, Text.Size - iPosition);
	}
	sResult[iSize] = 0;
	return sResult;
}



/* 按字节范围删除内容。 */
XRT_API str xrtStrRemove(xstrview Text, size_t iPosition, size_t iCount)
{
	size_t iEnd;
	size_t iSize;
	str sResult;

	if ( !__xrtStrViewValid(Text) ) {
		return NULL;
	}
	if ( iPosition > Text.Size ) {
		iPosition = Text.Size;
	}
	if ( iCount > (Text.Size - iPosition) ) {
		iCount = Text.Size - iPosition;
	}
	iEnd = iPosition + iCount;
	iSize = Text.Size - iCount;
	sResult = (str)xrtMalloc(iSize + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	if ( iPosition != 0 ) {
		memcpy(sResult, Text.Data, iPosition);
	}
	if ( iEnd != Text.Size ) {
		memcpy(sResult + iPosition, Text.Data + iEnd, Text.Size - iEnd);
	}
	sResult[iSize] = 0;
	return sResult;
}



/* 按字节反转到调用方缓冲区并补零；允许输入和输出起点相同。 */
XRT_API bool xrtStrReverseBytesTo(xstrview Text, char* sOutput, size_t iCapacity)
{
	if ( !__xrtStrTransformTargetValid(Text, sOutput, iCapacity) ) {
		return false;
	}
	if ( (const void*)sOutput != (const void*)Text.Data ) {
		if ( Text.Size != 0 ) {
			memcpy(sOutput, Text.Data, Text.Size);
		}
	}
	__xrtStrReverseRange(sOutput, Text.Size);
	sOutput[Text.Size] = 0;
	return true;
}



/* 按字节反转字符串。 */
XRT_API str xrtStrReverseBytes(xstrview Text)
{
	str sResult;

	if ( !__xrtStrViewValid(Text) ) {
		return NULL;
	}
	sResult = __xrtStrCopyView(Text);
	if ( sResult == NULL ) {
		return NULL;
	}
	__xrtStrReverseRange(sResult, Text.Size);
	return sResult;
}



/* 把 ASCII 字母转换为小写并写入调用方缓冲区；允许原地转换。 */
XRT_API bool xrtStrLowerTo(xstrview Text, char* sOutput, size_t iCapacity)
{
	if ( !__xrtStrTransformTargetValid(Text, sOutput, iCapacity) ) {
		return false;
	}
	__xrtStrLowerBody(Text, sOutput);
	return true;
}



/* 复制字符串并把 ASCII 字母转换为小写。 */
XRT_API str xrtStrLower(xstrview Text)
{
	str sResult;

	if ( !__xrtStrViewValid(Text) ) {
		return NULL;
	}
	sResult = __xrtStrCopyView(Text);
	if ( sResult == NULL ) {
		return NULL;
	}
	__xrtStrLowerBody(Text, sResult);
	return sResult;
}



/* 把 ASCII 字母转换为大写并写入调用方缓冲区；允许原地转换。 */
XRT_API bool xrtStrUpperTo(xstrview Text, char* sOutput, size_t iCapacity)
{
	if ( !__xrtStrTransformTargetValid(Text, sOutput, iCapacity) ) {
		return false;
	}
	__xrtStrUpperBody(Text, sOutput);
	return true;
}



/* 复制字符串并把 ASCII 字母转换为大写。 */
XRT_API str xrtStrUpper(xstrview Text)
{
	str sResult;

	if ( !__xrtStrViewValid(Text) ) {
		return NULL;
	}
	sResult = __xrtStrCopyView(Text);
	if ( sResult == NULL ) {
		return NULL;
	}
	__xrtStrUpperBody(Text, sResult);
	return sResult;
}



/* 删除集合中的全部字节并写入调用方缓冲区。 */
XRT_API bool xrtStrFilterTo(xstrview Text, xstrview Set,
	char* sOutput, size_t iCapacity, size_t* pOutputSize)
{
	uint64 arrRemove[4];
	size_t iRequired;

	if ( (pOutputSize == NULL) ||
		 ((sOutput == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtStrFilterMeasure(Text, Set, arrRemove, &iRequired) ) {
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
	if ( __xrtRangesOverlap(sOutput, iRequired + 1u,
			Text.Data, Text.Size) &&
		 ((const void*)sOutput != (const void*)Text.Data) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtStrFilterBody(Text, arrRemove, sOutput);
	*pOutputSize = iRequired;
	return true;
}



/* 删除集合中的全部字节并创建独立字符串。 */
XRT_API str xrtStrFilter(xstrview Text, xstrview Set)
{
	uint64 arrRemove[4];
	size_t iSize;
	str sResult;

	if ( !__xrtStrFilterMeasure(Text, Set, arrRemove, &iSize) ) {
		return NULL;
	}
	sResult = (str)xrtMalloc(iSize + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	__xrtStrFilterBody(Text, arrRemove, sResult);
	return sResult;
}



/* 按字节宽度在左侧重复填充字符串。 */
XRT_API str xrtStrPadLeft(xstrview Text, size_t iWidth, xstrview Fill)
{
	return __xrtStrPad(Text, iWidth, Fill, -1);
}



/* 按字节宽度在右侧重复填充字符串。 */
XRT_API str xrtStrPadRight(xstrview Text, size_t iWidth, xstrview Fill)
{
	return __xrtStrPad(Text, iWidth, Fill, 1);
}



/* 按字节宽度在两侧重复填充字符串。 */
XRT_API str xrtStrPadCenter(xstrview Text, size_t iWidth, xstrview Fill)
{
	return __xrtStrPad(Text, iWidth, Fill, 0);
}

#endif
