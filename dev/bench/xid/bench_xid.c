#include "../bench_common.h"

#define XRT_MODULE_XID
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



#define XID_BENCH_BATCH 256u



/* 分别测量单个生成、批量生成、无分配编解码和分配式格式化。 */
int main(int argc, char** argv)
{
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 200000u);
	xid arrValues[XID_BENCH_BATCH];
	xid Parsed;
	char arrText[XID_TEXT_CAPACITY];
	xbenchtimer Timer;
	uint64 iMakeElapsed;
	uint64 iBatchElapsed;
	uint64 iWriteElapsed;
	uint64 iParseElapsed;
	uint64 iFormatElapsed;
	uint64 iChecksum = 0;

	if ( iIterations == 0 ) {
		fprintf(stderr, "iteration count must be non-zero.\n");
		return 1;
	}
	if ( !xrtXidMake(&arrValues[0]) ||
		 !xrtXidWrite(&arrValues[0], arrText, sizeof(arrText)) ) {
		return 2;
	}

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		if ( !xrtXidMake(&arrValues[0]) ) {
			return 3;
		}
		iChecksum += arrValues[0].Data[XID_BINARY_SIZE - 1u];
	}
	xbenchTimerStop(&Timer);
	iMakeElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; ) {
		uint32 iRemain = iIterations - i;
		uint32 iCount = iRemain < XID_BENCH_BATCH ?
			iRemain : XID_BENCH_BATCH;

		if ( !xrtXidMakeMany(arrValues, iCount) ) {
			return 4;
		}
		iChecksum += arrValues[iCount - 1u].Data[XID_BINARY_SIZE - 1u];
		i += iCount;
	}
	xbenchTimerStop(&Timer);
	iBatchElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		if ( !xrtXidWrite(&arrValues[0], arrText, sizeof(arrText)) ) {
			return 5;
		}
		iChecksum += (uint8)arrText[31];
	}
	xbenchTimerStop(&Timer);
	iWriteElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		if ( !xrtXidParse(
			(xstrview){ arrText, XID_TEXT_SIZE },
			&Parsed
		) ) {
			return 6;
		}
		iChecksum += Parsed.Data[XID_BINARY_SIZE - 1u];
	}
	xbenchTimerStop(&Timer);
	iParseElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		str sText = xrtXidFormat(&arrValues[0]);

		if ( sText == NULL ) {
			return 7;
		}
		iChecksum += (uint8)sText[31];
		xrtFree(sText);
	}
	xbenchTimerStop(&Timer);
	iFormatElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt XID benchmark\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	xbenchPrintMetricDouble(
		"make_ops_per_sec",
		xbenchSafeRate(iIterations, iMakeElapsed)
	);
	xbenchPrintMetricDouble(
		"batch_ops_per_sec",
		xbenchSafeRate(iIterations, iBatchElapsed)
	);
	xbenchPrintMetricDouble(
		"write_ops_per_sec",
		xbenchSafeRate(iIterations, iWriteElapsed)
	);
	xbenchPrintMetricDouble(
		"parse_ops_per_sec",
		xbenchSafeRate(iIterations, iParseElapsed)
	);
	xbenchPrintMetricDouble(
		"format_ops_per_sec",
		xbenchSafeRate(iIterations, iFormatElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
