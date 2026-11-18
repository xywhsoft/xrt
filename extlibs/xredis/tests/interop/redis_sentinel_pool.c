#include <xredis/sentinel_pool.h>
#include <xredis/sentinel_events.h>
#include <xredis/dial.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(SPR_TLS)
#include <xredis/tls_auth.h>
#include "../../examples/tls_auth/material.h"
#endif
static void sprRequire(bool B,cstr T){if(!B){fprintf(stderr,"[FAIL] real Sentinel pool: %s\n",T);exit(1);}}
/* Three pool members, at most two discoveries and one optional subscriber. */
enum { SPR_MAX_OBSERVERS = 6 };
typedef struct sprcontext {
    uint16 Seed, Learned;
    unsigned Factory;
    xredisinitmode Mode;
    xnetresolver* Resolver;
    xfuture* Closed[SPR_MAX_OBSERVERS];
    size_t Count, Peak;
    xatomic32 Drops, LearnedDials;
#if defined(SPR_TLS)
    xredisclienttlsconfig TLS;
    xtlsclientauthconfigv1 Auth;
    xtlsverifier* Verifier;
    xtlscontext* Context;
    xtlsidentity* Identity;
    unsigned Version;
    bool BadPassword;
#endif
} sprcontext;
#if defined(SPR_TLS)
static void sprInitTLS(sprcontext* D,cstr CA,cstr Chain,cstr Key)
{
    size_t Size;
    bytes Pem=xredisExampleAuthRead(CA,65536,&Size);
    xx509store* Store=xrtX509StoreCreate();
    sprRequire(Pem && Store && xrtX509StoreAddData(Store,(xbytesview){Pem,Size},NULL),"load fixture CA");
    xrtFree(Pem);
    xtlsverifierconfig Verify;
    xrtTlsVerifierConfigInit(&Verify);Verify.Store=Store;
    xtlsverifylimits Limits;xrtTlsVerifyLimitsInit(&Limits);
    D->Verifier=xrtTlsVerifierCreateLimited(&Verify,&Limits);
    xrtX509StoreFree(Store);

    xtlsversion Version=D->Version==12?XTLS_VERSION_12:XTLS_VERSION_13;
    xtlscipher Cipher=D->Version==12?XTLS_ECDHE_RSA_AES_128_GCM_SHA256:XTLS_AES_128_GCM_SHA256;
    xtlssignature Signature=XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256;
    xtlspolicy Policy;xrtTlsPolicyInit(&Policy);
    Policy.Versions=&Version;Policy.VersionCount=1;
    Policy.Ciphers=&Cipher;Policy.CipherCount=1;
    Policy.Signatures=&Signature;Policy.SignatureCount=1;
    xtlscontextconfig Config;xrtTlsContextConfigInit(&Config);Config.Policy=&Policy;
    D->Context=xrtTlsContextCreate(&Config);
    D->Identity=xredisExampleAuthIdentity(Chain,Key);
    sprRequire(D->Verifier && D->Context && D->Identity,"fixture TLS policy and client identity");
    xredisClientTlsConfigInit(&D->TLS);
    D->TLS.Session.Context=D->Context;D->TLS.Session.Verifier=D->Verifier;
    D->TLS.Session.VerifyName=(xstrview)XRT_STR_LITERAL("localhost");
    D->TLS.Dial.Family=XNET_FAMILY_IPV4;
    xrtTlsClientAuthConfigInitV1(&D->Auth);
    D->Auth.Identity=D->Identity;D->Auth.RequireCertificate=true;
}
static bool sprConnectTLS(xnetengine* E,const xnetaddr* Address,xredisclientinitconfig* I,sprcontext* D,xredisclient** C)
{
    bool Seed=Address->Port==D->Seed || Address->Port==D->Learned;
    I->Flags=XREDIS_INIT_AUTH|XREDIS_INIT_USERNAME;
    I->Username=Seed?(xbytesview)XRT_BYTES_LITERAL("xredis-seed"):(xbytesview)XRT_BYTES_LITERAL("xredis-data");
    I->Password=Seed?(xbytesview)XRT_BYTES_LITERAL("fixture-seed-pass"):(xbytesview)XRT_BYTES_LITERAL("fixture-data-pass");
    if(D->BadPassword)I->Password=(xbytesview)XRT_BYTES_LITERAL("fixture-rejected-pass");
    if(D->Factory==0)return xredisClientTlsAuthConnectV1(E,Address,0,NULL,I,&D->TLS,&D->Auth,C);
    if(D->Factory==1)return xredisClientTlsAuthDialV1(E,D->Resolver,XRT_STR_LITERAL("localhost"),Address->Port,0,NULL,I,&D->TLS,&D->Auth,C);
    xnetaddrlist* Endpoints=xrtNetAddrListCreate(Address,1);
    if(!Endpoints)return false;
    bool OK=xredisClientTlsAuthDialAddressesV1(E,Endpoints,0,NULL,I,&D->TLS,&D->Auth,C);
    xrtNetAddrListDestroy(Endpoints);
    return OK;
}
#endif
static void sprReap(sprcontext* D)
{
    size_t Live=0;
    for(size_t N=0;N<D->Count;++N){
        xfuture* F=D->Closed[N];
        if(xrtFutureState(F)==XFUTURE_RESOLVED)xrtFutureDestroy(F);
        else D->Closed[Live++]=F;
    }
    D->Count=Live;
}
static xredissentinelstatus sprConnect(xnetengine* E,uint16 Port,int64 Timeout,sprcontext* D,xredisclient** C)
{
    sprReap(D);
    xredisclientinitconfig I;xredisClientInitConfigInit(&I);I.Mode=D->Mode;I.Timeout=Timeout;xredisclientdialconfig Dial;xredisClientDialConfigInit(&Dial);Dial.Family=XNET_FAMILY_IPV4;
    xnetaddr A;sprRequire(xrtNetAddrParse(&A,"127.0.0.1",Port),"announced numeric endpoint");bool OK;
#if defined(SPR_TLS)
    if(D->Version)OK=sprConnectTLS(E,&A,&I,D,C);
    else
#endif
    if(D->Factory==1)OK=xredisClientDial(E,D->Resolver,XRT_STR_LITERAL("localhost"),Port,0,NULL,&I,&Dial,C);
    else if(D->Factory==2){xnetaddrlist* L=xrtNetAddrListCreate(&A,1);if(!L)return XREDIS_SENTINEL_MEMORY;OK=xredisClientDialAddresses(E,L,0,NULL,&I,&Dial,C);xrtNetAddrListDestroy(L);}
    else OK=xredisClientConnectInit(E,&A,0,NULL,&I,C);
    if(!OK)return XREDIS_SENTINEL_MEMORY;
    sprRequire(D->Count<SPR_MAX_OBSERVERS,"bounded live fixture observers");
    xfuture* Closed=xredisClientClosed(*C);
    sprRequire(Closed!=NULL,"retained native close observer");
    D->Closed[D->Count++]=Closed;
    if(D->Peak<D->Count)D->Peak=D->Count;
    return XREDIS_SENTINEL_OK;
}
static xredissentinelstatus sprSeed(xnetengine* E,size_t Seed,int64 Timeout,ptr Data,xredisclient** C)
{sprRequire(Seed==0,"seed index");sprcontext* D=Data;return sprConnect(E,D->Seed,Timeout,D,C);}
static xredissentinelstatus sprMaster(xnetengine* E,const xredissentinelendpoint* A,int64 Timeout,ptr Data,xredisclient** C)
{sprRequire(A->Host.Size==9 && !memcmp(A->Host.Data,"127.0.0.1",9),"announced host");return sprConnect(E,A->Port,Timeout,Data,C);}
static xredissentinelstatus sprEndpointSeed(xnetengine* E,const xredissentinelendpoint* A,int64 Timeout,ptr Data,xredisclient** C)
{
    sprcontext* D=Data;
    sprRequire(A->Host.Size==9 && !memcmp(A->Host.Data,"127.0.0.1",9),"owned announced Sentinel host");
    sprRequire(A->Port==D->Seed || A->Port==D->Learned,"owned announced Sentinel port");
    if(A->Port==D->Learned)xrtAtomic32FetchAdd(&D->LearnedDials,1,XMEMORY_RELEASE);
    return sprConnect(E,A->Port,Timeout,D,C);
}
static void sprDrop(ptr Data)
{sprcontext* D=Data;for(size_t N=0;N<D->Count;++N){sprRequire(xrtFutureState(D->Closed[N])==XFUTURE_RESOLVED,"context drop after every seed/master closes");xrtFutureDestroy(D->Closed[N]);D->Closed[N]=NULL;}xrtAtomic32FetchAdd(&D->Drops,1,XMEMORY_RELEASE);}
static const void* sprWait(xfuture* F)
{sprRequire(F && xrtFutureWaitFor(F,30000)==XWAIT_OK && xrtFutureValue(F),"typed future deadline");return xrtFutureValue(F);}
#if defined(SPR_TLS)
static void sprRejectPasswords(xnetengine* E,uint16 DataPort,sprcontext* D)
{
    uint16 Ports[]={D->Seed,DataPort};
    D->BadPassword=true;
    for(size_t I=0;I<2;++I){
        xredisclient* Client=NULL;
        sprRequire(sprConnect(E,Ports[I],5000,D,&Client)==XREDIS_SENTINEL_OK,"native denied connection admitted");
        xfuture* Ready=xredisClientReady(Client);
        const xredisreadyresult* R=sprWait(Ready);
        sprRequire(R->Status==XREDIS_READY_AUTH_FAILED && !R->Downgraded && !R->Hello,"native rejected password");
        xfuture* Closed=xredisClientClosed(Client);
        sprRequire(xredisClientClose(Client) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"denied connection retired");
        xrtFutureDestroy(Ready);xrtFutureDestroy(Closed);xredisClientRelease(Client);
    }
    D->BadPassword=false;
}
#endif
static xfuture* sprAcquire(xredispool* P)
{xfuture* F=NULL;sprRequire(xredisPoolAcquireAsync(P,20000,&F)==XREDIS_ADMISSION_ACCEPTED,"native acquire admission");return F;}
static xredislease* sprLease(xfuture* F)
{const xredisleaseresult* R=sprWait(F);sprRequire(R->Status==XREDIS_REQUEST_OK && R->Lease,"verified native lease");return R->Lease;}
static xfuture* sprCall(xredislease* L,const xbytesview* A,size_t N)
{xfuture* F=NULL;sprRequire(xredisLeaseCallAsync(L,A,N,30000,&F)==XREDIS_ADMISSION_ACCEPTED,"native business admission");return F;}
static xredisreplyvalue sprValue(xfuture* F)
{const xredisrequestresult* R=sprWait(F);sprRequire(R->Status==XREDIS_REQUEST_OK && R->Delivery==XREDIS_REPLIED,"native business result");xredisreplyinfo I={0};I.Size=sizeof(I);xredisReplyInfo(R->Reply,&I);xredisreplyvalue V;xredisReplyGet(R->Reply,I.Root,&V);return V;}
static bool sprContains(xbytesview V,cstr Text)
{size_t N=strlen(Text);for(size_t I=0;I+N<=V.Size;++I)if(!memcmp(V.Data+I,Text,N))return true;return false;}
static xfuture* sprIdentity(xredislease* L,cstr Run)
{xbytesview A[]={XRT_BYTES_LITERAL("INFO"),XRT_BYTES_LITERAL("SERVER")};xfuture* F=sprCall(L,A,2);xredisreplyvalue V=sprValue(F);char Needle[60];snprintf(Needle,sizeof(Needle),"run_id:%s",Run);sprRequire(sprContains(V.Data,Needle),"owned member run identity");return F;}
static void sprClose(xredispool* P,xnetengine* E,sprcontext* D)
{
    xfuture* Closed=xredisPoolClosed(P);
    sprRequire(xredisPoolClose(P) && xrtFutureWaitFor(Closed,30000)==XWAIT_OK &&
        xrtAtomic32Load(&D->Drops,XMEMORY_ACQUIRE)==1,"physical pool close/context drop");
    xrtFutureDestroy(Closed);
    sprRequire(xrtNetResolverDestroy(D->Resolver) && xrtNetEngineStop(E) && xrtNetEngineDestroy(E),"physical owners joined");
#if defined(SPR_TLS)
    if(D->Version){
        xrtTlsVerifierRelease(D->Verifier);
        xrtTlsContextRelease(D->Context);
        xrtTlsIdentityRelease(D->Identity);
        printf("TLS 1.%u mutual verification and two native ACL rejections\n",D->Version-10);
    }
#endif
}
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_EVENTS)
#include "sentinel_load_support.h"
#endif
static void sprSeedRecovery(xredispool* P,xnetengine* E,sprcontext* D,xredislease** L,cstr Run,uint16 Port,bool Replicas)
{
    xredissentinelpoolseedinfo Seeds={.Size=sizeof(Seeds)};
    sprRequire(xredisSentinelPoolSeedInfo(P,&Seeds) && Seeds.Configured==1 && Seeds.Known==2 &&
        Seeds.Capacity==2 && !xrtAtomic32Load(&D->LearnedDials,XMEMORY_ACQUIRE),"alternate learned before its first dial");
    xfuture* Waiting=sprAcquire(P);
    sprRequire(!xrtFutureDone(Waiting),"all slots held during seed loss");
    puts("READY");fflush(stdout);
    char Go[16];
    sprRequire(fgets(Go,sizeof(Go),stdin) && !strcmp(Go,"GO\n"),"initial Sentinel has physically stopped");
    sprRequire(xredisLeaseAbort(L[0]),"remove one member after Sentinel loss");
    xredislease* Repaired=sprLease(Waiting);
    xrtFutureDestroy(sprIdentity(Repaired,Run));
    xrtFutureDestroy(sprIdentity(L[1],Run));
    xrtFutureDestroy(sprIdentity(L[2],Run));
    xredissentinelpoolinfo I={.Size=sizeof(I)};
    sprRequire(xredisSentinelPoolInfo(P,&I) && I.Generation==1 && !I.Changes && I.Port==Port &&
        I.PreferredSeed==1 && I.Connect.Connected==4 && xrtAtomic32Load(&D->LearnedDials,XMEMORY_ACQUIRE)==1,
        "learned Sentinel repaired the missing slot on the existing generation");
    sprClose(P,E,D);
    sprRequire(xredisSentinelPoolSeedInfo(P,&Seeds) && Seeds.Configured==1 && Seeds.Known==2,"closed seed counts retained");
    xrtFutureDestroy(Waiting);
    printf("[PASS] real Sentinel seed recovery role=%s learned=1 preferred=1 generation=1 observers_peak=%zu\n",
        Replicas?"replica":"master",D->Peak);
}
int main(int Argc,char** Argv)
{
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_EVENTS)
    redisopenconfig Load={0};
    if(Argc>=6 && !strcmp(Argv[Argc-5],"load")){
        if(!sprLoadConfig(Argv+Argc-4,&Load))return 2;
        Argc-=5;
    }
