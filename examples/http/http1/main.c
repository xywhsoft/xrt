#include <stdio.h>

#include <xrt.h>



/*
 * 范例：http/http1 —— HTTP/1.1 请求解析与响应封包（零拷贝核心）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttp1HeadInit      绑定调用方字段数组（零分配解析）
 *   xrtHttp1RequestParse  借用视图解析请求头（三态返回）
 *   xrtHttp1ResponseWrite 状态行 + 字段一次封包（容量原子性）
 * 模块宏：XRT_MODULE_HTTP1
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/http/http1/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   GET /health
 *   HTTP/1.1 200 OK
 *   Content-Type: application/json
 *   Content-Length: 11
 *
 * 这是 xhttp 运行时与自研网关的地基路径：
 *   解析结果全是借用视图（Method/Target/字段数组），
 *   一个线程一个 Head 结构反复复用，零逐请求分配；
 *   ResponseWrite 容量不足时不写半个报文（原子性约定）。
 */


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
