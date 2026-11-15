#include "../fixtures/tls_server.h"
#include <xrt/websocket_stream.h>
#include <xrt/thread.h>
typedef struct reentrantTest {
    xnetengine* Engine;xnetlistener* Listener;xatomicptr Client,Server;
    xtlscontext* Context;xtlsidentity* Identity;xtlsverifier* Verifier;xtlsserverconfig Config;
    xwsstream* WS;xatomic32 Opened,Attached,Sent,Messages,Owners;
    xnetpost Attach,Send,Resume;unsigned Used;char Payload[4];bool NativeParser;
} reentrantTest;
static void reentrantAwait(reentrantTest* T,xatomic32* A,uint32 N,cstr Name)
{
    double End=xrtTimer()+10;
    while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)<N){
        if(xrtTimer()>=End){
            fprintf(stderr,"[barrier] %s expected%u actual%u opened%u attached%u sent%u messages%u\n",Name,N,xrtAtomic32Load(A,XMEMORY_ACQUIRE),xrtAtomic32Load(&T->Opened,XMEMORY_ACQUIRE),xrtAtomic32Load(&T->Attached,XMEMORY_ACQUIRE),xrtAtomic32Load(&T->Sent,XMEMORY_ACQUIRE),xrtAtomic32Load(&T->Messages,XMEMORY_ACQUIRE));
            #if defined(REENTRANT_TEST_DIAG)
            REENTRANT_TEST_DIAG(T);
            #endif
            testRequire(false,"reentrant public fixture barrier");
        }
        xrtSleep(1);
    }
}
static bool reentrantCount(const void* Data,size_t* N)
{*N=xrtAtomic32Load(&((const reentrantTest*)Data)->Owners,XMEMORY_ACQUIRE);return true;}
static bool reentrantTrace(const void* Data,xrtownershipvisitor V,ptr User)
{(void)Data;(void)V;(void)User;return false;}
static void reentrantDrop(const void* Data)
{xrtAtomic32FetchSub(&((reentrantTest*)Data)->Owners,1,XMEMORY_RELEASE);}
static const xrtownershipops ReentrantOps={reentrantCount,reentrantTrace};
static void reentrantResume(xnetworker* W,ptr Data)
{(void)W;reentrantTest* T=Data;testRequire(xrtWsStreamResume(T->WS),"one explicit application Resume per completed message");}
static void reentrantBegin(xwsstream* S,const xwsmessageinfo* M,ptr Data)
{(void)S;reentrantTest* T=Data;testRequire(M->Opcode==XWS_OPCODE_TEXT,"ordered Text input");T->Used=0;}
static void reentrantData(xwsstream* S,xbytesview B,ptr Data)
{(void)S;reentrantTest* T=Data;testRequire(B.Size<=sizeof(T->Payload)-T->Used,"bounded reentrant payload");memcpy(T->Payload+T->Used,B.Data,B.Size);T->Used+=(unsigned)B.Size;}
static void reentrantEnd(xwsstream* S,ptr Data)
{
    reentrantTest* T=Data;char Expected[8];unsigned I=xrtAtomic32Load(&T->Messages,XMEMORY_ACQUIRE);int N=snprintf(Expected,sizeof(Expected),"%04u",I);
    testRequire(N==4&&T->Used==4&&!memcmp(T->Payload,Expected,4),"every record-split message delivered once in order");xrtWsStreamPause(S);
    if(I+1<32)testRequire(xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(xrtAtomicPtrLoad(&T->Server,XMEMORY_ACQUIRE))),&T->Resume,reentrantResume,T),"post application Resume outside native reader");
    xrtAtomic32FetchAdd(&T->Messages,1,XMEMORY_RELEASE);
}
static const xwsstreamownershipv1 ReentrantWS={sizeof(xwsstreamownershipv1),{.MessageBegin=reentrantBegin,.MessageData=reentrantData,.MessageEnd=reentrantEnd},reentrantDrop,&ReentrantOps,NULL};
static void reentrantAttach(xnetworker* W,ptr Data)
{
    (void)W;reentrantTest* T=Data;xwsstreamconfig C;xrtWsStreamConfigInit(&C);C.Role=XWS_ROLE_SERVER;xrtAtomic32FetchAdd(&T->Owners,1,XMEMORY_RELAXED);
    T->WS=xrtWsStreamAttachTlsOwnedV1(xrtAtomicPtrLoad(&T->Server,XMEMORY_ACQUIRE),NULL,&C,T,&ReentrantWS);testRequire(T->WS!=NULL,"public owned native WS attach");xrtAtomic32Store(&T->Attached,1,XMEMORY_RELEASE);
}
static void reentrantOpen(xtlsstream* S,ptr Data)
{
    reentrantTest* T=Data;bool Server=xrtTlsSessionRole(xrtTlsStreamSession(S))==XTLS_SERVER;
    xrtAtomicPtrStore(Server?&T->Server:&T->Client,S,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&T->Opened,1,XMEMORY_RELEASE);
    if(Server){if(T->NativeParser)xrtAtomic32Store(&T->Attached,1,XMEMORY_RELEASE);else testRequire(xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(S)),&T->Attach,reentrantAttach,T),"defer attach after TLS Open");}
}
static const xtlsstreamownershipv1 ReentrantTLS={sizeof(xtlsstreamownershipv1),{.Open=reentrantOpen},reentrantDrop,&ReentrantOps};
/* A separate public TLS consumer tests ReadMore directly, without the WS drive
 * node that could otherwise hide a parked native prefix. */
