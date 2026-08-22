#include "../internal/xrt_compress.h"
#include "../third_party/miniz/miniz_tinfl.h"

#if defined(XRT_FEATURE_INFLATE)

typedef enum xrt_inflate_gzip_state {
	XRT_INFLATE_GZIP_FIXED = 0,
	XRT_INFLATE_GZIP_EXTRA_LENGTH,
	XRT_INFLATE_GZIP_EXTRA,
	XRT_INFLATE_GZIP_NAME,
	XRT_INFLATE_GZIP_COMMENT,
	XRT_INFLATE_GZIP_HEADER_CRC,
	XRT_INFLATE_GZIP_DEFLATE,
	XRT_INFLATE_GZIP_TRAILER,
	XRT_INFLATE_GZIP_BOUNDARY
} xrt_inflate_gzip_state;



/* 解码器把状态和算法窗口放在同一按需分配中，复位不会再次分配。 */
struct xinflate {
	tinfl_decompressor Codec;
	xinflateconfig Config;
	xinflateformat CodecFormat;
	size_t DictionaryOffset;
	uint64 TotalOutput;
	uint8 Probe[2];
	uint32 ProbeCount;
	xrt_inflate_gzip_state GzipState;
	uint8 GzipFixed[10];
	uint32 GzipFixedCount;
	uint8 GzipFlags;
	uint8 GzipPair[2];
	uint32 GzipPairCount;
	uint32 GzipExtraRemain;
	uint32 GzipHeaderBytes;
	uint32 GzipHeaderCrc;
	uint32 GzipDataCrc;
	uint32 GzipDataSize;
	uint8 GzipTrailer[8];
	uint32 GzipTrailerCount;
	bool CodecReady;
	bool Writing;
	bool Done;
	bool Failed;
	uint8 Dictionary[TINFL_LZ_DICT_SIZE];
};



/* 整块便捷函数使用按需增长且带末尾零字节的输出。 */
typedef struct xrt_inflate_output {
	bytes Data;
	size_t Size;
	size_t Capacity;
} xrt_inflate_output;



/* 建立 Inflate 域的结构化错误。 */
static void __xrtInflateError(
	xerrkind Kind,
	xinflateerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.inflate";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 验证不依赖解码状态的公开配置。 */
bool __xrtInflateConfigValid(
	const xinflateconfig* pConfig,
	cstr sOperation
)
{
	if ( (pConfig->Format < XINFLATE_RAW) ||
		(pConfig->Format > XINFLATE_GZIP) ||
		(pConfig->WindowBits < XINFLATE_WINDOW_MIN) ||
		(pConfig->WindowBits > XINFLATE_WINDOW_MAX) ||
		(pConfig->GzipHeaderLimit < 10) ) {
		__xrtInflateError(
			XERR_VALUE,
			XINFLATE_ERROR_CONFIG,
			sOperation,
			"invalid Inflate format, window or gzip Header limit"
		);
		return false;
	}
	return true;
}



/* 验证调用方提供的 Inflate 配置快照。 */
XRT_API bool xrtInflateConfigValid(const xinflateconfig* pConfig)
{
	xinflateconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Config, pConfig, sizeof(Config));
	return __xrtInflateConfigValid(
		&Config,
		"validate-inflate-config"
	);
}



/* 复位底层 DEFLATE 状态并选择是否解析 zlib 包装。 */
static void __xrtInflateCodecReset(
	xinflate* pInflate,
	xinflateformat Format
)
{
	tinfl_init(&pInflate->Codec);
	(void)tinfl_set_window_bits(
		&pInflate->Codec,
		(int)pInflate->Config.WindowBits
	);
	pInflate->DictionaryOffset = 0;
	pInflate->CodecFormat = Format;
	pInflate->CodecReady = true;
}



/* 为一个新的 gzip member 初始化 Header、CRC 和 trailer 状态。 */
static void __xrtInflateGzipReset(xinflate* pInflate)
{
	pInflate->GzipState = XRT_INFLATE_GZIP_FIXED;
	pInflate->GzipFixedCount = 0;
	pInflate->GzipFlags = 0;
	pInflate->GzipPairCount = 0;
	pInflate->GzipExtraRemain = 0;
	pInflate->GzipHeaderBytes = 0;
	pInflate->GzipHeaderCrc = UINT32_MAX;
	pInflate->GzipDataCrc = UINT32_MAX;
	pInflate->GzipDataSize = 0;
	pInflate->GzipTrailerCount = 0;
	pInflate->CodecReady = false;
}



