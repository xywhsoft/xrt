#include "../internal/xrt_websocket.h"



#if defined(XRT_FEATURE_WEBSOCKET_INFLATER)

#define __XRT_WS_INFLATE_TAIL_SIZE 4u



/* 接收变换只保存消息状态，底层 Inflate 在首条压缩消息到来时才创建。 */
struct xwsinflater {
	xwsinflaterconfig Config;
	xinflate* Inflate;
	xwsoutputproc Output;
	ptr OutputData;
	uint64 Size;
	bool Active;
	bool Compressed;
	bool Writing;
	bool OutputFailure;
	bool Failed;
};



/* 初始化已经对齐的接收运行时默认配置。 */
static void __xrtWsInflaterConfigDefault(
	xwsinflaterconfig* pConfig
)
{
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->OutputLimit = XWS_INFLATE_OUTPUT_DEFAULT;
	pConfig->WindowBits = XWS_DEFLATE_WINDOW_MAX;
}



/* 验证已经对齐的运行时窗口配置。 */
static bool __xrtWsInflaterConfigValueValid(
	const xwsinflaterconfig* pConfig
)
{
	return (pConfig->WindowBits >= XWS_DEFLATE_WINDOW_MIN) &&
		(pConfig->WindowBits <= XWS_DEFLATE_WINDOW_MAX);
}



/* 读取可选且可能未对齐的配置，空配置使用默认值。 */
static bool __xrtWsInflaterConfigRead(
	const xwsinflaterconfig* pSource,
	xwsinflaterconfig* pConfig
)
{
	if ( pSource == NULL ) {
		__xrtWsInflaterConfigDefault(pConfig);
		return true;
	}
	if ( !__xrtRangeValid(pSource, sizeof(*pSource)) ) {
		return false;
	}
	memcpy(pConfig, pSource, sizeof(*pConfig));
	return __xrtWsInflaterConfigValueValid(pConfig);
}



/* 验证借用输入视图。 */
static bool __xrtWsInflaterViewValid(xbytesview Input)
{
	return __xrtRangeValid(Input.Data, Input.Size);
}



/* 构造底层 raw Inflate 配置。 */
static void __xrtWsInflaterCodecConfig(
	const xwsinflaterconfig* pConfig,
	xinflateconfig* pInflate
)
{
	xrtInflateConfigInit(pInflate);
	pInflate->Format = XINFLATE_RAW;
	pInflate->OutputLimit = XINFLATE_OUTPUT_UNLIMITED;
	pInflate->WindowBits = pConfig->WindowBits;
}



/* 创建或复位底层 Inflate，同时保留当前消息状态。 */
static bool __xrtWsInflaterCodecReset(
	xwsinflater* pInflater
)
{
	xinflateconfig Config;

	__xrtWsInflaterCodecConfig(
		&pInflater->Config,
		&Config
	);
	if ( pInflater->Inflate == NULL ) {
		pInflater->Inflate = xrtInflateCreate(&Config);
		return pInflater->Inflate != NULL;
	}
	return xrtInflateReset(pInflater->Inflate, &Config);
}



/* 在单消息硬上限内同步发布语义负载。 */
static bool __xrtWsInflaterOutput(
	xbytesview Data,
	ptr pData
)
{
	xwsinflater* pInflater = (xwsinflater*)pData;

	if ( (pInflater->Size >
		  pInflater->Config.OutputLimit) ||
		((uint64)Data.Size >
		 (pInflater->Config.OutputLimit -
		  pInflater->Size)) ) {
		__xrtWsDeflateError(
			XERR_RANGE,
			XWS_DEFLATE_ERROR_LIMIT,
			"inflate-output",
			"permessage-deflate output exceeds the message limit"
		);
		pInflater->OutputFailure = true;
		return false;
	}
	if ( pInflater->Output != NULL ) {
		xrtClearError();
		if ( !pInflater->Output(
			Data,
			pInflater->OutputData
		) ) {
			if ( xrtGetError() == NULL ) {
				__xrtWsDeflateError(
					XERR_CANCELLED,
					XWS_DEFLATE_ERROR_OUTPUT,
					"inflate-output",
					"permessage-deflate output callback stopped decoding"
				);
			}
			pInflater->OutputFailure = true;
			return false;
		}
	}
	pInflater->Size += (uint64)Data.Size;
	return true;
}



