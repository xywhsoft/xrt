#include <xrpc.h>
#include <string.h>

#if defined(XRPC_FEATURE_XRPC_DISPATCH)
static bool rpcConsumerCall(const xrpcmessage* Request, xrpcreply* Reply, ptr User)
{
	(void)User; char Local[] = "true";
	if (Request->Method.Raw.Size == 7 && !memcmp(Request->Method.Raw.Data, "\"abort\"", 7)) return xrpcReplyAbort(Reply, NULL);
	bool Done = xrpcReplyResult(Reply, (xstrview){Local, sizeof(Local)-1u});
	memset(Local, '?', sizeof(Local)); return Done;
}
static bool rpcConsumerEmit(const xrpcoutgoing* Response, bool Batch, size_t EncodedBytes, ptr User)
{
	if (Batch || EncodedBytes >= 128) return false;
	size_t Written = 0;
	return xrpcEncodeWrite(User, 127, Response, 1, false, NULL, &Written) && Written == EncodedBytes &&
		Written == sizeof("{\"jsonrpc\":\"2.0\",\"result\":true,\"id\":1e9999}")-1u &&
		memcmp(User, "{\"jsonrpc\":\"2.0\",\"result\":true,\"id\":1e9999}", Written) == 0;
}
#endif
#if defined(XRPC_FEATURE_XRPC_SERVER)
static bool rpcConsumerServerCall(const xrpcmessage* Request,xrpcreply* Reply,const xrpcservercontext* Context,ptr User)
{if(xrtCancelRequested(Context->Cancel) || xrtTimer()>=Context->Deadline)return false;return rpcConsumerCall(Request,Reply,User);}
#endif
#if defined(XRPC_FEATURE_XRPC_FRAME)
#define main framePackageExample
#include "../../examples/frame/main.c"
#undef main
#endif
int main(void)
{
#if defined(XRPC_FEATURE_XRPC_FRAME)
    if(framePackageExample())return 1;
#endif
#if defined(XRPC_FEATURE_XRPC_DECODE)
	xrpcmessage Message; xrpcdocument Doc;
	if (xrpcDecode(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1e9999}"), NULL, &Message, 1, &Doc) != XRPC_DECODE_DONE ||
		Doc.Count != 1 || Message.Kind != XRPC_MESSAGE_CALL || Message.Id.Raw.Size != 6 || memcmp(Message.Id.Raw.Data, "1e9999", 6)) return 1;
#endif
#if defined(XRPC_FEATURE_XRPC_ENCODE)
	xrpcoutgoing Reply = {0}; Reply.Kind = XRPC_MESSAGE_RESULT; Reply.Id = (xstrview)XRT_STR_LITERAL("1e9999"); Reply.Result = XRT_STR_LITERAL("null");
	char Output[128]; size_t Required, Written;
	if (!xrpcEncodeSize(&Reply, 1, false, NULL, &Required) ||
		!xrpcEncodeWrite(Output, sizeof(Output), &Reply, 1, false, NULL, &Written) || Written != Required) return 3;
#if defined(XRPC_FEATURE_XRPC_DECODE)
	if(xrpcDecode((xstrview){Output, Written}, NULL, &Message, 1, &Doc) != XRPC_DECODE_DONE ||
		Message.Kind != XRPC_MESSAGE_RESULT || Message.Id.Raw.Size != 6 || memcmp(Message.Id.Raw.Data, "1e9999", 6)) return 3;
#else
	if(Written!=sizeof("{\"jsonrpc\":\"2.0\",\"result\":null,\"id\":1e9999}")-1 ||
		memcmp(Output,"{\"jsonrpc\":\"2.0\",\"result\":null,\"id\":1e9999}",Written))return 3;
#endif
#endif
#if defined(XRPC_FEATURE_XRPC_DISPATCH)
	xrpcdispatchinfo Info = {0}; Info.Size = sizeof(Info);
	if (!xrpcDispatch(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1e9999}"), NULL, &Message, 1,
		rpcConsumerCall, NULL, rpcConsumerEmit, Output, &Info) || Info.Invoked != 1 || Info.Replies != 1 || Info.Visited != 1) return 4;
	if (xrpcDispatch(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"abort\",\"id\":1}"), NULL, &Message, 1,
		rpcConsumerCall, NULL, rpcConsumerEmit, Output, &Info) || Info.Invoked != 1 || Info.Replies || xrtErrorCode(xrtGetError()) != XRPC_DISPATCH_ABORTED) return 5;
	xrtClearError();
#endif
#if defined(XRPC_FEATURE_XRPC_METHODS)
	xrpcmethod Registration={XRT_STR_LITERAL("m"),rpcConsumerCall,NULL},Found={0};xrpcmethods* Methods=NULL;
	if(!xrpcMethodsCreate(&Registration,1,NULL,&Methods) || !xrpcMethodsRetain(Methods))return 6;
	xrpcMethodsRelease(Methods);
	if(xrpcMethodsFind(Methods,XRT_STR_LITERAL("\"\\u006d\""),&Found)!=XRPC_METHODS_FOUND || Found.Name.Size!=1 || Found.Name.Data[0]!='m')return 7;
	if(!xrpcDispatch(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"\\u006d\",\"id\":1e9999}"),NULL,&Message,1,
		xrpcMethodsCall,Methods,rpcConsumerEmit,Output,&Info) || Info.Replies!=1)return 8;
	xrpcMethodsRelease(Methods);
