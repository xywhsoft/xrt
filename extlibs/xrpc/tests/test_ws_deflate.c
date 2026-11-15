#include "test.h"
#include <xrpc/ws_deflate.h>
#include "../examples/common/ws_http_service.h"
static void wsDeflateExact(xfuture* F,xstrview Expected)
{
    const xrpccallresult* R=rpcWsHttpServiceResult(F);xrpcresponseinfo I={.Size=sizeof(I)};
    testRequire(R->Status==XRPC_CALL_OK&&R->Delivery==XRPC_REPLIED&&xrpcResponsesInfo(R->Responses,&I)&&R->Index<I.Count&&I.Messages[R->Index].Result.Raw.Size==Expected.Size&&!memcmp(I.Messages[R->Index].Result.Raw.Data,Expected.Data,Expected.Size),"exact compressed RPC response");
}
static bool wsDeflatePolicy(const xwsdeflate* Offer,xwsdeflate* Response,ptr Data)
{
    xatomic32* Calls=Data;xrtAtomic32FetchAdd(Calls,1,XMEMORY_RELAXED);
    if(!xrtWsDeflateAccept(Offer,Response))return false;
    if(Offer->Flags&XWS_DEFLATE_CLIENT_NO_CONTEXT)Response->Flags|=XWS_DEFLATE_CLIENT_NO_CONTEXT;
    return true;
}
static void wsDeflateDefault(bool TLS,xnetportbackend Backend)
{
    rpcWsHttpService S;if(Backend==XNET_PORT_SELECT)rpcWsHttpServiceStart(&S,TLS,NULL,false);else rpcWsHttpServiceStartEx(&S,TLS,NULL,false,0,NULL,NULL,Backend);
    wsDeflateExact(S.Initial,XRT_STR_LITERAL("[7]"));xrpcwsdeflateinfo I={.Size=sizeof(I)};
    testRequire(xrpcWsDeflateInfo(S.Client,&I)&&!I.Enabled&&!I.CompressOutput&&I.CompressMinBytes==256,"default does not negotiate or compress");
    rpcWsHttpServiceStop(&S);xrtFutureDestroy(S.Initial);
}
typedef struct wsDeflateHold {rpcWsHttpService* Scene;xatomic32 Entered,Permit;} wsDeflateHold;
static xrpcwshttpauthstatus wsDeflateHoldAuth(const xrpcwshttpauthrequest* R,bytes Context,size_t Cap,size_t* Used,ptr Data)
{
    (void)Context;(void)Cap;wsDeflateHold* H=Data;testRequire(!xrtNetEngineCurrent(H->Scene->Engine)&&R->Head&&R->Cancel,"held auth stays on shared executor");xrtAtomic32Store(&H->Entered,1,XMEMORY_RELEASE);
    rpcWsHttpServiceAwait(&H->Permit);*Used=0;return XRPC_WS_HTTP_ALLOW;
}
static void wsDeflatePending(bool TLS,xnetportbackend Backend,bool Close)
{
    rpcWsHttpService S;wsDeflateHold H={.Scene=&S};xrtAtomic32Init(&H.Entered,0);xrtAtomic32Init(&H.Permit,0);
    xrpcwshttpconfig C,A;xrpcWsHttpConfigInit(&C);xrpcWsHttpConfigInit(&A);C.Host=A.Host=XRT_STR_LITERAL("rpc.fixture");C.Client.EnableDeflate=A.Server.EnableDeflate=true;A.Authorize=wsDeflateHoldAuth;A.AuthorizationUser=&H;
    rpcWsHttpServiceStartEx(&S,TLS,NULL,false,0,&C,&A,Backend);rpcWsHttpServiceAwait(&H.Entered);
    xrpcwsdeflateinfo I;memset(&I,0x5a,sizeof(I));I.Size=sizeof(I);unsigned char Before[sizeof(I)];memcpy(Before,&I,sizeof(I));
    testRequire(!xrpcWsDeflateInfo(S.Client,&I)&&xrtErrorKind(xrtGetError())==XERR_STATE&&!memcmp(&I,Before,sizeof(I)),"pending negotiation preserves the complete info output");xrtClearError();
    if(Close)testRequire(xrpcWsClose(S.Accepted)&&xrpcWsWaitFor(S.Accepted,20)==XWAIT_TIMEOUT,"physical held auth survives close");
    xrtAtomic32Store(&H.Permit,1,XMEMORY_RELEASE);
    if(!Close){double End=xrtTimer()+10;while(!xrpcWsDeflateInfo(S.Client,&I)){testRequire(xrtErrorKind(xrtGetError())==XERR_STATE&&!memcmp(&I,Before,sizeof(I))&&xrtTimer()<End,"concurrent snapshot across actual handoff");xrtClearError();xrtSleep(1);}testRequire(I.Enabled&&!I.CompressOutput,"published negotiated facts with output default off");wsDeflateExact(S.Initial,XRT_STR_LITERAL("[7]"));}
    else{const xrpccallresult* R=rpcWsHttpServiceResult(S.Initial);testRequire(R->Status==XRPC_CALL_TRANSPORT&&R->Delivery==XRPC_NOT_SENT,"closed pending handshake never starts queued RPC");}
    rpcWsHttpServiceStop(&S);xrtFutureDestroy(S.Initial);
}
static void wsDeflateRun(bool TLS,xnetportbackend Backend,bool Context,bool Accept)
{
    xatomic32 Policies;xrtAtomic32Init(&Policies,0);xrpcwshttpconfig Client,Server;
    xrpcWsHttpConfigInit(&Client);xrpcWsHttpConfigInit(&Server);Client.Host=Server.Host=XRT_STR_LITERAL("rpc.fixture");
    Client.Client.EnableDeflate=true;Client.Client.RequireDeflate=Accept;
    if(!Context)Client.Client.Deflate.Flags=XWS_DEFLATE_SERVER_NO_CONTEXT|XWS_DEFLATE_CLIENT_NO_CONTEXT;
    if(Accept){Server.Server.EnableDeflate=true;Server.Server.RequireDeflate=true;Server.Server.AcceptDeflate=wsDeflatePolicy;Server.Server.DeflateData=&Policies;}
    xrpcwsconfig C;xrpcWsConfigInit(&C);testRequire(!C.CompressOutput&&C.CompressMinBytes==256,"compression is explicit and defaults off");C.CompressOutput=true;C.CompressMinBytes=0;
    if(Accept)C.Stream.SendLimit=2048;
    rpcWsHttpService S;rpcWsHttpServiceStartEx(&S,TLS,&C,false,0,&Client,&Server,Backend);wsDeflateExact(S.Initial,XRT_STR_LITERAL("[7]"));
    for(unsigned K=0;K<2;++K){xrpcwsdeflateinfo I={.Size=sizeof(I)};testRequire(xrpcWsDeflateInfo(K?S.Accepted:S.Client,&I)&&I.Enabled==Accept&&I.CompressOutput&&I.CompressMinBytes==0,"actual immutable compression facts");if(Accept&&!Context)testRequire((I.Negotiated.Flags&XWS_DEFLATE_SERVER_NO_CONTEXT)!=0,"negotiated server no-context direction");}
    char Text[8196];Text[0]='[';Text[1]='"';memset(Text+2,'a',8192);Text[8194]='"';Text[8195]=']';xstrview Params={Text,sizeof(Text)};
    for(unsigned N=0;N<8;++N){xfuture *A=NULL,*B=NULL;testRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("echo"),Params,5000,&A)==XRPC_DUPLEX_ACCEPTED&&xrpcWsCall(S.Accepted,XRT_STR_LITERAL("echo"),Params,5000,&B)==XRPC_DUPLEX_ACCEPTED,"bidirectional repeated compression/context");wsDeflateExact(A,Params);wsDeflateExact(B,Params);xrtFutureDestroy(A);xrtFutureDestroy(B);}
    rpcWsHttpServiceStop(&S);wsDeflateExact(S.Initial,XRT_STR_LITERAL("[7]"));xrtFutureDestroy(S.Initial);testRequire(xrtAtomic32Load(&Policies,XMEMORY_ACQUIRE)==(Accept?1u:0u),"one bounded negotiation policy call");
}
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend B=Argc==1||!strcmp(Argv[1],"select")?XNET_PORT_SELECT:!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:XNET_PORT_EPOLL;
    for(unsigned TLS=0;TLS<
        #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
        2
        #else
        1
        #endif
        ;++TLS){wsDeflateDefault(TLS!=0,B);wsDeflatePending(TLS!=0,B,false);wsDeflatePending(TLS!=0,B,true);for(unsigned Context=0;Context<2;++Context)for(unsigned Accept=0;Accept<2;++Accept){printf("[case] compression TLS%u context%u accepted%u\n",TLS,Context,Accept);wsDeflateRun(TLS!=0,B,Context!=0,Accept!=0);}}
    puts("[PASS] actual negotiated RPC compression, optional decline, repeated bidirectional contexts and physical joins");return 0;
}
