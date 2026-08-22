#define XHTTP_IMPLEMENTATION
#define XHTTP_MODULE_HTTP_SSE
#include "../../single/xhttp.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件保留 SSE 的多行事件封包。 */
int main(void)
{
	xhttpsseevent Event;
	char Output[64];
	size_t iSize;
	bool bPass;

	memset(&Event, 0, sizeof(Event));
	Event.Data = XRT_STR_LITERAL("one\ntwo");
	Event.Id = XRT_STR_LITERAL("9");
	Event.Flags = XHTTP_SSE_EVENT_DATA | XHTTP_SSE_EVENT_ID;
	bPass = xrtHttpSseEventWrite(
		&Event, Output, sizeof(Output), &iSize
	) && (iSize == 27u) &&
		(memcmp(
			Output,
			"id: 9\ndata: one\ndata: two\n\n",
			27u
		) == 0);
	printf("%s single-http-sse\n", bPass ? "[PASS]" : "[FAIL]");
	return bPass ? 0 : 1;
}
