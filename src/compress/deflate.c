#include "../internal/xrt_compress.h"
#include "../third_party/miniz/miniz_tdef.h"



#if defined(XRT_FEATURE_DEFLATE)

#define XRT_DEFLATE_GZIP_HEADER_SIZE 10u
#define XRT_DEFLATE_GZIP_TRAILER_SIZE 8u



/* 编码器把算法状态和公开生命周期放在同一个按需分配对象中。 */
struct xdeflate {
	tdefl_compressor Codec;
	xdeflateconfig Config;
	xdeflateoutputproc Output;
	ptr OutputData;
	uint64 TotalOutput;
	uint32 GzipCrc;
	uint32 GzipSize;
	bool GzipHeader;
	bool Writing;
	bool Done;
	bool Failed;
};



/* 整块便捷函数使用按需增长且带末尾零字节的输出。 */
typedef struct xrt_deflate_output {
	bytes Data;
	size_t Size;
	size_t Capacity;
} xrt_deflate_output;



/* 建立 Deflate 域的结构化错误。 */
static void __xrtDeflateError(
	xerrkind Kind,
	xdeflateerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.deflate";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 验证公开格式、级别、策略和输出上限配置。 */
bool __xrtDeflateConfigValid(
	const xdeflateconfig* pConfig,
	cstr sOperation
)
{
	if ( (pConfig->Format < XDEFLATE_RAW) ||
		(pConfig->Format > XDEFLATE_GZIP) ||
		(pConfig->Level < 0) ||
		(pConfig->Level > 10) ||
		(pConfig->WindowBits < XDEFLATE_WINDOW_MIN) ||
		(pConfig->WindowBits > XDEFLATE_WINDOW_MAX) ||
		(pConfig->Strategy <
		 XDEFLATE_STRATEGY_DEFAULT) ||
		(pConfig->Strategy >
		 XDEFLATE_STRATEGY_FIXED) ) {
		__xrtDeflateError(
			XERR_VALUE,
			XDEFLATE_ERROR_CONFIG,
			sOperation,
			"invalid Deflate format, level, window or strategy"
		);
		return false;
	}
	return true;
}



/* 验证调用方提供的 Deflate 配置快照。 */
XRT_API bool xrtDeflateConfigValid(const xdeflateconfig* pConfig)
{
	xdeflateconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Config, pConfig, sizeof(Config));
	return __xrtDeflateConfigValid(
		&Config,
		"validate-deflate-config"
	);
}



/* 把公开策略转换为 miniz 使用的稳定数值。 */
static int __xrtDeflateStrategy(
	xdeflatestrategy Strategy
)
{
	return (int)Strategy;
}



/* 把公开 Flush 转换为底层编码器模式。 */
static tdefl_flush __xrtDeflateFlush(
	xdeflateflush Flush
)
{
	switch ( Flush ) {
		case XDEFLATE_FLUSH_NONE:
			return TDEFL_NO_FLUSH;

		case XDEFLATE_FLUSH_SYNC:
			return TDEFL_SYNC_FLUSH;

		case XDEFLATE_FLUSH_FULL:
			return TDEFL_FULL_FLUSH;

		default:
			return TDEFL_FINISH;
	}
}



/* 在硬上限内把一个完整输出片段同步交给消费者。 */
static bool __xrtDeflateEmit(
	xdeflate* pDeflate,
	const void* pData,
	size_t iSize
)
{
	xbytesview Data;

	if ( iSize == 0 ) {
		return true;
	}
	if ( (pData == NULL) ||
		(pDeflate->TotalOutput >
		 pDeflate->Config.OutputLimit) ||
		((uint64)iSize >
		 (pDeflate->Config.OutputLimit -
		  pDeflate->TotalOutput)) ) {
		__xrtDeflateError(
			pData == NULL ? XERR_INTERNAL : XERR_RANGE,
			pData == NULL ?
				XDEFLATE_ERROR_CODEC :
				XDEFLATE_ERROR_LIMIT,
			"write-deflate",
			pData == NULL ?
				"Deflate codec produced an invalid output view" :
				"Deflate output exceeds the configured limit"
		);
		pDeflate->Failed = true;
		return false;
	}
	Data.Data = (cbytes)pData;
	Data.Size = iSize;
	if ( pDeflate->Output != NULL ) {
		xrtClearError();
		if ( !pDeflate->Output(
			Data,
			pDeflate->OutputData
		) ) {
			if ( xrtGetError() == NULL ) {
				__xrtDeflateError(
					XERR_CANCELLED,
					XDEFLATE_ERROR_OUTPUT,
					"write-deflate",
					"Deflate output callback stopped encoding"
				);
			}
			pDeflate->Failed = true;
			return false;
		}
	}
	pDeflate->TotalOutput += (uint64)iSize;
	return true;
}



