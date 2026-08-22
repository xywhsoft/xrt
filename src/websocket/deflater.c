#include "../internal/xrt_websocket.h"



#if defined(XRT_FEATURE_WEBSOCKET_DEFLATER)

#define __XRT_WS_DEFLATE_TAIL_SIZE 4u



static const uint8 __g_xrtWsDeflateTail[
	__XRT_WS_DEFLATE_TAIL_SIZE
] = {
	0x00u, 0x00u, 0xFFu, 0xFFu
};



/* 发送变换惰性拥有编码器，并只暂存可能成为同步尾部的最后四字节。 */
struct xwsdeflater {
	xwsdeflaterconfig Config;
	xdeflate* Deflate;
	xwsoutputproc Output;
	ptr OutputData;
	uint64 Size;
	uint8 Tail[__XRT_WS_DEFLATE_TAIL_SIZE];
	uint8 TailSize;
	bool Active;
	bool Compressed;
	bool Writing;
	bool OutputFailure;
	bool Failed;
};



/* 初始化已经对齐的发送运行时默认配置。 */
static void __xrtWsDeflaterConfigDefault(
	xwsdeflaterconfig* pConfig
)
{
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->OutputLimit = UINT64_MAX;
	pConfig->Level = XDEFLATE_LEVEL_DEFAULT;
	pConfig->Strategy = XDEFLATE_STRATEGY_DEFAULT;
	pConfig->WindowBits = XWS_DEFLATE_WINDOW_MAX;
}



/* 验证已经对齐的发送运行时配置。 */
static bool __xrtWsDeflaterConfigValueValid(
	const xwsdeflaterconfig* pConfig
)
{
	return (pConfig->Level >= 0) &&
		(pConfig->Level <= 10) &&
		(pConfig->Strategy >=
		 XDEFLATE_STRATEGY_DEFAULT) &&
		(pConfig->Strategy <=
		 XDEFLATE_STRATEGY_FIXED) &&
		(pConfig->WindowBits >=
		 XWS_DEFLATE_WINDOW_MIN) &&
		(pConfig->WindowBits <=
		 XWS_DEFLATE_WINDOW_MAX);
}



/* 读取可选且可能未对齐的配置，空配置使用默认值。 */
static bool __xrtWsDeflaterConfigRead(
	const xwsdeflaterconfig* pSource,
	xwsdeflaterconfig* pConfig
)
{
	if ( pSource == NULL ) {
		__xrtWsDeflaterConfigDefault(pConfig);
		return true;
	}
	if ( !__xrtRangeValid(pSource, sizeof(*pSource)) ) {
		return false;
	}
	memcpy(pConfig, pSource, sizeof(*pConfig));
	return __xrtWsDeflaterConfigValueValid(pConfig);
}



/* 验证借用输入视图。 */
static bool __xrtWsDeflaterViewValid(xbytesview Input)
{
	return __xrtRangeValid(Input.Data, Input.Size);
}



/* 构造底层 raw Deflate 配置。 */
static void __xrtWsDeflaterCodecConfig(
	const xwsdeflaterconfig* pConfig,
	xdeflateconfig* pDeflate
)
{
	xrtDeflateConfigInit(pDeflate);
	pDeflate->Format = XDEFLATE_RAW;
	pDeflate->Level = pConfig->Level;
	pDeflate->Strategy = pConfig->Strategy;
	pDeflate->OutputLimit =
		XDEFLATE_OUTPUT_UNLIMITED;
	pDeflate->WindowBits = pConfig->WindowBits;
}



/* 创建或复位底层 Deflate，同时保留当前消息状态。 */
static bool __xrtWsDeflaterCodecReset(
	xwsdeflater* pDeflater
)
{
	xdeflateconfig Config;

	__xrtWsDeflaterCodecConfig(
		&pDeflater->Config,
		&Config
	);
	if ( pDeflater->Deflate == NULL ) {
		pDeflater->Deflate = xrtDeflateCreate(&Config);
		return pDeflater->Deflate != NULL;
	}
	return xrtDeflateReset(pDeflater->Deflate, &Config);
}



