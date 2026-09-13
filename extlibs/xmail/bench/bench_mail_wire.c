#include "../../../dev/bench/bench_common.h"

#define XMAIL_MODULE_MAIL_WIRE
#include <xmail.h>



/* 测量邮件线路读写与 dot 编码的零分配热路径。 */
int main(int argc, char** argv)
{
	static const char sWire[] =
		"Header: value\r\n\r\n..first\r\nsecond\r\n";
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 200000u);
	char arrOutput[256];
	size_t iOutputSize;
	uint64 iChecksum = 0;
	xbenchtimer Timer;
	uint64 iWireElapsed;

	if ( iIterations == 0 ) {
		return 1;
	}

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xstrview Line;
		size_t iConsumed;

		if ( (xrtMailLineRead(
			XRT_STR_LITERAL(sWire),
			0,
			&Line,
			&iConsumed
		) != XMAIL_NEXT_ITEM) || !xrtMailDotWrite(
			XRT_STR_LITERAL(sWire),
			true,
			arrOutput,
			sizeof(arrOutput),
			&iOutputSize
		) ) {
			return 2;
		}
		iChecksum += Line.Size + iConsumed + iOutputSize;
	}
	xbenchTimerStop(&Timer);
	iWireElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt Mail wire benchmark\n");
	xbenchPrintMetricDouble(
		"wire_mib_per_sec",
		xbenchSafeRate(
			(uint64)iIterations * (sizeof(sWire) - 1u),
			iWireElapsed
		) / (1024.0 * 1024.0)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
