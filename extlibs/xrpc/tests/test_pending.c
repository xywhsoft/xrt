#include "test.h"
#include <inttypes.h>

static xstrview rpcPendingView(cstr Text) { return (xstrview){Text,strlen(Text)}; }
static xrpcpending* rpcPendingNew(size_t Capacity)
{
	xrpcpendingconfig Config;xrpcpending* P=NULL;xrpcPendingConfigInit(&Config);Config.MaxCalls=Capacity;
	Config.MaxOwnedBytes=16u*1024u*1024u;
	testRequire(xrpcPendingCreate(&Config,&P) && P,"pending create");return P;
}
static xrpcpendinginfo rpcPendingGet(xrpcpending* P)
{
	xrpcpendinginfo Info={0};Info.Size=sizeof(Info);testRequire(xrpcPendingInfo(P,&Info),"pending info");return Info;
}
static void testRpcPendingLifecycle(void)
{
	xrpcpending* P=rpcPendingNew(2);xrpcpendingtoken A,B,Old,Before={54321,12345};xrpcpendingcall Call;
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("1.00e0"),10,P,&A)==XRPC_PENDING_OK,"pending first insert");Old=A;
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("1"),0,NULL,&Before)==XRPC_PENDING_DUPLICATE && Before.Serial==54321,"pending equivalent duplicate");
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("\"1\""),UINT64_MAX,NULL,&B)==XRPC_PENDING_OK,"pending string numeric distinction");
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("1e0"),0,NULL,&Before)==XRPC_PENDING_DUPLICATE,"duplicate wins when full");
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("2"),0,NULL,&Before)==XRPC_PENDING_FULL && Before.Serial==54321,"pending full atomicity");
	memset(&Call,0xa5,sizeof(Call));xrpcpendingcall Saved=Call;
	testRequire(xrpcPendingExpire(P,9,&Call)==XRPC_PENDING_MISSING && !memcmp(&Call,&Saved,sizeof(Call)),"pending not yet due atomicity");
	testRequire(xrpcPendingExpire(P,10,&Call)==XRPC_PENDING_OK && Call.Token.Serial==A.Serial && Call.User==P && Call.Deadline==10,"pending exact deadline");
	testRequire(xrpcPendingTakeToken(P,A,&Call)==XRPC_PENDING_MISSING,"expired token missing");
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("1"),20,NULL,&A)==XRPC_PENDING_OK && A.Serial>Old.Serial && A.Slot==Old.Slot,"slot reuse serial");
	testRequire(xrpcPendingTakeToken(P,Old,&Call)==XRPC_PENDING_MISSING,"stale cancel cannot remove reused slot");
	testRequire(xrpcPendingTakeId(P,XRT_STR_LITERAL("10e-1"),&Call)==XRPC_PENDING_OK && Call.Token.Serial==A.Serial,"exact numeric response match");
	testRequire(xrpcPendingTakeId(P,XRT_STR_LITERAL("1"),&Call)==XRPC_PENDING_MISSING,"duplicate response missing");
	testRequire(xrpcPendingExpire(P,UINT64_MAX,&Call)==XRPC_PENDING_MISSING && rpcPendingGet(P).Count==1,"never deadline excluded");
	testRequire(xrpcPendingTakeFirst(P,&Call)==XRPC_PENDING_OK && Call.Token.Serial==B.Serial && !Call.User,"shutdown drains never deadline");
	testRequire(xrpcPendingTakeFirst(P,&Call)==XRPC_PENDING_MISSING && !rpcPendingGet(P).Count,"drained empty");
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("\"a\\u0000b\""),0,NULL,&A)==XRPC_PENDING_OK &&
		xrpcPendingTakeId(P,XRT_STR_LITERAL("\"a\\u0000\\u0062\""),&Call)==XRPC_PENDING_OK,"pending NUL escaped equality");
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("10e18446744073709551615"),0,NULL,&A)==XRPC_PENDING_OK &&
		xrpcPendingTakeId(P,XRT_STR_LITERAL("1e18446744073709551616"),&Call)==XRPC_PENDING_OK,"pending huge exponent equality");
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("null"),0,NULL,&A)==XRPC_PENDING_OK &&
		xrpcPendingTakeId(P,XRT_STR_LITERAL(" null "),&Call)==XRPC_PENDING_OK,"pending explicit null");
	char Raw[32]="\"retained\"";testRequire(xrpcPendingInsert(P,rpcPendingView(Raw),1,NULL,&A)==XRPC_PENDING_OK,"pending copied key insert");
	memset(Raw,'x',strlen(Raw));testRequire(xrpcPendingTakeId(P,XRT_STR_LITERAL("\"retained\""),&Call)==XRPC_PENDING_OK,"pending does not retain raw input");
	xrpcPendingDestroy(P);xrpcPendingDestroy(NULL);
}
typedef struct rpcpendingmodel {bool Live;uint64 Deadline;xrpcpendingtoken Token;} rpcpendingmodel;
static uint32 rpcPendingRandom(uint32* State) { *State=*State*1664525u+1013904223u;return *State; }
static void rpcPendingName(char* Raw,unsigned Key,bool Alternate)
{
	if(Key==256)strcpy(Raw,"null");
	else if(Key<128){if(Alternate)snprintf(Raw,64,"%ue-1",Key*10u);else snprintf(Raw,64,"%u",Key);}
	else snprintf(Raw,64,Alternate?"\"\\u0069d-%03u\"":"\"id-%03u\"",Key);
}
static int rpcPendingModelFirst(rpcpendingmodel* Model)
{
	int Best=-1;for(int Key=0;Key<257;++Key)if(Model[Key].Live && (Best<0 || Model[Key].Deadline<Model[Best].Deadline ||
		(Model[Key].Deadline==Model[Best].Deadline && Model[Key].Token.Serial<Model[Best].Token.Serial)))Best=Key;
	return Best;
}
static void testRpcPendingModel(void)
{
	xrpcpending* P=rpcPendingNew(127);rpcpendingmodel Model[257]={0};uint32 State=0x713851u;size_t Count=0;
	for(size_t Trial=0;Trial<100000;++Trial){
		unsigned Key=(rpcPendingRandom(&State)>>8)%257u,Operation=(rpcPendingRandom(&State)>>16)%7u;
		char Raw[64];rpcPendingName(Raw,Key,(Trial&1u)!=0);xrpcpendingtoken Token={54321,12345};
		xrpcpendingcall Call,Before;memset(&Call,0xa5,sizeof(Call));Before=Call;xrpcpendingstatus Status;int Removed=-1;
		if(Operation<=2){uint64 Deadline=(rpcPendingRandom(&State)>>8)%32u;if(Deadline==31)Deadline=UINT64_MAX;
			Status=xrpcPendingInsert(P,rpcPendingView(Raw),Deadline,&Model[Key],&Token);
			testRequire(Status==(Model[Key].Live?XRPC_PENDING_DUPLICATE:Count==127?XRPC_PENDING_FULL:XRPC_PENDING_OK),"model insert result");
			if(Status==XRPC_PENDING_OK){Model[Key]=(rpcpendingmodel){true,Deadline,Token};++Count;}
			else testRequire(Token.Serial==54321 && Token.Slot==12345,"model insert atomicity");
		}else if(Operation==3 || Operation==4){
			Status=Operation==3?xrpcPendingTakeId(P,rpcPendingView(Raw),&Call):xrpcPendingTakeToken(P,Model[Key].Token,&Call);
			testRequire(Status==(Model[Key].Live?XRPC_PENDING_OK:XRPC_PENDING_MISSING),"model response/cancel result");if(Status==XRPC_PENDING_OK)Removed=(int)Key;
		}else{int Best=rpcPendingModelFirst(Model);uint64 Now=(rpcPendingRandom(&State)>>8)%32u;
			bool Due=Best>=0 && (Operation==6 || (Model[Best].Deadline!=UINT64_MAX && Model[Best].Deadline<=Now));
			Status=Operation==6?xrpcPendingTakeFirst(P,&Call):xrpcPendingExpire(P,Now,&Call);
			testRequire(Status==(Due?XRPC_PENDING_OK:XRPC_PENDING_MISSING),"model deadline/drain result");if(Due)Removed=Best;
		}
		if(Removed>=0){testRequire(Call.User==&Model[Removed] && Call.Token.Serial==Model[Removed].Token.Serial &&
			Call.Token.Slot==Model[Removed].Token.Slot && Call.Deadline==Model[Removed].Deadline,"model removed wrong call");Model[Removed].Live=false;--Count;}
		else if(Operation>=3)testRequire(!memcmp(&Call,&Before,sizeof(Call)),"model missing changed output");
		xrpcpendinginfo Info=rpcPendingGet(P);int Best=rpcPendingModelFirst(Model);
		testRequire(Info.Count==Count && Info.Capacity==127 && Info.NextDeadline==(Best<0?UINT64_MAX:Model[Best].Deadline),"model table info");
	}
	xrpcPendingDestroy(P);
}
static void testRpcPendingOrdered(void)
{
	for(unsigned Pattern=0;Pattern<3;++Pattern){xrpcpending* P=rpcPendingNew(4096);xrpcpendingtoken Tokens[4096];
		for(unsigned Index=0;Index<4096;++Index){char Raw[64];snprintf(Raw,sizeof(Raw),"\"long-common-prefix-%08u\"",Pattern==1?4095u-Index:Index);
			testRequire(xrpcPendingInsert(P,rpcPendingView(Raw),100,NULL,&Tokens[Index])==XRPC_PENDING_OK,"ordered insertion");}
		for(unsigned Index=0;Index<4096;++Index){unsigned Position=Pattern==2?(Index*2053u)%4096u:Pattern==1?4095u-Index:Index;xrpcpendingcall Call;
			testRequire(xrpcPendingTakeToken(P,Tokens[Position],&Call)==XRPC_PENDING_OK && Call.Token.Serial==Tokens[Position].Serial,"ordered structural removal");}
		testRequire(!rpcPendingGet(P).Count,"ordered table not empty");xrpcPendingDestroy(P);}
	/* Equal deadlines retain admission order even after token removals. */
	xrpcpending* P=rpcPendingNew(257);xrpcpendingtoken Tokens[257];
	for(unsigned Index=0;Index<257;++Index){char Raw[32];snprintf(Raw,sizeof(Raw),"%u",Index);testRequire(xrpcPendingInsert(P,rpcPendingView(Raw),7,NULL,&Tokens[Index])==XRPC_PENDING_OK,"equal-deadline insert");}
	for(unsigned Index=0;Index<257;++Index){xrpcpendingcall Call;testRequire(xrpcPendingExpire(P,7,&Call)==XRPC_PENDING_OK && Call.Token.Serial==Tokens[Index].Serial,"equal-deadline order");}
	xrpcPendingDestroy(P);
}
static void testRpcPendingBounds(void)
{
	xrpcpendingconfig Config;xrpcpending* P=NULL;testRequire(xrpcPendingConfigInit(&Config),"pending config defaults");
	for(unsigned Field=0;Field<8;++Field){xrpcPendingConfigInit(&Config);
		if(Field==0)Config.Size=0;else if(Field==1)Config.MaxCalls=0;else if(Field==2)Config.MaxOwnedBytes=0;else if(Field==3)Config.Id.Size=0;
		else if(Field==4)Config.Id.MaxInputBytes=0;else if(Field==5)Config.Id.MaxStringBytes=0;else if(Field==6)Config.Id.MaxKeyBytes=0;else Config.Id.MaxKeyBytes=SIZE_MAX;
		testRequire(!xrpcPendingCreate(&Config,&P) && !P,"pending invalid config/overflow atomicity");xrtClearError();}
	P=rpcPendingNew(1);size_t Bytes=rpcPendingGet(P).OwnedBytes;xrpcPendingDestroy(P);P=NULL;
	xrpcPendingConfigInit(&Config);Config.MaxCalls=1;Config.MaxOwnedBytes=Bytes;
	testRequire(xrpcPendingCreate(&Config,&P) && rpcPendingGet(P).OwnedBytes==Bytes,"exact owned-byte limit");xrpcPendingDestroy(P);P=NULL;
	--Config.MaxOwnedBytes;testRequire(!xrpcPendingCreate(&Config,&P) && !P,"short owned-byte limit");xrtClearError();
	xrpcPendingConfigInit(&Config);Config.MaxCalls=1;Config.Id.MaxInputBytes=64;Config.Id.MaxStringBytes=3;Config.Id.MaxKeyBytes=4;
	testRequire(xrpcPendingCreate(&Config,&P),"pending small ID limits");xrpcpendingtoken Token={54321,12345};xrpcpendingcall Call;
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("\"abc\""),0,NULL,&Token)==XRPC_PENDING_OK,"pending exact key/string bound");
	static const cstr Invalid[]={"true","[1]","{}","1e+","null x","\"\\uD800\"","\"abcd\"","12345"};
	for(size_t Index=0;Index<sizeof(Invalid)/sizeof(Invalid[0]);++Index){xrpcpendingtoken Before=Token;memset(&Call,0xa5,sizeof(Call));xrpcpendingcall Saved=Call;
		testRequire(xrpcPendingInsert(P,rpcPendingView(Invalid[Index]),0,NULL,&Token)==XRPC_PENDING_ERROR && Token.Serial==Before.Serial &&
			rpcPendingGet(P).Count==1,"invalid ID admitted/changed state even full");xrtClearError();
		testRequire(xrpcPendingTakeId(P,rpcPendingView(Invalid[Index]),&Call)==XRPC_PENDING_ERROR && !memcmp(&Call,&Saved,sizeof(Call)),"invalid take output");xrtClearError();}
	testRequire(xrpcPendingTakeToken(P,Token,&Call)==XRPC_PENDING_OK,"valid call survived errors");xrpcPendingDestroy(P);
	P=rpcPendingNew(1);char Raw[128]="1";
	testRequire(xrpcPendingInsert(P,rpcPendingView(Raw),0,NULL,(xrpcpendingtoken*)Raw)==XRPC_PENDING_ERROR && Raw[0]=='1',"pending token/input alias");xrtClearError();
	testRequire(xrpcPendingTakeId(P,rpcPendingView(Raw),(xrpcpendingcall*)Raw)==XRPC_PENDING_ERROR && Raw[0]=='1',"pending record/input alias");xrtClearError();
	testRequire(xrpcPendingInsert(P,(xstrview){(cstr)P,1},0,NULL,&Token)==XRPC_PENDING_ERROR,"pending input/table alias");xrtClearError();
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("1"),0,NULL,(xrpcpendingtoken*)P)==XRPC_PENDING_ERROR,"pending token/table alias");xrtClearError();
	testRequire(xrpcPendingTakeFirst(P,(xrpcpendingcall*)P)==XRPC_PENDING_ERROR,"pending record/table alias");xrtClearError();
	testRequire(!xrpcPendingInfo(P,(xrpcpendinginfo*)P),"pending info/table alias");xrtClearError();
	testRequire(!xrpcPendingCreate(&Config,(xrpcpending**)&Config),"pending constructor/config alias");xrtClearError();
	testRequire(!xrpcPendingConfigInit(NULL) && !xrpcPendingCreate(NULL,NULL) && !xrpcPendingInfo(P,NULL) &&
		xrpcPendingInsert(NULL,XRT_STR_LITERAL("1"),0,NULL,&Token)==XRPC_PENDING_ERROR &&
		xrpcPendingTakeToken(NULL,Token,&Call)==XRPC_PENDING_ERROR && xrpcPendingExpire(NULL,0,&Call)==XRPC_PENDING_ERROR,"pending NULL API");xrtClearError();
	xrpcPendingDestroy(P);
}
static void testRpcPendingAllocation(void)
{
	xrpcpending* P=rpcPendingNew(4);xrpcpendingtoken Token;xrpcpendingcall Call;
	xrtSetErrorInfo(XERR_VALUE,"xrt.test",777,"ambient");const xerror* Ambient=xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
	#endif
	for(size_t Index=0;Index<10000;++Index){testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("10e18446744073709551615"),1,NULL,&Token)==XRPC_PENDING_OK &&
		xrpcPendingInsert(P,XRT_STR_LITERAL("1e18446744073709551616"),1,NULL,&Token)==XRPC_PENDING_DUPLICATE &&
		xrpcPendingTakeId(P,XRT_STR_LITERAL("1e18446744073709551616"),&Call)==XRPC_PENDING_OK &&
		xrpcPendingTakeToken(P,Token,&Call)==XRPC_PENDING_MISSING && xrtGetError()==Ambient,"pending steady allocation/error identity");}
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls &&
		Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"pending steady operations allocated");
	#endif
	xrtClearError();xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"pending quota enter");
	xrpcpending* Other=NULL;testRequire(!xrpcPendingCreate(NULL,&Other) && !Other && xrtErrorKind(xrtGetError())==XERR_MEMORY,"pending constructor OOM");xrtClearError();
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("1"),0,NULL,&Token)==XRPC_PENDING_OK && xrpcPendingExpire(P,0,&Call)==XRPC_PENDING_OK,"pending quota success");
	memset(&Call,0xa5,sizeof(Call));xrpcpendingcall Saved=Call;
	testRequire(xrpcPendingTakeId(P,XRT_STR_LITERAL("1e+"),&Call)==XRPC_PENDING_ERROR && !memcmp(&Call,&Saved,sizeof(Call)) &&
		xrtErrorKind(xrtGetError())==XERR_MEMORY && !rpcPendingGet(P).Count,"pending diagnostic OOM atomicity");xrtClearError();
	testRequire(xrtAllocationBudgetLeave(&Scope),"pending quota leave");xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);
	testRequire(xrtAllocationBudgetInfo(Budget,&Info) && !Info.LiveBytes && Info.Rejections,"pending quota cleanup");xrtAllocationBudgetFree(Budget);xrpcPendingDestroy(P);
}
int main(void)
{
	testRpcPendingLifecycle();testRpcPendingModel();testRpcPendingOrdered();testRpcPendingBounds();testRpcPendingAllocation();
	puts("[PASS] xrpc pending: exact-ID/response-cancel-expiry/single-removal/ABA/bounded-AVL/deadline-heap/100k-model/atomicity/zero-heap/OOM");return 0;
}