/* 把一段解码数据计入限额并同步交给输出消费者。 */
static bool __xrtInflateEmit(
	xinflate* pInflate,
	const void* pData,
	size_t iSize,
	xinflateoutputproc pOutput,
	ptr pOutputData
)
{
	xbytesview Data;
	bool bAccepted;

	if ( iSize == 0 ) {
		return true;
	}
	if ( (pInflate->TotalOutput > pInflate->Config.OutputLimit) ||
		((uint64)iSize >
		 (pInflate->Config.OutputLimit - pInflate->TotalOutput)) ) {
		__xrtInflateError(
			XERR_RANGE,
			XINFLATE_ERROR_LIMIT,
			"inflate-data",
			"Inflate output exceeds its configured limit"
		);
		return false;
	}
	if ( pInflate->Config.Format == XINFLATE_GZIP ) {
		pInflate->GzipDataCrc = __xrtCompressCrc32Update(
			pInflate->GzipDataCrc,
			pData,
			iSize
		);
		pInflate->GzipDataSize += (uint32)iSize;
	}
	if ( pOutput != NULL ) {
		Data.Data = (const uint8*)pData;
		Data.Size = iSize;
		xrtClearError();
		bAccepted = pOutput(Data, pOutputData);
		if ( !bAccepted ) {
			if ( xrtGetError() == NULL ) {
				__xrtInflateError(
					XERR_CANCELLED,
					XINFLATE_ERROR_OUTPUT,
					"write-inflate-output",
					"Inflate output callback stopped decoding"
				);
			}
			return false;
		}
	}
	pInflate->TotalOutput += (uint64)iSize;
	return true;
}



/*
	推进一个原始或 zlib DEFLATE 流。
	Consumed 和 Done 只描述当前输入片段，不转移任何输入所有权。
*/
static bool __xrtInflateFeedCodec(
	xinflate* pInflate,
	const uint8* pData,
	size_t iSize,
	bool bHasMoreInput,
	xinflateoutputproc pOutput,
	ptr pOutputData,
	size_t* pConsumed,
	bool* pDone
)
{
	size_t iOffset = 0;

	*pConsumed = 0;
	*pDone = false;
	while ( true ) {
		size_t iInput = iSize - iOffset;
		size_t iOutput =
			TINFL_LZ_DICT_SIZE - pInflate->DictionaryOffset;
		mz_uint32 iFlags =
			pInflate->CodecFormat == XINFLATE_ZLIB ?
				TINFL_FLAG_PARSE_ZLIB_HEADER : 0;
		tinfl_status Status;

		if ( bHasMoreInput ) {
			iFlags |= TINFL_FLAG_HAS_MORE_INPUT;
		}
		Status = tinfl_decompress(
			&pInflate->Codec,
			pData != NULL ? pData + iOffset : NULL,
			&iInput,
			pInflate->Dictionary,
			pInflate->Dictionary +
				pInflate->DictionaryOffset,
			&iOutput,
			iFlags
		);
		iOffset += iInput;
		if ( !__xrtInflateEmit(
			pInflate,
			pInflate->Dictionary +
				pInflate->DictionaryOffset,
			iOutput,
			pOutput,
			pOutputData
		) ) {
			return false;
		}
		pInflate->DictionaryOffset =
			(pInflate->DictionaryOffset + iOutput) &
			(TINFL_LZ_DICT_SIZE - 1u);
		if ( Status == TINFL_STATUS_DONE ) {
			*pConsumed = iOffset;
			*pDone = true;
			return true;
		}
		if ( Status == TINFL_STATUS_HAS_MORE_OUTPUT ) {
			continue;
		}
		if ( Status == TINFL_STATUS_NEEDS_MORE_INPUT ) {
			*pConsumed = iOffset;
			if ( (iOffset == iSize) && bHasMoreInput ) {
				return true;
			}
		}
		__xrtInflateError(
			XERR_PROTOCOL,
			XINFLATE_ERROR_DATA,
			"inflate-data",
			"DEFLATE stream is corrupt or truncated"
		);
		return false;
	}
}



/* 判断两个探测字节能否构成有效 zlib CMF/FLG。 */
static bool __xrtInflateLooksLikeZlib(const uint8 Probe[2])
{
	uint32 iHeader =
		((uint32)Probe[0] << 8u) | (uint32)Probe[1];

	return ((Probe[0] & 15u) == 8u) &&
		((Probe[0] >> 4u) <= 7u) &&
		((iHeader % 31u) == 0);
}



