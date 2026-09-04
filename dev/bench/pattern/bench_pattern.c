#include "../bench_common.h"

#define XRT_MODULE_PATTERN
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



typedef struct pattern_bench_fixture {
	char** Sources;
	char** Queries;
	size_t* QuerySizes;
	xpatternspec* Specs;
	size_t Count;
} pattern_bench_fixture;



static void patternBenchFixtureFree(pattern_bench_fixture* pFixture)
{
	for ( size_t i = 0; i < pFixture->Count; i++ ) {
		free(pFixture->Sources[i]);
		free(pFixture->Queries[i]);
	}
	free(pFixture->Sources);
	free(pFixture->Queries);
	free(pFixture->QuerySizes);
	free(pFixture->Specs);
	memset(pFixture, 0, sizeof(*pFixture));
}



static bool patternBenchFixtureInit(
	pattern_bench_fixture* pFixture,
	size_t iCount,
	bool bAffix
)
{
	memset(pFixture, 0, sizeof(*pFixture));
	pFixture->Sources = (char**)calloc(iCount, sizeof(char*));
	pFixture->Queries = (char**)calloc(iCount, sizeof(char*));
	pFixture->QuerySizes = (size_t*)calloc(iCount, sizeof(size_t));
	pFixture->Specs = (xpatternspec*)calloc(iCount, sizeof(xpatternspec));
	if ( (pFixture->Sources == NULL) || (pFixture->Queries == NULL) ||
		 (pFixture->QuerySizes == NULL) || (pFixture->Specs == NULL) ) {
		patternBenchFixtureFree(pFixture);
		return false;
	}
	pFixture->Count = iCount;
	for ( size_t i = 0; i < iCount; i++ ) {
		char arrPattern[96];
		char arrQuery[96];
		int iPattern = snprintf(
			arrPattern,
			sizeof(arrPattern),
			bAffix ?
				"/api/v1/item-value-{id}-suffix%08zu" :
				"/api/v1/resource%08zu/item/{id}",
			i
		);
		int iQuery = snprintf(
			arrQuery,
			sizeof(arrQuery),
			bAffix ?
				"/api/v1/item-value-12345-suffix%08zu" :
				"/api/v1/resource%08zu/item/12345",
			i
		);

		pFixture->Sources[i] = (char*)malloc((size_t)iPattern + 1u);
		pFixture->Queries[i] = (char*)malloc((size_t)iQuery + 1u);
		if ( (pFixture->Sources[i] == NULL) ||
			 (pFixture->Queries[i] == NULL) ) {
			patternBenchFixtureFree(pFixture);
			return false;
		}
		memcpy(pFixture->Sources[i], arrPattern, (size_t)iPattern + 1u);
		memcpy(pFixture->Queries[i], arrQuery, (size_t)iQuery + 1u);
		pFixture->Specs[i].Pattern = (xstrview){
			pFixture->Sources[i],
			(size_t)iPattern
		};
		pFixture->QuerySizes[i] = (size_t)iQuery;
		pFixture->Specs[i].Value = (ptr)(uintptr_t)(i + 1u);
	}
	return true;
}



