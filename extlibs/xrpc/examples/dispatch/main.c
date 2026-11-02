/* Synchronous caller-thread dispatch. This example's tiny raw-token resolver
 * recognizes its literal method only; a real method index must compare decoded
 * names, including equivalent JSON escapes. No network worker is involved. */
#include <xrpc.h>
#include <stdio.h>
#include <string.h>

typedef struct output { char Data[2048]; size_t Bytes, Count; } output;
static bool emit(const xrpcoutgoing* Response, bool Batch, size_t EncodedBytes, ptr User)
{
	output* Output = User;
	if (Batch) {
		if (Output->Bytes == sizeof(Output->Data)) return false;
		Output->Data[Output->Bytes++] = Output->Count ? ',' : '[';
	}
	size_t Written = 0;
	if (!xrpcEncodeWrite(Output->Data + Output->Bytes, sizeof(Output->Data) - Output->Bytes,
		Response, 1, false, NULL, &Written) || Written != EncodedBytes) return false;
	Output->Bytes += Written; ++Output->Count; return true;
}
static bool call(const xrpcmessage* Request, xrpcreply* Reply, ptr User)
{
	(void)User;
	if (Request->Method.Raw.Size != 6 || memcmp(Request->Method.Raw.Data, "\"echo\"", 6)) return false;
	xrpcReplyResult(Reply, Request->Params.Kind == XRPC_VALUE_MISSING ? XRT_STR_LITERAL("null") : Request->Params.Raw);
	return true;
}
int main(void)
{
	xstrview Input = XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"params\":[1,\"hello\"],\"id\":1e9999},"
		"{\"jsonrpc\":\"2.0\",\"method\":\"echo\"},1]");
	xrpcmessage Scratch[8]; xrpcdispatchinfo Info = {0}; Info.Size = sizeof(Info); output Output = {0};
	if (!xrpcDispatch(Input, NULL, Scratch, 8, call, NULL, emit, &Output, &Info)) return 1;
	if (Info.Batch && Info.Replies) {
		if (Output.Bytes == sizeof(Output.Data)) return 2;
		Output.Data[Output.Bytes++] = ']';
	}
	if (Output.Bytes != Info.OutputBytes || Info.Replies != 2 || Info.Notifications != 1) return 3;
	if (fwrite(Output.Data, 1, Output.Bytes, stdout) != Output.Bytes) return 4;
	putchar('\n'); return 0;
}
