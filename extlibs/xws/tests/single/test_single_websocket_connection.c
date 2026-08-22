#define XWS_IMPLEMENTATION
#include "../../single/xws.h"



typedef struct testsinglews testsinglews;



typedef struct testsinglewsendpoint {
	testsinglews* Test;
	xwsrole Role;
	size_t Size;
	char Data[16];
} testsinglewsendpoint;



struct testsinglews {
	xnetengine* Engine;
	xnetlistener* Listener;
	xatomicptr Client;
	xatomicptr Server;
	xatomic32 Received;
	xatomic32 Closed;
	xatomic32 Failed;
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
		xatomic32 Released;
	#endif
	xwsconnevents Events;
	testsinglewsendpoint ClientEndpoint;
	testsinglewsendpoint ServerEndpoint;
};



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
/* 单头引用完成后记录并释放来源负载。 */
static void testSingleWsRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	testsinglews* pTest = (testsinglews*)pContext;

	(void)iSize;
	(void)xrtAtomic32FetchAdd(
		&pTest->Released,
		1,
		XMEMORY_RELEASE
	);
	xrtFree((ptr)pData);
}
#endif



/* 记录异步回调中的第一处测试失败。 */
static void testSingleWsFail(testsinglews* pTest)
{
	xrtAtomic32Store(
		&pTest->Failed,
		1,
		XMEMORY_RELEASE
	);
}



/* 开始接收一条流式消息。 */
static void testSingleWsBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	testsinglewsendpoint* pEndpoint =
		(testsinglewsendpoint*)pData;

	(void)pConnection;
	pEndpoint->Size = 0;
	if ( (pInfo == NULL) ||
		(pInfo->Opcode != XWS_OPCODE_TEXT) ) {
		testSingleWsFail(pEndpoint->Test);
	}
}



/* 复制当前借用分块。 */
static void testSingleWsData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	testsinglewsendpoint* pEndpoint =
		(testsinglewsendpoint*)pData;

	(void)pConnection;
	if ( Data.Size > (sizeof(pEndpoint->Data) -
		pEndpoint->Size) ) {
		testSingleWsFail(pEndpoint->Test);
		return;
	}
	memcpy(
		pEndpoint->Data + pEndpoint->Size,
		Data.Data,
		Data.Size
	);
	pEndpoint->Size += Data.Size;
}



/* 服务端回显消息，客户端记录收到完整响应。 */
static void testSingleWsEnd(
	xwsconn* pConnection,
	ptr pData
)
{
	testsinglewsendpoint* pEndpoint =
		(testsinglewsendpoint*)pData;
	xstrview Text = {
		pEndpoint->Data,
		pEndpoint->Size
	};

	if ( pEndpoint->Role == XWS_ROLE_SERVER ) {
		if ( xrtWsConnText(
			pConnection,
			Text
		) != XNET_RESULT_OK ) {
			testSingleWsFail(pEndpoint->Test);
		}
		return;
	}
	if ( (Text.Size != 6) ||
		 (memcmp(Text.Data, "single", 6) != 0) ) {
		testSingleWsFail(pEndpoint->Test);
		return;
	}
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Test->Received,
		1,
		XMEMORY_RELEASE
	);
}



/* 任何会话错误都使单头回归失败。 */
static void testSingleWsError(
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	testsinglewsendpoint* pEndpoint =
		(testsinglewsendpoint*)pData;

	(void)pConnection;
	(void)pError;
	testSingleWsFail(pEndpoint->Test);
}



/* 记录客户端和服务端各自唯一的关闭终态。 */
static void testSingleWsClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	testsinglewsendpoint* pEndpoint =
		(testsinglewsendpoint*)pData;

	(void)pConnection;
	if ( ((pClose->Flags & XWS_CONN_CLOSE_CLEAN) == 0) ||
		 (pClose->LocalCode != XWS_CLOSE_NORMAL) ||
		 (pClose->RemoteCode != XWS_CLOSE_NORMAL) ) {
		testSingleWsFail(pEndpoint->Test);
	}
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Test->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 按端点角色接管已经开放的 TCP Stream。 */
static xwsconn* testSingleWsAttach(
	xnetstream* pStream,
	testsinglewsendpoint* pEndpoint
)
{
	xwsconnconfig Config;

	xrtWsConnConfigInit(&Config);
	Config.Role = pEndpoint->Role;
	return xrtWsConnAttach(
		pStream,
		&Config,
		&pEndpoint->Test->Events,
		pEndpoint
	);
}