#endif
    bool Learn=Argc==10 && (!strcmp(Argv[8],"seed-master") || !strcmp(Argv[8],"seed-replica"));
    bool Secure=Argc==13;
    bool Listen=(Argc==9 || Secure) && (!strcmp(Argv[8],"events-master") || !strcmp(Argv[8],"events-replica"));
    bool Replicas=(Argc==9&&!strcmp(Argv[8],"replica")) || (Learn&&!strcmp(Argv[8],"seed-replica")) ||
        (Listen&&!strcmp(Argv[8],"events-replica"));
    bool Refresh=Argc==9&&!strcmp(Argv[8],"refresh");
#if !defined(XREDIS_FEATURE_XREDIS_SENTINEL_EVENTS)
    if(Listen)return 2;
#else
    if(Load.Count && !Listen)return 2;
#endif
    if((Argc!=8 && !Learn && !Listen && !(Argc==9 && (Replicas || Refresh))) || strlen(Argv[3])!=40 || strlen(Argv[4])!=40)return 2;
#if !defined(SPR_TLS)
    if(Secure)return 2;
#endif
    unsigned long Seed=strtoul(Argv[1],NULL,10),Next=strtoul(Argv[2],NULL,10);sprcontext D={0};D.Seed=(uint16)Seed;D.Factory=(unsigned)strtoul(Argv[5],NULL,10);D.Mode=(xredisinitmode)strtol(Argv[6],NULL,10);size_t Count=(size_t)strtoul(Argv[7],NULL,10);if(!Seed || Seed>65535 || !Next || Next>65535 || D.Factory>2 || Count!=3)return 2;
    if(Learn){
        unsigned long Alternate=strtoul(Argv[9],NULL,10);
        if(!Alternate || Alternate>65535 || Alternate==Seed)return 2;
        D.Learned=(uint16)Alternate;
    }
    xnetengineconfig Ecfg;xrtNetEngineConfigInit(&Ecfg);Ecfg.Workers=2;xnetengine* E=xrtNetEngineCreate(&Ecfg);sprRequire(E && xrtNetEngineStart(E),"engine start");D.Resolver=xrtNetResolverCreate(NULL);sprRequire(D.Resolver!=NULL,"resolver");