/* 在单消息硬上限内同步发布线路负载。 */
static bool __xrtWsDeflaterEmit(
	xwsdeflater* pDeflater,
	xbytesview Data
)
{
	if ( (pDeflater->Size >
		  pDeflater->Config.OutputLimit) ||
		((uint64)Data.Size >
		 (pDeflater->Config.OutputLimit -
		  pDeflater->Size)) ) {
		__xrtWsDeflateError(
			XERR_RANGE,
			XWS_DEFLATE_ERROR_LIMIT,
			"deflate-output",
			"permessage-deflate output exceeds the message limit"
		);
		pDeflater->OutputFailure = true;
		return false;
	}
	if ( pDeflater->Output != NULL ) {
		xrtClearError();
		if ( !pDeflater->Output(
			Data,
			pDeflater->OutputData
		) ) {
			if ( xrtGetError() == NULL ) {
				__xrtWsDeflateError(
					XERR_CANCELLED,
					XWS_DEFLATE_ERROR_OUTPUT,
					"deflate-output",
					"permessage-deflate output callback stopped encoding"
				);
			}
			pDeflater->OutputFailure = true;
			return false;
		}
	}
	pDeflater->Size += (uint64)Data.Size;
	return true;
}



/* 保留输出末尾四字节，使消息结束时可以无复制地剥离同步尾部。 */
static bool __xrtWsDeflaterOutput(
	xbytesview Data,
	ptr pData
)
{
	xwsdeflater* pDeflater = (xwsdeflater*)pData;
	uint8 Tail[__XRT_WS_DEFLATE_TAIL_SIZE];
	size_t iOld = pDeflater->TailSize;
	size_t iTotal = iOld + Data.Size;
	size_t iKeep = iTotal <
		__XRT_WS_DEFLATE_TAIL_SIZE ?
			iTotal : __XRT_WS_DEFLATE_TAIL_SIZE;
	size_t iEmit = iTotal - iKeep;
	size_t iOldEmit = iEmit < iOld ? iEmit : iOld;
	size_t iDataEmit = iEmit - iOldEmit;

	if ( iKeep <= Data.Size ) {
		if ( iKeep != 0 ) {
			memcpy(
				Tail,
				Data.Data + Data.Size - iKeep,
				iKeep
			);
		}
	} else {
		size_t iOldKeep = iKeep - Data.Size;

		memcpy(
			Tail,
			pDeflater->Tail + iOld - iOldKeep,
			iOldKeep
		);
		if ( Data.Size != 0 ) {
			memcpy(
				Tail + iOldKeep,
				Data.Data,
				Data.Size
			);
		}
	}
	if ( (iOldEmit != 0) &&
		!__xrtWsDeflaterEmit(
			pDeflater,
			(xbytesview){
				pDeflater->Tail,
				iOldEmit
			}
		) ) {
		return false;
	}
	if ( (iDataEmit != 0) &&
		!__xrtWsDeflaterEmit(
			pDeflater,
			(xbytesview){
				Data.Data,
				iDataEmit
			}
		) ) {
		return false;
	}
	if ( iKeep != 0 ) {
		memcpy(pDeflater->Tail, Tail, iKeep);
	}
	pDeflater->TailSize = (uint8)iKeep;
	return true;
}



/* 完成一次底层写入，并把编码错误包装到稳定 WebSocket 域。 */
static bool __xrtWsDeflaterCodecWrite(
	xwsdeflater* pDeflater,
	xbytesview Input,
	xdeflateflush Flush,
	xwsoutputproc pOutput,
	ptr pData
)
{
	bool bSuccess;

	pDeflater->Output = pOutput;
	pDeflater->OutputData = pData;
	pDeflater->OutputFailure = false;
	pDeflater->Writing = true;
	bSuccess = xrtDeflateWrite(
		pDeflater->Deflate,
		Input,
		Flush,
		__xrtWsDeflaterOutput,
		pDeflater
	);
	pDeflater->Writing = false;
	pDeflater->Output = NULL;
	pDeflater->OutputData = NULL;
	if ( bSuccess ) {
		return true;
	}
	if ( !pDeflater->OutputFailure ) {
		__xrtWsDeflateWrap(
			XERR_INTERNAL,
			XWS_DEFLATE_ERROR_CODEC,
			"deflate",
			"failed to encode permessage-deflate payload"
		);
	}
	pDeflater->Failed = true;
	return false;
}



