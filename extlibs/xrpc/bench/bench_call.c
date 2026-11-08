#include <xrpc.h>
#include <xrt/memory.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static int callBenchCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
static bool callBenchCycle(unsigned Mode,xrpcresponses* Response,uint64* Sum)
{
	xrpccallconfig Config;xrpcCallConfigInit(&Config);Config.Timeout=Mode==2?0:XRT_WAIT_FOREVER;xrpccall* C=NULL;xfuture* F=NULL;
	if(!xrpcCallCreate(&Config,&C,&F))return false;
	xrpccallfinish Finished;
	if(Mode==1){if(!xrpcCallMarkSent(C) || !xrtFutureCancel(F))return false;Finished=xrpcCallPoll(C);}
	else if(Mode==2)Finished=xrpcCallPoll(C);
	else Finished=xrpcCallFinish(C,XRPC_CALL_OK,Response,0,NULL);
	xfutureresult R;bool OK=Finished==XRPC_CALL_FINISHED && xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value;
	if(OK){const xrpccallresult* Value=R.Value;OK=Value->Status==(Mode==0?XRPC_CALL_OK:Mode==1?XRPC_CALL_CANCELLED:XRPC_CALL_TIMEOUT);*Sum+=(uint64)Value->Status+Value->Delivery+1u;}
	if(!xrpcCallDestroy(C))return false;
	xrtFutureDestroy(F);return OK;
}
int main(int argc,char** argv)
{
	uint64 Count=argc>1?strtoull(argv[1],NULL,10):200000;unsigned Mode=argc>2?(unsigned)strtoul(argv[2],NULL,10):0;
	if(!Count || Count>1000000000u || Mode>2 || argc>3)return 1;
	xrpcmessage Scratch;xrpcresponses* Response=NULL;
	if(xrpcResponsesRead(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":true}"),NULL,&Scratch,1,&Response)!=XRPC_RESPONSE_DONE)return 2;
	size_t N=Count<10000?(size_t)Count:10000;double* Samples=xrtMalloc(N*sizeof(*Samples));if(!Samples)return 3;
	uint64 Sum=0;double Start=xrtTimer();for(uint64 K=0;K<Count;++K)if(!callBenchCycle(Mode,Response,&Sum))return 4;
	double Seconds=xrtTimer()-Start;if(!(Seconds>0))return 5;
	for(size_t K=0;K<N;++K){Start=xrtTimer();if(!callBenchCycle(Mode,Response,&Sum))return 6;Samples[K]=(xrtTimer()-Start)*1000000.;}
	qsort(Samples,N,sizeof(*Samples),callBenchCompare);
	printf("calls_per_sec: %.9f\np50_us: %.9f\np99_us: %.9f\ncompleted: %"PRIu64"\nlatency_samples: %zu\nmode: %s\n",(double)Count/Seconds,Samples[(N*50+99)/100-1],Samples[(N*99+99)/100-1],Count,N,Mode==0?"reply":Mode==1?"cancel":"immediate-timeout");
	puts("workload: producer/Future/result/native producer binding, terminal publication/read and all destruction; reply uses prebuilt retained snapshot; input encoding/queues/timers/transport excluded; separate latency phase includes two clocks");
	xrtFree(Samples);xrpcResponsesRelease(Response);return Sum?0:7;
}
