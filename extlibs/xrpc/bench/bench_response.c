#include <xrpc.h>
#include <xrt/memory.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int responseBenchCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
static bool responseBenchRead(xstrview Wire,xrpcmessage* Scratch,uint64* Sum)
{
	xrpcresponses* R=NULL;if(xrpcResponsesRead(Wire,NULL,Scratch,16,&R)!=XRPC_RESPONSE_DONE)return false;
	xrpcresponseinfo I={0};I.Size=sizeof(I);bool OK=xrpcResponsesInfo(R,&I);if(OK)*Sum+=I.Count+I.Wire.Size+I.Messages[0].Result.Raw.Size;xrpcResponsesRelease(R);return OK;
}
int main(int argc,char** argv)
{
	uint64 Count=argc>1?strtoull(argv[1],NULL,10):200000;unsigned Mode=argc>2?(unsigned)strtoul(argv[2],NULL,10):0;
	if(!Count || Count>1000000000u || Mode>1 || argc>3)return 1;
	char Large[65700];xstrview Wire=XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1e99999,\"result\":[1,2,3,\"\\u0000\"]}");
	if(Mode){cstr Prefix="{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"";size_t N=strlen(Prefix);memcpy(Large,Prefix,N);memset(Large+N,'x',65536);memcpy(Large+N+65536,"\"}",2);Wire=(xstrview){Large,N+65538};}
	size_t SamplesCount=Count<10000?(size_t)Count:10000;double* Samples=xrtMalloc(SamplesCount*sizeof(*Samples));if(!Samples)return 2;
	xrpcmessage Scratch[16];uint64 Sum=0;double Start=xrtTimer();for(uint64 K=0;K<Count;++K)if(!responseBenchRead(Wire,Scratch,&Sum))return 3;
	double Seconds=xrtTimer()-Start;if(!(Seconds>0))return 4;
	for(size_t K=0;K<SamplesCount;++K){Start=xrtTimer();if(!responseBenchRead(Wire,Scratch,&Sum))return 5;Samples[K]=(xrtTimer()-Start)*1000000.;}
	qsort(Samples,SamplesCount,sizeof(*Samples),responseBenchCompare);
	printf("documents_per_sec: %.9f\np50_us: %.9f\np99_us: %.9f\ncompleted: %"PRIu64"\nlatency_samples: %zu\ninput_bytes: %zu\nmode: %s\n",(double)Count/Seconds,Samples[(SamplesCount*50+99)/100-1],Samples[(SamplesCount*99+99)/100-1],Count,SamplesCount,Wire.Size,Mode?"64KiB-string":"single-result");
	puts("workload: full response validation, one snapshot allocation/copy/rebase/info/release; input construction/correlation/queues/transport excluded; separate latency phase includes two clocks");xrtFree(Samples);return Sum?0:6;
}