/* 解码原始、zlib 或 HTTP 兼容 deflate 数据。 */
static bool __xrtInflateWriteDeflate(
	xinflate* pInflate,
	const uint8* pData,
	size_t iSize,
	bool bFinal,
	xinflateoutputproc pOutput,
	ptr pOutputData
)
{
	size_t iOffset = 0;
	bool bDone = false;

	if ( (pInflate->Config.Format == XINFLATE_DEFLATE) &&
		!pInflate->CodecReady ) {
		while ( (pInflate->ProbeCount < 2) &&
			(iOffset < iSize) ) {
			pInflate->Probe[pInflate->ProbeCount] =
				pData[iOffset];
			pInflate->ProbeCount++;
			iOffset++;
		}
		if ( pInflate->ProbeCount < 2 ) {
			if ( bFinal ) {
				__xrtInflateError(
					XERR_PROTOCOL,
					XINFLATE_ERROR_DATA,
					"inflate-data",
					"deflate stream is truncated"
				);
				return false;
			}
			return true;
		}
		__xrtInflateCodecReset(
			pInflate,
			__xrtInflateLooksLikeZlib(pInflate->Probe) ?
				XINFLATE_ZLIB : XINFLATE_RAW
		);
		{
			size_t iConsumed;

			if ( !__xrtInflateFeedCodec(
				pInflate,
				pInflate->Probe,
				2,
				!bFinal || (iOffset < iSize),
				pOutput,
				pOutputData,
				&iConsumed,
				&bDone
			) ) {
				return false;
			}
			if ( (iConsumed != 2) ||
				(bDone && (iOffset != iSize)) ) {
				__xrtInflateError(
					XERR_PROTOCOL,
					XINFLATE_ERROR_DATA,
					"inflate-data",
					"deflate stream contains trailing data"
				);
				return false;
			}
			if ( bDone ) {
				pInflate->Done = true;
				return true;
			}
		}
	}
	{
		size_t iConsumed;

		if ( !__xrtInflateFeedCodec(
			pInflate,
			pData != NULL ? pData + iOffset : NULL,
			iSize - iOffset,
			!bFinal,
			pOutput,
			pOutputData,
			&iConsumed,
			&bDone
		) ) {
			return false;
		}
		iOffset += iConsumed;
		if ( bDone ) {
			if ( iOffset != iSize ) {
				__xrtInflateError(
					XERR_PROTOCOL,
					XINFLATE_ERROR_DATA,
					"inflate-data",
					"DEFLATE stream contains trailing data"
				);
				return false;
			}
			pInflate->Done = true;
			return true;
		}
	}
	if ( bFinal ) {
		__xrtInflateError(
			XERR_PROTOCOL,
			XINFLATE_ERROR_DATA,
			"inflate-data",
			"DEFLATE stream is truncated"
		);
		return false;
	}
	return true;
}



/* 计入一个参与 gzip Header CRC 的字节。 */
static bool __xrtInflateGzipHeaderByte(
	xinflate* pInflate,
	uint8 iByte
)
{
	if ( pInflate->GzipHeaderBytes >=
		pInflate->Config.GzipHeaderLimit ) {
		__xrtInflateError(
			XERR_RANGE,
			XINFLATE_ERROR_LIMIT,
			"inflate-gzip-header",
			"gzip Header exceeds its configured limit"
		);
		return false;
	}
	pInflate->GzipHeaderBytes++;
	pInflate->GzipHeaderCrc = __xrtCompressCrc32Update(
		pInflate->GzipHeaderCrc,
		&iByte,
		1
	);
	return true;
}



/* 根据剩余 gzip FLG 选择下一个可选字段或 DEFLATE 正文。 */
static void __xrtInflateGzipSelect(xinflate* pInflate)
{
	if ( (pInflate->GzipFlags & 0x04u) != 0 ) {
		pInflate->GzipState =
			XRT_INFLATE_GZIP_EXTRA_LENGTH;
	} else if ( (pInflate->GzipFlags & 0x08u) != 0 ) {
		pInflate->GzipState = XRT_INFLATE_GZIP_NAME;
	} else if ( (pInflate->GzipFlags & 0x10u) != 0 ) {
		pInflate->GzipState = XRT_INFLATE_GZIP_COMMENT;
	} else if ( (pInflate->GzipFlags & 0x02u) != 0 ) {
		pInflate->GzipState =
			XRT_INFLATE_GZIP_HEADER_CRC;
	} else {
		__xrtInflateCodecReset(pInflate, XINFLATE_RAW);
		pInflate->GzipState = XRT_INFLATE_GZIP_DEFLATE;
	}
}



