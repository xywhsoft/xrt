#include "../test.h"



static const uint8 TestMessageFirst[] = {
	0xF2, 0x48, 0xCD, 0xC9, 0xC9, 0x57, 0x28, 0x48,
	0x2D, 0xCA, 0x4D, 0x2D, 0x2E, 0x4E, 0x4C, 0x4F,
	0xD5, 0x4D, 0x49, 0x4D, 0xCB, 0x49, 0x2C, 0x49,
	0x05, 0x00
};

static const uint8 TestMessageContext[] = {
	0xF2, 0xC0, 0x21, 0x0E, 0x00
};

static const char TestPlain[] = "Hello permessage-deflate";



/* 测试输出固定收集语义负载，也可以主动拒绝回调。 */
typedef struct test_ws_inflater_output {
	uint8 Data[256];
	size_t Size;
	bool Stop;
} test_ws_inflater_output;



/* 重入探针验证活动回调不能破坏同一个接收变换。 */
typedef struct test_ws_inflater_reentry {
	xwsinflater* Inflater;
	bool Called;
	bool BeginRejected;
	bool ResetRejected;
	bool DestroyRejected;
} test_ws_inflater_reentry;



/* 收集一段解码输出。 */
static bool testWsInflaterOutput(
	xbytesview Data,
	ptr pData
)
{
	test_ws_inflater_output* pOutput =
		(test_ws_inflater_output*)pData;

	if ( pOutput->Stop ) {
		return false;
	}
	testRequire(
		Data.Size <=
			(sizeof(pOutput->Data) - pOutput->Size),
		"WebSocket Inflater fixture overflowed"
	);
	memcpy(
		pOutput->Data + pOutput->Size,
		Data.Data,
		Data.Size
	);
	pOutput->Size += Data.Size;
	return true;
}



/* 从输出回调尝试开始、复位和销毁同一对象。 */
static bool testWsInflaterReentryOutput(
	xbytesview Data,
	ptr pData
)
{
	test_ws_inflater_reentry* pReentry =
		(test_ws_inflater_reentry*)pData;

	(void)Data;
	if ( pReentry->Called ) {
		return true;
	}
	pReentry->Called = true;
	xrtClearError();
	pReentry->BeginRejected =
		!xrtWsInflaterBegin(
			pReentry->Inflater,
			false
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE);
	xrtClearError();
	pReentry->ResetRejected =
		!xrtWsInflaterReset(
			pReentry->Inflater,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE);
	xrtClearError();
	xrtWsInflaterDestroy(pReentry->Inflater);
	pReentry->DestroyRejected =
		(xrtErrorKind(xrtGetError()) == XERR_STATE);
	return true;
}



/* 以任意输入分块解码一条消息。 */
static bool testWsInflaterMessage(
	xwsinflater* pInflater,
	const uint8* pInput,
	size_t iInputSize,
	size_t iChunk,
	test_ws_inflater_output* pOutput
)
{
	size_t iOffset = 0;

	if ( !xrtWsInflaterBegin(pInflater, true) ) {
		return false;
	}
	while ( iOffset < iInputSize ) {
		size_t iSize = iInputSize - iOffset;

		if ( iSize > iChunk ) {
			iSize = iChunk;
		}
		if ( !xrtWsInflaterWrite(
			pInflater,
			(xbytesview){
				pInput + iOffset,
				iSize
			},
			testWsInflaterOutput,
			pOutput
		) ) {
			return false;
		}
		iOffset += iSize;
	}
	return xrtWsInflaterEnd(
		pInflater,
		testWsInflaterOutput,
		pOutput
	);
}



