#include <xredis/sentinel_pool.h>
#include <xredis/dial.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sprRequire(bool B,cstr T){if(!B){fprintf(stderr,"[FAIL] real Sentinel pool: %s\n",T);exit(1);}}
typedef struct sprcontext {uint16 Seed;unsigned Factory;xredisinitmode Mode;xnetresolver* Resolver;xfuture* Closed[256];size_t Count;xatomic32 Drops;} sprcontext;
static xredissentinelstatus sprConnect(xnetengine* E,uint16 Port,int64 Timeout,sprcontext* D,xredisclient** C)
{
    xredisclientinitconfig I;xredisClientInitConfigInit(&I);I.Mode=D->Mode;I.Timeout=Timeout;xredisclientdialconfig Dial;xredisClientDialConfigInit(&Dial);Dial.Family=XNET_FAMILY_IPV4;
    xnetaddr A;sprRequire(xrtNetAddrParse(&A,"127.0.0.1",Port),"announced numeric endpoint");bool OK;
    if(D->Factory==1)OK=xredisClientDial(E,D->Resolver,XRT_STR_LITERAL("localhost"),Port,0,NULL,&I,&Dial,C);
    else if(D->Factory==2){xnetaddrlist* L=xrtNetAddrListCreate(&A,1);if(!L)return XREDIS_SENTINEL_MEMORY;OK=xredisClientDialAddresses(E,L,0,NULL,&I,&Dial,C);xrtNetAddrListDestroy(L);}
    else OK=xredisClientConnectInit(E,&A,0,NULL,&I,C);
    if(OK){sprRequire(D->Count<256,"bounded real fixture observers");D->Closed[D->Count++]=xredisClientClosed(*C);}return OK?XREDIS_SENTINEL_OK:XREDIS_SENTINEL_MEMORY;
}
static xredissentinelstatus sprSeed(xnetengine* E,size_t Seed,int64 Timeout,ptr Data,xredisclient** C)
{sprRequire(Seed==0,"seed index");sprcontext* D=Data;return sprConnect(E,D->Seed,Timeout,D,C);}
static xredissentinelstatus sprMaster(xnetengine* E,const xredissentinelendpoint* A,int64 Timeout,ptr Data,xredisclient** C)
{sprRequire(A->Host.Size==9 && !memcmp(A->Host.Data,"127.0.0.1",9),"announced host");return sprConnect(E,A->Port,Timeout,Data,C);}
static void sprDrop(ptr Data)
{sprcontext* D=Data;for(size_t N=0;N<D->Count;++N){sprRequire(xrtFutureState(D->Closed[N])==XFUTURE_RESOLVED,"context drop after every seed/master closes");xrtFutureDestroy(D->Closed[N]);D->Closed[N]=NULL;}xrtAtomic32FetchAdd(&D->Drops,1,XMEMORY_RELEASE);}
static const void* sprWait(xfuture* F)
{sprRequire(F && xrtFutureWaitFor(F,30000)==XWAIT_OK && xrtFutureValue(F),"typed future deadline");return xrtFutureValue(F);}
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
int main(int Argc,char** Argv)
{
    if(Argc!=8 || strlen(Argv[3])!=40 || strlen(Argv[4])!=40)return 2;
    unsigned long Seed=strtoul(Argv[1],NULL,10),Next=strtoul(Argv[2],NULL,10);sprcontext D={0};D.Seed=(uint16)Seed;D.Factory=(unsigned)strtoul(Argv[5],NULL,10);D.Mode=(xredisinitmode)strtol(Argv[6],NULL,10);size_t Count=(size_t)strtoul(Argv[7],NULL,10);if(!Seed || Seed>65535 || !Next || Next>65535 || D.Factory>2 || Count!=3)return 2;
    xnetengineconfig Ecfg;xrtNetEngineConfigInit(&Ecfg);Ecfg.Workers=2;xnetengine* E=xrtNetEngineCreate(&Ecfg);sprRequire(E && xrtNetEngineStart(E),"engine start");D.Resolver=xrtNetResolverCreate(NULL);sprRequire(D.Resolver!=NULL,"resolver");
    xredissentinelpoolconfig K;xredisSentinelPoolConfigInit(&K);K.Discovery.Service=(xbytesview)XRT_BYTES_LITERAL("cache");K.Discovery.ConnectSeed=sprSeed;K.Discovery.ConnectMaster=sprMaster;K.Discovery.Data=&D;K.Discovery.Protocol=D.Mode==XREDIS_INIT_RESP2?XREDIS_RESP2:XREDIS_RESP3;K.Discovery.SeedTimeout=2000;K.MaxConnecting=2;K.RetryMin=10;K.RetryMax=30;K.Drop=sprDrop;xredispoolconfig O;xredisPoolConfigInit(&O);O.WorkItems=1;O.LeaseTimeout=60000;
    xredispool* P=NULL;sprRequire(xredisSentinelPoolCreate(E,1,Count,&O,&K,&P)==XREDIS_ADMISSION_ACCEPTED,"pool create");xfuture* Acquired[3];xredislease* L[3];for(size_t N=0;N<Count;++N){Acquired[N]=sprAcquire(P);L[N]=sprLease(Acquired[N]);xfuture* ID=sprIdentity(L[N],Argv[3]);xrtFutureDestroy(ID);}
    xbytesview Block[]={XRT_BYTES_LITERAL("BLPOP"),XRT_BYTES_LITERAL("xredis-owned-failover-empty"),XRT_BYTES_LITERAL("0")};xfuture* Old=sprCall(L[0],Block,3);
    xbytesview Clients[]={XRT_BYTES_LITERAL("CLIENT"),XRT_BYTES_LITERAL("LIST")};double Limit=xrtTimer()+5;
    for(;;){xfuture* F=sprCall(L[1],Clients,2);xredisreplyvalue V=sprValue(F);bool Sent=sprContains(V.Data,"cmd=blpop");xrtFutureDestroy(F);if(Sent)break;sprRequire(xrtTimer()<Limit,"old blocking command reached owned server");xrtSleep(1);}
    xfuture* Waiting=sprAcquire(P);sprRequire(!xrtFutureDone(Waiting),"all old leases held while new acquisition waits");puts("READY");fflush(stdout);char Go[16];sprRequire(fgets(Go,sizeof(Go),stdin) && !strcmp(Go,"GO\n"),"owned failover release");
    xredislease* New=sprLease(Waiting);xfuture* Current=sprIdentity(New,Argv[4]);const xredisrequestresult* OldResult=sprWait(Old);sprRequire(OldResult->Status!=XREDIS_REQUEST_OK && OldResult->Delivery==XREDIS_MAY_HAVE_EXECUTED,"old sent operation retains uncertainty");xfuture* List=sprCall(New,Clients,2);sprRequire(!sprContains(sprValue(List).Data,"cmd=blpop"),"no old blocking command replayed on new master");xrtFutureDestroy(List);
    xredissentinelpoolinfo I={0};I.Size=sizeof(I);sprRequire(xredisSentinelPoolInfo(P,&I) && I.Generation==2 && I.Changes==1 && I.Port==(uint16)Next,"whole verified generation replaced");
    xfuture* Closed=xredisPoolClosed(P);sprRequire(xredisPoolClose(P) && xrtFutureWaitFor(Closed,30000)==XWAIT_OK && xrtAtomic32Load(&D.Drops,XMEMORY_ACQUIRE)==1,"physical pool close/context drop");xrtFutureDestroy(Closed);sprRequire(xrtNetResolverDestroy(D.Resolver) && xrtNetEngineStop(E) && xrtNetEngineDestroy(E),"physical owners joined");sprRequire(sprValue(Current).Data.Size>0 && ((const xredisrequestresult*)sprWait(Old))->Delivery==XREDIS_MAY_HAVE_EXECUTED && xredisSentinelPoolInfo(P,&I) && I.Phase==XREDIS_SENTINEL_POOL_CLOSED,"retained outcomes after engine teardown");xredisPoolRelease(P);for(size_t N=0;N<Count;++N)xrtFutureDestroy(Acquired[N]);xrtFutureDestroy(Waiting);xrtFutureDestroy(Old);xrtFutureDestroy(Current);puts("[PASS] automatic Sentinel pool: real failover, native queued lease, no replay, retained uncertainty and physical close");return 0;
}
