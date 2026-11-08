#include "test.h"

typedef struct rpcmethodcontext { size_t Calls; xstrview Result; } rpcmethodcontext;
static bool rpcMethodHandler(const xrpcmessage* Request, xrpcreply* Reply, ptr User)
{
	(void)Request; rpcmethodcontext* Context = User; ++Context->Calls;
	return xrpcReplyResult(Reply, Context->Result.Size ? Context->Result : XRT_STR_LITERAL("null"));
}
static xstrview rpcMethodView(cstr Text) { return (xstrview){Text,strlen(Text)}; }
static void rpcMethodFound(xrpcmethods* Methods, xstrview Raw, xstrview Name, ptr User)
{
	xrpcmethod Binding = {0};
	testRequire(xrpcMethodsFind(Methods, Raw, &Binding) == XRPC_METHODS_FOUND && Binding.Name.Size == Name.Size &&
		(!Name.Size || memcmp(Binding.Name.Data,Name.Data,Name.Size)==0) && Binding.Call == rpcMethodHandler && Binding.User == User,
		"method lookup changed decoded name/callback/context");
}
static void testRpcMethodsNames(void)
{
	static const xstrview Names[] = {XRT_STR_LITERAL("echo"),XRT_STR_LITERAL(""),XRT_STR_LITERAL("a\0b"),
		XRT_STR_LITERAL("\xe4\xb8\xad\xf0\x9f\x98\x80"),XRT_STR_LITERAL("\"\\/\b\f\n\r\t"),XRT_STR_LITERAL("\xc3\xa9"),
		XRT_STR_LITERAL("e\xcc\x81"),XRT_STR_LITERAL("Echo"),XRT_STR_LITERAL("rpc")};
	static const cstr Queries[] = {"\"e\\u0063ho\"","\"\"","\"a\\u0000b\"","\"\\u4e2d\\uD83D\\uDE00\"", "\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"",
		"\"\\u00e9\"","\"e\\u0301\"","\"Echo\"","\"rpc\""};
	xrpcmethod Source[9]; rpcmethodcontext Contexts[9] = {{0}}; char Mutable[9][32]; size_t Bytes = 0;
	for (size_t Index=0; Index<9; ++Index) {
		memcpy(Mutable[Index],Names[Index].Data,Names[Index].Size); Bytes+=Names[Index].Size;
		Source[Index]=(xrpcmethod){{Mutable[Index],Names[Index].Size},rpcMethodHandler,&Contexts[Index]};
	}
	xrpcmethods* Methods = NULL; testRequire(xrpcMethodsCreate(Source,9,NULL,&Methods),"method constructor names");
	memset(Mutable,'?',sizeof(Mutable)); memset(Source,0,sizeof(Source));
	xrpcmethodsinfo Info={0}; Info.Size=sizeof(Info);
	testRequire(xrpcMethodsInfo(Methods,&Info) && Info.Methods==9 && Info.NameBytes==Bytes && Info.OwnedBytes>Bytes,"method owned info");
	for(size_t Index=0;Index<9;++Index) rpcMethodFound(Methods,rpcMethodView(Queries[Index]),Names[Index],&Contexts[Index]);
	rpcMethodFound(Methods,XRT_STR_LITERAL(" \t\"echo\"\r\n"),Names[0],&Contexts[0]);
	xrpcmethod Before={0},Binding=Before;
	testRequire(xrpcMethodsFind(Methods,XRT_STR_LITERAL("\"ECHO\""),&Binding)==XRPC_METHODS_NOT_FOUND && !memcmp(&Before,&Binding,sizeof(Binding)),"case miss modified binding");
	xrpcMethodsRelease(Methods);
	/* These two distinct names deliberately share FNV-1a/32 fingerprint
	 * 3416675096. Complete-name ordering/equality must separate them. */
	xrpcmethod Collision[2]={{XRT_STR_LITERAL("route-7cbe4da9"),rpcMethodHandler,&Contexts[0]},
		{XRT_STR_LITERAL("route-be6a2117"),rpcMethodHandler,&Contexts[1]}};
	testRequire(xrpcMethodsCreate(Collision,2,NULL,&Methods),"collision index");
	rpcMethodFound(Methods,XRT_STR_LITERAL("\"route-7cbe4da9\""),Collision[0].Name,&Contexts[0]);
	rpcMethodFound(Methods,XRT_STR_LITERAL("\"r\\u006fute-be6a2117\""),Collision[1].Name,&Contexts[1]);
	xrpcMethodsRelease(Methods);
	testRequire(xrpcMethodsCreate(NULL,0,NULL,&Methods),"empty method index");
	testRequire(xrpcMethodsFind(Methods,XRT_STR_LITERAL("\"missing\""),&Binding)==XRPC_METHODS_NOT_FOUND,"empty index miss");
	testRequire(xrpcMethodsFind(Methods,XRT_STR_LITERAL("\"missing\"x"),&Binding)==XRPC_METHODS_ERROR,"empty index skipped complete JSON validation");
	xrtClearError(); xrpcMethodsRelease(Methods); xrpcMethodsRelease(NULL);
}

