#include <xrpc.h>
#include <xrt/memory.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int rpcIdBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
int main(int argc,char** argv)
{
	uint64 Count=argc>1?strtoull(argv[1],NULL,10):1000000u;unsigned Mode=argc>2?(unsigned)strtoul(argv[2],NULL,10):0;
	if(!Count || Count>1000000000u || Mode>2 || argc>3)return 1;
	xstrview Input=Mode==0?XRT_STR_LITERAL("\"a\\u0000b\\u4e2d\\uD83D\\uDE00\""):XRT_STR_LITERAL("123456789012345678901234567890.001230000e+99999");
	char Large[1100];if(Mode==2){memcpy(Large,"10e1",4);memset(Large+4,'0',1000);Input=(xstrview){Large,1004};}
	size_t SamplesCount=Count<10000u?(size_t)Count:10000u,Expected=0,Written=0;
	double* Samples=xrtMalloc(SamplesCount*sizeof(*Samples));unsigned char Output[8192];
	if(!Samples || !xrpcIdKeySize(Input,NULL,&Expected))return 2;
	uint64 Checksum=0;double Began=xrtTimer();
	for(uint64 Index=0;Index<Count;++Index){
		if(!xrpcIdKeyWrite(Output,sizeof(Output),Input,NULL,&Written) || Written!=Expected)return 3;
		Checksum+=Written+Output[0]+Output[Written-1u];
	}
	double Seconds=xrtTimer()-Began;if(!(Seconds>0))return 4;
	for(size_t Index=0;Index<SamplesCount;++Index){double Start=xrtTimer();
		if(!xrpcIdKeyWrite(Output,sizeof(Output),Input,NULL,&Written) || Written!=Expected)return 5;
		Samples[Index]=(xrtTimer()-Start)*1000000.0;Checksum+=Written+Output[0];
	}
	qsort(Samples,SamplesCount,sizeof(*Samples),rpcIdBenchCompare);
	printf("ids_per_sec: %.9f\nid_p50_us: %.9f\nid_p99_us: %.9f\ncompleted: %"PRIu64"\nlatency_samples: %zu\nchecksum: %"PRIu64"\n",
		(double)Count/Seconds,Samples[(SamplesCount*50u+99u)/100u-1u],Samples[(SamplesCount*99u+99u)/100u-1u],Count,SamplesCount,Checksum);
	printf("mode: %s\ninput_bytes: %zu\nkey_bytes: %zu\n",Mode==0?"escaped-string":Mode==1?"decimal":"huge-exponent",Input.Size,Expected);
	puts("workload: independently validated ID KeyWrite into caller storage; construction/Size/queues/correlation/transport excluded; separate latency phase includes two clocks, throughput excludes per-ID clocks");
	xrtFree(Samples);return Checksum?0:6;
}
