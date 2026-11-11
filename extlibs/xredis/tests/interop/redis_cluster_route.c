#include <xredis/cluster_route.h>
#include <xredis/pool_shared_connect.h>
#include <xredis/dial.h>
#include <xrt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void rrRequire(bool B,cstr T){if(!B){fprintf(stderr,"[FAIL] real Cluster route: %s\n",T);exit(1);}}
static const void* rrWait(xfuture* F){rrRequire(F && xrtFutureWaitFor(F,15000)==XWAIT_OK && xrtFutureValue(F),"typed native wait");return xrtFutureValue(F);}
static xredisreplyvalue rrValue(xredisreply* R)
{xredisreplyinfo I={0};I.Size=sizeof(I);xredisreplyvalue V;rrRequire(xredisReplyInfo(R,&I) && xredisReplyGet(R,I.Root,&V),"native reply root");return V;}
static bool rrContains(xbytesview V,cstr Text)
{size_t N=strlen(Text);for(size_t I=0;I+N<=V.Size;++I)if(!memcmp(V.Data+I,Text,N))return true;return false;}
typedef struct rrnode {
    xnetresolver* Resolver;xnetaddr Address;xredisclientinitconfig Init;unsigned Factory;
    xredispool* Pool;xredisclient* Client;xfuture* Closed[8];xatomic32 Calls,Drops;
} rrnode;
static bool rrConnect(xnetengine* E,size_t Slot,uint64 Attempt,int64 Timeout,ptr Data,xredisclient** Out)
{
    (void)Slot;(void)Attempt;rrnode* N=Data;uint32 Call=xrtAtomic32FetchAdd(&N->Calls,1,XMEMORY_RELEASE);rrRequire(Call<8,"unexpected node churn");
    xredisclientinitconfig I=N->Init;I.Timeout=Timeout;xredisclientdialconfig D;xredisClientDialConfigInit(&D);D.Family=XNET_FAMILY_IPV4;bool OK;
    if(N->Factory==1)OK=xredisClientDial(E,N->Resolver,XRT_STR_LITERAL("localhost"),N->Address.Port,0,NULL,&I,&D,Out);
    else if(N->Factory==2){xnetaddrlist* A=xrtNetAddrListCreate(&N->Address,1);if(!A)return false;OK=xredisClientDialAddresses(E,A,0,NULL,&I,&D,Out);xrtNetAddrListDestroy(A);}
    else OK=xredisClientConnectInit(E,&N->Address,0,NULL,&I,Out);
    if(OK)N->Closed[Call]=xredisClientClosed(*Out);
    return OK;
}
static void rrDrop(ptr Data)
{
    rrnode* N=Data;for(size_t I=0;I<8;++I)if(N->Closed[I]){rrRequire(xrtFutureState(N->Closed[I])==XFUTURE_RESOLVED,"context before physical close");xrtFutureDestroy(N->Closed[I]);N->Closed[I]=NULL;}
    xrtAtomic32FetchAdd(&N->Drops,1,XMEMORY_RELEASE);
}
typedef struct rrcontext {rrnode Nodes[3];xatomic32 Providers;} rrcontext;
static xredisclusterpoolstatus rrPool(const xredisclusteraddress* A,int64 Remaining,ptr Data,xredispool** Out)
{
    rrcontext* C=Data;xrtAtomic32FetchAdd(&C->Providers,1,XMEMORY_RELAXED);
    if(Remaining<=0 || A->Tls || A->Host.Size!=9 || memcmp(A->Host.Data,"127.0.0.1",9))return XREDIS_CLUSTER_POOL_DENIED;
    for(size_t I=0;I<3;++I)if(A->Port==C->Nodes[I].Address.Port){xredisPoolRetain(C->Nodes[I].Pool);*Out=C->Nodes[I].Pool;return XREDIS_CLUSTER_POOL_READY;}
    return XREDIS_CLUSTER_POOL_DENIED;
}
static xfuture* rrCall(xredispool* P,const xbytesview* A,size_t N)
{xfuture* F=NULL;rrRequire(xredisSharedPoolCall(P,A,N,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"ordinary shared call");return F;}
static xredisclustertopology* rrStale(uint16 Port,uint16 Slot)
{
    char Wire[150];int N=snprintf(Wire,sizeof(Wire),"*1\r\n*3\r\n:%u\r\n:%u\r\n*2\r\n$9\r\n127.0.0.1\r\n:%u\r\n",(unsigned)Slot,(unsigned)Slot,(unsigned)Port);
    rrRequire(N>0 && (size_t)N<sizeof(Wire),"stale snapshot encoding");xredisreplyreader* Reader=NULL;xredisreply* Reply=NULL;size_t Used=0;xredisclustertopology* T=NULL;
    rrRequire(xredisReplyReaderCreate(NULL,&Reader) && xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire,(size_t)N},&Used,&Reply)==XREDIS_REPLY_READY && Used==(size_t)N &&
        xredisClusterTopologyCreate(Reply,XREDIS_CLUSTER_LEGACY_SLOTS,NULL,&T)==XREDIS_CLUSTER_VALUE,"stale snapshot parse");xredisReplyRelease(Reply);xredisReplyReaderDestroy(Reader);return T;
}
static xredisclusterrouteresult rrDrive(xredisclusterroute* R)
{
    double Due=xrtTimer()+15;
    for(;;){rrRequire(xrtTimer()<Due,"route drive bound");xredisclusterroutestep S=xredisClusterRouteStep(R);rrRequire(S!=XREDIS_CLUSTER_ROUTE_STEP_ERROR,"route step");if(S==XREDIS_CLUSTER_ROUTE_DONE)break;
        if(S==XREDIS_CLUSTER_ROUTE_WAIT){int64 Ms=(int64)ceil((xredisClusterRouteNextWake(R)-xrtTimer())*1000);if(Ms<1)Ms=1;if(Ms>10)Ms=10;xfuture* F=xredisClusterRoutePending(R);if(F){(void)xrtFutureWaitFor(F,Ms);xrtFutureDestroy(F);}else xrtSleep((uint32)Ms);}}
    xredisclusterrouteresult V={0};V.Size=sizeof(V);rrRequire(xredisClusterRouteResult(R,&V),"route result");return V;
}
typedef struct rrproducer {
    rrcontext* Context;const xredisclustertopology* Topology;uint16 Slot;bool Stale;
    int64 Values[8];xfuture* Saved;
} rrproducer;
static int32 rrProduce(ptr Data)
{
    rrproducer* P=Data;const xbytesview A[]={XRT_BYTES_LITERAL("INCR"),XRT_BYTES_LITERAL("xrt-owned:asking:{fixed}\0counter")};
    for(unsigned N=0;N<8;++N){xredisclusterrouteconfig C;xredisClusterRouteConfigInit(&C);C.GetPool=rrPool;C.Data=P->Context;C.Timeout=10000;xredisclusterroute* R=NULL;
        rrRequire(xredisClusterRouteCreate(P->Topology,(xbytesview)XRT_BYTES_LITERAL("127.0.0.1"),P->Slot,A,2,&C,&R)==XREDIS_ADMISSION_ACCEPTED,"automatic route create");xredisclusterrouteresult V=rrDrive(R);
        rrRequire(V.Status==XREDIS_CLUSTER_ROUTE_COMPLETE && V.RequestObserved && V.Query.Status==XREDIS_REQUEST_OK && V.ControlReceived && V.Control.Status==XREDIS_REQUEST_OK &&
            V.Attempts==(P->Stale?3u:2u) && V.Redirects==V.Attempts-1 && V.Moved==(P->Stale?1u:0u) && V.Asks==1 && V.UsedAsking,"real MOVED/ASK automatic chain");
        xredisreplyvalue Value=rrValue(V.Query.Reply);rrRequire(Value.Type==XREDIS_REPLY_INTEGER && Value.Integer>=1 && Value.Integer<=32,"exactly one counter mutation per routed request");P->Values[N]=Value.Integer;
        if(!N)P->Saved=xredisClusterRouteResultFuture(R);
        xredisClusterRouteDestroy(R);
    }
    return 0;
}
int main(int Argc,char** Argv)
{
    if(Argc!=7)return 2;
    uint16 Ports[3];for(unsigned I=0;I<3;++I){unsigned long P=strtoul(Argv[1+I],NULL,10);if(!P || P>65535 || strlen(Argv[4+I])!=40)return 2;Ports[I]=(uint16)P;}
    xnetengineconfig EC;xrtNetEngineConfigInit(&EC);EC.Workers=2;xnetengine* E=xrtNetEngineCreate(&EC);rrRequire(E && xrtNetEngineStart(E),"engine start");xnetresolver* Resolver=xrtNetResolverCreate(NULL);rrRequire(Resolver!=NULL,"resolver");
    const xredisinitmode Modes[]={XREDIS_INIT_RESP2,XREDIS_INIT_RESP3,XREDIS_INIT_AUTO};xfuture* Saved[144]={0};int64 Expected[144]={0};size_t Retained=0,Cases=0;
    for(unsigned Maintained=0;Maintained<2;++Maintained)for(unsigned Factory=0;Factory<3;++Factory)for(unsigned Protocol=0;Protocol<3;++Protocol)for(unsigned Stale=0;Stale<2;++Stale){
        rrcontext Context={0};
        for(unsigned I=0;I<3;++I){rrnode* N=&Context.Nodes[I];N->Resolver=Resolver;N->Factory=Factory;xredisClientInitConfigInit(&N->Init);N->Init.Mode=Modes[Protocol];rrRequire(xrtNetAddrParse(&N->Address,"127.0.0.1",Ports[I]),"owned node address");
            if(Maintained){xredispoolconnectconfig C;xredisPoolConnectConfigInit(&C);C.Connect=rrConnect;C.Drop=rrDrop;C.Data=N;C.Protocol=Protocol==0?XREDIS_RESP2:XREDIS_RESP3;C.RetryMin=C.RetryMax=5;
                rrRequire(xredisSharedPoolConnect(E,1,1,NULL,&C,&N->Pool)==XREDIS_ADMISSION_ACCEPTED,"maintained node pool");double Due=xrtTimer()+15;
                for(;;){xredispoolinfo PI={0};PI.Size=sizeof(PI);rrRequire(xredisPoolInfo(N->Pool,&PI) && xrtTimer()<Due,"maintained readiness");if(PI.Idle==1)break;xrtThreadYield();}}
            else {rrRequire(rrConnect(E,0,1,10000,N,&N->Client),"fixed node connect");xfuture* Ready=xredisClientReady(N->Client);rrRequire(((const xredisreadyresult*)rrWait(Ready))->Status==XREDIS_READY_OK,"fixed native ready");xrtFutureDestroy(Ready);
                rrRequire(xredisSharedPoolCreate(E,1,&N->Client,1,NULL,&N->Pool)==XREDIS_ADMISSION_ACCEPTED,"fixed node pool");}
            xbytesview Info[]={XRT_BYTES_LITERAL("INFO"),XRT_BYTES_LITERAL("SERVER")};xfuture* F=rrCall(N->Pool,Info,2);const xredisrequestresult* R=rrWait(F);
            rrRequire(R->Status==XREDIS_REQUEST_OK && rrContains(rrValue(R->Reply).Data,Argv[4+I]),"all actual owned node identities before writes");xrtFutureDestroy(F);
        }
        xbytesview Reset[]={XRT_BYTES_LITERAL("SET"),XRT_BYTES_LITERAL("xrt-owned:asking:{fixed}\0counter"),XRT_BYTES_LITERAL("0")};xfuture* F=NULL;
        rrRequire(xredisSharedPoolAskingCall(Context.Nodes[1].Pool,Reset,3,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"initialize owned importing counter");const xredisaskingresult* ResetResult=rrWait(F);
        rrRequire(ResetResult->Query.Status==XREDIS_REQUEST_OK && ResetResult->ControlReceived && ResetResult->Control.Status==XREDIS_REQUEST_OK,"owned counter initialization");xrtFutureDestroy(F);
        uint16 Slot;rrRequire(xredisClusterKeySlot(Reset[1],&Slot),"counter key slot");xredisclustertopology* T=NULL;
        if(Stale)T=rrStale(Ports[2],Slot);
        else {xbytesview Shards[]={XRT_BYTES_LITERAL("CLUSTER"),XRT_BYTES_LITERAL("SHARDS")};F=rrCall(Context.Nodes[0].Pool,Shards,2);const xredisrequestresult* R=rrWait(F);
            rrRequire(R->Status==XREDIS_REQUEST_OK && xredisClusterTopologyCreate(R->Reply,XREDIS_CLUSTER_SHARDS,NULL,&T)==XREDIS_CLUSTER_VALUE,"actual SHARDS bootstrap snapshot");xrtFutureDestroy(F);}
        rrproducer Work[4];xthread* Threads[4];for(unsigned I=0;I<4;++I){Work[I]=(rrproducer){&Context,T,Slot,Stale!=0,{0},NULL};Threads[I]=xrtThreadCreate(rrProduce,&Work[I],0);rrRequire(Threads[I]!=NULL,"route producer");}
        uint32 Seen=0;for(unsigned I=0;I<4;++I){rrRequire(xrtThreadWaitFor(Threads[I],30000)==XWAIT_OK && !xrtThreadExitCode(Threads[I]),"route producers joined");xrtThreadDestroy(Threads[I]);
            for(unsigned N=0;N<8;++N){uint32 Bit=(uint32)1u<<(unsigned)(Work[I].Values[N]-1);rrRequire(!(Seen&Bit),"routed mutation duplicated");Seen|=Bit;}
            Saved[Retained]=Work[I].Saved;Expected[Retained++]=Work[I].Values[0];}
        rrRequire(Seen==UINT32_MAX,"routed mutation sequence complete");xredisClusterTopologyRelease(T);
        xbytesview Get[]={XRT_BYTES_LITERAL("GET"),Reset[1]};F=NULL;rrRequire(xredisSharedPoolAskingCall(Context.Nodes[1].Pool,Get,2,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"verify actual final counter");
        const xredisaskingresult* Final=rrWait(F);xredisreplyvalue V=rrValue(Final->Query.Reply);rrRequire(Final->Query.Status==XREDIS_REQUEST_OK && V.Data.Size==2 && !memcmp(V.Data.Data,"32",2),"server counter proves no extra business execution");xrtFutureDestroy(F);
        for(unsigned I=0;I<3;++I){rrnode* N=&Context.Nodes[I];xfuture* Closed=xredisPoolClosed(N->Pool);xredisPoolClose(N->Pool);rrRequire(xrtFutureWaitFor(Closed,15000)==XWAIT_OK,"node physical close");xrtFutureDestroy(Closed);xredispoolinfo PI={0};PI.Size=sizeof(PI);
            rrRequire(xredisPoolInfo(N->Pool,&PI) && !PI.Requests && !PI.CommandBytes && !PI.Pending && !PI.Leased,"node physical quotas");xredisPoolRelease(N->Pool);
            if(N->Client){xredisClientRelease(N->Client);rrDrop(N);}rrRequire(xrtAtomic32Load(&N->Drops,XMEMORY_ACQUIRE)==1 && xrtAtomic32Load(&N->Calls,XMEMORY_ACQUIRE)==1,"node factory lifetime without replay/churn");}
        ++Cases;
    }
    rrRequire(Cases==36 && Retained==144 && xrtNetResolverDestroy(Resolver) && xrtNetEngineStop(E) && xrtNetEngineDestroy(E),"all native owners joined");
    for(size_t I=0;I<Retained;++I){const xredisaskingresult* R=rrWait(Saved[I]);rrRequire(R->Query.Status==XREDIS_REQUEST_OK && rrValue(R->Query.Reply).Integer==Expected[I],"retained routed native result");xrtFutureDestroy(Saved[I]);}
    puts("[PASS] real Cluster route: 36 fixed/maintained numeric/DNS/prepared RESP2/3/AUTO current/stale topology cases; four producers, automatic MOVED+ASK and exactly32 INCR executions; retained native outcomes");return 0;
}