/* 接管 Listener 交付的服务端 Stream。 */
static bool testSingleWsAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testsinglews* pTest = (testsinglews*)pData;
	xwsconn* pConnection;

	(void)pListener;
	pConnection = testSingleWsAttach(
		pStream,
		&pTest->ServerEndpoint
	);
	if ( pConnection == NULL ) {
		testSingleWsFail(pTest);
		return false;
	}
	xrtAtomicPtrStore(
		&pTest->Server,
		pConnection,
		XMEMORY_RELEASE
	);
	return true;
}



/* TCP 建立后接管客户端 Stream。 */
static void testSingleWsOpen(
	xnetstream* pStream,
	ptr pData
)
{
	testsinglews* pTest = (testsinglews*)pData;
	xwsconn* pConnection = testSingleWsAttach(
		pStream,
		&pTest->ClientEndpoint
	);

	if ( pConnection == NULL ) {
		testSingleWsFail(pTest);
		return;
	}
	xrtAtomicPtrStore(
		&pTest->Client,
		pConnection,
		XMEMORY_RELEASE
	);
}



/* 在客户端 Worker 上发送测试消息。 */
static void testSingleWsSend(
	xnetworker* pWorker,
	ptr pData
)
{
	testsinglews* pTest = (testsinglews*)pData;
	xwsconn* pConnection = (xwsconn*)xrtAtomicPtrLoad(
		&pTest->Client,
		XMEMORY_ACQUIRE
	);

	(void)pWorker;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		xwswriter* pWriter = xrtWsConnBeginText(
			pConnection
		);

		if ( pWriter == NULL ) {
			testSingleWsFail(pTest);
			return;
		}
		#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
			char* sFirst = (char*)xrtMalloc(3);
			char* sLast = (char*)xrtMalloc(3);
			xnetref First;
			xnetref Last;
			xnetresult WriteResult;
			xnetresult FinishResult;

			if ( (sFirst == NULL) || (sLast == NULL) ) {
				xrtFree(sFirst);
				xrtFree(sLast);
				xrtWsWriterDestroy(pWriter);
				testSingleWsFail(pTest);
				return;
			}
			memcpy(sFirst, "sin", 3);
			memcpy(sLast, "gle", 3);
			First = (xnetref) {
				(cbytes)sFirst,
				3,
				testSingleWsRelease,
				pTest
			};
			Last = (xnetref) {
				(cbytes)sLast,
				3,
				testSingleWsRelease,
				pTest
			};
			WriteResult = xrtWsWriterWriteRef(
				pWriter,
				&First
			);
			FinishResult = xrtWsWriterFinishRef(
				pWriter,
				&Last
			);
			if ( WriteResult != XNET_RESULT_OK ) {
				xrtFree(sFirst);
			}
			if ( FinishResult != XNET_RESULT_OK ) {
				xrtFree(sLast);
			}
			if ( (WriteResult != XNET_RESULT_OK) ||
				(FinishResult != XNET_RESULT_OK) ||
				(xrtAtomic32Load(
					&pTest->Released,
					XMEMORY_ACQUIRE
				 ) != 2) ) {
				testSingleWsFail(pTest);
			}
		#else
			if ( (xrtWsWriterWrite(
				pWriter,
				XRT_BYTES_LITERAL("sin")
			) != XNET_RESULT_OK) ||
				(xrtWsWriterFinish(
					pWriter,
					XRT_BYTES_LITERAL("gle")
				 ) != XNET_RESULT_OK) ) {
				testSingleWsFail(pTest);
			}
		#endif
		xrtWsWriterDestroy(pWriter);
	#elif defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
		char* sText = (char*)xrtMalloc(6);
		xnetref Ref;
		xnetresult Result;

		if ( sText == NULL ) {
			testSingleWsFail(pTest);
			return;
		}
		memcpy(sText, "single", 6);
		Ref = (xnetref) {
			(cbytes)sText,
			6,
			testSingleWsRelease,
			pTest
		};
		Result = xrtWsConnTextRef(pConnection, &Ref);
		if ( Result != XNET_RESULT_OK ) {
			xrtFree(sText);
			testSingleWsFail(pTest);
		} else if ( xrtAtomic32Load(
			&pTest->Released,
			XMEMORY_ACQUIRE
		) != 1 ) {
			testSingleWsFail(pTest);
		}
	#else
		if ( xrtWsConnText(
			pConnection,
			XRT_STR_LITERAL("single")
		) != XNET_RESULT_OK ) {
			testSingleWsFail(pTest);
		}
	#endif
}