static bool patternBenchRun(
	size_t iCount,
	uint32 iIterations,
	bool bAffix
)
{
	pattern_bench_fixture Fixture;
	xpattern* pPattern;
	xbenchtimer Timer;
	uint64 iCompileNs;
	uint64 iLookupNs;
	uint64 iCaptureNs;
	uint64 iChecksum = 0;
	uint64 iRandom = UINT64_C(0x9e3779b97f4a7c15);
	char arrMetric[96];

	if ( !patternBenchFixtureInit(&Fixture, iCount, bAffix) ) {
		return false;
	}
	xbenchTimerStart(&Timer);
	pPattern = xrtPatternCompileMany(Fixture.Specs, iCount);
	xbenchTimerStop(&Timer);
	if ( pPattern == NULL ) {
		patternBenchFixtureFree(&Fixture);
		return false;
	}
	iCompileNs = xbenchTimerElapsedNs(&Timer);
	for ( uint32 i = 0; i < 20000u; i++ ) {
		xpatternmatch Match;
		size_t iIndex;

		iRandom ^= iRandom << 13u;
		iRandom ^= iRandom >> 7u;
		iRandom ^= iRandom << 17u;
		iIndex = (size_t)(iRandom % iCount);
		(void)xrtPatternLookup(
			pPattern,
			(xstrview){
				Fixture.Queries[iIndex],
				Fixture.QuerySizes[iIndex]
			},
			&Match
		);
		iChecksum += (uint64)(uintptr_t)Match.Value;
	}
	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xpatternmatch Match;
		size_t iIndex;

		iRandom ^= iRandom << 13u;
		iRandom ^= iRandom >> 7u;
		iRandom ^= iRandom << 17u;
		iIndex = (size_t)(iRandom % iCount);
		if ( xrtPatternLookup(
			pPattern,
			(xstrview){
				Fixture.Queries[iIndex],
				Fixture.QuerySizes[iIndex]
			},
			&Match
		) != XPATTERN_MATCH ) {
			return false;
		}
		iChecksum += (uint64)(uintptr_t)Match.Value;
	}
	xbenchTimerStop(&Timer);
	iLookupNs = xbenchTimerElapsedNs(&Timer);
	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xpatternmatch Match;
		xstrview Capture;
		size_t iIndex;

		iRandom ^= iRandom << 13u;
		iRandom ^= iRandom >> 7u;
		iRandom ^= iRandom << 17u;
		iIndex = (size_t)(iRandom % iCount);
		if ( xrtPatternMatch(
			pPattern,
			(xstrview){
				Fixture.Queries[iIndex],
				Fixture.QuerySizes[iIndex]
			},
			&Capture,
			1u,
			&Match
		) != XPATTERN_MATCH ) {
			return false;
		}
		iChecksum += (uint64)Capture.Size;
	}
	xbenchTimerStop(&Timer);
	iCaptureNs = xbenchTimerElapsedNs(&Timer);
	printf("patterns=%zu kind=%s compiled_bytes=%zu\n", iCount,
		bAffix ? "affix" : "field",
		xrtPatternCompiledBytes(pPattern));
	(void)snprintf(
		arrMetric,
		sizeof(arrMetric),
		bAffix ? "pattern_affix_%zu_compile_elapsed_ns" :
			"pattern_%zu_compile_elapsed_ns",
		iCount
	);
	xbenchPrintMetricU64(arrMetric, iCompileNs);
	(void)snprintf(
		arrMetric,
		sizeof(arrMetric),
		bAffix ? "pattern_affix_%zu_lookup_ns_per_op" :
			"pattern_%zu_lookup_ns_per_op",
		iCount
	);
	xbenchPrintMetricDouble(
		arrMetric,
		(double)iLookupNs / (double)iIterations
	);
	(void)snprintf(
		arrMetric,
		sizeof(arrMetric),
		bAffix ? "pattern_affix_%zu_capture_ns_per_op" :
			"pattern_%zu_capture_ns_per_op",
		iCount
	);
	xbenchPrintMetricDouble(
		arrMetric,
		(double)iCaptureNs / (double)iIterations
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	xrtPatternRelease(pPattern);
	patternBenchFixtureFree(&Fixture);
	return true;
}



int main(int argc, char** argv)
{
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 1000000u);
	uint32 iMaximum = xbenchArgU32(argc, argv, 2, 10000u);
	const size_t arrCount[] = {
		1u, 20u, 100u, 500u, 2000u, 10000u, 100000u
	};

	if ( (iIterations == 0) || (iMaximum == 0) || (iMaximum > 100000u) ) {
		fprintf(
			stderr,
			"iteration count and maximum pattern count are invalid.\n"
		);
		return 1;
	}
	printf("xrt Pattern scaling benchmark\n");
	for ( size_t i = 0; i < sizeof(arrCount) / sizeof(arrCount[0]); i++ ) {
		if ( arrCount[i] > iMaximum ) {
			break;
		}
		if ( !patternBenchRun(arrCount[i], iIterations, false) ) {
			return 2;
		}
	}
	for ( size_t i = 0; i < sizeof(arrCount) / sizeof(arrCount[0]); i++ ) {
		if ( arrCount[i] > iMaximum ) {
			break;
		}
		if ( !patternBenchRun(arrCount[i], iIterations, true) ) {
			return 3;
		}
	}
	return 0;
}
