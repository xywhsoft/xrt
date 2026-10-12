#include "../test.h"



/* 在一个逻辑故障点创建完整 SSE Reply，并验证输出发布是原子的。 */
static bool testHttpSseServerCreateOomAttempt(size_t iFail)
{
	xhttpbodystream* pStream =
		(xhttpbodystream*)(uintptr_t)1;
	xhttpreply* pReply;
	bool bTriggered;
	bool bCreated;

	testRequire(
		xrtMemDebugFailAfter((uint64)iFail),
		"SSE server create OOM setup failed"
	);
	pReply = xrtHttpSseReplyCreate(NULL, &pStream);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( pReply == NULL ) {
		testRequire(
			bTriggered && (pStream == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
			"SSE server create OOM published partial output"
		);
	} else {
		testRequire(
			!bTriggered && (pStream != NULL) &&
			(xrtHttpReplyBody(pReply) != NULL),
			"SSE server create ignored an allocation fault"
		);
	}
	bCreated = pReply != NULL;
	xrtHttpBodyStreamDestroy(
		bCreated ? pStream : NULL
	);
	xrtHttpReplyDestroy(pReply);
	xrtClearError();
	testMemoryDebugDrain(
		"SSE server create OOM leaked storage"
	);
	return bCreated;
}



/* 扫描 Body、Reply、Header 与正文绑定涉及的全部创建分配点。 */
static size_t testHttpSseServerCreateOom(void)
{
	size_t iFail;

	for ( iFail = 0; iFail < 32u; iFail++ ) {
		if ( testHttpSseServerCreateOomAttempt(iFail) ) {
			return iFail;
		}
	}
	testRequire(false,
		"SSE server create OOM scan did not converge");
	return 0;
}



/* 验证事件节点分配失败不消耗 Stream 预算并且解除故障后可恢复。 */
static void testHttpSseServerSendOom(void)
{
	xhttpbodystream* pStream = NULL;
	xhttpreply* pReply = xrtHttpSseReplyCreate(
		NULL, &pStream
	);
	xhttpbodystreaminfo Info;
	xhttpsseevent Event;

	testRequire((pReply != NULL) && (pStream != NULL),
		"SSE server send OOM setup failed");
	memset(&Event, 0, sizeof(Event));
	Event.Id = XRT_STR_LITERAL("oom-1");
	Event.Data = XRT_STR_LITERAL("payload");
	Event.Flags = XHTTP_SSE_EVENT_ID |
		XHTTP_SSE_EVENT_DATA;
	testRequire(xrtMemDebugFailAfter(0),
		"SSE server event OOM setup failed");
	testRequire(
		(xrtHttpSseSendEvent(
			pStream, &Event
		) == XHTTP_BODY_STREAM_ERROR) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"SSE server event did not preserve OOM"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	testRequire(
		xrtHttpBodyStreamInfo(pStream, &Info) &&
		(Info.PendingBytes == 0) &&
		(Info.PendingChunks == 0) &&
		(Info.WrittenBytes == 0),
		"SSE server event OOM did not roll back budget"
	);

	testRequire(xrtMemDebugFailAfter(0),
		"SSE server comment OOM setup failed");
	testRequire(
		(xrtHttpSseSendComment(
			pStream, XRT_STR_LITERAL("heartbeat")
		) == XHTTP_BODY_STREAM_ERROR) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"SSE server comment did not preserve OOM"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	testRequire(
		xrtHttpBodyStreamInfo(pStream, &Info) &&
		(Info.PendingBytes == 0) &&
		(Info.PendingChunks == 0) &&
		(Info.WrittenBytes == 0),
		"SSE server comment OOM did not roll back budget"
	);

	testRequire(
		(xrtHttpSseSendEvent(
			pStream, &Event
		) == XHTTP_BODY_STREAM_OK) &&
		(xrtHttpSseSendComment(
			pStream, XRT_STR_LITERAL("heartbeat")
		) == XHTTP_BODY_STREAM_OK),
		"SSE server send did not recover after OOM"
	);
	xrtHttpBodyStreamDestroy(pStream);
	xrtHttpReplyDestroy(pReply);
	xrtClearError();
	testMemoryDebugDrain(
		"SSE server send OOM leaked storage"
	);
}



/* 运行 SSE 服务端组合与事件编码的逻辑分配故障回归。 */
int main(void)
{
	size_t iCreateFaults = testHttpSseServerCreateOom();

	testRequire(iCreateFaults != 0,
		"SSE server create path had no allocations");
	testHttpSseServerSendOom();
	printf(
		"[PASS] HTTP SSE server OOM create_faults=%u\n",
		(unsigned)iCreateFaults
	);
	return 0;
}
