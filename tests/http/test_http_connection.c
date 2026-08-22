#include "../test.h"

#include <xrt/http_connection.h>



/* 验证重复字段按线路顺序发布，并在首项前校验完整输入。 */
static void testHttpConnectionOptions(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Connection"),
			XRT_STR_INIT("close, X-Hop")
		},
		{
			XRT_STR_INIT("connection"),
			XRT_STR_INIT("Upgrade")
		},
		{
			XRT_STR_INIT("X-Hop"),
			XRT_STR_INIT("private")
		}
	};
	static const xhttpfield Invalid[] = {
		{
			XRT_STR_INIT("Connection"),
			XRT_STR_INIT("X-Hop")
		},
		{
			XRT_STR_INIT("Connection"),
			XRT_STR_INIT("(")
		}
	};
	xhttpfieldtokencursor Cursor;
	xstrview Option;
	size_t iOptions = 0;

	xrtHttpConnectionCursorInit(&Cursor);
	while ( xrtHttpConnectionNext(
		Fields, 3u, &Cursor, &Option
	) == XHTTP_NEXT_ITEM ) {
		iOptions++;
	}
	testRequire(
		(iOptions == 3u) && (xrtGetError() == NULL),
		"Connection option cursor order or count mismatch"
	);
	testRequire(
		xrtHttpConnectionCount(
			Fields, 3u, &iOptions
		) && (iOptions == 3u),
		"Connection option count mismatch"
	);
	testRequire(
		xrtHttpConnectionFind(
			Fields, 3u, XRT_STR_LITERAL("x-hop")
		) == XHTTP_NEXT_ITEM,
		"Connection option lookup missed repeated field"
	);
	testRequire(
		xrtHttpConnectionFind(
			Fields, 3u, XRT_STR_LITERAL("keep-alive")
		) == XHTTP_NEXT_END,
		"Connection option lookup reported a missing item"
	);
	testRequire(
		xrtHttpConnectionFind(
			Invalid, 2u, XRT_STR_LITERAL("X-Hop")
		) == XHTTP_NEXT_ERROR,
		"Connection lookup published before validating all fields"
	);
	xrtClearError();
	xrtHttpConnectionCursorInit(&Cursor);
	testRequire(
		xrtHttpConnectionNext(
			Invalid, 2u, &Cursor, &Option
		) == XHTTP_NEXT_ERROR,
		"Connection cursor published before full validation"
	);
	xrtClearError();
}



/* 验证空字段、空字段数组和非法目标选项的边界。 */
static void testHttpConnectionBoundaries(void)
{
	static const xhttpfield Empty = {
		XRT_STR_INIT("Connection"),
		XRT_STR_INIT("")
	};
	xhttpfieldtokencursor Cursor;
	xstrview Option = XRT_STR_LITERAL("sentinel");
	size_t iCount;

	testRequire(
		xrtHttpConnectionFind(
			NULL, 0, XRT_STR_LITERAL("close")
		) == XHTTP_NEXT_END,
		"empty Connection field set did not return END"
	);
	testRequire(
		xrtHttpConnectionFind(
			&Empty, 1u, XRT_STR_LITERAL("close")
		) == XHTTP_NEXT_END,
		"empty Connection field value was rejected"
	);
	testRequire(
		xrtHttpConnectionCount(&Empty, 1u, &iCount) &&
		(iCount == 0),
		"empty Connection field did not produce zero options"
	);
	testRequire(
		xrtHttpConnectionFind(
			NULL, 0, XRT_STR_LITERAL("bad option")
		) == XHTTP_NEXT_ERROR,
		"invalid Connection lookup token was accepted"
	);
	xrtClearError();
	xrtHttpConnectionCursorInit(&Cursor);
	testRequire(
		xrtHttpConnectionNext(
			NULL, 0, &Cursor, &Option
		) == XHTTP_NEXT_END,
		"empty Connection cursor did not return END"
	);
	testRequire(
		(Option.Data == NULL) && (Option.Size == 0),
		"Connection END did not clear the output view"
	);
}



