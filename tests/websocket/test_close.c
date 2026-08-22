#include "../test.h"



/* 验证状态码范围、禁用本地状态码和纯判断错误保持。 */
static void testCloseCodes(void)
{
	static const uint16 Valid[] = {
		1000, 1001, 1002, 1003,
		1007, 1008, 1009, 1010, 1011, 1012, 1013, 1014,
		3000, 3999, 4000, 4999
	};
	static const uint16 Invalid[] = {
		0, 999, 1004, 1005, 1006, 1015, 1016, 2999, 5000, 65535
	};
	xerror* pPrior;
	size_t i;

	pPrior = xrtErrorCreate(
		XERR_STATE,
		"test",
		17,
		"preserved close-code error"
	);
	testRequire(pPrior != NULL, "Close code prior error creation failed");
	xrtSetError(pPrior);

	for ( i = 0; i < sizeof(Valid) / sizeof(Valid[0]); i++ ) {
		testRequire(
			xrtWsCloseCodeValid(Valid[i]) &&
			(xrtGetError() == pPrior),
			"valid WebSocket Close code was rejected or changed error"
		);
	}
	for ( i = 0; i < sizeof(Invalid) / sizeof(Invalid[0]); i++ ) {
		testRequire(
			!xrtWsCloseCodeValid(Invalid[i]) &&
			(xrtGetError() == pPrior),
			"forbidden WebSocket Close code was accepted or changed error"
		);
	}
	xrtClearError();
}



/* 验证空负载、状态码和借用原因的解析契约。 */
static void testCloseParse(void)
{
	static const uint8 Payload[] = {
		0x03, 0xE8, 'b', 'y', 'e'
	};
	xwsclose Close;
	xbytesview Input;

	memset(&Close, 0xA5, sizeof(Close));
	Input.Data = NULL;
	Input.Size = 0;
	testRequire(
		xrtWsCloseParse(Input, &Close) &&
		(Close.Code == 0) &&
		(Close.Reason.Data == NULL) &&
		(Close.Reason.Size == 0),
		"empty WebSocket Close payload parse failed"
	);

	memset(&Close, 0xA5, sizeof(Close));
	Input.Data = Payload;
	Input.Size = sizeof(Payload);
	testRequire(
		xrtWsCloseParse(Input, &Close) &&
		(Close.Code == XWS_CLOSE_NORMAL) &&
		(Close.Reason.Data == (const char*)Payload + 2u) &&
		(Close.Reason.Size == 3u) &&
		(memcmp(Close.Reason.Data, "bye", 3) == 0),
		"WebSocket Close payload metadata mismatch"
	);
}



/* 验证畸形负载失败原子性和稳定错误域。 */
static void testCloseRejectsMalformed(void)
{
	static const uint8 One[] = { 0x03 };
	static const uint8 NoStatus[] = { 0x03, 0xED };
	static const uint8 Tls[] = { 0x03, 0xF7 };
	static const uint8 Unassigned[] = { 0x03, 0xF8 };
	static const uint8 InvalidUtf8[] = { 0x03, 0xE8, 0xC0, 0x80 };
	static uint8 TooLarge[XWS_CLOSE_PAYLOAD_MAX + 1u];
	static const struct {
		const uint8* Data;
		size_t Size;
		xwscloseerror Error;
	} Cases[] = {
		{ One, sizeof(One), XWS_CLOSE_ERROR_SIZE },
		{ NoStatus, sizeof(NoStatus), XWS_CLOSE_ERROR_CODE },
		{ Tls, sizeof(Tls), XWS_CLOSE_ERROR_CODE },
		{ Unassigned, sizeof(Unassigned), XWS_CLOSE_ERROR_CODE },
		{ InvalidUtf8, sizeof(InvalidUtf8), XWS_CLOSE_ERROR_UTF8 },
		{ TooLarge, sizeof(TooLarge), XWS_CLOSE_ERROR_SIZE }
	};
	size_t i;

	for ( i = 0; i < sizeof(Cases) / sizeof(Cases[0]); i++ ) {
		xwsclose Before;
		xwsclose Close;
		xbytesview Input;

		memset(&Close, 0xA5, sizeof(Close));
		Before = Close;
		Input.Data = Cases[i].Data;
		Input.Size = Cases[i].Size;
		xrtClearError();
		testRequire(
			!xrtWsCloseParse(Input, &Close) &&
			(memcmp(&Close, &Before, sizeof(Close)) == 0) &&
			(xrtGetError() != NULL) &&
			(strcmp(
				xrtErrorDomain(xrtGetError()),
				"xrt.websocket.close"
			) == 0) &&
			(xrtErrorCode(xrtGetError()) == (int32)Cases[i].Error),
			"malformed WebSocket Close payload contract mismatch"
		);
	}
}



