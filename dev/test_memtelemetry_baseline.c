#include "test_xnet_impl_env.h"
#include "../test/test_runtime_phase2.h"
#include "../test/test_coroutine.h"
#include "../test/test_xnet2_base.h"
#include "../test/test_xnet2_port.h"
#include "../test/test_xnet2_engine.h"
#include "../test/test_xnet2_stream.h"
#include "../test/test_xnet2_dgram.h"
#include "../test/test_xnet2_tls.h"
#include "../test/test_xnet2_sync.h"
#include "../test/test_xnet2_codec.h"
#include "../test/test_xnet_http.h"
#include "../test/test_xnet_httpd.h"
#include "../test/test_xnet_ws.h"
#include "../test/test_xnet2_mem.h"

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#endif

typedef void (*__test_memtelemetry_lane_proc)(xrtGlobalData* pCore);

static void __Test_MemTelemetryBaselinePrintTopClasses(const xrtMemTelemetrySnapshot* pSnap, int iTopN)
{
	int iRank;
	int iPicked[XRT_MEMPOOL_CLASS_COUNT_DEFAULT];
	memset(iPicked, 0, sizeof(iPicked));

	for ( iRank = 0; iRank < iTopN; iRank++ ) {
		int i;
		int iBest = -1;
		uint64 iBestCalls = 0;
		for ( i = 0; i < (int)pSnap->iClassCount; i++ ) {
			if ( iPicked[i] ) continue;
			if ( pSnap->arrClassCalls[i] > iBestCalls ) {
				iBestCalls = pSnap->arrClassCalls[i];
				iBest = i;
			}
		}
		if ( iBest < 0 || iBestCalls == 0 ) {
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

static void __Test_MemTelemetryBaselineRunLane(const char* sLane, __test_memtelemetry_lane_proc Proc, xrtGlobalData* pCore)
{
	xrtMemTelemetrySnapshot tSnap;
	memset(&tSnap, 0, sizeof(tSnap));
	xrtMemTelemetryReset();
	Proc(pCore);
	xrtMemTelemetryGetSnapshot(&tSnap);
	__Test_MemTelemetryBaselinePrintSnapshot(sLane, &tSnap);
}

static void __Test_MemTelemetryLane_Runtime(xrtGlobalData* pCore)
{
	Test_Runtime_Phase2(pCore);
}

static void __Test_MemTelemetryLane_Coroutine(xrtGlobalData* pCore)
{
	Test_Coroutine(pCore);
}

static void __Test_MemTelemetryLane_XNet(xrtGlobalData* pCore)
{
	(void)pCore;
	Test_XNet2_Base();
	Test_XNet2_Port();
	Test_XNet2_Engine();
	Test_XNet2_Stream();
	Test_XNet2_Dgram();
	Test_XNet2_TLS();
	Test_XNet2_Sync();
	Test_XNet2_Codec();
	Test_XNet_Http();
	Test_XNet_Httpd();
	Test_XNet_Ws();
	Test_XNet2_Mem();
}

int main(void)
{
	xrtGlobalData* pCore;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

#if defined(_WIN32) || defined(_WIN64)
	{
		WSADATA tWSA;
		if ( WSAStartup(MAKEWORD(2, 2), &tWSA) != 0 ) {
			return 1;
		}
	}
#endif

	pCore = xrtInit();
	if ( !pCore ) {
		return 2;
	}

	xrtMemTelemetryEnable(TRUE);

	__Test_MemTelemetryBaselineRunLane("runtime_phase2", __Test_MemTelemetryLane_Runtime, pCore);
	__Test_MemTelemetryBaselineRunLane("coroutine", __Test_MemTelemetryLane_Coroutine, pCore);
	__Test_MemTelemetryBaselineRunLane("xnet_modern", __Test_MemTelemetryLane_XNet, pCore);

	xrtMemTelemetryEnable(FALSE);
	xrtUnit();

#if defined(_WIN32) || defined(_WIN64)
	WSACleanup();
#endif

	return 0;
}
