#define XRPC_MODULE_XRPC
#include <xrpc.h>
#include <string.h>

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
int main(void)
{
	xrpcmessage Message; xrpcdocument Doc;
	if (xrpcDecode(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1e9999}"), NULL, &Message, 1, &Doc) != XRPC_DECODE_DONE ||
		Doc.Count != 1 || Message.Kind != XRPC_MESSAGE_CALL || Message.Id.Raw.Size != 6 || memcmp(Message.Id.Raw.Data, "1e9999", 6)) return 1;
	xrpcoutgoing Reply = {0}; Reply.Kind = XRPC_MESSAGE_RESULT; Reply.Id = Message.Id.Raw; Reply.Result = XRT_STR_LITERAL("null");
	char Output[128]; size_t Required, Written;
	if (!xrpcEncodeSize(&Reply, 1, false, NULL, &Required) ||
		!xrpcEncodeWrite(Output, sizeof(Output), &Reply, 1, false, NULL, &Written) || Written != Required ||
		xrpcDecode((xstrview){Output, Written}, NULL, &Message, 1, &Doc) != XRPC_DECODE_DONE ||
		Message.Kind != XRPC_MESSAGE_RESULT || Message.Id.Raw.Size != 6 || memcmp(Message.Id.Raw.Data, "1e9999", 6)) return 3;
	xrpcdispatchinfo Info = {0}; Info.Size = sizeof(Info);
	if (!xrpcDispatch(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1e9999}"), NULL, &Message, 1,
		rpcConsumerCall, NULL, rpcConsumerEmit, Output, &Info) || Info.Invoked != 1 || Info.Replies != 1 || Info.Visited != 1) return 4;
	if (xrpcDispatch(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"abort\",\"id\":1}"), NULL, &Message, 1,
		rpcConsumerCall, NULL, rpcConsumerEmit, Output, &Info) || Info.Invoked != 1 || Info.Replies || xrtErrorCode(xrtGetError()) != XRPC_DISPATCH_ABORTED) return 5;
	xrtClearError();
	xrpcmethod Registration={XRT_STR_LITERAL("m"),rpcConsumerCall,NULL},Found={0};xrpcmethods* Methods=NULL;
	if(!xrpcMethodsCreate(&Registration,1,NULL,&Methods) || !xrpcMethodsRetain(Methods))return 6;
	xrpcMethodsRelease(Methods);
	if(xrpcMethodsFind(Methods,XRT_STR_LITERAL("\"\\u006d\""),&Found)!=XRPC_METHODS_FOUND || Found.Name.Size!=1 || Found.Name.Data[0]!='m')return 7;
	if(!xrpcDispatch(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"\\u006d\",\"id\":1e9999}"),NULL,&Message,1,
		xrpcMethodsCall,Methods,rpcConsumerEmit,Output,&Info) || Info.Replies!=1)return 8;
	xrpcMethodsRelease(Methods);
	if (xrpcDecode((xstrview){NULL, 1}, NULL, &Message, 1, &Doc) != XRPC_DECODE_LOCAL_ERROR ||
		strcmp(xrtErrorDomain(xrtGetError()), "xrt.rpc") || xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) return 2;
	xrtClearError(); return 0;
}
