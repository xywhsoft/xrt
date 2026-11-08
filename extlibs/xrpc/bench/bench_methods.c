#include <xrpc.h>
#include <xrt/memory.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {RPC_BENCH_METHODS=4096,RPC_BENCH_QUERIES=64};
typedef struct rpcmethodsbench {
	xrpcmethods* Methods;unsigned Mode;
	xstrview Queries[RPC_BENCH_QUERIES],Documents[RPC_BENCH_QUERIES];
	uint32 Tags[RPC_BENCH_QUERIES];
	xrpcmessage Scratch[1];xrpcdispatchinfo Info;
	char Output[128];uint64 Checksum,Replies;
} rpcmethodsbench;
static bool rpcMethodsBenchHandler(const xrpcmessage* Request,xrpcreply* Reply,ptr User)
{(void)Request;(void)User;return xrpcReplyResult(Reply,XRT_STR_LITERAL("null"));}
static bool rpcMethodsBenchEmit(const xrpcoutgoing* Response,bool Batch,size_t Bytes,ptr User)
{
	rpcmethodsbench* Bench=User;size_t Written=0;
	if(Batch || !xrpcEncodeWrite(Bench->Output,sizeof(Bench->Output),Response,1,false,NULL,&Written) || Written!=Bytes)return false;
	Bench->Checksum+=Written+(unsigned char)Bench->Output[Written-1u];++Bench->Replies;return true;
}
static bool rpcMethodsBenchOperation(rpcmethodsbench* Bench,unsigned Query)
{
	if(Bench->Mode==3)return xrpcDispatch(Bench->Documents[Query],NULL,Bench->Scratch,1,xrpcMethodsCall,Bench->Methods,rpcMethodsBenchEmit,Bench,&Bench->Info) && Bench->Info.Replies==1;
	xrpcmethod Binding;xrpcmethodsstatus Status=xrpcMethodsFind(Bench->Methods,Bench->Queries[Query],&Binding);
	if(Bench->Mode==2){if(Status!=XRPC_METHODS_NOT_FOUND)return false;Bench->Checksum+=Query+1u;}
	else {if(Status!=XRPC_METHODS_FOUND || *(const uint32*)Binding.User!=Bench->Tags[Query])return false;
		Bench->Checksum+=*(const uint32*)Binding.User+Binding.Name.Size;}
	return true;
}
static int rpcMethodsBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
int main(int argc,char** argv)
{
	uint64 Count=argc>1?strtoull(argv[1],NULL,10):1000000u;unsigned Mode=argc>2?(unsigned)strtoul(argv[2],NULL,10):0;
	if(!Count || Count>1000000000u || Mode>3 || argc>3)return 1;
	size_t SampleCount=Count<10000u?(size_t)Count:10000u;
	double* Samples=xrtMalloc(SampleCount*sizeof(*Samples));xrpcmethod* Bindings=xrtCalloc(RPC_BENCH_METHODS,sizeof(*Bindings));
	char(*Names)[24]=xrtMalloc(RPC_BENCH_METHODS*24u);uint32* Tags=xrtMalloc(RPC_BENCH_METHODS*sizeof(*Tags));
	if(!Samples || !Bindings || !Names || !Tags)return 2;
	for(size_t Index=0;Index<RPC_BENCH_METHODS;++Index){int Bytes=snprintf(Names[Index],24,"svc.method.%04zu",Index);
		Tags[Index]=(uint32)Index;size_t Slot=(Index*4051u+17u)%RPC_BENCH_METHODS;
		Bindings[Slot]=(xrpcmethod){{Names[Index],(size_t)Bytes},rpcMethodsBenchHandler,&Tags[Index]};}
	rpcmethodsbench Bench={0};Bench.Mode=Mode;Bench.Info.Size=sizeof(Bench.Info);
	if(!xrpcMethodsCreate(Bindings,RPC_BENCH_METHODS,NULL,&Bench.Methods))return 3;
	xrpcmethodsinfo Info={0};Info.Size=sizeof(Info);if(!xrpcMethodsInfo(Bench.Methods,&Info) || Info.Methods!=RPC_BENCH_METHODS)return 4;
	char Raw[RPC_BENCH_QUERIES][256],Documents[RPC_BENCH_QUERIES][384];
	for(unsigned Query=0;Query<RPC_BENCH_QUERIES;++Query){
		unsigned Index=(Query*67u+113u)%RPC_BENCH_METHODS;Bench.Tags[Query]=Index;size_t Bytes=0;
		if(Mode==2){int Size=snprintf(Raw[Query],sizeof(Raw[Query]),"\"svc.missing.%04u\"",Index);Bytes=(size_t)Size;}
		else {Raw[Query][Bytes++]='"';
			for(size_t Offset=0;Names[Index][Offset];++Offset){unsigned char Byte=(unsigned char)Names[Index][Offset];
				if(Mode==1 || (Mode==3 && (Query&1u)))Bytes+=(size_t)snprintf(Raw[Query]+Bytes,sizeof(Raw[Query])-Bytes,"\\u%04x",(unsigned)Byte);
				else Raw[Query][Bytes++]=(char)Byte;}
			Raw[Query][Bytes++]='"';Raw[Query][Bytes]=0;}
		Bench.Queries[Query]=(xstrview){Raw[Query],Bytes};
		int Size=snprintf(Documents[Query],sizeof(Documents[Query]),"{\"jsonrpc\":\"2.0\",\"method\":%.*s,\"id\":1e9999}",(int)Bytes,Raw[Query]);
		if(Size<=0 || (size_t)Size>=sizeof(Documents[Query]))return 5;
		Bench.Documents[Query]=(xstrview){Documents[Query],(size_t)Size};
	}
	xrtFree(Bindings);xrtFree(Names);uint32 State=0x142857u;double Began=xrtTimer();
	for(uint64 Index=0;Index<Count;++Index){State=State*1664525u+1013904223u;if(!rpcMethodsBenchOperation(&Bench,State&63u))return 6;}
	double Seconds=xrtTimer()-Began;if(!(Seconds>0))return 7;
	for(size_t Index=0;Index<SampleCount;++Index){State=State*1664525u+1013904223u;double Start=xrtTimer();
		if(!rpcMethodsBenchOperation(&Bench,State&63u))return 8;
		Samples[Index]=(xrtTimer()-Start)*1000000.0;}
	qsort(Samples,SampleCount,sizeof(*Samples),rpcMethodsBenchCompare);
	if(!Bench.Checksum || (Mode==3 && Bench.Replies!=Count+SampleCount))return 9;
	printf("operations_per_sec: %.9f\noperation_p50_us: %.9f\noperation_p99_us: %.9f\ncompleted: %"PRIu64"\nlatency_samples: %zu\nchecksum: %"PRIu64"\n",
		(double)Count/Seconds,Samples[(SampleCount*50u+99u)/100u-1u],Samples[(SampleCount*99u+99u)/100u-1u],Count,SampleCount,Bench.Checksum);
	printf("index_methods: %zu\nrequested_owned_bytes: %zu\nmode: %s\n",Info.Methods,Info.OwnedBytes,Mode==0?"plain":Mode==1?"escaped":Mode==2?"miss":"dispatch");
	puts("workload: 4096 copied registrations; 64 spread queries; validated Find or one-call decode/index/handler/encode; construction/queues/transport excluded; separate latency phase includes two clocks, throughput excludes per-operation clocks");
	xrpcMethodsRelease(Bench.Methods);xrtFree(Tags);xrtFree(Samples);return 0;
}