/* 在客户端 Worker 上发起标准关闭握手。 */
static void testSingleWsStop(
	xnetworker* pWorker,
	ptr pData
)
{
	testsinglews* pTest = (testsinglews*)pData;
	xwsconn* pConnection = (xwsconn*)xrtAtomicPtrLoad(
		&pTest->Client,
		XMEMORY_ACQUIRE
	);

	(void)pWorker;
	if ( xrtWsConnClose(
		pConnection,
		XWS_CLOSE_NORMAL,
		XRT_STR_LITERAL("")
	) != XNET_RESULT_OK ) {
		testSingleWsFail(pTest);
	}
}



/* 在截止时间前等待原子计数达到目标。 */
static bool testSingleWsWait(
	testsinglews* pTest,
	const xatomic32* pValue,
	uint32 iExpected
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(5000000)
	);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		if ( xrtAtomic32Load(
			&pTest->Failed,
			XMEMORY_ACQUIRE
		) || xrtDeadlineExpired(Deadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 验证单头发布能够完成已建立 WebSocket 会话的收发和关闭。 */
int main(void)
{
	testsinglews Test;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents ClientEvents;
	xnetaddr Address;
	xnetstream* pStream;
	xwsconn* pClient;
	xwsconn* pServer;
	xdeadline Deadline;

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	xrtAtomicPtrInit(&Test.Client, NULL);
	xrtAtomicPtrInit(&Test.Server, NULL);
	xrtAtomic32Init(&Test.Received, 0);
	xrtAtomic32Init(&Test.Closed, 0);
	xrtAtomic32Init(&Test.Failed, 0);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
		xrtAtomic32Init(&Test.Released, 0);
	#endif
	Test.ClientEndpoint.Test = &Test;
	Test.ClientEndpoint.Role = XWS_ROLE_CLIENT;
	Test.ServerEndpoint.Test = &Test;
	Test.ServerEndpoint.Role = XWS_ROLE_SERVER;
	Test.Events.MessageBegin = testSingleWsBegin;
	Test.Events.MessageData = testSingleWsData;
	Test.Events.MessageEnd = testSingleWsEnd;
	Test.Events.Error = testSingleWsError;
	Test.Events.Close = testSingleWsClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	Test.Engine = xrtNetEngineCreate(&EngineConfig);
	if ( (Test.Engine == NULL) ||
		 !xrtNetEngineStart(Test.Engine) ) {
		return 1;
	}
	xrtNetListenConfigInit(&ListenConfig);
	(void)xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	);
	ListenerEvents.Accept = testSingleWsAccept;
	Test.Listener = xrtNetListen(
		Test.Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Test
	);
	if ( (Test.Listener == NULL) ||
		 !xrtNetListenerLocal(Test.Listener, &Address) ) {
		return 2;
	}
	ClientEvents.Open = testSingleWsOpen;
	pStream = xrtNetStreamConnect(
		Test.Engine,
		&Address,
		0,
		NULL,
		&ClientEvents,
		&Test
	);
	if ( pStream == NULL ) {
		return 3;
	}

	Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	while ( ((pClient = (xwsconn*)xrtAtomicPtrLoad(
		&Test.Client,
		XMEMORY_ACQUIRE
	)) == NULL) || ((pServer = (xwsconn*)xrtAtomicPtrLoad(
		&Test.Server,
		XMEMORY_ACQUIRE
	)) == NULL) ) {
		if ( xrtAtomic32Load(
			&Test.Failed,
			XMEMORY_ACQUIRE
		) || xrtDeadlineExpired(Deadline) ) {
			return 4;
		}
		xrtThreadYield();
	}
	if ( !xrtNetEnginePost(
		Test.Engine,
		xrtNetWorkerIndex(xrtWsConnWorker(pClient)),
		testSingleWsSend,
		&Test
	) || !testSingleWsWait(
		&Test,
		&Test.Received,
		1
	) ) {
		return 5;
	}
	if ( !xrtNetEnginePost(
		Test.Engine,
		xrtNetWorkerIndex(xrtWsConnWorker(pClient)),
		testSingleWsStop,
		&Test
	) || !testSingleWsWait(
		&Test,
		&Test.Closed,
		2
	) ) {
		return 6;
	}
	if ( !xrtNetListenerClose(Test.Listener) ) {
		return 7;
	}
	while ( xrtNetListenerState(Test.Listener) !=
		XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtWsConnDestroy(pClient);
	xrtWsConnDestroy(pServer);
	xrtNetListenerDestroy(Test.Listener);
	return xrtNetEngineDestroy(Test.Engine) ? 0 : 8;
}
