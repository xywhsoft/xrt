#include "test.h"
#include <xrpc/ws_http.h>
#include "../examples/common/ws_http_service.h"
static void wsHttpResult(xfuture* F,xrpccallstatus Status,xrpcdelivery Delivery)
{const xrpccallresult* R=rpcWsHttpServiceResult(F);if(R->Status!=Status||R->Delivery!=Delivery)fprintf(stderr,"HTTP got %d/%d expected %d/%d\n",R->Status,R->Delivery,Status,Delivery);testRequire(R->Status==Status&&R->Delivery==Delivery,"HTTP queued typed result/delivery");}
static void wsHttpExact(xfuture* F,cstr Text)
{const xrpccallresult* R=rpcWsHttpServiceResult(F);xrpcresponseinfo I={0};I.Size=sizeof(I);testRequire(R->Status==XRPC_CALL_OK&&xrpcResponsesInfo(R->Responses,&I)&&R->Index<I.Count&&I.Messages[R->Index].Result.Raw.Size==strlen(Text)&&!memcmp(I.Messages[R->Index].Result.Raw.Data,Text,strlen(Text)),"HTTP exact RPC response");}
static void wsHttpOpen(rpcWsHttpService* S)
{xrpcwshttpinfo I={.Size=sizeof(I)};double End=xrtTimer()+10;do{testRequire(xrpcWsHttpInfo(S->Client,&I)&&xrtTimer()<End,"HTTP Upgrade result");if(I.Outcome==XRPC_WS_HTTP_PENDING)xrtSleep(1);}while(I.Outcome==XRPC_WS_HTTP_PENDING);testRequire(I.Outcome==XRPC_WS_HTTP_OPEN&&I.Status==101&&I.HeaderBytes&&I.WriteBytes,"HTTP checked101/status/bytes");}
static void wsHttpBasic(bool TLS,xnetportbackend Backend)
{
    rpcWsHttpService S;if(Backend==XNET_PORT_SELECT)rpcWsHttpServiceStart(&S,TLS,NULL,false);else rpcWsHttpServiceStartEx(&S,TLS,NULL,false,0,NULL,NULL,Backend);
    wsHttpExact(S.Initial,"[7]");xfuture* F[4];testRequire(xrpcWsCall(S.Accepted,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[8]"),5000,F)==XRPC_DUPLEX_ACCEPTED,"HTTP server initiated call");wsHttpExact(F[0],"[8]");
    testRequire(xrpcWsNotify(S.Client,XRT_STR_LITERAL("note"),(xstrview){0},5000,F+1)==XRPC_DUPLEX_ACCEPTED,"HTTP notification");wsHttpResult(F[1],XRPC_CALL_NOTIFY_DONE,XRPC_MAY_HAVE_EXECUTED);
    xrpcduplexitem Items[]={{false,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")},{true,XRT_STR_LITERAL("note"),{0}}};testRequire(xrpcWsBatch(S.Client,Items,2,5000,F+2)==XRPC_DUPLEX_ACCEPTED,"HTTP queued batch");wsHttpResult(F[2],XRPC_CALL_BATCH_DONE,XRPC_REPLIED);wsHttpOpen(&S);
    xrpcwsinfo Info={.Size=sizeof(Info)};testRequire(xrpcWsInfo(S.Client,&Info)&&Info.State==XRPC_WS_OPEN&&Info.Encrypted==TLS&&Info.ProtocolBytes==7&&!memcmp(Info.Protocol,"jsonrpc",7),"same handle native role/security/protocol");
    rpcWsHttpServiceStop(&S);wsHttpExact(S.Initial,"[7]");xrtFutureDestroy(S.Initial);for(unsigned I=0;I<3;++I)xrtFutureDestroy(F[I]);
}
typedef struct wsHttpAuth {rpcWsHttpService* Scene;xatomic32 Entered,Permit,Calls;bool Deny;} wsHttpAuth;
static xrpcwshttpauthstatus wsHttpAuthorize(const xrpcwshttpauthrequest* R,bytes Context,size_t Capacity,size_t* Used,ptr Data)
{
    wsHttpAuth* A=Data;testRequire(!xrtNetEngineCurrent(A->Scene->Engine)&&R->Head&&R->Deadline>0&&R->Cancel&&R->Head->Target.Size==4&&!memcmp(R->Head->Target.Data,"/rpc",4),"authorization runs on shared executor with owned head/original deadline");
    xrtAtomic32FetchAdd(&A->Calls,1,XMEMORY_RELAXED);xrtAtomic32Store(&A->Entered,1,XMEMORY_RELEASE);rpcWsHttpServiceAwait(&A->Permit);
    testRequire(R->Head->Target.Size==4&&!memcmp(R->Head->Target.Data,"/rpc",4),"immutable authorization snapshot survives native consume");
    if(A->Deny)return XRPC_WS_HTTP_UNAUTHORIZED;
    testRequire(Capacity>=4,"authorization context bound");memcpy(Context,"[42]",4);*Used=4;return XRPC_WS_HTTP_ALLOW;
}
static void wsHttpHeld(bool TLS,xnetportbackend Backend,unsigned Mode)
{
    rpcWsHttpService S;wsHttpAuth A={.Scene=&S,.Deny=Mode==1};xrtAtomic32Init(&A.Entered,0);xrtAtomic32Init(&A.Permit,0);xrtAtomic32Init(&A.Calls,0);
    xrpcwshttpconfig Client,Server;xrpcWsHttpConfigInit(&Client);Client.Host=XRT_STR_LITERAL("rpc.fixture");xrpcWsHttpConfigInit(&Server);Server.Host=Client.Host;Server.Authorize=wsHttpAuthorize;Server.AuthorizationUser=&A;Server.MaxAuthorizationContextBytes=32;
    if(Mode==2)Client.SetupTimeout=500;
    rpcWsHttpServiceStartEx(&S,TLS,NULL,false,0,&Client,&Server,Backend);rpcWsHttpServiceAwait(&A.Entered);
    xrpcwsinfo I={.Size=sizeof(I)};testRequire(xrpcWsInfo(S.Client,&I)&&I.State==XRPC_WS_UPGRADING&&!I.WriteMessages&&!I.ReadMessages&&!xrtAtomic32Load(&S.Calls,XMEMORY_ACQUIRE),"no RPC bytes or business before authorization/101");
    xfuture *Timeout=NULL,*Cancel=NULL;testRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("echo"),(xstrview){0},0,&Timeout)==XRPC_DUPLEX_ACCEPTED&&xrpcWsCall(S.Client,XRT_STR_LITERAL("echo"),(xstrview){0},5000,&Cancel)==XRPC_DUPLEX_ACCEPTED&&xrtFutureCancel(Cancel),"calls retain original deadline and cancel during HTTP setup");wsHttpResult(Timeout,XRPC_CALL_TIMEOUT,XRPC_NOT_SENT);wsHttpResult(Cancel,XRPC_CALL_CANCELLED,XRPC_NOT_SENT);
    if(Mode==2){xrpcwshttpinfo H={.Size=sizeof(H)};testRequire(xrpcWsHttpInfo(S.Client,&H)&&xrtFutureWaitFor(S.Initial,1500)==XWAIT_OK&&xrtTimer()<H.Deadline+1,"whole setup timeout completes before independent 5s call deadline");wsHttpResult(S.Initial,XRPC_CALL_TIMEOUT,XRPC_NOT_SENT);testRequire(xrpcWsClose(S.Accepted)&&xrpcWsWaitFor(S.Accepted,20)==XWAIT_TIMEOUT,"close joins noncooperative authorization physical tail");}
    xrtAtomic32Store(&A.Permit,1,XMEMORY_RELEASE);
    if(!Mode){wsHttpExact(S.Initial,"[7]");xfuture* Who=NULL;testRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("who"),(xstrview){0},5000,&Who)==XRPC_DUPLEX_ACCEPTED,"authorized request context");wsHttpExact(Who,"[42]");xrtFutureDestroy(Who);wsHttpOpen(&S);}
    else if(Mode==1){wsHttpResult(S.Initial,XRPC_CALL_HTTP_ERROR,XRPC_NOT_SENT);xrpcwshttpinfo H={.Size=sizeof(H)};testRequire(xrpcWsHttpInfo(S.Client,&H)&&H.Outcome==XRPC_WS_HTTP_REJECTED&&H.Status==401&&!xrtAtomic32Load(&S.Calls,XMEMORY_ACQUIRE),"authorization denial HTTP401 without business or replay");}
    rpcWsHttpServiceStop(&S);testRequire(xrtAtomic32Load(&A.Calls,XMEMORY_ACQUIRE)==1,"one authorization execution");xrtFutureDestroy(S.Initial);xrtFutureDestroy(Timeout);xrtFutureDestroy(Cancel);
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
        ;++TLS){printf("[case] WS HTTP basic TLS%u\n",TLS);wsHttpBasic(TLS!=0,B);for(unsigned Mode=0;Mode<3;++Mode){printf("[case] WS HTTP auth TLS%u mode%u\n",TLS,Mode);wsHttpHeld(TLS!=0,B,Mode);}}
    puts("[PASS] same-handle HTTP Upgrade, queued bidirectional RPC/batch/notify, shared executor authorization, immutable request/context, original call/setup deadlines, cancellation, HTTP401 and physical auth/native/host joins");return 0;
}
