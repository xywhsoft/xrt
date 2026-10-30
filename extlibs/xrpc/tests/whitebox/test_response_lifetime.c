#include "../test.h"
#include "../../src/response/response.c"
int main(void)
{
	xrpcmessage Scratch;xrpcresponses* R=NULL;
	testRequire(xrpcResponsesRead(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":null}"),NULL,&Scratch,1,&R)==XRPC_RESPONSE_DONE,"lifetime snapshot");
	xrtAtomic32Store(&R->References,UINT32_MAX,XMEMORY_RELAXED);
	testRequire(!xrpcResponsesRetain(R) && xrtErrorCode(xrtGetError())==XRPC_RESPONSE_LIMIT &&
		xrtAtomic32Load(&R->References,XMEMORY_RELAXED)==UINT32_MAX,"snapshot reference overflow wrapped");xrtClearError();
	xrtAtomic32Store(&R->References,1,XMEMORY_RELAXED);
	xrpcresponseinfo I={0};I.Size=sizeof(I);testRequire(xrpcResponsesInfo(R,&I),"lifetime info");
	testRequire(!xrpcResponsesInfo(R,(xrpcresponseinfo*)R),"snapshot info/owned alias");xrtClearError();
	printf("[PASS] forced reference overflow and owned alias; header=%zu message=%zu id_view=%zu config=%zu info=%zu owned_one=%zu\n",
		sizeof(xrpcresponses),sizeof(xrpcmessage),sizeof(xstrview),sizeof(xrpcresponseconfig),sizeof(xrpcresponseinfo),I.OwnedBytes);
	xrpcResponsesRelease(R);return 0;
}
