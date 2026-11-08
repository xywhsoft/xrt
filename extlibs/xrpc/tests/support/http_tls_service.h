#ifndef XRPC_TEST_HTTP_TLS_SERVICE_H
#define XRPC_TEST_HTTP_TLS_SERVICE_H
#include "../../examples/common/http_service.h"
#include <xrpc/http_server_tls.h>
#include <xrpc/http_tls.h>
#include "../../../../tests/fixtures/tls_server.h"
/* Historical public fixture identity and explicit local-only trust. Production
 * configuration uses real identities/trust, as the independent TLS peers do. */
typedef struct rpcTlsService {
    rpcService Base;xtlscontext* Context;xtlsidentity* Identity;xtlsverifier* Verifier;
    xatomic32 Verified,Errors,LastKind,Reject,CloseOnError;bool OwnContext,OwnIdentity;
} rpcTlsService;
static inline xtlsverifydecision rpcTlsServiceVerify(const xtlspeer* P,ptr Data)
{
    rpcTlsService* T=Data;rpcServiceRequire(P && P->Role==XTLS_SERVER && P->CertificateCount && P->Name.Size==11 && !memcmp(P->Name.Data,"rpc.fixture",11),"TLS service peer identity");
    xrtAtomic32FetchAdd(&T->Verified,1,XMEMORY_RELAXED);return xrtAtomic32Load(&T->Reject,XMEMORY_ACQUIRE)?XTLS_VERIFY_REJECT:XTLS_VERIFY_ACCEPT;
}
static inline void rpcTlsServiceError(xnetresult Result,const xerror* Cause,ptr Data)
{
    (void)Result;rpcTlsService* T=Data;rpcServiceRequire(xrtNetEngineCurrent(T->Base.Engine),"handshake diagnostic worker");
    xrpchttpservertlsinfo I={0};I.Size=sizeof(I);rpcServiceRequire(xrpcHttpServerTlsInfo(T->Base.Server,&I),"reentrant TLS info");
    xrtAtomic32Store(&T->LastKind,(uint32)xrtErrorKind(Cause),XMEMORY_RELEASE);xrtAtomic32FetchAdd(&T->Errors,1,XMEMORY_RELEASE);
    if(xrtAtomic32Load(&T->CloseOnError,XMEMORY_ACQUIRE))rpcServiceRequire(xrpcHttpServerClose(T->Base.Server) && xrpcHttpServerWaitFor(T->Base.Server,0)==XWAIT_ERROR && !xrpcHttpServerDestroy(T->Base.Server),"handshake error reentrant close/self-wait guard");
}
static inline void rpcTlsServiceCreate(rpcTlsService* T,const xrpchttpserverconfig* HTTP,const xrpchttpservertlsconfig* TLS,bool Secure)
{
    memset(T,0,sizeof(*T));xrtAtomic32Init(&T->Verified,0);xrtAtomic32Init(&T->Errors,0);xrtAtomic32Init(&T->LastKind,0);xrtAtomic32Init(&T->Reject,0);xrtAtomic32Init(&T->CloseOnError,0);
    rpcService* S=&T->Base;xrtAtomic32Init(&S->Entered,0);xrtAtomic32Init(&S->Permit,0);xrtAtomic32Init(&S->Calls,0);xrtAtomic32Init(&S->Window,0);
    T->Context=testTlsServerContext();T->Identity=testTlsServerIdentity();T->OwnContext=T->OwnIdentity=true;
    xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=rpcTlsServiceVerify;V.Context=T;xtlsverifylimits L;xrtTlsVerifyLimitsInit(&L);T->Verifier=xrtTlsVerifierCreateLimited(&V,&L);
    rpcServiceRequire(T->Context && T->Identity && T->Verifier,"TLS service fixture context/identity/verifier");
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=3;S->Engine=xrtNetEngineCreate(&E);rpcServiceRequire(S->Engine && xrtNetEngineStart(S->Engine),"TLS service engine");
    xrpchttpserverconfig H={0};if(HTTP)H=*HTTP;else rpcServiceRequire(xrpcHttpServerConfigInit(&H),"HTTP service config init");H.Execution.Call=rpcServiceCall;H.Execution.User=S;
    xrpchttpservertlsconfig K={0};if(TLS)K=*TLS;else rpcServiceRequire(xrpcHttpServerTlsConfigInit(&K),"TLS service config init");
    if(!K.Session.Context)K.Session.Context=T->Context;
    if(!K.Session.Identity && !K.Session.Select)K.Session.Identity=T->Identity;
    if(!K.HandshakeError){K.HandshakeError=rpcTlsServiceError;K.Context=T;}
    rpcServiceRequire(Secure?xrpcHttpServerTlsCreate(S->Engine,&H,&K,&S->Server):xrpcHttpServerCreate(S->Engine,&H,&S->Server),"TLS service create");
    xrpchttpserverinfo I={0};I.Size=sizeof(I);rpcServiceRequire(xrpcHttpServerInfo(S->Server,&I),"TLS service created accounting");S->BaseBytes=I.BufferBytes;
}
static inline void rpcTlsServiceListen(rpcTlsService* T)
{rpcServiceRequire(xrpcHttpServerStart(T->Base.Server) && xrpcHttpServerLocal(T->Base.Server,&T->Base.Address),"TLS service start/local");}
static inline void rpcTlsServiceStart(rpcTlsService* T,const xrpchttpserverconfig* HTTP,const xrpchttpservertlsconfig* TLS,bool Secure)
{rpcTlsServiceCreate(T,HTTP,TLS,Secure);rpcTlsServiceListen(T);}
static inline xrpchttpclient* rpcTlsServiceClient(rpcTlsService* T,const xrpchttpconfig* HTTP)
{
    xrpchttptlsconfig K;xrpcHttpTlsConfigInit(&K);K.Session.Context=T->Context;K.Session.Verifier=T->Verifier;K.Session.VerifyName=(xstrview)XRT_STR_LITERAL("rpc.fixture");K.Stream.CloseTimeout=200;
    xrpchttpclient* C=NULL;rpcServiceRequire(xrpcHttpTlsConnect(T->Base.Engine,&T->Base.Address,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),HTTP,&K,&C),"TLS service client");return C;
}
static inline void rpcTlsServiceStop(rpcTlsService* T,bool Secure)
{
    rpcService* S=&T->Base;rpcServiceRequire(xrpcHttpServerClose(S->Server) && xrpcHttpServerWaitFor(S->Server,10000)==XWAIT_OK,"TLS service physical server drain");
    if(Secure){xrpchttpservertlsinfo I={0};I.Size=sizeof(I);rpcServiceRequire(xrpcHttpServerTlsInfo(S->Server,&I) && !I.ActiveHandshakes && I.Admitted==I.Completed+I.Failed,"TLS handshake ownership accounting");}
    rpcServiceStop(S);xrtTlsVerifierRelease(T->Verifier);if(T->OwnIdentity)xrtTlsIdentityRelease(T->Identity);if(T->OwnContext)xrtTlsContextRelease(T->Context);
}
#endif
