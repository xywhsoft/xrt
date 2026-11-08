#include "test.h"

static xrpcpending* rpcReceivePending(void)
{xrpcpending* P=NULL;testRequire(xrpcPendingCreate(NULL,&P),"receive pending create");return P;}
static size_t rpcReceiveCount(xrpcpending* P)
{xrpcpendinginfo I={0};I.Size=sizeof(I);testRequire(xrpcPendingInfo(P,&I),"receive pending info");return I.Count;}
static void testRpcReceiveProperties(void)
{
	xrpcpending* P=rpcReceivePending();xrpcmessage Scratch[16];xrpcpendingresolution Matches[16];int Users[8]={0};
	xrtSetErrorInfo(XERR_VALUE,"xrt.test",777,"ambient");const xerror* Ambient=xrtGetError();
	for(unsigned Trial=0;Trial<10000;++Trial){char Wire[2048];size_t Bytes=0;xrpcpendingtoken Tokens[8];
		Wire[Bytes++]='[';for(unsigned K=0;K<8;++K){char Id[32];int N=snprintf(Id,sizeof(Id),"%u",K);
			testRequire(xrpcPendingInsert(P,(xstrview){Id,(size_t)N},100+K,&Users[K],&Tokens[K])==XRPC_PENDING_OK,"receive fixture insert");
			if(K)Wire[Bytes++]=',';
			if(K&1u)Bytes+=(size_t)snprintf(Wire+Bytes,sizeof(Wire)-Bytes,"{\"jsonrpc\":\"2.0\",\"id\":%u,\"error\":{\"code\":-1,\"message\":\"m\",\"data\":%u}}",K,Trial);
			else Bytes+=(size_t)snprintf(Wire+Bytes,sizeof(Wire)-Bytes,"{\"jsonrpc\":\"2.0\",\"id\":%u,\"result\":[%u,\"\\u0000\"]}",K,Trial);}
		Bytes+=(size_t)snprintf(Wire+Bytes,sizeof(Wire)-Bytes,",{\"jsonrpc\":\"2.0\",\"id\":-0.000e99999,\"result\":true},{\"jsonrpc\":\"2.0\",\"id\":999,\"result\":false}]");
		xrpcreceiveinfo Info={0};Info.Size=sizeof(Info);xrpcresponses* R=NULL;
		testRequire(xrpcReceive(P,(xstrview){Wire,Bytes},NULL,Scratch,16,Matches,16,&R,&Info)==XRPC_RESPONSE_DONE &&
			Info.Batch && Info.Count==10 && Info.Matched==8 && Info.Repeated==1 && Info.Unknown==1 && !rpcReceiveCount(P),"independent receive count/status oracle");
		for(unsigned K=0;K<8;++K){testRequire(Matches[K].Status==XRPC_PENDING_MATCHED && Matches[K].Call.User==&Users[K] &&
			Matches[K].Call.Token.Serial==Tokens[K].Serial,"receive matched wrong call");xrpcpendingcall Call;
			testRequire(xrpcPendingTakeToken(P,Tokens[K],&Call)==XRPC_PENDING_MISSING,"receive did not remove before publish");}
		testRequire(Matches[8].Status==XRPC_PENDING_REPEATED && Matches[9].Status==XRPC_PENDING_UNKNOWN &&
			!Matches[8].Call.User && !Matches[8].Call.Token.Serial && !Matches[9].Call.Token.Serial,"receive nonmatch stale user");
		memset(Wire,0,sizeof(Wire));memset(Scratch,0,sizeof(Scratch));xrpcresponseinfo RI={0};RI.Size=sizeof(RI);
		testRequire(xrpcResponsesInfo(R,&RI) && RI.Count==10 && RI.Messages[0].Kind==XRPC_MESSAGE_RESULT && RI.Messages[1].Kind==XRPC_MESSAGE_ERROR &&
			RI.Messages[0].Result.Raw.Data[0]=='[',"receive retained snapshot payload");xrpcResponsesRelease(R);
		testRequire(xrtGetError()==Ambient,"receive ordinary outcomes changed ambient error");
	}
	xrpcPendingDestroy(P);xrtClearError();
}
static void rpcReceiveReject(xrpcpending* P,xstrview Input,xrpcresponsestatus Expected,size_t MatchCapacity)
{
	xrpcmessage Scratch[8];xrpcpendingresolution Matches[8];xrpcresponses* R=NULL;xrpcreceiveinfo I;
	memset(&I,0xa5,sizeof(I));I.Size=sizeof(I);xrpcreceiveinfo Before=I;size_t Count=rpcReceiveCount(P);
	testRequire(xrpcReceive(P,Input,NULL,Scratch,8,Matches,MatchCapacity,&R,&I)==Expected && !R && !memcmp(&I,&Before,sizeof(I)) &&
		rpcReceiveCount(P)==Count,"failed receive published/removed calls");xrtClearError();
}
static void testRpcReceiveFailureBoundary(void)
{
	xrpcpending* P=rpcReceivePending();xrpcpendingtoken Token;
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("1"),100,P,&Token)==XRPC_PENDING_OK,"failure-boundary insert");
	rpcReceiveReject(P,XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":true},"),XRPC_RESPONSE_PARSE_ERROR,8);
	rpcReceiveReject(P,XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":true},{}]"),XRPC_RESPONSE_INVALID,8);
	rpcReceiveReject(P,XRT_STR_LITERAL("[]"),XRPC_RESPONSE_INVALID,8);
	rpcReceiveReject(P,XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":true},{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":true}]"),XRPC_RESPONSE_LOCAL_ERROR,1);
	xrpcpendingcall Call;testRequire(xrpcPendingTakeToken(P,Token,&Call)==XRPC_PENDING_OK,"pending call survived failures");xrpcPendingDestroy(P);
	xrpcpendingconfig PC;xrpcPendingConfigInit(&PC);PC.Id.MaxKeyBytes=8;testRequire(xrpcPendingCreate(&PC,&P),"short key table");
	testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("1"),100,NULL,&Token)==XRPC_PENDING_OK,"short key fixture");
	rpcReceiveReject(P,XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":true},{\"jsonrpc\":\"2.0\",\"id\":\"very-long-id\",\"result\":true}]"),XRPC_RESPONSE_LOCAL_ERROR,8);
	testRequire(xrpcPendingTakeToken(P,Token,&Call)==XRPC_PENDING_OK,"late ID/key failure removed early call");xrpcPendingDestroy(P);
	P=rpcReceivePending();testRequire(xrpcPendingInsert(P,XRT_STR_LITERAL("1"),100,NULL,&Token)==XRPC_PENDING_OK,"quota fixture");
	xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"receive quota enter");
	rpcReceiveReject(P,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":null}"),XRPC_RESPONSE_LOCAL_ERROR,8);
	rpcReceiveReject(P,XRT_STR_LITERAL("{}"),XRPC_RESPONSE_LOCAL_ERROR,8);
	testRequire(xrtAllocationBudgetLeave(&Scope),"receive quota leave");xallocationbudgetinfo BI={0};BI.Size=sizeof(BI);
	testRequire(xrtAllocationBudgetInfo(Budget,&BI) && !BI.LiveBytes && BI.Rejections,"receive quota cleanup");xrtAllocationBudgetFree(Budget);
	testRequire(xrpcPendingTakeToken(P,Token,&Call)==XRPC_PENDING_OK,"quota failure consumed call");xrpcPendingDestroy(P);
}
static void testRpcReceiveAliases(void)
{
	xrpcpending* P=rpcReceivePending();xrpcmessage Scratch[4];xrpcpendingresolution Matches[4];xrpcresponses* R=NULL;xrpcreceiveinfo I={0};I.Size=sizeof(I);
	xstrview Input=XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":null}");
	testRequire(xrpcReceive(P,Input,NULL,Scratch,4,Matches,4,(xrpcresponses**)Scratch,&I)==XRPC_RESPONSE_LOCAL_ERROR,"receive result/scratch alias");xrtClearError();
	testRequire(xrpcReceive(P,Input,NULL,Scratch,4,Matches,4,&R,(xrpcreceiveinfo*)Matches)==XRPC_RESPONSE_LOCAL_ERROR,"receive info/matches alias");xrtClearError();
	testRequire(xrpcReceive(P,(xstrview){(cstr)P,1},NULL,Scratch,4,Matches,4,&R,&I)==XRPC_RESPONSE_LOCAL_ERROR,"receive input/table alias");xrtClearError();
	testRequire(xrpcReceive(P,Input,NULL,Scratch,4,(xrpcpendingresolution*)P,1,&R,&I)==XRPC_RESPONSE_LOCAL_ERROR,"receive matches/table alias");xrtClearError();
	testRequire(xrpcReceive(P,Input,NULL,Scratch,4,Matches,4,(xrpcresponses**)P,&I)==XRPC_RESPONSE_LOCAL_ERROR,"receive result/table alias");xrtClearError();
	testRequire(xrpcReceive(P,Input,NULL,Scratch,4,Matches,4,&R,(xrpcreceiveinfo*)P)==XRPC_RESPONSE_LOCAL_ERROR,"receive info/table alias");xrtClearError();
	testRequire(xrpcReceive(P,Input,NULL,NULL,0,Matches,4,&R,&I)==XRPC_RESPONSE_LOCAL_ERROR &&
		xrpcReceive(P,Input,NULL,Scratch,4,NULL,0,&R,&I)==XRPC_RESPONSE_LOCAL_ERROR &&
		xrpcReceive(P,Input,NULL,Scratch,4,Matches,4,NULL,&I)==XRPC_RESPONSE_LOCAL_ERROR &&
		xrpcReceive(P,Input,NULL,Scratch,4,Matches,4,&R,NULL)==XRPC_RESPONSE_LOCAL_ERROR,"receive NULL/capacity");xrtClearError();
	I.Size=0;testRequire(xrpcReceive(P,Input,NULL,Scratch,4,Matches,4,&R,&I)==XRPC_RESPONSE_LOCAL_ERROR && !R,"receive info ABI size");xrtClearError();xrpcPendingDestroy(P);
}
int main(void)
{
	testRpcReceiveProperties();testRpcReceiveFailureBoundary();testRpcReceiveAliases();
	puts("[PASS] xrpc receive: full-document-before-removal/10000-batches/results-errors/repeated-unknown/owned-lifetime/key-capacity-OOM/atomic-pending/aliases");return 0;
}
