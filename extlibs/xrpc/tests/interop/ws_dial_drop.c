#include "../test.h"
static xnetdial* dropDial(xnetengine*,xnetresolver*,cstr,uint16,const xnetdialconfig*,const xnetstreamevents*,ptr,ptr,const xnetdialownershipv1*);
#define xrtNetDialOwnedV1 dropDial
#ifndef XRPC_WS_DROP_SOURCE
#define XRPC_WS_DROP_SOURCE "../../src/ws/ws.c"
#endif
#include XRPC_WS_DROP_SOURCE
#undef xrtNetDialOwnedV1
#define main wsDropExistingMain
#include "../test_ws_dial.c"
#undef main
typedef struct dropHolder {xatomicptr Data;xatomic32 Held,Released,Sample;xnetpost Release,Inspect;const xnetdialownershipv1* Original;xrpcws* Peer;} dropHolder;
static dropHolder* DropHolder;
static void dropDone(xnetdial* D,xnetresult R,xnetstream* S,const xerror* E,ptr P)
{DropHolder->Original->Done(D,R,S,E,P);}
static void dropHold(const void* P)
{
    /* Move the ONE actual owned callback-context credit into this test holder.
     * No reference increment, invented Count, native result or IO is substituted.
     * Its original Drop is delegated later on the same owning Worker. */
    xrtAtomicPtrStore(&DropHolder->Data,(ptr)P,XMEMORY_RELEASE);xrtAtomic32Store(&DropHolder->Held,1,XMEMORY_RELEASE);
}
static xnetdial* dropDial(xnetengine* E,xnetresolver* R,cstr Name,uint16 Port,const xnetdialconfig* C,const xnetstreamevents* Events,ptr EventsData,ptr Data,const xnetdialownershipv1* Policy)
{
    static xnetdialownershipv1 Held;DropHolder->Original=Policy;Held=(xnetdialownershipv1){sizeof(Held),dropDone,dropHold,Policy->Ops};return xrtNetDialOwnedV1(E,R,Name,Port,C,Events,EventsData,Data,&Held);
}
static void dropRelease(xnetworker* W,ptr Data)
{(void)W;dropHolder* H=Data;ptr P=xrtAtomicPtrLoad(&H->Data,XMEMORY_ACQUIRE);H->Original->Drop(P);xrtAtomicPtrStore(&H->Data,NULL,XMEMORY_RELEASE);xrtAtomic32Store(&H->Released,1,XMEMORY_RELEASE);}
static void dropInspect(xnetworker* W,ptr Data)
{(void)W;dropHolder* H=Data;xrpcws* P=H->Peer;bool Retained=P->Upgrade&&P->Upgrade->Dial&&P->Upgrade->Dial->Pending&&P->Stream&&!P->Upgrade->Native;xrtAtomic32Store(&H->Sample,Retained?1:2,XMEMORY_RELEASE);}
static void dropScene(bool TLS,bool Close,xnetportbackend Backend)
{
    dropHolder H={0};xrtAtomicPtrInit(&H.Data,NULL);xrtAtomic32Init(&H.Held,0);xrtAtomic32Init(&H.Released,0);xrtAtomic32Init(&H.Sample,0);testRequire(xrtNetPostInit(&H.Release)&&xrtNetPostInit(&H.Inspect),"actual held-credit owner nodes");DropHolder=&H;
    rpcWsHttpService S;rpcWsHttpServiceListen(&S,TLS,NULL,false,0,NULL,NULL,Backend);wsDialLookup Lookup;xnetresolver* R=wsDialResolver(&S,&Lookup,false,false);xrpcwshttpconfig HTTP=S.ClientHTTP;HTTP.SetupTimeout=1000;testRequire(wsDialFactory(&S,1,R,NULL,XRT_STR_LITERAL("route.test"),&HTTP,NULL,&S.Client),"real DNS dial with retained callback-context credit");H.Peer=S.Client;xfuture* F=wsDialCall(S.Client,5000);rpcWsHttpServiceAwait(&H.Held);wsDialResult(F,XRPC_CALL_OK,XRPC_REPLIED);
    testRequire(xrtNetPost(S.Client->Worker,&H.Inspect,dropInspect,&H),"inspect retained source on its real owner");rpcWsHttpServiceAwait(&H.Sample);testRequire(xrtAtomic32Load(&H.Sample,XMEMORY_ACQUIRE)==1,"cold source/header retained across WS handoff until actual credit release");
    xfuture* Again=NULL;
    if(Close){testRequire(xrpcWsClose(S.Client)&&xrpcWsWaitFor(S.Client,20)==XWAIT_TIMEOUT,"close joins actual retained source context beyond native WS retirement");}
    else{xrpcwshttpinfo I={.Size=sizeof(I)};testRequire(xrpcWsHttpInfo(S.Client,&I),"original setup deadline snapshot");while(xrtTimer()<=I.Deadline+.02)xrtSleep(1);Again=wsDialCall(S.Client,5000);wsDialResult(Again,XRPC_CALL_OK,XRPC_REPLIED);}
    testRequire(xrtNetPost(S.Client->Worker,&H.Release,dropRelease,&H),"delegate real original owner drop");rpcWsHttpServiceAwait(&H.Released);
    if(!Close){xrpcwshttpinfo I={.Size=sizeof(I)};double End=xrtTimer()+5;do{testRequire(xrpcWsHttpInfo(S.Client,&I)&&xrtTimer()<End,"completed setup storage physically retires");if(I.ColdBytes)xrtSleep(1);}while(I.ColdBytes);testRequire(I.Outcome==XRPC_WS_HTTP_OPEN,"late source cleanup does not rewrite accepted Upgrade");}
    wsDialStop(&S,R);xrtFutureDestroy(F);xrtFutureDestroy(Again);testRequire(!xrtAtomicPtrLoad(&H.Data,XMEMORY_ACQUIRE),"actual held credit consumed once");DropHolder=NULL;printf("[PASS] retained real source Drop TLS%u close%u, live WS past setup deadline and physical source/host join\n",TLS,Close);
}
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;
    for(unsigned TLS=0;TLS<2;++TLS){for(unsigned Close=0;Close<2;++Close)dropScene(TLS!=0,Close!=0,B);}
    return 0;
}