/* 完成一次底层写入，并把编解码错误包装到稳定 WebSocket 域。 */
static bool __xrtWsInflaterCodecWrite(
	xwsinflater* pInflater,
	xbytesview Input,
	xwsoutputproc pOutput,
	ptr pData
)
{
	bool bSuccess;

	pInflater->Output = pOutput;
	pInflater->OutputData = pData;
	pInflater->OutputFailure = false;
	pInflater->Writing = true;
	bSuccess = xrtInflateWrite(
		pInflater->Inflate,
		Input,
		false,
		__xrtWsInflaterOutput,
		pInflater
	);
	pInflater->Writing = false;
	pInflater->Output = NULL;
	pInflater->OutputData = NULL;
	if ( bSuccess ) {
		return true;
	}
	if ( !pInflater->OutputFailure ) {
		__xrtWsDeflateWrap(
			XERR_PROTOCOL,
			XWS_DEFLATE_ERROR_DATA,
			"inflate",
			"invalid permessage-deflate payload"
		);
	}
	pInflater->Failed = true;
	return false;
}



/* 初始化接收运行时默认配置。 */
XRT_API void xrtWsInflaterConfigInit(
	xwsinflaterconfig* pConfig
)
{
	xwsinflaterconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"inflater-config-init",
			"WebSocket Inflater config range is invalid"
		);
		return;
	}
	__xrtWsInflaterConfigDefault(&Config);
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 应用经过协商层解释的单向窗口和上下文参数。 */
XRT_API bool xrtWsInflaterConfigApply(
	xwsinflaterconfig* pConfig,
	const xwsdeflatedirection* pDirection
)
{
	xwsinflaterconfig Config;
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
			"inflater-config-apply",
			"invalid WebSocket Inflater configuration ranges"
		);
		return false;
	}
	memcpy(&Config, pConfig, sizeof(Config));
	memcpy(&Direction, pDirection, sizeof(Direction));
	if ( !__xrtWsInflaterConfigValueValid(&Config) ||
		(Direction.WindowBits <
		 XWS_DEFLATE_WINDOW_MIN) ||
		(Direction.WindowBits >
		 XWS_DEFLATE_WINDOW_MAX) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_CONFIG,
			"inflater-config-apply",
			"invalid WebSocket Inflater direction"
		);
		return false;
	}
	Config.WindowBits = Direction.WindowBits;
	Config.NoContextTakeover = Direction.NoContextTakeover;
	memcpy(pConfig, &Config, sizeof(Config));
	return true;
}



/* 复位接收变换到一条新连接。 */
XRT_API bool xrtWsInflaterReset(
	xwsinflater* pInflater,
	const xwsinflaterconfig* pConfig
)
{
	xwsinflaterconfig Config;
	xinflateconfig InflateConfig;

	if ( !__xrtRangeValid(pInflater, sizeof(*pInflater)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"inflater-reset",
			"WebSocket Inflater range is invalid"
		);
		return false;
	}
	if ( pInflater->Writing ||
		(pInflater->Active && !pInflater->Failed) ) {
		__xrtWsDeflateError(
			XERR_STATE,
			XWS_DEFLATE_ERROR_STATE,
			"inflater-reset",
			"WebSocket Inflater is processing a message"
		);
		return false;
	}
	if ( !__xrtWsInflaterConfigRead(pConfig, &Config) ) {
		__xrtWsDeflateError(
			XERR_VALUE,
			XWS_DEFLATE_ERROR_CONFIG,
			"inflater-reset",
			"invalid WebSocket Inflater config"
		);
		return false;
	}
	if ( pInflater->Inflate != NULL ) {
		__xrtWsInflaterCodecConfig(
			&Config,
			&InflateConfig
		);
		if ( !xrtInflateReset(
			pInflater->Inflate,
			&InflateConfig
		) ) {
			__xrtWsDeflateWrap(
				XERR_INTERNAL,
				XWS_DEFLATE_ERROR_CODEC,
				"inflater-reset",
				"failed to reset WebSocket Inflate state"
			);
			return false;
		}
	}
	pInflater->Config = Config;
	pInflater->Output = NULL;
	pInflater->OutputData = NULL;
	pInflater->Size = 0;
	pInflater->Active = false;
	pInflater->Compressed = false;
	pInflater->OutputFailure = false;
	pInflater->Failed = false;
	return true;
}