/* Independent scalar-to-UTF8 oracle, not the production Unicode encoder. */
static size_t rpcMethodUtf8(uint32 Scalar,char* Output)
{
	if(Scalar<0x80){Output[0]=(char)Scalar;return 1;}
	if(Scalar<0x800){Output[0]=(char)(0xc0|(Scalar>>6));Output[1]=(char)(0x80|(Scalar&63));return 2;}
	if(Scalar<0x10000){Output[0]=(char)(0xe0|(Scalar>>12));Output[1]=(char)(0x80|((Scalar>>6)&63));Output[2]=(char)(0x80|(Scalar&63));return 3;}
	Output[0]=(char)(0xf0|(Scalar>>18));Output[1]=(char)(0x80|((Scalar>>12)&63));Output[2]=(char)(0x80|((Scalar>>6)&63));Output[3]=(char)(0x80|(Scalar&63));return 4;
}
static void testRpcMethodsUnicode(void)
{
	size_t Checked=0;
	for(uint32 Base=0;Base<=0x10ffffu;Base+=64u){
		xrpcmethod Source[64]; rpcmethodcontext Contexts[64]={{0}}; uint32 Scalars[64]; char Text[64][4]; size_t Count=0;
		for(uint32 Scalar=Base;Scalar<Base+64u && Scalar<=0x10ffffu;++Scalar){
			if(Scalar>=0xd800 && Scalar<=0xdfff)continue;
			size_t Bytes=rpcMethodUtf8(Scalar,Text[Count]); Scalars[Count]=Scalar;
			Source[Count]=(xrpcmethod){{Text[Count],Bytes},rpcMethodHandler,&Contexts[Count]};++Count;
		}
		if(!Count)continue;
		xrpcmethods* Methods=NULL;testRequire(xrpcMethodsCreate(Source,Count,NULL,&Methods),"scalar batch registration");
		for(size_t Index=0;Index<Count;++Index){
			char Raw[20];uint32 Scalar=Scalars[Index];int Size;
			if(Scalar<0x10000)Size=snprintf(Raw,sizeof(Raw),"\"\\u%04X\"",(unsigned)Scalar);
			else {uint32 Pair=Scalar-0x10000;Size=snprintf(Raw,sizeof(Raw),"\"\\u%04X\\u%04X\"",(unsigned)(0xd800u+(Pair>>10)),(unsigned)(0xdc00u+(Pair&1023)));}
			rpcMethodFound(Methods,(xstrview){Raw,(size_t)Size},Source[Index].Name,&Contexts[Index]);
			if(Scalar>=32 && Scalar!='"' && Scalar!='\\'){
				Raw[0]='"';memcpy(Raw+1,Text[Index],Source[Index].Name.Size);Raw[Source[Index].Name.Size+1u]='"';
				rpcMethodFound(Methods,(xstrview){Raw,Source[Index].Name.Size+2u},Source[Index].Name,&Contexts[Index]);
			}
			++Checked;
		}
		xrpcMethodsRelease(Methods);
	}
	testRequire(Checked==1112064u,"not all Unicode scalars checked");
}