#endif
#if defined(XRPC_FEATURE_XRPC_ID)
	unsigned char IdA[128],IdB[128];size_t IdLeft=0,IdRight=0;
	if(!xrpcIdKeyWrite(IdA,sizeof(IdA),XRT_STR_LITERAL("10e18446744073709551615"),NULL,&IdLeft) ||
		!xrpcIdKeyWrite(IdB,sizeof(IdB),XRT_STR_LITERAL("1e18446744073709551616"),NULL,&IdRight) || IdLeft!=IdRight || memcmp(IdA,IdB,IdLeft))return 9;
#endif
#if defined(XRPC_FEATURE_XRPC_PENDING)
	xrpcpending* Pending=NULL;xrpcpendingtoken Token;xrpcpendingcall Taken;
	if(!xrpcPendingCreate(NULL,&Pending) || xrpcPendingInsert(Pending,XRT_STR_LITERAL("1"),100,NULL,&Token)!=XRPC_PENDING_OK ||
		xrpcPendingTakeId(Pending,XRT_STR_LITERAL("1.00e0"),&Taken)!=XRPC_PENDING_OK || Taken.Token.Serial!=Token.Serial ||
		xrpcPendingTakeToken(Pending,Token,&Taken)!=XRPC_PENDING_MISSING)return 10;
	xrpcPendingDestroy(Pending);
#endif
#if defined(XRPC_FEATURE_XRPC_RECEIVE)
	xrpcpending* ReceivePending=NULL;
	xrpcpendingtoken ReceiveToken;
	if(!xrpcPendingCreate(NULL,&ReceivePending) || xrpcPendingInsert(ReceivePending,XRT_STR_LITERAL("1"),100,NULL,&ReceiveToken)!=XRPC_PENDING_OK)return 11;
	xrpcpendingresolution Resolution;xrpcreceiveinfo Received={0};Received.Size=sizeof(Received);xrpcresponses* Responses=NULL;
	if(xrpcReceive(ReceivePending,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1.0,\"result\":[1,2,3]}"),NULL,&Message,1,&Resolution,1,&Responses,&Received)!=XRPC_RESPONSE_DONE || Received.Matched!=1)return 12;
	xrpcresponseinfo Snapshot={0};Snapshot.Size=sizeof(Snapshot);
	if(!xrpcResponsesRetain(Responses))return 13;
	xrpcResponsesRelease(Responses);
	if(!xrpcResponsesInfo(Responses,&Snapshot) || Snapshot.Count!=1 || Snapshot.Messages[0].Result.Raw.Size!=7)return 14;
	xrpcResponsesRelease(Responses);xrpcPendingDestroy(ReceivePending);
#endif
#if defined(XRPC_FEATURE_XRPC_CALL)
	xrpccall* Call=NULL;xfuture* Future=NULL;xrpccallconfig CallConfig;xrpcCallConfigInit(&CallConfig);CallConfig.Timeout=XRT_WAIT_FOREVER;
	if(!xrpcCallCreate(&CallConfig,&Call,&Future) || !xrpcCallMarkSent(Call) || !xrtFutureCancel(Future) || xrpcCallPoll(Call)!=XRPC_CALL_FINISHED || !xrpcCallDestroy(Call))return 15;
	xfutureresult Ready;if(!xrtFutureResult(Future,&Ready) || Ready.State!=XFUTURE_RESOLVED || !Ready.Value ||
		((const xrpccallresult*)Ready.Value)->Status!=XRPC_CALL_CANCELLED || ((const xrpccallresult*)Ready.Value)->Delivery!=XRPC_MAY_HAVE_EXECUTED)return 16;
	xrtFutureDestroy(Future);
#endif
#if defined(XRPC_FEATURE_XRPC_SERVER)
	xfuture* ServerFuture=NULL;xfutureresult ServerReady;
	xrpcserverconfig ServerConfig;xrpcServerConfigInit(&ServerConfig);ServerConfig.Call=rpcConsumerServerCall;xrpcserver* Server=NULL;
	if(!xrpcServerCreate(&ServerConfig,&Server) || xrpcServerSubmit(Server,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1e9999}"),5000,&ServerFuture)!=XRPC_SERVER_ACCEPTED ||
		xrtFutureWaitFor(ServerFuture,5000)!=XWAIT_OK || !xrtFutureResult(ServerFuture,&ServerReady) || ServerReady.State!=XFUTURE_RESOLVED || !ServerReady.Value || !xrpcServerDestroy(Server))return 17;
	const xrpcserverresult* Served=ServerReady.Value;
	if(Served->Status!=XRPC_SERVER_DONE || Served->Dispatch.Replies!=1 || Served->Wire.Size!=sizeof("{\"jsonrpc\":\"2.0\",\"result\":true,\"id\":1e9999}")-1u ||
		memcmp(Served->Wire.Data,"{\"jsonrpc\":\"2.0\",\"result\":true,\"id\":1e9999}",Served->Wire.Size))return 18;
	xrtFutureDestroy(ServerFuture);
#endif
#if defined(XRPC_FEATURE_XRPC_DECODE)
	if (xrpcDecode((xstrview){NULL, 1}, NULL, &Message, 1, &Doc) != XRPC_DECODE_LOCAL_ERROR ||
		strcmp(xrtErrorDomain(xrtGetError()), "xrt.rpc") || xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) return 2;
	xrtClearError();
#endif
	return 0;
}
