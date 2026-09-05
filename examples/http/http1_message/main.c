#include <stdio.h>

#include <xrt.h>



/*
 * 范例：http/http1_message —— 一次扫描整条消息（头+正文+trailer）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttp1MessageInit               绑定字段与 trailer 数组
 *   xrtHttp1ResponseMessageParse      头和 chunked 正文一遍解析完
 *   xrtHttp1MessageBodyCopy           把分段正文拼为连续缓冲
 *   Message.Wire.Size / TrailerCount  消费量与 trailer 计数
 * 模块宏：XRT_MODULE_HTTP1
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/http/http1_message/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   status=200 body=hello world trailers=1 suffix=NEXT
 *
 * Message vs Body 两层 API：Body（http1_body 范例）是流式原语，
 *   适合代理边收边转；Message 一次到位并给出"消费到哪"
 *   （Wire.Size）——管道里跟了别的数据（suffix=NEXT）时，
 *   剩余偏移拿来继续解析下一帧，粘包处理的关键。
 */


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