/* 验证协商方向映射和运行配置应用。 */
static void testWsInflaterConfig(void)
{
	xwsdeflate Response;
	xwsdeflatedirection Direction;
	xwsinflaterconfig Config;

	xrtWsDeflateInit(&Response);
	Response.Flags =
		XWS_DEFLATE_SERVER_NO_CONTEXT |
		XWS_DEFLATE_CLIENT_MAX_WINDOW;
	Response.ClientMaxWindowBits = 10;
	testRequire(
		xrtWsDeflateDirection(
			&Response,
			XWS_ROLE_SERVER,
			false,
			&Direction
		) &&
		(Direction.WindowBits == 10) &&
		!Direction.NoContextTakeover,
		"server receive direction mismatch"
	);

	xrtWsInflaterConfigInit(&Config);
	testRequire(
		(Config.OutputLimit ==
		 XWS_INFLATE_OUTPUT_DEFAULT) &&
		(Config.WindowBits ==
		 XWS_DEFLATE_WINDOW_MAX) &&
		!Config.NoContextTakeover &&
		!Config.Retain &&
		xrtWsInflaterConfigApply(
			&Config,
			&Direction
		) &&
		(Config.WindowBits == 10),
		"WebSocket Inflater config mismatch"
	);
}



/* 验证未对齐配置、地址回绕和无效输入后的状态可恢复性。 */
static void testWsInflaterMemoryContracts(void)
{
	uint8 ConfigStorage[sizeof(xwsinflaterconfig) + 2u];
	uint8 DirectionStorage[sizeof(xwsdeflatedirection) + 2u];
	xbytesview Wrapping = {
		(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
		4u
	};
	xwsinflaterconfig Config;
	xwsinflaterconfig Before;
	xwsdeflatedirection Direction;
	test_ws_inflater_output Output;
	xwsinflater* pInflater;
	xwsinflater* pWrapping =
		(xwsinflater*)(uintptr_t)(UINTPTR_MAX - 1u);

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtWsInflaterConfigInit(
		(xwsinflaterconfig*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire(
		(ConfigStorage[0] == UINT8_C(0xA5)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == UINT8_C(0xA5)) &&
		(Config.OutputLimit == XWS_INFLATE_OUTPUT_DEFAULT) &&
		(Config.WindowBits == XWS_DEFLATE_WINDOW_MAX),
		"WebSocket Inflater init corrupted unaligned storage"
	);

	Direction.WindowBits = 10u;
	Direction.NoContextTakeover = true;
	memset(DirectionStorage, 0xA5, sizeof(DirectionStorage));
	memcpy(DirectionStorage + 1u, &Direction, sizeof(Direction));
	testRequire(
		xrtWsInflaterConfigApply(
			(xwsinflaterconfig*)(void*)(ConfigStorage + 1u),
			(const xwsdeflatedirection*)(const void*)(
				DirectionStorage + 1u
			)
		),
		"WebSocket Inflater rejected unaligned config apply"
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire(
		(ConfigStorage[0] == UINT8_C(0xA5)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == UINT8_C(0xA5)) &&
		(DirectionStorage[0] == UINT8_C(0xA5)) &&
		(DirectionStorage[sizeof(DirectionStorage) - 1u] ==
		 UINT8_C(0xA5)) &&
		(Config.WindowBits == 10u) &&
		Config.NoContextTakeover,
		"WebSocket Inflater apply corrupted unaligned storage"
	);

	Before = Config;
	testRequire(
		!xrtWsInflaterConfigApply(
			&Config,
			(const xwsdeflatedirection*)(const void*)&Config
		) &&
		(memcmp(&Config, &Before, sizeof(Config)) == 0),
		"WebSocket Inflater overlapping apply was not atomic"
	);
	xrtClearError();

	pInflater = xrtWsInflaterCreate(
		(const xwsinflaterconfig*)(const void*)(ConfigStorage + 1u)
	);
	memset(ConfigStorage, 0, sizeof(ConfigStorage));
	memset(&Output, 0, sizeof(Output));
	testRequire(
		(pInflater != NULL) &&
		xrtWsInflaterBegin(pInflater, false) &&
		!xrtWsInflaterWrite(
			pInflater,
			Wrapping,
			testWsInflaterOutput,
			&Output
		) &&
		!xrtWsInflaterWrite(
			pInflater,
			(xbytesview){ (cbytes)(const void*)pInflater, 1u },
			testWsInflaterOutput,
			&Output
		) &&
		xrtWsInflaterWrite(
			pInflater,
			XRT_BYTES_LITERAL("ok"),
			testWsInflaterOutput,
			&Output
		) &&
		xrtWsInflaterEnd(
			pInflater,
			testWsInflaterOutput,
			&Output
		) &&
		(Output.Size == 2u) &&
		(memcmp(Output.Data, "ok", 2u) == 0),
		"WebSocket Inflater argument failure poisoned active message"
	);

	testRequire(
		!xrtWsInflaterReset(
			pInflater,
			(const xwsinflaterconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		xrtWsInflaterReset(pInflater, &Before),
		"WebSocket Inflater reset did not recover from invalid config"
	);
	xrtWsInflaterDestroy(pInflater);
	xrtClearError();

	xrtWsInflaterConfigInit(
		(xwsinflaterconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		!xrtWsInflaterConfigApply(
			(xwsinflaterconfig*)(uintptr_t)(UINTPTR_MAX - 1u),
			&Direction
		) &&
		!xrtWsInflaterConfigApply(
			&Before,
			(const xwsdeflatedirection*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		(xrtWsInflaterCreate(
			(const xwsinflaterconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
		) == NULL) &&
		!xrtWsInflaterReset(pWrapping, NULL) &&
		!xrtWsInflaterBegin(pWrapping, false) &&
		!xrtWsInflaterWrite(
			pWrapping,
			XRT_BYTES_LITERAL("x"),
			NULL,
			NULL
		) &&
		!xrtWsInflaterEnd(pWrapping, NULL, NULL) &&
		(xrtWsInflaterSize(pWrapping) == 0),
		"WebSocket Inflater accepted wrapping fixed ranges"
	);
	xrtWsInflaterDestroy(pWrapping);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) == XWS_DEFLATE_ERROR_ARGUMENT),
		"WebSocket Inflater wrapping range error mismatch"
	);
	xrtClearError();
}



/* 验证单字节分块、上下文接管和消息计数。 */
static void testWsInflaterContext(void)
{
	xwsinflaterconfig Config;
	test_ws_inflater_output Output;
	xwsinflater* pInflater;

	memset(&Output, 0, sizeof(Output));
	xrtWsInflaterConfigInit(&Config);
	pInflater = xrtWsInflaterCreate(&Config);
	testRequire(
		(pInflater != NULL) &&
		testWsInflaterMessage(
			pInflater,
			TestMessageFirst,
			sizeof(TestMessageFirst),
			1,
			&Output
		) &&
		(Output.Size == (sizeof(TestPlain) - 1u)) &&
		(memcmp(
			Output.Data,
			TestPlain,
			Output.Size
		 ) == 0) &&
		(xrtWsInflaterSize(pInflater) ==
		 Output.Size),
		"first context-takeover message failed"
	);

	memset(&Output, 0, sizeof(Output));
	testRequire(
		testWsInflaterMessage(
			pInflater,
			TestMessageContext,
			sizeof(TestMessageContext),
			2,
			&Output
		) &&
		(Output.Size == (sizeof(TestPlain) - 1u)) &&
		(memcmp(
			Output.Data,
			TestPlain,
			Output.Size
		 ) == 0),
		"second context-takeover message failed"
	);
	xrtWsInflaterDestroy(pInflater);
}



/* 验证 no_context_takeover、直通消息和零长度消息。 */
static void testWsInflaterIndependent(void)
{
	xwsinflaterconfig Config;
	test_ws_inflater_output Output;
	xwsinflater* pInflater;
	size_t i;

	xrtWsInflaterConfigInit(&Config);
	Config.NoContextTakeover = true;
	pInflater = xrtWsInflaterCreate(&Config);
	testRequire(
		pInflater != NULL,
		"independent WebSocket Inflater create failed"
	);
	for ( i = 0; i < 2u; i++ ) {
		memset(&Output, 0, sizeof(Output));
		testRequire(
			testWsInflaterMessage(
				pInflater,
				TestMessageFirst,
				sizeof(TestMessageFirst),
				3,
				&Output
			) &&
			(Output.Size == (sizeof(TestPlain) - 1u)),
			"independent compressed message failed"
		);
	}

	memset(&Output, 0, sizeof(Output));
	testRequire(
		xrtWsInflaterBegin(pInflater, false) &&
		xrtWsInflaterWrite(
			pInflater,
			XRT_BYTES_LITERAL("plain"),
			testWsInflaterOutput,
			&Output
		) &&
		xrtWsInflaterEnd(
			pInflater,
			testWsInflaterOutput,
			&Output
		) &&
		(Output.Size == 5u) &&
		(memcmp(Output.Data, "plain", 5u) == 0),
		"WebSocket Inflater passthrough failed"
	);

	memset(&Output, 0, sizeof(Output));
	testRequire(
		xrtWsInflaterBegin(pInflater, true) &&
		xrtWsInflaterEnd(
			pInflater,
			testWsInflaterOutput,
			&Output
		) &&
		(Output.Size == 0),
		"empty compressed message failed"
	);
	xrtWsInflaterDestroy(pInflater);
}



/* 验证上限、坏数据、回调拒绝和失败终态。 */
static void testWsInflaterFailures(void)
{
	static const uint8 Bad[] = {
		0xFF, 0xFF, 0xFF
	};
	xwsinflaterconfig Config;
	test_ws_inflater_output Output;
	xwsinflater* pInflater;

	xrtWsInflaterConfigInit(&Config);
	Config.OutputLimit = 4;
	pInflater = xrtWsInflaterCreate(&Config);
	testRequire(
		(pInflater != NULL) &&
		xrtWsInflaterBegin(pInflater, true) &&
		!xrtWsInflaterWrite(
			pInflater,
			(xbytesview){
				TestMessageFirst,
				sizeof(TestMessageFirst)
			},
			NULL,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_DEFLATE_ERROR_LIMIT) &&
		!xrtWsInflaterBegin(pInflater, false),
		"WebSocket Inflater limit or failed state mismatch"
	);
	xrtClearError();
	testRequire(
		xrtWsInflaterReset(pInflater, NULL),
		"WebSocket Inflater failed state did not reset"
	);
	xrtWsInflaterDestroy(pInflater);

	pInflater = xrtWsInflaterCreate(NULL);
	testRequire(
		(pInflater != NULL) &&
		xrtWsInflaterBegin(pInflater, true) &&
		!xrtWsInflaterWrite(
			pInflater,
			(xbytesview){ Bad, sizeof(Bad) },
			NULL,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_DEFLATE_ERROR_DATA),
		"WebSocket Inflater accepted invalid payload"
	);
	xrtClearError();
	xrtWsInflaterDestroy(pInflater);

	memset(&Output, 0, sizeof(Output));
	Output.Stop = true;
	pInflater = xrtWsInflaterCreate(NULL);
	testRequire(
		(pInflater != NULL) &&
		xrtWsInflaterBegin(pInflater, false) &&
		!xrtWsInflaterWrite(
			pInflater,
			XRT_BYTES_LITERAL("stop"),
			testWsInflaterOutput,
			&Output
		) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_CANCELLED) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_DEFLATE_ERROR_OUTPUT),
		"WebSocket Inflater callback stop mismatch"
	);
	xrtClearError();
	xrtWsInflaterDestroy(pInflater);
}



/* 验证输出回调期间的生命周期防护。 */
static void testWsInflaterReentry(void)
{
	test_ws_inflater_reentry Reentry;
	xwsinflater* pInflater;

	memset(&Reentry, 0, sizeof(Reentry));
	pInflater = xrtWsInflaterCreate(NULL);
	testRequire(
		(pInflater != NULL) &&
		xrtWsInflaterBegin(pInflater, false),
		"WebSocket Inflater reentry fixture failed"
	);
	Reentry.Inflater = pInflater;
	testRequire(
		xrtWsInflaterWrite(
			pInflater,
			XRT_BYTES_LITERAL("reentry"),
			testWsInflaterReentryOutput,
			&Reentry
		) &&
		xrtWsInflaterEnd(pInflater, NULL, NULL) &&
		Reentry.Called &&
		Reentry.BeginRejected &&
		Reentry.ResetRejected &&
		Reentry.DestroyRejected,
		"WebSocket Inflater reentry guard failed"
	);
	xrtWsInflaterDestroy(pInflater);
	xrtClearError();
}



/* 运行接收运行时的流式、上下文、限额和生命周期回归。 */
int main(void)
{
	testWsInflaterConfig();
	testWsInflaterMemoryContracts();
	testWsInflaterContext();
	testWsInflaterIndependent();
	testWsInflaterFailures();
	testWsInflaterReentry();
	printf("[PASS] websocket_inflater\n");
	return 0;
}
