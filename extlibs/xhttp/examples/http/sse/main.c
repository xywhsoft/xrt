#include <xrt/http_sse.h>

#include <stdio.h>
#include <string.h>



/* 构建一条可直接交给 HTTP 流式 Body 的 SSE 事件。 */
int main(void)
{
	xhttpsseevent Event;
	str sText;
	size_t iSize;

	memset(&Event, 0, sizeof(Event));
	Event.Type = XRT_STR_LITERAL("progress");
	Event.Data = XRT_STR_LITERAL("{\"percent\": 75}");
	Event.Id = XRT_STR_LITERAL("job-42:3");
	Event.Retry = 2000;
	Event.Flags = XHTTP_SSE_EVENT_TYPE |
		XHTTP_SSE_EVENT_DATA |
		XHTTP_SSE_EVENT_ID |
		XHTTP_SSE_EVENT_RETRY;
	sText = xrtHttpSseEventBuild(&Event, &iSize);
	if ( sText == NULL ) {
		return 1;
	}
	(void)fwrite(sText, 1, iSize, stdout);
	xrtFree(sText);
	return 0;
}