static void testRpcMethodsLargeAndLimits(void)
{
	enum {Count=4096}; xrpcmethod* Source=xrtCalloc(Count,sizeof(*Source)); char (*Names)[16]=xrtMalloc(Count*16u);
	rpcmethodcontext* Contexts=xrtCalloc(Count,sizeof(*Contexts));testRequire(Source && Names && Contexts,"large method fixture");
	for(size_t Index=0;Index<Count;++Index){
		int Size=snprintf(Names[Index],16,"m-%04zu",Index);size_t Slot=(Index*4051u+17u)%Count;
		Source[Slot]=(xrpcmethod){{Names[Index],(size_t)Size},rpcMethodHandler,&Contexts[Index]};
	}
	xrpcmethods* Methods=NULL;testRequire(xrpcMethodsCreate(Source,Count,NULL,&Methods),"unsorted large index");
	memset(Names,'?',Count*16u);xrtFree(Source);xrtFree(Names);
	for(size_t Index=0;Index<Count;++Index){char Name[16],Raw[20];int Length=snprintf(Name,sizeof(Name),"m-%04zu",Index);
		int Size=snprintf(Raw,sizeof(Raw),"\"%s\"",Name);rpcMethodFound(Methods,(xstrview){Raw,(size_t)Size},(xstrview){Name,(size_t)Length},&Contexts[Index]);}
	xrpcmethodsinfo Info={0};Info.Size=sizeof(Info);testRequire(xrpcMethodsInfo(Methods,&Info) && Info.Methods==Count,"large info");
	xrpcMethodsRelease(Methods);xrtFree(Contexts);
	rpcmethodcontext Context={0};xrpcmethod One={XRT_STR_LITERAL("echo"),rpcMethodHandler,&Context};
	testRequire(xrpcMethodsCreate(&One,1,NULL,&Methods),"size bound fixture");Info.Size=sizeof(Info);xrpcMethodsInfo(Methods,&Info);xrpcMethodsRelease(Methods);
	xrpcmethodsconfig Config;xrpcMethodsConfigInit(&Config);Config.MaxMethods=1;Config.MaxNameBytes=4;Config.MaxRawNameBytes=6;Config.MaxTotalNameBytes=4;Config.MaxOwnedBytes=Info.OwnedBytes;
	testRequire(xrpcMethodsCreate(&One,1,&Config,&Methods),"exact constructor bounds");rpcMethodFound(Methods,XRT_STR_LITERAL("\"echo\""),One.Name,&Context);
	xrpcmethod Binding={0};testRequire(xrpcMethodsFind(Methods,XRT_STR_LITERAL(" \"echo\""),&Binding)==XRPC_METHODS_ERROR,"raw query bound excludes whitespace");xrtClearError();xrpcMethodsRelease(Methods);
	for(unsigned Kind=0;Kind<3;++Kind){xrpcMethodsConfigInit(&Config);
		if(Kind==0)Config.MaxNameBytes=3;else if(Kind==1)Config.MaxTotalNameBytes=3;else Config.MaxOwnedBytes=Info.OwnedBytes-1u;
		Methods=(xrpcmethods*)(uintptr_t)1;testRequire(!xrpcMethodsCreate(&One,1,&Config,&Methods) && Methods==(xrpcmethods*)(uintptr_t)1,"constructor bound published output");xrtClearError();}
	xrpcmethod Pair[2]={One,One};Pair[1].Name=XRT_STR_LITERAL("other");xrpcMethodsConfigInit(&Config);Config.MaxMethods=1;Methods=NULL;
	testRequire(!xrpcMethodsCreate(Pair,2,&Config,&Methods) && !Methods && xrtErrorCode(xrtGetError())==XRPC_METHODS_LIMIT,"method count admission limit");xrtClearError();
}

