#include "../xnet2/bench_common.h"
#include "../../../xrt.h"

static void __benchCoNoop(ptr pArg)
{
	(void)pArg;
}

int main(int argc, char** argv)
{
	uint64_t iIterations = xbenchArgU64(argc, argv, 1, 100000u);
	xbenchtimer tCreateDestroyTimer;
	xbenchtimer tRunDestroyTimer;
	uint64_t iCreateDestroyNs = 0u;
	uint64_t iRunDestroyNs = 0u;

	printf("xrt coroutine bench_create_destroy\n");
	printf("backend=%s\n", xrtCoGetBackendName());
	printf("iterations=%" PRIu64 "\n", iIterations);

	if ( !xrtInit() ) return 1;

	xbenchTimerStart(&tCreateDestroyTimer);
	for ( uint64_t i = 0; i < iIterations; ++i ) {
		xcoro pCo = xrtCoCreate(__benchCoNoop, NULL, 0u);
		if ( !pCo ) {
			xrtUnit();
			return 2;
		}
		xrtCoDestroy(pCo);
	}
	xbenchTimerStop(&tCreateDestroyTimer);
	iCreateDestroyNs = xbenchTimerElapsedNs(&tCreateDestroyTimer);

	xbenchTimerStart(&tRunDestroyTimer);
	for ( uint64_t i = 0; i < iIterations; ++i ) {
		xcoro pCo = xrtCoCreate(__benchCoNoop, NULL, 0u);
		if ( !pCo ) {
			xrtUnit();
			return 3;
		}
		if ( !xrtCoResume(pCo) ) {
			xrtCoDestroy(pCo);
			xrtUnit();
			return 4;
		}
		xrtCoDestroy(pCo);
	}
	xbenchTimerStop(&tRunDestroyTimer);
	iRunDestroyNs = xbenchTimerElapsedNs(&tRunDestroyTimer);

	xbenchPrintMetricU64("create_destroy_elapsed_ns", iCreateDestroyNs);
	xbenchPrintMetricDouble("create_destroy_ops_per_sec", xbenchSafeRate(iIterations, iCreateDestroyNs));
	xbenchPrintMetricDouble("create_destroy_ns_per_op", iIterations ? ((double)iCreateDestroyNs / (double)iIterations) : 0.0);

	xbenchPrintMetricU64("create_resume_destroy_elapsed_ns", iRunDestroyNs);
	xbenchPrintMetricDouble("create_resume_destroy_ops_per_sec", xbenchSafeRate(iIterations, iRunDestroyNs));
	xbenchPrintMetricDouble("create_resume_destroy_ns_per_op", iIterations ? ((double)iRunDestroyNs / (double)iIterations) : 0.0);

	xrtUnit();
	return 0;
}
