#include "../test.h"



/* 当前时刻的系统本地分解必须能用显式偏移无损还原。 */
static void testCurrentLocalRoundtrip(void)
{
	xtime iNow = xrtNow();
	xtime iFixedRoundtrip;
	xtime iLocalRoundtrip;
	xdatetime tLocal;
	xdatetime tDerivedChanged;

	testRequire(xrtTimeLocal(iNow, &tLocal), "system local split failed");
	testRequire((tLocal.Offset > -86400) && (tLocal.Offset < 86400),
		"system local offset is outside the valid range");
	testRequire(xrtTimeMake(&tLocal, &iFixedRoundtrip) &&
		(iFixedRoundtrip == iNow), "local fixed-offset roundtrip failed");
	testRequire(xrtTimeFromLocal(&tLocal, XTIME_FOLD_EARLIER, &iLocalRoundtrip) &&
		(iLocalRoundtrip == iNow), "system local roundtrip failed");

	/* 反解只读取墙钟字段，调用方不需要清理分解时产生的派生字段。 */
	tDerivedChanged = tLocal;
	tDerivedChanged.Offset = 86399;
	tDerivedChanged.Weekday = -1;
	tDerivedChanged.YearDay = -1;
	tDerivedChanged.IsDST = -1;
	testRequire(xrtTimeFromLocal(&tDerivedChanged, XTIME_FOLD_EARLIER,
		&iLocalRoundtrip) && (iLocalRoundtrip == iNow),
		"derived local fields affected wall-time construction");
}



/* 非法参数必须原子失败，平台时间边界则接受支持或原子拒绝。 */
static void testLocalFailures(void)
{
	xdatetime tLocal;
	xdatetime tSaved;
	xtime iBoundary = INT64_MIN + XRT_TIME_DAY;
	xtime iBoundaryRoundtrip;
	xtime iOutput = 123;

	testRequire(xrtTimeLocal(xrtNow(), &tLocal), "local setup failed");
	xrtClearError();
	testRequire(!xrtTimeFromLocal(&tLocal, (xtimefold)99, &iOutput) &&
		(iOutput == 123), "invalid fold policy modified the output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid fold policy reported the wrong error");

	xrtClearError();
	tSaved = tLocal;
	if ( xrtTimeLocal(iBoundary, &tLocal) ) {
		testRequire(xrtTimeMake(&tLocal, &iBoundaryRoundtrip) &&
			(iBoundaryRoundtrip == iBoundary),
			"supported operating system timezone boundary did not roundtrip");
	} else {
		testRequire(memcmp(&tLocal, &tSaved, sizeof(tLocal)) == 0,
			"failed local split modified the output");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorCode(xrtGetError()) == XTIME_ERROR_LOCAL_UNSUPPORTED),
			"unsupported local time reported the wrong error");
	}

	tLocal = tSaved;
	tLocal.Month = 13;
	xrtClearError();
	testRequire(!xrtTimeFromLocal(&tLocal, XTIME_FOLD_EARLIER, &iOutput) &&
		(iOutput == 123), "invalid local date modified the output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XTIME_ERROR_RANGE),
		"invalid local date reported the wrong error");
}



/* 执行本地时区往返和失败原子性测试。 */
int main(void)
{
	testCurrentLocalRoundtrip();
	testLocalFailures();
	return 0;
}