/* 在重入保护下直接发布直通或协议补充输出。 */
static bool __xrtWsDeflaterPublish(
	xwsdeflater* pDeflater,
	xbytesview Data,
	xwsoutputproc pOutput,
	ptr pData
)
{
	bool bSuccess;

	pDeflater->Output = pOutput;
	pDeflater->OutputData = pData;
	pDeflater->OutputFailure = false;
	pDeflater->Writing = true;
	bSuccess = __xrtWsDeflaterEmit(
		pDeflater,
		Data
	);
	pDeflater->Writing = false;
	pDeflater->Output = NULL;
	pDeflater->OutputData = NULL;
	if ( !bSuccess ) {
		pDeflater->Failed = true;
	}
	return bSuccess;
}



/*
	建立 Deflate 同步边界；中间边界保留在线路中，最终边界只剥离固定尾部。
	调用方据此保证每个流式帧之后编码器都不再暂存待发布负载。
*/
static bool __xrtWsDeflaterSync(
	xwsdeflater* pDeflater,
	xwsoutputproc pOutput,
	ptr pData,
	bool bFinal
)
{
	if ( !__xrtWsDeflaterCodecWrite(
		pDeflater,
		(xbytesview){ NULL, 0 },
		XDEFLATE_FLUSH_SYNC,
		pOutput,
		pData
	) ) {
		return false;
	}
	if ( (pDeflater->TailSize !=
		  sizeof(__g_xrtWsDeflateTail)) ||
		(memcmp(
			pDeflater->Tail,
			__g_xrtWsDeflateTail,
			sizeof(__g_xrtWsDeflateTail)
		 ) != 0) ) {
		__xrtWsDeflateError(
			XERR_INTERNAL,
			XWS_DEFLATE_ERROR_CODEC,
			bFinal ? "deflater-end" : "deflater-flush",
			"Deflate encoder produced an invalid sync tail"
		);
		pDeflater->Failed = true;
		return false;
	}
	if ( !bFinal && !__xrtWsDeflaterPublish(
		pDeflater,
		(xbytesview) {
			pDeflater->Tail,
			pDeflater->TailSize
		},
		pOutput,
		pData
	) ) {
		return false;
	}
	pDeflater->TailSize = 0;
	return true;
}



/* 初始化发送运行时默认配置。 */
XRT_API void xrtWsDeflaterConfigInit(
	xwsdeflaterconfig* pConfig
)
{
	xwsdeflaterconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"deflater-config-init",
			"WebSocket Deflater config range is invalid"
		);
		return;
	}
	__xrtWsDeflaterConfigDefault(&Config);
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 应用经过协商层解释的单向窗口和上下文参数。 */
XRT_API bool xrtWsDeflaterConfigApply(
	xwsdeflaterconfig* pConfig,
	const xwsdeflatedirection* pDirection
)
{
	xwsdeflaterconfig Config;
	xwsdeflatedirection Direction;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ||
		!__xrtRangeValid(pDirection, sizeof(Direction)) ||
		__xrtRangesOverlap(
			pConfig,
			sizeof(Config),
			pDirection,
			sizeof(Direction)
		) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_CONFIG,
			"deflater-config-apply",
			"invalid WebSocket Deflater configuration ranges"
		);
		return false;
	}
	memcpy(&Config, pConfig, sizeof(Config));
	memcpy(&Direction, pDirection, sizeof(Direction));
	if ( !__xrtWsDeflaterConfigValueValid(&Config) ||
		(Direction.WindowBits <
		 XWS_DEFLATE_WINDOW_MIN) ||
		(Direction.WindowBits >
		 XWS_DEFLATE_WINDOW_MAX) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_CONFIG,
			"deflater-config-apply",
			"invalid WebSocket Deflater direction"
		);
		return false;
	}
	Config.WindowBits = Direction.WindowBits;
	Config.NoContextTakeover = Direction.NoContextTakeover;
	memcpy(pConfig, &Config, sizeof(Config));
	return true;
}



