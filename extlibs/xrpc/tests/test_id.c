#include "test.h"
#include <inttypes.h>

static xstrview rpcIdView(cstr Text) { return (xstrview){Text,strlen(Text)}; }
static size_t rpcIdKey(xstrview Input, unsigned char* Output, size_t Capacity)
{
	size_t Required=12345,Written=54321;
	testRequire(xrpcIdKeySize(Input,NULL,&Required) && Required<=Capacity,"ID key size");
	testRequire(xrpcIdKeyWrite(Output,Capacity,Input,NULL,&Written) && Written==Required,"ID key write/size agreement");
	return Written;
}
static void rpcIdEqual(cstr A,cstr B,bool Equal)
{
	unsigned char Left[8192],Right[8192];size_t L=rpcIdKey(rpcIdView(A),Left,sizeof(Left)),R=rpcIdKey(rpcIdView(B),Right,sizeof(Right));
	testRequire((L==R && !memcmp(Left,Right,L))==Equal,"ID semantic equality differs from fixture");
}
static void testRpcIdKnown(void)
{
	static const cstr Ones[]={"1","1.0","1e0","1E+000","10e-1","10000e-4","0.1e1","0.01000e2","100.000e-2"," \t1.000\r\n"};
	for(size_t Index=0;Index<sizeof(Ones)/sizeof(Ones[0]);++Index)rpcIdEqual(Ones[Index],"1",true);
	static const cstr Zeros[]={"0","-0","0.0","-0.000e-99999","0e9999999999999999999999999999999999999","0e-000"};
	for(size_t Index=0;Index<sizeof(Zeros)/sizeof(Zeros[0]);++Index)rpcIdEqual(Zeros[Index],"0",true);
	rpcIdEqual("12.3400e2","1234",true);rpcIdEqual("0.0012300","123e-5",true);rpcIdEqual("-12.3400e2","-1234",true);
	rpcIdEqual("9007199254740992","9007199254740993",false);rpcIdEqual("9007199254740993.0","9007199254740993",true);
	rpcIdEqual("1e99999","2e99999",false);rpcIdEqual("1e-99999","0",false);rpcIdEqual("-1","1",false);
	rpcIdEqual("10e18446744073709551615","1e18446744073709551616",true);
	rpcIdEqual("0.1e-18446744073709551615","1e-18446744073709551616",true);
	rpcIdEqual("1e-1000","10e-1001",true);rpcIdEqual("1e-999","0.1e-998",true);
	rpcIdEqual("\"a\\u0000b\"","\"a\\u0000\\u0062\"",true);
	rpcIdEqual("\"\\u4e2d\\uD83D\\uDE00\"","\"\xe4\xb8\xad\xf0\x9f\x98\x80\"",true);
	rpcIdEqual("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"","\"\\u0022\\u005c/\\u0008\\u000c\\u000a\\u000d\\u0009\"",true);
	rpcIdEqual("\"A\"","\"a\"",false);rpcIdEqual("\"\\u00e9\"","\"e\\u0301\"",false);
	rpcIdEqual("\"1\"","1",false);rpcIdEqual("\"\"","null",false);rpcIdEqual("\"null\"","null",false);
	rpcIdEqual("\"\\u0012+1\\u00000\"","1",false);
	char A[4096],B[4096];
	for(size_t Digits=1;Digits<=3000;Digits=Digits<10?Digits+1:Digits*3){
		memcpy(A,"1e1",3);memset(A+3,'0',Digits);A[Digits+3]=0;
		memcpy(B,"10e",3);memset(B+3,'9',Digits);B[Digits+3]=0;rpcIdEqual(A,B,true);
		memcpy(A,"1e-1",4);memset(A+4,'0',Digits);A[Digits+4]=0;
		memcpy(B,"0.1e-",5);memset(B+5,'9',Digits);B[Digits+5]=0;rpcIdEqual(A,B,true);
	}
}

