#include "../test.h"

#include <xrt/http_via.h>



/* 验证 Via 规范写出、Build 和解析闭环。 */
static void testHttpViaWrite(void)
{
	static const xhttpviavalue Values[] = {
		{
			XRT_STR_INIT(""),
			XRT_STR_INIT("1.0"),
			XRT_STR_INIT("first"),
			XRT_STR_INIT(""),
			XRT_STR_INIT(""),
			0
		},
		{
			XRT_STR_INIT("HTTP"),
			XRT_STR_INIT("1.1"),
			XRT_STR_INIT("edge"),
			XRT_STR_INIT("8443"),
			XRT_STR_INIT("west (blue)\\path"),
			XHTTP_VIA_HAS_PROTOCOL_NAME |
			XHTTP_VIA_HAS_PORT |
			XHTTP_VIA_HAS_COMMENT
		}
	};
	static const char Expected[] =
		"1.0 first, HTTP/1.1 edge:8443 "
		"(west \\(blue\\)\\\\path)";
	char sOutput[128];
	xhttpviacursor Cursor;
	xhttpvia Via;
	char sComment[32];
	size_t iComment;
	size_t iSize;
	str sBuilt;

	testRequire(
		xrtHttpViaWrite(
			Values, 2u, NULL, 0, &iSize
		) && (iSize == (sizeof(Expected) - 1u)),
		"Via writer size query mismatch"
	);
	testRequire(
		xrtHttpViaWrite(
			Values, 2u, sOutput, sizeof(sOutput), &iSize
		) && (iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(sOutput, Expected, iSize) == 0),
		"Via writer output mismatch"
	);
	sBuilt = xrtHttpViaBuild(Values, 2u, &iSize);
	testRequire(
		(sBuilt != NULL) &&
		(iSize == (sizeof(Expected) - 1u)) &&
		(strcmp(sBuilt, Expected) == 0),
		"Via Build mismatch"
	);
	xrtHttpViaCursorInit(&Cursor);
	testRequire(
		(xrtHttpViaNext(
			(xstrview){ sBuilt, iSize }, &Cursor, &Via
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpViaNext(
			(xstrview){ sBuilt, iSize }, &Cursor, &Via
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpViaCommentDecode(
			Via.Comment, sComment, sizeof(sComment), &iComment
		) && (iComment == 16u) &&
		(memcmp(sComment, "west (blue)\\path", 16u) == 0),
		"Via write and parse roundtrip mismatch"
	);
	xrtFree(sBuilt);
}



/* 验证存在位、非法值、短输出和输入别名。 */
static void testHttpViaWriteFailure(void)
{
	xhttpviavalue Via = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("1.1"),
		XRT_STR_INIT("edge"),
		XRT_STR_INIT(""),
		XRT_STR_INIT(""),
		XHTTP_VIA_HAS_COMMENT
	};
	xhttpviavalue EmptyPort = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("1.1"),
		XRT_STR_INIT("edge"),
		XRT_STR_INIT(""),
		XRT_STR_INIT(""),
		XHTTP_VIA_HAS_PORT
	};
	char sOutput[8];
	char sPort[16];
	char sSaved[8];
	size_t iSize;

	testRequire(
		xrtHttpViaElementWrite(
			&Via, NULL, 0, &iSize
		) && (iSize == 11u),
		"Via empty comment was not representable"
	);
	testRequire(
		xrtHttpViaElementWrite(
			&EmptyPort, sPort, sizeof(sPort), &iSize
		) && (iSize == 9u) &&
		(memcmp(sPort, "1.1 edge:", iSize) == 0),
		"Via explicit empty port was not representable"
	);
	Via.ProtocolName = XRT_STR_LITERAL("HTTP");
	testRequire(
		!xrtHttpViaElementWrite(
			&Via, NULL, 0, &iSize
		),
		"Via writer accepted unflagged protocol name"
	);
	xrtClearError();
	Via.ProtocolName = XRT_STR_LITERAL("");
	memset(sOutput, 0xA5, sizeof(sOutput));
	memcpy(sSaved, sOutput, sizeof(sSaved));
	testRequire(
		!xrtHttpViaElementWrite(
			&Via, sOutput, sizeof(sOutput), &iSize
		) && (iSize == 11u) &&
		(memcmp(sOutput, sSaved, sizeof(sOutput)) == 0),
		"Via short output was not failure atomic"
	);
	xrtClearError();
	testRequire(
		!xrtHttpViaElementWrite(
			&Via,
			(void*)Via.ProtocolVersion.Data,
			Via.ProtocolVersion.Size,
			&iSize
		),
		"Via writer accepted input and output overlap"
	);
	xrtClearError();
}



int main(void)
{
	testHttpViaWrite();
	testHttpViaWriteFailure();
	printf("[PASS] http_via_write\n");
	return 0;
}