/* 复位发送变换到一条新连接。 */
XRT_API bool xrtWsDeflaterReset(
	xwsdeflater* pDeflater,
	const xwsdeflaterconfig* pConfig
)
{
	xwsdeflaterconfig Config;
	xdeflateconfig DeflateConfig;

	if ( !__xrtRangeValid(pDeflater, sizeof(*pDeflater)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"deflater-reset",
			"WebSocket Deflater range is invalid"
		);
		return false;
	}
	if ( pDeflater->Writing ||
		(pDeflater->Active && !pDeflater->Failed) ) {
		__xrtWsDeflateError(
			XERR_STATE,
			XWS_DEFLATE_ERROR_STATE,
			"deflater-reset",
			"WebSocket Deflater is processing a message"
		);
		return false;
	}
	if ( !__xrtWsDeflaterConfigRead(pConfig, &Config) ) {
		__xrtWsDeflateError(
			XERR_VALUE,
			XWS_DEFLATE_ERROR_CONFIG,
			"deflater-reset",
			"invalid WebSocket Deflater config"
		);
		return false;
	}
	if ( pDeflater->Deflate != NULL ) {
		__xrtWsDeflaterCodecConfig(
			&Config,
			&DeflateConfig
		);
		if ( !xrtDeflateReset(
			pDeflater->Deflate,
			&DeflateConfig
		) ) {
			__xrtWsDeflateWrap(
				XERR_INTERNAL,
				XWS_DEFLATE_ERROR_CODEC,
				"deflater-reset",
				"failed to reset WebSocket Deflate state"
			);
			return false;
		}
	}
	pDeflater->Config = Config;
	pDeflater->Output = NULL;
	pDeflater->OutputData = NULL;
	pDeflater->Size = 0;
	pDeflater->TailSize = 0;
	pDeflater->Active = false;
	pDeflater->Compressed = false;
	pDeflater->OutputFailure = false;
	pDeflater->Failed = false;
	return true;
}



/* 创建不预先分配算法状态的发送变换。 */
XRT_API xwsdeflater* xrtWsDeflaterCreate(
	const xwsdeflaterconfig* pConfig
)
{
	xwsdeflaterconfig Config;
	xwsdeflater* pDeflater =
		NULL;

	if ( !__xrtWsDeflaterConfigRead(pConfig, &Config) ) {
		__xrtWsDeflateError(
			XERR_VALUE,
			XWS_DEFLATE_ERROR_CONFIG,
			"deflater-create",
			"invalid WebSocket Deflater config"
		);
		return NULL;
	}
	pDeflater = (xwsdeflater*)xrtMalloc(sizeof(*pDeflater));

	if ( !__xrtRangeValid(pDeflater, sizeof(*pDeflater)) ) {
		return NULL;
	}
	memset(pDeflater, 0, sizeof(*pDeflater));
	if ( !xrtWsDeflaterReset(pDeflater, &Config) ) {
		xrtFree(pDeflater);
		return NULL;
	}
	return pDeflater;
}