static void testRpcMethodsInvalid(void)
{
	rpcmethodcontext Context={0};xrpcmethod One={XRT_STR_LITERAL("echo"),rpcMethodHandler,&Context};xrpcmethods* Methods=NULL;
	testRequire(xrpcMethodsCreate(&One,1,NULL,&Methods),"invalid query fixture");
	static const cstr Invalid[]={"","null","1","true","[]","{}","\"echo\"x","\"echo\" \"echo\"","\"\\uD800\"","\"\\uDC00\"","\"\\uD800\\u0041\"","\"\\x41\"","\"\xc0\xaf\"","\"\xed\xa0\x80\"","\"\xf4\x90\x80\x80\""};
	for(size_t Index=0;Index<sizeof(Invalid)/sizeof(Invalid[0]);++Index){xrpcmethod Binding;unsigned char Before[sizeof(Binding)];memset(&Binding,0xa5,sizeof(Binding));memcpy(Before,&Binding,sizeof(Binding));
		testRequire(xrpcMethodsFind(Methods,rpcMethodView(Invalid[Index]),&Binding)==XRPC_METHODS_ERROR && !memcmp(&Binding,Before,sizeof(Binding)),"invalid lookup published binding");xrtClearError();}
	xrpcmethod Binding={0};testRequire(xrpcMethodsFind(NULL,XRT_STR_LITERAL("\"echo\""),&Binding)==XRPC_METHODS_ERROR &&
		xrpcMethodsFind(Methods,(xstrview){NULL,1},&Binding)==XRPC_METHODS_ERROR && xrpcMethodsFind(Methods,XRT_STR_LITERAL("\"echo\""),NULL)==XRPC_METHODS_ERROR,"invalid lookup argument");xrtClearError();
	xrpcMethodsRelease(Methods);
	for(unsigned Kind=0;Kind<9;++Kind){xrpcmethodsconfig Config;xrpcMethodsConfigInit(&Config);xrpcmethod Copy=One;
		if(Kind==0)Config.Size=0;else if(Kind==1)Config.MaxMethods=0;else if(Kind==2)Config.MaxNameBytes=0;else if(Kind==3)Config.MaxRawNameBytes=0;
		else if(Kind==4)Config.MaxTotalNameBytes=0;else if(Kind==5)Config.MaxOwnedBytes=0;else if(Kind==6)Copy.Call=NULL;else if(Kind==7)Copy.Name=(xstrview){NULL,1};else Copy.Name=XRT_STR_LITERAL("rpc.internal");
		Methods=NULL;testRequire(!xrpcMethodsCreate(&Copy,1,&Config,&Methods) && !Methods,"invalid constructor binding/config");xrtClearError();}
	xrpcmethod Duplicate[2]={One,One};Methods=NULL;
	testRequire(!xrpcMethodsCreate(Duplicate,2,NULL,&Methods) && !Methods && xrtErrorCode(xrtGetError())==XRPC_METHODS_DUPLICATE,"duplicate name registration");xrtClearError();
	One.Name=XRT_STR_LITERAL("\xc0\xaf");testRequire(!xrpcMethodsCreate(&One,1,NULL,&Methods) && !Methods,"invalid registered UTF8");xrtClearError();
	One.Name=XRT_STR_LITERAL("echo");
	testRequire(!xrpcMethodsCreate(&One,SIZE_MAX,NULL,&Methods) && !xrpcMethodsCreate(NULL,1,NULL,&Methods) &&
		!xrpcMethodsCreate(&One,1,NULL,(xrpcmethods**)&One) && !xrpcMethodsConfigInit(NULL) && !xrpcMethodsRetain(NULL),"invalid constructor/retain API");xrtClearError();
}

