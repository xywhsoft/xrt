#include <xrpc.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct rpcbenchoutput { char Bytes[512]; uint64 Checksum, Replies; } rpcbenchoutput;
static bool rpcBenchEmit(const xrpcoutgoing* Response, bool Batch, size_t EncodedBytes, ptr User)
{
	rpcbenchoutput* Output = User; size_t Written = 0; (void)Batch;
	if (!xrpcEncodeWrite(Output->Bytes, sizeof(Output->Bytes), Response, 1, false, NULL, &Written) || Written != EncodedBytes) return false;
	Output->Checksum += Written + (unsigned char)Output->Bytes[Written-1u]; ++Output->Replies; return true;
}
static bool rpcBenchCall(const xrpcmessage* Request, xrpcreply* Reply, ptr User)
{
	(void)User; xrpcReplyResult(Reply, Request->Params.Raw); return true;
}
int main(int argc, char** argv)
{
	uint64 Count = argc > 1 ? strtoull(argv[1], NULL, 10) : 100000u;
	if (!Count || Count > 1000000000u) return 1;
	xstrview Input = XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"params\":[1,true,\"\\u0061\"],\"id\":1e9999},"
		"{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"params\":[2]},1]");
	xrpcmessage Scratch[3]; xrpcdispatchinfo Info = {0}; Info.Size = sizeof(Info); rpcbenchoutput Output = {0};
	double Started = xrtTimer();
	for (uint64 Index = 0; Index < Count; ++Index) {
		if (!xrpcDispatch(Input, NULL, Scratch, 3, rpcBenchCall, NULL, rpcBenchEmit, &Output, &Info) ||
			Info.Replies != 2 || Info.Invoked != 2 || Info.Notifications != 1 || !Info.Batch) return 2;
		/* This sink measures validated per-object encoding; it does not build
		 * batch delimiters, queue bytes, perform transport I/O or resolve names. */
		Output.Checksum += Info.OutputBytes;
	}
	double Seconds = xrtTimer() - Started;
	if (!(Seconds > 0) || Output.Replies != Count * 2u || !Output.Checksum) return 3;
	printf("documents_per_sec: %.3f\nmessages_per_sec: %.3f\nchecksum: %" PRIu64 "\n", (double)Count / Seconds,
		(double)Count * 3.0 / Seconds, Output.Checksum);
	puts("workload: three-item batch; one call, one notification, one invalid; full decode/dispatch/reply preflight/encode; no name lookup/transport/queue/batch buffer"); return 0;
}
