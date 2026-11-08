#include <xrpc.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
	uint64 Count = argc > 1 ? strtoull(argv[1], NULL, 10) : 100000u, Checksum = 0;
	if (Count == 0 || Count > 1000000000u) return 1;
	xrpcoutgoing Message = {0}; Message.Kind = XRPC_MESSAGE_CALL;
	Message.Id = XRT_STR_LITERAL("123456789012345678901234567890"); Message.Method = XRT_STR_LITERAL("echo");
	Message.Params = XRT_STR_LITERAL("[1,true,\"\\u0061\"]");
	char Output[256]; size_t Written;
	double Start = xrtTimer();
	for (uint64 Index = 0; Index < Count; ++Index) {
		if (!xrpcEncodeWrite(Output, sizeof(Output), &Message, 1, false, NULL, &Written) || Written == 0) return 2;
		Checksum += Written + (unsigned char)Output[Written-1u];
	}
	double Seconds = xrtTimer() - Start; if (!(Seconds > 0.0)) return 3;
	printf("messages_per_sec: %.3f\nchecksum: %" PRIu64 "\n", (double)Count/Seconds, Checksum); return 0;
}
