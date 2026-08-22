#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 RFC 9530 摘要解析与解码。 */
int main(void)
{
	xhttpdigestcursor Cursor;
	xhttpdigest Digest;
	uint8 iValue;
	size_t iSize;

	xrtHttpDigestCursorInit(&Cursor);
	return (xrtHttpDigestNext(
		XRT_STR_LITERAL("sha-256=:AQ==:"),
		&Cursor, &Digest
	) == XHTTP_NEXT_ITEM) && xrtHttpDigestRead(
		&Digest, &iValue, 1u, &iSize
	) && (iSize == 1u) && (iValue == 1u) ? 0 : 1;
}