typedef struct rpcmethodsoutput {char Data[512];size_t Size,Count;} rpcmethodsoutput;
static bool rpcMethodSink(const xrpcoutgoing* Response,bool Batch,size_t Bytes,ptr User)
{
	rpcmethodsoutput* Output=User;(void)Batch;size_t Written=0;
	if(!xrpcEncodeWrite(Output->Data+Output->Size,sizeof(Output->Data)-Output->Size,Response,1,false,NULL,&Written) || Written!=Bytes)return false;
	Output->Size+=Written;++Output->Count;return true;
}
static void testRpcMethodsDispatch(void)
{
	rpcmethodcontext Context={0};Context.Result=XRT_STR_LITERAL("true");xrpcmethod One={XRT_STR_LITERAL("echo"),rpcMethodHandler,&Context};
	xrpcmethods* Methods=NULL;testRequire(xrpcMethodsCreate(&One,1,NULL,&Methods),"dispatch index");
	xrpcmessage Scratch[4];xrpcdispatchinfo Info={0};Info.Size=sizeof(Info);rpcmethodsoutput Output={0};
	testRequire(xrpcDispatch(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"e\\u0063ho\",\"id\":1e9999}"),NULL,Scratch,4,xrpcMethodsCall,Methods,rpcMethodSink,&Output,&Info) &&
		Context.Calls==1 && Info.Replies==1 && !memcmp(Output.Data,"{\"jsonrpc\":\"2.0\",\"result\":true,\"id\":1e9999}",Info.OutputBytes),"semantic name dispatch/exact id");
	xrpcMethodsRelease(Methods);xrpcmethodsconfig Config;xrpcMethodsConfigInit(&Config);Config.MaxRawNameBytes=3;
	testRequire(xrpcMethodsCreate(&One,1,&Config,&Methods),"local lookup limit fixture");
	for(unsigned Notification=0;Notification<2;++Notification){Output=(rpcmethodsoutput){0};
		xstrview Input=Notification ? XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"echo\"},{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"id\":2}]") :
			XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"id\":1},{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"id\":2}]");
		testRequire(!xrpcDispatch(Input,NULL,Scratch,4,xrpcMethodsCall,Methods,rpcMethodSink,&Output,&Info) && !Output.Count && Info.Invoked==1 && !Info.Visited &&
			Context.Calls==1 && xrtErrorCode(xrtGetError())==XRPC_METHODS_LIMIT,"local lookup failure became wire method error or executed callback");xrtClearError();}
	xrpcMethodsRelease(Methods);
}

