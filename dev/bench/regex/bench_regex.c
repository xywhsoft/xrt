#include "../bench_common.h"

#define XRT_MODULE_REGEX
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 分别测量编译、matcher 复用、集合分类和替换构建热路径。 */
int main(int argc, char** argv)
{
	const xstrview arrSetPattern[] = {
		{ "error", 5u },
		{ "timeout", 7u },
		{ "disk", 4u },
		{ "network", 7u }
	};
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 100000u);
	xstrview Pattern = XRT_STR_LITERAL("(?<key>[A-Za-z_]+)=(?<value>\\d+)");
	xstrview Text = XRT_STR_LITERAL("width=128 height=72 depth=24 workers=16");
	xregex* pRegex;
	xregexmatcher* pMatcher;
	xregexset* pSet;
	xregexsetmatcher* pSetMatcher;
	xstrbuf Output;
	xbenchtimer Timer;
	uint64 iCompileElapsed;
	uint64 iMatchElapsed;
	uint64 iSetElapsed;
	uint64 iReplaceElapsed;
	uint64 iChecksum = 0;

	if ( iIterations == 0 ) {
		fprintf(stderr, "iteration count must be non-zero.\n");
		return 1;
	}
	pRegex = xrtRegexCompile(Pattern);
	if ( pRegex == NULL ) {
		return 2;
	}
	pMatcher = xrtRegexMatcherCreate(pRegex);
	if ( pMatcher == NULL ) {
		xrtRegexRelease(pRegex);
		return 3;
	}
	pSet = xrtRegexSetCompile(arrSetPattern, 4u);
	if ( pSet == NULL ) {
		xrtRegexMatcherFree(pMatcher);
		xrtRegexRelease(pRegex);
		return 4;
	}
	pSetMatcher = xrtRegexSetMatcherCreate(pSet);
	if ( pSetMatcher == NULL ) {
		xrtRegexSetRelease(pSet);
		xrtRegexMatcherFree(pMatcher);
		xrtRegexRelease(pRegex);
		return 5;
	}
	xrtStrBufInit(&Output);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xregex* pCompiled = xrtRegexCompile(Pattern);

		if ( pCompiled == NULL ) {
			return 6;
		}
		iChecksum += (uint64)xrtRegexCaptureCount(pCompiled);
		xrtRegexRelease(pCompiled);
	}
	xbenchTimerStop(&Timer);
	iCompileElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xregexresult Result = xrtRegexMatcherFind(pMatcher, Text, 0);

		while ( Result == XREGEX_MATCH ) {
			xregexcapture Capture;

			if ( !xrtRegexMatcherCapture(pMatcher, 2u, &Capture) ) {
				return 7;
			}
			iChecksum += (uint64)Capture.Text.Size;
			Result = xrtRegexMatcherNext(pMatcher);
		}
		if ( Result == XREGEX_ERROR ) {
			return 8;
		}
	}
	xbenchTimerStop(&Timer);
	iMatchElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		if ( xrtRegexSetMatcherMatch(
			pSetMatcher,
			XRT_STR_LITERAL("disk timeout"),
			0
		) != XREGEX_MATCH ) {
			return 9;
		}
		iChecksum += (uint64)xrtRegexSetMatcherCount(pSetMatcher);
	}
	xbenchTimerStop(&Timer);
	iSetElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xrtStrBufClear(&Output);
		if ( !xrtRegexReplaceTo(
			pRegex,
			Text,
			XRT_STR_LITERAL("${key}:$2"),
			SIZE_MAX,
			&Output,
			NULL
		) ) {
			return 10;
		}
		iChecksum += (uint64)Output.Size;
	}
	xbenchTimerStop(&Timer);
	iReplaceElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt Regex benchmark\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	xbenchPrintMetricU64("compile_elapsed_ns", iCompileElapsed);
	xbenchPrintMetricDouble(
		"compile_ops_per_sec",
		xbenchSafeRate(iIterations, iCompileElapsed)
	);
	xbenchPrintMetricU64("match_elapsed_ns", iMatchElapsed);
	xbenchPrintMetricDouble(
		"match_ops_per_sec",
		xbenchSafeRate(iIterations, iMatchElapsed)
	);
	xbenchPrintMetricU64("set_elapsed_ns", iSetElapsed);
	xbenchPrintMetricDouble(
		"set_ops_per_sec",
		xbenchSafeRate(iIterations, iSetElapsed)
	);
	xbenchPrintMetricU64("replace_elapsed_ns", iReplaceElapsed);
	xbenchPrintMetricDouble(
		"replace_ops_per_sec",
		xbenchSafeRate(iIterations, iReplaceElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);

	xrtStrBufFree(&Output);
	xrtRegexSetMatcherFree(pSetMatcher);
	xrtRegexSetRelease(pSet);
	xrtRegexMatcherFree(pMatcher);
	xrtRegexRelease(pRegex);
	return 0;
}
