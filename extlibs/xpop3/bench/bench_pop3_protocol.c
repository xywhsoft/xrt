#include "../../../dev/bench/bench_common.h"

#define XPOP3_MODULE_POP3
#include <xpop3.h>



/* 测量 POP3 协议零分配解析与命令写出热路径。 */
int main(int argc, char** argv)
{
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 200000u);
	char arrOutput[256];
	size_t iOutputSize;
	uint64 iChecksum = 0;
	xbenchtimer Timer;
	uint64 iPop3Elapsed;

	if ( iIterations == 0 ) {
		return 1;
	}

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xpop3replyview Reply;
		xpop3stat Stat;
		xpop3capabilityview Capability;

		if ( !xrtPop3ReplyParse(
			XRT_STR_LITERAL("+OK mailbox ready"),
			&Reply
		) || !xrtPop3StatParse(
			XRT_STR_LITERAL("+OK 42 1048576"),
			&Stat
		) || !xrtPop3CapabilityParse(
			XRT_STR_LITERAL("SASL PLAIN XOAUTH2"),
			&Capability
		) || !xrtPop3CommandWrite(
			XRT_STR_LITERAL("RETR"),
			XRT_STR_LITERAL("42"),
			arrOutput,
			sizeof(arrOutput),
			&iOutputSize
		) ) {
			return 4;
		}
		iChecksum += Reply.Text.Size + Stat.Messages + Stat.Bytes +
			xrtPop3Capability(Capability.Name) + iOutputSize;
	}
	xbenchTimerStop(&Timer);
	iPop3Elapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt POP3 protocol benchmark\n");
	xbenchPrintMetricDouble(
		"pop3_ops_per_sec",
		xbenchSafeRate(iIterations, iPop3Elapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
