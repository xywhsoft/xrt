#include "../../../dev/bench/bench_common.h"

#define XSMTP_MODULE_SMTP
#include <xsmtp.h>



/* 测量 SMTP 协议零分配解析与命令写出热路径。 */
int main(int argc, char** argv)
{
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 200000u);
	char arrOutput[256];
	size_t iOutputSize;
	uint64 iChecksum = 0;
	xbenchtimer Timer;
	uint64 iSmtpElapsed;

	if ( iIterations == 0 ) {
		return 1;
	}

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xsmtpreplyline Reply;
		xsmtpcapabilityview Capability;
		uint64 iCapabilities = 0;
		uint64 iSizeLimit = 0;

		if ( !xrtSmtpReplyLineParse(
			XRT_STR_LITERAL("250-SIZE 10485760"),
			&Reply
		) || !xrtSmtpCapabilityParse(Reply.Text, &Capability) ||
			!xrtSmtpCapabilityAdd(
				&Capability,
				&iCapabilities,
				&iSizeLimit
			) || !xrtSmtpCommandWrite(
				XRT_STR_LITERAL("MAIL"),
				XRT_STR_LITERAL("FROM:<sender@example.com> SIZE=128"),
				arrOutput,
				sizeof(arrOutput),
				&iOutputSize
			) ) {
			return 3;
		}
		iChecksum += (uint64)Reply.Code + iCapabilities +
			iSizeLimit + iOutputSize;
	}
	xbenchTimerStop(&Timer);
	iSmtpElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt SMTP protocol benchmark\n");
	xbenchPrintMetricDouble(
		"smtp_ops_per_sec",
		xbenchSafeRate(iIterations, iSmtpElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
