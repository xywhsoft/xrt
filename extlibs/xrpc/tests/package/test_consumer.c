#define XRPC_MODULE_XRPC
#include <xrpc.h>
#include <string.h>

static bool rpcConsumerCall(const xrpcmessage* Request, xrpcreply* Reply, ptr User)
{
	(void)Request; (void)User; char Local[] = "true";
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
	if (xrpcDecode((xstrview){NULL, 1}, NULL, &Message, 1, &Doc) != XRPC_DECODE_LOCAL_ERROR ||
		strcmp(xrtErrorDomain(xrtGetError()), "xrt.rpc") || xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) return 2;
	xrtClearError(); return 0;
}
