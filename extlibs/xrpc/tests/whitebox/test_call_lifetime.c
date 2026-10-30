#include "../test.h"
#include "../../src/response/response.c"
#include "../../src/call/call.c"
int main(void)
{
	xrpcmessage Scratch;xrpcresponses* R=NULL;xrpccall* C=NULL;xfuture* F=NULL;xrpccallconfig Config;xrpcCallConfigInit(&Config);Config.Timeout=XRT_WAIT_FOREVER;
	testRequire(xrpcCallCreate(&Config,&C,&F) && xrpcResponsesRead(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":true}"),NULL,&Scratch,1,&R)==XRPC_RESPONSE_DONE,"call private setup");
	xrtAtomic32Store(&R->References,UINT32_MAX,XMEMORY_RELAXED);
	testRequire(xrpcCallFinish(C,XRPC_CALL_OK,R,0,NULL)==XRPC_CALL_FINISH_ERROR && xrtFutureState(F)==XFUTURE_PENDING && C->Promise && C->Result &&
		xrtAtomic32Load(&C->References,XMEMORY_RELAXED)==2,"response ref overflow changed producer");xrtClearError();xrtAtomic32Store(&R->References,1,XMEMORY_RELAXED);
	xrtAtomic32Store(&C->References,UINT32_MAX,XMEMORY_RELAXED);
	testRequire(xrpcCallFinish(C,XRPC_CALL_OK,R,0,NULL)==XRPC_CALL_FINISH_ERROR && xrtFutureState(F)==XFUTURE_PENDING &&
		xrtAtomic32Load(&R->References,XMEMORY_RELAXED)==1,"activation ref overflow consumed response/state");xrtClearError();xrtAtomic32Store(&C->References,2,XMEMORY_RELAXED);
	testRequire(xrpcCallFinish(C,XRPC_CALL_OK,R,0,NULL)==XRPC_CALL_FINISHED,"retry valid completion after ref failure");
	printf("[PASS] checked payload-ref overflow/retry; producer=%zu config=%zu result=%zu\n",sizeof(xrpccall),sizeof(xrpccallconfig),sizeof(xrpccallresult));
	testRequire(xrpcCallDestroy(C),"call private destroy");xrpcResponsesRelease(R);xrtFutureDestroy(F);return 0;
}
