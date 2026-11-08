#include <xrpc.h>
#include <xrt/memory.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static int rpcPendingBenchCompare(const void* A,const void* B)
{ double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y); }
static bool rpcPendingBenchCycle(xrpcpending* P,unsigned Mode,uint64 Index,size_t Depth,
	xstrview* Names,xrpcpendingtoken* Tokens,uint64* Checksum)
{
	size_t Slot=(size_t)(Index%Depth);xrpcpendingcall Call;xrpcpendingstatus Status;
	if(Mode==0)Status=xrpcPendingTakeId(P,Names[Slot],&Call);
	else if(Mode==1)Status=xrpcPendingTakeToken(P,Tokens[Slot],&Call);
	else Status=xrpcPendingExpire(P,Index,&Call);
	if(Status!=XRPC_PENDING_OK || Call.Token.Serial!=Tokens[Slot].Serial || Call.Deadline!=Index)return false;
	*Checksum+=Call.Token.Serial;
	return xrpcPendingInsert(P,Names[Slot],Index+Depth,NULL,&Tokens[Slot])==XRPC_PENDING_OK;
}
int main(int argc,char** argv)
{
	uint64 Count=argc>1?strtoull(argv[1],NULL,10):1000000u;
	unsigned Mode=argc>2?(unsigned)strtoul(argv[2],NULL,10):0;size_t Depth=argc>3?(size_t)strtoul(argv[3],NULL,10):1024;
	if(!Count || Count>1000000000u || Mode>2 || !Depth || Depth>4096 || argc>4)return 1;
	xrpcpendingconfig Config;xrpcPendingConfigInit(&Config);Config.MaxCalls=Depth;Config.MaxOwnedBytes=8u*1024u*1024u;
	Config.Id.MaxInputBytes=128;Config.Id.MaxStringBytes=32;Config.Id.MaxKeyBytes=64;xrpcpending* P=NULL;
	char Raw[4096][32];xstrview Names[4096];xrpcpendingtoken Tokens[4096];
	size_t SamplesCount=Count<10000?(size_t)Count:10000;double* Samples=xrtMalloc(SamplesCount*sizeof(*Samples));
	if(!Samples || !xrpcPendingCreate(&Config,&P))return 2;
	for(size_t Index=0;Index<Depth;++Index){int Bytes=snprintf(Raw[Index],sizeof(Raw[Index]),"\"call-%08zu\"",Index);
		Names[Index]=(xstrview){Raw[Index],(size_t)Bytes};
		if(xrpcPendingInsert(P,Names[Index],Index,NULL,&Tokens[Index])!=XRPC_PENDING_OK)return 3;}
	xrpcpendinginfo Info={0};Info.Size=sizeof(Info);if(!xrpcPendingInfo(P,&Info))return 4;
	uint64 Checksum=0;double Start=xrtTimer();
	for(uint64 Index=0;Index<Count;++Index)if(!rpcPendingBenchCycle(P,Mode,Index,Depth,Names,Tokens,&Checksum))return 5;
	double Seconds=xrtTimer()-Start;if(!(Seconds>0))return 6;
	for(size_t Sample=0;Sample<SamplesCount;++Sample){Start=xrtTimer();
		if(!rpcPendingBenchCycle(P,Mode,Count+Sample,Depth,Names,Tokens,&Checksum))return 7;
		Samples[Sample]=(xrtTimer()-Start)*1000000.0;}
	qsort(Samples,SamplesCount,sizeof(*Samples),rpcPendingBenchCompare);
	printf("cycles_per_sec: %.9f\ncycle_p50_us: %.9f\ncycle_p99_us: %.9f\ncompleted: %"PRIu64"\nlatency_samples: %zu\nchecksum: %"PRIu64"\n",
		(double)Count/Seconds,Samples[(SamplesCount*50u+99u)/100u-1u],Samples[(SamplesCount*99u+99u)/100u-1u],Count,SamplesCount,Checksum);
	printf("mode: %s\nlive_calls: %zu\nowned_bytes: %zu\nmax_key_bytes: %zu\n",Mode==0?"response":Mode==1?"cancel":"expire",Depth,Info.OwnedBytes,Config.Id.MaxKeyBytes);
	puts("workload: serialized remove+reinsert at fixed occupancy, precomputed locally reused wire IDs, independent ID validation; setup/format/queues/futures/transport excluded; separate latency phase includes two clocks");
	xrpcPendingDestroy(P);xrtFree(Samples);return Checksum?0:8;
}
