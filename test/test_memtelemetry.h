#ifndef XRT_TEST_MEMTELEMETRY_H
#define XRT_TEST_MEMTELEMETRY_H

static void __Test_MemTelemetryRequire(bool bCond, const char* sMsg)
{
	if ( !bCond ) {
		fprintf(stderr, "[memtelemetry] FAIL: %s\n", sMsg ? sMsg : "(no message)");
		exit(1);
	}
}

static void Test_MemTelemetry(void)
{
	xrtMemTelemetrySnapshot tSnap;
	ptr pA;
	ptr pB;
	ptr pC;
	ptr pD;
	uint32 iIdx16;
	uint32 iIdx32;
	uint32 iIdx48;

	printf("[memtelemetry] start\n");

	xrtMemTelemetryEnable(FALSE);
	xrtMemTelemetryReset();
	memset(&tSnap, 0, sizeof(tSnap));
	xrtMemTelemetryGetSnapshot(&tSnap);
	__Test_MemTelemetryRequire(tSnap.bEnabled == FALSE, "telemetry should start disabled");
	__Test_MemTelemetryRequire(tSnap.iMallocCalls == 0, "disabled snapshot should be empty");

	xrtMemTelemetryEnable(TRUE);
	xrtMemTelemetryReset();

	pA = xrtMalloc(8);
	pB = xrtCalloc(2, 12);
	pC = xrtMalloc(2048);
	pD = xrtRealloc(NULL, 40);
	__Test_MemTelemetryRequire(pA != NULL, "malloc(8) failed");
	__Test_MemTelemetryRequire(pB != NULL, "calloc(24) failed");
	__Test_MemTelemetryRequire(pC != NULL, "malloc(2048) failed");
	__Test_MemTelemetryRequire(pD != NULL, "realloc(NULL, 40) failed");
	__Test_MemTelemetryRequire(xrtTempMemory(20) != NULL, "temp memory failed");

	xrtFree(pA);
	xrtFree(pB);
	xrtFree(pC);
	xrtFree(pD);
	xrtFreeTempMemory();

	memset(&tSnap, 0, sizeof(tSnap));
	xrtMemTelemetryGetSnapshot(&tSnap);

	iIdx16 = 0;
	iIdx32 = 1;
	iIdx48 = 2;

	__Test_MemTelemetryRequire(tSnap.bEnabled == TRUE, "telemetry should be enabled");
	__Test_MemTelemetryRequire(tSnap.iMallocCalls == 2, "malloc call count mismatch");
	__Test_MemTelemetryRequire(tSnap.iMallocBytes == (8u + 2048u), "malloc bytes mismatch");
	__Test_MemTelemetryRequire(tSnap.iCallocCalls == 1, "calloc call count mismatch");
	__Test_MemTelemetryRequire(tSnap.iCallocBytes == 24, "calloc bytes mismatch");
	__Test_MemTelemetryRequire(tSnap.iReallocCalls == 1, "realloc call count mismatch");
	__Test_MemTelemetryRequire(tSnap.iReallocBytes == 40, "realloc bytes mismatch");
	__Test_MemTelemetryRequire(tSnap.iFreeCalls == 4, "free call count mismatch");
	__Test_MemTelemetryRequire(tSnap.iTempCalls == 1, "temp call count mismatch");
	__Test_MemTelemetryRequire(tSnap.iTempBytes == 20, "temp bytes mismatch");
	__Test_MemTelemetryRequire(tSnap.iPooledCandidateCalls == 3, "pooled candidate count mismatch");
	__Test_MemTelemetryRequire(tSnap.iPooledCandidateBytes == (8u + 24u + 40u), "pooled candidate bytes mismatch");
	__Test_MemTelemetryRequire(tSnap.iFallbackCalls == 1, "fallback count mismatch");
	__Test_MemTelemetryRequire(tSnap.iFallbackBytes == 2048, "fallback bytes mismatch");
	__Test_MemTelemetryRequire(tSnap.arrClassCalls[iIdx16] == 1, "16-byte class count mismatch");
	__Test_MemTelemetryRequire(tSnap.arrClassCalls[iIdx32] == 1, "32-byte class count mismatch");
	__Test_MemTelemetryRequire(tSnap.arrClassCalls[iIdx48] == 1, "48-byte class count mismatch");
	__Test_MemTelemetryRequire(tSnap.arrClassBytes[iIdx16] == 8, "16-byte class bytes mismatch");
	__Test_MemTelemetryRequire(tSnap.arrClassBytes[iIdx32] == 24, "32-byte class bytes mismatch");
	__Test_MemTelemetryRequire(tSnap.arrClassBytes[iIdx48] == 40, "48-byte class bytes mismatch");

	xrtMemTelemetryReset();
	memset(&tSnap, 0, sizeof(tSnap));
	xrtMemTelemetryGetSnapshot(&tSnap);
	__Test_MemTelemetryRequire(tSnap.iMallocCalls == 0, "reset should clear malloc calls");
	__Test_MemTelemetryRequire(tSnap.iFreeCalls == 0, "reset should clear free calls");
	__Test_MemTelemetryRequire(tSnap.iTempCalls == 0, "reset should clear temp calls");

	xrtMemTelemetryEnable(FALSE);
	printf("[memtelemetry] PASS\n");
}

#endif