/* 将 miniz 输出回调收口到 XRT 限额、错误和消费者契约。 */
static mz_bool __xrtDeflatePut(
	const void* pData,
	int iSize,
	void* pUser
)
{
	xdeflate* pDeflate = (xdeflate*)pUser;

	if ( (pDeflate == NULL) || (iSize < 0) ) {
		if ( pDeflate != NULL ) {
			__xrtDeflateError(
				XERR_INTERNAL,
				XDEFLATE_ERROR_CODEC,
				"write-deflate",
				"Deflate codec returned an invalid output length"
			);
			pDeflate->Failed = true;
		}
		return MZ_FALSE;
	}
	return __xrtDeflateEmit(
		pDeflate,
		pData,
		(size_t)iSize
	) ? MZ_TRUE : MZ_FALSE;
}



/* 为 gzip 数据流写入确定性、无可选字段的十字节 Header。 */
static bool __xrtDeflateGzipHeader(xdeflate* pDeflate)
{
	uint8 Header[XRT_DEFLATE_GZIP_HEADER_SIZE] = {
		UINT8_C(0x1f), UINT8_C(0x8b), 8u, 0u,
		0u, 0u, 0u, 0u, 0u, 255u
	};

	if ( pDeflate->GzipHeader ) {
		return true;
	}
	pDeflate->GzipHeader = true;
	return __xrtDeflateEmit(
		pDeflate,
		Header,
		sizeof(Header)
	);
}



/* 在原始 DEFLATE 结束后写入 gzip CRC32 与输入长度模 2^32。 */
static bool __xrtDeflateGzipTrailer(xdeflate* pDeflate)
{
	uint32 iCrc = ~pDeflate->GzipCrc;
	uint32 iSize = pDeflate->GzipSize;
	uint8 Trailer[XRT_DEFLATE_GZIP_TRAILER_SIZE];

	Trailer[0] = (uint8)iCrc;
	Trailer[1] = (uint8)(iCrc >> 8u);
	Trailer[2] = (uint8)(iCrc >> 16u);
	Trailer[3] = (uint8)(iCrc >> 24u);
	Trailer[4] = (uint8)iSize;
	Trailer[5] = (uint8)(iSize >> 8u);
	Trailer[6] = (uint8)(iSize >> 16u);
	Trailer[7] = (uint8)(iSize >> 24u);
	return __xrtDeflateEmit(
		pDeflate,
		Trailer,
		sizeof(Trailer)
	);
}



