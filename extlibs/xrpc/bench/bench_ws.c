#include <xrpc/ws.h>
#include <xrt/memory_stats.h>
#include "../examples/common/ws_service.h"
#include "process_cpu.h"
static bool benchWsCycle(xrpcws* P,size_t Count,xstrview Params)
{
    xfuture* F[32];for(size_t I=0;I<Count;++I)if(xrpcWsCall(P,XRT_STR_LITERAL("echo"),Params,10000,F+I)!=XRPC_DUPLEX_ACCEPTED)return false;
    for(size_t I=0;I<Count;++I){const xrpccallresult* R=rpcWsServiceResult(F[I]);xrpcresponseinfo Info={0};Info.Size=sizeof(Info);if(R->Status!=XRPC_CALL_OK||R->Delivery!=XRPC_REPLIED||!xrpcResponsesInfo(R->Responses,&Info)||R->Index>=Info.Count||Info.Messages[R->Index].Result.Raw.Size!=Params.Size||memcmp(Info.Messages[R->Index].Result.Raw.Data,Params.Data,Params.Size))return false;xrtFutureDestroy(F[I]);}return true;
}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):2000,Window=Argc>2?(size_t)strtoull(Argv[2],NULL,10):1;bool Heap=Argc>3&&strtoul(Argv[3],NULL,10),TLS=Argc>5&&strtoul(Argv[5],NULL,10);size_t Size=Argc>4?(size_t)strtoull(Argv[4],NULL,10):7;
    if(!Count||Count>1000000||!Window||Window>32||Size<4||Size>524288||Argc>6)return 1;
    char* Text=malloc(Size);if(!Text)return 2;Text[0]='[';Text[1]='"';memset(Text+2,'x',Size-4);Text[Size-2]='"';Text[Size-1]=']';xstrview Params={Text,Size};rpcWsService S;rpcWsServiceStart(&S,TLS,NULL,false);
    for(unsigned I=0;I<100;++I)rpcWsServiceRequire(benchWsCycle(S.Client,Window,Params),"WS benchmark warmup");
    xmemstats Before,After;xrtMemStatsEnable(Heap);xrtMemStatsGet(&Before);double CPU0,CPU1;rpcWsServiceRequire(rpcBenchProcessCpu(&CPU0),"WS process CPU");double Start=xrtTimer();
    for(size_t I=0;I<Count;){size_t N=Count-I;if(N>Window)N=Window;rpcWsServiceRequire(benchWsCycle(S.Client,N,Params),"WS benchmark exact reply");I+=N;}
    double Seconds=xrtTimer()-Start;rpcWsServiceRequire(rpcBenchProcessCpu(&CPU1),"WS process CPU end");xrtMemStatsGet(&After);xrtMemStatsEnable(false);printf("completed: %zu\ncalls_per_sec: %.6f\nprocess_cpu_ns_per_call: %.6f\npayload_bytes: %zu\n",Count,Count/Seconds,(CPU1-CPU0)*1000./Count,Size);
    if(Heap)printf("heap_blocks_per_call: %.6f\nheap_bytes_per_call: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
    rpcWsServiceStop(&S);free(Text);puts("scope: validated single-connection loopback Text WS, masked client/native retained unmasked server output, shared Server/Engine2; TLS fixture pin optional, setup/handshake outside samples; all process threads, no queue-arrival/tail/scale/native Linux/production acceptance");return 0;
}
