#define XHTTP_IMPLEMENTATION
#define XHTTP_MODULE_HTTP_SSE_PARSER
#include "../../single/xhttp.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件保留 SSE 增量 Parser 与持久事件 ID。 */
int main(void)
{
	static const uint8 Input[] = "id: 3\ndata: ready\n\n";
	xhttpsseparser Parser;
	xhttpsseitem Item;
	xhttpsseerrorinfo Error;
	size_t iConsumed;
	bool bPass;

	bPass = xrtHttpSseParserInit(&Parser, NULL) &&
		(xrtHttpSseParserRead(
			&Parser,
			(xbytesview){ Input, sizeof(Input) - 1u },
			false,
			&iConsumed,
			&Item,
			&Error
		 ) == XHTTP_SSE_PARSE_ITEM) &&
		(Item.Kind == XHTTP_SSE_ITEM_EVENT) &&
		(Item.Message.Data.Size == 5u) &&
		(memcmp(Item.Message.Data.Data, "ready", 5u) == 0) &&
		(Item.Message.LastEventId.Size == 1u) &&
		(Item.Message.LastEventId.Data[0] == '3');
	xrtHttpSseParserUnit(&Parser);
	printf(
		"%s single-http-sse-parser\n",
		bPass ? "[PASS]" : "[FAIL]"
	);
	return bPass ? 0 : 1;
}
