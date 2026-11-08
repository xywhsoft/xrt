#include <xrpc.h>
#include <stdio.h>

int main(void)
{
	xrpcpending* Pending=NULL;xrpcpendingtoken A,B;xrpcpendingcall Call;
	if(!xrpcPendingCreate(NULL,&Pending))return 1;
	/* One serialized connection owner admits, correlates, cancels and expires.
	 * User points to application-owned call state; a real runtime completes its
	 * future after taking this record. This example uses arbitrary tick units. */
	if(xrpcPendingInsert(Pending,XRT_STR_LITERAL("1"),100,NULL,&A)!=XRPC_PENDING_OK ||
		xrpcPendingTakeId(Pending,XRT_STR_LITERAL("1.00e0"),&Call)!=XRPC_PENDING_OK || Call.Token.Serial!=A.Serial)return 2;
	if(xrpcPendingTakeToken(Pending,A,&Call)!=XRPC_PENDING_MISSING)return 3;
	if(xrpcPendingInsert(Pending,XRT_STR_LITERAL("\"second\""),200,NULL,&B)!=XRPC_PENDING_OK ||
		xrpcPendingExpire(Pending,199,&Call)!=XRPC_PENDING_MISSING ||
		xrpcPendingExpire(Pending,200,&Call)!=XRPC_PENDING_OK || Call.Token.Serial!=B.Serial)return 4;
	xrpcPendingDestroy(Pending);
	puts("[PASS] pending response wins once; stale cancellation is missing; next call expires exactly at its deadline");return 0;
}
