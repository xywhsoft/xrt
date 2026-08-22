#include <stdio.h>

#include <xrt.h>



/* 一次扫描完整响应，并按需把 chunked 正文复制为连续数据。 */
int main(void)
{
	static const uint8 Wire[] =
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
		"5\r\nhello\r\n6\r\n world\r\n0\r\nDigest: ok\r\n\r\nNEXT";
	xhttpfield Fields[8];
	xhttpfield Trailers[4];
	xhttp1message Message;
	uint8 Body[32];
	size_t iSize;

	xrtHttp1MessageInit(&Message, Fields, 8, Trailers, 4);
	if ( xrtHttp1ResponseMessageParse(
		(xbytesview){ Wire, sizeof(Wire) - 1u }, false,
		XRT_STR_LITERAL("GET"),
		&Message, NULL, NULL, NULL
	) != XHTTP1_READY ) {
		return 1;
	}
	if ( !xrtHttp1MessageBodyCopy(
		&Message, Body, sizeof(Body), &iSize
	) ) {
		return 2;
	}
	printf(
		"status=%u body=%.*s trailers=%zu suffix=%.*s\n",
		(unsigned)Message.Head.Status,
		(int)iSize, (cstr)Body,
		Message.TrailerCount,
		(int)(sizeof(Wire) - 1u - Message.Wire.Size),
		(cstr)(Wire + Message.Wire.Size)
	);
	return 0;
}