/* 验证查询、原子写出、重叠输入和往返解析。 */
static void testCloseWrite(void)
{
	uint8 Buffer[XWS_CLOSE_PAYLOAD_MAX];
	uint8 Before[XWS_CLOSE_PAYLOAD_MAX];
	xwsclose Close;
	xbytesview Input;
	xstrview Reason = XRT_STR_LITERAL("normal shutdown");
	size_t iSize = SIZE_MAX;

	testRequire(
		xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			Reason,
			NULL,
			0,
			&iSize
		) && (iSize == Reason.Size + 2u),
		"WebSocket Close write query failed"
	);
	memset(Buffer, 0xA5, sizeof(Buffer));
	testRequire(
		xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			Reason,
			Buffer,
			sizeof(Buffer),
			&iSize
		) &&
		(Buffer[0] == 0x03) && (Buffer[1] == 0xE8),
		"WebSocket Close write failed"
	);
	Input.Data = Buffer;
	Input.Size = iSize;
	testRequire(
		xrtWsCloseParse(Input, &Close) &&
		(Close.Code == XWS_CLOSE_NORMAL) &&
		(Close.Reason.Size == Reason.Size) &&
		(memcmp(Close.Reason.Data, Reason.Data, Reason.Size) == 0),
		"WebSocket Close write did not round trip"
	);

	memcpy(Buffer + 2u, "overlap", 7u);
	Reason.Data = (const char*)Buffer + 2u;
	Reason.Size = 7u;
	testRequire(
		xrtWsCloseWrite(
			XWS_CLOSE_GOING_AWAY,
			Reason,
			Buffer,
			sizeof(Buffer),
			&iSize
		) &&
		(iSize == 9u) &&
		(memcmp(Buffer + 2u, "overlap", 7u) == 0),
		"overlapping WebSocket Close write failed"
	);

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Buffer));
	Reason = XRT_STR_LITERAL("x");
	iSize = 0;
	testRequire(
		!xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			Reason,
			Buffer,
			2u,
			&iSize
		) &&
		(iSize == 3u) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
		(xrtErrorCode(xrtGetError()) == XWS_CLOSE_ERROR_OUTPUT),
		"short WebSocket Close output was not failure-atomic"
	);

	testRequire(
		xrtWsCloseWrite(
			0,
			(xstrview) { NULL, 0 },
			Buffer,
			sizeof(Buffer),
			&iSize
		) && (iSize == 0),
		"empty WebSocket Close payload write failed"
	);
}



/* 验证写出端拒绝禁用状态、超长和非法文本。 */
static void testCloseWriteRejects(void)
{
	char LongReason[XWS_CLOSE_REASON_MAX + 1u];
	static const char Invalid[] = { (char)0xC0, (char)0x80 };
	uint8 Buffer[XWS_CLOSE_PAYLOAD_MAX];
	uint8 Before[XWS_CLOSE_PAYLOAD_MAX];
	size_t iSize = 0;

	memset(LongReason, 'x', sizeof(LongReason));
	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Buffer));

	testRequire(
		!xrtWsCloseWrite(
			XWS_CLOSE_NO_STATUS,
			(xstrview) { NULL, 0 },
			Buffer,
			sizeof(Buffer),
			&iSize
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_CLOSE_ERROR_CODE),
		"WebSocket Close writer accepted synthetic 1005"
	);
	testRequire(
		!xrtWsCloseWrite(
			0,
			XRT_STR_LITERAL("reason"),
			Buffer,
			sizeof(Buffer),
			&iSize
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_CLOSE_ERROR_CODE),
		"WebSocket Close writer accepted reason without code"
	);
	testRequire(
		!xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			(xstrview) { LongReason, sizeof(LongReason) },
			Buffer,
			sizeof(Buffer),
			&iSize
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_CLOSE_ERROR_SIZE),
		"WebSocket Close writer accepted oversized reason"
	);
	testRequire(
		!xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			(xstrview) { Invalid, sizeof(Invalid) },
			Buffer,
			sizeof(Buffer),
			&iSize
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_CLOSE_ERROR_UTF8) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"WebSocket Close writer accepted invalid UTF-8 or changed output"
	);
}



