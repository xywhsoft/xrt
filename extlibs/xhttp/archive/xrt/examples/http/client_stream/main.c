#include <stdio.h>
#include <string.h>
#include <xrt.h>

#include "../common/async_body.h"



/* 示例状态只保存需要跨 Worker 回收的拥有型引用。 */
typedef struct example_http_call {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Client;
	xnetstream* Server;
	xhttp1call* Call;
	xhttpresponse* Response;
	xatomic32 Done;
	example_http_async_body Body;
	bool Responded;
} example_http_call;



/* 输出当前线程的结构化错误。 */
static void exampleHttpCallError(cstr sOperation)
{
	const xerror* pError = xrtGetError();

	fprintf(
		stderr,
		"%s: %s\n",
		sOperation,
		pError != NULL ?
			xrtErrorMessage(pError) :
			"unknown error"
	);
}



/* 回环服务端收到完整请求后返回一个固定响应。 */
static void exampleHttpCallServerRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	static const char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain; charset=utf-8\r\n"
		"Content-Length: 2\r\n"
		"\r\n"
		"OK";
	example_http_call* pExample =
		(example_http_call*)pData;
	char Request[1024];
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t i;

	if ( pExample->Responded ||
		(iSize < 4u) ||
		(iSize >= sizeof(Request)) ||
		(xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) != iSize) ) {
		return;
	}
	for ( i = 3; i < iSize; ++i ) {
		if ( (Request[i - 3] == '\r') &&
			(Request[i - 2] == '\n') &&
			(Request[i - 1] == '\r') &&
			(Request[i] == '\n') ) {
			pExample->Responded = true;
			(void)xrtNetBufConsume(pBuffer, iSize);
			if ( xrtNetStreamSend(
				pStream,
				Response,
				sizeof(Response) - 1u
			) != XNET_RESULT_OK ) {
				(void)xrtNetStreamAbort(pStream);
			}
			return;
		}
	}
}



/* 客户端关闭写方向后，服务端完成连接关闭。 */
static void exampleHttpCallServerEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* Listener 接管服务端连接并安装最小 HTTP 回调。 */
static bool exampleHttpCallAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	example_http_call* pExample =
		(example_http_call*)pData;
	xnetstreamevents Events;

	(void)pListener;
	memset(&Events, 0, sizeof(Events));
	Events.Read = exampleHttpCallServerRead;
	Events.End = exampleHttpCallServerEnd;
	if ( !xrtNetStreamSetEvents(
		pStream,
		&Events,
		pExample
	) ) {
		return false;
	}
	pExample->Server = pStream;
	return true;
}



