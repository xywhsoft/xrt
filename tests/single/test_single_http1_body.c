#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 HTTP/1 Body Plan、分块解码和分块写出。 */
int main(void)
{
	static const uint8 Wire[] = "4\r\ntest\r\n0\r\n\r\n";
	xhttp1bodylimits Limits;
	xhttp1bodyplan Plan = { XHTTP1_BODY_CHUNKED, 0 };
	xhttp1body Body;
	xbytesview Data;
	uint8 Output[32];
	size_t iConsumed;
	size_t iSize;

	xrtHttp1BodyLimitsInit(&Limits);
	if ( !xrtHttp1BodyInit(&Body, &Plan, NULL, 0, &Limits) ) {
		return 1;
	}
	if ( (xrtHttp1BodyRead(
		&Body, (xbytesview){ Wire, sizeof(Wire) - 1u }, false,
		&iConsumed, &Data, NULL
	) != XHTTP1_BODY_DATA) || (Data.Size != 4) ||
		(memcmp(Data.Data, "test", 4) != 0) ) {
		return 2;
	}
	if ( xrtHttp1BodyRead(
		&Body,
		(xbytesview){ Wire + iConsumed, sizeof(Wire) - 1u - iConsumed },
		false, &iConsumed, &Data, NULL
	) != XHTTP1_BODY_DONE ) {
		return 3;
	}
	if ( !xrtHttp1ChunkWrite(
		(xbytesview){ (cbytes)"test", 4 },
		Output, sizeof(Output), &iSize
	) || (iSize != 9) ||
		(memcmp(Output, "4\r\ntest\r\n", 9) != 0) ) {
		return 4;
	}
	return 0;
}
