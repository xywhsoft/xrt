#include "../test.h"



/* 验证默认配置、空参数和不持有传输的查询边界。 */
int main(void)
{
	uint8 ConfigStorage[sizeof(xwsconnconfig) + 2u];
	xwsconnconfig Config;
	xwsconnclose Close;
	xwsconn* pWrapping = (xwsconn*)(uintptr_t)(UINTPTR_MAX - 1u);
	xstrview Protocol;
	#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		xwsdeflate Deflate;
	#endif

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtWsConnConfigInit(
		(xwsconnconfig*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire(
		(ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		xrtWsConnConfigValid(
			(const xwsconnconfig*)(const void*)(
				ConfigStorage + 1u
			)
		) &&
		(Config.Role == XWS_ROLE_SERVER) &&
		(Config.MessageLimit ==
		 XWS_CONN_MESSAGE_LIMIT_DEFAULT) &&
		(Config.FrameLimit ==
		 XWS_CONN_FRAME_LIMIT_DEFAULT) &&
		(Config.SendLimit ==
		 XWS_CONN_SEND_LIMIT_DEFAULT) &&
		(Config.ControlReserve ==
		 XWS_CONN_CONTROL_RESERVE_DEFAULT) &&
		(Config.CloseTimeout ==
		 XWS_CONN_CLOSE_TIMEOUT_DEFAULT) &&
		Config.AutoPong,
		"WebSocket connection defaults mismatch"
	);
	xrtClearError();
	xrtWsConnConfigInit((xwsconnconfig*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		!xrtWsConnConfigValid(
			(const xwsconnconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
		),
		"WebSocket connection config accepted a wrapping range"
	);
	xrtClearError();
	testRequire(
		(xrtWsConnAttach(
			NULL,
			&Config,
			NULL,
			NULL
		 ) == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT),
		"WebSocket connection accepted a null TCP Stream"
	);
	xrtClearError();
	memset(&Close, 0xA5, sizeof(Close));
	testRequire(
		!xrtWsConnCloseInfo(NULL, &Close) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT) &&
		(Close.Flags == UINT32_C(0xA5A5A5A5)),
		"WebSocket Close query accepted a null connection"
	);
	Protocol = xrtWsConnProtocol(NULL);
	testRequire(
		(xrtWsConnState(NULL) == XWS_CONN_CLOSED) &&
		(xrtWsConnRole(NULL) == XWS_ROLE_SERVER) &&
		(Protocol.Data == NULL) &&
		(Protocol.Size == 0) &&
		(xrtWsConnWorker(NULL) == NULL) &&
		(xrtWsConnTcp(NULL) == NULL) &&
		(xrtWsConnTcpRef(NULL) == NULL) &&
		(xrtWsConnPending(NULL) == 0) &&
		(xrtWsConnWritable(NULL) == 0) &&
		!xrtWsConnPaused(NULL) &&
		(xrtWsConnError(NULL) == NULL) &&
		!xrtWsConnAbort(NULL),
		"WebSocket null query contract mismatch"
	);
	xrtWsConnPause(NULL);
	xrtClearError();
	testRequire(
		!xrtWsConnResume(NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT),
		"WebSocket resume accepted a null connection"
	);

	xrtClearError();
	Protocol = xrtWsConnProtocol(pWrapping);
	testRequire(
		(xrtWsConnRef(pWrapping) == NULL) &&
		(xrtWsConnState(pWrapping) == XWS_CONN_CLOSED) &&
		(xrtWsConnRole(pWrapping) == XWS_ROLE_SERVER) &&
		(Protocol.Data == NULL) &&
		(Protocol.Size == 0) &&
		(xrtWsConnWorker(pWrapping) == NULL) &&
		(xrtWsConnTcp(pWrapping) == NULL) &&
		(xrtWsConnTcpRef(pWrapping) == NULL) &&
		(xrtWsConnPending(pWrapping) == 0) &&
		(xrtWsConnWritable(pWrapping) == 0) &&
		!xrtWsConnPaused(pWrapping) &&
		(xrtWsConnError(pWrapping) == NULL) &&
		!xrtWsConnAbort(pWrapping) &&
		!xrtWsConnResume(pWrapping) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT),
		"WebSocket connection queries accepted a wrapping range"
	);
	xrtWsConnPause(pWrapping);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT),
		"WebSocket connection pause accepted a wrapping range"
	);
	xrtWsConnDestroy(pWrapping);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT),
		"WebSocket connection destroy accepted a wrapping range"
	);
	memset(&Close, 0xA5, sizeof(Close));
	testRequire(
		!xrtWsConnCloseInfo(pWrapping, &Close) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT) &&
		(Close.Flags == UINT32_C(0xA5A5A5A5)),
		"WebSocket Close query accepted a wrapping connection"
	);
	#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_TLS)
		testRequire(
			(xrtWsConnTls(pWrapping) == NULL) &&
			(xrtWsConnTlsRef(pWrapping) == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
			(xrtErrorCode(xrtGetError()) ==
			 XWS_CONN_ERROR_ARGUMENT),
			"WebSocket TLS query accepted a wrapping connection"
		);
	#endif
	#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		memset(&Deflate, 0xA5, sizeof(Deflate));
		testRequire(
			!xrtWsConnDeflate(pWrapping, &Deflate) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
			(xrtErrorCode(xrtGetError()) ==
			 XWS_CONN_ERROR_ARGUMENT),
			"WebSocket Deflate query accepted a wrapping connection"
		);
	#endif
	return 0;
}