/* 验证空指针、无效视图和输出别名不会形成未定义行为。 */
static void testCloseArguments(void)
{
	union {
		xwsclose Align;
		uint8 Bytes[sizeof(xwsclose)];
	} Storage;
	uint8 Payload[4] = { 0x03, 0xE8, 'o', 'k' };
	xwsclose Close;
	xbytesview Input;
	size_t iSize;

	Input.Data = NULL;
	Input.Size = 1;
	testRequire(
		!xrtWsCloseParse(Input, &Close) &&
		(xrtErrorCode(xrtGetError()) == XWS_CLOSE_ERROR_ARGUMENT),
		"WebSocket Close parser accepted null nonempty input"
	);

	Input.Data = Storage.Bytes;
	Input.Size = sizeof(Storage.Bytes);
	testRequire(
		!xrtWsCloseParse(Input, &Storage.Align) &&
		(xrtErrorCode(xrtGetError()) == XWS_CLOSE_ERROR_ARGUMENT),
		"WebSocket Close parser accepted overlapping output"
	);

	testRequire(
		!xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			XRT_STR_LITERAL("x"),
			Payload,
			sizeof(Payload),
			NULL
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_CLOSE_ERROR_ARGUMENT),
		"WebSocket Close writer accepted null size output"
	);
	testRequire(
		!xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			(xstrview) { NULL, 1 },
			Payload,
			sizeof(Payload),
			&iSize
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_CLOSE_ERROR_ARGUMENT),
		"WebSocket Close writer accepted null nonempty reason"
	);
	testRequire(
		!xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			XRT_STR_LITERAL("x"),
			NULL,
			1,
			&iSize
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_CLOSE_ERROR_ARGUMENT),
		"WebSocket Close writer accepted null output with capacity"
	);
}



/* 验证未对齐输出、完整范围和长度输出隔离契约。 */
static void testCloseMemoryContracts(void)
{
	static const uint8 Payload[] = {
		0x03, 0xE8, 'b', 'y', 'e'
	};
	uint8 CloseStorage[sizeof(xwsclose) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	uint8 Output[XWS_CLOSE_PAYLOAD_MAX + 2u];
	uint8 Before[sizeof(Output)];
	xbytesview Input;
	xstrview Reason = XRT_STR_LITERAL("done");
	xwsclose Close;
	size_t iSize;

	memset(CloseStorage, 0xA5, sizeof(CloseStorage));
	Input.Data = Payload;
	Input.Size = sizeof(Payload);
	testRequire(
		xrtWsCloseParse(
			Input,
			(xwsclose*)(void*)(CloseStorage + 1u)
		),
		"WebSocket Close parser rejected unaligned output"
	);
	memcpy(&Close, CloseStorage + 1u, sizeof(Close));
	testRequire(
		(CloseStorage[0] == UINT8_C(0xA5)) &&
		(CloseStorage[sizeof(CloseStorage) - 1u] == UINT8_C(0xA5)) &&
		(Close.Code == XWS_CLOSE_NORMAL) &&
		(Close.Reason.Data == (const char*)Payload + 2u) &&
		(Close.Reason.Size == 3u),
		"WebSocket Close parser corrupted unaligned output"
	);

	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memset(Output, 0xA5, sizeof(Output));
	testRequire(
		xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			Reason,
			Output + 1u,
			XWS_CLOSE_PAYLOAD_MAX,
			(size_t*)(void*)(SizeStorage + 1u)
		),
		"WebSocket Close writer rejected unaligned size output"
	);
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire(
		(iSize == Reason.Size + 2u) &&
		(Output[0] == UINT8_C(0xA5)) &&
		(Output[sizeof(Output) - 1u] == UINT8_C(0xA5)) &&
		(SizeStorage[0] == UINT8_C(0xA5)) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == UINT8_C(0xA5)),
		"WebSocket Close writer corrupted unaligned outputs"
	);

	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	iSize = 37u;
	testRequire(
		!xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			(xstrview){ (const char*)&iSize, 1u },
			Output,
			sizeof(Output),
			&iSize
		) &&
		(iSize == 37u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0),
		"WebSocket Close writer accepted an aliased size output"
	);

	Input.Data = (cbytes)(uintptr_t)(UINTPTR_MAX - 1u);
	Input.Size = 4u;
	testRequire(
		!xrtWsCloseParse(Input, &Close) &&
		!xrtWsCloseParse(
			(xbytesview){ Payload, sizeof(Payload) },
			(xwsclose*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		!xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			(xstrview){
				(const char*)(uintptr_t)(UINTPTR_MAX - 1u), 4u
			},
			Output,
			sizeof(Output),
			&iSize
		) &&
		!xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			Reason,
			(void*)(uintptr_t)(UINTPTR_MAX - 1u),
			4u,
			&iSize
		) &&
		!xrtWsCloseWrite(
			XWS_CLOSE_NORMAL,
			Reason,
			Output,
			sizeof(Output),
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
		),
		"WebSocket Close layer accepted wrapping ranges"
	);
	xrtClearError();
}



/* 执行 Close 状态码、解析和原子写出测试。 */
int main(void)
{
	testCloseCodes();
	testCloseParse();
	testCloseRejectsMalformed();
	testCloseWrite();
	testCloseWriteRejects();
	testCloseArguments();
	testCloseMemoryContracts();
	return 0;
}