/* 开始压缩或直通的数据消息。 */
XRT_API bool xrtWsDeflaterBegin(
	xwsdeflater* pDeflater,
	bool bCompressed
)
{
	if ( !__xrtRangeValid(pDeflater, sizeof(*pDeflater)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"deflater-begin",
			"WebSocket Deflater range is invalid"
		);
		return false;
	}
	if ( pDeflater->Writing ||
		pDeflater->Active ||
		pDeflater->Failed ) {
		__xrtWsDeflateError(
			XERR_STATE,
			XWS_DEFLATE_ERROR_STATE,
			"deflater-begin",
			"WebSocket Deflater is busy or failed"
		);
		return false;
	}
	if ( bCompressed &&
		(pDeflater->Deflate == NULL) &&
		!__xrtWsDeflaterCodecReset(pDeflater) ) {
		__xrtWsDeflateWrap(
			XERR_MEMORY,
			XWS_DEFLATE_ERROR_CODEC,
			"deflater-begin",
			"failed to create WebSocket Deflate state"
		);
		pDeflater->Failed = true;
		return false;
	}
	pDeflater->Size = 0;
	pDeflater->TailSize = 0;
	pDeflater->Compressed = bCompressed;
	pDeflater->Active = true;
	return true;
}



/* 推进语义输入或直接发布未压缩负载。 */
XRT_API bool xrtWsDeflaterWrite(
	xwsdeflater* pDeflater,
	xbytesview Input,
	xwsoutputproc pOutput,
	ptr pData
)
{
	if ( !__xrtRangeValid(pDeflater, sizeof(*pDeflater)) ||
		!__xrtWsDeflaterViewValid(Input) ||
		__xrtRangesOverlap(
			pDeflater,
			sizeof(*pDeflater),
			Input.Data,
			Input.Size
		) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"deflater-write",
			"invalid WebSocket Deflater input"
		);
		return false;
	}
	if ( pDeflater->Writing ||
		!pDeflater->Active ||
		pDeflater->Failed ) {
		__xrtWsDeflateError(
			XERR_STATE,
			XWS_DEFLATE_ERROR_STATE,
			"deflater-write",
			"WebSocket Deflater has no active message"
		);
		return false;
	}
	if ( pDeflater->Compressed ) {
		return __xrtWsDeflaterCodecWrite(
			pDeflater,
			Input,
			XDEFLATE_FLUSH_NONE,
			pOutput,
			pData
		);
	}

	return __xrtWsDeflaterPublish(
		pDeflater,
		Input,
		pOutput,
		pData
	);
}



/* 发布一个可继续写入的完整同步边界。 */
XRT_API bool xrtWsDeflaterFlush(
	xwsdeflater* pDeflater,
	xwsoutputproc pOutput,
	ptr pData
)
{
	if ( !__xrtRangeValid(pDeflater, sizeof(*pDeflater)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"deflater-flush",
			"WebSocket Deflater range is invalid"
		);
		return false;
	}
	if ( pDeflater->Writing ||
		!pDeflater->Active ||
		pDeflater->Failed ) {
		__xrtWsDeflateError(
			XERR_STATE,
			XWS_DEFLATE_ERROR_STATE,
			"deflater-flush",
			"WebSocket Deflater has no active message"
		);
		return false;
	}
	if ( !pDeflater->Compressed ) {
		return true;
	}
	return __xrtWsDeflaterSync(
		pDeflater,
		pOutput,
		pData,
		false
	);
}



/* 放弃当前消息并从干净的发送状态继续。 */
XRT_API bool xrtWsDeflaterAbort(
	xwsdeflater* pDeflater
)
{
	xwsdeflaterconfig Config;

	if ( !__xrtRangeValid(pDeflater, sizeof(*pDeflater)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"deflater-abort",
			"WebSocket Deflater range is invalid"
		);
		return false;
	}
	if ( pDeflater->Writing ) {
		__xrtWsDeflateError(
			XERR_STATE,
			XWS_DEFLATE_ERROR_STATE,
			"deflater-abort",
			"WebSocket Deflater cannot abort from its output callback"
		);
		return false;
	}
	Config = pDeflater->Config;
	pDeflater->Failed = true;
	return xrtWsDeflaterReset(
		pDeflater,
		&Config
	);
}



