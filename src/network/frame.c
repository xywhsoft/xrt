#include "../internal/xrt_net.h"



#if defined(XRT_FEATURE_NET_FRAME)

/* 验证帧的 payload 与总长度都位于当前输入内部。 */
static bool __xrtNetFrameValidate(
	const xnetbuf* pInput,
	const xnetframe* pFrame
)
{
	if ( (pInput == NULL) || (pFrame == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pFrame->FrameSize == 0) ||
		 (pFrame->PayloadOffset > pFrame->FrameSize) ||
		 (pFrame->PayloadSize >
			(pFrame->FrameSize - pFrame->PayloadOffset)) ||
		 (pFrame->FrameSize > xrtNetBufSize(pInput)) ) {
		__xrtNetSetError(XERR_STATE, XNET_ERROR_FRAME_STATE,
			"frame", "frame is outside the current input", 0);
		return false;
	}
	return true;
}



/* 复用网络缓冲的跨块 Peek，并允许调用方按容量读取前缀。 */
XRT_API size_t xrtNetFrameCopy(
	const xnetbuf* pInput,
	const xnetframe* pFrame,
	void* pOutput,
	size_t iCapacity
)
{
	size_t iCopy;

	if ( (pOutput == NULL) && (iCapacity != 0) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( !__xrtNetFrameValidate(pInput, pFrame) ) {
		return 0;
	}
	iCopy = pFrame->PayloadSize;
	if ( iCopy > iCapacity ) {
		iCopy = iCapacity;
	}
	if ( iCopy == 0 ) {
		return 0;
	}
	return xrtNetBufPeek(
		pInput, pFrame->PayloadOffset, pOutput, iCopy
	);
}



/* 拒绝当前输入已无法容纳的帧，避免把输入尾部静默截断。 */
XRT_API bool xrtNetFrameConsume(
	xnetbuf* pInput,
	const xnetframe* pFrame
)
{
	if ( !__xrtNetFrameValidate(pInput, pFrame) ) {
		return false;
	}
	return xrtNetBufConsume(pInput, pFrame->FrameSize) ==
		pFrame->FrameSize;
}

#endif