/* 初始化公开 Deflate 默认配置。 */
XRT_API void xrtDeflateConfigInit(
	xdeflateconfig* pConfig
)
{
	xdeflateconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtDeflateError(
			XERR_ARGUMENT,
			XDEFLATE_ERROR_ARGUMENT,
			"init-deflate-config",
			"Deflate config range is invalid"
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.Format = XDEFLATE_GZIP;
	Config.Level = XDEFLATE_LEVEL_DEFAULT;
	Config.Strategy =
		XDEFLATE_STRATEGY_DEFAULT;
	Config.OutputLimit =
		XDEFLATE_OUTPUT_UNLIMITED;
	Config.WindowBits = XDEFLATE_WINDOW_MAX;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 复位公开编码器并保留算法状态存储。 */
XRT_API bool xrtDeflateReset(
	xdeflate* pDeflate,
	const xdeflateconfig* pConfig
)
{
	xdeflateconfig Config;
	int iFlags;

	if ( pDeflate == NULL ) {
		__xrtDeflateError(
			XERR_ARGUMENT,
			XDEFLATE_ERROR_ARGUMENT,
			"reset-deflate",
			"Deflate encoder is null"
		);
		return false;
	}
	if ( pDeflate->Writing ) {
		__xrtDeflateError(
			XERR_STATE,
			XDEFLATE_ERROR_STATE,
			"reset-deflate",
			"Deflate encoder cannot reset from its output callback"
		);
		return false;
	}
	xrtDeflateConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !__xrtRangeValid(pConfig, sizeof(*pConfig)) ) {
			__xrtDeflateError(
				XERR_ARGUMENT,
				XDEFLATE_ERROR_ARGUMENT,
				"reset-deflate",
				"Deflate config range is invalid"
			);
			return false;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( !__xrtDeflateConfigValid(
		&Config,
		"reset-deflate"
	) ) {
		return false;
	}
	memset(
		(uint8*)pDeflate + offsetof(xdeflate, Config),
		0,
		sizeof(*pDeflate) -
			offsetof(xdeflate, Config)
	);
	pDeflate->Config = Config;
	pDeflate->GzipCrc = UINT32_MAX;
	iFlags = (int)tdefl_create_comp_flags_from_zip_params(
		Config.Level,
		Config.Format == XDEFLATE_ZLIB ? 15 : -15,
		__xrtDeflateStrategy(Config.Strategy)
	);
	if ( (tdefl_init(
			&pDeflate->Codec,
			__xrtDeflatePut,
			pDeflate,
			iFlags
		  ) != TDEFL_STATUS_OKAY) ||
		!tdefl_set_window_bits(
			&pDeflate->Codec,
			(int)Config.WindowBits
		) ) {
		__xrtDeflateError(
			XERR_INTERNAL,
			XDEFLATE_ERROR_CODEC,
			"reset-deflate",
			"Deflate codec initialization failed"
		);
		pDeflate->Failed = true;
		return false;
	}
	return true;
}



/* 创建一个按需拥有算法状态的编码器。 */
XRT_API xdeflate* xrtDeflateCreate(
	const xdeflateconfig* pConfig
)
{
	xdeflate* pDeflate = (xdeflate*)xrtMalloc(
		sizeof(*pDeflate)
	);

	if ( pDeflate == NULL ) {
		return NULL;
	}
	pDeflate->Writing = false;
	if ( !xrtDeflateReset(pDeflate, pConfig) ) {
		xrtFree(pDeflate);
		return NULL;
	}
	return pDeflate;
}



/* 同步推进一个完整输入片段和可选 Flush 边界。 */
XRT_API bool xrtDeflateWrite(
	xdeflate* pDeflate,
	xbytesview Input,
	xdeflateflush Flush,
	xdeflateoutputproc pOutput,
	ptr pData
)
{
	tdefl_status Status;
	bool bFinish;

	if ( (pDeflate == NULL) ||
		!__xrtRangeValid(Input.Data, Input.Size) ||
		(Flush < XDEFLATE_FLUSH_NONE) ||
		(Flush > XDEFLATE_FLUSH_FINISH) ) {
		__xrtDeflateError(
			XERR_ARGUMENT,
			XDEFLATE_ERROR_ARGUMENT,
			"write-deflate",
			"Deflate encoder, input view or Flush is invalid"
		);
		return false;
	}
	if ( pDeflate->Writing ||
		pDeflate->Done ||
		pDeflate->Failed ) {
		__xrtDeflateError(
			XERR_STATE,
			XDEFLATE_ERROR_STATE,
			"write-deflate",
			"Deflate encoder is busy or already terminal"
		);
		return false;
	}
	pDeflate->Output = pOutput;
	pDeflate->OutputData = pData;
	pDeflate->Writing = true;
	if ( (pDeflate->Config.Format == XDEFLATE_GZIP) &&
		!__xrtDeflateGzipHeader(pDeflate) ) {
		pDeflate->Writing = false;
		pDeflate->Output = NULL;
		pDeflate->OutputData = NULL;
		return false;
	}
	if ( pDeflate->Config.Format == XDEFLATE_GZIP ) {
		pDeflate->GzipCrc = __xrtCompressCrc32Update(
			pDeflate->GzipCrc,
			Input.Data,
			Input.Size
		);
		pDeflate->GzipSize += (uint32)Input.Size;
	}
	bFinish = Flush == XDEFLATE_FLUSH_FINISH;
	Status = tdefl_compress_buffer(
		&pDeflate->Codec,
		Input.Data,
		Input.Size,
		__xrtDeflateFlush(Flush)
	);
	if ( ((bFinish && (Status != TDEFL_STATUS_DONE)) ||
		  (!bFinish && (Status != TDEFL_STATUS_OKAY))) &&
		!pDeflate->Failed ) {
		__xrtDeflateError(
			Status == TDEFL_STATUS_PUT_BUF_FAILED ?
				XERR_IO : XERR_INTERNAL,
			Status == TDEFL_STATUS_PUT_BUF_FAILED ?
				XDEFLATE_ERROR_OUTPUT :
				XDEFLATE_ERROR_CODEC,
			"write-deflate",
			Status == TDEFL_STATUS_PUT_BUF_FAILED ?
				"Deflate output consumer failed" :
				"Deflate codec entered an invalid state"
		);
		pDeflate->Failed = true;
	}
	if ( bFinish &&
		!pDeflate->Failed &&
		(pDeflate->Config.Format == XDEFLATE_GZIP) &&
		!__xrtDeflateGzipTrailer(pDeflate) ) {
		pDeflate->Failed = true;
	}
	pDeflate->Writing = false;
	pDeflate->Output = NULL;
	pDeflate->OutputData = NULL;
	if ( pDeflate->Failed ) {
		return false;
	}
	if ( bFinish ) {
		pDeflate->Done = true;
	}
	return true;
}



/* 查询完整结束状态。 */
XRT_API bool xrtDeflateDone(
	const xdeflate* pDeflate
)
{
	if ( pDeflate == NULL ) {
		__xrtDeflateError(
			XERR_ARGUMENT,
			XDEFLATE_ERROR_ARGUMENT,
			"query-deflate-done",
			"Deflate encoder is null"
		);
		return false;
	}
	return pDeflate->Done && !pDeflate->Failed;
}



/* 查询已经成功交付的输出字节数。 */
XRT_API uint64 xrtDeflateOutputSize(
	const xdeflate* pDeflate
)
{
	if ( pDeflate == NULL ) {
		__xrtDeflateError(
			XERR_ARGUMENT,
			XDEFLATE_ERROR_ARGUMENT,
			"query-deflate-output",
			"Deflate encoder is null"
		);
		return 0;
	}
	return pDeflate->TotalOutput;
}



/* 销毁按需分配的编码器状态。 */
XRT_API void xrtDeflateDestroy(xdeflate* pDeflate)
{
	if ( pDeflate == NULL ) {
		return;
	}
	if ( pDeflate->Writing ) {
		__xrtDeflateError(
			XERR_STATE,
			XDEFLATE_ERROR_STATE,
			"destroy-deflate",
			"Deflate encoder cannot be destroyed from its output callback"
		);
		return;
	}
	xrtFree(pDeflate);
}



/* 为整块便捷函数追加输出并保留一个末尾零字节。 */
static bool __xrtDeflateOutputAppend(
	xbytesview Data,
	ptr pData
)
{
	xrt_deflate_output* pOutput =
		(xrt_deflate_output*)pData;
	bytes pBytes;
	size_t iRequired;
	size_t iCapacity;

	if ( (Data.Size > (SIZE_MAX - pOutput->Size)) ||
		((pOutput->Size + Data.Size) == SIZE_MAX) ) {
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
	memcpy(
		pOutput->Data + pOutput->Size,
		Data.Data,
		Data.Size
	);
	pOutput->Size += Data.Size;
	pOutput->Data[pOutput->Size] = 0;
	return true;
}



/* 一次性编码完整输入并返回拥有型连续结果。 */
XRT_API bytes xrtDeflateAll(
	xbytesview Input,
	const xdeflateconfig* pConfig,
	size_t* pOutputSize
)
{
	xrt_deflate_output Output;
	xdeflate* pDeflate;

	if ( !__xrtRangeValid(Input.Data, Input.Size) ||
		!__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtDeflateError(
			XERR_ARGUMENT,
			XDEFLATE_ERROR_ARGUMENT,
			"deflate-all",
			"Deflate input view or output length is invalid"
		);
		return NULL;
	}
	memset(&Output, 0, sizeof(Output));
	pDeflate = xrtDeflateCreate(pConfig);
	if ( pDeflate == NULL ) {
		return NULL;
	}
	if ( !xrtDeflateWrite(
		pDeflate,
		Input,
		XDEFLATE_FLUSH_FINISH,
		__xrtDeflateOutputAppend,
		&Output
	) ) {
		xrtDeflateDestroy(pDeflate);
		xrtFree(Output.Data);
		return NULL;
	}
	xrtDeflateDestroy(pDeflate);
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
