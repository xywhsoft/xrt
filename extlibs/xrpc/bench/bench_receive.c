#include <xrpc.h>
#include <xrt/memory.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static int receiveBenchCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
static bool receiveBenchCycle(xrpcpending* P,uint64* Sum)
{
	xrpcpendingtoken Token;if(xrpcPendingInsert(P,XRT_STR_LITERAL("1"),100,NULL,&Token)!=XRPC_PENDING_OK ||
		xrpcPendingInsert(P,XRT_STR_LITERAL("\"a\\u0000b\""),100,NULL,&Token)!=XRPC_PENDING_OK)return false;
	xrpcmessage Scratch[4];xrpcpendingresolution Matches[4];xrpcresponses* R=NULL;xrpcreceiveinfo I={0};I.Size=sizeof(I);
	xrpcresponsestatus Status=xrpcReceive(P,XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":true},{\"jsonrpc\":\"2.0\",\"id\":\"a\\u0000b\",\"error\":{\"code\":-1,\"message\":\"m\"}},{\"jsonrpc\":\"2.0\",\"id\":1.0,\"result\":false},{\"jsonrpc\":\"2.0\",\"id\":999,\"result\":null}]"),NULL,Scratch,4,Matches,4,&R,&I);
	bool OK=Status==XRPC_RESPONSE_DONE && I.Count==4 && I.Matched==2 && I.Repeated==1 && I.Unknown==1;
	if(OK)*Sum+=Matches[0].Call.Token.Serial+Matches[1].Call.Token.Serial;
	xrpcResponsesRelease(R);return OK;
}
int main(int argc,char** argv)
{
	uint64 Count=argc>1?strtoull(argv[1],NULL,10):200000;if(!Count || Count>1000000000u || argc>2)return 1;
	xrpcpendingconfig C;xrpcPendingConfigInit(&C);C.MaxCalls=2;xrpcpending* P=NULL;size_t N=Count<10000?(size_t)Count:10000;double* Samples=xrtMalloc(N*sizeof(*Samples));
	if(!Samples || !xrpcPendingCreate(&C,&P))return 2;
	uint64 Sum=0;double Start=xrtTimer();
	for(uint64 K=0;K<Count;++K)if(!receiveBenchCycle(P,&Sum))return 3;
	double Seconds=xrtTimer()-Start;if(!(Seconds>0))return 4;
	for(size_t K=0;K<N;++K){Start=xrtTimer();if(!receiveBenchCycle(P,&Sum))return 5;Samples[K]=(xrtTimer()-Start)*1000000.;}
	qsort(Samples,N,sizeof(*Samples),receiveBenchCompare);
	printf("batches_per_sec: %.9f\np50_us: %.9f\np99_us: %.9f\ncompleted: %"PRIu64"\nlatency_samples: %zu\n",(double)Count/Seconds,Samples[(N*50+99)/100-1],Samples[(N*99+99)/100-1],Count,N);
	puts("workload: two admissions, four-response decode/snapshot/canonicalization/all-removals, repeated+unknown IDs, release; locally reused wire IDs; queues/futures/transport excluded; separate latency phase includes two clocks");
	xrpcPendingDestroy(P);xrtFree(Samples);return Sum?0:6;
}
