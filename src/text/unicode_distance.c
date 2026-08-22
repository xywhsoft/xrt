#include "../internal/xrt_charset.h"



#if defined(XRT_FEATURE_UNICODE_DISTANCE)

/* 有限阈值使用饱和值，避免无意义的距离继续增长。 */
static size_t __xrtUtfDistanceAdd(size_t iValue, size_t iInfinite)
{
	return iValue < iInfinite ? iValue + 1u : iInfinite;
}



/* 把已经严格校验的 UTF-8 视图解码到标量数组。 */
static void __xrtUtfDistanceDecode(xstrview Text, uint32* pScalars)
{
	size_t iPosition = 0;
	size_t iIndex = 0;

	while ( iPosition < Text.Size ) {
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iPosition, Text.Size - iPosition);

		pScalars[iIndex++] = Decode.Scalar;
		iPosition += Decode.Read;
	}
}



/* 在一个 DP 行和一个短串标量数组上计算编辑距离。 */
static size_t __xrtUtfDistanceRun(xstrview LongText, size_t iLongCount,
	const uint32* pShort, size_t iShortCount, size_t iLimit, size_t* pRow)
{
	size_t iInfinite = iLimit == XRT_NPOS ? XRT_NPOS : iLimit + 1u;
	size_t iLongPosition = 0;

	for ( size_t j = 0; j <= iShortCount; j++ ) {
		pRow[j] = (iLimit == XRT_NPOS) || (j <= iLimit) ? j : iInfinite;
	}

	/* 有限阈值只更新主对角线附近的带状区域。 */
	for ( size_t i = 1; i <= iLongCount; i++ ) {
		xrt_utf_decode LongScalar = __xrtUtf8Decode(
			(const unsigned char*)LongText.Data + iLongPosition,
			LongText.Size - iLongPosition);
		size_t iBegin = 1;
		size_t iEnd = iShortCount;
		size_t iDiagonal;

		iLongPosition += LongScalar.Read;
		if ( iLimit != XRT_NPOS ) {
			iBegin = i > iLimit ? i - iLimit : 1u;
			if ( i < iShortCount ) {
				iEnd = iLimit >= (iShortCount - i) ?
					iShortCount : i + iLimit;
			}
		}
		iDiagonal = pRow[iBegin - 1u];
		if ( iBegin == 1u ) {
			pRow[0] = (iLimit == XRT_NPOS) || (i <= iLimit) ? i : iInfinite;
		} else {
			pRow[iBegin - 1u] = iInfinite;
		}
		for ( size_t j = iBegin; j <= iEnd; j++ ) {
			size_t iAbove = pRow[j];
			size_t iDelete = __xrtUtfDistanceAdd(iAbove, iInfinite);
			size_t iInsert = __xrtUtfDistanceAdd(pRow[j - 1u], iInfinite);
			size_t iReplace = iDiagonal;
			size_t iValue;

			if ( LongScalar.Scalar != pShort[j - 1u] ) {
				iReplace = __xrtUtfDistanceAdd(iReplace, iInfinite);
			}
			iValue = iDelete < iInsert ? iDelete : iInsert;
			if ( iReplace < iValue ) {
				iValue = iReplace;
			}
			pRow[j] = iValue;
			iDiagonal = iAbove;
		}
		if ( iEnd < iShortCount ) {
			pRow[iEnd + 1u] = iInfinite;
		}
	}
	return pRow[iShortCount] <= iLimit || iLimit == XRT_NPOS ?
		pRow[iShortCount] : XRT_NPOS;
}



/* 在一次遍历中严格校验 UTF-8 并统计标量数。 */
static bool __xrtUtfDistanceCount(xstrview Text, cstr sOperation,
	size_t* pCount)
{
	size_t iPosition = 0;
	size_t iCount = 0;

	if ( (Text.Data == NULL) && (Text.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	while ( iPosition < Text.Size ) {
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iPosition,
			Text.Size - iPosition);

		if ( Decode.Status != XUTF_OK ) {
			__xrtUtfSetInvalid(sOperation, iPosition);
			return false;
		}
		iPosition += Decode.Read;
		iCount++;
	}
	*pCount = iCount;
	return true;
}



/* 计算 UTF-8 编辑距离并返回归一化需要的最大标量数。 */
static size_t __xrtUtfDistanceMeasure(xstrview Left, xstrview Right,
	size_t iLimit, size_t* pMaximum)
{
	xstrview LongText;
	xstrview ShortText;
	size_t iLeftCount;
	size_t iRightCount;
	size_t iLongCount;
	size_t iShortCount;
	size_t iRowBytes;
	size_t iScalarBytes;
	size_t iTotal;
	size_t* pRow;
	uint32* pShort;
	size_t iDistance;

	*pMaximum = 0;
	if ( !__xrtUtfDistanceCount(Left, "utf8-distance-left", &iLeftCount) ||
		 !__xrtUtfDistanceCount(Right, "utf8-distance-right", &iRightCount) ) {
		return XRT_NPOS;
	}
	LongText = iLeftCount >= iRightCount ? Left : Right;
	ShortText = iLeftCount >= iRightCount ? Right : Left;
	iLongCount = iLeftCount >= iRightCount ? iLeftCount : iRightCount;
	iShortCount = iLeftCount >= iRightCount ? iRightCount : iLeftCount;
	*pMaximum = iLongCount;

	/* 长度差已经超过阈值时不需要分配或进入动态规划。 */
	if ( (iLimit != XRT_NPOS) && ((iLongCount - iShortCount) > iLimit) ) {
		return XRT_NPOS;
	}
	if ( iShortCount == 0 ) {
		return iLongCount;
	}
	if ( iShortCount == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return XRT_NPOS;
	}
	if ( (iShortCount + 1u) > (SIZE_MAX / sizeof(size_t)) ) {
		__xrtErrorSetSizeOverflow();
		return XRT_NPOS;
	}
	iRowBytes = (iShortCount + 1u) * sizeof(size_t);
	if ( iShortCount > ((SIZE_MAX - iRowBytes) / sizeof(uint32)) ) {
		__xrtErrorSetSizeOverflow();
		return XRT_NPOS;
	}
	iScalarBytes = iShortCount * sizeof(uint32);
	iTotal = iRowBytes + iScalarBytes;
	pRow = (size_t*)xrtMalloc(iTotal);
	if ( pRow == NULL ) {
		return XRT_NPOS;
	}
	pShort = (uint32*)((unsigned char*)pRow + iRowBytes);
	__xrtUtfDistanceDecode(ShortText, pShort);
	iDistance = __xrtUtfDistanceRun(LongText, iLongCount, pShort,
		iShortCount, iLimit, pRow);
	xrtFree(pRow);
	return iDistance;
}



/* 按 Unicode 标量计算 UTF-8 编辑距离。 */
XRT_API size_t xrtUtf8Distance(xstrview Left, xstrview Right, size_t iLimit)
{
	size_t iMaximum;

	return __xrtUtfDistanceMeasure(Left, Right, iLimit, &iMaximum);
}



/* 按 Unicode 标量返回归一化 UTF-8 相似度。 */
XRT_API double xrtUtf8Similarity(xstrview Left, xstrview Right)
{
	size_t iMaximum;
	size_t iDistance = __xrtUtfDistanceMeasure(Left, Right,
		XRT_NPOS, &iMaximum);

	if ( iDistance == XRT_NPOS ) {
		return -1.0;
	}
	if ( iMaximum == 0 ) {
		return 1.0;
	}
	return 1.0 - ((double)iDistance / (double)iMaximum);
}

#endif
