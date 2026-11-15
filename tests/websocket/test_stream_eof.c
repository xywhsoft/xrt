#include "../test.h"
#include <xrt/thread.h>
/* Public APIs only. Pausing at the final Data byte must not let native EOF
 * suppress MessageEnd or a second complete frame already in the input chain. */
typedef struct wsEof {
    xnetengine* Engine;xnetlistener* Listener;xnetstream* Raw;xwsstream* WS;
    xatomic32 Begun,Bytes,Ended,Closed,Ready;bool Mask;
} wsEof;
static void wsEofAwait(xatomic32* A,uint32 Value)
{double End=xrtTimer()+5;while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)<Value){testRequire(xrtTimer()<End,"buffered EOF callback missing");xrtSleep(1);}}
static void wsEofBegin(xwsstream* S,const xwsmessageinfo* M,ptr Data)
{wsEof* T=Data;testRequire(M->Opcode==XWS_OPCODE_TEXT,"EOF Text opcode");xrtWsStreamPause(S);xrtAtomic32FetchAdd(&T->Begun,1,XMEMORY_RELEASE);}
static void wsEofData(xwsstream* S,xbytesview B,ptr Data)
{wsEof* T=Data;testRequire(B.Size==2&&(!memcmp(B.Data,"ok",2)||!memcmp(B.Data,"hi",2)),"buffered EOF exact payload");xrtWsStreamPause(S);xrtAtomic32FetchAdd(&T->Bytes,(int32)B.Size,XMEMORY_RELEASE);}
static void wsEofEnd(xwsstream* S,ptr Data)
{wsEof* T=Data;xrtWsStreamPause(S);xrtAtomic32FetchAdd(&T->Ended,1,XMEMORY_RELEASE);}
static void wsEofClose(xwsstream* S,const xwsstreamclose* C,ptr Data)
{(void)S;(void)C;wsEof* T=Data;testRequire(xrtAtomic32Load(&T->Ended,XMEMORY_ACQUIRE)==2,"EOF overtook buffered messages/End");xrtAtomic32Store(&T->Closed,1,XMEMORY_RELEASE);}
static bool wsEofAccept(xnetlistener* L,xnetstream* S,ptr Data)
{(void)L;wsEof* T=Data;xwsstreamconfig C;xrtWsStreamConfigInit(&C);C.Role=T->Mask?XWS_ROLE_SERVER:XWS_ROLE_CLIENT;xwsstreamevents E={.MessageBegin=wsEofBegin,.MessageData=wsEofData,.MessageEnd=wsEofEnd,.Close=wsEofClose};T->WS=xrtWsStreamAttach(S,0,&C,&E,T);testRequire(T->WS!=NULL,"EOF attach");xrtAtomic32Store(&T->Ready,1,XMEMORY_RELEASE);return true;}
static void wsEofOpen(xnetstream* S,ptr Data)
{
    wsEof* T=Data;unsigned char Wire[16];size_t N=0;const char* Texts[]={"ok","hi"};
    for(unsigned M=0;M<2;++M){Wire[N++]=129;Wire[N++]=(unsigned char)(2|(T->Mask?128:0));if(T->Mask){Wire[N++]=1;Wire[N++]=2;Wire[N++]=3;Wire[N++]=4;}Wire[N++]=(unsigned char)(Texts[M][0]^(T->Mask?1:0));Wire[N++]=(unsigned char)(Texts[M][1]^(T->Mask?2:0));}
    testRequire(xrtNetStreamSend(S,Wire,N)==XNET_RESULT_OK&&xrtNetStreamShutdownWrite(S),"buffered frames then TCP EOF");
}
int main(void)
{
    for(unsigned Mask=0;Mask<2;++Mask){
        wsEof T={0};T.Mask=Mask!=0;xrtAtomic32Init(&T.Begun,0);xrtAtomic32Init(&T.Bytes,0);xrtAtomic32Init(&T.Ended,0);xrtAtomic32Init(&T.Closed,0);xrtAtomic32Init(&T.Ready,0);
        xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;T.Engine=xrtNetEngineCreate(&E);testRequire(T.Engine&&xrtNetEngineStart(T.Engine),"EOF Engine");xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents A={wsEofAccept,NULL,NULL};T.Listener=xrtNetListen(T.Engine,&L,&A,NULL,&T);xnetaddr Address;testRequire(T.Listener&&xrtNetListenerLocal(T.Listener,&Address),"EOF listener");xnetstreamevents C={.Open=wsEofOpen};T.Raw=xrtNetStreamConnect(T.Engine,&Address,1,NULL,&C,&T);testRequire(T.Raw!=NULL,"EOF raw connect");wsEofAwait(&T.Ready,1);
        for(int32 M=1;M<=2;++M){wsEofAwait(&T.Begun,M);testRequire(xrtWsStreamResume(T.WS),"EOF resume Begin");wsEofAwait(&T.Bytes,M*2);testRequire(xrtWsStreamResume(T.WS),"EOF resume final Data byte");wsEofAwait(&T.Ended,M);testRequire(xrtWsStreamResume(T.WS),"EOF resume End");}
        wsEofAwait(&T.Closed,1);xrtWsStreamDestroy(T.WS);if(xrtNetStreamState(T.Raw)!=XNET_STREAM_CLOSED)testRequire(xrtNetStreamAbort(T.Raw),"EOF raw abort");xrtNetStreamDestroy(T.Raw);testRequire(xrtNetListenerClose(T.Listener),"EOF listener close");xrtNetListenerDestroy(T.Listener);
        double End=xrtTimer()+5;xnetretireresult R;while((R=xrtNetEngineTryDestroy(T.Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"EOF Engine physical tail");xrtSleep(1);}testRequire(R==XNET_RETIRE_READY,"EOF Engine join");
    }
    puts("[PASS] native WS masked/unmasked buffered frames/Begin+Data+End pause/EOF ordering");return 0;
}
