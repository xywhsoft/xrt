#include "../test.h"

#include <xrt/http_te.h>



/* 比较借用视图与固定文本。 */
static bool testHttpTeText(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 验证特殊 trailers、传输参数和权重边界。 */
static void testHttpTeCoding(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("trailers;q=0.5"),
		XRT_STR_INIT("gzip;"),
		XRT_STR_INIT("gzip; level"),
		XRT_STR_INIT("gzip; q =0.5"),
		XRT_STR_INIT("gzip;q= 0.5"),
		XRT_STR_INIT("gzip;q=\"0.5\""),
		XRT_STR_INIT("gzip;q=1.001"),
		XRT_STR_INIT("gzip;q=0.5;level=1"),
		XRT_STR_INIT("gzip, deflate"),
		XRT_STR_INIT("gzip;level=\"open")
	};
	xhttptecoding Coding;
	size_t i;

	testRequire(xrtHttpTeCodingParse(
		XRT_STR_LITERAL(" trailers "), &Coding
	) && testHttpTeText(
		Coding.Coding, "trailers"
	) && (Coding.Quality == XHTTP_QUALITY_MAX) &&
		(Coding.Flags == XHTTP_TE_CODING_TRAILERS),
		"HTTP TE trailers parse mismatch");
	testRequire(xrtHttpTeCodingParse(
		XRT_STR_LITERAL(
			"gzip; level = \"a,b\"; mode=fast; q=0.500"
		),
		&Coding
	) && testHttpTeText(
		Coding.Coding, "gzip"
	) && testHttpTeText(
		Coding.Parameters,
		"level = \"a,b\"; mode=fast"
	) && (Coding.ParameterCount == 2u) &&
		(Coding.Quality == 500u) &&
		((Coding.Flags &
		  XHTTP_TE_CODING_HAS_PARAMETERS) != 0) &&
		((Coding.Flags &
		  XHTTP_TE_CODING_HAS_WEIGHT) != 0),
		"HTTP TE transfer coding parse mismatch");

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		memset(&Coding, 0xA5, sizeof(Coding));
		testRequire(!xrtHttpTeCodingParse(
			Invalid[i], &Coding
		) && (Coding.Element.Data == NULL) &&
			(Coding.Coding.Data == NULL),
			"HTTP TE parser accepted malformed member");
		xrtClearError();
	}
}



/* 验证列表在发布首项前完整校验后缀。 */
static void testHttpTeList(void)
{
	xstrview Value = XRT_STR_LITERAL(
		", trailers, gzip;level=\"a,b\";q=0.5,,"
	);
	xhttptecursor Cursor;
	xhttptecursor Before;
	xhttptecoding Coding;
	size_t iCount;

	testRequire(xrtHttpTeCount(Value, &iCount) &&
		(iCount == 2u) && xrtHttpTeValid(Value),
		"HTTP TE list count mismatch");
	xrtHttpTeCursorInit(&Cursor);
	testRequire((xrtHttpTeNext(
		Value, &Cursor, &Coding
	) == XHTTP_NEXT_ITEM) &&
		((Coding.Flags & XHTTP_TE_CODING_TRAILERS) != 0),
		"HTTP TE list trailers mismatch");
	testRequire((xrtHttpTeNext(
		Value, &Cursor, &Coding
	) == XHTTP_NEXT_ITEM) && testHttpTeText(
		Coding.Coding, "gzip"
	) && (Coding.Quality == 500u),
		"HTTP TE list coding mismatch");
	testRequire(xrtHttpTeNext(
		Value, &Cursor, &Coding
	) == XHTTP_NEXT_END,
		"HTTP TE list did not end exactly");

	xrtHttpTeCursorInit(&Cursor);
	Before = Cursor;
	memset(&Coding, 0xA5, sizeof(Coding));
	testRequire((xrtHttpTeNext(
		XRT_STR_LITERAL("trailers, gzip;q=2"),
		&Cursor,
		&Coding
	) == XHTTP_NEXT_ERROR) &&
		(memcmp(&Cursor, &Before, sizeof(Cursor)) == 0) &&
		(Coding.Element.Data == NULL),
		"HTTP TE list published before tail validation");
	xrtClearError();
	testRequire(xrtHttpTeCount(
		XRT_STR_LITERAL(",,\t,"), &iCount
	) && (iCount == 0),
		"HTTP TE list rejected empty members");
}



