/*
 * XRT Example - Memory Telemetry
 * XRT 范例 - 内存遥测
 *
 * Description / 说明:
 *   EN: Demonstrates enabling memory telemetry, performing a few allocations,
 *       and reading the aggregated snapshot counters.
 *   CN: 演示启用内存遥测、执行几次分配，并读取聚合后的快照计数。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/base_mem_telemetry.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/base_mem_telemetry -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>
#include <string.h>


static void procPrintFirstClass(const xrtMemTelemetrySnapshot* pSnap)
{
	uint32 i;

	if ( pSnap == NULL ) {
		return;
	}

	for ( i = 0u; i < pSnap->iClassCount; ++i ) {
		if ( pSnap->arrClassCalls[i] > 0u ) {
			printf("first_pool_class = %u\n", (unsigned)i);
			printf("first_pool_calls = %llu\n", (unsigned long long)pSnap->arrClassCalls[i]);
			printf("first_pool_bytes = %llu\n", (unsigned long long)pSnap->arrClassBytes[i]);
			return;
		}
	}

	printf("first_pool_class = none\n");
	printf("first_pool_calls = 0\n");
	printf("first_pool_bytes = 0\n");
}


int main(void)
{
	xrtMemTelemetrySnapshot tSnap;
	char* pBlock = NULL;
	int* pZero = NULL;

	xrtInit();
	memset(&tSnap, 0, sizeof(tSnap));

	xrtMemTelemetryReset();
	xrtMemTelemetryEnable(TRUE);

	pBlock = (char*)xrtMalloc(48u);
	pZero = (int*)xrtCalloc(4u, sizeof(int));
	pBlock = (char*)xrtRealloc(pBlock, 96u);
	(void)xrtTempMemory(64u);

	if ( pBlock != NULL ) {
		memset(pBlock, 'T', 95u);
		pBlock[95] = '\0';
	}

	xrtFree(pZero);
	xrtFree(pBlock);

	xrtMemTelemetryGetSnapshot(&tSnap);

	printf("telemetry_enabled = %s\n", tSnap.bEnabled ? "TRUE" : "FALSE");
	printf("malloc_calls = %llu\n", (unsigned long long)tSnap.iMallocCalls);
	printf("calloc_calls = %llu\n", (unsigned long long)tSnap.iCallocCalls);
	printf("realloc_calls = %llu\n", (unsigned long long)tSnap.iReallocCalls);
	printf("free_calls = %llu\n", (unsigned long long)tSnap.iFreeCalls);
	printf("temp_calls = %llu\n", (unsigned long long)tSnap.iTempCalls);
	printf("temp_bytes = %llu\n", (unsigned long long)tSnap.iTempBytes);
	printf("pooled_candidate_calls = %llu\n", (unsigned long long)tSnap.iPooledCandidateCalls);
	printf("fallback_calls = %llu\n", (unsigned long long)tSnap.iFallbackCalls);
	procPrintFirstClass(&tSnap);

	xrtMemTelemetryEnable(FALSE);
	xrtUnit();
	return 0;
}
