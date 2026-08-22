#include "../test.h"



static const char TestPlain[] = "Hello permessage-deflate";



/* 固定输出收集器记录单条消息的线路字节。 */
typedef struct test_ws_deflater_output {
	uint8 Data[512];
	size_t Size;
	bool Stop;
} test_ws_deflater_output;



/* 重入探针验证活动回调不能破坏同一个发送变换。 */
typedef struct test_ws_deflater_reentry {
	xwsdeflater* Deflater;
	bool Called;
	bool BeginRejected;
	bool ResetRejected;
	bool AbortRejected;
	bool DestroyRejected;
} test_ws_deflater_reentry;



/* 收集压缩或直通输出。 */
static bool testWsDeflaterOutput(
	xbytesview Data,
	ptr pData
)
{
	test_ws_deflater_output* pOutput =
		(test_ws_deflater_output*)pData;

	if ( pOutput->Stop ) {
		return false;
	}
	testRequire(
		Data.Size <=
			(sizeof(pOutput->Data) - pOutput->Size),
		"WebSocket Deflater fixture overflowed"
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
static bool testWsDeflaterReentryOutput(
	xbytesview Data,
	ptr pData
)
{
	test_ws_deflater_reentry* pReentry =
		(test_ws_deflater_reentry*)pData;

	(void)Data;
	if ( pReentry->Called ) {
		return true;
	}
	pReentry->Called = true;
	xrtClearError();
	pReentry->BeginRejected =
		!xrtWsDeflaterBegin(
			pReentry->Deflater,
			false
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE);
	xrtClearError();
	pReentry->ResetRejected =
		!xrtWsDeflaterReset(
			pReentry->Deflater,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE);
	xrtClearError();
	pReentry->AbortRejected =
		!xrtWsDeflaterAbort(pReentry->Deflater) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE);
	xrtClearError();
	xrtWsDeflaterDestroy(pReentry->Deflater);
	pReentry->DestroyRejected =
		(xrtErrorKind(xrtGetError()) == XERR_STATE);
	return true;
}



/* 按变化分块压缩一条消息。 */
static bool testWsDeflaterMessage(
	xwsdeflater* pDeflater,
	xbytesview Input,
	size_t iChunk,
	test_ws_deflater_output* pOutput
)
{
	size_t iOffset = 0;

	if ( !xrtWsDeflaterBegin(pDeflater, true) ) {
		return false;
	}
	while ( iOffset < Input.Size ) {
		size_t iSize = Input.Size - iOffset;

		if ( iSize > iChunk ) {
			iSize = iChunk;
		}
		if ( !xrtWsDeflaterWrite(
			pDeflater,
			(xbytesview){
				Input.Data + iOffset,
				iSize
			},
			testWsDeflaterOutput,
			pOutput
		) ) {
			return false;
		}
		iOffset += iSize;
	}
	return xrtWsDeflaterEnd(
		pDeflater,
		testWsDeflaterOutput,
		pOutput
	);
}



/* 验证中间同步尾部、输出上界和活动消息放弃。 */
static void testWsDeflaterFlush(void)
{
	test_ws_deflater_output Output;
	xwsdeflater* pDeflater;
	size_t iFirst;
	size_t iFirstBound;
	size_t iSecondBound;

	memset(&Output, 0, sizeof(Output));
	pDeflater = xrtWsDeflaterCreate(NULL);
	testRequire(
		(pDeflater != NULL) &&
		xrtWsDeflaterBound(5u, &iFirstBound) &&
		xrtWsDeflaterBound(6u, &iSecondBound) &&
		xrtWsDeflaterBegin(pDeflater, true) &&
		xrtWsDeflaterWrite(
			pDeflater,
			XRT_BYTES_LITERAL("first"),
			testWsDeflaterOutput,
			&Output
		) &&
		xrtWsDeflaterFlush(
			pDeflater,
			testWsDeflaterOutput,
			&Output
		),
		"WebSocket Deflater intermediate flush failed"
	);
	iFirst = Output.Size;
	testRequire(
		(iFirst >= 4u) &&
		(iFirst <= iFirstBound) &&
		(memcmp(
			Output.Data + iFirst - 4u,
			"\x00\x00\xff\xff",
			4u
		 ) == 0),
		"WebSocket Deflater intermediate sync tail mismatch"
	);
	testRequire(
		xrtWsDeflaterWrite(
			pDeflater,
			XRT_BYTES_LITERAL("second"),
			testWsDeflaterOutput,
			&Output
		) &&
		xrtWsDeflaterEnd(
			pDeflater,
			testWsDeflaterOutput,
			&Output
		) &&
		((Output.Size - iFirst) <= iSecondBound) &&
		!((Output.Size >= 4u) &&
		  (memcmp(
			Output.Data + Output.Size - 4u,
			"\x00\x00\xff\xff",
			4u
		   ) == 0)),
		"WebSocket Deflater final sync boundary mismatch"
	);

	testRequire(
		xrtWsDeflaterBegin(pDeflater, true) &&
		xrtWsDeflaterWrite(
			pDeflater,
			XRT_BYTES_LITERAL("discard"),
			NULL,
			NULL
		) &&
		xrtWsDeflaterAbort(pDeflater),
		"WebSocket Deflater active message abort failed"
	);
	memset(&Output, 0, sizeof(Output));
	testRequire(
		testWsDeflaterMessage(
			pDeflater,
			XRT_BYTES_LITERAL("fresh"),
			2u,
			&Output
		) &&
		(Output.Size != 0),
		"WebSocket Deflater did not recover after abort"
	);
	xrtWsDeflaterDestroy(pDeflater);
}



/* 验证协商方向映射和发送配置应用。 */
static void testWsDeflaterConfig(void)
{
	xwsdeflate Response;
	xwsdeflatedirection Direction;
	xwsdeflaterconfig Config;

	xrtWsDeflateInit(&Response);
	Response.Flags =
		XWS_DEFLATE_SERVER_NO_CONTEXT |
		XWS_DEFLATE_SERVER_MAX_WINDOW;
	Response.ServerMaxWindowBits = 11;
	testRequire(
		xrtWsDeflateDirection(
			&Response,
			XWS_ROLE_SERVER,
			true,
			&Direction
		) &&
		(Direction.WindowBits == 11) &&
		Direction.NoContextTakeover,
		"server send direction mismatch"
	);

	xrtWsDeflaterConfigInit(&Config);
	testRequire(
		(Config.OutputLimit == UINT64_MAX) &&
		(Config.Level == XDEFLATE_LEVEL_DEFAULT) &&
		(Config.Strategy ==
		 XDEFLATE_STRATEGY_DEFAULT) &&
		(Config.WindowBits ==
		 XWS_DEFLATE_WINDOW_MAX) &&
		!Config.NoContextTakeover &&
		!Config.Retain &&
		xrtWsDeflaterConfigApply(
			&Config,
			&Direction
		) &&
		(Config.WindowBits == 11) &&
		Config.NoContextTakeover,
		"WebSocket Deflater config mismatch"
	);
}



/* 验证未对齐配置、地址回绕和无效输入后的状态可恢复性。 */
static void testWsDeflaterMemoryContracts(void)
{
	uint8 ConfigStorage[sizeof(xwsdeflaterconfig) + 2u];
	uint8 DirectionStorage[sizeof(xwsdeflatedirection) + 2u];
	uint8 BoundStorage[sizeof(size_t) + 2u];
	xbytesview Wrapping = {
		(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
		4u
	};
	xwsdeflaterconfig Config;
	xwsdeflaterconfig Before;
	xwsdeflatedirection Direction;
	test_ws_deflater_output Output;
	xwsdeflater* pDeflater;
	xwsdeflater* pWrapping =
		(xwsdeflater*)(uintptr_t)(UINTPTR_MAX - 1u);
	size_t iBound;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtWsDeflaterConfigInit(
		(xwsdeflaterconfig*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire(
		(ConfigStorage[0] == UINT8_C(0xA5)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == UINT8_C(0xA5)) &&
		(Config.OutputLimit == UINT64_MAX) &&
		(Config.Level == XDEFLATE_LEVEL_DEFAULT) &&
		(Config.Strategy == XDEFLATE_STRATEGY_DEFAULT) &&
		(Config.WindowBits == XWS_DEFLATE_WINDOW_MAX),
		"WebSocket Deflater init corrupted unaligned storage"
	);

	Direction.WindowBits = 10u;
	Direction.NoContextTakeover = true;
	memset(DirectionStorage, 0xA5, sizeof(DirectionStorage));
	memcpy(DirectionStorage + 1u, &Direction, sizeof(Direction));
	testRequire(
		xrtWsDeflaterConfigApply(
			(xwsdeflaterconfig*)(void*)(ConfigStorage + 1u),
			(const xwsdeflatedirection*)(const void*)(
				DirectionStorage + 1u
			)
		),
		"WebSocket Deflater rejected unaligned config apply"
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
		"WebSocket Deflater apply corrupted unaligned storage"
	);

	Before = Config;
	testRequire(
		!xrtWsDeflaterConfigApply(
			&Config,
			(const xwsdeflatedirection*)(const void*)&Config
		) &&
		(memcmp(&Config, &Before, sizeof(Config)) == 0),
		"WebSocket Deflater overlapping apply was not atomic"
	);
	xrtClearError();

	memset(BoundStorage, 0xA5, sizeof(BoundStorage));
	testRequire(
		xrtWsDeflaterBound(
			64u,
			(size_t*)(void*)(BoundStorage + 1u)
		),
		"WebSocket Deflater bound rejected unaligned output"
	);
	memcpy(&iBound, BoundStorage + 1u, sizeof(iBound));
	testRequire(
		(BoundStorage[0] == UINT8_C(0xA5)) &&
		(BoundStorage[sizeof(BoundStorage) - 1u] == UINT8_C(0xA5)) &&
		(iBound >= 64u),
		"WebSocket Deflater bound corrupted unaligned output"
	);
	iBound = 73u;
	testRequire(
		!xrtWsDeflaterBound(SIZE_MAX, &iBound) &&
		(iBound == 73u),
		"WebSocket Deflater bound overflow changed its output"
	);
	xrtClearError();

	pDeflater = xrtWsDeflaterCreate(
		(const xwsdeflaterconfig*)(const void*)(ConfigStorage + 1u)
	);
	memset(ConfigStorage, 0, sizeof(ConfigStorage));
	memset(&Output, 0, sizeof(Output));
	testRequire(
		(pDeflater != NULL) &&
		xrtWsDeflaterBegin(pDeflater, false) &&
		!xrtWsDeflaterWrite(
			pDeflater,
			Wrapping,
			testWsDeflaterOutput,
			&Output
		) &&
		!xrtWsDeflaterWrite(
			pDeflater,
			(xbytesview){ (cbytes)(const void*)pDeflater, 1u },
			testWsDeflaterOutput,
			&Output
		) &&
		xrtWsDeflaterWrite(
			pDeflater,
			XRT_BYTES_LITERAL("ok"),
			testWsDeflaterOutput,
			&Output
		) &&
		xrtWsDeflaterFlush(
			pDeflater,
			testWsDeflaterOutput,
			&Output
		) &&
		xrtWsDeflaterEnd(
			pDeflater,
			testWsDeflaterOutput,
			&Output
		) &&
		(Output.Size == 2u) &&
		(memcmp(Output.Data, "ok", 2u) == 0),
		"WebSocket Deflater argument failure poisoned active message"
	);

	testRequire(
		!xrtWsDeflaterReset(
			pDeflater,
			(const xwsdeflaterconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		xrtWsDeflaterReset(pDeflater, &Before),
		"WebSocket Deflater reset did not recover from invalid config"
	);
	xrtWsDeflaterDestroy(pDeflater);
	xrtClearError();

	xrtWsDeflaterConfigInit(
		(xwsdeflaterconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		!xrtWsDeflaterConfigApply(
			(xwsdeflaterconfig*)(uintptr_t)(UINTPTR_MAX - 1u),
			&Direction
		) &&
		!xrtWsDeflaterConfigApply(
			&Before,
			(const xwsdeflatedirection*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		(xrtWsDeflaterCreate(
			(const xwsdeflaterconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
		) == NULL) &&
		!xrtWsDeflaterReset(pWrapping, NULL) &&
		!xrtWsDeflaterBegin(pWrapping, false) &&
		!xrtWsDeflaterWrite(
			pWrapping,
			XRT_BYTES_LITERAL("x"),
			NULL,
			NULL
		) &&
		!xrtWsDeflaterFlush(pWrapping, NULL, NULL) &&
		!xrtWsDeflaterAbort(pWrapping) &&
		!xrtWsDeflaterEnd(pWrapping, NULL, NULL) &&
		!xrtWsDeflaterBound(
			1u,
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		(xrtWsDeflaterSize(pWrapping) == 0),
		"WebSocket Deflater accepted wrapping fixed ranges"
	);
	xrtWsDeflaterDestroy(pWrapping);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) == XWS_DEFLATE_ERROR_ARGUMENT),
		"WebSocket Deflater wrapping range error mismatch"
	);
	xrtClearError();
}



/* 验证流式编码剥离尾部，并让上下文接管改善重复消息。 */
static void testWsDeflaterContext(void)
{
	test_ws_deflater_output First;
	test_ws_deflater_output Second;
	xwsdeflater* pDeflater;

	memset(&First, 0, sizeof(First));
	memset(&Second, 0, sizeof(Second));
	pDeflater = xrtWsDeflaterCreate(NULL);
	testRequire(
		(pDeflater != NULL) &&
		testWsDeflaterMessage(
			pDeflater,
			XRT_BYTES_LITERAL(TestPlain),
			1,
			&First
		) &&
		(xrtWsDeflaterSize(pDeflater) ==
		 First.Size) &&
		testWsDeflaterMessage(
			pDeflater,
			XRT_BYTES_LITERAL(TestPlain),
			3,
			&Second
		) &&
		(First.Size > 4u) &&
		(Second.Size < First.Size) &&
		!((First.Size >= 4u) &&
		  (memcmp(
			First.Data + First.Size - 4u,
			"\x00\x00\xff\xff",
			4u
		   ) == 0)),
		"WebSocket Deflater context takeover failed"
	);
	xrtWsDeflaterDestroy(pDeflater);
}



/* 验证 no_context_takeover 输出独立、可释放并可选择保留复位对象。 */
static void testWsDeflaterIndependent(void)
{
	xwsdeflaterconfig Config;
	test_ws_deflater_output First;
	test_ws_deflater_output Second;
	test_ws_deflater_output Empty;
	xwsdeflater* pDeflater;

	memset(&First, 0, sizeof(First));
	memset(&Second, 0, sizeof(Second));
	memset(&Empty, 0, sizeof(Empty));
	xrtWsDeflaterConfigInit(&Config);
	Config.NoContextTakeover = true;
	pDeflater = xrtWsDeflaterCreate(&Config);
	testRequire(
		pDeflater != NULL,
		"independent WebSocket Deflater create failed"
	);
	testRequire(
		testWsDeflaterMessage(
			pDeflater,
			XRT_BYTES_LITERAL(TestPlain),
			2,
			&First
		),
		"first independent WebSocket Deflater message failed"
	);
	testRequire(
		testWsDeflaterMessage(
			pDeflater,
			XRT_BYTES_LITERAL(TestPlain),
			2,
			&Second
		),
		"second independent WebSocket Deflater message failed"
	);
	testRequire(
		(First.Size == Second.Size) &&
		(memcmp(
			First.Data,
			Second.Data,
			First.Size
		 ) == 0),
		"independent WebSocket Deflater output mismatch"
	);
	testRequire(
		testWsDeflaterMessage(
			pDeflater,
			(xbytesview){ NULL, 0 },
			1,
			&Empty
		),
		"empty WebSocket Deflater message failed"
	);
	testRequire(
		(Empty.Size != 0u) &&
		!((Empty.Size >= 4u) &&
		  (memcmp(
			Empty.Data + Empty.Size - 4u,
			"\x00\x00\xff\xff",
			4u
		   ) == 0)),
		"empty WebSocket Deflater size mismatch"
	);
	xrtWsDeflaterDestroy(pDeflater);

	Config.Retain = true;
	pDeflater = xrtWsDeflaterCreate(&Config);
	memset(&First, 0, sizeof(First));
	memset(&Second, 0, sizeof(Second));
	testRequire(
		(pDeflater != NULL) &&
		testWsDeflaterMessage(
			pDeflater,
			XRT_BYTES_LITERAL(TestPlain),
			4,
			&First
		) &&
		testWsDeflaterMessage(
			pDeflater,
			XRT_BYTES_LITERAL(TestPlain),
			4,
			&Second
		) &&
		(First.Size == Second.Size) &&
		(memcmp(
			First.Data,
			Second.Data,
			First.Size
		 ) == 0),
		"retained no-context WebSocket Deflater mismatch"
	);
	xrtWsDeflaterDestroy(pDeflater);
}



/* 验证直通路径、输出上限、回调拒绝和失败恢复。 */
static void testWsDeflaterFailures(void)
{
	xwsdeflaterconfig Config;
	test_ws_deflater_output Output;
	xwsdeflater* pDeflater;

	memset(&Output, 0, sizeof(Output));
	pDeflater = xrtWsDeflaterCreate(NULL);
	testRequire(
		(pDeflater != NULL) &&
		xrtWsDeflaterBegin(pDeflater, false) &&
		xrtWsDeflaterWrite(
			pDeflater,
			XRT_BYTES_LITERAL("plain"),
			testWsDeflaterOutput,
			&Output
		) &&
		xrtWsDeflaterEnd(
			pDeflater,
			testWsDeflaterOutput,
			&Output
		) &&
		(Output.Size == 5u) &&
		(memcmp(Output.Data, "plain", 5u) == 0),
		"WebSocket Deflater passthrough failed"
	);
	xrtWsDeflaterDestroy(pDeflater);

	xrtWsDeflaterConfigInit(&Config);
	Config.OutputLimit = 0;
	pDeflater = xrtWsDeflaterCreate(&Config);
	testRequire(
		(pDeflater != NULL) &&
		xrtWsDeflaterBegin(pDeflater, true) &&
		!xrtWsDeflaterEnd(pDeflater, NULL, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_DEFLATE_ERROR_LIMIT) &&
		!xrtWsDeflaterBegin(pDeflater, false),
		"WebSocket Deflater limit or failed state mismatch"
	);
	xrtClearError();
	testRequire(
		xrtWsDeflaterReset(pDeflater, NULL),
		"WebSocket Deflater failed state did not reset"
	);
	xrtWsDeflaterDestroy(pDeflater);

	memset(&Output, 0, sizeof(Output));
	Output.Stop = true;
	pDeflater = xrtWsDeflaterCreate(NULL);
	testRequire(
		(pDeflater != NULL) &&
		xrtWsDeflaterBegin(pDeflater, false) &&
		!xrtWsDeflaterWrite(
			pDeflater,
			XRT_BYTES_LITERAL("stop"),
			testWsDeflaterOutput,
			&Output
		) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_CANCELLED) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_DEFLATE_ERROR_OUTPUT),
		"WebSocket Deflater callback stop mismatch"
	);
	xrtClearError();
	xrtWsDeflaterDestroy(pDeflater);
}



/* 验证输出回调期间的生命周期防护。 */
static void testWsDeflaterReentry(void)
{
	test_ws_deflater_reentry Reentry;
	xwsdeflater* pDeflater;

	memset(&Reentry, 0, sizeof(Reentry));
	pDeflater = xrtWsDeflaterCreate(NULL);
	testRequire(
		(pDeflater != NULL) &&
		xrtWsDeflaterBegin(pDeflater, false),
		"WebSocket Deflater reentry fixture failed"
	);
	Reentry.Deflater = pDeflater;
	testRequire(
		xrtWsDeflaterWrite(
			pDeflater,
			XRT_BYTES_LITERAL("reentry"),
			testWsDeflaterReentryOutput,
			&Reentry
		) &&
		xrtWsDeflaterEnd(pDeflater, NULL, NULL) &&
		Reentry.Called &&
		Reentry.BeginRejected &&
		Reentry.ResetRejected &&
		Reentry.AbortRejected &&
		Reentry.DestroyRejected,
		"WebSocket Deflater reentry guard failed"
	);
	xrtWsDeflaterDestroy(pDeflater);
	xrtClearError();
}



/* 运行发送运行时的流式、上下文、尾部和生命周期回归。 */
int main(void)
{
	testWsDeflaterConfig();
	testWsDeflaterMemoryContracts();
	testWsDeflaterFlush();
	testWsDeflaterContext();
	testWsDeflaterIndependent();
	testWsDeflaterFailures();
	testWsDeflaterReentry();
	printf("[PASS] websocket_deflater\n");
	return 0;
}