/* 验证重复字段汇总、质量查询和 Trailer 能力。 */
static void testHttpTeFields(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("TE"), XRT_STR_INIT("trailers") },
		{ XRT_STR_INIT("X-Other"), XRT_STR_INIT("ignored") },
		{
			XRT_STR_INIT("te"),
			XRT_STR_INIT("gzip;level=1;q=0.5, br;q=0.8")
		}
	};
	static const xhttpfield Invalid[] = {
		{ XRT_STR_INIT("TE"), XRT_STR_INIT("trailers") },
		{ XRT_STR_INIT("TE"), XRT_STR_INIT("gzip;q=2") }
	};
	xhttptefieldcursor Cursor;
	xhttptefieldcursor Before;
	xhttptecoding Coding;
	xhttpteinfo Info;

	testRequire(xrtHttpTeParse(
		Fields, 3u, &Info
	) && (Info.FieldCount == 2u) &&
		(Info.CodingCount == 3u) &&
		(Info.TransferCodingCount == 2u) &&
		((Info.Flags & XHTTP_TE_PRESENT) != 0) &&
		((Info.Flags & XHTTP_TE_ACCEPTS_TRAILERS) != 0) &&
		((Info.Flags & XHTTP_TE_HAS_TRANSFER_CODINGS) != 0),
		"HTTP TE repeated field summary mismatch");
	testRequire((xrtHttpTeQuality(
		Fields, 3u, XRT_STR_LITERAL("GZIP")
	) == 500u) && (xrtHttpTeQuality(
		Fields, 3u, XRT_STR_LITERAL("br")
	) == 800u) && (xrtHttpTeQuality(
		Fields, 3u, XRT_STR_LITERAL("deflate")
	) == 0) && (xrtHttpTeAcceptsTrailers(
		Fields, 3u
	) == XHTTP_NEXT_ITEM),
		"HTTP TE capability query mismatch");
	testRequire(xrtHttpTeParse(
		NULL, 0, &Info
	) && (Info.Flags == XHTTP_TE_NONE) &&
		(xrtHttpTeAcceptsTrailers(
			NULL, 0
		) == XHTTP_NEXT_END),
		"HTTP TE missing field summary mismatch");

	xrtHttpTeFieldCursorInit(&Cursor);
	Before = Cursor;
	memset(&Coding, 0xA5, sizeof(Coding));
	testRequire((xrtHttpTeFieldNext(
		Invalid, 2u, &Cursor, &Coding
	) == XHTTP_NEXT_ERROR) &&
		(memcmp(&Cursor, &Before, sizeof(Cursor)) == 0) &&
		(Coding.Element.Data == NULL),
		"HTTP TE repeated fields published before validation");
	xrtClearError();
}



/* 验证字段、游标和结果支持未对齐存储。 */
static void testHttpTeUnaligned(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("TE"), XRT_STR_INIT("gzip;q=0.4") }
	};
	uint8 FieldsStorage[sizeof(Fields) + 1u];
	uint8 CursorStorage[sizeof(xhttptefieldcursor) + 1u];
	uint8 CodingStorage[sizeof(xhttptecoding) + 1u];
	xhttptecoding Coding;

	memcpy(FieldsStorage + 1u, Fields, sizeof(Fields));
	xrtHttpTeFieldCursorInit(
		(xhttptefieldcursor*)(void*)(CursorStorage + 1u)
	);
	testRequire(xrtHttpTeFieldNext(
		(const xhttpfield*)(const void*)(FieldsStorage + 1u),
		1u,
		(xhttptefieldcursor*)(void*)(CursorStorage + 1u),
		(xhttptecoding*)(void*)(CodingStorage + 1u)
	) == XHTTP_NEXT_ITEM,
		"HTTP TE rejected unaligned storage");
	memcpy(&Coding, CodingStorage + 1u, sizeof(Coding));
	testRequire(testHttpTeText(
		Coding.Coding, "gzip"
	) && (Coding.Quality == 400u),
		"HTTP TE unaligned output mismatch");
}



/* 验证所有公开入口都会拒绝发生地址回绕的借用输入。 */
static void testHttpTeWrappedRange(void)
{
	xstrview Wrapped = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	xhttptecursor Cursor;
	xhttptecoding Coding;
	size_t iCount = 77u;

	testRequire(!xrtHttpTeCodingParse(
		Wrapped, &Coding
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP TE parser accepted a wrapped input range");
	xrtClearError();
	testRequire(!xrtHttpTeValid(Wrapped) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP TE validator accepted a wrapped input range");
	xrtClearError();
	testRequire(!xrtHttpTeCount(Wrapped, &iCount) &&
		(iCount == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP TE counter accepted a wrapped input range");
	xrtClearError();
	xrtHttpTeCursorInit(&Cursor);
	testRequire((xrtHttpTeNext(
		Wrapped, &Cursor, &Coding
	) == XHTTP_NEXT_ERROR) &&
		(Cursor.Offset == 0) && (Cursor.Validated == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP TE iterator accepted a wrapped input range");
	xrtClearError();
}



/* 执行传输无关 TE 协议测试。 */
int main(void)
{
	testHttpTeCoding();
	testHttpTeList();
	testHttpTeFields();
	testHttpTeUnaligned();
	testHttpTeWrappedRange();
	puts("[PASS] http_te");
	return 0;
}