/* 清除已经消费的 gzip 可选字段标志并继续选择。 */
static void __xrtInflateGzipAfter(
	xinflate* pInflate,
	uint8 iFlag
)
{
	pInflate->GzipFlags &= (uint8)~iFlag;
	__xrtInflateGzipSelect(pInflate);
}



/* 流式解析 gzip Header、DEFLATE 正文、trailer 和拼接 member。 */
static bool __xrtInflateWriteGzip(
	xinflate* pInflate,
	const uint8* pData,
	size_t iSize,
	bool bFinal,
	xinflateoutputproc pOutput,
	ptr pOutputData
)
{
	size_t iOffset = 0;

	while ( iOffset < iSize ) {
		if ( pInflate->GzipState ==
			XRT_INFLATE_GZIP_BOUNDARY ) {
			__xrtInflateGzipReset(pInflate);
		}
		if ( pInflate->GzipState ==
			XRT_INFLATE_GZIP_FIXED ) {
			uint8 iByte = pData[iOffset];

			iOffset++;
			if ( !__xrtInflateGzipHeaderByte(
				pInflate,
				iByte
			) ) {
				return false;
			}
			pInflate->GzipFixed[
				pInflate->GzipFixedCount
			] = iByte;
			pInflate->GzipFixedCount++;
			if ( pInflate->GzipFixedCount == 10 ) {
				if ( (pInflate->GzipFixed[0] != 0x1Fu) ||
					(pInflate->GzipFixed[1] != 0x8Bu) ||
					(pInflate->GzipFixed[2] != 8u) ||
					((pInflate->GzipFixed[3] &
					  0xE0u) != 0) ) {
					__xrtInflateError(
						XERR_PROTOCOL,
						XINFLATE_ERROR_DATA,
						"inflate-gzip-header",
						"gzip Header is invalid"
					);
					return false;
				}
				pInflate->GzipFlags =
					pInflate->GzipFixed[3];
				__xrtInflateGzipSelect(pInflate);
			}
			continue;
		}
		if ( pInflate->GzipState ==
			XRT_INFLATE_GZIP_EXTRA_LENGTH ) {
			uint8 iByte = pData[iOffset];

			iOffset++;
			if ( !__xrtInflateGzipHeaderByte(
				pInflate,
				iByte
			) ) {
				return false;
			}
			pInflate->GzipPair[
				pInflate->GzipPairCount
			] = iByte;
			pInflate->GzipPairCount++;
			if ( pInflate->GzipPairCount == 2 ) {
				pInflate->GzipExtraRemain =
					(uint32)pInflate->GzipPair[0] |
					((uint32)pInflate->GzipPair[1] <<
					 8u);
				pInflate->GzipPairCount = 0;
				if ( pInflate->GzipExtraRemain == 0 ) {
					__xrtInflateGzipAfter(
						pInflate,
						0x04u
					);
				} else {
					pInflate->GzipState =
						XRT_INFLATE_GZIP_EXTRA;
				}
			}
			continue;
		}
		if ( pInflate->GzipState ==
			XRT_INFLATE_GZIP_EXTRA ) {
			uint8 iByte = pData[iOffset];

			iOffset++;
			if ( !__xrtInflateGzipHeaderByte(
				pInflate,
				iByte
			) ) {
				return false;
			}
			pInflate->GzipExtraRemain--;
			if ( pInflate->GzipExtraRemain == 0 ) {
				__xrtInflateGzipAfter(
					pInflate,
					0x04u
				);
			}
			continue;
		}
		if ( (pInflate->GzipState ==
			  XRT_INFLATE_GZIP_NAME) ||
			(pInflate->GzipState ==
			 XRT_INFLATE_GZIP_COMMENT) ) {
			xrt_inflate_gzip_state State =
				pInflate->GzipState;
			uint8 iByte = pData[iOffset];

			iOffset++;
			if ( !__xrtInflateGzipHeaderByte(
				pInflate,
				iByte
			) ) {
				return false;
			}
			if ( iByte == 0 ) {
				__xrtInflateGzipAfter(
					pInflate,
					State == XRT_INFLATE_GZIP_NAME ?
						0x08u : 0x10u
				);
			}
			continue;
		}
		if ( pInflate->GzipState ==
			XRT_INFLATE_GZIP_HEADER_CRC ) {
			if ( pInflate->GzipHeaderBytes >=
				pInflate->Config.GzipHeaderLimit ) {
				__xrtInflateError(
					XERR_RANGE,
					XINFLATE_ERROR_LIMIT,
					"inflate-gzip-header",
					"gzip Header exceeds its configured limit"
				);
				return false;
			}
			pInflate->GzipHeaderBytes++;
			pInflate->GzipPair[
				pInflate->GzipPairCount
			] = pData[iOffset];
			pInflate->GzipPairCount++;
			iOffset++;
			if ( pInflate->GzipPairCount == 2 ) {
				uint32 iExpected =
					(uint32)pInflate->GzipPair[0] |
					((uint32)pInflate->GzipPair[1] <<
					 8u);

				if ( ((pInflate->GzipHeaderCrc ^
					   UINT32_MAX) & 0xFFFFu) !=
					iExpected ) {
					__xrtInflateError(
						XERR_PROTOCOL,
						XINFLATE_ERROR_DATA,
						"inflate-gzip-header",
						"gzip Header checksum is invalid"
					);
					return false;
				}
				pInflate->GzipPairCount = 0;
				__xrtInflateGzipAfter(
					pInflate,
					0x02u
				);
			}
			continue;
		}
		if ( pInflate->GzipState ==
			XRT_INFLATE_GZIP_DEFLATE ) {
			size_t iConsumed;
			bool bDone;

			if ( !__xrtInflateFeedCodec(
				pInflate,
				pData + iOffset,
				iSize - iOffset,
				!bFinal,
				pOutput,
				pOutputData,
				&iConsumed,
				&bDone
			) ) {
				return false;
			}
			iOffset += iConsumed;
			if ( bDone ) {
				pInflate->GzipState =
					XRT_INFLATE_GZIP_TRAILER;
			} else {
				break;
			}
			continue;
		}
		if ( pInflate->GzipState ==
			XRT_INFLATE_GZIP_TRAILER ) {
			pInflate->GzipTrailer[
				pInflate->GzipTrailerCount
			] = pData[iOffset];
			pInflate->GzipTrailerCount++;
			iOffset++;
			if ( pInflate->GzipTrailerCount == 8 ) {
				uint32 iCrc = MZ_READ_LE32(
					pInflate->GzipTrailer
				);
				uint32 iSize32 = MZ_READ_LE32(
					pInflate->GzipTrailer + 4
				);

				if ( (iCrc !=
					  (pInflate->GzipDataCrc ^
					   UINT32_MAX)) ||
					(iSize32 !=
					 pInflate->GzipDataSize) ) {
					__xrtInflateError(
						XERR_PROTOCOL,
						XINFLATE_ERROR_DATA,
						"inflate-gzip-trailer",
						"gzip trailer checksum or size is invalid"
					);
					return false;
				}
				pInflate->GzipState =
					XRT_INFLATE_GZIP_BOUNDARY;
			}
			continue;
		}
	}
	if ( bFinal &&
		(pInflate->GzipState ==
		 XRT_INFLATE_GZIP_DEFLATE) ) {
		size_t iConsumed;
		bool bDone;

		if ( !__xrtInflateFeedCodec(
			pInflate,
			NULL,
			0,
			false,
			pOutput,
			pOutputData,
			&iConsumed,
			&bDone
		) || !bDone ) {
			if ( xrtGetError() == NULL ) {
				__xrtInflateError(
					XERR_PROTOCOL,
					XINFLATE_ERROR_DATA,
					"inflate-gzip-data",
					"gzip DEFLATE stream is truncated"
				);
			}
			return false;
		}
		pInflate->GzipState =
			XRT_INFLATE_GZIP_TRAILER;
	}
	if ( bFinal ) {
		if ( pInflate->GzipState !=
			XRT_INFLATE_GZIP_BOUNDARY ) {
			__xrtInflateError(
				XERR_PROTOCOL,
				XINFLATE_ERROR_DATA,
				"inflate-gzip-data",
				"gzip stream is truncated"
			);
			return false;
		}
		pInflate->Done = true;
	}
	return true;
}



