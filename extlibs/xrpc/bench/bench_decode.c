#include <xrpc.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char** argv)
{
	uint64 Count = argc > 1 ? strtoull(argv[1], NULL, 10) : 100000u, Checksum = 0;
	xstrview Input = XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"params\":[1,true,\"\\u0061\"],\"id\":123456789012345678901234567890}");
	if (Count == 0 || Count > 1000000000u) return 1;
	double Start = xrtTimer();
	for (uint64 Index = 0; Index < Count; ++Index) {
		xrpcmessage Message; xrpcdocument Doc;
		if (xrpcDecode(Input, NULL, &Message, 1, &Doc) != XRPC_DECODE_DONE || Doc.Count != 1 || Message.Kind != XRPC_MESSAGE_CALL) return 2;
		Checksum += Message.Id.Raw.Size + Message.Params.Raw.Size;
	}
	double Seconds = xrtTimer() - Start; if (!(Seconds > 0.0)) return 3;
	printf("messages_per_sec: %.3f\nchecksum: %" PRIu64 "\n", (double)Count/Seconds, Checksum); return 0;
}