#if defined(SPR_TLS)
    if(Secure){
        D.Version=(unsigned)strtoul(Argv[12],NULL,10);
        sprRequire(D.Version==12 || D.Version==13,"supported TLS version");
        sprInitTLS(&D,Argv[9],Argv[10],Argv[11]);
        sprRejectPasswords(E,(uint16)Next,&D);
    }
#endif
    xredissentinelpoolconfig K;xredisSentinelPoolConfigInit(&K);K.Discovery.Service=(xbytesview)XRT_BYTES_LITERAL("cache");K.Discovery.ConnectSeed=sprSeed;K.Discovery.ConnectMaster=sprMaster;K.Discovery.Data=&D;K.Discovery.Protocol=D.Mode==XREDIS_INIT_RESP2?XREDIS_RESP2:XREDIS_RESP3;K.Discovery.SeedTimeout=2000;K.MaxConnecting=2;K.RetryMin=10;K.RetryMax=30;K.Drop=sprDrop;xredispoolconfig O;xredisPoolConfigInit(&O);O.WorkItems=1;O.LeaseTimeout=60000;
    xredispool* P=NULL;
    xredisadmission Pool;
    if(Learn){
        xredissentinelendpoint SeedAddress={XRT_BYTES_LITERAL("127.0.0.1"),D.Seed};
        xredissentinelpoolseedconfig Seeds;
        sprRequire(xredisSentinelPoolSeedConfigInit(&Seeds),"init Sentinel seed config");
        Seeds.Seeds=&SeedAddress;Seeds.Connect=sprEndpointSeed;Seeds.MaxSeeds=2;Seeds.PeerTimeout=500;
        K.Discovery.ConnectSeed=NULL;K.MaxConnecting=1;
        Pool=Replicas?xredisSentinelReplicaPoolCreateWithSeeds(E,1,Count,&O,&K,&Seeds,sprMaster,8,&P):
            xredisSentinelPoolCreateWithSeeds(E,1,Count,&O,&K,&Seeds,&P);
    }else Pool=Replicas?xredisSentinelReplicaPoolCreate(E,1,Count,&O,&K,sprMaster,8,&P):
        xredisSentinelPoolCreate(E,1,Count,&O,&K,&P);
    sprRequire(Pool==XREDIS_ADMISSION_ACCEPTED,"pool create");xfuture* Acquired[3];xredislease* L[3];for(size_t N=0;N<Count;++N){Acquired[N]=sprAcquire(P);L[N]=sprLease(Acquired[N]);xfuture* ID=sprIdentity(L[N],Argv[3]);xrtFutureDestroy(ID);}
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_EVENTS)
    uint64 PreviousHints=0;
    if(Listen){
        xredissentineleventconfig C;
        sprRequire(xredisSentinelEventConfigInit(&C),"initialize native listener");
        sprRequire(xredisSentinelPoolListen(P,&C)==XREDIS_ADMISSION_ACCEPTED,"enable native listener");
        xredissentineleventinfo Snapshot={.Size=sizeof(Snapshot)};
        double Due=xrtTimer()+10;
        for(;;){
            sprRequire(xredisSentinelPoolEventInfo(P,&Snapshot),"listener info");
            if(Snapshot.Session.State==XREDIS_SESSION_ONLINE && Snapshot.Hints)break;
            sprRequire(xrtTimer()<Due,"listener acknowledge and initial recheck");
            xrtSleep(1);
        }
        PreviousHints=Snapshot.Hints;
    }
    if(Load.Count){
        for(size_t N=0;N<Count;++N){
            xfuture* Returned=xredisLeaseClosed(L[N]);
            sprRequire(xredisLeaseReturn(L[N]),"release initial load lease");
            const xredisleaseclosedresult* Result=sprWait(Returned);
            sprRequire(Result->Reusable,"initial load lease physically returned");
            xrtFutureDestroy(Returned);
            xrtFutureDestroy(Acquired[N]);
        }
        bool Switch=strcmp(Argv[3],Argv[4])!=0;
        sprLoad(P,&Load,Argv[4],(uint16)Next,Count,Switch,PreviousHints);
        sprClose(P,E,&D);
        xredisPoolRelease(P);
        puts("[PASS] fixed-arrival Sentinel pool load");
        return 0;
    }
