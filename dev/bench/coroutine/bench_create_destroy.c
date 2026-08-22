#include "../bench_common.h"

#define XRT_MODULE_COROUTINE
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 最小协程过程立即返回，用于隔离创建、首次恢复和销毁成本。 */
static ptr benchCoroutineNoop(ptr pData)
{
	return pData;
}



/* 分别测量未启动协程和运行完成协程的完整生命周期。 */
int main(int argc, char** argv)
{
	uint64 iIterations = xbenchArgU64(argc, argv, 1, 100000u);
	xbenchtimer CreateTimer;
	xbenchtimer RunTimer;
	uint64 iCreateElapsed;
	uint64 iRunElapsed;

	if ( iIterations == 0 ) {
		return 1;
	}

	xbenchTimerStart(&CreateTimer);
	for ( uint64 i = 0; i < iIterations; i++ ) {
		xcoro* pCoroutine = xrtCoCreate(benchCoroutineNoop, NULL, NULL);

		if ( (pCoroutine == NULL) || !xrtCoDestroy(pCoroutine) ) {
			return 2;
		}
	}
	xbenchTimerStop(&CreateTimer);
	iCreateElapsed = xbenchTimerElapsedNs(&CreateTimer);

	xbenchTimerStart(&RunTimer);
	for ( uint64 i = 0; i < iIterations; i++ ) {
		xcoro* pCoroutine = xrtCoCreate(benchCoroutineNoop, NULL, NULL);

		if (
			(pCoroutine == NULL) ||
			!xrtCoResume(pCoroutine) ||
			!xrtCoDestroy(pCoroutine)
		) {
			return 3;
		}
	}
	xbenchTimerStop(&RunTimer);
	iRunElapsed = xbenchTimerElapsedNs(&RunTimer);

	printf("coroutine_backend: %s\n", xrtCoBackend());
	xbenchPrintMetricDouble(
		"coroutine_create_destroy_ops_per_sec",
		xbenchSafeRate(iIterations, iCreateElapsed)
	);
	xbenchPrintMetricDouble(
		"coroutine_run_destroy_ops_per_sec",
		xbenchSafeRate(iIterations, iRunElapsed)
	);
	return xrtCoThreadDetach() ? 0 : 4;
}
