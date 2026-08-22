#include "../../../dev/bench/bench_common.h"

#define XHTTP_MODULE_XHTTP
#include <xhttp.h>



/* 测量 URL 解析和 Query 扫描两条无分配协议热路径。 */
int main(int argc, char** argv)
{
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 1000000u);
	xstrview UrlText = XRT_STR_LITERAL(
		"https://user@example.com:8443/a/b?q=hello%20world&n=42#part"
	);
	xstrview QueryText = XRT_STR_LITERAL(
		"q=hello%20world&n=42&empty=&flag"
	);
	xbenchtimer Timer;
	uint64 iUrlElapsed;
	uint64 iQueryElapsed;
	uint64 iChecksum = 0;
	uint32 i;

	if ( iIterations == 0 ) {
		fprintf(stderr, "iteration count must be non-zero.\n");
		return 1;
	}

	xbenchTimerStart(&Timer);
	for ( i = 0; i < iIterations; i++ ) {
		xurl Url;

		if ( !xrtUrlParse(UrlText, &Url) ) {
			return 2;
		}
		iChecksum += (uint64)Url.Path.Size;
	}
	xbenchTimerStop(&Timer);
	iUrlElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( i = 0; i < iIterations; i++ ) {
		xquerypair Pair;
		size_t iOffset = 0;

		while ( xrtQueryNext(
			QueryText, &iOffset, &Pair
		) == XQUERY_NEXT_ITEM ) {
			iChecksum += (uint64)(Pair.Key.Size + Pair.Value.Size);
		}
	}
	xbenchTimerStop(&Timer);
	iQueryElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xhttp extension benchmark\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	xbenchPrintMetricDouble(
		"url_parse_ops_per_sec",
		xbenchSafeRate(iIterations, iUrlElapsed)
	);
	xbenchPrintMetricDouble(
		"query_scan_ops_per_sec",
		xbenchSafeRate(iIterations, iQueryElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
