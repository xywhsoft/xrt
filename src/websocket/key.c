#include "../internal/xrt_websocket.h"



#if defined(XRT_FEATURE_WEBSOCKET_KEYGEN)

/* 使用安全随机 nonce 生成规范客户端握手密钥，并延迟提交输出。 */
XRT_API bool xrtWsKeyGenerate(
	char* sKey,
	size_t iCapacity
)
{
	uint8 Random[XWS_KEY_BYTES];
	char Key[XWS_KEY_CAPACITY];
	size_t iOutputSize = 0;
	bool bResult;

	if ( sKey == NULL ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"generate-websocket-key",
			"invalid WebSocket key output"
		);
		return false;
	}
	if ( iCapacity < XWS_KEY_CAPACITY ) {
		__xrtWsHandshakeError(
			XERR_RANGE,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"generate-websocket-key",
			"WebSocket key output is too small"
		);
		return false;
	}
	if ( !xrtSecureRandom(Random, sizeof(Random)) ) {
		__xrtWsHandshakeWrap(
			XERR_IO,
			XWS_HANDSHAKE_ERROR_RANDOM,
			"generate-websocket-key",
			"failed to generate WebSocket nonce"
		);
		xrtSecureZero(Random, sizeof(Random));
		return false;
	}
	bResult = xrtBase64Encode(
		Random,
		sizeof(Random),
		Key,
		sizeof(Key),
		&iOutputSize,
		NULL
	) && (iOutputSize == XWS_KEY_SIZE);
	xrtSecureZero(Random, sizeof(Random));
	if ( !bResult ) {
		__xrtWsHandshakeWrap(
			XERR_INTERNAL,
			XWS_HANDSHAKE_ERROR_KEY,
			"generate-websocket-key",
			"failed to encode WebSocket nonce"
		);
		xrtSecureZero(Key, sizeof(Key));
		return false;
	}
	memcpy(sKey, Key, sizeof(Key));
	xrtSecureZero(Key, sizeof(Key));
	return true;
}

#endif
