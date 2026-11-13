#include "test.h"
#include <xrpc/tcp_dial.h>
#include <xrt/memory_stats.h>
#include "../examples/common/tcp_service.h"
typedef struct tcpDialLookup {
    rpcTcpService* Service;xatomic32 Entered,Permit,Calls;bool Hold,Fail;uint64 Caller;
} tcpDialLookup;
static void tcpDialWait(xatomic32* Value)
{double End=xrtTimer()+10;while(!xrtAtomic32Load(Value,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"TCP dial barrier timeout");xrtSleep(1);}}
static xnetaddrlist* tcpDialLookupCall(cstr Name,xnetfamily Family,ptr Data)
{
    tcpDialLookup* L=Data;testRequire(!xrtNetEngineCurrent(L->Service->Engine) && xrtThreadCurrentId()!=L->Caller,"DNS ran on caller/network worker");
    testRequire(!strcmp(Name,"rpc.test") && (Family==XNET_FAMILY_UNSPEC || Family==XNET_FAMILY_IPV4),"DNS copied name/family");
    xrtAtomic32FetchAdd(&L->Calls,1,XMEMORY_RELAXED);xrtAtomic32Store(&L->Entered,1,XMEMORY_RELEASE);
    while(L->Hold && !xrtAtomic32Load(&L->Permit,XMEMORY_ACQUIRE))xrtSleep(1);
    if(L->Fail){xrtSetErrorInfo(XERR_IO,"dns.failure.domain.with.more.than.sixty.three.bytes.for.native.snapshot.truncation",17,"fixture DNS failure");return NULL;}
    xnetaddr Address=L->Service->Address;Address.Port=0;return xrtNetAddrListCreate(&Address,1);
}
static xnetresolver* tcpDialResolver(rpcTcpService* S,tcpDialLookup* L,bool Hold,bool Fail)
{
    memset(L,0,sizeof(*L));L->Service=S;L->Hold=Hold;L->Fail=Fail;L->Caller=xrtThreadCurrentId();xrtAtomic32Init(&L->Entered,0);xrtAtomic32Init(&L->Permit,0);xrtAtomic32Init(&L->Calls,0);
    xnetresolverconfig C;xrtNetResolverConfigInit(&C);C.Workers=1;C.CacheEntries=0;C.RequestLimit=8;C.QueryLimit=8;C.Lookup=tcpDialLookupCall;C.LookupData=L;
    xnetresolver* R=xrtNetResolverCreate(&C);testRequire(R!=NULL,"TCP dial resolver");return R;
}
typedef struct tcpDialGate {xnetpost Post;xatomic32 Entered,Permit;} tcpDialGate;
static void tcpDialGateRun(xnetworker* W,ptr Data)
{(void)W;tcpDialGate* G=Data;xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&G->Permit,XMEMORY_ACQUIRE))xrtSleep(1);}
static void tcpDialGateStart(rpcTcpService* S,tcpDialGate* G)
{xrtAtomic32Init(&G->Entered,0);xrtAtomic32Init(&G->Permit,0);testRequire(xrtNetPostInit(&G->Post) && xrtNetPost(xrtNetEngineWorker(S->Engine,1),&G->Post,tcpDialGateRun,G),"TCP dial worker gate");tcpDialWait(&G->Entered);}
static xfuture* tcpDialCall(xrpctcp* Peer,int64 Timeout)
{xfuture* F=NULL;testRequire(xrpcTcpCall(Peer,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]"),Timeout,&F)==XRPC_DUPLEX_ACCEPTED,"TCP dial queued call");return F;}
static void tcpDialResult(xfuture* F,xrpccallstatus Status,xrpcdelivery Delivery)
{const xrpccallresult* R=rpcTcpServiceResult(F);if(R->Status!=Status || R->Delivery!=Delivery)fprintf(stderr,"result %d/%d wanted %d/%d\n",R->Status,R->Delivery,Status,Delivery);testRequire(R->Status==Status && R->Delivery==Delivery,"TCP dial typed result");}
static xrpctcp* tcpDialPeer(rpcTcpService* S,xnetresolver* R)
{xrpctcp* P=NULL;testRequire(xrpcTcpDial(S->Engine,R,XRT_STR_LITERAL("rpc.test"),S->Address.Port,1,&S->Config,NULL,&P),"TCP DNS factory");return P;}
static void testTcpDialCalls(void)
{
    for(unsigned Format=1;Format<=2;++Format){rpcTcpService S;rpcTcpServiceStart(&S);S.Config.Frame.Format=(xrpcframeformat)Format;S.Config.ReadChunkBytes=3;S.Config.ReadBytes=7;S.Config.WriteChunkBytes=1;S.Config.WriteBytes=5;
        tcpDialLookup L;xnetresolver* R=tcpDialResolver(&S,&L,false,false);tcpDialGate G;tcpDialGateStart(&S,&G);char Name[]="rpc.test";xrpctcp* P=NULL;
        testRequire(xrpcTcpDial(S.Engine,R,(xstrview){Name,8},S.Address.Port,1,&S.Config,NULL,&P),"copied DNS name");memset(Name,'x',8);
        xfuture* F[12];for(unsigned I=0;I<8;++I)F[I]=tcpDialCall(P,5000);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);
        for(unsigned I=0;I<8;++I)tcpDialResult(F[I],XRPC_CALL_OK,XRPC_REPLIED);
        testRequire(xrtAtomic32Load(&S.Ready,XMEMORY_ACQUIRE),"winner never reached shared server");
        testRequire(xrpcTcpCall(S.Accepted,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[8]"),5000,F+8)==XRPC_DUPLEX_ACCEPTED,"DNS reverse call");tcpDialResult(F[8],XRPC_CALL_OK,XRPC_REPLIED);
        testRequire(xrpcTcpNotify(P,XRT_STR_LITERAL("note"),(xstrview){0},5000,F+9)==XRPC_DUPLEX_ACCEPTED,"DNS notification");tcpDialResult(F[9],XRPC_CALL_NOTIFY_DONE,XRPC_MAY_HAVE_EXECUTED);
        xrpcduplexitem Items[]={{false,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")},{true,XRT_STR_LITERAL("note"),{0}}};
        testRequire(xrpcTcpBatch(P,Items,2,5000,F+10)==XRPC_DUPLEX_ACCEPTED,"DNS batch");tcpDialResult(F[10],XRPC_CALL_BATCH_DONE,XRPC_REPLIED);
        testRequire(xrtAtomic32Load(&L.Calls,XMEMORY_RELAXED)==1,"RPC replay/reconnect DNS");rpcTcpServiceCycleClose(&S,P);testRequire(xrtNetResolverDestroy(R),"DNS external resolver join");rpcTcpServiceStop(&S);
        tcpDialResult(F[0],XRPC_CALL_OK,XRPC_REPLIED);for(unsigned I=0;I<11;++I)xrtFutureDestroy(F[I]);
    }
}
static void testTcpDialPrepared(void)
{
    rpcTcpService S;rpcTcpServiceStart(&S);xnetaddr Addresses[2];xrtNetAddrLoopback(Addresses,XNET_FAMILY_IPV4,0);xnetsocket Bound=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_STREAM,0);
    testRequire(Bound && xrtNetSocketBind(Bound,Addresses) && xrtNetSocketLocal(Bound,Addresses),"reserve refused endpoint");Addresses[1]=S.Address;
    xnetaddrlist* List=xrtNetAddrListCreate(Addresses,2);xrpctcpdialconfig D;xrpcTcpDialConfigInit(&D);D.Family=XNET_FAMILY_IPV4;D.FallbackDelay=0;xrpctcp* P=NULL;
    testRequire(List && xrpcTcpDialAddresses(S.Engine,List,1,&S.Config,&D,&P),"prepared candidate factory");xrtNetAddrListDestroy(List);xfuture* F=tcpDialCall(P,5000);tcpDialResult(F,XRPC_CALL_OK,XRPC_REPLIED);
    rpcTcpServiceCycleClose(&S,P);testRequire(xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED)==1,"candidate failure replayed operation");xrtFutureDestroy(F);testRequire(xrtNetSocketClose(Bound),"reserved endpoint close");rpcTcpServiceStop(&S);
}
static void testTcpDialStops(void)
{
    for(unsigned Mode=0;Mode<4;++Mode){rpcTcpService S;rpcTcpServiceStart(&S);tcpDialLookup L;xnetresolver* R=tcpDialResolver(&S,&L,true,false);
        if(Mode==2)S.Config.Stream.ConnectTimeout=50;else if(Mode<2)S.Config.Stream.ConnectTimeout=XRT_WAIT_FOREVER;
        xrpctcp* P=tcpDialPeer(&S,R);tcpDialWait(&L.Entered);xfuture* F=tcpDialCall(P,Mode==0?40:5000);
        if(Mode==0)tcpDialResult(F,XRPC_CALL_TIMEOUT,XRPC_NOT_SENT);
        else if(Mode==1){testRequire(xrtFutureCancel(F),"DNS queued cancel");tcpDialResult(F,XRPC_CALL_CANCELLED,XRPC_NOT_SENT);}
        else if(Mode==2)tcpDialResult(F,XRPC_CALL_TRANSPORT,XRPC_NOT_SENT);
        else{testRequire(xrpcTcpClose(P),"close held DNS");tcpDialResult(F,XRPC_CALL_CLOSED,XRPC_NOT_SENT);}
        xrtFutureDestroy(F);
        if(Mode<2){F=tcpDialCall(P,5000);xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);tcpDialResult(F,XRPC_CALL_OK,XRPC_REPLIED);xrtFutureDestroy(F);}
        else {
            testRequire(xrpcTcpClose(P) && xrpcTcpWaitFor(P,5000)==XWAIT_OK && xrpcTcpDestroy(P),"cancelled operation callback retirement");P=NULL;
            testRequire(!xrtAtomic32Load(&L.Permit,XMEMORY_ACQUIRE) && xrtNetResolverTryDestroy(R)==XNET_RETIRE_BUSY,"peer retirement was confused with host lookup/thread retirement");
            xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);
        }
        rpcTcpServiceCycleClose(&S,P);testRequire(xrtNetResolverDestroy(R),"held DNS host join");testRequire(xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED)==(Mode<2?1u:0u),"stopped DNS operation executed");rpcTcpServiceStop(&S);
    }
    for(unsigned Mode=0;Mode<3;++Mode){rpcTcpService S;rpcTcpServiceStart(&S);tcpDialLookup L;xnetresolver* R=tcpDialResolver(&S,&L,false,false);tcpDialGate G;tcpDialGateStart(&S,&G);
        S.Config.Stream.ConnectTimeout=Mode==0?0:Mode==1?40:5000;xrpctcp* P=tcpDialPeer(&S,R);xfuture* F=tcpDialCall(P,5000);
        if(Mode==2)testRequire(xrpcTcpClose(P),"pre-start close");else if(Mode==1)xrtSleep(60);
        xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);tcpDialResult(F,Mode==2?XRPC_CALL_CLOSED:XRPC_CALL_TRANSPORT,XRPC_NOT_SENT);xrtFutureDestroy(F);
        testRequire(xrpcTcpWaitFor(P,5000)==XWAIT_OK,"pre-start physical retirement");xrpctcpinfo I={0};I.Size=sizeof(I);testRequire(xrpcTcpInfo(P,&I) && I.Reason==(Mode==2?XRPC_TCP_LOCAL:XRPC_TCP_TIMEOUT),"original factory deadline/reset classification");
        rpcTcpServiceCycleClose(&S,P);testRequire(!xrtAtomic32Load(&L.Calls,XMEMORY_RELAXED) && xrtNetResolverDestroy(R),"expired/closed factory started DNS");rpcTcpServiceStop(&S);
    }
}
static void tcpDialNoop(xnetresolveop* Op,ptr Data){(void)Op;(void)Data;}
static void testTcpDialAdmissionAndCause(void)
{
    for(unsigned Mode=0;Mode<2;++Mode){rpcTcpService S;rpcTcpServiceStart(&S);tcpDialLookup L;xnetresolver* R=tcpDialResolver(&S,&L,Mode==0,Mode==1);xnetresolveop* Ops[8]={0};
        if(!Mode){for(unsigned I=0;I<8;++I){Ops[I]=xrtNetResolverResolve(R,"rpc.test",XNET_FAMILY_IPV4,tcpDialNoop,NULL);testRequire(Ops[I]!=NULL,"fill DNS request quota");}tcpDialWait(&L.Entered);}
        xrpctcp* P=tcpDialPeer(&S,R);xfuture* F=tcpDialCall(P,5000);tcpDialResult(F,Mode?XRPC_CALL_TRANSPORT:XRPC_CALL_LIMIT,XRPC_NOT_SENT);testRequire(xrpcTcpWaitFor(P,5000)==XWAIT_OK,"failed source retirement");
        xrpctcpinfo I={0};I.Size=sizeof(I);testRequire(xrpcTcpInfo(P,&I) && I.NativeErrorKind && I.NativeErrorDomain[0],"native DNS failure snapshot missing");
        if(Mode)testRequire(I.NativeErrorKind==XERR_IO && I.NativeErrorCode && strlen(I.NativeErrorDomain)<64,"native DNS wrapper cause snapshot");
        rpcTcpServiceCycleClose(&S,P);xrtFutureDestroy(F);xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);for(unsigned I=0;I<8;++I)xrtNetResolveOpDestroy(Ops[I]);
        testRequire(xrtNetResolverDestroy(R) && !xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED),"failed source sent operation/leaked resolver duty");rpcTcpServiceStop(&S);
    }
}
static void testTcpDialColdAndOOM(void)
{
    rpcTcpService S;rpcTcpServiceStart(&S);tcpDialLookup L;xnetresolver* R=tcpDialResolver(&S,&L,true,false);xallocationbudget* B=NULL;xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(1048576,&B) && xrtAllocationBudgetEnter(B,&Scope),"cold budget enter");xrpctcp* P=tcpDialPeer(&S,R);testRequire(xrtAllocationBudgetLeave(&Scope),"cold budget leave");tcpDialWait(&L.Entered);
    xallocationbudgetinfo Before={0},After={0};Before.Size=After.Size=sizeof(Before);testRequire(xrtAllocationBudgetInfo(B,&Before),"pending source bytes");xfuture* F=tcpDialCall(P,5000);xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);tcpDialResult(F,XRPC_CALL_OK,XRPC_REPLIED);
    double End=xrtTimer()+5;do{testRequire(xrtAllocationBudgetInfo(B,&After) && xrtTimer()<End,"cold source kept on open peer");if(After.LiveBytes>=Before.LiveBytes)xrtSleep(1);}while(After.LiveBytes>=Before.LiveBytes);
    rpcTcpServiceCycleClose(&S,P);xrtFutureDestroy(F);testRequire(xrtAllocationBudgetInfo(B,&After) && !After.LiveBytes && !After.LiveCount,"cold factory leaked");xrtAllocationBudgetFree(B);testRequire(xrtNetResolverDestroy(R),"cold resolver join");
    R=tcpDialResolver(&S,&L,false,false);unsigned Passed=0,Failed=0;static const size_t Limits[]={1,64,1024,8192,65536,131072,262144,1048576};
    for(unsigned N=0;N<sizeof(Limits)/sizeof(Limits[0]);++N){B=NULL;Scope=(xallocationscope){0};testRequire(xrtAllocationBudgetCreate(Limits[N],&B) && xrtAllocationBudgetEnter(B,&Scope),"factory budget enter");
        P=NULL;bool OK=xrpcTcpDial(S.Engine,R,XRT_STR_LITERAL("rpc.test"),S.Address.Port,1,&S.Config,NULL,&P);testRequire(xrtAllocationBudgetLeave(&Scope),"factory budget leave");
        if(OK){++Passed;F=tcpDialCall(P,5000);tcpDialResult(F,XRPC_CALL_OK,XRPC_REPLIED);rpcTcpServiceCycleClose(&S,P);xrtFutureDestroy(F);}else{++Failed;testRequire(!P && xrtErrorKind(xrtGetError())==XERR_MEMORY,"failed factory output/cause");}xrtClearError();
        After=(xallocationbudgetinfo){0};After.Size=sizeof(After);testRequire(xrtAllocationBudgetInfo(B,&After) && !After.LiveBytes && !After.LiveCount,"factory source/peer budget leaked");xrtAllocationBudgetFree(B);
    }
    testRequire(Passed && Failed,"constructor allocation boundaries");testRequire(xrtNetResolverDestroy(R),"OOM host resolver join");
    xnetaddr Invalid[65];for(unsigned I=0;I<65;++I){Invalid[I]=S.Address;Invalid[I].Port=(uint16)(10000+I);}
    for(unsigned Mode=0;Mode<3;++Mode){if(Mode==1)Invalid[0].Port=0;else Invalid[0]=S.Address;
        xnetaddrlist* List=xrtNetAddrListCreate(Invalid,Mode==0?65:1);testRequire(List && xrtNetAddrListCount(List)==(Mode==0?65u:1u),"prepared validation list has distinct endpoints");xrpctcpdialconfig D;xrpcTcpDialConfigInit(&D);if(Mode==2)D.Family=XNET_FAMILY_IPV6;
        P=(xrpctcp*)(uintptr_t)1;testRequire(!xrpcTcpDialAddresses(S.Engine,List,1,&S.Config,&D,&P) && P==(xrpctcp*)(uintptr_t)1,"prepared count/port/family validation changed output");xrtNetAddrListDestroy(List);xrtClearError();}
    for(unsigned Mode=0;Mode<8;++Mode){xrpctcpdialconfig D;xrpcTcpDialConfigInit(&D);if(Mode==0)D.Size=0;else if(Mode==1)D.Family=(xnetfamily)999;else if(Mode==2)D.FallbackDelay=-1;else if(Mode==3)D.MaxAttempts=65;else if(Mode==4)D.MaxAttempts=0;else if(Mode==5)D.MaxHostBytes=65536;else if(Mode==6)D.MaxHostBytes=0;
        P=(xrpctcp*)(uintptr_t)1;testRequire(!xrpcTcpDial(S.Engine,(xnetresolver*)(uintptr_t)1,Mode==7?(xstrview){"a\0b",3}:XRT_STR_LITERAL("rpc.test"),S.Address.Port,1,&S.Config,&D,&P) && P==(xrpctcp*)(uintptr_t)1,"invalid dial inputs changed output");xrtClearError();}
    rpcTcpServiceStop(&S);
}
int main(void)
{setbuf(stdout,NULL);puts("[case] TCP DNS calls");testTcpDialCalls();puts("[case] TCP prepared");testTcpDialPrepared();puts("[case] TCP DNS stops");testTcpDialStops();puts("[case] TCP DNS admission/cause");testTcpDialAdmissionAndCause();puts("[case] TCP DNS cold/OOM");testTcpDialColdAndOOM();puts("[PASS] xrpc TCP dial: copied-DNS/prepared-candidates/bidirectional/batch/notify/original-deadline/cancel/physical-source-retirement/native-error/admission/retained/cold-release/OOM");return 0;}
