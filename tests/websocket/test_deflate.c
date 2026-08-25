#include "../test.h"



/* 检查最近一次错误属于稳定 permessage-deflate 错误域。 */
static void testDeflateError(
	xwsdeflateerror Code,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.websocket.deflate"
		) == 0) &&
		(xrtErrorCode(pError) == (int32)Code),
		sMessage
	);
	xrtClearError();
}



/* 把单个完整字段值解析为借用扩展视图。 */
static xwsextension testDeflateExtension(xstrview Text)
{
	xwsextension Extension;
	size_t iOffset = 0;

	testRequire(
		xrtWsExtensionNext(
			Text,
			&iOffset,
			&Extension
		) == XHTTP_NEXT_ITEM &&
		xrtWsExtensionNext(
			Text,
			&iOffset,
			&Extension
		) == XHTTP_NEXT_END,
		"permessage-deflate extension fixture is invalid"
	);
	iOffset = 0;
	testRequire(
		xrtWsExtensionNext(
			Text,
			&iOffset,
			&Extension
		) == XHTTP_NEXT_ITEM,
		"permessage-deflate extension fixture could not be restored"
	);
	return Extension;
}



/* 验证 offer 与 response 的四个参数、大小写和 quoted 值。 */
static void testDeflateParse(void)
{
	xwsextension OfferExtension = testDeflateExtension(
		XRT_STR_LITERAL(
			"PerMessage-Deflate; SERVER_NO_CONTEXT_TAKEOVER; "
			"client_no_context_takeover; "
			"server_max_window_bits=\"1\\0\"; "
			"client_max_window_bits"
		)
	);
	xwsextension ResponseExtension = testDeflateExtension(
		XRT_STR_LITERAL(
			"permessage-deflate; server_no_context_takeover; "
			"client_no_context_takeover; "
			"server_max_window_bits=9; "
			"client_max_window_bits=\"1\\2\""
		)
	);
	xwsdeflate Offer;
	xwsdeflate Response;
	char Output[8];
	size_t iSize;
	const xerror* pBefore;
	uint32 iAllOffer =
		XWS_DEFLATE_SERVER_NO_CONTEXT |
		XWS_DEFLATE_CLIENT_NO_CONTEXT |
		XWS_DEFLATE_SERVER_MAX_WINDOW |
		XWS_DEFLATE_CLIENT_MAX_WINDOW |
		XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY;
	uint32 iAllResponse =
		XWS_DEFLATE_SERVER_NO_CONTEXT |
		XWS_DEFLATE_CLIENT_NO_CONTEXT |
		XWS_DEFLATE_SERVER_MAX_WINDOW |
		XWS_DEFLATE_CLIENT_MAX_WINDOW;

	testRequire(
		(XWS_DEFLATE_WINDOW_MIN == 8u) &&
		(XWS_DEFLATE_WINDOW_MAX == 15u) &&
		(XWS_DEFLATE_MAX_SIZE == 128u) &&
		xrtWsDeflateIs(&OfferExtension),
		"permessage-deflate constants or name predicate mismatch"
	);
	testRequire(
		xrtWsDeflateOfferParse(
			&OfferExtension,
			&Offer
		) &&
		(Offer.Flags == iAllOffer) &&
		(Offer.ServerMaxWindowBits == 10u) &&
		(Offer.ClientMaxWindowBits == 15u),
		"permessage-deflate offer parse mismatch"
	);
	testRequire(
		xrtWsDeflateResponseParse(
			&ResponseExtension,
			&Response
		) &&
		(Response.Flags == iAllResponse) &&
		(Response.ServerMaxWindowBits == 9u) &&
		(Response.ClientMaxWindowBits == 12u),
		"permessage-deflate response parse mismatch"
	);

	testRequire(
		!xrtWsDeflateOfferWrite(
			NULL,
			Output,
			sizeof(Output),
			&iSize
		),
		"permessage-deflate predicate error fixture failed"
	);
	pBefore = xrtGetError();
	testRequire(
		(pBefore != NULL) &&
		xrtWsDeflateIs(&OfferExtension) &&
		(xrtGetError() == pBefore),
		"permessage-deflate name predicate replaced the current error"
	);
	xrtClearError();
}



