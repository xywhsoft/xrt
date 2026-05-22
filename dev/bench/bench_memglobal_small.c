#include "xnet2/bench_common.h"
#include "../../xrt.h"

static void __benchRawLoop(uint32_t iIterations, const uint32_t* arrSizes, uint32_t iSizeCount, bool bUseCalloc)
{
	uint32_t i;

	for ( i = 0; i < iIterations; ++i ) {
		uint32_t iSize = arrSizes[i % iSizeCount];
		void* pMem = bUseCalloc ? calloc(1u, iSize) : malloc(iSize);
		if ( !pMem ) exit(10);
		memset(pMem, 0x5A, iSize);
		free(pMem);
	}
}

static void __benchXrtLoop(uint32_t iIterations, const uint32_t* arrSizes, uint32_t iSizeCount, bool bUseCalloc)
{
	uint32_t i;

	for ( i = 0; i < iIterations; ++i ) {
		uint32_t iSize = arrSizes[i % iSizeCount];
		void* pMem = bUseCalloc ? xrtCalloc(1u, iSize) : xrtMalloc(iSize);
		if ( !pMem ) exit(11);
		memset(pMem, 0x5A, iSize);
		xrtFree(pMem);
	}
}

int main(int argc, char** argv)
{
	static const uint32_t arrFixed32[] = { 32u };
	static const uint32_t arrMixed[] = { 16u, 32u, 48u, 64u, 80u, 96u, 112u, 128u };
	uint32_t iIterations = xbenchArgU32(argc, argv, 1, 500000u);
	xbenchtimer tTimer;
	uint64_t iElapsedRaw32;
	uint64_t iElapsedXrt32;
	uint64_t iElapsedRawMixed;
	uint64_t iElapsedXrtMixed;
	uint64_t iElapsedRawCalloc;
	uint64_t iElapsedXrtCalloc;

	printf("xrt memory bench_memglobal_small\n");
	printf("iterations=%" PRIu32 "\n", iIterations);

	if ( !xrtInit() ) return 1;

	xbenchTimerStart(&tTimer);
	__benchRawLoop(iIterations, arrFixed32, 1u, false);
	xbenchTimerStop(&tTimer);
	iElapsedRaw32 = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	__benchXrtLoop(iIterations, arrFixed32, 1u, false);
	xbenchTimerStop(&tTimer);
	iElapsedXrt32 = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	__benchRawLoop(iIterations, arrMixed, (uint32_t)(sizeof(arrMixed) / sizeof(arrMixed[0])), false);
	xbenchTimerStop(&tTimer);
	iElapsedRawMixed = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	__benchXrtLoop(iIterations, arrMixed, (uint32_t)(sizeof(arrMixed) / sizeof(arrMixed[0])), false);
	xbenchTimerStop(&tTimer);
	iElapsedXrtMixed = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	__benchRawLoop(iIterations, arrFixed32, 1u, true);
	xbenchTimerStop(&tTimer);
	iElapsedRawCalloc = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	__benchXrtLoop(iIterations, arrFixed32, 1u, true);
	xbenchTimerStop(&tTimer);
	iElapsedXrtCalloc = xbenchTimerElapsedNs(&tTimer);

	xbenchPrintMetricU64("raw_fixed32_elapsed_ns", iElapsedRaw32);
	xbenchPrintMetricU64("xrt_fixed32_elapsed_ns", iElapsedXrt32);
	xbenchPrintMetricDouble("raw_fixed32_ops_per_sec", xbenchSafeRate(iIterations, iElapsedRaw32));
	xbenchPrintMetricDouble("xrt_fixed32_ops_per_sec", xbenchSafeRate(iIterations, iElapsedXrt32));
	xbenchPrintMetricDouble("fixed32_speedup_vs_raw", iElapsedXrt32 ? ((double)iElapsedRaw32 / (double)iElapsedXrt32) : 0.0);

	xbenchPrintMetricU64("raw_mixed_elapsed_ns", iElapsedRawMixed);
	xbenchPrintMetricU64("xrt_mixed_elapsed_ns", iElapsedXrtMixed);
	xbenchPrintMetricDouble("raw_mixed_ops_per_sec", xbenchSafeRate(iIterations, iElapsedRawMixed));
	xbenchPrintMetricDouble("xrt_mixed_ops_per_sec", xbenchSafeRate(iIterations, iElapsedXrtMixed));
	xbenchPrintMetricDouble("mixed_speedup_vs_raw", iElapsedXrtMixed ? ((double)iElapsedRawMixed / (double)iElapsedXrtMixed) : 0.0);

	xbenchPrintMetricU64("raw_calloc32_elapsed_ns", iElapsedRawCalloc);
	xbenchPrintMetricU64("xrt_calloc32_elapsed_ns", iElapsedXrtCalloc);
	xbenchPrintMetricDouble("raw_calloc32_ops_per_sec", xbenchSafeRate(iIterations, iElapsedRawCalloc));
	xbenchPrintMetricDouble("xrt_calloc32_ops_per_sec", xbenchSafeRate(iIterations, iElapsedXrtCalloc));
	xbenchPrintMetricDouble("calloc32_speedup_vs_raw", iElapsedXrtCalloc ? ((double)iElapsedRawCalloc / (double)iElapsedXrtCalloc) : 0.0);

	xrtUnit();
	return 0;
}
