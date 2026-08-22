#ifndef TEST_MEMTELEMETRY_BASELINE_H
#define TEST_MEMTELEMETRY_BASELINE_H

#include "../xrt.h"

int Test_Runtime_Phase2(xrtGlobalData* xCore);
void Test_Coroutine(xrtGlobalData* xCore);
void Test_XNet2_Base(void);
void Test_XNet2_Port(void);
void Test_XNet2_Engine(void);
int Test_XNet2_Stream(void);
void Test_XNet2_Dgram(void);
#ifndef XRT_NO_NETTLS
	int Test_XNet2_TLS(void);
#endif
int Test_XNet2_Sync(void);
void Test_XNet2_Codec(void);
int Test_XNet_Http(void);
int Test_XNet_Httpd(void);
int Test_XNet_Ws(void);
void Test_XNet2_Mem(void);

typedef void (*__test_memtelemetry_baseline_lane_proc)(xrtGlobalData* pCore);


// 内部函数：__Test_MemTelemetryBaselinePrintTopClasses
static void __Test_MemTelemetryBaselinePrintTopClasses(const xrtMemTelemetrySnapshot* pSnap, int iTopN)
{
	int iRank;
	int iPicked[XRT_MEMPOOL_CLASS_COUNT_DEFAULT];

	memset(iPicked, 0, sizeof(iPicked));

	for ( iRank = 0; iRank < iTopN; iRank++ ) {
		int iClass;
		int iBest = -1;
		uint64 iBestCalls = 0;

		for ( iClass = 0; iClass < (int)pSnap->iClassCount; iClass++ ) {
			if ( iPicked[iClass] ) continue;
			if ( pSnap->arrClassCalls[iClass] > iBestCalls ) {
				iBestCalls = pSnap->arrClassCalls[iClass];
				iBest = iClass;
			}
		}

		if ( (iBest < 0) || (iBestCalls == 0) ) {
			break;
		}

		iPicked[iBest] = 1;
		printf("  class[%d] <= %uB : calls=%llu bytes=%llu\n",
			iBest,
			(unsigned)((iBest + 1) * pSnap->iClassStep),
			(unsigned long long)pSnap->arrClassCalls[iBest],
			(unsigned long long)pSnap->arrClassBytes[iBest]);
	}
}


// 内部函数：__Test_MemTelemetryBaselinePrintSnapshot
static void __Test_MemTelemetryBaselinePrintSnapshot(const char* sLane, const xrtMemTelemetrySnapshot* pSnap)
{
	printf("\n[memtelemetry-baseline] lane=%s\n", sLane ? sLane : "(null)");
	printf("  malloc : calls=%llu bytes=%llu\n",
		(unsigned long long)pSnap->iMallocCalls,
		(unsigned long long)pSnap->iMallocBytes);
	printf("  calloc : calls=%llu bytes=%llu\n",
		(unsigned long long)pSnap->iCallocCalls,
		(unsigned long long)pSnap->iCallocBytes);
	printf("  realloc: calls=%llu bytes=%llu\n",
		(unsigned long long)pSnap->iReallocCalls,
		(unsigned long long)pSnap->iReallocBytes);
	printf("  free   : calls=%llu\n",
		(unsigned long long)pSnap->iFreeCalls);
	printf("  temp   : calls=%llu bytes=%llu\n",
		(unsigned long long)pSnap->iTempCalls,
		(unsigned long long)pSnap->iTempBytes);
	printf("  pooled-candidate : calls=%llu bytes=%llu\n",
		(unsigned long long)pSnap->iPooledCandidateCalls,
		(unsigned long long)pSnap->iPooledCandidateBytes);
	printf("  fallback         : calls=%llu bytes=%llu\n",
		(unsigned long long)pSnap->iFallbackCalls,
		(unsigned long long)pSnap->iFallbackBytes);
	printf("  top classes:\n");
	__Test_MemTelemetryBaselinePrintTopClasses(pSnap, 8);
}


// 内部函数：__Test_MemTelemetryBaselineRunLane
static void __Test_MemTelemetryBaselineRunLane(const char* sLane, __test_memtelemetry_baseline_lane_proc Proc, xrtGlobalData* pCore)
{
	xrtMemTelemetrySnapshot tSnap;

	memset(&tSnap, 0, sizeof(tSnap));
	xrtMemTelemetryReset();
	Proc(pCore);
	xrtMemTelemetryGetSnapshot(&tSnap);
	__Test_MemTelemetryBaselinePrintSnapshot(sLane, &tSnap);
}


// 内部函数：__Test_MemTelemetryBaselineLane_Runtime
static void __Test_MemTelemetryBaselineLane_Runtime(xrtGlobalData* pCore)
{
	(void)Test_Runtime_Phase2(pCore);
}


// 内部函数：__Test_MemTelemetryBaselineLane_Coroutine
static void __Test_MemTelemetryBaselineLane_Coroutine(xrtGlobalData* pCore)
{
	Test_Coroutine(pCore);
}


// 内部函数：__Test_MemTelemetryBaselineLane_XNet
static void __Test_MemTelemetryBaselineLane_XNet(xrtGlobalData* pCore)
{
	(void)pCore;
	Test_XNet2_Base();
	Test_XNet2_Port();
	Test_XNet2_Engine();
	Test_XNet2_Stream();
	Test_XNet2_Dgram();
	#ifndef XRT_NO_NETTLS
		Test_XNet2_TLS();
	#endif
	(void)Test_XNet2_Sync();
	Test_XNet2_Codec();
	Test_XNet_Http();
	(void)Test_XNet_Httpd();
	(void)Test_XNet_Ws();
	Test_XNet2_Mem();
}


// MEMTELEMETRY基线测试
static int Test_MemTelemetryBaseline(xrtGlobalData* pCore)
{
	if ( !pCore ) return 1;

	xrtMemTelemetryEnable(TRUE);
	__Test_MemTelemetryBaselineRunLane("runtime_phase2", __Test_MemTelemetryBaselineLane_Runtime, pCore);
	__Test_MemTelemetryBaselineRunLane("coroutine", __Test_MemTelemetryBaselineLane_Coroutine, pCore);
	__Test_MemTelemetryBaselineRunLane("xnet_modern", __Test_MemTelemetryBaselineLane_XNet, pCore);
	xrtMemTelemetryEnable(FALSE);

	return 0;
}

#endif
