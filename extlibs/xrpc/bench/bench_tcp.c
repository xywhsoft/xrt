#include <xrpc/tcp.h>
#include <xrt/memory_stats.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "process_cpu.h"
static xatomic32 Ready;static xrpctcp* Accepted;static xrpctcpconfig Config;
static bool benchTcpEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* C,ptr U)
{(void)C;(void)U;return xrpcReplyResult(R,M->Params.Raw);}
static bool benchTcpAccept(xnetlistener* L,xnetstream* S,ptr U)
{(void)L;(void)U;if(!xrpcTcpAttach(S,&Config,&Accepted))return false;xrtAtomic32Store(&Ready,1,XMEMORY_RELEASE);return true;}
static bool benchTcpCycle(xrpctcp* Peer,size_t N,double* Latency)
{
    xfuture* F[32];double Start=xrtTimer();
    for(size_t I=0;I<N;++I)if(xrpcTcpCall(Peer,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2,3]"),10000,F+I)!=XRPC_DUPLEX_ACCEPTED)return false;
    for(size_t I=0;I<N;++I){if(xrtFutureWaitFor(F[I],15000)!=XWAIT_OK)return false;const xrpccallresult* R=xrtFutureValue(F[I]);if(!R || R->Status!=XRPC_CALL_OK || R->Delivery!=XRPC_REPLIED)return false;xrtFutureDestroy(F[I]);}
    if(Latency)*Latency=(xrtTimer()-Start)*1e6;
    return true;
}
static int benchTcpCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):2000,Width=Argc>2?(size_t)strtoull(Argv[2],NULL,10):1;bool Heap=Argc>3 && strtoul(Argv[3],NULL,10);
    if(!Count || Count>1000000 || !Width || Width>32 || Argc>4)return 1;
    xrtAtomic32Init(&Ready,0);xnetengineconfig C;xrtNetEngineConfigInit(&C);C.Workers=2;xnetengine* E=xrtNetEngineCreate(&C);if(!E || !xrtNetEngineStart(E))return 2;
    xrpcserverconfig S;xrpcServerConfigInit(&S);S.Call=benchTcpEcho;xrpcserver* Server=NULL;if(!xrpcServerCreate(&S,&Server))return 3;xrpcTcpConfigInit(&Config);Config.Duplex.Server=Server;
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);xnetlistenerevents Events={benchTcpAccept,NULL,NULL};xnetlistener* Listener=xrtNetListen(E,&L,&Events,NULL,NULL);xnetaddr Remote;
    if(!Listener || !xrtNetListenerLocal(Listener,&Remote))return 4;
    xrpctcp* Peer=NULL;if(!xrpcTcpConnect(E,&Remote,1,&Config,&Peer))return 5;double End=xrtTimer()+10;
    while(!xrtAtomic32Load(&Ready,XMEMORY_ACQUIRE)){if(xrtTimer()>=End)return 6;xrtSleep(1);}
    double* Latency=Width==1?xrtMalloc(Count*sizeof(double)):NULL;if(Width==1 && !Latency)return 7;
    for(unsigned I=0;I<100;++I)if(!benchTcpCycle(Peer,Width,NULL))return 8;
    xmemstats Before,After;xrtMemStatsEnable(Heap);xrtMemStatsGet(&Before);double CpuStart,CpuEnd;if(!rpcBenchProcessCpu(&CpuStart))return 9;
    double Start=xrtTimer();for(size_t I=0;I<Count;){size_t N=Count-I;if(N>Width)N=Width;if(!benchTcpCycle(Peer,N,Latency?Latency+I:NULL))return 10;I+=N;}double Seconds=xrtTimer()-Start;
    if(!rpcBenchProcessCpu(&CpuEnd))return 11;
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
    printf("calls_per_sec: %.6f\nns_per_call: %.6f\nprocess_cpu_ns_per_call: %.6f\ncompleted: %zu\n",Count/Seconds,Seconds*1e9/Count,(CpuEnd-CpuStart)*1000./Count,Count);
    if(Latency){qsort(Latency,Count,sizeof(double),benchTcpCompare);printf("closed_loop_p50_us: %.6f\nclosed_loop_p99_us: %.6f\n",Latency[(Count-1)/2],Latency[(Count*99-1)/100]);}
    if(Heap)printf("heap_blocks_per_call: %.6f\nheap_bytes_per_call: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
    puts("scope: native loopback TCP/Content-Length; one connection, shared executor, closed-loop pipeline1 or barrier-window32; P99 only pipeline1 includes caller wait/collection and has coordinated omission; all process threads in CPU; unpinned, no fixed-arrival/connection-scale/native-Linux production acceptance");
    xrtFree(Latency);if(!xrpcTcpClose(Peer) || !xrpcTcpClose(Accepted) || xrpcTcpWaitFor(Peer,10000)!=XWAIT_OK || xrpcTcpWaitFor(Accepted,10000)!=XWAIT_OK || !xrpcTcpDestroy(Peer) || !xrpcTcpDestroy(Accepted))return 12;
    xrtNetListenerClose(Listener);xrtNetListenerDestroy(Listener);if(!xrpcServerDestroy(Server))return 13;End=xrtTimer()+10;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(E))==XNET_RETIRE_BUSY){if(xrtTimer()>=End)return 14;xrtSleep(1);}return Q==XNET_RETIRE_READY?0:15;
}
