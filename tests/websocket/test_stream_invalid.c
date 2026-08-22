#include "../test.h"



/* 验证默认配置、空参数和不持有传输的查询边界。 */
int main(void)
{
	uint8 ConfigStorage[sizeof(xwsstreamconfig) + 2u];
	xwsstreamconfig Config;
	xwsstreamclose Close;
	xwsstream* pWrapping = (xwsstream*)(uintptr_t)(UINTPTR_MAX - 1u);
	xstrview Protocol;
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		xwsdeflate Deflate;
	#endif

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtWsStreamConfigInit(
		(xwsstreamconfig*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire(
		(ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		xrtWsStreamConfigValid(
			(const xwsstreamconfig*)(const void*)(
				ConfigStorage + 1u
			)
		) &&
		(Config.Role == XWS_ROLE_SERVER) &&
		(Config.MessageLimit ==
		 XWS_STREAM_MESSAGE_LIMIT_DEFAULT) &&
		(Config.FrameLimit ==
		 XWS_STREAM_FRAME_LIMIT_DEFAULT) &&
		(Config.SendLimit ==
		 XWS_STREAM_SEND_LIMIT_DEFAULT) &&
		(Config.ControlReserve ==
		 XWS_STREAM_CONTROL_RESERVE_DEFAULT) &&
		(Config.CloseTimeout ==
		 XWS_STREAM_CLOSE_TIMEOUT_DEFAULT) &&
		Config.AutoPong,
		"WebSocket connection defaults mismatch"
	);
	xrtClearError();
	xrtWsStreamConfigInit((xwsstreamconfig*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		!xrtWsStreamConfigValid(
			(const xwsstreamconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
		),
		"WebSocket connection config accepted a wrapping range"
	);
	xrtClearError();
	testRequire(
		(xrtWsStreamAttach(
			NULL,
			0,
			&Config,
			NULL,
			NULL
		 ) == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_STREAM_ERROR_ARGUMENT),
		"WebSocket connection accepted a null TCP Stream"
	);
	xrtClearError();
	memset(&Close, 0xA5, sizeof(Close));
	testRequire(
		!xrtWsStreamCloseInfo(NULL, &Close) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_STREAM_ERROR_ARGUMENT) &&
		(Close.Flags == UINT32_C(0xA5A5A5A5)),
		"WebSocket Close query accepted a null connection"
	);
	Protocol = xrtWsStreamProtocol(NULL);
	testRequire(
		(xrtWsStreamState(NULL) == XWS_STREAM_CLOSED) &&
		(xrtWsStreamRole(NULL) == XWS_ROLE_SERVER) &&
		(Protocol.Data == NULL) &&
		(Protocol.Size == 0) &&
		(xrtWsStreamWorker(NULL) == NULL) &&
		(xrtWsStreamTcp(NULL) == NULL) &&
		(xrtWsStreamTcpRef(NULL) == NULL) &&
		(xrtWsStreamPending(NULL) == 0) &&
		(xrtWsStreamWritable(NULL) == 0) &&
		!xrtWsStreamPaused(NULL) &&
		(xrtWsStreamError(NULL) == NULL) &&
		!xrtWsStreamAbort(NULL),
		"WebSocket null query contract mismatch"
	);
	xrtWsStreamPause(NULL);
	xrtClearError();
	testRequire(
		!xrtWsStreamResume(NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_STREAM_ERROR_ARGUMENT),
		"WebSocket resume accepted a null connection"
	);

	xrtClearError();
	Protocol = xrtWsStreamProtocol(pWrapping);
	testRequire(
		(xrtWsStreamRef(pWrapping) == NULL) &&
		(xrtWsStreamState(pWrapping) == XWS_STREAM_CLOSED) &&
		(xrtWsStreamRole(pWrapping) == XWS_ROLE_SERVER) &&
		(Protocol.Data == NULL) &&
		(Protocol.Size == 0) &&
		(xrtWsStreamWorker(pWrapping) == NULL) &&
		(xrtWsStreamTcp(pWrapping) == NULL) &&
		(xrtWsStreamTcpRef(pWrapping) == NULL) &&
		(xrtWsStreamPending(pWrapping) == 0) &&
		(xrtWsStreamWritable(pWrapping) == 0) &&
		!xrtWsStreamPaused(pWrapping) &&
		(xrtWsStreamError(pWrapping) == NULL) &&
		!xrtWsStreamAbort(pWrapping) &&
		!xrtWsStreamResume(pWrapping) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_STREAM_ERROR_ARGUMENT),
		"WebSocket connection queries accepted a wrapping range"
	);
	xrtWsStreamPause(pWrapping);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_STREAM_ERROR_ARGUMENT),
		"WebSocket connection pause accepted a wrapping range"
	);
	xrtWsStreamDestroy(pWrapping);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_STREAM_ERROR_ARGUMENT),
		"WebSocket connection destroy accepted a wrapping range"
	);
	memset(&Close, 0xA5, sizeof(Close));
	testRequire(
		!xrtWsStreamCloseInfo(pWrapping, &Close) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_STREAM_ERROR_ARGUMENT) &&
		(Close.Flags == UINT32_C(0xA5A5A5A5)),
		"WebSocket Close query accepted a wrapping connection"
	);
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		testRequire(
			(xrtWsStreamTls(pWrapping) == NULL) &&
			(xrtWsStreamTlsRef(pWrapping) == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
			(xrtErrorCode(xrtGetError()) ==
			 XWS_STREAM_ERROR_ARGUMENT),
			"WebSocket TLS query accepted a wrapping connection"
		);
	#endif
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		memset(&Deflate, 0xA5, sizeof(Deflate));
		testRequire(
			!xrtWsStreamDeflate(pWrapping, &Deflate) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
			(xrtErrorCode(xrtGetError()) ==
			 XWS_STREAM_ERROR_ARGUMENT),
			"WebSocket Deflate query accepted a wrapping connection"
		);
	#endif
	return 0;
}
