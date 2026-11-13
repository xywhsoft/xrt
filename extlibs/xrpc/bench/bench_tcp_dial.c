#include <xrpc/tcp_dial.h>
#include <xrt/memory_stats.h>
#include "../examples/common/tcp_service.h"
#include "process_cpu.h"
static int tcpDialBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):100;unsigned Mode=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):2;bool Heap=Argc>3 && strtoul(Argv[3],NULL,10);
    if(!Count || Count>100000 || Mode>2 || Argc>4)return 1;
    double* Latency=malloc(Count*sizeof(double));if(!Latency)return 2;rpcTcpService S;rpcTcpServiceStart(&S);
    xnetresolverconfig C;xrtNetResolverConfigInit(&C);C.Workers=1;C.CacheEntries=0;xnetresolver* Resolver=Mode==2?xrtNetResolverCreate(&C):NULL;
    rpcTcpServiceRequire(Mode!=2 || Resolver!=NULL,"cold host Resolver");xnetaddrlist* Addresses=Mode==1?xrtNetAddrListCreate(&S.Address,1):NULL;rpcTcpServiceRequire(Mode!=1 || Addresses,"cold prepared endpoint");
    xrpctcpdialconfig D;xrpcTcpDialConfigInit(&D);D.Family=XNET_FAMILY_IPV4;double CPUStart,CPUEnd;rpcTcpServiceRequire(rpcBenchProcessCpu(&CPUStart),"CPU clock");
    xrtMemStatsEnable(Heap);xmemstats Before,After;xrtMemStatsGet(&Before);double Began=xrtTimer();
    for(size_t I=0;I<Count;++I){double Start=xrtTimer();xrpctcp* Peer=NULL;
        bool OK=Mode==0?xrpcTcpConnect(S.Engine,&S.Address,1,&S.Config,&Peer):Mode==1?xrpcTcpDialAddresses(S.Engine,Addresses,1,&S.Config,&D,&Peer):xrpcTcpDial(S.Engine,Resolver,XRT_STR_LITERAL("localhost"),S.Address.Port,1,&S.Config,&D,&Peer);
        rpcTcpServiceRequire(OK,"cold factory");xfuture* F=NULL;rpcTcpServiceRequire(xrpcTcpCall(Peer,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2,3]"),5000,&F)==XRPC_DUPLEX_ACCEPTED,"cold call");
        const xrpccallresult* R=rpcTcpServiceResult(F);xrpcresponseinfo Info={0};Info.Size=sizeof(Info);
        rpcTcpServiceRequire(R->Status==XRPC_CALL_OK && R->Delivery==XRPC_REPLIED && xrpcResponsesInfo(R->Responses,&Info) && Info.Messages[R->Index].Result.Raw.Size==7 && !memcmp(Info.Messages[R->Index].Result.Raw.Data,"[1,2,3]",7),"validate exact reply");
        xrtFutureDestroy(F);rpcTcpServiceCycleClose(&S,Peer);Latency[I]=(xrtTimer()-Start)*1e6;
    }
    double Seconds=xrtTimer()-Began;xrtMemStatsGet(&After);xrtMemStatsEnable(false);rpcTcpServiceRequire(rpcBenchProcessCpu(&CPUEnd),"CPU clock");
    if(Resolver){xnetresolverstats Stats;rpcTcpServiceRequire(xrtNetResolverStats(Resolver,&Stats) && Stats.Submitted==Count && !Stats.CacheHits,"one uncached app lookup per cycle");rpcTcpServiceRequire(xrtNetResolverDestroy(Resolver),"external Resolver join");}
    xrtNetAddrListDestroy(Addresses);rpcTcpServiceRequire(xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED)==Count,"operation replay/count");rpcTcpServiceStop(&S);qsort(Latency,Count,sizeof(double),tcpDialBenchCompare);
    printf("completed: %zu\ncycles_per_sec: %.9f\nprocess_cpu_ns_per_cycle: %.9f\np50_us: %.9f\np95_us: %.9f\np99_us: %.9f\np999_us: %.9f\n",Count,Count/Seconds,(CPUEnd-CPUStart)*1000./Count,Latency[(Count*50+99)/100-1],Latency[(Count*95+99)/100-1],Latency[(Count*99+99)/100-1],Latency[(Count*999+999)/1000-1]);
    if(Heap)printf("heap_blocks_per_cycle: %.9f\nheap_bytes_per_cycle: %.9f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
    printf("mode: %u\n",Mode);puts("scope: serial closed-loop IPv4 loopback numeric/prepared/OS localhost DNS, app cache off; factory/call/exact byte validation/result release/both physical peer retire+destroy included, callback return tails at final Engine join; host Engine/shared Server/Resolver/list construction and final joins excluded; all process-thread CPU, no fixed-arrival, external DNS, TLS, connection-scale, native Linux or production-tail acceptance");free(Latency);return 0;
}
