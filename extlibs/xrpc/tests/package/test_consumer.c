#define XRPC_MODULE_XRPC
#include <xrpc.h>
#include <string.h>
int main(void)
{
	xrpcmessage Message; xrpcdocument Doc;
	if (xrpcDecode(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1e9999}"), NULL, &Message, 1, &Doc) != XRPC_DECODE_DONE ||
		Doc.Count != 1 || Message.Kind != XRPC_MESSAGE_CALL || Message.Id.Raw.Size != 6 || memcmp(Message.Id.Raw.Data, "1e9999", 6)) return 1;
	if (xrpcDecode((xstrview){NULL, 1}, NULL, &Message, 1, &Doc) != XRPC_DECODE_LOCAL_ERROR ||
		strcmp(xrtErrorDomain(xrtGetError()), "xrt.rpc") || xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) return 2;
	xrtClearError(); return 0;
}
