#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留无分配 HTTP token-list 与字段基础能力。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Connection"), XRT_STR_INIT("keep-alive, Upgrade") }
	};
	static const xstrview Tokens[] = {
		XRT_STR_INIT("keep-alive"),
		XRT_STR_INIT("Upgrade")
	};
	char Output[64];
	xhttpfieldtokencursor Cursor;
	xstrview Token;
	uint64 iLength;
	size_t iOffset = 0;
	size_t iSize;

	if ( (xrtHttpTokenNext(
		Fields[0].Value, &iOffset, &Token
	) != XHTTP_NEXT_ITEM) ||
		!xrtHttpTokenEqual(Token, XRT_STR_LITERAL("keep-alive")) ) {
		return 1;
	}
	if ( !xrtHttpTokenListHas(
		Fields[0].Value, XRT_STR_LITERAL("upgrade")
	) ) {
		return 2;
	}
	if ( !xrtHttpTokenListWrite(
		Tokens, 2u, Output, sizeof(Output), &iSize
	) || (iSize != 19u) ) {
		return 7;
	}
	xrtHttpFieldTokenCursorInit(&Cursor);
	if ( (xrtHttpFieldTokenNext(
		Fields,
		1u,
		XRT_STR_LITERAL("Connection"),
		&Cursor,
		&Token
	) != XHTTP_NEXT_ITEM) || !xrtHttpTokenEqual(
		Token, XRT_STR_LITERAL("keep-alive")
	) ) {
		return 3;
	}
	if ( xrtHttpFieldGet(
		Fields, 1, XRT_STR_LITERAL("connection")
	) != &Fields[0] ) {
		return 4;
	}
	if ( !xrtHttpContentLengthParse(
		XRT_STR_LITERAL("9, 9"), &iLength
	) || (iLength != 9) ) {
		return 5;
	}
	if ( !xrtHttpFieldBlockWrite(
		Fields, 1, Output, sizeof(Output), &iSize
	) || (iSize != 35u) ) {
		return 6;
	}
	return 0;
}