/* 验证 HTTP/1.1 默认持久与 HTTP/1.0 兼容机制的完整判定矩阵。 */
static void testHttpConnectionPersistence(void)
{
	static const xhttpfield Close = {
		XRT_STR_INIT("Connection"),
		XRT_STR_INIT("keep-alive, close")
	};
	static const xhttpfield KeepAlive = {
		XRT_STR_INIT("Connection"),
		XRT_STR_INIT("keep-alive")
	};
	static const xhttpfield Invalid = {
		XRT_STR_INIT("Connection"),
		XRT_STR_INIT("bad option")
	};
	const uint32 iAllow =
		(uint32)XHTTP_CONNECTION_ALLOW_HTTP10_KEEP_ALIVE;

	testRequire(
		xrtHttpConnectionPersistence(
			XHTTP_VERSION_1_1, NULL, 0, 0
		) == XHTTP_CONNECTION_PERSIST,
		"HTTP/1.1 did not default to a persistent connection"
	);
	testRequire(
		xrtHttpConnectionPersistence(
			XHTTP_VERSION_1_1, &Close, 1u, 0
		) == XHTTP_CONNECTION_CLOSE,
		"close did not override HTTP/1.1 persistence"
	);
	testRequire(
		xrtHttpConnectionPersistence(
			XHTTP_VERSION_1_0, &KeepAlive, 1u, 0
		) == XHTTP_CONNECTION_CLOSE,
		"HTTP/1.0 keep-alive bypassed local policy"
	);
	testRequire(
		xrtHttpConnectionPersistence(
			XHTTP_VERSION_1_0, &KeepAlive, 1u, iAllow
		) == XHTTP_CONNECTION_PERSIST,
		"HTTP/1.0 endpoint keep-alive was not honored"
	);
	testRequire(
		xrtHttpConnectionPersistence(
			XHTTP_VERSION_1_0,
			&KeepAlive,
			1u,
			iAllow | (uint32)XHTTP_CONNECTION_PROXY
		) == XHTTP_CONNECTION_CLOSE,
		"proxy kept an HTTP/1.0 request connection persistent"
	);
	testRequire(
		xrtHttpConnectionPersistence(
			XHTTP_VERSION_1_0,
			&KeepAlive,
			1u,
			iAllow |
				(uint32)XHTTP_CONNECTION_PROXY |
				(uint32)XHTTP_CONNECTION_RESPONSE
		) == XHTTP_CONNECTION_PERSIST,
		"proxy rejected HTTP/1.0 response persistence"
	);
	testRequire(
		xrtHttpConnectionPersistence(
			XHTTP_VERSION_1_1, &Invalid, 1u, 0
		) == XHTTP_CONNECTION_ERROR,
		"Connection persistence accepted malformed syntax"
	);
	xrtClearError();
	testRequire(
		xrtHttpConnectionPersistence(
			(xhttpversion)12, NULL, 0, 0
		) == XHTTP_CONNECTION_ERROR,
		"Connection persistence accepted an unsupported version"
	);
	xrtClearError();
	testRequire(
		xrtHttpConnectionPersistence(
			XHTTP_VERSION_1_1, NULL, 0, UINT32_C(0x80000000)
		) == XHTTP_CONNECTION_ERROR,
		"Connection persistence accepted unknown flags"
	);
	xrtClearError();
}



/* 验证未对齐字段描述符和大规模重复字段不受固定槽位限制。 */
static void testHttpConnectionMemory(void)
{
	enum { TEST_CONNECTION_FIELDS = 128 };
	static const xhttpfield Field = {
		XRT_STR_INIT("Connection"),
		XRT_STR_INIT("X-Hop")
	};
	static xhttpfield Fields[TEST_CONNECTION_FIELDS];
	uint8 Storage[sizeof(Field) + 1u];
	uint8 CursorStorage[sizeof(xhttpfieldtokencursor) + 1u];
	uint8 OptionStorage[sizeof(xstrview) + 1u];
	xhttpfieldtokencursor* pUnalignedCursor =
		(xhttpfieldtokencursor*)(void*)(CursorStorage + 1u);
	xstrview* pUnalignedOption =
		(xstrview*)(void*)(OptionStorage + 1u);
	xhttpfieldtokencursor Cursor;
	xstrview Option;
	size_t iOptions = 0;
	size_t i;

	memcpy(Storage + 1u, &Field, sizeof(Field));
	testRequire(
		xrtHttpConnectionFind(
			(const xhttpfield*)(const void*)(Storage + 1u),
			1u,
			XRT_STR_LITERAL("x-hop")
		) == XHTTP_NEXT_ITEM,
		"unaligned Connection field descriptor failed"
	);
	xrtHttpConnectionCursorInit(pUnalignedCursor);
	testRequire(
		xrtHttpConnectionNext(
			(const xhttpfield*)(const void*)(Storage + 1u),
			1u,
			pUnalignedCursor,
			pUnalignedOption
		) == XHTTP_NEXT_ITEM,
		"unaligned Connection cursor or output failed"
	);
	memcpy(&Option, pUnalignedOption, sizeof(Option));
	testRequire(
		(Option.Size == 5u) &&
		(memcmp(Option.Data, "X-Hop", Option.Size) == 0),
		"unaligned Connection cursor published the wrong option"
	);
	for ( i = 0; i < TEST_CONNECTION_FIELDS; i++ ) {
		Fields[i].Name = XRT_STR_LITERAL("Connection");
		Fields[i].Value = XRT_STR_LITERAL("X-First, X-Second");
	}
	xrtHttpConnectionCursorInit(&Cursor);
	while ( xrtHttpConnectionNext(
		Fields, TEST_CONNECTION_FIELDS, &Cursor, &Option
	) == XHTTP_NEXT_ITEM ) {
		iOptions++;
	}
	testRequire(
		(iOptions == (TEST_CONNECTION_FIELDS * 2u)) &&
		(xrtGetError() == NULL),
		"many Connection fields were truncated"
	);
}



int main(void)
{
	testHttpConnectionOptions();
	testHttpConnectionBoundaries();
	testHttpConnectionPersistence();
	testHttpConnectionMemory();
	printf("[PASS] http_connection\n");
	return 0;
}
