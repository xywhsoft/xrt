#include <xredis/sentinel.h>
#include <xredis/dial.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static void sentinelRealRequire(bool V,cstr Text){if(!V){fprintf(stderr,"[FAIL] real Sentinel: %s\n",Text);exit(1);}}
typedef struct sentinelreal {uint16 Seed;unsigned Factory; xredisinitmode Mode;xnetresolver* Resolver;} sentinelreal;
static xredissentinelstatus sentinelRealConnect(xnetengine* E,uint16 Port,int64 Timeout,sentinelreal* D,xredisclient** C)
{
    xredisclientdialconfig Dial;xredisClientDialConfigInit(&Dial);Dial.Family=XNET_FAMILY_IPV4;
    xredisclientinitconfig I;xredisClientInitConfigInit(&I);I.Mode=D->Mode;I.Timeout=Timeout;xnetaddr A;sentinelRealRequire(xrtNetAddrParse(&A,"127.0.0.1",Port),"numeric endpoint");bool OK;
    if(D->Factory==1)OK=xredisClientDial(E,D->Resolver,XRT_STR_LITERAL("localhost"),Port,0,NULL,&I,&Dial,C);
    else if(D->Factory==2){xnetaddrlist* L=xrtNetAddrListCreate(&A,1);if(!L)return XREDIS_SENTINEL_MEMORY;OK=xredisClientDialAddresses(E,L,0,NULL,&I,&Dial,C);xrtNetAddrListDestroy(L);}
    else OK=xredisClientConnectInit(E,&A,0,NULL,&I,C);
    return OK?XREDIS_SENTINEL_OK:XREDIS_SENTINEL_MEMORY;
}
static xredissentinelstatus sentinelRealSeed(xnetengine* E,size_t Seed,int64 Timeout,ptr Data,xredisclient** C)
{sentinelreal* D=Data;sentinelRealRequire(Seed==0,"one real seed");return sentinelRealConnect(E,D->Seed,Timeout,D,C);}
static xredissentinelstatus sentinelRealMaster(xnetengine* E,const xredissentinelendpoint* A,int64 Timeout,ptr Data,xredisclient** C)
{sentinelRealRequire(A->Host.Size==9 && !memcmp(A->Host.Data,"127.0.0.1",9),"actual Sentinel announced host");return sentinelRealConnect(E,A->Port,Timeout,Data,C);}
static const void* sentinelRealWait(xfuture* F)
{sentinelRealRequire(F && xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureValue(F),"native typed future");return xrtFutureValue(F);}
int main(int Argc,char** Argv)
{
    if(Argc!=8)return 2;
    unsigned long Port=strtoul(Argv[1],NULL,10),Master=strtoul(Argv[2],NULL,10);sentinelreal Data={(uint16)Port,(unsigned)strtoul(Argv[5],NULL,10),(xredisinitmode)strtol(Argv[6],NULL,10),NULL};unsigned Expected=(unsigned)strtoul(Argv[7],NULL,10);
    if(!Port || Port>65535 || Master>65535 || Data.Factory>2 || strlen(Argv[3])!=40)return 2;
    xnetengineconfig Engine;xrtNetEngineConfigInit(&Engine);Engine.Workers=2;xnetengine* E=xrtNetEngineCreate(&Engine);sentinelRealRequire(E && xrtNetEngineStart(E),"engine start");Data.Resolver=xrtNetResolverCreate(NULL);sentinelRealRequire(Data.Resolver!=NULL,"resolver");
    xredissentinelconfig K;xredisSentinelConfigInit(&K);K.Service=(xbytesview){(cbytes)Argv[4],strlen(Argv[4])};K.ConnectSeed=sentinelRealSeed;K.ConnectMaster=sentinelRealMaster;K.Data=&Data;K.Protocol=Data.Mode==XREDIS_INIT_RESP2?XREDIS_RESP2:XREDIS_RESP3;K.SeedTimeout=2000;K.Timeout=5000;K.MaxRounds=2;
    xredissentineldiscovery* D=NULL;sentinelRealRequire(xredisSentinelDiscoveryCreate(E,&K,&D)==XREDIS_ADMISSION_ACCEPTED,"discovery create");double Limit=xrtTimer()+12;
    for(;;){sentinelRealRequire(xrtTimer()<Limit,"drive deadline");xredissentinelstep S=xredisSentinelDiscoveryStep(D);sentinelRealRequire(S!=XREDIS_SENTINEL_STEP_ERROR,"Step");if(S==XREDIS_SENTINEL_DONE)break;if(S==XREDIS_SENTINEL_WAIT){int64 Ms=(int64)ceil((xredisSentinelDiscoveryNextWake(D)-xrtTimer())*1000);if(Ms<1)Ms=1;if(Ms>20)Ms=20;xfuture* F=xredisSentinelDiscoveryPending(D);if(F){(void)xrtFutureWaitFor(F,Ms);xrtFutureDestroy(F);}else xrtSleep(Ms);}}
    xredissentinelresult R={0};R.Size=sizeof(R);sentinelRealRequire(xredisSentinelDiscoveryResult(D,&R) && R.Status==(xredissentinelstatus)Expected,"expected discovery result");
    if(Expected==XREDIS_SENTINEL_OK){sentinelRealRequire(R.Endpoint.Port==Master && R.SeedAttempts==1 && R.MasterAttempts==1,"announced and verified master");xredisclient* C=NULL;sentinelRealRequire(xredisSentinelDiscoveryTakeClient(D,&C),"native master transfer");xredisSentinelDiscoveryDestroy(D);D=NULL;
        xbytesview Args[]={XRT_BYTES_LITERAL("INFO"),XRT_BYTES_LITERAL("SERVER")};xfuture* F=NULL;sentinelRealRequire(xredisClientCallAsync(C,Args,2,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"native business call");const xredisrequestresult* B=sentinelRealWait(F);sentinelRealRequire(B->Status==XREDIS_REQUEST_OK && B->Delivery==XREDIS_REPLIED,"native business success");xredisreplyinfo Info={0};Info.Size=sizeof(Info);xredisReplyInfo(B->Reply,&Info);xredisreplyvalue V;xredisReplyGet(B->Reply,Info.Root,&V);bool Match=false;for(size_t I=0;I+47<=V.Data.Size;++I)if(!memcmp(V.Data.Data+I,"run_id:",7) && !memcmp(V.Data.Data+I+7,Argv[3],40)){Match=true;break;}sentinelRealRequire(Match,"transferred connection reaches owned master identity");
        xfuture* Closed=xredisClientClosed(C);xredisClientClose(C);sentinelRealRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"master physical close");xrtFutureDestroy(Closed);xredisClientRelease(C);sentinelRealRequire(xrtNetResolverDestroy(Data.Resolver) && xrtNetEngineStop(E) && xrtNetEngineDestroy(E),"physical owners joined");sentinelRealRequire(((const xredisrequestresult*)sentinelRealWait(F))->Status==XREDIS_REQUEST_OK,"result retained after engine");xrtFutureDestroy(F);
    }else {sentinelRealRequire(R.MasterAttempts==0 && R.UnknownReplies==1,"unknown service never contacts a master");xredisSentinelDiscoveryDestroy(D);sentinelRealRequire(xrtNetResolverDestroy(Data.Resolver) && xrtNetEngineStop(E) && xrtNetEngineDestroy(E),"unknown physical owners joined");}
    printf("[PASS] real Sentinel discovery status=%u factory=%u protocol=%u target=%lu\n",Expected,Data.Factory,(unsigned)Data.Mode,Master);return 0;
}
