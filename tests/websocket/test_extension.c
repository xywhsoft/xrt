#include "../test.h"



/* 检查最近一次错误属于稳定 WebSocket 握手错误域。 */
static void testExtensionError(
	xwshandshakeerror Code,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.websocket.handshake"
		) == 0) &&
		(xrtErrorCode(pError) == (int32)Code),
		sMessage
	);
	xrtClearError();
}



/* 验证扩展列表、参数借用视图和 HTTP #rule 空成员。 */
static void testExtensionParse(void)
{
	xstrview Text = XRT_STR_LITERAL(
		", permessage-deflate; server_no_context_takeover; "
		"token=\"web\\socket\", x-test; mode=fast, ,"
	);
	xwsextension Extension;
	xhttpparam Param;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iParam = 0;
	size_t iCount;

	Next = xrtWsExtensionNext(Text, &iOffset, &Extension);
	testRequire(
		(Next == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Extension.Name,
			XRT_STR_LITERAL("permessage-deflate")
		),
		"WebSocket first extension mismatch"
	);
	Next = xrtWsExtensionParamNext(
		&Extension,
		&iParam,
		&Param
	);
	testRequire(
		(Next == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Param.Name,
			XRT_STR_LITERAL("server_no_context_takeover")
		) &&
		(Param.Flags == XHTTP_PARAM_NONE),
		"WebSocket valueless extension parameter mismatch"
	);
	Next = xrtWsExtensionParamNext(
		&Extension,
		&iParam,
		&Param
	);
	testRequire(
		(Next == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Param.Name,
			XRT_STR_LITERAL("token")
		) &&
		((Param.Flags & XHTTP_PARAM_QUOTED) != 0) &&
		xrtHttpParamTokenValid(&Param),
		"WebSocket quoted token parameter mismatch"
	);
	testRequire(
		xrtWsExtensionParamNext(
			&Extension,
			&iParam,
			&Param
		) == XHTTP_NEXT_END,
		"WebSocket first extension parameters did not end"
	);

	Next = xrtWsExtensionNext(Text, &iOffset, &Extension);
	testRequire(
		(Next == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Extension.Name,
			XRT_STR_LITERAL("x-test")
		),
		"WebSocket second extension mismatch"
	);
	testRequire(
		xrtWsExtensionNext(
			Text,
			&iOffset,
			&Extension
		) == XHTTP_NEXT_END,
		"WebSocket extension list did not end"
	);
	testRequire(
		xrtWsExtensionCount(Text, &iCount) &&
		(iCount == 2u) &&
		xrtWsExtensionCount(
			(xstrview){ NULL, 0 },
			&iCount
		) &&
		(iCount == 0),
		"WebSocket extension count mismatch"
	);
}



/* 验证扩展语法错误、完整尾部校验和错误输出原子性。 */
static void testExtensionInvalid(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT(" "),
		XRT_STR_INIT(", ,"),
		XRT_STR_INIT("bad name"),
		XRT_STR_INIT("ext tail"),
		XRT_STR_INIT("ext;"),
		XRT_STR_INIT("ext;; p"),
		XRT_STR_INIT("ext; p="),
		XRT_STR_INIT("ext; p=\"unterminated"),
		XRT_STR_INIT("ext; p=\"has space\""),
		XRT_STR_INIT("ext; p=\"a\\,b\""),
		XRT_STR_INIT("ext; p=\"bad\\\r\"")
	};
	xwsextension Extension;
	xwsextension Before;
	xhttpparam Param;
	xhttpparam ParamBefore;
	size_t iOffset;
	size_t iBefore;
	size_t i;

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		iOffset = 0;
		Extension.Name = XRT_STR_LITERAL("unchanged");
		Extension.Parameters = XRT_STR_LITERAL("p=v");
		Before = Extension;
		testRequire(
			xrtWsExtensionNext(
				Invalid[i],
				&iOffset,
				&Extension
			) == XHTTP_NEXT_ERROR &&
			(iOffset == 0) &&
			(Extension.Name.Data == Before.Name.Data) &&
			(Extension.Name.Size == Before.Name.Size) &&
			(Extension.Parameters.Data == Before.Parameters.Data) &&
			(Extension.Parameters.Size == Before.Parameters.Size),
			"WebSocket malformed extension was accepted or mutated outputs"
		);
		testExtensionError(
			XWS_HANDSHAKE_ERROR_EXTENSION,
			"WebSocket malformed extension error mismatch"
		);
	}

	Extension.Name = XRT_STR_LITERAL("x-test");
	Extension.Parameters = XRT_STR_LITERAL("p=\"has space\"");
	iOffset = 0;
	iBefore = iOffset;
	memset(&Param, 0xA5, sizeof(Param));
	ParamBefore = Param;
	testRequire(
		xrtWsExtensionParamNext(
			&Extension,
			&iOffset,
			&Param
		) == XHTTP_NEXT_ERROR &&
		(iOffset == iBefore) &&
		(memcmp(
			&Param,
			&ParamBefore,
			sizeof(Param)
		) == 0),
		"WebSocket parameter failure was not atomic"
	);
	testExtensionError(
		XWS_HANDSHAKE_ERROR_EXTENSION,
		"WebSocket parameter token error mismatch"
	);

	iOffset = 0;
	testRequire(
		xrtWsExtensionNext(
			XRT_STR_LITERAL("valid, broken="),
			&iOffset,
			&Extension
		) == XHTTP_NEXT_ITEM &&
		!xrtWsExtensionCount(
			XRT_STR_LITERAL("valid, broken="),
			&iBefore
		),
		"WebSocket extension count ignored malformed suffix"
	);
	testExtensionError(
		XWS_HANDSHAKE_ERROR_EXTENSION,
		"WebSocket extension suffix error mismatch"
	);
}



