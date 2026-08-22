#include "../bench_common.h"

#define XRT_MODULE_JSON
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 事件访问基准只累计稳定元数据，避免构建 DOM 和被优化器删除。 */
static xjsonvisitaction benchJsonVisit(
	const xjsonevent* pEvent,
	ptr pUserData
)
{
	uint64* pChecksum = (uint64*)pUserData;

	*pChecksum += (uint64)pEvent->Type + (uint64)pEvent->Depth + 1u;
	if ( pEvent->HasName ) {
		*pChecksum += (uint64)pEvent->Name.Size;
	}
	return XJSON_VISIT_NEXT;
}



/* 分别测量 DOM 解析、直接事件访问和 DOM 序列化热路径。 */
int main(int argc, char** argv)
{
	static const char sJson[] =
		"{\"service\":\"xrt\",\"enabled\":true,\"port\":8080,"
		"\"limits\":{\"connections\":10000,\"body\":1048576},"
		"\"routes\":[\"/health\",\"/api/items\",\"/events\"],"
		"\"message\":\"Unicode: \\u4E2D\\u6587 \\uD834\\uDD1E\"}";
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 100000u);
	xstrview Text = { sJson, sizeof(sJson) - 1u };
	xjsonreadconfig ReadConfig;
	xbenchtimer Timer;
	xvalue* pFixture;
	uint64 iParseElapsed;
	uint64 iVisitElapsed;
	uint64 iWriteElapsed;
	uint64 iChecksum = 0;

	if ( iIterations == 0 ) {
		fprintf(stderr, "iteration count must be non-zero.\n");
		return 1;
	}
	xrtJsonReadConfigInit(&ReadConfig);
	pFixture = xrtJsonParse(Text);
	if ( pFixture == NULL ) {
		return 2;
	}

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xvalue* pValue = xrtJsonParse(Text);

		if ( pValue == NULL ) {
			xrtValueRelease(pFixture);
			return 3;
		}
		iChecksum += (uint64)xrtValueCount(pValue);
		xrtValueRelease(pValue);
	}
	xbenchTimerStop(&Timer);
	iParseElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		if (
			xrtJsonVisit(
				Text,
				&ReadConfig,
				benchJsonVisit,
				&iChecksum
			) != XJSON_VISIT_DONE
		) {
			xrtValueRelease(pFixture);
			return 4;
		}
	}
	xbenchTimerStop(&Timer);
	iVisitElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		size_t iSize;
		str sText = xrtJsonStringify(pFixture, false, &iSize);

		if ( sText == NULL ) {
			xrtValueRelease(pFixture);
			return 5;
		}
		iChecksum += (uint64)iSize;
		xrtFree(sText);
	}
	xbenchTimerStop(&Timer);
	iWriteElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt JSON benchmark\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	printf("input_bytes=%u\n", (unsigned)Text.Size);
	xbenchPrintMetricU64("parse_elapsed_ns", iParseElapsed);
	xbenchPrintMetricDouble(
		"parse_ops_per_sec",
		xbenchSafeRate(iIterations, iParseElapsed)
	);
	xbenchPrintMetricDouble(
		"parse_mib_per_sec",
		xbenchSafeRate((uint64)iIterations * Text.Size, iParseElapsed) /
			(1024.0 * 1024.0)
	);
	xbenchPrintMetricU64("visit_elapsed_ns", iVisitElapsed);
	xbenchPrintMetricDouble(
		"visit_ops_per_sec",
		xbenchSafeRate(iIterations, iVisitElapsed)
	);
	xbenchPrintMetricDouble(
		"visit_mib_per_sec",
		xbenchSafeRate((uint64)iIterations * Text.Size, iVisitElapsed) /
			(1024.0 * 1024.0)
	);
	xbenchPrintMetricU64("write_elapsed_ns", iWriteElapsed);
	xbenchPrintMetricDouble(
		"write_ops_per_sec",
		xbenchSafeRate(iIterations, iWriteElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);

	xrtValueRelease(pFixture);
	return 0;
}
