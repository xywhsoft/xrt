#include "../test.h"



/* 验证安全密钥生成边界、规范格式和输出失败原子性。 */
int main(void)
{
	char Key[XWS_KEY_CAPACITY];
	char Previous[XWS_KEY_CAPACITY];
	char Small[XWS_KEY_CAPACITY];
	bool bDifferent = false;

	memset(Previous, 0, sizeof(Previous));
	for ( size_t i = 0; i < 128u; i++ ) {
		testRequire(
			xrtWsKeyGenerate(Key, sizeof(Key)) &&
			(strlen(Key) == XWS_KEY_SIZE) &&
			xrtWsKeyValid((xstrview){ Key, XWS_KEY_SIZE }),
			"WebSocket secure key generation failed"
		);
		if ( (i != 0) &&
			(memcmp(Key, Previous, sizeof(Key)) != 0) ) {
			bDifferent = true;
		}
		memcpy(Previous, Key, sizeof(Key));
	}
	testRequire(
		bDifferent,
		"WebSocket secure key generator repeated every nonce"
	);

	memset(Small, 0xA5, sizeof(Small));
	testRequire(
		!xrtWsKeyGenerate(
			Small,
			XWS_KEY_CAPACITY - 1u
		) &&
		((unsigned char)Small[0] == UINT8_C(0xA5)),
		"WebSocket key capacity failure was not atomic"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"xrt.websocket.handshake"
		) == 0) &&
		(xrtErrorCode(xrtGetError()) ==
		 (int32)XWS_HANDSHAKE_ERROR_OUTPUT),
		"WebSocket key capacity error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtWsKeyGenerate(NULL, XWS_KEY_CAPACITY) &&
		(xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 (int32)XWS_HANDSHAKE_ERROR_ARGUMENT),
		"WebSocket key argument error mismatch"
	);
	printf("[PASS] websocket_keygen\n");
	return 0;
}
