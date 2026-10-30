#include <xrpc.h>
#include <stdio.h>
int main(void)
{
	xrpccall* Call=NULL;xfuture* Future=NULL;xrpccallconfig Config;xrpcCallConfigInit(&Config);Config.Timeout=XRT_WAIT_FOREVER;
	if(!xrpcCallCreate(&Config,&Call,&Future))return 1;
	/* A connection owner normally records actual transport byte acceptance,
	 * handles the wake signal and polls this producer. No I/O occurs here. */
	if(!xrpcCallMarkSent(Call) || !xrtFutureCancel(Future) || xrpcCallPoll(Call)!=XRPC_CALL_FINISHED || !xrpcCallDestroy(Call))return 2;
	xfutureresult Ready;bool OK=xrtFutureResult(Future,&Ready) && Ready.State==XFUTURE_RESOLVED && Ready.Value;
	if(OK){const xrpccallresult* Result=Ready.Value;OK=Result->Status==XRPC_CALL_CANCELLED && Result->Delivery==XRPC_MAY_HAVE_EXECUTED;}
	xrtFutureDestroy(Future);if(!OK)return 3;
	puts("[PASS] cooperative cancellation publishes typed uncertainty; result survives producer destruction");return 0;
}
