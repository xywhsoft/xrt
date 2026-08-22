#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留完整消息扫描和 chunked 正文复制。 */
int main(void)
{
	static const uint8 Wire[] =
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
		"4\r\ntest\r\n0\r\n\r\n";
	xhttpfield Fields[1];
	xhttp1message Message;
	uint8 Output[8];
	size_t iSize;

	xrtHttp1MessageInit(&Message, Fields, 1, NULL, 0);
	if ( xrtHttp1ResponseMessageParse(
		(xbytesview){ Wire, sizeof(Wire) - 1u }, false,
		XRT_STR_LITERAL("GET"),
		&Message, NULL, NULL, NULL
	) != XHTTP1_READY ) {
		return 1;
	}
	if ( !xrtHttp1MessageBodyCopy(
		&Message, Output, sizeof(Output), &iSize
	) || (iSize != 4) || (memcmp(Output, "test", 4) != 0) ) {
		return 2;
	}
	return 0;
}
