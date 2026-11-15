#define main wsEofPlainMain
#include "test_stream_eof.c"
#undef main
#include "../fixtures/tls_server.h"
typedef struct wsEofTls {wsEof Base;xtlsstream* Client;xtlscontext* Context;xtlsidentity* Identity;xtlsverifier* Verifier;xtlsserverconfig Config;unsigned Mode;xnetpost ClosePost;} wsEofTls;
static void wsEofTlsClosed(xwsstream* S,const xwsstreamclose* C,ptr Data)
{wsEofTls* T=Data;if(!T->Mode)wsEofClose(S,C,Data);else {testRequire(xrtAtomic32Load(&T->Base.Ended,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&T->Base.Bytes,XMEMORY_ACQUIRE)==4,"explicit close/abort after final Data pause");xrtAtomic32Store(&T->Base.Closed,1,XMEMORY_RELEASE);}}
static void wsEofTlsClosePost(xnetworker* W,ptr Data)
{(void)W;wsEofTls* T=Data;testRequire(xrtWsStreamClose(T->Base.WS,XWS_CLOSE_NORMAL,(xstrview){0})==XNET_RESULT_CLOSED,"close abandoned paused TLS input");}
static void wsEofTlsOpen(xtlsstream* S,ptr Data)
{
    wsEofTls* T=Data;wsEof* B=&T->Base;xwsstreamconfig C;xrtWsStreamConfigInit(&C);C.Role=B->Mask?XWS_ROLE_SERVER:XWS_ROLE_CLIENT;xwsstreamevents E={.MessageBegin=wsEofBegin,.MessageData=wsEofData,.MessageEnd=wsEofEnd,.Close=wsEofTlsClosed};B->WS=xrtWsStreamAttachTls(S,0,&C,&E,B);testRequire(B->WS!=NULL,"EOF TLS WS attach");xrtAtomic32Store(&B->Ready,1,XMEMORY_RELEASE);
}
static bool wsEofTlsAccept(xnetlistener* L,xnetstream* S,ptr Data)
{(void)L;wsEofTls* T=Data;xtlsstream* Native=NULL;xtlsstreamevents E={.Open=wsEofTlsOpen};return xrtTlsStreamAccept(S,&T->Config,NULL,&E,T,&Native);}
static void wsEofTlsSend(xtlsstream* S,ptr Data)
{
    wsEofTls* T=Data;unsigned char Wire[16];size_t N=0;const char* Texts[]={"ok","hi"};bool Mask=T->Base.Mask;
    for(unsigned M=0;M<2;++M){Wire[N++]=129;Wire[N++]=(unsigned char)(2|(Mask?128:0));if(Mask){Wire[N++]=1;Wire[N++]=2;Wire[N++]=3;Wire[N++]=4;}Wire[N++]=(unsigned char)(Texts[M][0]^(Mask?1:0));Wire[N++]=(unsigned char)(Texts[M][1]^(Mask?2:0));}
    size_t Written=0;testRequire(xrtTlsStreamSend(S,Wire,N,&Written)==XTLS_OK&&Written==N&&xrtTlsStreamClose(S),"TLS frames followed by authenticated close_notify");
}
int main(int Argc,char** Argv)
{
    xnetportbackend Backend=Argc==1||!strcmp(Argv[1],"XNET_PORT_SELECT")?XNET_PORT_SELECT:!strcmp(Argv[1],"XNET_PORT_IOCP")?XNET_PORT_IOCP:XNET_PORT_EPOLL;
    for(unsigned Mode=0;Mode<3;++Mode)for(unsigned Mask=0;Mask<2;++Mask){
        wsEofTls T={0};T.Mode=Mode;wsEof* B=&T.Base;B->Mask=Mask!=0;xrtAtomic32Init(&B->Begun,0);xrtAtomic32Init(&B->Bytes,0);xrtAtomic32Init(&B->Ended,0);xrtAtomic32Init(&B->Closed,0);xrtAtomic32Init(&B->Ready,0);
        T.Context=testTlsServerContext();T.Identity=testTlsServerIdentity();xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=testTlsServerAccept;T.Verifier=xrtTlsVerifierCreate(&V);testRequire(T.Context&&T.Identity&&T.Verifier,"TLS EOF fixture");xrtTlsServerConfigInit(&T.Config);T.Config.Context=T.Context;T.Config.Identity=T.Identity;
        xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;E.Backend=Backend;B->Engine=xrtNetEngineCreate(&E);testRequire(B->Engine&&xrtNetEngineStart(B->Engine),"TLS EOF Engine");xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents A={wsEofTlsAccept,NULL,NULL};B->Listener=xrtNetListen(B->Engine,&L,&A,NULL,&T);xnetaddr Address;testRequire(B->Listener&&xrtNetListenerLocal(B->Listener,&Address),"TLS EOF listener");xtlsclientconfig C;xrtTlsClientConfigInit(&C);C.Context=T.Context;C.Verifier=T.Verifier;C.ServerName=XRT_STR_LITERAL("fixture");xtlsstreamevents Native={.Open=wsEofTlsSend};T.Client=xrtTlsStreamConnect(B->Engine,&Address,1,NULL,&C,NULL,&Native,&T);testRequire(T.Client!=NULL,"TLS EOF connect");wsEofAwait(&B->Ready,1);
        for(uint32 M=1;M<=2;++M){
            wsEofAwait(&B->Begun,M);testRequire(xrtWsStreamResume(B->WS),"TLS resume Begin");wsEofAwait(&B->Bytes,M*2);
            if(M==2&&Mode){xtlsstream* NativeAlias=xrtWsStreamTlsRef(B->WS);testRequire(NativeAlias!=NULL,"retained already-closed native TLS");double End=xrtTimer()+5;while(xrtTlsStreamState(NativeAlias)!=XTLS_STREAM_CLOSED){testRequire(xrtTimer()<End,"native TLS close while WS remains paused");xrtSleep(1);}if(Mode==1)testRequire(xrtWsStreamAbort(B->WS),"cross-thread abort deferred native close");else testRequire(xrtNetPostInit(&T.ClosePost)&&xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(NativeAlias)),&T.ClosePost,wsEofTlsClosePost,&T),"post close abandoned paused TLS input");xrtTlsStreamDestroy(NativeAlias);break;}
            testRequire(xrtWsStreamResume(B->WS),"TLS resume final Data byte");wsEofAwait(&B->Ended,M);testRequire(xrtWsStreamResume(B->WS),"TLS resume End");
        }
        wsEofAwait(&B->Closed,1);xrtWsStreamDestroy(B->WS);xrtTlsStreamDestroy(T.Client);testRequire(xrtNetListenerClose(B->Listener),"TLS EOF listener close");xrtNetListenerDestroy(B->Listener);double End=xrtTimer()+5;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(B->Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"TLS EOF Engine physical tail");xrtSleep(1);}testRequire(Q==XNET_RETIRE_READY,"TLS EOF Engine join");xrtTlsVerifierRelease(T.Verifier);xrtTlsIdentityRelease(T.Identity);xrtTlsContextRelease(T.Context);
    }
    puts("[PASS] authenticated TLS input End after paused buffered WS frames/Begin+Data+End, both roles/Resume/Close/Abort");return 0;
}