/* 创建不预先分配算法窗口的接收变换。 */
XRT_API xwsinflater* xrtWsInflaterCreate(
	const xwsinflaterconfig* pConfig
)
{
	xwsinflaterconfig Config;
	xwsinflater* pInflater =
		NULL;

	if ( !__xrtWsInflaterConfigRead(pConfig, &Config) ) {
		__xrtWsDeflateError(
			XERR_VALUE,
			XWS_DEFLATE_ERROR_CONFIG,
			"inflater-create",
			"invalid WebSocket Inflater config"
		);
		return NULL;
	}
	pInflater = (xwsinflater*)xrtMalloc(sizeof(*pInflater));

	if ( pInflater == NULL ) {
		return NULL;
	}
	memset(pInflater, 0, sizeof(*pInflater));
	if ( !xrtWsInflaterReset(pInflater, &Config) ) {
		xrtFree(pInflater);
		return NULL;
	}
	return pInflater;
}



/* 开始压缩或直通的数据消息。 */
XRT_API bool xrtWsInflaterBegin(
	xwsinflater* pInflater,
	bool bCompressed
)
{
	if ( !__xrtRangeValid(pInflater, sizeof(*pInflater)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"inflater-begin",
			"WebSocket Inflater range is invalid"
		);
		return false;
	}
	if ( pInflater->Writing ||
		pInflater->Active ||
		pInflater->Failed ) {
		__xrtWsDeflateError(
			XERR_STATE,
			XWS_DEFLATE_ERROR_STATE,
			"inflater-begin",
			"WebSocket Inflater is busy or failed"
		);
		return false;
	}
	if ( bCompressed &&
		(pInflater->Inflate == NULL) &&
		!__xrtWsInflaterCodecReset(pInflater) ) {
		__xrtWsDeflateWrap(
			XERR_MEMORY,
			XWS_DEFLATE_ERROR_CODEC,
			"inflater-begin",
			"failed to create WebSocket Inflate state"
		);
		pInflater->Failed = true;
		return false;
	}
	pInflater->Size = 0;
	pInflater->Compressed = bCompressed;
	pInflater->Active = true;
	return true;
}



/* 推进线路输入或直接发布未压缩负载。 */
XRT_API bool xrtWsInflaterWrite(
	xwsinflater* pInflater,
	xbytesview Input,
	xwsoutputproc pOutput,
	ptr pData
)
{
	bool bSuccess;

	if ( !__xrtRangeValid(pInflater, sizeof(*pInflater)) ||
		!__xrtWsInflaterViewValid(Input) ||
		__xrtRangesOverlap(
			pInflater,
			sizeof(*pInflater),
			Input.Data,
			Input.Size
		) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"inflater-write",
			"invalid WebSocket Inflater input"
		);
		return false;
	}
	if ( pInflater->Writing ||
		!pInflater->Active ||
		pInflater->Failed ) {
		__xrtWsDeflateError(
			XERR_STATE,
			XWS_DEFLATE_ERROR_STATE,
			"inflater-write",
			"WebSocket Inflater has no active message"
		);
		return false;
	}
	if ( pInflater->Compressed ) {
		return __xrtWsInflaterCodecWrite(
			pInflater,
			Input,
			pOutput,
			pData
		);
	}

	pInflater->Output = pOutput;
	pInflater->OutputData = pData;
	pInflater->OutputFailure = false;
	pInflater->Writing = true;
	bSuccess = __xrtWsInflaterOutput(
		Input,
		pInflater
	);
	pInflater->Writing = false;
	pInflater->Output = NULL;
	pInflater->OutputData = NULL;
	if ( !bSuccess ) {
		pInflater->Failed = true;
	}
	return bSuccess;
}



