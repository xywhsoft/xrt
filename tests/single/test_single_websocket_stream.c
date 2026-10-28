#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



typedef struct testsinglews testsinglews;



typedef struct testsinglewsendpoint {
	testsinglews* Test;
	xwsrole Role;
	size_t Size;
	char Data[16];
	#if defined(TEST_SINGLE_WS_OWNED)
		xatomic32 OwnedReferences;
		xatomic32 OwnedDrops;
	#endif
} testsinglewsendpoint;



struct testsinglews {
	xnetengine* Engine;
	xnetlistener* Listener;
	xatomicptr Client;
	xatomicptr Server;
	xatomic32 Received;
	xatomic32 Closed;
	xatomic32 Failed;
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
		xatomic32 Released;
	#endif
	xwsstreamevents Events;
	#if defined(TEST_SINGLE_WS_OWNED)
		xwsstreamownershipv1 OwnedPolicy;
	#endif
	testsinglewsendpoint ClientEndpoint;
	testsinglewsendpoint ServerEndpoint;
};



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
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
	xwsstream* pConnection,
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
	xwsstream* pConnection,
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
	xwsstream* pConnection,
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
		if ( xrtWsStreamText(
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
	xwsstream* pConnection,
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
	xwsstream* pConnection,
	const xwsstreamclose* pClose,
	ptr pData
)
{
	testsinglewsendpoint* pEndpoint =
		(testsinglewsendpoint*)pData;

	(void)pConnection;
	if ( ((pClose->Flags & XWS_STREAM_CLOSE_CLEAN) == 0) ||
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
#if defined(TEST_SINGLE_WS_OWNED)
static bool testSingleWsOwnedCount(const void* data,size_t* count)
{
	const testsinglewsendpoint* endpoint=data;
	uint32 n=xrtAtomic32Load(&endpoint->OwnedReferences,XMEMORY_ACQUIRE);
	if(!count||!n)return false;
	*count=n;return true;
}
static bool testSingleWsOwnedTrace(const void* data,xrtownershipvisitor visit,ptr context)
{size_t count;(void)context;return visit&&testSingleWsOwnedCount(data,&count);}
static void testSingleWsOwnedDrop(const void* data)
{
	testsinglewsendpoint* endpoint=(testsinglewsendpoint*)data;
	if(xrtAtomic32FetchSub(&endpoint->OwnedReferences,1,XMEMORY_ACQ_REL)!=1)
		testSingleWsFail(endpoint->Test);
	xrtAtomic32FetchAdd(&endpoint->OwnedDrops,1,XMEMORY_RELEASE);
}
static const xrtownershipops testSingleWsOwnedOps={testSingleWsOwnedCount,testSingleWsOwnedTrace};
static bool testSingleWsOwnedVisit(xrtownershipref ref,ptr data)
{
	size_t* count=data;
	if(!ref.Data||!ref.Ops||!ref.Ops->Count||!ref.Ops->Trace)return false;
	++*count;return true;
}
static bool testSingleWsCollectClosed(xwsstream** slot,testsinglews* test)
{
	xwsstream* stream=*slot;
	xrtownershipref ref=xrtWsStreamOwnership(stream);
	const xrtownershippreparationv1* preparing=NULL;
	const xrtownershipadapterv1* adapter=NULL;
	const xwsstreamownershipv1* policies[]={&test->OwnedPolicy};
	for(size_t i=0;i<1000000&&!adapter;++i){
		xrtownershipscope frozen={0};size_t count=0,edges=0;
		if(xrtOwnershipFreezeTryBegin(&frozen)){
			if(ref.Ops&&ref.Ops->Count(ref.Data,&count)&&count==1&&
			   ref.Ops->Trace(ref.Data,testSingleWsOwnedVisit,&edges)&&edges==2){
				if(xrtWsStreamOwnershipAdapterV1(ref,NULL,0,&preparing)){
					(void)xrtOwnershipScopeEnd(&frozen);return false;
				}
				adapter=xrtWsStreamOwnershipAdapterV1(ref,policies,1,&preparing);
			}
			if(!xrtOwnershipScopeEnd(&frozen))return false;
		}
		if(!adapter)xrtThreadYield();
	}
	if(!adapter||!preparing||!adapter->Hold(ref.Data))return false;
	/* Turn the caller's user credit into the inspection-only plan hold. */
	xrtWsStreamDestroy(stream);*slot=NULL;
	bool claimed=false;
	for(size_t i=0;i<1000000&&!claimed;++i){
		xrtownershipscope frozen={0};
		if(xrtOwnershipFreezeTryBegin(&frozen)){
			claimed=adapter->Claim(ref.Data,test);
			if(!xrtOwnershipScopeEnd(&frozen))return false;
		}
		if(!claimed)xrtThreadYield();
	}
	if(!claimed){adapter->Drop(ref.Data);return false;}
	bool cleared=false;
	for(size_t i=0;i<1000000&&!cleared;++i){
		xrtownershipprepareresult result=preparing->Prepare(ref.Data,test);
		if(result==XRT_OWNERSHIP_PREPARE_FAILED)return false;
		xrtownershipscope frozen={0};
		if(xrtOwnershipFreezeTryBegin(&frozen)){
			if(preparing->Ready(ref.Data)){adapter->Clear(ref.Data,test);cleared=true;}
			if(!xrtOwnershipScopeEnd(&frozen))return false;
		}
		if(!cleared)xrtThreadYield();
	}
	if(!cleared)return false;
	bool finished=adapter->Finish(ref.Data,test);
	if(finished)finished=adapter->Finish(ref.Data,test); /* idempotent completion */
	adapter->Drop(ref.Data);return finished;
}
#endif
#if defined(TEST_SINGLE_WS_BUFFERED)
static xwsstream* testSingleWsAttachInitial(xnetstream* stream,xnetbuf* initial,
	const xwsstreamconfig* config,testsinglewsendpoint* endpoint)
{
	#if defined(TEST_SINGLE_WS_OWNED)
		return xrtWsStreamAttachOwnedV1(stream,initial,config,endpoint,&endpoint->Test->OwnedPolicy);
	#else
		return xrtWsStreamAttachBuffered(stream,initial,config,&endpoint->Test->Events,endpoint);
	#endif
}
#endif
static xwsstream* testSingleWsAttach(
	xnetstream* pStream,
	testsinglewsendpoint* pEndpoint
)
{
	xwsstreamconfig Config;

	xrtWsStreamConfigInit(&Config);
	Config.Role = pEndpoint->Role;
	#if defined(TEST_SINGLE_WS_OWNED)
		xrtAtomic32Init(&pEndpoint->OwnedReferences,1);
	#endif
	#if defined(TEST_SINGLE_WS_BUFFERED)
		xnetbuf Initial={0};
		uint8 Head=0x89; /* Server Ping header split after its first byte. */
		if (pEndpoint->Role == XWS_ROLE_CLIENT) {
			if (!xrtNetBufAppend(&Initial,&Head,1)) return NULL;
			xwsstreamconfig Bad=Config;Bad.SendLimit=0;
			if (testSingleWsAttachInitial(pStream,&Initial,&Bad,pEndpoint) || Initial.Size != 1) {
				xrtNetBufClear(&Initial);testSingleWsFail(pEndpoint->Test);return NULL;
			}
			xrtClearError();
		}
		xwsstream* Result=testSingleWsAttachInitial(pStream,&Initial,&Config,pEndpoint);
		if (!Result) {xrtNetBufClear(&Initial);return NULL;}
		if (Initial.Size != 0) {testSingleWsFail(pEndpoint->Test);return Result;}
		if (pEndpoint->Role == XWS_ROLE_SERVER) {
			uint8 Tail=0;
			if (xrtNetStreamSend(pStream,&Tail,1) != XNET_RESULT_OK)
				testSingleWsFail(pEndpoint->Test);
		}
		return Result;
	#else
	return xrtWsStreamAttach(
		pStream,
		0,
		&Config,
		&pEndpoint->Test->Events,
		pEndpoint
	);
	#endif
}



/* 接管 Listener 交付的服务端 Stream。 */
static bool testSingleWsAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testsinglews* pTest = (testsinglews*)pData;
	xwsstream* pConnection;

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
	xwsstream* pConnection = testSingleWsAttach(
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
	xwsstream* pConnection = (xwsstream*)xrtAtomicPtrLoad(
		&pTest->Client,
		XMEMORY_ACQUIRE
	);

	(void)pWorker;
	#if defined(XRT_FEATURE_WEBSOCKET_WRITER)
		xwswriter* pWriter = xrtWsStreamBeginText(
			pConnection
		);

		if ( pWriter == NULL ) {
			testSingleWsFail(pTest);
			return;
		}
		#if defined(XRT_FEATURE_WEBSOCKET_WRITER_REF)
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
	#elif defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
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
		Result = xrtWsStreamTextRef(pConnection, &Ref);
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
		if ( xrtWsStreamText(
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
	xwsstream* pConnection = (xwsstream*)xrtAtomicPtrLoad(
		&pTest->Client,
		XMEMORY_ACQUIRE
	);

	(void)pWorker;
	if ( xrtWsStreamClose(
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
	double Deadline = __xrtWaitAfter(
		INT64_C(5000)
	);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		if ( xrtAtomic32Load(
			&pTest->Failed,
			XMEMORY_ACQUIRE
		) || __xrtWaitExpired(Deadline) ) {
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
	xwsstream* pClient;
	xwsstream* pServer;
	double Deadline;

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	xrtAtomicPtrInit(&Test.Client, NULL);
	xrtAtomicPtrInit(&Test.Server, NULL);
	xrtAtomic32Init(&Test.Received, 0);
	xrtAtomic32Init(&Test.Closed, 0);
	xrtAtomic32Init(&Test.Failed, 0);
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
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
	#if defined(TEST_SINGLE_WS_OWNED)
		Test.OwnedPolicy=(xwsstreamownershipv1){sizeof(Test.OwnedPolicy),Test.Events,
			testSingleWsOwnedDrop,&testSingleWsOwnedOps,NULL};
	#endif

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

	Deadline = __xrtWaitAfter(INT64_C(5000));
	while ( ((pClient = (xwsstream*)xrtAtomicPtrLoad(
		&Test.Client,
		XMEMORY_ACQUIRE
	)) == NULL) || ((pServer = (xwsstream*)xrtAtomicPtrLoad(
		&Test.Server,
		XMEMORY_ACQUIRE
	)) == NULL) ) {
		if ( xrtAtomic32Load(
			&Test.Failed,
			XMEMORY_ACQUIRE
		) || __xrtWaitExpired(Deadline) ) {
			return 4;
		}
		xrtThreadYield();
	}
	if ( !xrtNetEnginePost(
		Test.Engine,
		xrtNetWorkerIndex(xrtWsStreamWorker(pClient)),
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
		xrtNetWorkerIndex(xrtWsStreamWorker(pClient)),
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
	#if defined(TEST_SINGLE_WS_OWNED)
		if(!testSingleWsCollectClosed(&pClient,&Test)||!testSingleWsCollectClosed(&pServer,&Test))return 9;
	#else
		xrtWsStreamDestroy(pClient);
		xrtWsStreamDestroy(pServer);
	#endif
	xrtNetListenerDestroy(Test.Listener);
	bool destroyed=xrtNetEngineDestroy(Test.Engine);
	#if defined(TEST_SINGLE_WS_OWNED)
		if(xrtAtomic32Load(&Test.ClientEndpoint.OwnedReferences,XMEMORY_ACQUIRE)||
		   xrtAtomic32Load(&Test.ServerEndpoint.OwnedReferences,XMEMORY_ACQUIRE)||
		   xrtAtomic32Load(&Test.ClientEndpoint.OwnedDrops,XMEMORY_ACQUIRE)!=1||
		   xrtAtomic32Load(&Test.ServerEndpoint.OwnedDrops,XMEMORY_ACQUIRE)!=1||
		   xrtAtomic32Load(&Test.Failed,XMEMORY_ACQUIRE))return 10;
	#endif
	return destroyed ? 0 : 8;
}