static uint32 rpcIdRandom(uint32* State) { *State=*State*1664525u+1013904223u;return *State; }
static uint64 rpcIdPower(unsigned Count) { uint64 Value=1;while(Count--)Value*=10;return Value; }
typedef struct rpcidrational {bool Negative;uint64 Numerator,Denominator;} rpcidrational;
/* Small-number oracle uses rational cross multiplication rather than decimal
 * mantissa normalization or exponent digit arithmetic. Generated ranges keep
 * both cross products below 10^18. */
static rpcidrational rpcIdRational(cstr Text)
{
	rpcidrational Value={false,0,1};size_t Position=0;bool Dot=false,ExpNegative=false;unsigned Fraction=0,Exponent=0;
	if(Text[Position]=='-'){Value.Negative=true;++Position;}
	for(;Text[Position] && Text[Position]!='e';++Position){if(Text[Position]=='.')Dot=true;else{Value.Numerator=Value.Numerator*10u+(unsigned)(Text[Position]-'0');if(Dot)++Fraction;}}
	if(Text[Position]){++Position;if(Text[Position]=='-' || Text[Position]=='+')ExpNegative=Text[Position++]=='-';while(Text[Position])Exponent=Exponent*10u+(unsigned)(Text[Position++]-'0');}
	Value.Denominator=rpcIdPower(Fraction);if(ExpNegative)Value.Denominator*=rpcIdPower(Exponent);else Value.Numerator*=rpcIdPower(Exponent);
	if(!Value.Numerator)Value.Negative=false;
	return Value;
}
static void rpcIdSmall(char* Output,uint32* State)
{
	unsigned Coefficient=rpcIdRandom(State)%100000u,Fraction=rpcIdRandom(State)%5u;int Exponent=(int)(rpcIdRandom(State)%9u)-4;
	unsigned Divisor=(unsigned)rpcIdPower(Fraction);bool Negative=((rpcIdRandom(State)>>16)&1u)!=0;
	/* Format decimal/exponent explicitly; no float participates in the oracle. */
	if(Fraction)snprintf(Output,64,"%s%u.%0*ue%d",Negative?"-":"",Coefficient/Divisor,(int)Fraction,Coefficient%Divisor,Exponent);
	else snprintf(Output,64,"%s%ue%d",Negative?"-":"",Coefficient,Exponent);
}
static void testRpcIdProperties(void)
{
	uint32 State=0x124875u;
	for(size_t Trial=0;Trial<20000;++Trial){
		char A[64],B[64];rpcIdSmall(A,&State);rpcIdSmall(B,&State);
		rpcidrational X=rpcIdRational(A),Y=rpcIdRational(B);
		bool Equal=X.Negative==Y.Negative && X.Numerator*Y.Denominator==Y.Numerator*X.Denominator;
		rpcIdEqual(A,B,Equal);
		/* Equivalent alternate spellings exercise every small offset/sign
		 * combination, not just unequal random pairs. */
		unsigned Coefficient=rpcIdRandom(&State)%100000u,Power=rpcIdRandom(&State)%7u;int Exponent=(int)(rpcIdRandom(&State)%41u)-20;
		snprintf(A,sizeof(A),"%ue%d",Coefficient,Exponent);
		snprintf(B,sizeof(B),"%" PRIu64 "e%d",(uint64)Coefficient*rpcIdPower(Power),Exponent-(int)Power);rpcIdEqual(A,B,true);
	}
}

