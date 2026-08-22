/*
 * XRT Example - TCP Chatroom
 * XRT 范例 - TCP 聊天室
 *
 * Description / 说明:
 *   EN: Demonstrates a small TCP chatroom where messages are broadcast to other clients.
 *   CN: 演示一个简单 TCP 聊天室，收到的消息会广播给其他客户端。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/tools_tcp_chatroom.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/tools_tcp_chatroom -lm -lpthread
 */
#include "../../../xrt.c"

typedef struct {
	xparray pClients;
} chatroom_ctx;

typedef struct {
	char aRecv[256];
	volatile long iRecvCount;
} chat_client_ctx;


static bool ChatOnAccept(ptr pOwner, xnetlistener* pListener, xnetstream* pStream)
{
	chatroom_ctx* pRoom = (chatroom_ctx*)pOwner;

	(void)pListener;
	if ( !pRoom || !pStream ) return FALSE;
	(void)xrtPtrArrayAppend(pRoom->pClients, pStream);
	xrtNetStreamSetUserData(pStream, pRoom);
	return TRUE;
}



static void ChatServerOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	chatroom_ctx* pRoom = (chatroom_ctx*)pOwner;
	char sMsg[256] = { 0 };
	size_t iBytes;
	size_t iCopy;
	uint32 i;

	if ( !pRoom || !pChain ) return;

	iBytes = xrtNetChainBytes(pChain);
	iCopy = iBytes;
	if ( iCopy >= sizeof(sMsg) ) {
		iCopy = sizeof(sMsg) - 1u;
	}
	(void)xrtNetChainPeek(pChain, sMsg, iCopy);
	xrtNetChainConsume(pChain, iBytes);

	for ( i = 1; i <= pRoom->pClients->Count; i++ ) {
		xnetstream* pTarget = (xnetstream*)xrtPtrArrayGet(pRoom->pClients, i);
		if ( pTarget && pTarget != pStream ) {
			(void)xrtNetStreamSend(pTarget, sMsg, iCopy);
		}
	}
}



static void ChatServerOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	chatroom_ctx* pRoom = (chatroom_ctx*)pOwner;
	uint32 i;

	(void)iReason;
	if ( !pRoom || !pStream ) return;

	for ( i = 1; i <= pRoom->pClients->Count; i++ ) {
		if ( xrtPtrArrayGet(pRoom->pClients, i) == pStream ) {
			(void)xrtPtrArraySet(pRoom->pClients, i, NULL);
			break;
		}
	}
}



static void ChatClientOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	chat_client_ctx* pCtx = (chat_client_ctx*)pOwner;
	size_t iBytes;
	size_t iCopy;

	(void)pStream;
	if ( !pCtx || !pChain ) return;

	iBytes = xrtNetChainBytes(pChain);
	iCopy = iBytes;
	if ( iCopy >= sizeof(pCtx->aRecv) ) {
		iCopy = sizeof(pCtx->aRecv) - 1u;
	}
	(void)xrtNetChainPeek(pChain, pCtx->aRecv, iCopy);
	pCtx->aRecv[iCopy] = '\0';
	xrtNetChainConsume(pChain, iBytes);
	pCtx->iRecvCount++;
}



static bool WaitRecv(volatile long* pValue)
{
	double fDeadline = xrtTimer() + 1.5;

	while ( xrtTimer() < fDeadline ) {
		if ( *pValue > 0 ) {
			return TRUE;
		}
		xrtSleep(5);
	}
	return *pValue > 0;
}



int main()
{
	chatroom_ctx tRoom = { 0 };
	chat_client_ctx tAlice = { 0 };
	chat_client_ctx tBob = { 0 };
	xnetengineconfig tCfg;
	xnetlistenconfig tListenCfg;
	xnetconnectconfig tConnCfg;
	xnetlistenerevents tListenerEvents;
	xnetstreamevents tServerEvents;
	xnetstreamevents tClientEvents;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	xnetstream* pAlice = NULL;
	xnetstream* pBob = NULL;

	xrtInit();

	tRoom.pClients = xrtPtrArrayCreate(XRT_OBJMODE_SHARED);

	memset(&tListenerEvents, 0, sizeof(tListenerEvents));
	tListenerEvents.OnAccept = ChatOnAccept;

	memset(&tServerEvents, 0, sizeof(tServerEvents));
	tServerEvents.OnRecv = ChatServerOnRecv;
	tServerEvents.OnClose = ChatServerOnClose;

	memset(&tClientEvents, 0, sizeof(tClientEvents));
	tClientEvents.OnRecv = ChatClientOnRecv;

	xrtNetEngineConfigInit(&tCfg);
	tCfg.iWorkerCount = 1;
	xrtNetListenConfigInit(&tListenCfg);
	xrtNetConnectConfigInit(&tConnCfg);
	xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
	tConnCfg.sHost = "127.0.0.1";

	pEngine = xrtNetEngineCreate(&tCfg);
	xrtNetEngineStart(pEngine);
	pListener = xrtNetListenerCreate(pEngine, &tListenCfg, &tListenerEvents, &tServerEvents, &tRoom);
	xrtNetListenerStart(pListener);

	tConnCfg.iPort = pListener->tConfig.tBindAddr.iPort;
	pAlice = xrtNetStreamCreate(pEngine, &tClientEvents, &tAlice);
	pBob = xrtNetStreamCreate(pEngine, &tClientEvents, &tBob);
	(void)xrtNetStreamConnect(pAlice, &tConnCfg);
	(void)xrtNetStreamConnect(pBob, &tConnCfg);

	xrtSleep(150);

	(void)xrtNetStreamSend(pAlice, "Alice: hello", 12);
	(void)WaitRecv(&tBob.iRecvCount);
	(void)xrtNetStreamSend(pBob, "Bob: hi", 7);
	(void)WaitRecv(&tAlice.iRecvCount);

	printf("alice_received = %s\n", tAlice.aRecv);
	printf("bob_received = %s\n", tBob.aRecv);

	xrtNetStreamClose(pAlice, 0);
	xrtNetStreamClose(pBob, 0);
	xrtSleep(80);
	xrtNetStreamDestroy(pAlice);
	xrtNetStreamDestroy(pBob);
	xrtNetListenerStop(pListener);
	xrtNetListenerDestroy(pListener);
	xrtNetEngineStop(pEngine);
	xrtNetEngineDestroy(pEngine);
	xrtPtrArrayDestroy(tRoom.pClients);
	xrtUnit();
	return 0;
}
