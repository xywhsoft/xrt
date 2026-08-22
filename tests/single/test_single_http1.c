#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 HTTP/1 解析、字段视图和原始封包能力。 */
int main(void)
{
	static const char Message[] =
		"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
	xhttpfield Fields[2];
	xhttp1head Head;
	xbytesview Input;
	char Output[64];
	size_t iSize;

	Input.Data = (cbytes)Message;
	Input.Size = sizeof(Message) - 1u;
	xrtHttp1HeadInit(&Head, Fields, 2);
	if ( (xrtHttp1ResponseParse(
		Input, &Head, NULL, NULL
	) != XHTTP1_READY) || (Head.Status != 200) ||
		(Head.ContentLength != 2) || (Head.FieldCount != 1) ) {
		return 1;
	}
	if ( !xrtHttp1ResponseWrite(
		XHTTP_VERSION_1_1, 200, XRT_STR_LITERAL("OK"),
		Fields, 1, Output, sizeof(Output), &iSize
	) || (iSize != Head.Bytes) ||
		(memcmp(Output, Message, iSize) != 0) ) {
		return 2;
	}
	return 0;
}
