#include "../internal/xrt_internal.h"
#include <xrt/logger.h>



#if defined(XRT_FEATURE_LOGGER_FORMAT_TEXT_BUFFER)

/* 把格式器分段输出追加到连续缓冲。 */
static bool __xrtLogTextBufferWrite(xbytesview Data, ptr pUserData)
{
	return xrtBufferAppend((xbuffer*)pUserData, Data);
}



/* 创建调用方拥有的完整文本记录。 */
XRT_API str xrtLogText(
	const xlogrecord* pRecord,
	const xlogtextconfig* pConfig,
	size_t* pSize
)
{
	xbuffer Buffer;
	bytes pData;
	size_t iSize;
	size_t iCapacity;

	if ( pSize != NULL ) {
		*pSize = 0;
	}
	if ( !xrtBufferInit(&Buffer) ) {
		return NULL;
	}
	if (
		!xrtLogTextWrite(
			pRecord,
			pConfig,
			__xrtLogTextBufferWrite,
			&Buffer,
			NULL
		) ||
		!xrtBufferAppendByte(&Buffer, 0)
	) {
		xrtBufferUnit(&Buffer);
		return NULL;
	}
	pData = xrtBufferTake(&Buffer, &iSize, &iCapacity);
	(void)iCapacity;
	if ( pData == NULL ) {
		return NULL;
	}
	if ( pSize != NULL ) {
		*pSize = iSize - 1u;
	}
	return (str)pData;
}

#endif
