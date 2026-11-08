#include <xrpc.h>
#include <stdio.h>
int main(void)
{
	xrpcpending* P=NULL;xrpcpendingtoken Token;xrpcresponses* R=NULL;int Exit=1;
	if(!xrpcPendingCreate(NULL,&P))return Exit;
	if(xrpcPendingInsert(P,XRT_STR_LITERAL("1"),100,NULL,&Token)!=XRPC_PENDING_OK)goto Cleanup;
	xrpcmessage Scratch[2];xrpcpendingresolution Matches[2];xrpcreceiveinfo I={0};I.Size=sizeof(I);
	if(xrpcReceive(P,XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":1.0,\"result\":true},{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":false}]"),
		NULL,Scratch,2,Matches,2,&R,&I)!=XRPC_RESPONSE_DONE || I.Matched!=1 || I.Repeated!=1)goto Cleanup;
	/* All removals precede return. Complete matched application state here;
	 * retain R/index for each asynchronous result before releasing this ref. */
	xrpcpendingcall Call;if(xrpcPendingTakeToken(P,Token,&Call)!=XRPC_PENDING_MISSING)goto Cleanup;
	puts("[PASS] complete response batch admitted; duplicate ID completes only one pending call");Exit=0;
Cleanup:
	xrpcResponsesRelease(R);xrpcPendingDestroy(P);return Exit;
}