static size_t rpcIdUtf8(uint32 Scalar,char* Output)
{
	if(Scalar<128){Output[0]=(char)Scalar;return 1;}
	if(Scalar<2048){Output[0]=(char)(0xc0|(Scalar>>6));Output[1]=(char)(0x80|(Scalar&63));return 2;}
	if(Scalar<65536){Output[0]=(char)(0xe0|(Scalar>>12));Output[1]=(char)(0x80|((Scalar>>6)&63));Output[2]=(char)(0x80|(Scalar&63));return 3;}
	Output[0]=(char)(0xf0|(Scalar>>18));Output[1]=(char)(0x80|((Scalar>>12)&63));Output[2]=(char)(0x80|((Scalar>>6)&63));Output[3]=(char)(0x80|(Scalar&63));return 4;
}
static void testRpcIdUnicode(void)
{
	unsigned char Key[16];size_t Checked=0;
	for(uint32 Scalar=0;Scalar<=0x10ffffu;++Scalar){
		if(Scalar>=0xd800u && Scalar<=0xdfffu)continue;
		char Raw[16],Text[4];size_t Bytes=rpcIdUtf8(Scalar,Text);int Size;
		if(Scalar<65536)Size=snprintf(Raw,sizeof(Raw),"\"\\u%04x\"",(unsigned)Scalar);
		else{uint32 Pair=Scalar-65536;Size=snprintf(Raw,sizeof(Raw),"\"\\u%04x\\u%04x\"",(unsigned)(0xd800+(Pair>>10)),(unsigned)(0xdc00+(Pair&1023)));}
		size_t Written=rpcIdKey((xstrview){Raw,(size_t)Size},Key,sizeof(Key));
		testRequire(Written==Bytes+1u && !memcmp(Key+1,Text,Bytes),"ID scalar key differs from independent UTF8 oracle");++Checked;
	}
	testRequire(Checked==1112064u,"ID Unicode scalar coverage incomplete");
}

static void rpcIdReject(xstrview Input,const xrpcidconfig* Config)
{
	unsigned char Output[8192],Before[sizeof(Output)];memset(Output,0xa5,sizeof(Output));memcpy(Before,Output,sizeof(Output));
	size_t Required=12345,Written=54321;
	testRequire(!xrpcIdKeySize(Input,Config,&Required) && Required==12345,"failed ID size published output");xrtClearError();
	testRequire(!xrpcIdKeyWrite(Output,sizeof(Output),Input,Config,&Written) && Written==54321 && !memcmp(Output,Before,sizeof(Output)),"failed ID write changed output");xrtClearError();
}
static void testRpcIdBounds(void)
{
	static const cstr Invalid[]={"","true","false","[]","{}","[1]","01","+1","1e","1e+","NaN","Infinity","\"a\"x","null 0","\"\\uD800\"","\"\\uDC00\"","\"\xc0\xaf\"","\"\xed\xa0\x80\""};
	for(size_t Index=0;Index<sizeof(Invalid)/sizeof(Invalid[0]);++Index)rpcIdReject(rpcIdView(Invalid[Index]),NULL);
	xrpcidconfig Config;xrpcIdConfigInit(&Config);Config.MaxInputBytes=3;rpcIdReject(XRT_STR_LITERAL("null"),&Config);
	xrpcIdConfigInit(&Config);Config.MaxStringBytes=2;rpcIdReject(XRT_STR_LITERAL("\"abc\""),&Config);
	xrpcIdConfigInit(&Config);Config.MaxKeyBytes=1;rpcIdReject(XRT_STR_LITERAL("0"),&Config);rpcIdReject(XRT_STR_LITERAL("\"a\""),&Config);
	static const cstr Valid[]={"null","0","-0.1","100000e-10","\"a\\u0000\\uD83D\\uDE00\"","1e9999999999999999999999"};
	for(size_t Index=0;Index<sizeof(Valid)/sizeof(Valid[0]);++Index){unsigned char Expected[8192],Output[8194];xstrview Input=rpcIdView(Valid[Index]);size_t Bytes=rpcIdKey(Input,Expected,sizeof(Expected));
		xrpcIdConfigInit(&Config);Config.MaxKeyBytes=Bytes;size_t Required=0;testRequire(xrpcIdKeySize(Input,&Config,&Required) && Required==Bytes,"exact ID key limit");
		if(Bytes>1){Config.MaxKeyBytes=Bytes-1u;rpcIdReject(Input,&Config);}xrpcIdConfigInit(&Config);
		for(size_t Capacity=0;Capacity<Bytes;++Capacity){memset(Output,0xa5,sizeof(Output));size_t Written=54321;
			testRequire(!xrpcIdKeyWrite(Output+1,Capacity,Input,&Config,&Written) && Written==54321,"ID capacity failure published size");
			for(size_t Byte=0;Byte<sizeof(Output);++Byte)testRequire(Output[Byte]==0xa5,"ID capacity failure changed bytes");
			xrtClearError();}
		memset(Output,0xa5,sizeof(Output));size_t Written=0;testRequire(xrpcIdKeyWrite(Output+1,Bytes,Input,&Config,&Written) && Written==Bytes &&
			!memcmp(Output+1,Expected,Bytes) && Output[0]==0xa5 && Output[Bytes+1u]==0xa5,"ID exact capacity/canaries");
	}
	for(unsigned Field=0;Field<4;++Field){xrpcIdConfigInit(&Config);if(Field==0)Config.Size=0;else if(Field==1)Config.MaxInputBytes=0;else if(Field==2)Config.MaxStringBytes=0;else Config.MaxKeyBytes=0;rpcIdReject(XRT_STR_LITERAL("1"),&Config);}
}

