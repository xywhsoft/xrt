#include <xrt/memory_debug.h>
#include "../test.h"

int main(void)
{
	struct {
		xtime Time;
		int64 Value;
		xtimeunit Unit;
	} Cases[] = {
		{ INT64_MAX, 1, XTIME_UNIT_MONTH },
		{ INT64_MIN, -1, XTIME_UNIT_MONTH },
		{ 0, INT64_C(4000000), XTIME_UNIT_MONTH },
		{ 0, INT64_MAX, XTIME_UNIT_MONTH },
		{ 0, INT64_MAX, XTIME_UNIT_QUARTER },
		{ 0, INT64_MIN, XTIME_UNIT_YEAR },
		{ 0, INT64_MIN, XTIME_UNIT_MONTH }
	};
	size_t iFailures = 0;

	/* 年零的一月对应月份索引零，极小增量能直接到达 INT64_MIN 索引。 */
	testRequire(xrtDate(0, 1, 1, &Cases[6].Time),
		"year-zero arithmetic boundary fixture failed");
	for ( size_t i = 0; i < sizeof(Cases) / sizeof(Cases[0]); i++ ) {
		xtime Result = 123;

		xrtClearError();
		testRequire(!xrtTimeAdd(Cases[i].Time, Cases[i].Value,
			Cases[i].Unit, &Result) && Result == 123 &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE &&
			xrtErrorCode(xrtGetError()) == XTIME_ERROR_OVERFLOW,
			"time overflow control lost its error or modified output");
		xrtClearError();
		/* 逻辑分配故障不会被小对象池绕过，且仅失败一次，后续
		 * 分配恢复成功，才能检出首次诊断的 OOM 被重复构造覆盖。 */
		testRequire(xrtMemDebugFailAfter(0), "time diagnostic fault setup failed");
		bool Added = xrtTimeAdd(Cases[i].Time, Cases[i].Value,
			Cases[i].Unit, &Result);
		bool Preserved = !Added && Result == 123 &&
			xrtMemDebugFailTriggered() &&
			xrtErrorKind(xrtGetError()) == XERR_MEMORY;
		if ( !Preserved ) fprintf(stderr,
			"[diagnostic] time case=%zu triggered=%d kind=%d output=%lld\n",
			i, (int)xrtMemDebugFailTriggered(),
			(int)xrtErrorKind(xrtGetError()), (long long)Result);
		testRequire(Preserved,
			"calendar add overwrote diagnostic OOM or modified output");
		iFailures++;
		xrtClearError();
		xrtMemDebugFailClear();
	}
	testRequire(xrtMemDebugReset(), "time diagnostics retained allocations");
	printf("[PASS] time diagnostic one-shot OOM: %zu controls and %zu faults\n",
		sizeof(Cases) / sizeof(Cases[0]), iFailures);
	return 0;
}