static void reentrantNativeRead(xtlsstream* S,const xnetbuf* B,ptr Data)
{
    (void)B;reentrantTest* T=Data;
    for(;;){
        unsigned char Wire[10];size_t N=xrtNetBufPeek(xrtTlsStreamBuffer(S),0,Wire,sizeof(Wire));
        if(!N)return;
        if(N<sizeof(Wire)){testRequire(xrtTlsStreamReadMore(S),"native prefix requests growth inside Read");return;}
        unsigned I=xrtAtomic32Load(&T->Messages,XMEMORY_ACQUIRE);char Expected[8];testRequire(snprintf(Expected,sizeof(Expected),"%04u",I)==4,"native expected text");
        testRequire(Wire[0]==129&&Wire[1]==132&&!memcmp(Wire+2,"\1\2\3\4",4),"native exact masked frame prefix");
        for(unsigned K=0;K<4;++K)testRequire((Wire[6+K]^Wire[2+K])==(unsigned char)Expected[K],"native exact payload and order");
        testRequire(xrtTlsStreamConsume(S,sizeof(Wire)),"native exact consume");xrtAtomic32FetchAdd(&T->Messages,1,XMEMORY_RELEASE);
    }
}
static const xtlsstreamownershipv1 ReentrantNativeTLS={sizeof(xtlsstreamownershipv1),{.Open=reentrantOpen,.Read=reentrantNativeRead},reentrantDrop,&ReentrantOps};
static bool reentrantAccept(xnetlistener* L,xnetstream* S,ptr Data)
{(void)L;reentrantTest* T=Data;xtlsstream* Native=NULL;xrtAtomic32FetchAdd(&T->Owners,1,XMEMORY_RELAXED);testRequire(xrtTlsStreamAcceptOwnedV1(S,&T->Config,NULL,T,T->NativeParser?&ReentrantNativeTLS:&ReentrantTLS,&Native),"public TLS accept");return true;}
static void reentrantSend(xnetworker* W,ptr Data)
{
    (void)W;reentrantTest* T=Data;xtlsstream* Client=xrtAtomicPtrLoad(&T->Client,XMEMORY_ACQUIRE);static const unsigned char Mask[4]={1,2,3,4};
    for(unsigned I=0;I<32;++I){char Text[8];testRequire(snprintf(Text,sizeof(Text),"%04u",I)==4,"fixture text");unsigned char Wire[10]={129,132,1,2,3,4};for(unsigned K=0;K<4;++K)Wire[6+K]=(unsigned char)Text[K]^Mask[K];
        for(size_t K=0;K<sizeof(Wire);++K){size_t Written=0;testRequire(xrtTlsStreamSend(Client,Wire+K,1,&Written)==XTLS_OK&&Written==1,"each frame byte in its own authenticated TLS record");}}
    xrtAtomic32Store(&T->Sent,1,XMEMORY_RELEASE);
}
static void reentrantRun(int Argc,char** Argv,bool NativeParser)
{
    reentrantTest T={0};T.NativeParser=NativeParser;xrtAtomicPtrInit(&T.Client,NULL);xrtAtomicPtrInit(&T.Server,NULL);xrtAtomic32Init(&T.Opened,0);xrtAtomic32Init(&T.Attached,0);xrtAtomic32Init(&T.Sent,0);xrtAtomic32Init(&T.Messages,0);xrtAtomic32Init(&T.Owners,2);testRequire(xrtNetPostInit(&T.Attach)&&xrtNetPostInit(&T.Send)&&xrtNetPostInit(&T.Resume),"public fixture nodes");
    T.Context=testTlsServerContext();T.Identity=testTlsServerIdentity();xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=testTlsServerAccept;T.Verifier=xrtTlsVerifierCreate(&V);testRequire(T.Context&&T.Identity&&T.Verifier,"public TLS fixture");xrtTlsServerConfigInit(&T.Config);T.Config.Context=T.Context;T.Config.Identity=T.Identity;
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;E.Backend=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;T.Engine=xrtNetEngineCreate(&E);testRequire(T.Engine&&xrtNetEngineStart(T.Engine),"fixture Engine");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents A={reentrantAccept,NULL,NULL};T.Listener=xrtNetListen(T.Engine,&L,&A,NULL,&T);xnetaddr Address;testRequire(T.Listener&&xrtNetListenerLocal(T.Listener,&Address),"fixture listener");xtlsclientconfig C;xrtTlsClientConfigInit(&C);C.Context=T.Context;C.Verifier=T.Verifier;C.ServerName=XRT_STR_LITERAL("reentrant.fixture");testRequire(xrtTlsStreamConnectOwnedV1(T.Engine,&Address,1,NULL,&C,NULL,&T,&ReentrantTLS)!=NULL,"public TLS client");reentrantAwait(&T,&T.Opened,2,"TLS Open");reentrantAwait(&T,&T.Attached,1,"WS attach");
    testRequire(xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(xrtAtomicPtrLoad(&T.Client,XMEMORY_ACQUIRE))),&T.Send,reentrantSend,&T),"post320 authenticated records");reentrantAwait(&T,&T.Sent,1,"TLS sends");reentrantAwait(&T,&T.Messages,32,"WS delivery");
    if(NativeParser){xtlsstream* Server=xrtAtomicPtrLoad(&T.Server,XMEMORY_ACQUIRE);(void)xrtTlsStreamAbort(Server);testRequire(xrtTlsStreamWaitRetired(Server,5000)==XWAIT_OK,"native server physical tail");xrtTlsStreamDestroy(Server);}
    else{(void)xrtWsStreamAbort(T.WS);testRequire(xrtWsStreamWaitRetired(T.WS,5000)==XWAIT_OK,"WS physical tail");xrtWsStreamDestroy(T.WS);}
    xtlsstream* Client=xrtAtomicPtrLoad(&T.Client,XMEMORY_ACQUIRE);(void)xrtTlsStreamAbort(Client);testRequire(xrtTlsStreamWaitRetired(Client,5000)==XWAIT_OK,"client physical tail");xrtTlsStreamDestroy(Client);testRequire(xrtNetListenerClose(T.Listener),"listener close");xrtNetListenerDestroy(T.Listener);double End=xrtTimer()+5;xnetretireresult R;while((R=xrtNetEngineTryDestroy(T.Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"Engine join");xrtSleep(1);}testRequire(R==XNET_RETIRE_READY&&xrtAtomic32Load(&T.Owners,XMEMORY_ACQUIRE)==1,"actual native/WS callback credits and host joins");xrtTlsVerifierRelease(T.Verifier);xrtTlsIdentityRelease(T.Identity);xrtTlsContextRelease(T.Context);
    puts(NativeParser?"[PASS] public native TLS ReadMore,32 exact record-split frames, original owners and physical joins":"[PASS] public TLS record-per-byte WS delivery,32 exact messages with Pause/posted Resume, no peer polling/nudge, original owners and physical joins");
}
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);
    if(Argc<=2||strcmp(Argv[2],"ws"))reentrantRun(Argc,Argv,true);
    if(Argc<=2||strcmp(Argv[2],"native"))reentrantRun(Argc,Argv,false);
    return 0;
}
