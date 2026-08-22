#include "../internal/xrt_http.h"

#include <xrt/codec.h>
#include <xrt/http_proxy_status.h>



#if defined(XHTTP_FEATURE_HTTP_PROXY_ALIAS_WRITE)

/* 验证解码后的 RFC 9532 展示形式反斜杠。 */
static bool __xrtHttpProxyAliasPresentationValid(xstrview Alias)
{
	size_t i;

	if ( Alias.Size == 0 ) {
		return false;
	}
	for ( i = 0; i < Alias.Size; i++ ) {
		if ( Alias.Data[i] != '\\' ) {
			continue;
		}
		if ( (++i == Alias.Size) ||
			((Alias.Data[i] != '.') &&
			 (Alias.Data[i] != '\\')) ) {
			return false;
		}
	}
	return true;
}



/* 验证名称数组并计算规范编码长度。 */
static bool __xrtHttpProxyAliasesMeasure(
	const xstrview* pAliases,
	size_t iCount,
	size_t* pRequired
)
{
	xstrview Alias;
	size_t iEncoded;
	size_t iBytes;
	size_t iRequired = 0;
	size_t i;

	if ( iCount > (SIZE_MAX / sizeof(Alias)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iBytes = iCount * sizeof(Alias);
	if ( !__xrtRangeValid(pAliases, iBytes) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(
			&Alias, (const uint8*)pAliases +
			(i * sizeof(Alias)), sizeof(Alias)
		);
		if ( !__xrtHttpViewValid(Alias) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		if ( !__xrtHttpProxyAliasPresentationValid(Alias) ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( !xrtPercentWrite(
			Alias.Data, Alias.Size,
			XRT_STR_LITERAL(""),
			NULL, 0, &iEncoded
		) ) {
			return false;
		}
		if ( ((i != 0) && (iRequired == SIZE_MAX)) ||
			(iEncoded > (SIZE_MAX - iRequired -
			 ((i != 0) ? 1u : 0u))) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iRequired += iEncoded + ((i != 0) ? 1u : 0u);
	}
	*pRequired = iRequired;
	return true;
}



/* 判断长度输出是否覆盖名称描述符或借用名称。 */
static bool __xrtHttpProxyAliasesSizeOverlap(
	const xstrview* pAliases,
	size_t iCount,
	const size_t* pSize
)
{
	xstrview Alias;
	size_t iBytes = iCount * sizeof(Alias);
	size_t i;

	if ( __xrtRangesOverlap(
		pSize, sizeof(*pSize), pAliases, iBytes
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(
			&Alias, (const uint8*)pAliases +
			(i * sizeof(Alias)), sizeof(Alias)
		);
		if ( __xrtRangesOverlap(
			pSize, sizeof(*pSize), Alias.Data, Alias.Size
		) ) {
			return true;
		}
	}
	return false;
}



/* 判断写出区域是否覆盖任一仍需读取的名称。 */
static bool __xrtHttpProxyAliasesOutputOverlap(
	const xstrview* pAliases,
	size_t iCount,
	const void* pOutput,
	size_t iSize
)
{
	xstrview Alias;
	size_t iBytes = iCount * sizeof(Alias);
	size_t i;

	if ( __xrtRangesOverlap(
		pOutput, iSize, pAliases, iBytes
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(
			&Alias, (const uint8*)pAliases +
			(i * sizeof(Alias)), sizeof(Alias)
		);
		if ( __xrtRangesOverlap(
			pOutput, iSize, Alias.Data, Alias.Size
		) ) {
			return true;
		}
	}
	return false;
}



/* 在容量和输入已经验证后写出别名列表。 */
static bool __xrtHttpProxyAliasesWriteUnchecked(
	const xstrview* pAliases,
	size_t iCount,
	char* sOutput,
	size_t iCapacity,
	size_t* pWritten
)
{
	xstrview Alias;
	size_t iEncoded;
	size_t iPosition = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		memcpy(
			&Alias, (const uint8*)pAliases +
			(i * sizeof(Alias)), sizeof(Alias)
		);
		if ( i != 0 ) {
			sOutput[iPosition++] = ',';
		}
		if ( !xrtPercentWrite(
			Alias.Data, Alias.Size,
			XRT_STR_LITERAL(""),
			sOutput + iPosition,
			iCapacity - iPosition,
			&iEncoded
		) ) {
			return false;
		}
		iPosition += iEncoded;
	}
	*pWritten = iPosition;
	return true;
}



/* 写出完整别名列表并保持失败原子性。 */
XRT_API bool xrtHttpProxyAliasesWrite(
	const xstrview* pAliases,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iRequired;
	size_t iWritten;

	if ( !__xrtRangeValid(pOutput, iCapacity) ||
		!__xrtRangeValid(pSize, sizeof(*pSize)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpProxyAliasesMeasure(
		pAliases, iCount, &iRequired
	) ) {
		return false;
	}
	if ( __xrtHttpProxyAliasesSizeOverlap(
		pAliases, iCount, pSize
	) || __xrtRangesOverlap(
		pSize, sizeof(*pSize), pOutput, iRequired
	) || ((pOutput != NULL) &&
		__xrtHttpProxyAliasesOutputOverlap(
			pAliases, iCount, pOutput, iRequired
		)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	if ( !__xrtHttpProxyAliasesWriteUnchecked(
		pAliases, iCount, (char*)pOutput,
		iRequired, &iWritten
	) ) {
		return false;
	}
	memcpy(pSize, &iWritten, sizeof(iWritten));
	return true;
}



/* 写出单个别名。 */
XRT_API bool xrtHttpProxyAliasWrite(
	xstrview Alias,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return xrtHttpProxyAliasesWrite(
		&Alias, 1u, pOutput, iCapacity, pSize
	);
}



/* 构建零结尾别名列表。 */
XRT_API str xrtHttpProxyAliasesBuild(
	const xstrview* pAliases,
	size_t iCount,
	size_t* pSize
)
{
	char* sOutput;
	size_t iRequired;
	size_t iWritten;

	if ( (pSize != NULL) &&
		!__xrtRangeValid(pSize, sizeof(*pSize)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpProxyAliasesWrite(
		pAliases, iCount, NULL, 0, &iRequired
	) ) {
		return NULL;
	}
	if ( (pSize != NULL) &&
		__xrtHttpProxyAliasesSizeOverlap(
			pAliases, iCount, pSize
		) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (char*)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpProxyAliasesWrite(
		pAliases, iCount, sOutput, iRequired, &iWritten
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iWritten] = '\0';
	if ( pSize != NULL ) {
		memcpy(pSize, &iWritten, sizeof(iWritten));
	}
	return sOutput;
}

#endif
