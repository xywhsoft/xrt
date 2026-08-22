#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留无固定接收缓冲的 HTTP/1 块链适配。 */
int main(void)
{
	static const char HeadText[] =
		"HTTP/1.1 101 Switching Protocols\r\n\r\n";
	xnetbuf Buffer;
	xhttpfield Fields[2];
	xhttp1head Head;

	if ( !xrtNetBufInit(&Buffer, NULL) ||
		!xrtNetBufAppendBorrow(
			&Buffer,
			HeadText,
			sizeof(HeadText) - 1u
		) ) {
		return 1;
	}
	xrtHttp1HeadInit(
		&Head,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	if ( xrtHttp1ResponseParseBuffer(
		&Buffer,
		&Head,
		NULL,
		NULL
	) != XHTTP1_READY ) {
		xrtNetBufClear(&Buffer);
		return 2;
	}
	xrtNetBufClear(&Buffer);
	return 0;
}
