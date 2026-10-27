#define XRT_MODULE_JSON_READ
#include <xrt.h>
#include <string.h>

typedef struct scanconsumer { size_t Events, Numbers; } scanconsumer;

static xjsonvisitaction consumeScan(const xjsonscanevent* Event, ptr User)
{
	scanconsumer* Count = (scanconsumer*)User; ++Count->Events;
	if (Event->Type == XJSON_SCAN_NUMBER) {
		++Count->Numbers;
		if (Event->Raw.Size != 6 || memcmp(Event->Raw.Data, "1e9999", 6) != 0) return XJSON_VISIT_FAIL;
	}
	return XJSON_VISIT_NEXT;
}

/* Run as an ordinary public consumer and separately link this same source
 * against the independently produced static/shared JSON-read package. */
int main(void)
{
	xjsonreadconfig Config; xrtJsonReadConfigInit(&Config); scanconsumer Count = {0};
	if (xrtJsonScan(XRT_STR_LITERAL("{\"method\":\"echo\",\"id\":1e9999}"), &Config, consumeScan, &Count) != XJSON_VISIT_DONE ||
		Count.Events != 4 || Count.Numbers != 1) return 1;
	if (xrtJsonScan(XRT_STR_LITERAL("[true,]"), &Config, consumeScan, &Count) != XJSON_VISIT_ERROR ||
		xrtErrorKind(xrtGetError()) != XERR_PROTOCOL || strcmp(xrtErrorDomain(xrtGetError()), "xrt.json") != 0) return 2;
	xrtClearError();
	return 0;
}