static void testRpcIdAliasesAndAllocation(void)
{
	char Input[64]="1.00e0";size_t Size=12345;unsigned char Output[64];xrpcidconfig Config;xrpcIdConfigInit(&Config);
	testRequire(!xrpcIdConfigInit(NULL) && !xrpcIdKeySize(rpcIdView(Input),NULL,NULL) && !xrpcIdKeyWrite(NULL,0,rpcIdView(Input),NULL,&Size),"ID NULL API");xrtClearError();
	testRequire(!xrpcIdKeySize(rpcIdView(Input),NULL,(size_t*)Input) && !strcmp(Input,"1.00e0"),"ID size/input alias");xrtClearError();
	testRequire(!xrpcIdKeyWrite(Input,sizeof(Input),rpcIdView(Input),NULL,&Size) && !strcmp(Input,"1.00e0"),"ID key/input alias");xrtClearError();
	testRequire(!xrpcIdKeyWrite(&Config,sizeof(Config),rpcIdView(Input),&Config,&Size) && Config.Size==sizeof(Config),"ID key/config alias");xrtClearError();
	testRequire(!xrpcIdKeyWrite(Output,sizeof(Output),rpcIdView(Input),NULL,(size_t*)Output),"ID key/written alias");xrtClearError();
	xstrview Raw=XRT_STR_LITERAL("\"a\\u0000\\uD83D\\uDE00\"");rpcIdKey(Raw,Output,sizeof(Output));
	xrtSetErrorInfo(XERR_VALUE,"xrt.test",777,"ambient");const xerror* Ambient=xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
	#endif
	for(size_t Index=0;Index<10000;++Index){rpcIdKey(Raw,Output,sizeof(Output));rpcIdKey(XRT_STR_LITERAL("10e18446744073709551615"),Output,sizeof(Output));testRequire(xrtGetError()==Ambient,"ID success changed ambient error");}
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"ID key success allocated");
	#endif
	xrtClearError();xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"ID quota enter");
	rpcIdKey(Raw,Output,sizeof(Output));rpcIdKey(XRT_STR_LITERAL("10e18446744073709551615"),Output,sizeof(Output));
	Size=12345;memset(Output,0xa5,sizeof(Output));testRequire(!xrpcIdKeyWrite(Output,sizeof(Output),XRT_STR_LITERAL("1e+"),NULL,&Size) && Size==12345 && Output[0]==0xa5 && xrtErrorKind(xrtGetError())==XERR_MEMORY,"ID diagnostic OOM publication");
	xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"ID quota leave");xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);
	testRequire(xrtAllocationBudgetInfo(Budget,&Info) && !Info.LiveBytes && Info.Rejections,"ID quota leak");xrtAllocationBudgetFree(Budget);
}
int main(void)
{
	testRpcIdKnown();testRpcIdProperties();testRpcIdUnicode();testRpcIdBounds();testRpcIdAliasesAndAllocation();
	puts("[PASS] xrpc ID keys: exact-decimal/huge-exponents/signed-zero/type-domains/all-Unicode/rational-properties/limits/atomicity/zero-heap/OOM");return 0;
}