/* 验证扩展写出长度查询、保留参数和失败原子性。 */
static void testExtensionWrite(void)
{
	xstrview Name = XRT_STR_LITERAL("x-test");
	xstrview Parameters = XRT_STR_LITERAL(
		"mode=fast; token=\"web\\socket\""
	);
	char Output[64];
	char Small[8];
	size_t iSize = 0;

	testRequire(
		xrtWsExtensionWrite(
			Name,
			Parameters,
			NULL,
			0,
			&iSize
		) &&
		(iSize == 37u),
		"WebSocket extension size query mismatch"
	);
	memset(Output, 0, sizeof(Output));
	testRequire(
		xrtWsExtensionWrite(
			Name,
			Parameters,
			Output,
			sizeof(Output),
			&iSize
		) &&
		(iSize == 37u) &&
		(memcmp(
			Output,
			"x-test; mode=fast; token=\"web\\socket\"",
			iSize
		) == 0),
		"WebSocket extension write mismatch"
	);

	memset(Small, 0xA5, sizeof(Small));
	testRequire(
		!xrtWsExtensionWrite(
			Name,
			Parameters,
			Small,
			sizeof(Small),
			&iSize
		) &&
		(iSize == 37u) &&
		((unsigned char)Small[0] == UINT8_C(0xA5)),
		"WebSocket short extension output was not atomic"
	);
	testExtensionError(
		XWS_HANDSHAKE_ERROR_OUTPUT,
		"WebSocket extension output error mismatch"
	);

	testRequire(
		!xrtWsExtensionWrite(
			XRT_STR_LITERAL("bad name"),
			(xstrview){ NULL, 0 },
			Output,
			sizeof(Output),
			&iSize
		),
		"WebSocket invalid extension name was written"
	);
	testExtensionError(
		XWS_HANDSHAKE_ERROR_EXTENSION,
		"WebSocket extension name error mismatch"
	);

	testRequire(
		!xrtWsExtensionWrite(
			XRT_STR_LITERAL("x-test"),
			XRT_STR_LITERAL("p=\"has space\""),
			Output,
			sizeof(Output),
			&iSize
		),
		"WebSocket invalid extension parameter was written"
	);
	testExtensionError(
		XWS_HANDSHAKE_ERROR_EXTENSION,
		"WebSocket extension parameter write error mismatch"
	);
}



