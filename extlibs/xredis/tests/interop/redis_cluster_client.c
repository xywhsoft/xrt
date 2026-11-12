#include <xredis/cluster_client.h>
#include <xredis/cluster_command.h>
#include <xredis/pool_shared_connect.h>
#include <xredis/dial.h>
#include <xrt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void rrRequire(bool B,cstr T){if(!B){fprintf(stderr,"[FAIL] real Cluster client: %s\n",T);exit(1);}}
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
typedef struct rrcontext {rrnode Nodes[3];xatomic32 Providers,DenySeed,SeedDenied;xredisclusternodes* Cache;} rrcontext;
static bool rrCacheAuthorize(const xredisclusteraddress* A,ptr Data)
{rrcontext* C=Data;if(A->Tls || A->Host.Size!=9 || memcmp(A->Host.Data,"127.0.0.1",9))return false;if(xrtAtomic32Load(&C->DenySeed,XMEMORY_ACQUIRE) && A->Port==C->Nodes[0].Address.Port){xrtAtomic32FetchAdd(&C->SeedDenied,1,XMEMORY_RELEASE);return false;}for(size_t I=0;I<3;++I)if(A->Port==C->Nodes[I].Address.Port)return true;return false;}
static bool rrCacheConnect(const xredisclusteraddress* A,xnetengine* E,size_t Slot,uint64 Attempt,int64 Timeout,ptr Data,xredisclient** Out)
{rrcontext* C=Data;for(size_t I=0;I<3;++I)if(A->Port==C->Nodes[I].Address.Port)return rrConnect(E,Slot,Attempt,Timeout,&C->Nodes[I],Out);return false;}
static void rrCacheDrop(ptr Data){rrcontext* C=Data;for(size_t I=0;I<3;++I)rrDrop(&C->Nodes[I]);}
static xfuture* rrCall(xredispool* P,const xbytesview* A,size_t N)
{xfuture* F=NULL;rrRequire(xredisSharedPoolCall(P,A,N,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"ordinary shared call");return F;}
static xredisclusterclientinfo rrInfo(xredisclusterclient* C)
{xredisclusterclientinfo I={0};I.Size=sizeof(I);rrRequire(xredisClusterClientInfo(C,&I),"managed info");return I;}
static void rrAdmin(rrcontext* C,unsigned Node,const xbytesview* A,size_t Count)
{xfuture* F=rrCall(C->Nodes[Node].Pool,A,Count);const xredisrequestresult* R=rrWait(F);if(R->Status!=XREDIS_REQUEST_OK){xredisreplyvalue V=rrValue(R->Reply);fprintf(stderr,"admin error: %.*s\n",(int)V.Data.Size,V.Data.Data);}rrRequire(R->Status==XREDIS_REQUEST_OK,"owned cluster administration");xrtFutureDestroy(F);}
static uint64 rrEpoch(rrcontext* C,unsigned Node)
{
    xbytesview A[]={XRT_BYTES_LITERAL("CLUSTER"),XRT_BYTES_LITERAL("INFO")};xfuture* F=rrCall(C->Nodes[Node].Pool,A,2);const xredisrequestresult* R=rrWait(F);rrRequire(R->Status==XREDIS_REQUEST_OK,"actual config epoch read");xredisreplyvalue V=rrValue(R->Reply);const char Key[]="cluster_my_epoch:";uint64 Epoch=0;bool Found=false;
    for(size_t I=0;I+sizeof(Key)-1<V.Data.Size;++I)if((!I || V.Data.Data[I-1]=='\n') && !memcmp(V.Data.Data+I,Key,sizeof(Key)-1)){size_t J=I+sizeof(Key)-1,Digits=0;for(;J<V.Data.Size && V.Data.Data[J]>='0' && V.Data.Data[J]<='9';++J){unsigned D=V.Data.Data[J]-'0';rrRequire(Epoch<=(UINT64_MAX-D)/10,"epoch decimal bound");Epoch=Epoch*10+D;++Digits;}rrRequire(Digits && J+1<V.Data.Size && V.Data.Data[J]=='\r' && V.Data.Data[J+1]=='\n',"config epoch shape");Found=true;break;}
    rrRequire(Found,"config epoch field");xrtFutureDestroy(F);return Epoch;
}
static bool rrOwnerViews(rrcontext* C,uint16 Slot,unsigned Owner)
{
    bool Same=true;for(unsigned I=0;I<3;++I){xbytesview A[]={XRT_BYTES_LITERAL("CLUSTER"),XRT_BYTES_LITERAL("SLOTS")};xfuture* F=rrCall(C->Nodes[I].Pool,A,2);const xredisrequestresult* R=rrWait(F);xredisclustertopology* T=NULL;rrRequire(R->Status==XREDIS_REQUEST_OK && xredisClusterTopologyCreate(R->Reply,XREDIS_CLUSTER_LEGACY_SLOTS,NULL,&T)==XREDIS_CLUSTER_VALUE,"owned control-plane snapshot");xredisclusternode N={0};Same=Same && xredisClusterTopologyLookup(T,Slot,&N)==XREDIS_CLUSTER_VALUE && N.Port==C->Nodes[Owner].Address.Port;xredisClusterTopologyRelease(T);xrtFutureDestroy(F);}return Same;
}
static void rrOwner(rrcontext* C,const char* Slot,unsigned Owner,char Ids[3][41])
{
    /* A manual restoration without a newer config epoch can lose to gossip
     * from the previous owner and create artificial MOVED loops. Establish a
     * unique highest actual epoch, then wait for all three real slot views. */
    double Due=xrtTimer()+15;for(;;){xbytesview Bump[]={XRT_BYTES_LITERAL("CLUSTER"),XRT_BYTES_LITERAL("BUMPEPOCH")};rrAdmin(C,Owner,Bump,2);uint64 E[3];for(unsigned I=0;I<3;++I)E[I]=rrEpoch(C,I);if(E[Owner]>E[(Owner+1)%3] && E[Owner]>E[(Owner+2)%3])break;rrRequire(xrtTimer()<Due,"new owner epoch convergence");xrtSleep(5);}
    const unsigned Order[3]={Owner,(Owner+1)%3,(Owner+2)%3};
    for(unsigned I=0;I<3;++I){xbytesview A[]={XRT_BYTES_LITERAL("CLUSTER"),XRT_BYTES_LITERAL("SETSLOT"),{(cbytes)Slot,strlen(Slot)},XRT_BYTES_LITERAL("NODE"),{(cbytes)Ids[Owner],40}};rrAdmin(C,Order[I],A,5);}
    while(!rrOwnerViews(C,(uint16)strtoul(Slot,NULL,10),Owner)){rrRequire(xrtTimer()<Due,"all actual slot views agree");xrtSleep(5);}
}
typedef struct rrproducer {
    xredisclusterclient* Client;uint16 Slot;bool Stale;size_t Moved,Case;
    int64 Values[8];xfuture* Saved;
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
    const xrediscommandcatalog* Catalogue;
#endif
} rrproducer;
static int32 rrProduce(ptr Data)
{
    rrproducer* P=Data;const xbytesview A[]={XRT_BYTES_LITERAL("INCR"),XRT_BYTES_LITERAL("xrt-owned:asking:{fixed}\0counter")};
    for(unsigned N=0;N<8;++N){xfuture* F=NULL;
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
        rrRequire(xredisClusterClientCallCommand(P->Client,P->Catalogue,A,2,XREDIS_CLUSTER_COMMAND_NO_SLOT,10000,&F)==XREDIS_CLUSTER_COMMAND_ACCEPTED,"metadata-selected INCR route");
#else
        rrRequire(xredisClusterClientCall(P->Client,P->Slot,A,2,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"managed route call");
#endif
        const xredisclusterclientresult* V=rrWait(F);const xredisclusterrouteresult* R=&V->Route;
        if(V->Status!=XREDIS_CLUSTER_CLIENT_COMPLETE || !V->RouteObserved || R->Status!=XREDIS_CLUSTER_ROUTE_COMPLETE || !R->RequestObserved || R->Query.Status!=XREDIS_REQUEST_OK){
            fprintf(stderr,"case=%zu sequence=%u stale=%u generation=%llu parent=%d route=%d query=%d observed=%u delivery=%d attempts=%zu moved=%zu asks=%zu\n",P->Case,N,(unsigned)P->Stale,(unsigned long long)V->Generation,(int)V->Status,(int)R->Status,(int)R->Query.Status,(unsigned)R->RequestObserved,(int)R->Query.Delivery,R->Attempts,R->Moved,R->Asks);
            if(R->Query.Reply){xredisreplyvalue Error=rrValue(R->Query.Reply);fprintf(stderr,"reply=%.*s\n",(int)Error.Data.Size,Error.Data.Data);}
            rrRequire(false,"managed native outcome");}
        if(P->Stale)rrRequire(R->Attempts>=1 && R->Attempts<=2 && R->Moved==R->Attempts-1 && !R->Asks && !R->UsedAsking,"real MOVED to new owner");
        else rrRequire(R->Attempts==2 && !R->Moved && R->Asks==1 && R->UsedAsking && R->ControlReceived && R->Control.Status==XREDIS_REQUEST_OK,"real one-shot ASK");
        P->Moved+=R->Moved;xredisreplyvalue Value=rrValue(R->Query.Reply);rrRequire(Value.Type==XREDIS_REPLY_INTEGER && Value.Integer>=1 && Value.Integer<=32,"exactly one counter mutation per managed request");P->Values[N]=Value.Integer;
        if(!N)P->Saved=F;else xrtFutureDestroy(F);
    }
    return 0;
}
int main(int Argc,char** Argv)
{
    if(Argc!=7)return 2;
    uint16 Ports[3];for(unsigned I=0;I<3;++I){unsigned long P=strtoul(Argv[1+I],NULL,10);if(!P || P>65535 || strlen(Argv[4+I])!=40)return 2;Ports[I]=(uint16)P;}
    xnetengineconfig EC;xrtNetEngineConfigInit(&EC);EC.Workers=2;xnetengine* E=xrtNetEngineCreate(&EC);rrRequire(E && xrtNetEngineStart(E),"engine start");xnetresolver* Resolver=xrtNetResolverCreate(NULL);rrRequire(Resolver!=NULL,"resolver");
    const xredisinitmode Modes[]={XREDIS_INIT_RESP2,XREDIS_INIT_RESP3,XREDIS_INIT_AUTO};xfuture* Saved[144]={0};int64 Expected[144]={0};size_t Retained=0,Cases=0;xredisclustermap* Maps[36]={0};uint64 Generations[36]={0};
    for(unsigned Factory=0;Factory<3;++Factory)for(unsigned Protocol=0;Protocol<3;++Protocol)for(unsigned Stale=0;Stale<2;++Stale)for(unsigned Format=0;Format<2;++Format){
        printf("[case] %zu factory=%u protocol=%u stale=%u format=%u\n",Cases,Factory,Protocol,Stale,Format);fflush(stdout);
        rrcontext Context={0};
        for(unsigned I=0;I<3;++I){rrnode* N=&Context.Nodes[I];N->Resolver=Resolver;N->Factory=Factory;xredisClientInitConfigInit(&N->Init);N->Init.Mode=Modes[Protocol];rrRequire(xrtNetAddrParse(&N->Address,"127.0.0.1",Ports[I]),"owned node address");}
        xredisclusternodesconfig NC;xredisClusterNodesConfigInit(&NC);NC.MaxNodes=3;NC.MaxHostBytes=64;NC.Protocol=Protocol==0?XREDIS_RESP2:XREDIS_RESP3;NC.RetryMin=NC.RetryMax=5;NC.Authorize=rrCacheAuthorize;NC.Connect=rrCacheConnect;NC.Drop=rrCacheDrop;NC.Data=&Context;
        rrRequire(xredisClusterNodesCreate(E,1,&NC,&Context.Cache)==XREDIS_ADMISSION_ACCEPTED,"owned node cache");
        for(unsigned I=0;I<3;++I){rrnode* N=&Context.Nodes[I];xredisclusteraddress Address={XRT_BYTES_LITERAL("127.0.0.1"),Ports[I],0,false,0};
            rrRequire(xredisClusterNodesGetPool(&Address,10000,Context.Cache,&N->Pool)==XREDIS_CLUSTER_POOL_READY,"cached native pool");double Due=xrtTimer()+15;
            for(;;){xredispoolinfo PI={0};PI.Size=sizeof(PI);rrRequire(xredisPoolInfo(N->Pool,&PI) && xrtTimer()<Due,"cached node ready");if(PI.Idle==1)break;xrtSleep(1);}
            xbytesview Info[]={XRT_BYTES_LITERAL("INFO"),XRT_BYTES_LITERAL("SERVER")};xfuture* F=rrCall(N->Pool,Info,2);const xredisrequestresult* R=rrWait(F);
            rrRequire(R->Status==XREDIS_REQUEST_OK && rrContains(rrValue(R->Reply).Data,Argv[4+I]),"all actual owned node identities before writes");xrtFutureDestroy(F);
        }
        char Ids[3][41];for(unsigned I=0;I<3;++I){xbytesview A[]={XRT_BYTES_LITERAL("CLUSTER"),XRT_BYTES_LITERAL("MYID")};xfuture* F=rrCall(Context.Nodes[I].Pool,A,2);const xredisrequestresult* R=rrWait(F);xredisreplyvalue V=rrValue(R->Reply);rrRequire(R->Status==XREDIS_REQUEST_OK && V.Data.Size==40,"actual fixture node IDs");memcpy(Ids[I],V.Data.Data,40);Ids[I][40]=0;xrtFutureDestroy(F);}
        xredisclusterclientconfig CC;xredisClusterClientConfigInit(&CC);CC.RefreshMin=1;CC.RefreshRetryMax=5;CC.Refresh.RetryDelay=1;CC.Route.RetryDelay=1;CC.Refresh.Format=Format?XREDIS_CLUSTER_LEGACY_SLOTS:XREDIS_CLUSTER_SHARDS;
        xredisclusteraddress Seed={XRT_BYTES_LITERAL("127.0.0.1"),Ports[0],0,false,0};xredisclusterclient* Client=NULL;rrRequire(xredisClusterClientCreate(E,1,Context.Cache,&Seed,1,&CC,&Client)==XREDIS_ADMISSION_ACCEPTED,"managed owned Cluster client");xfuture* Ready=xredisClusterClientReady(Client);const xredisclusterclientready* ReadyResult=rrWait(Ready);rrRequire(ReadyResult->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && ReadyResult->Generation==1,"real asynchronous bootstrap");xrtFutureDestroy(Ready);
        xbytesview Reset[]={XRT_BYTES_LITERAL("SET"),XRT_BYTES_LITERAL("xrt-owned:asking:{fixed}\0counter"),XRT_BYTES_LITERAL("0")};uint16 Slot;rrRequire(xredisClusterKeySlot(Reset[1],&Slot),"counter slot");char SlotText[6];snprintf(SlotText,sizeof(SlotText),"%u",(unsigned)Slot);
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
        xbytesview Meta[]={XRT_BYTES_LITERAL("COMMAND"),XRT_BYTES_LITERAL("INFO"),XRT_BYTES_LITERAL("INCR"),XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("MSET"),XRT_BYTES_LITERAL("PING"),XRT_BYTES_LITERAL("SORT")};xfuture* Metadata=rrCall(Context.Nodes[0].Pool,Meta,7);const xredisrequestresult* MR=rrWait(Metadata);xrediscommandcatalog* Catalogue=NULL;rrRequire(MR->Status==XREDIS_REQUEST_OK && xredisCommandCatalogCreate(MR->Reply,NULL,&Catalogue)==XREDIS_KEYS_OK,"actual authorized COMMAND INFO snapshot");xrtFutureDestroy(Metadata);
        uint64 AdmissionBefore=rrInfo(Client).Accepted;xfuture* Refused=(void*)(uintptr_t)1;xbytesview Cross[]={XRT_BYTES_LITERAL("MSET"),Reset[1],XRT_BYTES_LITERAL("0"),XRT_BYTES_LITERAL("xrt-owned:other:{distinct}\0key"),XRT_BYTES_LITERAL("0")};uint16 OtherSlot;rrRequire(xredisClusterKeySlot(Cross[3],&OtherSlot) && OtherSlot!=Slot,"distinct oracle slot");
        rrRequire(xredisClusterClientCallCommand(Client,Catalogue,Cross,5,XREDIS_CLUSTER_COMMAND_NO_SLOT,10000,&Refused)==XREDIS_CLUSTER_COMMAND_CROSS_SLOT && Refused==(void*)(uintptr_t)1,"actual metadata cross-slot admission");
        xbytesview Ping[]={XRT_BYTES_LITERAL("PING")},Sort[]={XRT_BYTES_LITERAL("SORT"),Reset[1]};rrRequire(xredisClusterClientCallCommand(Client,Catalogue,Ping,1,XREDIS_CLUSTER_COMMAND_NO_SLOT,10000,&Refused)==XREDIS_CLUSTER_COMMAND_KEYLESS && Refused==(void*)(uintptr_t)1,"keyless command guessed node");rrRequire(xredisClusterClientCallCommand(Client,Catalogue,Sort,2,Slot,10000,&Refused)==XREDIS_CLUSTER_COMMAND_UNSUPPORTED && Refused==(void*)(uintptr_t)1 && rrInfo(Client).Accepted==AdmissionBefore,"incomplete key rule submitted business");
        xfuture* Pinged=NULL;rrRequire(xredisClusterClientCallCommand(Client,Catalogue,Ping,1,Slot,10000,&Pinged)==XREDIS_CLUSTER_COMMAND_ACCEPTED && ((const xredisclusterclientresult*)rrWait(Pinged))->Route.Query.Status==XREDIS_REQUEST_OK,"explicit keyless command");xrtFutureDestroy(Pinged);
#endif
        if(Stale)rrOwner(&Context,SlotText,1,Ids);
        xfuture* F=NULL;if(Stale)F=rrCall(Context.Nodes[1].Pool,Reset,3);else rrRequire(xredisSharedPoolAskingCall(Context.Nodes[1].Pool,Reset,3,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"owned importing initialization");const xredisrequestresult* ResetResult=rrWait(F);rrRequire(ResetResult->Status==XREDIS_REQUEST_OK,"owned counter reset");xrtFutureDestroy(F);
        rrproducer Work[4];xthread* Threads[4];for(unsigned I=0;I<4;++I){Work[I]=(rrproducer){.Client=Client,.Slot=Slot,.Stale=Stale!=0,.Case=Cases};
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
            Work[I].Catalogue=Catalogue;
#endif
            Threads[I]=xrtThreadCreate(rrProduce,&Work[I],0);rrRequire(Threads[I]!=NULL,"route producer");}
        uint32 Seen=0;for(unsigned I=0;I<4;++I){rrRequire(xrtThreadWaitFor(Threads[I],30000)==XWAIT_OK && !xrtThreadExitCode(Threads[I]),"route producers joined");xrtThreadDestroy(Threads[I]);
            for(unsigned N=0;N<8;++N){uint32 Bit=(uint32)1u<<(unsigned)(Work[I].Values[N]-1);rrRequire(!(Seen&Bit),"routed mutation duplicated");Seen|=Bit;}
            Saved[Retained]=Work[I].Saved;Expected[Retained++]=Work[I].Values[0];}
        rrRequire(Seen==UINT32_MAX,"managed mutation sequence complete");
        if(Stale){size_t Moved=0;for(unsigned I=0;I<4;++I)Moved+=Work[I].Moved;rrRequire(Moved>0,"stale bootstrap did not hit real MOVED");double Due=xrtTimer()+15;while(rrInfo(Client).Generation<2 || rrInfo(Client).Dirty){rrRequire(xrtTimer()<Due,"real automatic topology refresh");xrtSleep(1);}}
        else rrRequire(rrInfo(Client).Generation==1 && !rrInfo(Client).Moved && rrInfo(Client).Refreshes==1,"ASK refreshed shared map");
        bool Dirty=true;rrRequire(xredisClusterClientSnapshot(Client,&Maps[Cases],&Dirty)==XREDIS_CLUSTER_VALUE && !Dirty,"managed acquired real map");Generations[Cases]=rrInfo(Client).Generation;
        if(Stale){xbytesview Probe[]={XRT_BYTES_LITERAL("GET"),Reset[1]};xfuture* P=NULL;
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
            rrRequire(xredisClusterClientCallCommand(Client,Catalogue,Probe,2,XREDIS_CLUSTER_COMMAND_NO_SLOT,10000,&P)==XREDIS_CLUSTER_COMMAND_ACCEPTED,"metadata post-MOVED route");
#else
            rrRequire(xredisClusterClientCall(Client,Slot,Probe,2,10000,&P)==XREDIS_ADMISSION_ACCEPTED,"post-MOVED route");
#endif
            const xredisclusterclientresult* V=rrWait(P);rrRequire(V->Route.Query.Status==XREDIS_REQUEST_OK && V->Route.Attempts==1 && !V->Route.Moved && V->Generation>=2,"new shared map still routes through old owner");xrtFutureDestroy(P);}
        xbytesview Get[]={XRT_BYTES_LITERAL("GET"),Reset[1]};F=NULL;if(Stale)F=rrCall(Context.Nodes[1].Pool,Get,2);else rrRequire(xredisSharedPoolAskingCall(Context.Nodes[1].Pool,Get,2,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"verify actual final counter");const xredisrequestresult* Final=rrWait(F);xredisreplyvalue V=rrValue(Final->Reply);rrRequire(Final->Status==XREDIS_REQUEST_OK && V.Data.Size==2 && !memcmp(V.Data.Data,"32",2),"server counter proves no extra execution");xrtFutureDestroy(F);
        uint64 BeforeSeedFailure=rrInfo(Client).Generation;xrtAtomic32Store(&Context.DenySeed,1,XMEMORY_RELEASE);rrRequire(xredisClusterClientInvalidate(Client),"original seed denial demand");double SeedDue=xrtTimer()+15;
        while(rrInfo(Client).Generation<=BeforeSeedFailure || rrInfo(Client).Dirty){rrRequire(xrtTimer()<SeedDue,"metadata stranded at denied original seed");xrtSleep(1);}
        xredisclustermap* NewOrigin=NULL;bool SeedDirty=true;rrRequire(xredisClusterClientSnapshot(Client,&NewOrigin,&SeedDirty)==XREDIS_CLUSTER_VALUE && !SeedDirty,"new query origin map");xredisclustermapinfo NewInfo={0};NewInfo.Size=sizeof(NewInfo);rrRequire(xredisClusterMapInfo(NewOrigin,&NewInfo) && NewInfo.Origin.Port!=Ports[0] && xrtAtomic32Load(&Context.SeedDenied,XMEMORY_ACQUIRE)>0,"metadata did not use discovered authorized endpoint");xredisClusterMapRelease(NewOrigin);
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
        xredisCommandCatalogRelease(Catalogue);
#endif
        xfuture* ClientClosed=xredisClusterClientClosed(Client);rrRequire(xredisClusterClientClose(Client) && xrtFutureWaitFor(ClientClosed,15000)==XWAIT_OK && !rrInfo(Client).Requests && !rrInfo(Client).APIs && !rrInfo(Client).StorageBytes,"managed physical observer/post/timer close");xrtFutureDestroy(ClientClosed);xredisClusterClientRelease(Client);xrtAtomic32Store(&Context.DenySeed,0,XMEMORY_RELEASE);
        if(Stale){xbytesview Del[]={XRT_BYTES_LITERAL("DEL"),Reset[1],XRT_BYTES_LITERAL("xrt-owned:asking:{fixed}\0key")};rrAdmin(&Context,1,Del,3);rrOwner(&Context,SlotText,0,Ids);
            xbytesview Import[]={XRT_BYTES_LITERAL("CLUSTER"),XRT_BYTES_LITERAL("SETSLOT"),{(cbytes)SlotText,strlen(SlotText)},XRT_BYTES_LITERAL("IMPORTING"),{(cbytes)Ids[0],40}};rrAdmin(&Context,1,Import,5);
            xbytesview Migrate[]={XRT_BYTES_LITERAL("CLUSTER"),XRT_BYTES_LITERAL("SETSLOT"),{(cbytes)SlotText,strlen(SlotText)},XRT_BYTES_LITERAL("MIGRATING"),{(cbytes)Ids[1],40}};rrAdmin(&Context,0,Migrate,5);}
        xfuture* CacheClosed=xredisClusterNodesClosed(Context.Cache);xredisClusterNodesClose(Context.Cache);rrRequire(xrtFutureWaitFor(CacheClosed,15000)==XWAIT_OK,"cache physical close");xrtFutureDestroy(CacheClosed);
        xredisclusternodesinfo CI={0};CI.Size=sizeof(CI);rrRequire(xredisClusterNodesInfo(Context.Cache,&CI) && CI.State==XREDIS_CLUSTER_NODES_CLOSED && !CI.Nodes && !CI.Opening && !CI.APIs && CI.Created==3 && CI.Retired==3 && CI.Hits>=32,"cache bounded duties/counters");
        for(unsigned I=0;I<3;++I){rrnode* N=&Context.Nodes[I];xredispoolinfo PI={0};PI.Size=sizeof(PI);rrRequire(xredisPoolInfo(N->Pool,&PI) && PI.State==XREDIS_POOL_CLOSED && !PI.Requests && !PI.CommandBytes && !PI.Pending && !PI.Leased,"cached native quotas");xredisPoolRelease(N->Pool);rrRequire(xrtAtomic32Load(&N->Drops,XMEMORY_ACQUIRE)==1 && xrtAtomic32Load(&N->Calls,XMEMORY_ACQUIRE)==1,"one cached generation/Drop");}
        xredisClusterNodesRelease(Context.Cache);
        ++Cases;
    }
    rrRequire(Cases==36 && Retained==144 && xrtNetResolverDestroy(Resolver) && xrtNetEngineStop(E) && xrtNetEngineDestroy(E),"all native owners joined");
    for(size_t I=0;I<Retained;++I){const xredisclusterclientresult* R=rrWait(Saved[I]);rrRequire(R->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && R->Route.Query.Status==XREDIS_REQUEST_OK && rrValue(R->Route.Query.Reply).Integer==Expected[I],"retained managed native result");xrtFutureDestroy(Saved[I]);}
    for(size_t I=0;I<Cases;++I){xredisclustermapinfo M={0};M.Size=sizeof(M);rrRequire(xredisClusterMapInfo(Maps[I],&M) && M.Generation==Generations[I],"retained managed map after engine teardown");xredisClusterMapRelease(Maps[I]);}
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
    puts("[PASS] metadata command routing: actual COMMAND INFO, complete key preflight, explicit keyless policy and unchanged native duties");
#endif
    puts("[PASS] real Cluster client: 36 SHARDS/SLOTS numeric/DNS/prepared RESP2/3/AUTO migrating ASK/actual-owner-change MOVED cases; automatic bootstrap/refresh, denied original seed with discovered endpoint recovery, four producers, exactly32 INCR executions, new-map direct routes and retained native outcomes");return 0;
}
