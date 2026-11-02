#include <xrpc.h>
#include <stdio.h>

int main(void)
{
	/* A reply echoes the borrowed raw id, without a floating-point conversion. */
	xrpcoutgoing Reply = {0}; Reply.Kind = XRPC_MESSAGE_RESULT;
	Reply.Id = XRT_STR_LITERAL("123456789012345678901234567890");
	Reply.Result = XRT_STR_LITERAL("{\"status\":\"ok\"}");
	char Output[256]; size_t Required, Written;
	if (!xrpcEncodeSize(&Reply, 1, false, NULL, &Required) || Required > sizeof(Output) ||
		!xrpcEncodeWrite(Output, sizeof(Output), &Reply, 1, false, NULL, &Written)) return 1;
	if (fwrite(Output, 1, Written, stdout) != Written) return 2;
	putchar('\n'); return 0;
}
