#include "../fixtures/tls_server.h"
#include <xrt/websocket_upgrade.h>
#include <xrt/http1_tls.h>
#include <xrt/thread.h>
#include <stddef.h>

typedef struct exchangeTest exchangeTest;
typedef struct exchangeOwner {xatomic32 Refs,Drops;} exchangeOwner;
typedef struct exchangeHTTP {exchangeOwner Owner;exchangeTest* Test;} exchangeHTTP;
struct exchangeTest {
    exchangeOwner Owner,WSOwner,Alternate;
    xnetengine* Engine;xnetlistener* Listener;xtlsstream* Client;xatomicptr Server;
    xtlscontext* Context;xtlsidentity* Identity;xtlsverifier* Verifier;xtlsserverconfig Config;
    exchangeHTTP* HTTP;xwsstream* WS;xatomic32 Opened,Connected,Bound,Proceed,NeedSecond,Done,Messages,HTTPDropped;
    xnetpost StartPost,HTTPPost,SecondPost;xnetbuf Initial;bool Queued,Finished,Published;unsigned Mode;
    char Key[XWS_KEY_CAPACITY],Response[512],Message[16];size_t ResponseSize,MessageSize;
    #if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
    exchangeOwner AsyncOwner;xfuture* AsyncRead;xnetpost AsyncPost;xtlsstreambindingv1 HTTPReturned;
    #endif
};
static void exchangeAwait(xatomic32* A,uint32 N)
{double End=xrtTimer()+10;while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)<N){testRequire(xrtTimer()<End,"exchange callback barrier");xrtSleep(1);}}
static void exchangeOwnerInit(exchangeOwner* O,uint32 Refs)
{xrtAtomic32Init(&O->Refs,Refs);xrtAtomic32Init(&O->Drops,0);}
static bool exchangeCount(const void* Data,size_t* N)
{*N=xrtAtomic32Load(&((const exchangeOwner*)Data)->Refs,XMEMORY_ACQUIRE);return true;}
static bool exchangeTrace(const void* D,xrtownershipvisitor V,ptr U)
{(void)D;(void)V;(void)U;return false;}
static void exchangeDrop(const void* Data)
{exchangeOwner* O=(exchangeOwner*)Data;xrtAtomic32FetchSub(&O->Refs,1,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&O->Drops,1,XMEMORY_RELEASE);}
static const xrtownershipops ExchangeOps={exchangeCount,exchangeTrace};
static void exchangeHTTPDrop(const void* Data)
{
    exchangeHTTP* H=(exchangeHTTP*)Data;exchangeTest* T=H->Test;
    testRequire(T->Published&&T->WS&&xrtAtomic32Load(&H->Owner.Refs,XMEMORY_ACQUIRE)==1,"old sole event owner drops after composite publication");
    exchangeDrop(H);xrtAtomic32FetchAdd(&T->HTTPDropped,1,XMEMORY_RELEASE);
    if(T->Mode==2)testRequire(xrtWsStreamAbort(T->WS),"old owner Drop safely reenters new WS facade");
    xrtFree(H);
}
static void exchangeWork(xnetworker*,ptr);
static void exchangeStart(xnetworker*,ptr);
static void exchangeHTTPRead(xtlsstream*,const xnetbuf*,ptr);
static void exchangeServerRead(xtlsstream*,const xnetbuf*,ptr);
static const xtlsstreamownershipv1 ExchangeHTTPPolicy={sizeof(xtlsstreamownershipv1),{.Read=exchangeHTTPRead},exchangeHTTPDrop,&ExchangeOps};
static const xtlsstreamownershipv1 ExchangeAlternatePolicy={sizeof(xtlsstreamownershipv1),{0},exchangeDrop,&ExchangeOps};
static void exchangeOpen(xtlsstream* S,ptr Data)
{
    exchangeTest* T=Data;xrtAtomic32FetchAdd(&T->Opened,1,XMEMORY_RELEASE);
    if(xrtTlsSessionRole(xrtTlsStreamSession(S))==XTLS_CLIENT)testRequire(xrtNetPostInit(&T->StartPost)&&xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(S)),&T->StartPost,exchangeStart,T),"post initial HTTP owner outside TLS Open/drive");
}
static const xtlsstreamownershipv1 ExchangeNativePolicy={sizeof(xtlsstreamownershipv1),{.Open=exchangeOpen,.Read=exchangeServerRead},exchangeDrop,&ExchangeOps};
static void exchangeBegin(xwsstream* S,const xwsmessageinfo* M,ptr Data)
{(void)S;exchangeTest* T=(exchangeTest*)((char*)Data-offsetof(exchangeTest,WSOwner));testRequire(M->Opcode==XWS_OPCODE_TEXT,"exchange Text message");T->MessageSize=0;}
static void exchangeData(xwsstream* S,xbytesview B,ptr Data)
{(void)S;exchangeTest* T=(exchangeTest*)((char*)Data-offsetof(exchangeTest,WSOwner));testRequire(B.Size<=sizeof(T->Message)-T->MessageSize,"exchange payload bound");memcpy(T->Message+T->MessageSize,B.Data,B.Size);T->MessageSize+=B.Size;}
static void exchangeEnd(xwsstream* S,ptr Data)
{(void)S;exchangeTest* T=(exchangeTest*)((char*)Data-offsetof(exchangeTest,WSOwner));uint32 I=xrtAtomic32Load(&T->Messages,XMEMORY_ACQUIRE);cstr Expected=T->Mode==1&&I==0?"[3]":"[7]";testRequire(T->MessageSize==3&&!memcmp(T->Message,Expected,3),"exclusive Initial and coalesced native suffix delivered in order");xrtAtomic32FetchAdd(&T->Messages,1,XMEMORY_RELEASE);}
static const xwsstreamownershipv1 ExchangeWSPolicy={sizeof(xwsstreamownershipv1),{.MessageBegin=exchangeBegin,.MessageData=exchangeData,.MessageEnd=exchangeEnd},exchangeDrop,&ExchangeOps,NULL};
static void exchangeSend(xtlsstream* S,const void* Data,size_t N)
{size_t Written=0;testRequire(xrtTlsStreamSend(S,Data,N,&Written)==XTLS_OK&&Written==N,"fixture complete TLS offer");}
static void exchangeSendRequest(exchangeTest* T)
{
    testRequire(xrtWsKeyGenerate(T->Key,sizeof(T->Key)),"OS-random client nonce");xhttpfield Fields[XWS_UPGRADE_REQUEST_FIELDS_MAX];size_t Count=0,N=0;char Wire[512];
    testRequire(xrtWsUpgradeRequestFields(XRT_STR_LITERAL("exchange.fixture"),(xstrview){T->Key,XWS_KEY_SIZE},XRT_STR_LITERAL("jsonrpc"),(xstrview){0},Fields,XWS_UPGRADE_REQUEST_FIELDS_MAX,&Count)&&xrtHttp1RequestWrite(XRT_STR_LITERAL("GET"),XRT_STR_LITERAL("/rpc"),XHTTP_VERSION_1_1,Fields,Count,Wire,sizeof(Wire),&N),"strict Upgrade request");exchangeSend(T->Client,Wire,N);
}
#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
static void exchangeAfterAsync(xnetworker* W,ptr Data)
{
    exchangeTest* T=Data;
    if(xrtTlsStreamAsyncCount(T->Client)){
        testRequire(xrtNetPost(W,&T->AsyncPost,exchangeAfterAsync,T),"join cancelled read outside TLS driver");return;
    }
    testRequire(xrtFutureState(T->AsyncRead)==XFUTURE_CANCELLED,"temporary read cancelled without consuming HTTP");xrtFutureDestroy(T->AsyncRead);T->AsyncRead=NULL;
    xtlsstreambindingv1 Expected={&T->AsyncOwner,&ExchangeAlternatePolicy},Previous={0};
    testRequire(xrtTlsStreamExchangeEventsOwnedV1(T->Client,&Expected,T->HTTPReturned.Data,T->HTTPReturned.Policy,&Previous)&&Previous.Data==&T->AsyncOwner,"restore HTTP only after physical async read release");Previous.Policy->Drop(Previous.Data);memset(&T->HTTPReturned,0,sizeof(T->HTTPReturned));
    exchangeSendRequest(T);
}
#endif
static void exchangeStart(xnetworker* W,ptr Data)
{
    (void)W;exchangeTest* T=Data;exchangeAwait(&T->Connected,1);exchangeHTTP* H=xrtCalloc(1,sizeof(*H));testRequire(H!=NULL,"HTTP sole-owner allocation");exchangeOwnerInit(&H->Owner,1);H->Test=T;T->HTTP=H;
    testRequire(xrtTlsStreamBindEventsOwnedV1(T->Client,H,&ExchangeHTTPPolicy),"first owned TLS->HTTP handoff");xrtAtomic32Store(&T->Bound,1,XMEMORY_RELEASE);exchangeAwait(&T->Proceed,1);
    #if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
    xtlsstreambindingv1 Expected={H,&ExchangeHTTPPolicy},Previous={(ptr)(uintptr_t)17,(const xtlsstreamownershipv1*)(uintptr_t)19};
    testRequire(xrtTlsStreamExchangeEventsOwnedV1(T->Client,&Expected,&T->AsyncOwner,&ExchangeAlternatePolicy,&T->HTTPReturned),"temporary pull binding");
    T->AsyncRead=xrtTlsStreamWaitAsync(T->Client,XTLS_STREAM_WAIT_READ);testRequire(T->AsyncRead&&xrtTlsStreamAsyncCount(T->Client)==1&&xrtFutureState(T->AsyncRead)==XFUTURE_PENDING,"actual pending async read");Expected=(xtlsstreambindingv1){&T->AsyncOwner,&ExchangeAlternatePolicy};
    testRequire(!xrtTlsStreamExchangeEventsOwnedV1(T->Client,&Expected,H,&ExchangeHTTPPolicy,&Previous)&&xrtErrorKind(xrtGetError())==XERR_STATE&&Previous.Data==(ptr)(uintptr_t)17,"pending async reader preserves current binding/output/HTTP credit");xrtClearError();
    testRequire(xrtFutureCancel(T->AsyncRead)&&xrtNetPost(W,&T->AsyncPost,exchangeAfterAsync,T),"cancel then join temporary read");
    #else
    exchangeSendRequest(T);
    #endif
}
static void exchangeServerRead(xtlsstream* S,const xnetbuf* B,ptr Data)
{
    (void)B;exchangeTest* T=Data;if(xrtTlsSessionRole(xrtTlsStreamSession(S))!=XTLS_SERVER)return;
    xhttpfield Fields[16];xhttp1head Head={.Fields=Fields,.FieldCapacity=16};xhttp1status Status=xrtHttp1RequestParseTls(S,&Head,NULL,NULL);
    if(Status==XHTTP1_MORE){testRequire(xrtTlsStreamReadMore(S),"grow partial HTTP request");return;}testRequire(Status==XHTTP1_READY,"strict request header");
    xwsupgradeserverconfig Config;xrtWsUpgradeServerConfigInit(&Config);Config.Protocols=XRT_STR_LITERAL("jsonrpc");xwsupgrade U;testRequire(xrtWsUpgradeRequestCheck(&Head,&Config,&U)&&U.Protocol.Size==7,"server Upgrade verification");
    xhttpfield Response[5];size_t Count=0,N=0;testRequire(xrtWsUpgradeResponseFields((xstrview){U.Accept,XWS_ACCEPT_SIZE},U.Protocol,(xstrview){0},Response,5,&Count)&&xrtHttp1ResponseWrite(XHTTP_VERSION_1_1,101,XRT_STR_LITERAL("Switching Protocols"),Response,Count,T->Response,sizeof(T->Response)-5,&N),"server 101 response");
    static const unsigned char Frame[]={129,3,'[','7',']'};memcpy(T->Response+N,Frame,sizeof(Frame));T->ResponseSize=N+sizeof(Frame);testRequire(xrtTlsStreamConsume(S,Head.Bytes),"consume validated request only");
    exchangeSend(S,T->Response,T->Mode==1?9:T->ResponseSize);
}
static void exchangeHTTPRead(xtlsstream* S,const xnetbuf* B,ptr Data)
{
    (void)B;exchangeHTTP* H=Data;exchangeTest* T=H->Test;if(T->Finished)return;
    xtlsstreambindingv1 Expected={H,&ExchangeHTTPPolicy},Previous={(ptr)(uintptr_t)17,(const xtlsstreamownershipv1*)(uintptr_t)19};
    testRequire(!xrtTlsStreamExchangeEventsOwnedV1(S,&Expected,&T->Alternate,&ExchangeAlternatePolicy,&Previous)&&xrtErrorKind(xrtGetError())==XERR_STATE&&Previous.Data==(ptr)(uintptr_t)17,"exchange rejected inside active native read callback");xrtClearError();
    if(!T->Queued){T->Queued=true;testRequire(xrtNetPost(T->Engine?xrtNetEngineWorker(T->Engine,1):NULL,&T->HTTPPost,exchangeWork,T),"post HTTP->WS outside callback");}
}
static void exchangeSecond(xnetworker* W,ptr Data)
{(void)W;exchangeTest* T=Data;exchangeSend(xrtAtomicPtrLoad(&T->Server,XMEMORY_ACQUIRE),T->Response+9,T->ResponseSize-9);}
static void exchangeWork(xnetworker* W,ptr Data)
{
    (void)W;exchangeTest* T=Data;T->Queued=false;if(T->Finished)return;exchangeHTTP* H=T->HTTP;
    xhttpfield Fields[16];xhttp1head Head={.Fields=Fields,.FieldCapacity=16};xhttp1status Status=xrtHttp1ResponseParseTls(T->Client,&Head,NULL,NULL);xtlsstreambindingv1 Expected={H,&ExchangeHTTPPolicy},Previous={(ptr)(uintptr_t)17,(const xtlsstreamownershipv1*)(uintptr_t)19};
    if(Status==XHTTP1_MORE){
        testRequire(xrtTlsStreamReadMore(T->Client),"retain partial response prefix");
        testRequire(!xrtTlsStreamExchangeEventsOwnedV1(T->Client,&Expected,&T->Alternate,&ExchangeAlternatePolicy,&Previous)&&xrtErrorKind(xrtGetError())==XERR_STATE&&Previous.Data==(ptr)(uintptr_t)17,"pending incremental read blocks exchange");xrtClearError();xrtAtomic32Store(&T->NeedSecond,1,XMEMORY_RELEASE);return;
    }
    testRequire(Status==XHTTP1_READY,"strict response header");xwsupgradeclientconfig C;xrtWsUpgradeClientConfigInit(&C);C.Protocols=XRT_STR_LITERAL("jsonrpc");xwsupgrade U;testRequire(xrtWsUpgradeResponseCheck(&Head,(xstrview){T->Key,XWS_KEY_SIZE},&C,&U)&&U.Protocol.Size==7&&!memcmp(U.Protocol.Data,"jsonrpc",7),"client nonce/subprotocol/101 verification");
    /* Exchange itself allocates nothing; a composite WS factory still has
     * its existing constructor allocations and must respect the caller quota. */
    xallocationbudget* DirectBudget=NULL;xallocationscope DirectScope={0};testRequire(xrtAllocationBudgetCreate(1,&DirectBudget)&&xrtAllocationBudgetEnter(DirectBudget,&DirectScope),"direct exchange budget1");
    testRequire(xrtTlsStreamExchangeEventsOwnedV1(T->Client,&Expected,&T->Alternate,&ExchangeAlternatePolicy,&Previous)&&Previous.Data==H&&Previous.Policy==&ExchangeHTTPPolicy&&xrtAllocationBudgetLeave(&DirectScope),"direct exchange moves old owner under original quota");
    xallocationbudgetinfo DirectInfo={0};DirectInfo.Size=sizeof(DirectInfo);testRequire(xrtAllocationBudgetInfo(DirectBudget,&DirectInfo)&&!DirectInfo.LiveBytes&&!DirectInfo.LiveCount&&!DirectInfo.PeakBytes&&!DirectInfo.Rejections,"direct exchange has no heap requests");xrtAllocationBudgetFree(DirectBudget);
    xtlsstreambindingv1 HTTPReturned=Previous,AltExpected={&T->Alternate,&ExchangeAlternatePolicy},AltReturned={0};
    xrtAtomic32FetchAdd(&T->Alternate.Refs,1,XMEMORY_RELAXED); /* real new caller credit */
    testRequire(xrtTlsStreamExchangeEventsOwnedV1(T->Client,&AltExpected,&T->Alternate,&ExchangeAlternatePolicy,&AltReturned)&&AltReturned.Data==&T->Alternate&&!xrtAtomic32Load(&T->Alternate.Drops,XMEMORY_ACQUIRE),"same identity exchanges distinct actual owner credits");AltReturned.Policy->Drop(AltReturned.Data);
    testRequire(xrtTlsStreamExchangeEventsOwnedV1(T->Client,&AltExpected,HTTPReturned.Data,HTTPReturned.Policy,&AltReturned)&&AltReturned.Data==&T->Alternate,"restore HTTP by consuming returned owner");AltReturned.Policy->Drop(AltReturned.Data);memset(&HTTPReturned,0,sizeof(HTTPReturned));
    Previous=(xtlsstreambindingv1){(ptr)(uintptr_t)17,(const xtlsstreamownershipv1*)(uintptr_t)19};
    T->Finished=true;testRequire(xrtTlsStreamConsume(T->Client,Head.Bytes),"consume only validated 101, preserve suffix");
    if(T->Mode==1){static const unsigned char Prefix[]={129,3,'[','3',']'};testRequire(xrtNetBufAppend(&T->Initial,Prefix,sizeof(Prefix)),"owned read-ahead prefix");}
    xwsstreamconfig K;xrtWsStreamConfigInit(&K);K.Role=XWS_ROLE_CLIENT;K.Protocol=XRT_STR_LITERAL("jsonrpc");xwsstreamconfig Bad=K;Bad.MessageLimit=0;size_t Bytes=xrtNetBufSize(&T->Initial);const xnetblock* First=T->Initial.Head;
    testRequire(!xrtWsStreamAttachTlsExchangeOwnedV1(T->Client,&T->Initial,&Bad,&T->WSOwner,&ExchangeWSPolicy,&Expected,&Previous)&&Previous.Data==(ptr)(uintptr_t)17&&xrtNetBufSize(&T->Initial)==Bytes&&T->Initial.Head==First,"invalid config preserves native/Initial/output/owners");xrtClearError();
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"composite original caller budget");
    testRequire(!xrtWsStreamAttachTlsExchangeOwnedV1(T->Client,&T->Initial,&K,&T->WSOwner,&ExchangeWSPolicy,&Expected,&Previous)&&xrtErrorKind(xrtGetError())==XERR_MEMORY&&xrtAllocationBudgetLeave(&Scope),"composite respects original OOM quota");xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(Budget,&I)&&!I.LiveBytes&&!I.LiveCount&&I.Rejections&&Previous.Data==(ptr)(uintptr_t)17&&xrtNetBufSize(&T->Initial)==Bytes&&T->Initial.Head==First,"OOM rollback/ref/Initial/output ledger");xrtAllocationBudgetFree(Budget);
    xtlsstreambindingv1 Wrong={&T->Alternate,&ExchangeHTTPPolicy};testRequire(!xrtWsStreamAttachTlsExchangeOwnedV1(T->Client,&T->Initial,&K,&T->WSOwner,&ExchangeWSPolicy,&Wrong,&Previous)&&xrtErrorKind(xrtGetError())==XERR_STATE&&Previous.Data==(ptr)(uintptr_t)17&&xrtNetBufSize(&T->Initial)==Bytes&&T->Initial.Head==First,"exact previous identity prevents wrong handoff");xrtClearError();
    testRequire(!xrtTlsStreamExchangeEventsOwnedV1(T->Client,&Expected,&T->Alternate,&ExchangeAlternatePolicy,&Expected)&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"expected/output alias rejected");xrtClearError();
    testRequire(!xrtAtomic32Load(&T->HTTPDropped,XMEMORY_ACQUIRE)&&xrtAtomic32Load(&T->WSOwner.Refs,XMEMORY_ACQUIRE)==2&&xrtAtomic32Load(&H->Owner.Refs,XMEMORY_ACQUIRE)==1,"failed factories consume no credit");
    T->WS=xrtWsStreamAttachTlsExchangeOwnedV1(T->Client,&T->Initial,&K,&T->WSOwner,&ExchangeWSPolicy,&Expected,&Previous);
    testRequire(T->WS&&Previous.Data==H&&Previous.Policy==&ExchangeHTTPPolicy&&!xrtNetBufSize(&T->Initial)&&!xrtAtomic32Load(&T->HTTPDropped,XMEMORY_ACQUIRE),"successful composite returns old credit without invoking it");T->Published=true;Previous.Policy->Drop(Previous.Data);memset(&Previous,0,sizeof(Previous));T->HTTP=NULL;
    xrtAtomic32Store(&T->Done,1,XMEMORY_RELEASE);
}
static bool exchangeAccept(xnetlistener* L,xnetstream* S,ptr Data)
{(void)L;exchangeTest* T=Data;xtlsstream* Native=NULL;xrtAtomic32FetchAdd(&T->Owner.Refs,1,XMEMORY_RELAXED);testRequire(xrtTlsStreamAcceptOwnedV1(S,&T->Config,NULL,T,&ExchangeNativePolicy,&Native),"exchange TLS accept");xrtAtomicPtrStore(&T->Server,Native,XMEMORY_RELEASE);return true;}
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend Backend=Argc==1||!strcmp(Argv[1],"select")?XNET_PORT_SELECT:!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:XNET_PORT_EPOLL;
    for(unsigned Mode=0;Mode<3;++Mode){
        printf("[case] HTTP->WS owned exchange mode%u\n",Mode);exchangeTest T={0};T.Mode=Mode;exchangeOwnerInit(&T.Owner,2);exchangeOwnerInit(&T.WSOwner,2);exchangeOwnerInit(&T.Alternate,2);xrtAtomicPtrInit(&T.Server,NULL);xrtAtomic32Init(&T.Opened,0);xrtAtomic32Init(&T.Connected,0);xrtAtomic32Init(&T.Bound,0);xrtAtomic32Init(&T.Proceed,0);xrtAtomic32Init(&T.Done,0);xrtAtomic32Init(&T.NeedSecond,0);xrtAtomic32Init(&T.Messages,0);xrtAtomic32Init(&T.HTTPDropped,0);testRequire(xrtNetPostInit(&T.HTTPPost)&&xrtNetPostInit(&T.SecondPost)&&xrtNetBufInit(&T.Initial,NULL),"exchange fixture nodes");
        #if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
        exchangeOwnerInit(&T.AsyncOwner,2);testRequire(xrtNetPostInit(&T.AsyncPost),"async join embedded post");
        #endif
        T.Context=testTlsServerContext();T.Identity=testTlsServerIdentity();xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=testTlsServerAccept;T.Verifier=xrtTlsVerifierCreate(&V);testRequire(T.Context&&T.Identity&&T.Verifier,"exchange TLS fixture");xrtTlsServerConfigInit(&T.Config);T.Config.Context=T.Context;T.Config.Identity=T.Identity;
        xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;E.Backend=Backend;T.Engine=xrtNetEngineCreate(&E);testRequire(T.Engine&&xrtNetEngineStart(T.Engine),"exchange Engine");xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents A={exchangeAccept,NULL,NULL};T.Listener=xrtNetListen(T.Engine,&L,&A,NULL,&T);xnetaddr Address;testRequire(T.Listener&&xrtNetListenerLocal(T.Listener,&Address),"exchange listener");xtlsclientconfig Client;xrtTlsClientConfigInit(&Client);Client.Context=T.Context;Client.Verifier=T.Verifier;Client.ServerName=XRT_STR_LITERAL("exchange.fixture");T.Client=xrtTlsStreamConnectOwnedV1(T.Engine,&Address,1,NULL,&Client,NULL,&T,&ExchangeNativePolicy);testRequire(T.Client!=NULL,"exchange TLS connect");xrtAtomic32Store(&T.Connected,1,XMEMORY_RELEASE);
        exchangeAwait(&T.Bound,1);xtlsstreambindingv1 ForeignExpected={T.HTTP,&ExchangeHTTPPolicy},ForeignPrevious={(ptr)(uintptr_t)17,(const xtlsstreamownershipv1*)(uintptr_t)19};testRequire(!xrtTlsStreamExchangeEventsOwnedV1(T.Client,&ForeignExpected,&T.Alternate,&ExchangeAlternatePolicy,&ForeignPrevious)&&xrtErrorKind(xrtGetError())==XERR_STATE&&ForeignPrevious.Data==(ptr)(uintptr_t)17,"foreign-thread exchange rejected without ownership/output changes");xrtClearError();xrtAtomic32Store(&T.Proceed,1,XMEMORY_RELEASE);
        if(Mode==1){exchangeAwait(&T.NeedSecond,1);testRequire(xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(xrtAtomicPtrLoad(&T.Server,XMEMORY_ACQUIRE))),&T.SecondPost,exchangeSecond,&T),"release second authenticated response fragment");}
        exchangeAwait(&T.Done,1);if(Mode!=2)exchangeAwait(&T.Messages,Mode==1?2:1);xstrview Protocol=xrtWsStreamProtocol(T.WS);testRequire(Protocol.Size==7&&!memcmp(Protocol.Data,"jsonrpc",7),"negotiated protocol survives old context destruction");if(xrtWsStreamState(T.WS)!=XWS_STREAM_CLOSED)(void)xrtWsStreamAbort(T.WS);testRequire(xrtWsStreamWaitRetired(T.WS,5000)==XWAIT_OK,"new WS physical retirement");xrtWsStreamDestroy(T.WS);
        xtlsstream* Server=xrtAtomicPtrLoad(&T.Server,XMEMORY_ACQUIRE);if(xrtTlsStreamState(Server)!=XTLS_STREAM_CLOSED&&xrtTlsStreamState(Server)!=XTLS_STREAM_FAILED)testRequire(xrtTlsStreamAbort(Server),"raw server abort");testRequire(xrtTlsStreamWaitRetired(Server,5000)==XWAIT_OK,"server physical retirement");xrtTlsStreamDestroy(Server);xrtNetBufClear(&T.Initial);testRequire(xrtNetListenerClose(T.Listener),"exchange listener close");xrtNetListenerDestroy(T.Listener);double End=xrtTimer()+5;xnetretireresult R;while((R=xrtNetEngineTryDestroy(T.Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"exchange Engine tail");xrtSleep(1);}testRequire(R==XNET_RETIRE_READY&&xrtAtomic32Load(&T.Owner.Refs,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&T.WSOwner.Refs,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&T.WSOwner.Drops,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&T.Alternate.Refs,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&T.Alternate.Drops,XMEMORY_ACQUIRE)==2&&xrtAtomic32Load(&T.HTTPDropped,XMEMORY_ACQUIRE)==1,"exact original/event/returned owners and full host joins");xrtTlsVerifierRelease(T.Verifier);xrtTlsIdentityRelease(T.Identity);xrtTlsContextRelease(T.Context);
        #if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
        testRequire(!T.AsyncRead&&!T.HTTPReturned.Data&&xrtAtomic32Load(&T.AsyncOwner.Refs,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&T.AsyncOwner.Drops,XMEMORY_ACQUIRE)==1,"pending async read rejected exchange without leaking returned owner");
        #endif
    }
    puts("[PASS] real authenticated HTTP Upgrade->owned WS exchange, nonce/protocol/suffix/Initial, no old Drop before publication, original quota rollback, reentrant Drop/Abort and physical joins");return 0;
}
