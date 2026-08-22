#include <stdio.h>

#include <xrt.h>



/* 展示零拷贝解析与直接响应封包的基础路径。 */
int main(void)
{
	static const char Request[] =
		"GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n";
	static const xhttpfield ResponseFields[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") },
		{ XRT_STR_INIT("Content-Length"), XRT_STR_INIT("11") }
	};
	xhttpfield RequestFields[8];
	xhttp1head Head;
	xbytesview Input;
	char Response[256];
	size_t iSize;

	Input.Data = (cbytes)Request;
	Input.Size = sizeof(Request) - 1u;
	xrtHttp1HeadInit(&Head, RequestFields, 8);
	if ( xrtHttp1RequestParse(
		Input, &Head, NULL, NULL
	) != XHTTP1_READY ) {
		return 1;
	}
	printf("%.*s %.*s\n",
		(int)Head.Method.Size, Head.Method.Data,
		(int)Head.Target.Size, Head.Target.Data);
	if ( !xrtHttp1ResponseWrite(
		XHTTP_VERSION_1_1, 200, XRT_STR_LITERAL("OK"),
		ResponseFields, 2, Response, sizeof(Response), &iSize
	) ) {
		return 2;
	}
	fwrite(Response, 1, iSize, stdout);
	fwrite("{\"ok\":true}", 1, 11, stdout);
	return 0;
}