#endif
    if(Learn){
        sprSeedRecovery(P,E,&D,L,Argv[3],(uint16)Next,Replicas);
        for(size_t N=0;N<Count;++N)xrtFutureDestroy(Acquired[N]);
        xredisPoolRelease(P);
        return 0;
    }
    const xbytesview Block[]={XRT_BYTES_LITERAL("BLPOP"),XRT_BYTES_LITERAL("xredis-owned-failover-empty"),XRT_BYTES_LITERAL("0")};
    const xbytesview Read[]={XRT_BYTES_LITERAL("XREAD"),XRT_BYTES_LITERAL("BLOCK"),XRT_BYTES_LITERAL("0"),
        XRT_BYTES_LITERAL("STREAMS"),XRT_BYTES_LITERAL("xredis-owned-failover-empty"),XRT_BYTES_LITERAL("$")};
    const char* Command=Replicas?"cmd=xread":"cmd=blpop";
    xfuture* Old=Replicas?sprCall(L[0],Read,6):sprCall(L[0],Block,3);
    xbytesview Clients[]={XRT_BYTES_LITERAL("CLIENT"),XRT_BYTES_LITERAL("LIST")};double Limit=xrtTimer()+5;
    for(;;){xfuture* F=sprCall(L[1],Clients,2);xredisreplyvalue V=sprValue(F);bool Sent=sprContains(V.Data,Command);xrtFutureDestroy(F);if(Sent)break;sprRequire(xrtTimer()<Limit,"old blocking command reached owned server");xrtSleep(1);}
    xfuture* Waiting=sprAcquire(P);sprRequire(!xrtFutureDone(Waiting),"all old leases held while new acquisition waits");puts("READY");fflush(stdout);char Go[16];sprRequire(fgets(Go,sizeof(Go),stdin) && !strcmp(Go,"GO\n"),"owned failover release");
    if(Refresh){
        xfuture* Live=sprIdentity(L[1],Argv[3]);xrtFutureDestroy(Live);
        sprRequire(!xrtFutureDone(Old) && !xrtFutureDone(Waiting),"old connection and queue still live before refresh");
        sprRequire(xredisSentinelPoolRefresh(P)==XREDIS_ADMISSION_ACCEPTED,"refresh requested");
    }
    xredislease* New=sprLease(Waiting);xfuture* Current=sprIdentity(New,Argv[4]);const xredisrequestresult* OldResult=sprWait(Old);sprRequire(OldResult->Status!=XREDIS_REQUEST_OK && OldResult->Delivery==XREDIS_MAY_HAVE_EXECUTED,"old sent operation retains uncertainty");xfuture* List=sprCall(New,Clients,2);sprRequire(!sprContains(sprValue(List).Data,Command),"no old blocking command replayed on selected node");xrtFutureDestroy(List);
    xredissentinelpoolinfo I={0};I.Size=sizeof(I);sprRequire(xredisSentinelPoolInfo(P,&I) && I.Generation==2 && I.Changes==1 && I.Port==(uint16)Next,"whole verified generation replaced");
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_EVENTS)
    if(Listen){
        xredissentineleventinfo Snapshot={.Size=sizeof(Snapshot)};
        double Due=xrtTimer()+10;
        for(;;){
            sprRequire(xredisSentinelPoolEventInfo(P,&Snapshot),"event observation");
            if(Snapshot.Matched && Snapshot.Hints>PreviousHints)break;
            sprRequire(xrtTimer()<Due,"real +switch-master event reached the listener");
            xrtSleep(1);
        }
        printf("event matched=%llu hints=%llu\n",(unsigned long long)Snapshot.Matched,(unsigned long long)Snapshot.Hints);
    }
#endif
    sprClose(P,E,&D);sprRequire(sprValue(Current).Data.Size>0 && ((const xredisrequestresult*)sprWait(Old))->Delivery==XREDIS_MAY_HAVE_EXECUTED && xredisSentinelPoolInfo(P,&I) && I.Phase==XREDIS_SENTINEL_POOL_CLOSED,"retained outcomes after engine teardown");xredisPoolRelease(P);for(size_t N=0;N<Count;++N)xrtFutureDestroy(Acquired[N]);xrtFutureDestroy(Waiting);xrtFutureDestroy(Old);xrtFutureDestroy(Current);printf("[PASS] automatic Sentinel %s pool: %s, native queued lease, no replay, retained uncertainty and physical close; observers_peak=%zu\n",Listen?(Replicas?"events-replica":"events-master"):Replicas?"replica":Refresh?"refresh":"master",
        Refresh?"live Sentinel remap":"real failover",D.Peak);return 0;
}
