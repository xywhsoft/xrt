#include <xrpc/duplex.h>
#include <xrt/memory.h>
#include <xrt/memory_stats.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "process_cpu.h"
static bool benchDuplexEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* C,ptr U)
{(void)C;(void)U;return xrpcReplyResult(R,M->Params.Raw);}
static bool benchDuplexPump(xrpcduplex* A,xrpcduplex* B)
{
    xrpcduplexoutput* O=NULL;uint64 T=0;int N=xrpcDuplexNext(A,&O,&T);if(N<0)return false;if(!N)return true;
    bool OK=xrpcDuplexStarted(A,T) && xrpcDuplexSent(A,T) && xrpcDuplexReceive(B,xrpcDuplexOutputWire(O),(xbytesview){0})==XRPC_DUPLEX_ACCEPTED;
    xrpcDuplexOutputRelease(O);return OK;
}
static bool benchDuplexCycle(unsigned Mode,xrpcduplex* A,xrpcduplex* B,xrpcresponses* Snapshot,uint64* Sum)
{
    xfuture* F=NULL;
    if(!Mode){xrpccallconfig K;xrpcCallConfigInit(&K);K.Timeout=XRT_WAIT_FOREVER;xrpccall* C=NULL;
        if(!xrpcCallCreate(&K,&C,&F) || xrpcCallFinish(C,XRPC_CALL_OK,Snapshot,0,NULL)!=XRPC_CALL_FINISHED || !xrpcCallDestroy(C))return false;}
    else {
        if(xrpcDuplexCall(A,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2,3]"),XRT_WAIT_FOREVER,&F)!=XRPC_DUPLEX_ACCEPTED || !xrpcDuplexStep(A))return false;
        if(Mode==1){xrpcduplexoutput* O=NULL;uint64 T;int N=xrpcDuplexNext(A,&O,&T);if(N!=1)return false;
            xrpcmessage M;xrpcdocument Doc;
            if(xrpcDecode(xrpcDuplexOutputWire(O),NULL,&M,1,&Doc)!=XRPC_DECODE_DONE || !xrpcDuplexStarted(A,T) || !xrpcDuplexSent(A,T))return false;
            xrpcoutgoing V={0};V.Kind=XRPC_MESSAGE_RESULT;V.Id=M.Id.Raw;V.Result=XRT_STR_LITERAL("[1,2,3]");char Wire[256];size_t Bytes;
            if(!xrpcEncodeWrite(Wire,sizeof(Wire),&V,1,false,NULL,&Bytes) || xrpcDuplexReceive(A,(xstrview){Wire,Bytes},(xbytesview){0})!=XRPC_DUPLEX_ACCEPTED)return false;
            xrpcDuplexOutputRelease(O);
        }else {
            while(!xrtFutureDone(F))if(!xrpcDuplexStep(A) || !xrpcDuplexStep(B) || !benchDuplexPump(A,B) || !benchDuplexPump(B,A))return false;
        }
    }
    xfutureresult R;if(!xrtFutureResult(F,&R) || R.State!=XFUTURE_RESOLVED || !R.Value)return false;
    const xrpccallresult* V=R.Value;if(V->Status!=XRPC_CALL_OK || V->Delivery!=XRPC_REPLIED)return false;
    ++*Sum;xrtFutureDestroy(F);return true;
}
static bool benchDuplexClose(xrpcduplex* D)
{
    if(!xrpcDuplexClose(D,XRPC_CALL_CLOSED))return false;
    for(;;){xrpcduplexinfo I={0};I.Size=sizeof(I);if(!xrpcDuplexStep(D) || !xrpcDuplexInfo(D,&I))return false;if(I.Drained)break;}
    return xrpcDuplexDestroy(D);
}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):10000;unsigned Mode=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):1;bool Heap=Argc>3 && strtoul(Argv[3],NULL,10);
    if(!Count || Count>10000000 || Mode>2 || Argc>4)return 1;
    xrpcserver* S=NULL;xrpcduplex *A=NULL,*B=NULL;xrpcduplexconfig C;xrpcDuplexConfigInit(&C);
    if(Mode==2){xrpcserverconfig K;xrpcServerConfigInit(&K);K.Call=benchDuplexEcho;if(!xrpcServerCreate(&K,&S))return 2;C.Server=S;}
    if(!xrpcDuplexCreate(&C,&A) || !xrpcDuplexCreate(&C,&B))return 3;
    xrpcmessage M;xrpcresponses* Snapshot=NULL;
    if(xrpcResponsesRead(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"result\":[1,2,3]}"),NULL,&M,1,&Snapshot)!=XRPC_RESPONSE_DONE)return 4;
    uint64 Sum=0;for(unsigned I=0;I<100;++I)if(!benchDuplexCycle(Mode,A,B,Snapshot,&Sum))return 5;Sum=0;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Heap);xrtMemStatsGet(&Before);
#else
    if(Heap)return 6;
#endif
    double CpuStart,CpuEnd;if(!rpcBenchProcessCpu(&CpuStart))return 7;
    double Start=xrtTimer();for(size_t I=0;I<Count;++I)if(!benchDuplexCycle(Mode,A,B,Snapshot,&Sum))return 8;double Seconds=xrtTimer()-Start;
    if(!rpcBenchProcessCpu(&CpuEnd))return 9;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    printf("calls_per_sec: %.6f\nns_per_call: %.6f\nprocess_cpu_ns_per_call: %.6f\ncompleted: %"PRIu64"\n",Count/Seconds,Seconds*1e9/Count,(CpuEnd-CpuStart)*1000./Count,Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Heap)printf("heap_blocks_per_call: %.6f\nheap_bytes_per_call: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    puts("scope: closed-loop pipeline1; native-prebuilt-snapshot cost reference / duplex plus synthetic peer decode-encode / two duplex endpoints plus shared executor; no sockets/framing/TLS, fixed-arrival or tail-latency acceptance; all process threads included in CPU");
    xrpcResponsesRelease(Snapshot);if(!benchDuplexClose(A) || !benchDuplexClose(B) || (S && !xrpcServerDestroy(S)))return 10;return Sum==Count?0:11;
}