/* 验证未对齐固定结构、地址回绕、重叠与失败原子性。 */
static void testExtensionMemoryContracts(void)
{
	uint8 OffsetStorage[sizeof(size_t) + 2u];
	uint8 ExtensionStorage[sizeof(xwsextension) + 2u];
	uint8 CountStorage[sizeof(size_t) + 2u];
	uint8 DescriptorStorage[sizeof(xwsextension) + 2u];
	uint8 ParamStorage[sizeof(xhttpparam) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	uint8 OutputStorage[32];
	xstrview Text = XRT_STR_LITERAL("x-test; mode=fast");
	xstrview Wrapping = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
		4u
	};
	xwsextension Extension;
	xwsextension Before;
	xhttpparam Param;
	size_t iOffset = 0;
	size_t iCount;
	size_t iSize;

	memset(OffsetStorage, 0xA5, sizeof(OffsetStorage));
	memset(ExtensionStorage, 0xA5, sizeof(ExtensionStorage));
	memcpy(OffsetStorage + 1u, &iOffset, sizeof(iOffset));
	testRequire(
		xrtWsExtensionNext(
			Text,
			(size_t*)(void*)(OffsetStorage + 1u),
			(xwsextension*)(void*)(ExtensionStorage + 1u)
		) == XHTTP_NEXT_ITEM,
		"WebSocket extension iterator rejected unaligned outputs"
	);
	memcpy(&iOffset, OffsetStorage + 1u, sizeof(iOffset));
	memcpy(&Extension, ExtensionStorage + 1u, sizeof(Extension));
	testRequire(
		(OffsetStorage[0] == UINT8_C(0xA5)) &&
		(OffsetStorage[sizeof(OffsetStorage) - 1u] == UINT8_C(0xA5)) &&
		(ExtensionStorage[0] == UINT8_C(0xA5)) &&
		(ExtensionStorage[sizeof(ExtensionStorage) - 1u] ==
		 UINT8_C(0xA5)) &&
		(iOffset == Text.Size) &&
		xrtHttpTokenEqual(
			Extension.Name,
			XRT_STR_LITERAL("x-test")
		),
		"WebSocket extension iterator corrupted unaligned storage"
	);

	memset(DescriptorStorage, 0xA5, sizeof(DescriptorStorage));
	memset(OffsetStorage, 0xA5, sizeof(OffsetStorage));
	memset(ParamStorage, 0xA5, sizeof(ParamStorage));
	memcpy(DescriptorStorage + 1u, &Extension, sizeof(Extension));
	iOffset = 0;
	memcpy(OffsetStorage + 1u, &iOffset, sizeof(iOffset));
	testRequire(
		xrtWsExtensionParamNext(
			(const xwsextension*)(const void*)(DescriptorStorage + 1u),
			(size_t*)(void*)(OffsetStorage + 1u),
			(xhttpparam*)(void*)(ParamStorage + 1u)
		) == XHTTP_NEXT_ITEM,
		"WebSocket extension parameter rejected unaligned structures"
	);
	memcpy(&iOffset, OffsetStorage + 1u, sizeof(iOffset));
	memcpy(&Param, ParamStorage + 1u, sizeof(Param));
	testRequire(
		(DescriptorStorage[0] == UINT8_C(0xA5)) &&
		(DescriptorStorage[sizeof(DescriptorStorage) - 1u] ==
		 UINT8_C(0xA5)) &&
		(OffsetStorage[0] == UINT8_C(0xA5)) &&
		(OffsetStorage[sizeof(OffsetStorage) - 1u] == UINT8_C(0xA5)) &&
		(ParamStorage[0] == UINT8_C(0xA5)) &&
		(ParamStorage[sizeof(ParamStorage) - 1u] == UINT8_C(0xA5)) &&
		(iOffset == Extension.Parameters.Size) &&
		xrtHttpTokenEqual(
			Param.Name,
			XRT_STR_LITERAL("mode")
		),
		"WebSocket extension parameter corrupted unaligned storage"
	);

	memset(CountStorage, 0xA5, sizeof(CountStorage));
	testRequire(
		xrtWsExtensionCount(
			Text,
			(size_t*)(void*)(CountStorage + 1u)
		),
		"WebSocket extension count rejected unaligned output"
	);
	memcpy(&iCount, CountStorage + 1u, sizeof(iCount));
	testRequire(
		(CountStorage[0] == UINT8_C(0xA5)) &&
		(CountStorage[sizeof(CountStorage) - 1u] == UINT8_C(0xA5)) &&
		(iCount == 1u),
		"WebSocket extension count corrupted unaligned output"
	);

	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memset(OutputStorage, 0xA5, sizeof(OutputStorage));
	testRequire(
		xrtWsExtensionWrite(
			XRT_STR_LITERAL("x-test"),
			XRT_STR_LITERAL("mode=fast"),
			OutputStorage + 1u,
			sizeof(OutputStorage) - 2u,
			(size_t*)(void*)(SizeStorage + 1u)
		),
		"WebSocket extension writer rejected unaligned length output"
	);
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire(
		(SizeStorage[0] == UINT8_C(0xA5)) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == UINT8_C(0xA5)) &&
		(OutputStorage[0] == UINT8_C(0xA5)) &&
		(OutputStorage[sizeof(OutputStorage) - 1u] == UINT8_C(0xA5)) &&
		(iSize == 17u) &&
		(memcmp(OutputStorage + 1u, "x-test; mode=fast", iSize) == 0),
		"WebSocket extension writer corrupted unaligned storage"
	);

	iCount = 73u;
	testRequire(
		!xrtWsExtensionCount(
			XRT_STR_LITERAL("valid, broken="),
			&iCount
		) && (iCount == 73u),
		"WebSocket extension count failure changed its output"
	);
	xrtClearError();

	Extension.Name = XRT_STR_LITERAL("unchanged");
	Extension.Parameters = XRT_STR_LITERAL("p=v");
	Before = Extension;
	iOffset = 0;
	testRequire(
		xrtWsExtensionNext(
			Wrapping,
			&iOffset,
			&Extension
		) == XHTTP_NEXT_ERROR &&
		(iOffset == 0) &&
		(memcmp(&Extension, &Before, sizeof(Extension)) == 0) &&
		!xrtWsExtensionCount(Wrapping, &iCount),
		"WebSocket extension parser accepted a wrapping view"
	);
	xrtClearError();

	Extension.Name = Wrapping;
	Extension.Parameters = (xstrview){ NULL, 0 };
	iOffset = 0;
	memset(&Param, 0xA5, sizeof(Param));
	testRequire(
		xrtWsExtensionParamNext(
			&Extension,
			&iOffset,
			&Param
		) == XHTTP_NEXT_ERROR &&
		(iOffset == 0) &&
		!xrtWsExtensionWrite(
			Wrapping,
			(xstrview){ NULL, 0 },
			OutputStorage,
			sizeof(OutputStorage),
			&iSize
		) &&
		!xrtWsExtensionWrite(
			XRT_STR_LITERAL("x-test"),
			Wrapping,
			OutputStorage,
			sizeof(OutputStorage),
			&iSize
		),
		"WebSocket extension APIs accepted wrapping borrowed ranges"
	);
	xrtClearError();

	iOffset = 0;
	Extension = Before;
	testRequire(
		xrtWsExtensionNext(
			Text,
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u),
			&Extension
		) == XHTTP_NEXT_ERROR &&
		xrtWsExtensionNext(
			Text,
			&iOffset,
			(xwsextension*)(uintptr_t)(UINTPTR_MAX - 1u)
		) == XHTTP_NEXT_ERROR &&
		xrtWsExtensionParamNext(
			(const xwsextension*)(uintptr_t)(UINTPTR_MAX - 1u),
			&iOffset,
			&Param
		) == XHTTP_NEXT_ERROR &&
		!xrtWsExtensionCount(
			Text,
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		!xrtWsExtensionWrite(
			XRT_STR_LITERAL("x-test"),
			(xstrview){ NULL, 0 },
			(void*)(uintptr_t)(UINTPTR_MAX - 1u),
			4u,
			&iSize
		) &&
		!xrtWsExtensionWrite(
			XRT_STR_LITERAL("x-test"),
			(xstrview){ NULL, 0 },
			OutputStorage,
			sizeof(OutputStorage),
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
		),
		"WebSocket extension APIs accepted wrapping fixed outputs"
	);
	testExtensionError(
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		"WebSocket extension memory contract error mismatch"
	);

	iOffset = 0;
	Extension = Before;
	testRequire(
		xrtWsExtensionNext(
			Text,
			(size_t*)(void*)Text.Data,
			&Extension
		) == XHTTP_NEXT_ERROR &&
		xrtWsExtensionParamNext(
			&Before,
			(size_t*)(void*)&Before,
			&Param
		) == XHTTP_NEXT_ERROR &&
		!xrtWsExtensionCount(
			Text,
			(size_t*)(void*)Text.Data
		),
		"WebSocket extension APIs accepted overlapping outputs"
	);
	testExtensionError(
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		"WebSocket extension overlap error mismatch"
	);
}



/* 执行 WebSocket 扩展字段契约测试。 */
int main(void)
{
	testExtensionParse();
	testExtensionInvalid();
	testExtensionWrite();
	testExtensionMemoryContracts();
	printf("[PASS] websocket_extension\n");
	return 0;
}
