#include "../internal/xrt_net.h"



#if defined(XRT_FEATURE_NET_FRAME_LENGTH)

#define XRT_NET_LENGTH_GUARD UINT32_C(0x4C454E47)



/* 按配置字节序读取 1 到 8 字节无符号长度。 */
static uint64 __xrtNetLengthRead(
	const uint8* pData,
	size_t iSize,
	xnetframeorder Order
)
{
	uint64 iValue = 0;

	if ( Order == XNET_FRAME_BIG_ENDIAN ) {
		for ( size_t i = 0; i < iSize; i++ ) {
			iValue = (iValue << 8u) | pData[i];
		}
	} else {
		for ( size_t i = 0; i < iSize; i++ ) {
			iValue |= (uint64)pData[i] << (i * 8u);
		}
	}
	return iValue;
}



/* 报告长度字段无法形成合法本机帧长度。 */
static xnetframestatus __xrtNetLengthError(
	xerrkind Kind,
	xneterror Code,
	cstr sMessage
)
{
	__xrtNetSetError(Kind, Code, "length frame", sMessage, 0);
	return XNET_FRAME_ERROR;
}



/* 初始化常见的四字节网络序 payload 长度前缀。 */
XRT_API void xrtNetLengthConfigInit(xnetlengthconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->LengthSize = 4;
	pConfig->Strip = 4;
	pConfig->MaxFrame = 1024u * 1024u;
	pConfig->Order = XNET_FRAME_BIG_ENDIAN;
}



/* 预先拒绝字段末端溢出、非法字节序和无效硬上限。 */
XRT_API bool xrtNetLengthInit(
	xnetlengthframer* pFramer,
	const xnetlengthconfig* pConfig
)
{
	xnetlengthframer Next;

	if ( (pFramer == NULL) || (pConfig == NULL) ||
		 (pConfig->LengthSize == 0) ||
		 (pConfig->LengthSize > 8) ||
		 (pConfig->LengthOffset >
			(SIZE_MAX - pConfig->LengthSize)) ||
		 (pConfig->MaxFrame == 0) ||
		 (pConfig->Strip > pConfig->MaxFrame) ||
		 ((pConfig->LengthOffset + pConfig->LengthSize) >
			pConfig->MaxFrame) ||
		 ((pConfig->Order != XNET_FRAME_BIG_ENDIAN) &&
		  (pConfig->Order != XNET_FRAME_LITTLE_ENDIAN)) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_FRAME_CONFIG,
			"length frame", "invalid length frame configuration", 0);
		return false;
	}
	memset(&Next, 0, sizeof(Next));
	Next.Config = *pConfig;
	Next.Guard = XRT_NET_LENGTH_GUARD;
	*pFramer = Next;
	return true;
}



/* 安全组合字段末端、声明长度和有符号调整，再等待完整帧。 */
XRT_API xnetframestatus xrtNetLengthNext(
	const xnetlengthframer* pFramer,
	const xnetbuf* pInput,
	xnetframe* pFrame
)
{
	uint8 arrLength[8];
	size_t iFieldEnd;
	uint64 iDeclared;
	uint64 iFrame;

	if ( (pFramer == NULL) || (pInput == NULL) || (pFrame == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_FRAME_ERROR;
	}
	if ( pFramer->Guard != XRT_NET_LENGTH_GUARD ) {
		return __xrtNetLengthError(XERR_STATE,
			XNET_ERROR_FRAME_STATE, "invalid length framer state");
	}
	memset(pFrame, 0, sizeof(*pFrame));
	iFieldEnd = pFramer->Config.LengthOffset +
		pFramer->Config.LengthSize;
	if ( xrtNetBufSize(pInput) < iFieldEnd ) {
		return XNET_FRAME_MORE;
	}
	if ( xrtNetBufPeek(
			pInput, pFramer->Config.LengthOffset,
			arrLength, pFramer->Config.LengthSize
		) != pFramer->Config.LengthSize ) {
		return __xrtNetLengthError(XERR_STATE,
			XNET_ERROR_FRAME_STATE, "length field is outside the input");
	}
	iDeclared = __xrtNetLengthRead(
		arrLength, pFramer->Config.LengthSize, pFramer->Config.Order
	);
	if ( iDeclared > (UINT64_MAX - (uint64)iFieldEnd) ) {
		return __xrtNetLengthError(XERR_RANGE,
			XNET_ERROR_FRAME_LENGTH, "declared frame length overflows");
	}
	iFrame = (uint64)iFieldEnd + iDeclared;
	if ( pFramer->Config.Adjustment >= 0 ) {
		uint64 iAdjustment = (uint64)pFramer->Config.Adjustment;

		if ( iFrame > (UINT64_MAX - iAdjustment) ) {
			return __xrtNetLengthError(XERR_RANGE,
				XNET_ERROR_FRAME_LENGTH, "adjusted frame length overflows");
		}
		iFrame += iAdjustment;
	} else {
		uint64 iAdjustment =
			(uint64)(-(pFramer->Config.Adjustment + 1)) + 1u;

		if ( iFrame < iAdjustment ) {
			return __xrtNetLengthError(XERR_VALUE,
				XNET_ERROR_FRAME_LENGTH, "adjusted frame length is negative");
		}
		iFrame -= iAdjustment;
	}
	if ( (iFrame > SIZE_MAX) ||
		 ((size_t)iFrame < iFieldEnd) ||
		 (pFramer->Config.Strip > (size_t)iFrame) ) {
		return __xrtNetLengthError(XERR_RANGE,
			XNET_ERROR_FRAME_LENGTH, "length frame boundaries are invalid");
	}
	if ( (size_t)iFrame > pFramer->Config.MaxFrame ) {
		return __xrtNetLengthError(XERR_RANGE,
			XNET_ERROR_FRAME_LIMIT, "frame exceeds configured limit");
	}
	if ( xrtNetBufSize(pInput) < (size_t)iFrame ) {
		return XNET_FRAME_MORE;
	}
	pFrame->PayloadOffset = pFramer->Config.Strip;
	pFrame->PayloadSize = (size_t)iFrame - pFramer->Config.Strip;
	pFrame->FrameSize = (size_t)iFrame;
	pFrame->Declared = iDeclared;
	return XNET_FRAME_READY;
}



#undef XRT_NET_LENGTH_GUARD

#endif