/* 验证无参数、未知项、重复项和窗口值的严格拒绝规则。 */
static void testDeflateInvalid(void)
{
	static const xstrview InvalidOffer[] = {
		XRT_STR_INIT("x-deflate"),
		XRT_STR_INIT("permessage-deflate; unknown"),
		XRT_STR_INIT(
			"permessage-deflate; server_no_context_takeover=1"
		),
		XRT_STR_INIT(
			"permessage-deflate; client_no_context_takeover=1"
		),
		XRT_STR_INIT(
			"permessage-deflate; server_max_window_bits"
		),
		XRT_STR_INIT(
			"permessage-deflate; server_max_window_bits=7"
		),
		XRT_STR_INIT(
			"permessage-deflate; server_max_window_bits=16"
		),
		XRT_STR_INIT(
			"permessage-deflate; server_max_window_bits=08"
		),
		XRT_STR_INIT(
			"permessage-deflate; client_max_window_bits=0"
		),
		XRT_STR_INIT(
			"permessage-deflate; client_max_window_bits=15; "
			"CLIENT_MAX_WINDOW_BITS=14"
		)
	};
	xwsdeflate Config;
	xwsdeflate Before;
	size_t i;

	for ( i = 0;
		i < (sizeof(InvalidOffer) / sizeof(InvalidOffer[0]));
		i++ ) {
		xwsextension Extension =
			testDeflateExtension(InvalidOffer[i]);

		memset(&Config, 0xA5, sizeof(Config));
		Before = Config;
		testRequire(
			!xrtWsDeflateOfferParse(
				&Extension,
				&Config
			) &&
			(memcmp(
				&Config,
				&Before,
				sizeof(Config)
			) == 0),
			"invalid permessage-deflate offer was accepted or mutated output"
		);
		xrtClearError();
	}

	{
		xwsextension Extension = testDeflateExtension(
			XRT_STR_LITERAL(
				"permessage-deflate; client_max_window_bits"
			)
		);

		testRequire(
			!xrtWsDeflateResponseParse(
				&Extension,
				&Config
			),
			"valueless response client_max_window_bits was accepted"
		);
		testDeflateError(
			XWS_DEFLATE_ERROR_WINDOW,
			"permessage-deflate response window error mismatch"
		);
	}

	{
		xwsextension Extension = testDeflateExtension(
			XRT_STR_LITERAL(
				"permessage-deflate; unknown=value"
			)
		);

		testRequire(
			!xrtWsDeflateOfferParse(
				&Extension,
				&Config
			),
			"unknown permessage-deflate parameter was accepted"
		);
		testDeflateError(
			XWS_DEFLATE_ERROR_PARAMETER,
			"permessage-deflate unknown parameter error mismatch"
		);
	}

	{
		xwsextension Extension = testDeflateExtension(
			XRT_STR_LITERAL(
				"permessage-deflate; server_max_window_bits=10; "
				"SERVER_MAX_WINDOW_BITS=9"
			)
		);

		testRequire(
			!xrtWsDeflateOfferParse(
				&Extension,
				&Config
			),
			"duplicate permessage-deflate parameter was accepted"
		);
		testDeflateError(
			XWS_DEFLATE_ERROR_DUPLICATE,
			"permessage-deflate duplicate parameter error mismatch"
		);
	}
}