/* 提交同步尾部并根据协商策略复位或释放上下文。 */
XRT_API bool xrtWsInflaterEnd(
	xwsinflater* pInflater,
	xwsoutputproc pOutput,
	ptr pData
)
{
	static const uint8 Tail[__XRT_WS_INFLATE_TAIL_SIZE] = {
		0x00u, 0x00u, 0xFFu, 0xFFu
	};

	if ( !__xrtRangeValid(pInflater, sizeof(*pInflater)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"inflater-end",
			"WebSocket Inflater range is invalid"
		);
		return false;
	}
	if ( pInflater->Writing ||
		!pInflater->Active ||
		pInflater->Failed ) {
		__xrtWsDeflateError(
			XERR_STATE,
			XWS_DEFLATE_ERROR_STATE,
			"inflater-end",
			"WebSocket Inflater has no active message"
		);
		return false;
	}
	if ( pInflater->Compressed ) {
		if ( !__xrtWsInflaterCodecWrite(
			pInflater,
			(xbytesview){ Tail, sizeof(Tail) },
			pOutput,
			pData
		) ) {
			return false;
		}
		if ( xrtInflateDone(pInflater->Inflate) ) {
			__xrtWsDeflateError(
				XERR_PROTOCOL,
				XWS_DEFLATE_ERROR_DATA,
				"inflater-end",
				"permessage-deflate payload ended its DEFLATE stream"
			);
			pInflater->Failed = true;
			return false;
		}
		if ( pInflater->Config.NoContextTakeover ) {
			if ( pInflater->Config.Retain ) {
				if ( !__xrtWsInflaterCodecReset(pInflater) ) {
					__xrtWsDeflateWrap(
						XERR_INTERNAL,
						XWS_DEFLATE_ERROR_CODEC,
						"inflater-end",
						"failed to reset WebSocket Inflate state"
					);
					pInflater->Failed = true;
					return false;
				}
			} else {
				xrtInflateDestroy(pInflater->Inflate);
				pInflater->Inflate = NULL;
			}
		}
	}
	pInflater->Active = false;
	pInflater->Compressed = false;
	return true;
}



/* 查询单条消息已经交付的语义长度。 */
XRT_API uint64 xrtWsInflaterSize(
	const xwsinflater* pInflater
)
{
	if ( !__xrtRangeValid(pInflater, sizeof(*pInflater)) ) {
		return 0;
	}
	return pInflater->Size;
}



/* 销毁接收变换及其按需算法窗口。 */
XRT_API void xrtWsInflaterDestroy(
	xwsinflater* pInflater
)
{
	if ( pInflater == NULL ) {
		return;
	}
	if ( !__xrtRangeValid(pInflater, sizeof(*pInflater)) ) {
		__xrtWsDeflateError(
			XERR_ARGUMENT,
			XWS_DEFLATE_ERROR_ARGUMENT,
			"inflater-destroy",
			"WebSocket Inflater range is invalid"
		);
		return;
	}
	if ( pInflater->Writing ) {
		__xrtWsDeflateError(
			XERR_STATE,
			XWS_DEFLATE_ERROR_STATE,
			"inflater-destroy",
			"WebSocket Inflater cannot be destroyed from its output callback"
		);
		return;
	}
	xrtInflateDestroy(pInflater->Inflate);
	xrtFree(pInflater);
}

#endif