/* 建立消息同步边界，剥离固定尾部并按协商策略处理上下文。 */
XRT_API bool xrtWsDeflaterEnd(
	xwsdeflater* pDeflater,
	xwsoutputproc pOutput,
	ptr pData
)
{
	if ( !__xrtRangeValid(pDeflater, sizeof(*pDeflater)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"deflater-end",
			"WebSocket Deflater range is invalid"
		);
		return false;
	}
	if ( pDeflater->Writing ||
		!pDeflater->Active ||
		pDeflater->Failed ) {
		__xrtWsDeflateError(
			XERR_STATE,
			XWS_DEFLATE_ERROR_STATE,
			"deflater-end",
			"WebSocket Deflater has no active message"
		);
		return false;
	}
	if ( pDeflater->Compressed ) {
		if ( !__xrtWsDeflaterSync(
			pDeflater,
			pOutput,
			pData,
			true
		) ) {
			return false;
		}
		if ( pDeflater->Size == 0u ) {
			static const uint8 Empty = 0x00u;

			if ( !__xrtWsDeflaterPublish(
				pDeflater,
				(xbytesview){ &Empty, 1u },
				pOutput,
				pData
			) ) {
				return false;
			}
		}
		if ( pDeflater->Config.NoContextTakeover ) {
			if ( pDeflater->Config.Retain ) {
				if ( !__xrtWsDeflaterCodecReset(pDeflater) ) {
					__xrtWsDeflateWrap(
						XERR_INTERNAL,
						XWS_DEFLATE_ERROR_CODEC,
						"deflater-end",
						"failed to reset WebSocket Deflate state"
					);
					pDeflater->Failed = true;
					return false;
				}
			} else {
				xrtDeflateDestroy(pDeflater->Deflate);
				pDeflater->Deflate = NULL;
			}
		}
	}
	pDeflater->Active = false;
	pDeflater->Compressed = false;
	return true;
}



/* 计算 miniz 任意策略加一次同步边界的保守输出硬上界。 */
XRT_API bool xrtWsDeflaterBound(
	size_t iInputSize,
	size_t* pOutputSize
)
{
	size_t iEighth = iInputSize / 8u;
	size_t iSixtyFourth = iInputSize / 64u;
	size_t iBound;

	if ( !__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"bound-deflater-output",
			"WebSocket Deflater output size range is invalid"
		);
		return false;
	}
	if ( (iInputSize % 8u) != 0 ) {
		iEighth++;
	}
	if ( (iInputSize % 64u) != 0 ) {
		iSixtyFourth++;
	}
	if ( (iInputSize > (SIZE_MAX - iEighth)) ||
		((iInputSize + iEighth) >
		 (SIZE_MAX - iSixtyFourth)) ||
		((iInputSize + iEighth + iSixtyFourth) >
		 (SIZE_MAX - 64u)) ) {
		__xrtWsDeflateError(
			XERR_RANGE,
			XWS_DEFLATE_ERROR_LIMIT,
			"bound-deflater-output",
			"WebSocket Deflater output bound overflowed"
		);
		return false;
	}
	iBound = iInputSize + iEighth +
		iSixtyFourth + 64u;
	memcpy(pOutputSize, &iBound, sizeof(iBound));
	return true;
}



/* 查询单条消息已经交付的线路长度。 */
XRT_API uint64 xrtWsDeflaterSize(
	const xwsdeflater* pDeflater
)
{
	if ( !__xrtRangeValid(pDeflater, sizeof(*pDeflater)) ) {
		return 0;
	}
	return pDeflater->Size;
}



/* 销毁发送变换及其按需算法状态。 */
XRT_API void xrtWsDeflaterDestroy(
	xwsdeflater* pDeflater
)
{
	if ( pDeflater == NULL ) {
		return;
	}
	if ( !__xrtRangeValid(pDeflater, sizeof(*pDeflater)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"deflater-destroy",
			"WebSocket Deflater range is invalid"
		);
		return;
	}
	if ( pDeflater->Writing ) {
		__xrtWsDeflateError(
			XERR_STATE,
			XWS_DEFLATE_ERROR_STATE,
			"deflater-destroy",
			"WebSocket Deflater cannot be destroyed from its output callback"
		);
		return;
	}
	xrtDeflateDestroy(pDeflater->Deflate);
	xrtFree(pDeflater);
}

#endif