/* 验证协商参数到本地四个收发方向的映射和失败原子性。 */
static void testDeflateDirection(void)
{
	union {
		xwsdeflate Response;
		xwsdeflatedirection Direction;
	} Overlap;
	xwsdeflate Response;
	xwsdeflate Before;
	xwsdeflatedirection Direction;

	xrtWsDeflateInit(&Response);
	Response.Flags =
		XWS_DEFLATE_SERVER_NO_CONTEXT |
		XWS_DEFLATE_SERVER_MAX_WINDOW |
		XWS_DEFLATE_CLIENT_MAX_WINDOW;
	Response.ServerMaxWindowBits = 10u;
	Response.ClientMaxWindowBits = 12u;

	testRequire(
		xrtWsDeflateDirection(
			&Response,
			XWS_ROLE_SERVER,
			true,
			&Direction
		) &&
		(Direction.WindowBits == 10u) &&
		Direction.NoContextTakeover,
		"server send Deflate direction mismatch"
	);
	testRequire(
		xrtWsDeflateDirection(
			&Response,
			XWS_ROLE_SERVER,
			false,
			&Direction
		) &&
		(Direction.WindowBits == 12u) &&
		!Direction.NoContextTakeover,
		"server receive Deflate direction mismatch"
	);
	testRequire(
		xrtWsDeflateDirection(
			&Response,
			XWS_ROLE_CLIENT,
			true,
			&Direction
		) &&
		(Direction.WindowBits == 12u) &&
		!Direction.NoContextTakeover,
		"client send Deflate direction mismatch"
	);
	testRequire(
		xrtWsDeflateDirection(
			&Response,
			XWS_ROLE_CLIENT,
			false,
			&Direction
		) &&
		(Direction.WindowBits == 10u) &&
		Direction.NoContextTakeover,
		"client receive Deflate direction mismatch"
	);

	memset(&Direction, 0xA5, sizeof(Direction));
	testRequire(
		!xrtWsDeflateDirection(
			&Response,
			(xwsrole)77,
			true,
			&Direction
		) &&
		(Direction.WindowBits == UINT8_C(0xA5)),
		"invalid Deflate role mutated direction"
	);
	xrtClearError();

	Overlap.Response = Response;
	Before = Overlap.Response;
	testRequire(
		!xrtWsDeflateDirection(
			&Overlap.Response,
			XWS_ROLE_CLIENT,
			true,
			&Overlap.Direction
		) &&
		(memcmp(
			&Overlap.Response,
			&Before,
			sizeof(Before)
		 ) == 0),
		"overlapping Deflate direction was not failure-atomic"
	);
	xrtClearError();
}



/* 验证规范写出、精确最大长度和解析往返。 */
static void testDeflateWrite(void)
{
	xwsdeflate Config;
	xwsdeflate Parsed;
	xwsextension Extension;
	struct {
		char Output[16];
		unsigned char Gap[8];
		size_t Size;
	} Short;
	char Output[XWS_DEFLATE_MAX_SIZE];
	size_t iOffset;
	size_t iSize;

	xrtWsDeflateInit(&Config);
	Config.Flags =
		XWS_DEFLATE_SERVER_NO_CONTEXT |
		XWS_DEFLATE_CLIENT_NO_CONTEXT |
		XWS_DEFLATE_SERVER_MAX_WINDOW |
		XWS_DEFLATE_CLIENT_MAX_WINDOW;
	testRequire(
		xrtWsDeflateResponseWrite(
			&Config,
			NULL,
			0,
			&iSize
		) &&
		(iSize == XWS_DEFLATE_MAX_SIZE),
		"permessage-deflate maximum size query mismatch"
	);
	testRequire(
		xrtWsDeflateResponseWrite(
			&Config,
			Output,
			sizeof(Output),
			&iSize
		),
		"permessage-deflate response write failed"
	);
	iOffset = 0;
	testRequire(
		xrtWsExtensionNext(
			(xstrview){ Output, iSize },
			&iOffset,
			&Extension
		) == XHTTP_NEXT_ITEM &&
		xrtWsDeflateResponseParse(
			&Extension,
			&Parsed
		) &&
		(memcmp(
			&Config,
			&Parsed,
			sizeof(Config)
		) == 0),
		"permessage-deflate response write round-trip mismatch"
	);

	Config.Flags |= XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY;
	testRequire(
		xrtWsDeflateOfferWrite(
			&Config,
			Output,
			sizeof(Output),
			&iSize
		) &&
		(memcmp(
			Output + iSize -
				sizeof("client_max_window_bits") + 1u,
			"client_max_window_bits",
			sizeof("client_max_window_bits") - 1u
		) == 0),
		"permessage-deflate valueless offer write mismatch"
	);

	memset(&Short, 0xA5, sizeof(Short));
	testRequire(
		!xrtWsDeflateOfferWrite(
			&Config,
			Short.Output,
			sizeof(Short.Output),
			&Short.Size
		) &&
		(Short.Size > sizeof(Short.Output)) &&
		((unsigned char)Short.Output[0] == UINT8_C(0xA5)),
		"permessage-deflate short output was not atomic"
	);
	testDeflateError(
		XWS_DEFLATE_ERROR_OUTPUT,
		"permessage-deflate output error mismatch"
	);
}



