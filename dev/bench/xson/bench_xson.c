#include "../bench_common.h"

#define XRT_MODULE_XSON
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 事件访问基准累计类型、深度、键和扩展载荷尺寸。 */
static xxsonvisitaction benchXsonVisit(
	const xxsonevent* pEvent,
	ptr pUserData
)
{
	uint64* pChecksum = (uint64*)pUserData;

	*pChecksum += (uint64)pEvent->Type + (uint64)pEvent->Depth + 1u;
	if ( pEvent->Key.Type == XVALUE_KEY_STRING ) {
		*pChecksum += (uint64)pEvent->Key.String.Size;
	} else if ( pEvent->Key.Type == XVALUE_KEY_INT ) {
		*pChecksum += (uint64)(pEvent->Key.Integer & 0xFF);
	}
	if ( pEvent->Type == XXSON_EVENT_BYTES ) {
		*pChecksum += (uint64)pEvent->Value.Bytes.Size;
	}
	return XXSON_VISIT_NEXT;
}



/* 分别测量扩展类型 DOM 解析、事件访问和 DOM 序列化热路径。 */
int main(int argc, char** argv)
{
	static const char sXson[] =
		"{\"service\":\"xrt\",\"enabled\":true,"
		"\"blob\":bytes(\"AAECAwQFBgcICQoLDA0ODw==\"),"
		"\"updated\":time(\"2026-07-31T08:00:00+08:00\"),"
		"\"limits\":intmap{-1:1024,7:1048576},"
		"\"roles\":set[\"reader\",\"writer\",\"admin\"],"
		"\"special\":[float(\"nan\"),float(\"inf\")],"
		"\"message\":\"Unicode: \\u4E2D\\u6587\"}";
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 100000u);
	xstrview Text = { sXson, sizeof(sXson) - 1u };
	xxsonreadconfig ReadConfig;
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
	xrtXsonReadConfigInit(&ReadConfig);
	pFixture = xrtXsonParse(Text);
	if ( pFixture == NULL ) {
		return 2;
	}

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xvalue* pValue = xrtXsonParse(Text);

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
			xrtXsonVisit(
				Text,
				&ReadConfig,
				benchXsonVisit,
				&iChecksum
			) != XXSON_VISIT_DONE
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
		str sText = xrtXsonStringify(pFixture, false, &iSize);

		if ( sText == NULL ) {
			xrtValueRelease(pFixture);
			return 5;
		}
		iChecksum += (uint64)iSize;
		xrtFree(sText);
	}
	xbenchTimerStop(&Timer);
	iWriteElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt XSON benchmark\n");
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
