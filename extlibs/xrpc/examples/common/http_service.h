#ifndef XRPC_EXAMPLE_HTTP_SERVICE_H
#define XRPC_EXAMPLE_HTTP_SERVICE_H
#include <xrpc/http_server.h>
#include <xrpc/http_client.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static inline void rpcServiceRequire(bool OK,cstr Message)
{if(!OK){fprintf(stderr,"[FAIL] HTTP service: %s (%d %s)\n",Message,xrtErrorCode(xrtGetError()),xrtErrorMessage(xrtGetError()));exit(1);}}
typedef struct rpcService {
    xnetengine* Engine;xrpchttpserver* Server;xnetaddr Address;size_t BaseBytes;
    xatomic32 Entered,Permit,Calls,Window;
} rpcService;
static inline bool rpcServiceMethod(const xrpcmessage* R,cstr Name)
{size_t N=strlen(Name);return R->Method.Raw.Size==N+2 && !memcmp(R->Method.Raw.Data+1,Name,N);}
static inline bool rpcServiceCall(const xrpcmessage* R,xrpcreply* Reply,const xrpcservercontext* Context,ptr Data)
{
    rpcService* S=Data;rpcServiceRequire(!xrtNetEngineCurrent(S->Engine),"handler entered network worker");xrtAtomic32FetchAdd(&S->Calls,1,XMEMORY_RELAXED);
    if(rpcServiceMethod(R,"hold")){
        xrtAtomic32FetchAdd(&S->Entered,1,XMEMORY_RELEASE);
        while(!xrtAtomic32Load(&S->Permit,XMEMORY_ACQUIRE) && !xrtCancelRequested(Context->Cancel) && xrtTimer()<Context->Deadline)xrtSleep(1);
    }
    if(rpcServiceMethod(R,"stubborn")){
        xrtAtomic32FetchAdd(&S->Entered,1,XMEMORY_RELEASE);
        while(!xrtAtomic32Load(&S->Permit,XMEMORY_ACQUIRE))xrtSleep(1);
    }
    if(rpcServiceMethod(R,"self")){
        bool OK=xrpcHttpServerWaitFor(S->Server,0)==XWAIT_ERROR && !xrpcHttpServerDestroy(S->Server);
        xrtAtomic32Store(&S->Window,OK?1u:0u,XMEMORY_RELEASE);
    }
    if(rpcServiceMethod(R,"error"))return xrpcReplyError(Reply,XRT_STR_LITERAL("-1"),XRT_STR_LITERAL("failure"),(xstrview){0});
    if(rpcServiceMethod(R,"identity"))return xrpcReplyResult(Reply,(xstrview){(cstr)Context->Data.Data,Context->Data.Size});
    if(rpcServiceMethod(R,"budget"))xrtAtomic32Store(&S->Window,(uint32)((Context->Deadline-xrtTimer())*1000.),XMEMORY_RELEASE);
    return xrpcReplyResult(Reply,R->Params.Raw.Size?R->Params.Raw:XRT_STR_LITERAL("null"));
}
static inline void rpcServiceStart(rpcService* S,const xrpchttpserverconfig* Config)
{
    memset(S,0,sizeof(*S));xrtAtomic32Init(&S->Entered,0);xrtAtomic32Init(&S->Permit,0);xrtAtomic32Init(&S->Calls,0);xrtAtomic32Init(&S->Window,0);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=3;S->Engine=xrtNetEngineCreate(&E);rpcServiceRequire(S->Engine && xrtNetEngineStart(S->Engine),"engine start");
    xrpchttpserverconfig K;if(Config)K=*Config;else xrpcHttpServerConfigInit(&K);K.Execution.Call=rpcServiceCall;K.Execution.User=S;
    rpcServiceRequire(xrpcHttpServerCreate(S->Engine,&K,&S->Server),"server create");xrpchttpserverinfo I={0};I.Size=sizeof(I);
    rpcServiceRequire(xrpcHttpServerInfo(S->Server,&I) && I.State==XRPC_HTTP_SERVER_CREATED && !I.Accepted && !I.Connections,"created before bind");S->BaseBytes=I.BufferBytes;
    rpcServiceRequire(xrpcHttpServerStart(S->Server) && xrpcHttpServerLocal(S->Server,&S->Address),"server start/local");
}
static inline xrpchttpclient* rpcServiceClient(rpcService* S,const xrpchttpconfig* Config)
{xrpchttpclient* C=NULL;rpcServiceRequire(xrpcHttpConnect(S->Engine,&S->Address,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),Config,&C),"client connect");return C;}
static inline xfuture* rpcServiceSubmit(xrpchttpclient* C,cstr Method,cstr Params,int64 Timeout)
{xfuture* F=NULL;rpcServiceRequire(xrpcHttpCallAsync(C,(xstrview){Method,strlen(Method)},(xstrview){Params,Params?strlen(Params):0},Timeout,&F)==XRPC_ADMISSION_ACCEPTED,"client submit");return F;}
static inline const xrpccallresult* rpcServiceResult(xfuture* F)
{xfutureresult R;rpcServiceRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value,"client result wait");return R.Value;}
static inline void rpcServiceClientClose(xrpchttpclient* C)
{xfuture* F=xrpcHttpClosed(C);rpcServiceRequire(F && xrpcHttpClose(C) && xrtFutureWaitFor(F,10000)==XWAIT_OK,"client close");xrpcHttpRelease(C);xrtFutureDestroy(F);}
static inline void rpcServiceStop(rpcService* S)
{
    rpcServiceRequire(xrpcHttpServerClose(S->Server) && xrpcHttpServerWaitFor(S->Server,10000)==XWAIT_OK,"server physical drain");
    xrpchttpserverinfo I={0};I.Size=sizeof(I);rpcServiceRequire(xrpcHttpServerInfo(S->Server,&I) && I.State==XRPC_HTTP_SERVER_CLOSED &&
        !I.Connections && !I.Operations && !I.Authorizations && !I.QueuedBytes && I.BufferBytes==S->BaseBytes && !I.Execution.Operations && !I.Execution.Results && !I.Execution.Auxiliary && I.Execution.AuxiliaryAccepted==I.Execution.AuxiliaryCompleted,"server retired accounting");
    rpcServiceRequire(xrpcHttpServerDestroy(S->Server),"server destroy");S->Server=NULL;
    rpcServiceRequire(xrtNetEngineStop(S->Engine) && xrtNetEngineDestroy(S->Engine),"host engine physical duties leaked");S->Engine=NULL;
}
#endif
