#include "../../../dev/bench/bench_common.h"

#define XIMAP_MODULE_IMAP
#include <ximap.h>



/* 测量 IMAP 协议零分配解析与命令写出热路径。 */
int main(int argc, char** argv)
{
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 200000u);
	char arrOutput[256];
	size_t iOutputSize;
	uint64 iChecksum = 0;
	xbenchtimer Timer;
	uint64 iImapElapsed;

	if ( iIterations == 0 ) {
		return 1;
	}

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		ximapresponseview Response;
		ximapliteralview Literal;
		ximapnumberview Number;

		if ( !xrtImapResponseParse(
			XRT_STR_LITERAL("* 23 FETCH (BODY[] {4096}"),
			&Response
		) || (xrtImapLiteralParse(
			XRT_STR_LITERAL("* 23 FETCH (BODY[] {4096}"),
			&Literal
		) != XMAIL_NEXT_ITEM) || (xrtImapNumberParse(
			Response.Text,
			&Number
		) != XMAIL_NEXT_ITEM) || !xrtImapCommandWrite(
			XRT_STR_LITERAL("A0000001"),
			XRT_STR_LITERAL("UID"),
			XRT_STR_LITERAL("FETCH 1:* (FLAGS BODY.PEEK[] UID)"),
			XIMAP_COMMAND_LINE_DEFAULT,
			arrOutput,
			sizeof(arrOutput),
			&iOutputSize
		) ) {
			return 5;
		}
		iChecksum += Response.Text.Size + Literal.Size + Number.Number +
			iOutputSize;
	}
	xbenchTimerStop(&Timer);
	iImapElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt IMAP protocol benchmark\n");
	xbenchPrintMetricDouble(
		"imap_ops_per_sec",
		xbenchSafeRate(iIterations, iImapElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
