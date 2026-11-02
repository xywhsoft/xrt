#include "test.h"

static xstrview rpcResponseView(cstr Text){return (xstrview){Text,strlen(Text)};}
static xrpcresponses* rpcResponsesNew(xstrview Input)
{
	xrpcmessage Scratch[32];xrpcresponses* R=NULL;testRequire(xrpcResponsesRead(Input,NULL,Scratch,32,&R)==XRPC_RESPONSE_DONE,"response snapshot read");return R;
}
static xrpcresponseinfo rpcResponsesGet(xrpcresponses* R)
{xrpcresponseinfo I={0};I.Size=sizeof(I);testRequire(xrpcResponsesInfo(R,&I),"snapshot info");return I;}
static void rpcResponseSlice(xstrview View,xstrview Wire)
{if(View.Data)testRequire((uintptr_t)View.Data>=(uintptr_t)Wire.Data && View.Size<=Wire.Size && (uintptr_t)View.Data-(uintptr_t)Wire.Data<=Wire.Size-View.Size,"snapshot view escaped owned wire");else testRequire(!View.Size,"missing raw view");}
static void testRpcResponseOwnership(void)
{
	cstr Fixture=" [ {\"jsonrpc\":\"2.0\",\"id\":10e18446744073709551615,\"result\":{\"a\":[true,null,\"x\\u0000y\"]}}, {\"jsonrpc\":\"2.0\",\"id\":\"a\\u0000b\",\"error\":{\"code\":-32000,\"message\":\"bad\\u0000x\",\"data\":[1,{\"x\":false}]}} ] ";
	char* Input=xrtMalloc(strlen(Fixture)+1u);strcpy(Input,Fixture);xrpcresponses* R=rpcResponsesNew(rpcResponseView(Input));
	memset(Input,0,strlen(Fixture));xrtFree(Input);xrpcresponseinfo I=rpcResponsesGet(R);
	testRequire(I.Count==2 && I.Batch && I.Wire.Size==strlen(Fixture) && !memcmp(I.Wire.Data,Fixture,I.Wire.Size),"snapshot owns complete original wire");
	for(size_t K=0;K<I.Count;++K){const xrpcmessage* M=&I.Messages[K];xstrview Views[]={M->Wire,M->Id.Raw,M->Method.Raw,M->Params.Raw,M->Result.Raw,M->Error.Raw,M->ErrorCode.Raw,M->ErrorMessage.Raw,M->ErrorData.Raw,I.Ids[K]};
		for(size_t V=0;V<sizeof(Views)/sizeof(Views[0]);++V)rpcResponseSlice(Views[V],I.Wire);
		testRequire(I.Ids[K].Data==M->Id.Raw.Data && I.Ids[K].Size==M->Id.Raw.Size,"contiguous ID vector rebasing");}
	testRequire(I.Messages[0].Kind==XRPC_MESSAGE_RESULT && I.Messages[0].Id.Raw.Size==23 &&
		I.Messages[1].Kind==XRPC_MESSAGE_ERROR && I.Messages[1].ErrorCode.Raw.Size==6 && I.Messages[1].ErrorData.Kind==XRPC_VALUE_ARRAY,"snapshot response descriptors");
	testRequire(xrpcResponsesRetain(R),"snapshot retained ref");xrpcResponsesRelease(R);I=rpcResponsesGet(R);testRequire(I.Count==2,"snapshot retained lifetime");xrpcResponsesRelease(R);
	for(unsigned Trial=0;Trial<10000;++Trial){char Wire[512];unsigned V=Trial*7919u;
		int N=snprintf(Wire,sizeof(Wire),"[{\"jsonrpc\":\"2.0\",\"id\":%u,\"result\":[%u,\"\\u0061\"]},{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-1,\"message\":\"m\",\"data\":%u}}]",Trial,V,V);
		R=rpcResponsesNew((xstrview){Wire,(size_t)N});I=rpcResponsesGet(R);char Expected[64];int E=snprintf(Expected,sizeof(Expected),"[%u,\"\\u0061\"]",V);
		testRequire(I.Count==2 && I.Messages[0].Result.Raw.Size==(size_t)E && !memcmp(I.Messages[0].Result.Raw.Data,Expected,(size_t)E) && I.Messages[1].Id.Kind==XRPC_VALUE_NULL,"independent snapshot fixture property");
		memset(Wire,0,sizeof(Wire));testRequire(I.Wire.Data[0]=='[',"generated input not copied");xrpcResponsesRelease(R);}
}
static void testRpcResponseErrors(void)
{
	xrpcmessage Scratch[4];xrpcresponses* R=NULL;static const cstr Invalid[]={"[]","null","true","1","{}","{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1}","[{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":1},{}]"};
	for(size_t K=0;K<sizeof(Invalid)/sizeof(Invalid[0]);++K){testRequire(xrpcResponsesRead(rpcResponseView(Invalid[K]),NULL,Scratch,4,&R)==XRPC_RESPONSE_INVALID && !R,"invalid response snapshot published");xrtClearError();}
	testRequire(xrpcResponsesRead(XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":1},"),NULL,Scratch,4,&R)==XRPC_RESPONSE_PARSE_ERROR && !R,"malformed tail published");xrtClearError();
	xstrview Wire=XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":null}");R=rpcResponsesNew(Wire);size_t Bytes=rpcResponsesGet(R).OwnedBytes;xrpcResponsesRelease(R);R=NULL;
	xrpcresponseconfig C;xrpcResponseConfigInit(&C);C.MaxOwnedBytes=Bytes;testRequire(xrpcResponsesRead(Wire,&C,Scratch,4,&R)==XRPC_RESPONSE_DONE,"exact snapshot byte ceiling");xrpcResponsesRelease(R);R=NULL;
	--C.MaxOwnedBytes;testRequire(xrpcResponsesRead(Wire,&C,Scratch,4,&R)==XRPC_RESPONSE_LOCAL_ERROR && !R,"short snapshot byte ceiling");xrtClearError();
	xrpcResponseConfigInit(&C);C.Decode.Mode=XRPC_DECODE_DUPLEX;testRequire(xrpcResponsesRead(Wire,&C,Scratch,4,&R)==XRPC_RESPONSE_LOCAL_ERROR,"wrong snapshot role");xrtClearError();
	xrpcResponseConfigInit(&C);C.Decode.MaxInputBytes=1;testRequire(xrpcResponsesRead(Wire,&C,Scratch,4,&R)==XRPC_RESPONSE_LOCAL_ERROR,"snapshot decoder limit");xrtClearError();
	char Input[128];memcpy(Input,Wire.Data,Wire.Size);testRequire(xrpcResponsesRead((xstrview){Input,Wire.Size},NULL,Scratch,4,(xrpcresponses**)Input)==XRPC_RESPONSE_LOCAL_ERROR && Input[0]=='{',"snapshot output/input alias");xrtClearError();
	testRequire(xrpcResponsesRead(Wire,NULL,Scratch,4,(xrpcresponses**)Scratch)==XRPC_RESPONSE_LOCAL_ERROR,"snapshot output/scratch alias");xrtClearError();
	testRequire(!xrpcResponseConfigInit(NULL) && !xrpcResponsesRetain(NULL) && !xrpcResponsesInfo(NULL,NULL) && xrpcResponsesRead(Wire,NULL,NULL,0,&R)==XRPC_RESPONSE_LOCAL_ERROR,"snapshot NULL API");xrtClearError();xrpcResponsesRelease(NULL);
	xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"snapshot quota enter");
	testRequire(xrpcResponsesRead(Wire,NULL,Scratch,4,&R)==XRPC_RESPONSE_LOCAL_ERROR && !R && xrtErrorKind(xrtGetError())==XERR_MEMORY,"snapshot constructor OOM");xrtClearError();
	testRequire(xrpcResponsesRead(XRT_STR_LITERAL("{}"),NULL,Scratch,4,&R)==XRPC_RESPONSE_LOCAL_ERROR && !R && xrtErrorKind(xrtGetError())==XERR_MEMORY,"envelope diagnostic OOM classification");xrtClearError();
	testRequire(xrtAllocationBudgetLeave(&Scope),"snapshot quota leave");xallocationbudgetinfo BI={0};BI.Size=sizeof(BI);testRequire(xrtAllocationBudgetInfo(Budget,&BI) && !BI.LiveBytes && BI.Rejections,"snapshot quota cleanup");xrtAllocationBudgetFree(Budget);
}
#if defined(XRT_FEATURE_THREAD)
typedef struct rpcresponsethread {xrpcresponses* R;size_t Completed;} rpcresponsethread;
static int32 rpcResponseThread(ptr User)
{rpcresponsethread* C=User;for(size_t K=0;K<20000;++K){xrpcresponseinfo I=rpcResponsesGet(C->R);if(I.Count!=1 || I.Messages[0].Result.Raw.Data[0]!='[')return 1;++C->Completed;}xrpcResponsesRelease(C->R);return 0;}
static void testRpcResponseThreads(void)
{
	xrpcresponses* R=rpcResponsesNew(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":[1,2,3]}"));rpcresponsethread C[4]={{0}};xthread* T[4];
	for(unsigned K=0;K<4;++K){testRequire(xrpcResponsesRetain(R),"reader ref");C[K].R=R;T[K]=xrtThreadCreate(rpcResponseThread,&C[K],0);testRequire(T[K]!=NULL,"snapshot thread create");}xrpcResponsesRelease(R);
	for(unsigned K=0;K<4;++K){testRequire(xrtThreadWait(T[K])==XWAIT_OK && xrtThreadExitCode(T[K])==0 && C[K].Completed==20000,"concurrent snapshot readers");xrtThreadDestroy(T[K]);}
}
#endif
static void testRpcResponseAllocation(void)
{
	xstrview Wire=XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":null}");xrpcresponses* R=rpcResponsesNew(Wire);xrpcResponsesRelease(R);
	xrtSetErrorInfo(XERR_VALUE,"xrt.test",777,"ambient");const xerror* Ambient=xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
	#endif
	for(unsigned K=0;K<10000;++K){R=rpcResponsesNew(Wire);testRequire(xrpcResponsesRetain(R),"snapshot extra ref");xrpcResponsesRelease(R);xrpcResponsesRelease(R);testRequire(xrtGetError()==Ambient,"snapshot success changed ambient error");}
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(After.MallocCalls-Before.MallocCalls==10000 && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls,"snapshot allocation count differs from one per document");
	#endif
	xrtClearError();
}
int main(void)
{
	testRpcResponseOwnership();testRpcResponseErrors();testRpcResponseAllocation();
	#if defined(XRT_FEATURE_THREAD)
	testRpcResponseThreads();
	#endif
	puts("[PASS] xrpc response: complete-envelope/owned-copy/rebased-views/raw-IDs/10000-properties/limits/atomic-output/OOM/concurrent-refs");return 0;
}