/* 调用完成后读取响应，并关闭归还的可复用 Stream。 */
static void exampleHttpCallDone(
	xhttp1call* pCall,
	const xhttp1callresult* pResult,
	ptr pData
)
{
	example_http_call* pExample =
		(example_http_call*)pData;
	xbytesview Body;

	(void)pCall;
	if ( (pResult == NULL) ||
		(pResult->Result != XNET_RESULT_OK) ||
		(pResult->Response == NULL) ) {
		if ( (pResult != NULL) &&
			(pResult->Error != NULL) ) {
			fprintf(
				stderr,
				"HTTP call: %s\n",
				xrtErrorMessage(pResult->Error)
			);
		}
		xrtAtomic32Store(
			&pExample->Done,
			1,
			XMEMORY_RELEASE
		);
		return;
	}
	pExample->Response = pResult->Response;
	pExample->Client = pResult->Tcp;
	Body = xrtHttpResponseBody(pResult->Response);
	printf(
		"status=%d body=%.*s\n",
		xrtHttpResponseStatus(pResult->Response),
		(int)Body.Size,
		(const char*)Body.Data
	);
	if ( pExample->Client != NULL ) {
		(void)xrtNetStreamClose(pExample->Client);
	}
	xrtAtomic32Store(
		&pExample->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* TCP 建立后创建请求计划并转移给 HTTP/1 调用驱动器。 */
static void exampleHttpCallOpen(
	xnetstream* pStream,
	ptr pData
)
{
	example_http_call* pExample =
		(example_http_call*)pData;
	xhttprequest* pRequest;
	xhttpbody* pBody;
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;
	xhttp1callevents CallEvents;
	bool bBodySet = false;

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://127.0.0.1/example")
	);
	pBody = pRequest != NULL ?
		exampleHttpAsyncBodyCreate(
			&pExample->Body,
			XRT_BYTES_LITERAL("hello")
		) :
		NULL;
	if ( (pRequest != NULL) && (pBody != NULL) ) {
		bBodySet = xrtHttpRequestSetBody(pRequest, pBody);
	}
	xrtHttpBodyDestroy(pBody);
	pPlan = bBodySet ?
		xrtHttp1RequestPrepare(pRequest, NULL) :
		NULL;
	xrtHttpRequestDestroy(pRequest);
	pExchange = pPlan != NULL ?
		xrtHttp1ExchangeCreate(
			pPlan,
			NULL,
			NULL
		) :
		NULL;
	if ( (pPlan != NULL) && (pExchange == NULL) ) {
		xrtHttp1RequestPlanDestroy(pPlan);
	}
	if ( pExchange == NULL ) {
		exampleHttpCallError("prepare HTTP request");
		(void)xrtNetStreamAbort(pStream);
		xrtAtomic32Store(
			&pExample->Done,
			1,
			XMEMORY_RELEASE
		);
		return;
	}
	xrtHttp1CallEventsInit(&CallEvents);
	CallEvents.Done = exampleHttpCallDone;
	CallEvents.Data = pExample;
	pExample->Call = xrtHttp1CallTcp(
		pStream,
		pExchange,
		NULL,
		&CallEvents
	);
	if ( pExample->Call == NULL ) {
		exampleHttpCallError("start HTTP call");
		xrtHttp1ExchangeDestroy(pExchange);
		(void)xrtNetStreamAbort(pStream);
		xrtAtomic32Store(
			&pExample->Done,
			1,
			XMEMORY_RELEASE
		);
		return;
	}
	pExample->Client = NULL;
}



/* 运行一次本地 HTTP/1 请求并完整回收调用、传输和 Engine。 */
int main(void)
{
	example_http_call Example;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents ClientEvents;
	xnetaddr Address;
	xdeadline iDeadline;
	int iResult = 1;

	memset(&Example, 0, sizeof(Example));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	xrtAtomic32Init(&Example.Done, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1u;
	Example.Engine = xrtNetEngineCreate(&EngineConfig);
	if ( (Example.Engine == NULL) ||
		!xrtNetEngineStart(Example.Engine) ) {
		exampleHttpCallError("start network Engine");
		goto Cleanup;
	}

	xrtNetListenConfigInit(&ListenConfig);
	(void)xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	);
	ListenerEvents.Accept = exampleHttpCallAccept;
	Example.Listener = xrtNetListen(
		Example.Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Example
	);
	if ( (Example.Listener == NULL) ||
		!xrtNetListenerLocal(
			Example.Listener,
			&Address
		) ) {
		exampleHttpCallError("start HTTP listener");
		goto Cleanup;
	}

	ClientEvents.Open = exampleHttpCallOpen;
	Example.Client = xrtNetStreamConnect(
		Example.Engine,
		&Address,
		0,
		NULL,
		&ClientEvents,
		&Example
	);
	if ( Example.Client == NULL ) {
		exampleHttpCallError("connect HTTP client");
		goto Cleanup;
	}
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( xrtAtomic32Load(
		&Example.Done,
		XMEMORY_ACQUIRE
	) == 0 ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			fprintf(stderr, "HTTP call timed out\n");
			goto Cleanup;
		}
		xrtThreadYield();
	}
	iResult = Example.Response != NULL ? 0 : 1;

Cleanup:
	if ( (Example.Client != NULL) &&
		(xrtNetStreamState(Example.Client) !=
		 XNET_STREAM_CLOSED) ) {
		(void)xrtNetStreamAbort(Example.Client);
	}
	if ( (Example.Server != NULL) &&
		(xrtNetStreamState(Example.Server) !=
		 XNET_STREAM_CLOSED) ) {
		(void)xrtNetStreamAbort(Example.Server);
	}
	if ( (Example.Listener != NULL) &&
		(xrtNetListenerState(Example.Listener) !=
		 XNET_LISTENER_CLOSED) ) {
		(void)xrtNetListenerClose(Example.Listener);
	}
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( ((Example.Client != NULL) &&
		  (xrtNetStreamState(Example.Client) !=
		   XNET_STREAM_CLOSED)) ||
		 ((Example.Server != NULL) &&
		  (xrtNetStreamState(Example.Server) !=
		   XNET_STREAM_CLOSED)) ||
		 ((Example.Listener != NULL) &&
		  (xrtNetListenerState(Example.Listener) !=
		   XNET_LISTENER_CLOSED)) ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			iResult = 1;
			break;
		}
		xrtThreadYield();
	}
	xrtHttpResponseDestroy(Example.Response);
	xrtHttp1CallDestroy(Example.Call);
	xrtNetStreamDestroy(Example.Client);
	xrtNetStreamDestroy(Example.Server);
	xrtNetListenerDestroy(Example.Listener);
	if ( (Example.Engine != NULL) &&
		!xrtNetEngineDestroy(Example.Engine) ) {
		iResult = 1;
	}
	return iResult;
}