static void testRpcMethodsAllocation(void)
{
	rpcmethodcontext Context={0};xrpcmethod One={XRT_STR_LITERAL("echo"),rpcMethodHandler,&Context};xrpcmethods* Methods=NULL;
	testRequire(xrpcMethodsCreate(&One,1,NULL,&Methods),"allocation index");xrpcmethod Binding;
	rpcMethodFound(Methods,XRT_STR_LITERAL("\"e\\u0063ho\""),One.Name,&Context);
	xrtSetErrorInfo(XERR_VALUE,"xrt.test",777,"ambient");const xerror* Ambient=xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
	#endif
	for(size_t Index=0;Index<10000;++Index){testRequire(xrpcMethodsFind(Methods,XRT_STR_LITERAL("\"e\\u0063ho\""),&Binding)==XRPC_METHODS_FOUND &&
		xrpcMethodsFind(Methods,XRT_STR_LITERAL("\"missing\""),&Binding)==XRPC_METHODS_NOT_FOUND && xrtGetError()==Ambient,"lookup ambient identity");}
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(After.MallocCalls==Before.MallocCalls && After.CallocCalls==Before.CallocCalls &&
		After.ReallocCalls==Before.ReallocCalls && After.BlockAllocCalls==Before.BlockAllocCalls,"ordinary method lookup allocated");
	#endif
	xrtClearError();xallocationbudget* Budget=NULL;xallocationscope Scope={0};
	testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"lookup quota");
	testRequire(xrpcMethodsFind(Methods,XRT_STR_LITERAL("\"echo\""),&Binding)==XRPC_METHODS_FOUND &&
		xrpcMethodsFind(Methods,XRT_STR_LITERAL("\"missing\""),&Binding)==XRPC_METHODS_NOT_FOUND,"lookup one-byte quota");
	testRequire(xrpcMethodsFind(Methods,XRT_STR_LITERAL("\"echo\"x"),&Binding)==XRPC_METHODS_ERROR && xrtErrorKind(xrtGetError())==XERR_MEMORY,"lookup diagnostic OOM became miss");
	xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"lookup quota leave");xrtAllocationBudgetFree(Budget);xrpcMethodsRelease(Methods);
	bool Rejected=false,Accepted=false;
	for(size_t Limit=1;Limit<512;++Limit){Budget=NULL;Scope=(xallocationscope){0};Methods=NULL;
		testRequire(xrtAllocationBudgetCreate(Limit,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"constructor quota enter");
		if(xrpcMethodsCreate(&One,1,NULL,&Methods)){Accepted=true;xrpcMethodsRelease(Methods);}else Rejected=true;
		xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"constructor quota leave");xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);
		testRequire(xrtAllocationBudgetInfo(Budget,&Info) && !Info.LiveBytes,"method constructor quota leak");xrtAllocationBudgetFree(Budget);
	}
	testRequire(Rejected && Accepted,"constructor quota thresholds not exercised");
}

#if defined(XRT_FEATURE_THREAD)
typedef struct rpcmethodthread {xrpcmethods* Methods;size_t Completed;} rpcmethodthread;
static int32 rpcMethodThread(ptr User)
{
	rpcmethodthread* Context=User;xrpcmethod Binding;
	for(size_t Index=0;Index<20000;++Index){if(xrpcMethodsFind(Context->Methods,XRT_STR_LITERAL("\"e\\u0063ho\""),&Binding)!=XRPC_METHODS_FOUND)return 1;++Context->Completed;}
	xrpcMethodsRelease(Context->Methods);return 0;
}
static void testRpcMethodsThreads(void)
{
	rpcmethodcontext Handler={0};xrpcmethod One={XRT_STR_LITERAL("echo"),rpcMethodHandler,&Handler};xrpcmethods* Methods=NULL;
	testRequire(xrpcMethodsCreate(&One,1,NULL,&Methods),"threaded index");rpcmethodthread Contexts[4]={{0}};xthread* Threads[4];
	for(unsigned Index=0;Index<4;++Index){testRequire(xrpcMethodsRetain(Methods),"thread reference");Contexts[Index].Methods=Methods;Threads[Index]=xrtThreadCreate(rpcMethodThread,&Contexts[Index],0);testRequire(Threads[Index]!=NULL,"method thread create");}
	xrpcMethodsRelease(Methods);
	for(unsigned Index=0;Index<4;++Index){testRequire(xrtThreadWait(Threads[Index])==XWAIT_OK && xrtThreadExitCode(Threads[Index])==0 && Contexts[Index].Completed==20000,"concurrent method lookup/reference lifetime");xrtThreadDestroy(Threads[Index]);}
}
#endif

int main(void)
{
	testRpcMethodsNames();testRpcMethodsUnicode();testRpcMethodsLargeAndLimits();testRpcMethodsInvalid();testRpcMethodsDispatch();testRpcMethodsAllocation();
	#if defined(XRT_FEATURE_THREAD)
	testRpcMethodsThreads();
	#endif
	puts("[PASS] xrpc methods: immutable-copy/all-Unicode-scalars/escape/collision/4096-unsorted/dispatch/local-abort/limits/zero-heap/OOM/concurrent-lifetime");return 0;
}
