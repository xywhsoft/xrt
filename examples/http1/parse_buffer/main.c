/*
 * 范例：http1/parse_buffer —— 从网络缓冲链解析 HTTP/1 请求与响应
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttp1RequestParseBuffer    从 xnetbuf 链解析请求头
 *   xrtHttp1ResponseParseBuffer   从 xnetbuf 链解析响应头
 *   （输入不消费；头跨块时才按需分配连续前缀）
 * 模块宏：XRT_MODULE_HTTP1_NET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/http1/parse_buffer/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   request: GET /x
 *   response: 200
 *
 * ParseBuffer 面向真实收包路径：引擎收到的是缓冲链
 *   （可能头被切在两块里）——与 Parse（连续内存）的
 *   分工就是"网络层 vs 测试向量"。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	static const char sRequest[] =
		"GET /x HTTP/1.1\r\nHost: a.test\r\n\r\n";
	static const char sResponse[] =
		"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
	xnetbufpool* pPool = xrtNetBufPoolCreate(NULL);
	xnetbuf ReqBuf;
	xnetbuf RspBuf;
	xhttpfield Fields[8];
	xhttp1head Head;
	bool bOk = false;

	if ( pPool == NULL ) {
		return 1;
	}
	/* 请求链：分两块追加（头跨块场景的最小化模拟）。 */
	if ( xrtNetBufInit(&ReqBuf, pPool) &&
		xrtNetBufAppend(&ReqBuf, sRequest, 12u) &&
		xrtNetBufAppend(&ReqBuf, sRequest + 12u, sizeof(sRequest) - 13u) ) {
		xrtHttp1HeadInit(&Head, Fields, 8);
		if ( xrtHttp1RequestParseBuffer(&ReqBuf, &Head, NULL, NULL) ==
			XHTTP1_READY ) {
			printf("request: %.*s %.*s\n",
				(int)Head.Method.Size, Head.Method.Data,
				(int)Head.Target.Size, Head.Target.Data);
			bOk = true;
		}
		xrtNetBufClear(&ReqBuf);
	}
	/* 响应链：单块。 */
	if ( bOk && xrtNetBufInit(&RspBuf, pPool) &&
		xrtNetBufAppend(&RspBuf, sResponse, sizeof(sResponse) - 1u) ) {
		xrtHttp1HeadInit(&Head, Fields, 8);
		if ( xrtHttp1ResponseParseBuffer(&RspBuf, &Head, NULL, NULL) ==
			XHTTP1_READY ) {
			printf("response: %u\n", (unsigned)Head.Status);
			bOk = Head.Status == 200u;
		}
		xrtNetBufClear(&RspBuf);
	}
	xrtNetBufPoolDestroy(pPool);
	return bOk ? 0 : 2;
}
