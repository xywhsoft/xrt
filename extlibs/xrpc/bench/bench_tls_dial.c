#include <xrpc/tls_dial.h>
#include <xrt/memory_stats.h>
#include "../examples/common/tls_service.h"
#include "process_cpu.h"
static int tlsDialBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):100;unsigned Mode=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):2;bool Heap=Argc>3 && strtoul(Argv[3],NULL,10);
    if(!Count || Count>100000 || Mode>2 || Argc>4)return 1;
    double* Latency=malloc(Count*sizeof(double));if(!Latency)return 2;rpcTlsService S;rpcTlsServiceStart(&S,1,false,false,false);xtlsclientconfig Client;rpcTlsServiceClient(&S,&Client);
    xnetresolverconfig C;xrtNetResolverConfigInit(&C);C.Workers=1;C.CacheEntries=0;xnetresolver* Resolver=Mode==2?xrtNetResolverCreate(&C):NULL;
    rpcTlsServiceRequire(Mode!=2 || Resolver!=NULL,"cold host Resolver");xnetaddrlist* Addresses=Mode==1?xrtNetAddrListCreate(&S.Address,1):NULL;rpcTlsServiceRequire(Mode!=1 || Addresses,"cold prepared endpoint");
    xrpctlsdialconfig D;xrpcTlsDialConfigInit(&D);D.Family=XNET_FAMILY_IPV4;double CPUStart,CPUEnd;rpcTlsServiceRequire(rpcBenchProcessCpu(&CPUStart),"CPU clock");
    xrtMemStatsEnable(Heap);xmemstats Before,After;xrtMemStatsGet(&Before);double Began=xrtTimer();
    for(size_t I=0;I<Count;++I){double Start=xrtTimer();xrpctls* Peer=NULL;
        bool OK=Mode==0?xrpcTlsConnect(S.Engine,&S.Address,1,&Client,&S.Config,&Peer):Mode==1?xrpcTlsDialAddresses(S.Engine,Addresses,1,&Client,&S.Config,&D,&Peer):xrpcTlsDial(S.Engine,Resolver,XRT_STR_LITERAL("localhost"),S.Address.Port,1,&Client,&S.Config,&D,&Peer);
        rpcTlsServiceRequire(OK,"cold factory");xfuture* F=NULL;rpcTlsServiceRequire(xrpcTlsCall(Peer,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2,3]"),5000,&F)==XRPC_DUPLEX_ACCEPTED,"cold call");
        const xrpccallresult* R=rpcTlsServiceResult(F);xrpcresponseinfo Info={0};Info.Size=sizeof(Info);
        rpcTlsServiceRequire(R->Status==XRPC_CALL_OK && R->Delivery==XRPC_REPLIED && xrpcResponsesInfo(R->Responses,&Info) && Info.Messages[R->Index].Result.Raw.Size==7 && !memcmp(Info.Messages[R->Index].Result.Raw.Data,"[1,2,3]",7),"validate exact reply");
        xrtFutureDestroy(F);rpcTlsServiceCycleClose(&S,Peer);Latency[I]=(xrtTimer()-Start)*1e6;
    }
    double Seconds=xrtTimer()-Began;xrtMemStatsGet(&After);xrtMemStatsEnable(false);rpcTlsServiceRequire(rpcBenchProcessCpu(&CPUEnd),"CPU clock");
    if(Resolver){xnetresolverstats Stats;rpcTlsServiceRequire(xrtNetResolverStats(Resolver,&Stats) && Stats.Submitted==Count && !Stats.CacheHits,"one uncached app lookup per cycle");rpcTlsServiceRequire(xrtNetResolverDestroy(Resolver),"external Resolver join");}
    xrtNetAddrListDestroy(Addresses);rpcTlsServiceRequire(xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED)==Count,"operation replay/count");rpcTlsServiceStop(&S);qsort(Latency,Count,sizeof(double),tlsDialBenchCompare);
    printf("completed: %zu\ncycles_per_sec: %.9f\nprocess_cpu_ns_per_cycle: %.9f\np50_us: %.9f\np95_us: %.9f\np99_us: %.9f\np999_us: %.9f\n",Count,Count/Seconds,(CPUEnd-CPUStart)*1000./Count,Latency[(Count*50+99)/100-1],Latency[(Count*95+99)/100-1],Latency[(Count*99+99)/100-1],Latency[(Count*999+999)/1000-1]);
    if(Heap)printf("heap_blocks_per_cycle: %.9f\nheap_bytes_per_cycle: %.9f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
    printf("mode: %u\n",Mode);puts("scope: serial closed-loop IPv4 loopback numeric/prepared/OS localhost DNS, app cache off, explicit TLS13 fixture pin/full handshake and native record copies; factory/call/exact byte validation/result release/both physical peer retire+destroy included, callback return tails at final Engine join; host Engine/shared Server/Resolver/list construction and final joins excluded; all process-thread CPU, no fixed-arrival, external DNS, connection-scale, native Linux or production-tail acceptance");free(Latency);return 0;
}