/* 验证未对齐结构、地址回绕和固定输出的失败原子性。 */
static void testDeflateMemoryContracts(void)
{
	uint8 ConfigStorage[sizeof(xwsdeflate) + 2u];
	uint8 ParsedStorage[sizeof(xwsdeflate) + 2u];
	uint8 ExtensionStorage[sizeof(xwsextension) + 2u];
	uint8 DirectionStorage[sizeof(xwsdeflatedirection) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	uint8 OutputStorage[XWS_DEFLATE_MAX_SIZE + 2u];
	xstrview Wrapping = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
		4u
	};
	xwsextension Extension = testDeflateExtension(
		XRT_STR_LITERAL(
			"permessage-deflate; server_no_context_takeover; "
			"server_max_window_bits=10; client_max_window_bits"
		)
	);
	xwsdeflate Config;
	xwsdeflate Parsed;
	xwsdeflate Before;
	xwsdeflatedirection Direction;
	size_t iSize;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtWsDeflateInit(
		(xwsdeflate*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire(
		(ConfigStorage[0] == UINT8_C(0xA5)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == UINT8_C(0xA5)) &&
		(Config.Flags == 0) &&
		(Config.ServerMaxWindowBits == XWS_DEFLATE_WINDOW_MAX) &&
		(Config.ClientMaxWindowBits == XWS_DEFLATE_WINDOW_MAX),
		"permessage-deflate initializer corrupted unaligned storage"
	);

	memset(ExtensionStorage, 0xA5, sizeof(ExtensionStorage));
	memset(ParsedStorage, 0xA5, sizeof(ParsedStorage));
	memcpy(ExtensionStorage + 1u, &Extension, sizeof(Extension));
	testRequire(
		xrtWsDeflateIs(
			(const xwsextension*)(const void*)(ExtensionStorage + 1u)
		) &&
		xrtWsDeflateOfferParse(
			(const xwsextension*)(const void*)(ExtensionStorage + 1u),
			(xwsdeflate*)(void*)(ParsedStorage + 1u)
		),
		"permessage-deflate rejected unaligned parse structures"
	);
	memcpy(&Parsed, ParsedStorage + 1u, sizeof(Parsed));
	testRequire(
		(ExtensionStorage[0] == UINT8_C(0xA5)) &&
		(ExtensionStorage[sizeof(ExtensionStorage) - 1u] ==
		 UINT8_C(0xA5)) &&
		(ParsedStorage[0] == UINT8_C(0xA5)) &&
		(ParsedStorage[sizeof(ParsedStorage) - 1u] == UINT8_C(0xA5)) &&
		((Parsed.Flags & XWS_DEFLATE_SERVER_NO_CONTEXT) != 0) &&
		(Parsed.ServerMaxWindowBits == 10u),
		"permessage-deflate parse corrupted unaligned storage"
	);

	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memset(OutputStorage, 0xA5, sizeof(OutputStorage));
	testRequire(
		xrtWsDeflateOfferWrite(
			(const xwsdeflate*)(const void*)(ParsedStorage + 1u),
			OutputStorage + 1u,
			sizeof(OutputStorage) - 2u,
			(size_t*)(void*)(SizeStorage + 1u)
		),
		"permessage-deflate rejected unaligned write structures"
	);
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire(
		(SizeStorage[0] == UINT8_C(0xA5)) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == UINT8_C(0xA5)) &&
		(OutputStorage[0] == UINT8_C(0xA5)) &&
		(OutputStorage[sizeof(OutputStorage) - 1u] == UINT8_C(0xA5)) &&
		(iSize < XWS_DEFLATE_MAX_SIZE),
		"permessage-deflate writer corrupted unaligned storage"
	);

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memcpy(ConfigStorage + 1u, &Parsed, sizeof(Parsed));
	memset(ParsedStorage, 0xA5, sizeof(ParsedStorage));
	testRequire(
		xrtWsDeflateAccept(
			(const xwsdeflate*)(const void*)(ConfigStorage + 1u),
			(xwsdeflate*)(void*)(ParsedStorage + 1u)
		),
		"permessage-deflate rejected unaligned acceptance structures"
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	memcpy(&Parsed, ParsedStorage + 1u, sizeof(Parsed));
	testRequire(
		xrtWsDeflateResponseCheck(
			(const xwsdeflate*)(const void*)(ConfigStorage + 1u),
			(const xwsdeflate*)(const void*)(ParsedStorage + 1u)
		),
		"permessage-deflate rejected unaligned response check"
	);
	memset(DirectionStorage, 0xA5, sizeof(DirectionStorage));
	testRequire(
		xrtWsDeflateDirection(
			(const xwsdeflate*)(const void*)(ParsedStorage + 1u),
			XWS_ROLE_CLIENT,
			false,
			(xwsdeflatedirection*)(void*)(DirectionStorage + 1u)
		),
		"permessage-deflate rejected unaligned direction output"
	);
	memcpy(&Direction, DirectionStorage + 1u, sizeof(Direction));
	testRequire(
		(ConfigStorage[0] == UINT8_C(0xA5)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == UINT8_C(0xA5)) &&
		(ParsedStorage[0] == UINT8_C(0xA5)) &&
		(ParsedStorage[sizeof(ParsedStorage) - 1u] == UINT8_C(0xA5)) &&
		(DirectionStorage[0] == UINT8_C(0xA5)) &&
		(DirectionStorage[sizeof(DirectionStorage) - 1u] ==
		 UINT8_C(0xA5)) &&
		(Direction.WindowBits == 10u) &&
		Direction.NoContextTakeover,
		"permessage-deflate negotiation corrupted unaligned storage"
	);

	testRequire(
		xrtWsDeflateAccept(
			(const xwsdeflate*)(const void*)(ConfigStorage + 1u),
			(xwsdeflate*)(void*)(ConfigStorage + 1u)
		),
		"permessage-deflate rejected unaligned in-place acceptance"
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire(
		(Config.Flags == (
			XWS_DEFLATE_SERVER_NO_CONTEXT |
			XWS_DEFLATE_SERVER_MAX_WINDOW
		)) && (Config.ServerMaxWindowBits == 10u),
		"permessage-deflate in-place acceptance result mismatch"
	);

	xrtClearError();
	xrtWsDeflateInit(
		(xwsdeflate*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testDeflateError(
		XWS_DEFLATE_ERROR_ARGUMENT,
		"permessage-deflate wrapping init error mismatch"
	);

	Before = Parsed;
	Extension.Name = Wrapping;
	Extension.Parameters = (xstrview){ NULL, 0 };
	testRequire(
		!xrtWsDeflateIs(
			(const xwsextension*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		!xrtWsDeflateIs(&Extension) &&
		!xrtWsDeflateOfferParse(&Extension, &Parsed) &&
		(memcmp(&Parsed, &Before, sizeof(Parsed)) == 0) &&
		!xrtWsDeflateOfferParse(
			(const xwsextension*)(uintptr_t)(UINTPTR_MAX - 1u),
			&Parsed
		) &&
		!xrtWsDeflateOfferParse(
			(const xwsextension*)(const void*)ExtensionStorage,
			(xwsdeflate*)(uintptr_t)(UINTPTR_MAX - 1u)
		),
		"permessage-deflate parser accepted wrapping ranges"
	);
	xrtClearError();

	testRequire(
		!xrtWsDeflateOfferWrite(
			(const xwsdeflate*)(uintptr_t)(UINTPTR_MAX - 1u),
			OutputStorage,
			sizeof(OutputStorage),
			&iSize
		) &&
		!xrtWsDeflateOfferWrite(
			&Before,
			(void*)(uintptr_t)(UINTPTR_MAX - 1u),
			4u,
			&iSize
		) &&
		!xrtWsDeflateOfferWrite(
			&Before,
			OutputStorage,
			sizeof(OutputStorage),
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		!xrtWsDeflateAccept(
			(const xwsdeflate*)(uintptr_t)(UINTPTR_MAX - 1u),
			&Parsed
		) &&
		!xrtWsDeflateAccept(
			&Before,
			(xwsdeflate*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		!xrtWsDeflateResponseCheck(
			(const xwsdeflate*)(uintptr_t)(UINTPTR_MAX - 1u),
			&Before
		) &&
		!xrtWsDeflateDirection(
			&Before,
			XWS_ROLE_CLIENT,
			true,
			(xwsdeflatedirection*)(uintptr_t)(UINTPTR_MAX - 1u)
		),
		"permessage-deflate APIs accepted wrapping fixed ranges"
	);
	testDeflateError(
		XWS_DEFLATE_ERROR_ARGUMENT,
		"permessage-deflate wrapping output error mismatch"
	);
}



/* 执行 permessage-deflate 基础协议测试。 */
int main(void)
{
	testDeflateParse();
	testDeflateInvalid();
	testDeflateDirection();
	testDeflateWrite();
	testDeflateMemoryContracts();
	printf("[PASS] websocket_deflate\n");
	return 0;
}
