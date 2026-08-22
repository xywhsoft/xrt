#include "../internal/xrt_internal.h"



#if defined(XRT_FEATURE_BUFFER_HEX)

/* 严格解码 HEX 文本并创建缓冲。 */
XRT_API xbuffer* xrtBufferFromHex(xstrview Text, uint32 iFlags)
{
	xbuffer* pBuffer;
	size_t iSize;

	if ( !xrtHexDecode(Text, NULL, 0, &iSize, iFlags) ) {
		return NULL;
	}
	pBuffer = xrtBufferCreate();
	if ( pBuffer == NULL ) {
		return NULL;
	}
	if ( (iSize != 0) &&
		(!xrtBufferResize(pBuffer, iSize) ||
		 !xrtHexDecode(
			Text,
			pBuffer->Data,
			pBuffer->Size,
			&iSize,
			iFlags
		)) ) {
		xrtBufferDestroy(pBuffer);
		return NULL;
	}
	return pBuffer;
}

#endif



#if defined(XRT_FEATURE_BUFFER_BASE64)

/* 按 Base64 配置严格解码文本并创建缓冲。 */
XRT_API xbuffer* xrtBufferFromBase64(
	xstrview Text,
	const xbase64config* pConfig
)
{
	xbuffer* pBuffer;
	size_t iSize;

	if ( !xrtBase64Decode(
		Text.Data,
		Text.Size,
		NULL,
		0,
		&iSize,
		pConfig
	) ) {
		return NULL;
	}
	pBuffer = xrtBufferCreate();
	if ( pBuffer == NULL ) {
		return NULL;
	}
	if ( (iSize != 0) &&
		(!xrtBufferResize(pBuffer, iSize) ||
		 !xrtBase64Decode(
			Text.Data,
			Text.Size,
			pBuffer->Data,
			pBuffer->Size,
			&iSize,
			pConfig
		)) ) {
		xrtBufferDestroy(pBuffer);
		return NULL;
	}
	return pBuffer;
}

#endif
