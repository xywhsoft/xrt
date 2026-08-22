#include "../internal/xrt_http_static.h"



#if defined(XRT_FEATURE_HTTP_STATIC_PATH)

/* 第一次扫描保留路由匹配与相对路径起点，不持有任何输入。 */
typedef struct xrt_http_static_path_info {
	size_t RelativeOffset;
	bool Matched;
	bool TrailingSlash;
} xrt_http_static_path_info;



/* 内部写入器同时服务精确长度查询和已经验证容量的实际写出。 */
typedef struct xrt_http_static_path_writer {
	char* Data;
	size_t Size;
} xrt_http_static_path_writer;



/* 判断一个完整路径段是否是点或双点。 */
static bool __xrtHttpStaticPathDot(
	size_t iSize,
	uint8 iFirst,
	uint8 iLast
)
{
	return ((iSize == 1u) && (iFirst == (uint8)'.')) ||
		((iSize == 2u) && (iFirst == (uint8)'.') &&
		 (iLast == (uint8)'.'));
}



/* 验证挂载点是已解码、无空段且不含查询或片段的规范 origin 路径。 */
static bool __xrtHttpStaticPathMountValid(
	xstrview Mount
)
{
	size_t iSegmentSize = 0;
	uint8 iFirst = 0;
	uint8 iLast = 0;
	size_t i;

	if ( !__xrtRangeValid(Mount.Data, Mount.Size) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Mount.Data == NULL) || (Mount.Size == 0) ||
		(Mount.Data[0] != '/') ||
		((Mount.Size != 1u) &&
		 (Mount.Data[Mount.Size - 1u] == '/')) ||
		!xrtUtf8Valid(Mount, NULL) ) {
		__xrtErrorSetValue();
		return false;
	}
	for ( i = 1; i < Mount.Size; i++ ) {
		uint8 iValue = (uint8)Mount.Data[i];

		if ( iValue == (uint8)'/' ) {
			if ( (iSegmentSize == 0) ||
				__xrtHttpStaticPathDot(
					iSegmentSize,
					iFirst,
					iLast
				) ) {
				__xrtErrorSetValue();
				return false;
			}
			iSegmentSize = 0;
			continue;
		}
		if ( (iValue < 0x20u) || (iValue == 0x7Fu) ||
			(iValue == (uint8)'\\') ||
			(iValue == (uint8)'?') ||
			(iValue == (uint8)'#') ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( iSegmentSize == 0 ) {
			iFirst = iValue;
		}
		iLast = iValue;
		iSegmentSize++;
	}
	if ( (Mount.Size != 1u) &&
		((iSegmentSize == 0) ||
		 __xrtHttpStaticPathDot(
			iSegmentSize,
			iFirst,
			iLast
		 )) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 解析默认配置并拒绝未知策略位。 */
static bool __xrtHttpStaticPathConfigResolve(
	const xhttpstaticpathconfig* pInput,
	xhttpstaticpathconfig* pConfig
)
{
	*pConfig = (xhttpstaticpathconfig){
		XRT_STR_LITERAL("/"),
		XHTTP_STATIC_PATH_PORTABLE
	};
	if ( pInput != NULL ) {
		if ( !__xrtRangeValid(pInput, sizeof(*pConfig)) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	if ( (pConfig->Flags &
		~((uint32)XHTTP_STATIC_PATH_PORTABLE |
		  (uint32)XHTTP_STATIC_PATH_ALLOW_HIDDEN)) != 0u ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtHttpStaticPathMountValid(pConfig->Mount);
}



/* 完成一个 origin 路径段的空段与点段检查。 */
static bool __xrtHttpStaticPathSegmentFinish(
	size_t iSize,
	uint8 iFirst,
	uint8 iLast
)
{
	if ( (iSize == 0) ||
		__xrtHttpStaticPathDot(iSize, iFirst, iLast) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/*
	严格扫描原始 URL path，验证 percent、UTF-8 和结构，
	同时按解码后的字节匹配挂载点。
*/
static bool __xrtHttpStaticPathInspect(
	xstrview RawPath,
	xstrview Mount,
	xrt_http_static_path_info* pInfo
)
{
	xutf8state Utf8;
	size_t iRaw = 0;
	size_t iDecoded = 0;
	size_t iSegmentSize = 0;
	size_t iRootRelative = 0;
	size_t iBoundaryRelative = 0;
	uint8 iFirst = 0;
	uint8 iLast = 0;
	bool bPrefix = true;
	bool bBoundary = false;

	memset(pInfo, 0, sizeof(*pInfo));
	if ( (RawPath.Data == NULL) || (RawPath.Size == 0) ||
		(RawPath.Data[0] != '/') ) {
		__xrtErrorSetValue();
		return false;
	}
	xrtUtf8StateInit(&Utf8);
	while ( iRaw < RawPath.Size ) {
		size_t iBefore = iRaw;
		uint8 iValue;
		char iText;
		xutfstatus UtfStatus;
		xrt_percent_next Next;

		if ( (RawPath.Data[iBefore] == '?') ||
			(RawPath.Data[iBefore] == '#') ) {
			__xrtErrorSetValue();
			return false;
		}
		Next = __xrtPercentDecodeNext(
			RawPath,
			false,
			&iRaw,
			&iValue
		);
		if ( Next == XRT_PERCENT_NEXT_ERROR ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( Next == XRT_PERCENT_NEXT_END ) {
			break;
		}

		/* percent 编码不得改变路径分段结构。 */
		if ( ((RawPath.Data[iBefore] == '%') &&
			 (iValue == (uint8)'/')) ||
			(iValue == 0) || (iValue < 0x20u) ||
			(iValue == 0x7Fu) || (iValue == (uint8)'\\') ) {
			__xrtErrorSetValue();
			return false;
		}
		iText = (char)iValue;
		UtfStatus = xrtUtf8StateFeed(
			&Utf8,
			(xstrview){ &iText, 1u },
			false
		);
		if ( (UtfStatus != XUTF_OK) &&
			(UtfStatus != XUTF_MORE) ) {
			if ( UtfStatus != XUTF_OVERFLOW ) {
				__xrtErrorSetValue();
			}
			return false;
		}

		/* 挂载点按解码后的 origin 路径逐字节区分大小写匹配。 */
		if ( iDecoded < Mount.Size ) {
			if ( iValue != (uint8)Mount.Data[iDecoded] ) {
				bPrefix = false;
			}
		} else if ( iDecoded == Mount.Size ) {
			bBoundary = iValue == (uint8)'/';
			if ( bBoundary ) {
				iBoundaryRelative = iRaw;
			}
		}
		if ( iDecoded == 0 ) {
			if ( iValue != (uint8)'/' ) {
				__xrtErrorSetValue();
				return false;
			}
			iRootRelative = iRaw;
		} else if ( iValue == (uint8)'/' ) {
			if ( !__xrtHttpStaticPathSegmentFinish(
				iSegmentSize,
				iFirst,
				iLast
			) ) {
				return false;
			}
			iSegmentSize = 0;
		} else {
			if ( iSegmentSize == SIZE_MAX ) {
				__xrtErrorSetSizeOverflow();
				return false;
			}
			if ( iSegmentSize == 0 ) {
				iFirst = iValue;
			}
			iLast = iValue;
			iSegmentSize++;
		}
		iDecoded++;
	}

	if ( (iSegmentSize != 0) &&
		!__xrtHttpStaticPathSegmentFinish(
			iSegmentSize,
			iFirst,
			iLast
		) ) {
		return false;
	}
	if ( xrtUtf8StateFeed(
		&Utf8,
		(xstrview){ NULL, 0 },
		true
	) != XUTF_OK ) {
		__xrtErrorSetValue();
		return false;
	}
	pInfo->TrailingSlash = iSegmentSize == 0;

	/* 根挂载省略唯一的前导斜杠，其余挂载要求精确命中或段边界。 */
	if ( Mount.Size == 1u ) {
		pInfo->Matched = bPrefix;
		pInfo->RelativeOffset = iRootRelative;
	} else if ( bPrefix && (iDecoded == Mount.Size) ) {
		pInfo->Matched = true;
		pInfo->RelativeOffset = RawPath.Size;
	} else if ( bPrefix && (iDecoded > Mount.Size) && bBoundary ) {
		pInfo->Matched = true;
		pInfo->RelativeOffset = iBoundaryRelative;
	}
	return true;
}



/* 向可空的内部写入器追加一个字节并检查长度溢出。 */
static bool __xrtHttpStaticPathAppend(
	xrt_http_static_path_writer* pWriter,
	uint8 iValue
)
{
	if ( pWriter->Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( pWriter->Data != NULL ) {
		pWriter->Data[pWriter->Size] = (char)iValue;
	}
	pWriter->Size++;
	return true;
}



/* 完成映射段的隐藏策略和可移植路径规则。 */
static bool __xrtHttpStaticPathMappedSegmentFinish(
	const xrt_path_safe_segment* pPortable,
	bool bPortable
)
{
	if ( bPortable &&
		!__xrtPathSafeSegmentFinish(pPortable) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/*
	把已经完成协议扫描的相对后缀解码为文件路径。
	该过程既可只计算长度，也可在第二遍直接写出。
*/
static bool __xrtHttpStaticPathMapBody(
	xstrview Relative,
	uint32 iFlags,
	xrt_http_static_path_writer* pWriter
)
{
	xrt_path_safe_segment Portable;
	size_t iRaw = 0;
	size_t iSegmentSize = 0;
	bool bPortable =
		(iFlags & XHTTP_STATIC_PATH_PORTABLE) != 0u;
	bool bAllowHidden =
		(iFlags & XHTTP_STATIC_PATH_ALLOW_HIDDEN) != 0u;
	xrt_percent_next Next;

	if ( Relative.Size == 0 ) {
		return __xrtHttpStaticPathAppend(
			pWriter,
			(uint8)'.'
		);
	}
	__xrtPathSafeSegmentInit(&Portable);
	do {
		uint8 iValue;

		Next = __xrtPercentDecodeNext(
			Relative,
			false,
			&iRaw,
			&iValue
		);
		if ( Next == XRT_PERCENT_NEXT_ERROR ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( Next == XRT_PERCENT_NEXT_END ) {
			break;
		}
		if ( iValue == (uint8)'/' ) {
			if ( !__xrtHttpStaticPathMappedSegmentFinish(
				&Portable,
				bPortable
			) ) {
				return false;
			}
			if ( iRaw == Relative.Size ) {
				iSegmentSize = 0;
				break;
			}
			if ( !__xrtHttpStaticPathAppend(
				pWriter,
				(uint8)'/'
			) ) {
				return false;
			}
			iSegmentSize = 0;
			__xrtPathSafeSegmentInit(&Portable);
			continue;
		}
		if ( (iSegmentSize == 0) &&
			!bAllowHidden &&
			(iValue == (uint8)'.') ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( bPortable &&
			!__xrtPathSafeSegmentFeed(
				&Portable,
				iValue
			) ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( !__xrtHttpStaticPathAppend(
			pWriter,
			iValue
		) ) {
			return false;
		}
		iSegmentSize++;
	} while ( Next == XRT_PERCENT_NEXT_BYTE );

	if ( (iSegmentSize != 0) &&
		!__xrtHttpStaticPathMappedSegmentFinish(
			&Portable,
			bPortable
		) ) {
		return false;
	}
	return true;
}



/* 检查两个标量输出不会覆盖请求路径、配置或彼此。 */
static bool __xrtHttpStaticPathMetadataAliases(
	size_t* pSize,
	bool* pTrailingSlash,
	xstrview RawPath,
	const xhttpstaticpathconfig* pInput,
	xstrview Mount
)
{
	if ( __xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			pTrailingSlash,
			sizeof(*pTrailingSlash)
		) || __xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			RawPath.Data,
			RawPath.Size
		) || __xrtRangesOverlap(
			pTrailingSlash,
			sizeof(*pTrailingSlash),
			RawPath.Data,
			RawPath.Size
		) || __xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			Mount.Data,
			Mount.Size
		) || __xrtRangesOverlap(
			pTrailingSlash,
			sizeof(*pTrailingSlash),
			Mount.Data,
			Mount.Size
		) || ((pInput != NULL) &&
		 (__xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			pInput,
			sizeof(*pInput)
		 ) || __xrtRangesOverlap(
			pTrailingSlash,
			sizeof(*pTrailingSlash),
			pInput,
			sizeof(*pInput)
		 ))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 检查实际文本输出不会覆盖任何借用输入或标量输出。 */
static bool __xrtHttpStaticPathOutputAliases(
	char* sOutput,
	size_t iWriteSize,
	size_t* pSize,
	bool* pTrailingSlash,
	xstrview RawPath,
	const xhttpstaticpathconfig* pInput,
	xstrview Mount
)
{
	if ( __xrtRangesOverlap(
			sOutput,
			iWriteSize,
			pSize,
			sizeof(*pSize)
		) || __xrtRangesOverlap(
			sOutput,
			iWriteSize,
			pTrailingSlash,
			sizeof(*pTrailingSlash)
		) || __xrtRangesOverlap(
			sOutput,
			iWriteSize,
			RawPath.Data,
			RawPath.Size
		) || __xrtRangesOverlap(
			sOutput,
			iWriteSize,
			Mount.Data,
			Mount.Size
		) || ((pInput != NULL) && __xrtRangesOverlap(
			sOutput,
			iWriteSize,
			pInput,
			sizeof(*pInput)
		)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 初始化静态路径映射的安全默认策略。 */
XRT_API void xrtHttpStaticPathConfigInit(
	xhttpstaticpathconfig* pConfig
)
{
	xhttpstaticpathconfig Config = {
		XRT_STR_LITERAL("/"),
		XHTTP_STATIC_PATH_PORTABLE
	};

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 无分配地查询或写出静态资源相对路径。 */
static xhttpstaticpathstatus __xrtHttpStaticPathWrite(
	xstrview RawPath,
	const xhttpstaticpathconfig* pInput,
	const xhttpstaticpathconfig* pConfig,
	char* sOutput,
	size_t iCapacity,
	size_t* pSize,
	bool* pTrailingSlash
)
{
	xrt_http_static_path_info Info;
	xrt_http_static_path_writer Writer;
	xstrview Relative;
	size_t iWriteSize;
	size_t iResultSize;
	bool bResultTrailing;

	if ( !__xrtHttpStaticPathMetadataAliases(
		pSize,
		pTrailingSlash,
		RawPath,
		pInput,
		pConfig->Mount
	) || !__xrtHttpStaticPathInspect(
		RawPath,
		pConfig->Mount,
		&Info
	) ) {
		return XHTTP_STATIC_PATH_ERROR;
	}
	if ( !Info.Matched ) {
		iResultSize = 0;
		bResultTrailing = false;
		memcpy(pSize, &iResultSize, sizeof(iResultSize));
		memcpy(
			pTrailingSlash,
			&bResultTrailing,
			sizeof(bResultTrailing)
		);
		return XHTTP_STATIC_PATH_NO_MATCH;
	}
	Relative = (xstrview){
		RawPath.Data + Info.RelativeOffset,
		RawPath.Size - Info.RelativeOffset
	};
	Writer = (xrt_http_static_path_writer){ NULL, 0 };
	if ( !__xrtHttpStaticPathMapBody(
		Relative,
		pConfig->Flags,
		&Writer
	) || (Writer.Size == SIZE_MAX) ) {
		if ( Writer.Size == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
		}
		return XHTTP_STATIC_PATH_ERROR;
	}
	iWriteSize = Writer.Size + 1u;
	if ( (sOutput != NULL) &&
		!__xrtHttpStaticPathOutputAliases(
			sOutput,
			iWriteSize,
			pSize,
			pTrailingSlash,
			RawPath,
			pInput,
			pConfig->Mount
		) ) {
		return XHTTP_STATIC_PATH_ERROR;
	}
	iResultSize = Writer.Size;
	bResultTrailing = Info.TrailingSlash;
	memcpy(pSize, &iResultSize, sizeof(iResultSize));
	memcpy(
		pTrailingSlash,
		&bResultTrailing,
		sizeof(bResultTrailing)
	);
	if ( sOutput == NULL ) {
		return XHTTP_STATIC_PATH_MATCH;
	}
	if ( iCapacity < iWriteSize ) {
		__xrtErrorSetRange();
		return XHTTP_STATIC_PATH_ERROR;
	}
	Writer.Data = sOutput;
	Writer.Size = 0;
	if ( !__xrtHttpStaticPathMapBody(
		Relative,
		pConfig->Flags,
		&Writer
	) ) {
		return XHTTP_STATIC_PATH_ERROR;
	}
	sOutput[Writer.Size] = 0;
	return XHTTP_STATIC_PATH_MATCH;
}



/* 无分配地查询或写出静态资源相对路径。 */
XRT_API xhttpstaticpathstatus xrtHttpStaticPathWrite(
	xstrview RawPath,
	const xhttpstaticpathconfig* pConfig,
	char* sOutput,
	size_t iCapacity,
	size_t* pSize,
	bool* pTrailingSlash
)
{
	xhttpstaticpathconfig Config;

	if ( !__xrtRangeValid(RawPath.Data, RawPath.Size) ||
		!__xrtRangeValid(pSize, sizeof(*pSize)) ||
		!__xrtRangeValid(
			pTrailingSlash,
			sizeof(*pTrailingSlash)
		) || !__xrtRangeValid(sOutput, iCapacity) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_STATIC_PATH_ERROR;
	}
	if ( !__xrtHttpStaticPathConfigResolve(
		pConfig,
		&Config
	) ) {
		return XHTTP_STATIC_PATH_ERROR;
	}
	return __xrtHttpStaticPathWrite(
		RawPath,
		pConfig,
		&Config,
		sOutput,
		iCapacity,
		pSize,
		pTrailingSlash
	);
}



/* 初始化拥有型静态路径结果。 */
XRT_API void xrtHttpStaticPathInit(
	xhttpstaticpath* pPath
)
{
	xhttpstaticpath Path = { 0 };

	if ( !__xrtRangeValid(pPath, sizeof(Path)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pPath, &Path, sizeof(Path));
}



/* 分配并构建静态资源相对路径。 */
XRT_API bool xrtHttpStaticPathMap(
	xstrview RawPath,
	const xhttpstaticpathconfig* pConfig,
	xhttpstaticpath* pPath
)
{
	xhttpstaticpathconfig Config;
	xhttpstaticpath Current;
	xhttpstaticpath Result = { 0 };
	xhttpstaticpathstatus Status;
	size_t iSize;
	bool bTrailingSlash;
	str sPath;

	if ( !__xrtRangeValid(RawPath.Data, RawPath.Size) ||
		!__xrtRangeValid(pPath, sizeof(Current)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpStaticPathConfigResolve(
		pConfig,
		&Config
	) ) {
		return false;
	}
	memcpy(&Current, pPath, sizeof(Current));
	if ( (Current.Path != NULL) ||
		(Current.Size != 0) ||
		Current.Matched ||
		Current.TrailingSlash ||
		__xrtRangesOverlap(
			pPath,
			sizeof(Current),
			RawPath.Data,
			RawPath.Size
		) || ((pConfig != NULL) &&
		 (__xrtRangesOverlap(
			pPath,
			sizeof(Current),
			pConfig,
			sizeof(*pConfig)
		 ) || __xrtRangesOverlap(
			pPath,
			sizeof(Current),
			Config.Mount.Data,
			Config.Mount.Size
		 ))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Status = __xrtHttpStaticPathWrite(
		RawPath,
		pConfig,
		&Config,
		NULL,
		0,
		&iSize,
		&bTrailingSlash
	);
	if ( Status == XHTTP_STATIC_PATH_ERROR ) {
		return false;
	}
	if ( Status == XHTTP_STATIC_PATH_NO_MATCH ) {
		return true;
	}
	if ( iSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	sPath = (str)xrtMalloc(iSize + 1u);
	if ( sPath == NULL ) {
		return false;
	}
	Status = __xrtHttpStaticPathWrite(
		RawPath,
		pConfig,
		&Config,
		sPath,
		iSize + 1u,
		&iSize,
		&bTrailingSlash
	);
	if ( Status != XHTTP_STATIC_PATH_MATCH ) {
		xrtFree(sPath);
		return false;
	}
	Result.Path = sPath;
	Result.Size = iSize;
	Result.Matched = true;
	Result.TrailingSlash = bTrailingSlash;
	memcpy(pPath, &Result, sizeof(Result));
	return true;
}



/* 释放并清空拥有型静态路径。 */
XRT_API void xrtHttpStaticPathFree(
	xhttpstaticpath* pPath
)
{
	xhttpstaticpath Path;
	xhttpstaticpath Empty = { 0 };

	if ( pPath == NULL ) {
		return;
	}
	if ( !__xrtRangeValid(pPath, sizeof(Path)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(&Path, pPath, sizeof(Path));
	xrtFree(Path.Path);
	memcpy(pPath, &Empty, sizeof(Empty));
}

#endif
