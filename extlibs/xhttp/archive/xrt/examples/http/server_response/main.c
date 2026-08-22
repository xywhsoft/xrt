#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 把一个冻结响应的全部租约提交给示例输出。 */
static bool writeResponse(xhttp1serverresponse* pResponse)
{
	xhttp1serveroutputstatus Status;
	xbytesview Data;

	if ( pResponse == NULL ) {
		return false;
	}
	for ( ;; ) {
		Status = xrtHttp1ServerResponseOutput(
			pResponse, 4096, &Data
		);
		if ( Status != XHTTP1_SERVER_OUTPUT_DATA ) {
			break;
		}
		if ( (fwrite(Data.Data, 1, Data.Size, stdout) !=
			  Data.Size) ||
			!xrtHttp1ServerResponseOutputConsume(
				pResponse, Data.Size
			) ) {
			return false;
		}
	}
	return Status == XHTTP1_SERVER_OUTPUT_DONE;
}



/* 演示依次冻结信息响应和最终响应并接入同一发送队列。 */
int main(void)
{
	xhttpreply* pReply = xrtHttpReplyCreate(103);
	xhttp1serverresponse* pResponse;

	if ( (pReply == NULL) ||
		!xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Link"),
			XRT_STR_LITERAL("</app.css>; rel=preload")
		) ) {
		xrtHttpReplyDestroy(pReply);
		return 1;
	}
	pResponse = xrtHttp1ServerResponseInform(
		XHTTP_VERSION_1_1, pReply
	);
	xrtHttpReplyDestroy(pReply);
	if ( !writeResponse(pResponse) ) {
		xrtHttp1ServerResponseDestroy(pResponse);
		return 1;
	}
	xrtHttp1ServerResponseDestroy(pResponse);

	pReply = xrtHttpReplyCreate(200);
	if ( (pReply == NULL) ||
		!xrtHttpReplySetBytes(
			pReply,
			XRT_BYTES_LITERAL("{\"status\":\"ok\"}"),
			XRT_STR_LITERAL("application/json; charset=utf-8")
		) ) {
		xrtHttpReplyDestroy(pReply);
		return 1;
	}
	pResponse = xrtHttp1ServerResponsePrepare(
		XHTTP_VERSION_1_1,
		XRT_STR_LITERAL("GET"),
		XHTTP_SERVER_REQUEST_KEEP_ALIVE,
		pReply
	);
	xrtHttpReplyDestroy(pReply);
	if ( !writeResponse(pResponse) ) {
		xrtHttp1ServerResponseDestroy(pResponse);
		return 1;
	}
	xrtHttp1ServerResponseDestroy(pResponse);
	return 0;
}
