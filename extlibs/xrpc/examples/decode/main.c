#include <xrpc.h>
#include <stdio.h>
int main(void)
{
	xrpcmessage Messages[4]; xrpcdocument Doc;
	const xstrview Input = XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"id\":1e9999},{\"jsonrpc\":\"2.0\",\"method\":\"notify\"},false]");
	if (xrpcDecode(Input, NULL, Messages, 4, &Doc) != XRPC_DECODE_DONE) return 1;
	printf("batch=%d messages=%zu invalid=%zu\n", (int)Doc.Batch, Doc.Count, Doc.InvalidCount);
	for (size_t Index = 0; Index < Doc.Count; ++Index)
		printf("message %zu kind=%d raw=%.*s\n", Index, (int)Messages[Index].Kind, (int)Messages[Index].Wire.Size, Messages[Index].Wire.Data);
	return 0;
}