/* 初始化公开 Inflate 默认配置。 */
XRT_API void xrtInflateConfigInit(xinflateconfig* pConfig)
{
	xinflateconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtInflateError(
			XERR_ARGUMENT,
			XINFLATE_ERROR_ARGUMENT,
			"init-inflate-config",
			"Inflate config range is invalid"
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.Format = XINFLATE_DEFLATE;
	Config.OutputLimit = XINFLATE_OUTPUT_UNLIMITED;
	Config.GzipHeaderLimit =
		XINFLATE_GZIP_HEADER_DEFAULT;
	Config.WindowBits = XINFLATE_WINDOW_MAX;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 复位公开解码器并保留窗口存储。 */
XRT_API bool xrtInflateReset(
	xinflate* pInflate,
	const xinflateconfig* pConfig
)
{
	xinflateconfig Config;

	if ( pInflate == NULL ) {
		__xrtInflateError(
			XERR_ARGUMENT,
			XINFLATE_ERROR_ARGUMENT,
			"reset-inflate",
			"Inflate decoder is null"
		);
		return false;
	}
	if ( pInflate->Writing ) {
		__xrtInflateError(
			XERR_STATE,
			XINFLATE_ERROR_STATE,
			"reset-inflate",
			"Inflate decoder cannot reset from its output callback"
		);
		return false;
	}
	xrtInflateConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !__xrtRangeValid(pConfig, sizeof(*pConfig)) ) {
			__xrtInflateError(
				XERR_ARGUMENT,
				XINFLATE_ERROR_ARGUMENT,
				"reset-inflate",
				"Inflate config range is invalid"
			);
			return false;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( !__xrtInflateConfigValid(
		&Config,
		"reset-inflate"
	) ) {
		return false;
	}
	memset(
		(uint8*)pInflate + offsetof(xinflate, Config),
		0,
		offsetof(xinflate, Dictionary) -
			offsetof(xinflate, Config)
	);
	pInflate->Config = Config;
	if ( Config.Format == XINFLATE_GZIP ) {
		__xrtInflateGzipReset(pInflate);
	} else if ( Config.Format != XINFLATE_DEFLATE ) {
		__xrtInflateCodecReset(
			pInflate,
			Config.Format
		);
	}
	return true;
}



/* 创建一个按需拥有算法窗口的解码器。 */
XRT_API xinflate* xrtInflateCreate(
	const xinflateconfig* pConfig
)
{
	xinflate* pInflate = (xinflate*)xrtMalloc(
		sizeof(*pInflate)
	);

	if ( pInflate == NULL ) {
		return NULL;
	}
	memset(pInflate, 0, sizeof(*pInflate));
	if ( !xrtInflateReset(pInflate, pConfig) ) {
		xrtFree(pInflate);
		return NULL;
	}
	return pInflate;
}



/* 同步推进一个完整输入片段。 */
XRT_API bool xrtInflateWrite(
	xinflate* pInflate,
	xbytesview Input,
	bool bFinal,
	xinflateoutputproc pOutput,
	ptr pData
)
{
	bool bSuccess;

	if ( (pInflate == NULL) ||
		!__xrtRangeValid(Input.Data, Input.Size) ) {
		__xrtInflateError(
			XERR_ARGUMENT,
			XINFLATE_ERROR_ARGUMENT,
			"write-inflate",
			"Inflate decoder or input view is invalid"
		);
		return false;
	}
	if ( pInflate->Writing ||
		pInflate->Done ||
		pInflate->Failed ) {
		__xrtInflateError(
			XERR_STATE,
			XINFLATE_ERROR_STATE,
			"write-inflate",
			"Inflate decoder is busy or already terminal"
		);
		return false;
	}
	pInflate->Writing = true;
	bSuccess = pInflate->Config.Format == XINFLATE_GZIP ?
		__xrtInflateWriteGzip(
			pInflate,
			Input.Data,
			Input.Size,
			bFinal,
			pOutput,
			pData
		) :
		__xrtInflateWriteDeflate(
			pInflate,
			Input.Data,
			Input.Size,
			bFinal,
			pOutput,
			pData
		);
	pInflate->Writing = false;
	if ( !bSuccess ) {
		pInflate->Failed = true;
		return false;
	}
	if ( bFinal && !pInflate->Done ) {
		__xrtInflateError(
			XERR_PROTOCOL,
			XINFLATE_ERROR_DATA,
			"write-inflate",
			"compressed stream did not reach a complete end"
		);
		pInflate->Failed = true;
		return false;
	}
	return true;
}



/* 返回解码器完整结束状态。 */
XRT_API bool xrtInflateDone(const xinflate* pInflate)
{
	if ( pInflate == NULL ) {
		__xrtInflateError(
			XERR_ARGUMENT,
			XINFLATE_ERROR_ARGUMENT,
			"query-inflate-done",
			"Inflate decoder is null"
		);
		return false;
	}
	return pInflate->Done && !pInflate->Failed;
}



/* 返回累计解码输出长度。 */
XRT_API uint64 xrtInflateOutputSize(
	const xinflate* pInflate
)
{
	if ( pInflate == NULL ) {
		__xrtInflateError(
			XERR_ARGUMENT,
			XINFLATE_ERROR_ARGUMENT,
			"query-inflate-output",
			"Inflate decoder is null"
		);
		return 0;
	}
	return pInflate->TotalOutput;
}



/* 销毁解码器并清除压缩状态。 */
XRT_API void xrtInflateDestroy(xinflate* pInflate)
{
	if ( pInflate == NULL ) {
		return;
	}
	if ( pInflate->Writing ) {
		__xrtInflateError(
			XERR_STATE,
			XINFLATE_ERROR_STATE,
			"destroy-inflate",
			"Inflate decoder cannot be destroyed from its output callback"
		);
		return;
	}
	memset(pInflate, 0, sizeof(*pInflate));
	xrtFree(pInflate);
}



/* 为整块便捷函数追加一段输出并保留一个零字节空间。 */
static bool __xrtInflateOutputAppend(
	xbytesview Data,
	ptr pData
)
{
	xrt_inflate_output* pOutput =
		(xrt_inflate_output*)pData;
	size_t iRequired;
	size_t iCapacity;
	bytes pBytes;

	if ( Data.Size > (SIZE_MAX - pOutput->Size - 1u) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iRequired = pOutput->Size + Data.Size + 1u;
	if ( iRequired > pOutput->Capacity ) {
		iCapacity = pOutput->Capacity != 0 ?
			pOutput->Capacity : 256u;
		while ( iCapacity < iRequired ) {
			size_t iNext = iCapacity > (SIZE_MAX / 2u) ?
				iRequired : (iCapacity * 2u);

			if ( iNext <= iCapacity ) {
				iCapacity = iRequired;
				break;
			}
			iCapacity = iNext;
		}
		pBytes = (bytes)xrtRealloc(
			pOutput->Data,
			iCapacity
		);
		if ( pBytes == NULL ) {
			return false;
		}
		pOutput->Data = pBytes;
		pOutput->Capacity = iCapacity;
	}
	if ( Data.Size != 0 ) {
		memcpy(
			pOutput->Data + pOutput->Size,
			Data.Data,
			Data.Size
		);
	}
	pOutput->Size += Data.Size;
	pOutput->Data[pOutput->Size] = 0;
	return true;
}



/* 一次性解码完整输入并返回拥有型连续结果。 */
XRT_API bytes xrtInflateAll(
	xbytesview Input,
	const xinflateconfig* pConfig,
	size_t* pOutputSize
)
{
	xrt_inflate_output Output;
	xinflate* pInflate;

	if ( !__xrtRangeValid(Input.Data, Input.Size) ||
		!__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtInflateError(
			XERR_ARGUMENT,
			XINFLATE_ERROR_ARGUMENT,
			"inflate-all",
			"Inflate input view or output length is invalid"
		);
		return NULL;
	}
	memset(&Output, 0, sizeof(Output));
	pInflate = xrtInflateCreate(pConfig);
	if ( pInflate == NULL ) {
		return NULL;
	}
	if ( !xrtInflateWrite(
		pInflate,
		Input,
		true,
		__xrtInflateOutputAppend,
		&Output
	) ) {
		xrtInflateDestroy(pInflate);
		xrtFree(Output.Data);
		return NULL;
	}
	xrtInflateDestroy(pInflate);
	if ( Output.Data == NULL ) {
		Output.Data = (bytes)xrtMalloc(1);
		if ( Output.Data == NULL ) {
			return NULL;
		}
		Output.Data[0] = 0;
	}
	memcpy(pOutputSize, &Output.Size, sizeof(Output.Size));
	return Output.Data;
}

#endif
