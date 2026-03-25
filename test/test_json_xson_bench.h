#ifndef XRT_TEST_JSON_XSON_BENCH_H
#define XRT_TEST_JSON_XSON_BENCH_H

typedef xvalue (*xrt_json_xson_bench_parse_proc)(str sText, size_t iSize);
typedef str (*xrt_json_xson_bench_stringify_proc)(xvalue pVal, int bFormat, size_t* pRetSize);

typedef struct
{
	const char* sName;
	double fSeconds;
	uint64 iRounds;
	uint64 iBytes;
	size_t iTextSize;
} xrt_json_xson_bench_result_t;

static volatile uint64 __g_iJSONXSONBenchSink = 0;

static xvalue __Test_JSONXSONBench_ParseJSON(str sText, size_t iSize)
{
	return xrtParseJSON(sText, iSize);
}


static xvalue __Test_JSONXSONBench_ParseXSON(str sText, size_t iSize)
{
	return xrtParseXSON(sText, iSize);
}


static str __Test_JSONXSONBench_StringifyJSON(xvalue pVal, int bFormat, size_t* pRetSize)
{
	return xrtStringifyJSON(pVal, bFormat, pRetSize);
}


static str __Test_JSONXSONBench_StringifyXSON(xvalue pVal, int bFormat, size_t* pRetSize)
{
	return xrtStringifyXSON(pVal, bFormat, 0, pRetSize);
}


static uint64 __Test_JSONXSONBench_AccumulateRoot(xvalue pVal)
{
	uint64 iValue = 0;

	if ( pVal == NULL ) {
		return 0;
	}

	iValue += (uint64)xvoType(pVal);
	if ( xvoType(pVal) == XVO_DT_TABLE ) {
		iValue += (uint64)xvoTableItemCount(pVal);
	} else if ( xvoType(pVal) == XVO_DT_ARRAY ) {
		iValue += (uint64)xvoArrayItemCount(pVal);
	} else if ( xvoType(pVal) == XVO_DT_LIST ) {
		iValue += (uint64)xvoListItemCount(pVal);
	} else if ( xvoType(pVal) == XVO_DT_COLL ) {
		iValue += (uint64)xvoCollItemCount(pVal);
	} else {
		iValue += (uint64)xvoGetSize(pVal);
	}

	return iValue;
}


static void __Test_JSONXSONBench_PrintResult(const xrt_json_xson_bench_result_t* pResult)
{
	double fMs;
	double fUsPerOp;
	double fMiBPerSec;

	if ( pResult == NULL ) {
		return;
	}

	fMs = pResult->fSeconds * 1000.0;
	fUsPerOp = (pResult->iRounds > 0) ? ((pResult->fSeconds * 1000000.0) / (double)pResult->iRounds) : 0.0;
	fMiBPerSec = (pResult->fSeconds > 0.0) ? (((double)pResult->iBytes / (1024.0 * 1024.0)) / pResult->fSeconds) : 0.0;

	printf("  %-24s total=%9.3f ms  avg=%9.3f us/op  throughput=%9.2f MiB/s\n",
		pResult->sName,
		fMs,
		fUsPerOp,
		fMiBPerSec);
}


static void __Test_JSONXSONBench_PrintDelta(const xrt_json_xson_bench_result_t* pJSON, const xrt_json_xson_bench_result_t* pXSON)
{
	double fDelta;
	double fAbsDelta;
	double fRatio;

	if ( pJSON == NULL || pXSON == NULL || pJSON->fSeconds <= 0.0 ) {
		return;
	}

	fRatio = pXSON->fSeconds / pJSON->fSeconds;
	fDelta = (fRatio - 1.0) * 100.0;
	fAbsDelta = (fDelta < 0.0) ? -fDelta : fDelta;

	if ( fDelta >= 0.0 ) {
		printf("  差距: XSON 比 JSON 慢 %.2f%% (%.3fx)\n", fAbsDelta, fRatio);
	} else {
		printf("  差距: XSON 比 JSON 快 %.2f%% (%.3fx)\n", fAbsDelta, pJSON->fSeconds / pXSON->fSeconds);
	}
}


static int __Test_JSONXSONBench_RunParse(
	const char* sName,
	xrt_json_xson_bench_parse_proc procParse,
	str sText,
	size_t iSize,
	uint32 iRounds,
	uint32 iWarmup,
	xrt_json_xson_bench_result_t* pResult)
{
	double fStart;
	double fEnd;
	uint64 iBytes = 0;

	if ( sName == NULL || procParse == NULL || sText == NULL || iSize == 0 || iRounds == 0 || pResult == NULL ) {
		return 1;
	}

	for ( uint32 i = 0; i < iWarmup; i++ ) {
		xvalue pVal = procParse(sText, iSize);
		if ( pVal == NULL || xvoIsNull(pVal) ) {
			return 2;
		}
		__g_iJSONXSONBenchSink += __Test_JSONXSONBench_AccumulateRoot(pVal);
		xvoUnref(pVal);
	}

	fStart = xrtTimer();
	for ( uint32 i = 0; i < iRounds; i++ ) {
		xvalue pVal = procParse(sText, iSize);
		if ( pVal == NULL || xvoIsNull(pVal) ) {
			return 3;
		}
		__g_iJSONXSONBenchSink += __Test_JSONXSONBench_AccumulateRoot(pVal);
		xvoUnref(pVal);
		iBytes += (uint64)iSize;
	}
	fEnd = xrtTimer();

	pResult->sName = sName;
	pResult->fSeconds = fEnd - fStart;
	pResult->iRounds = iRounds;
	pResult->iBytes = iBytes;
	pResult->iTextSize = iSize;
	return 0;
}


static int __Test_JSONXSONBench_RunStringify(
	const char* sName,
	xrt_json_xson_bench_stringify_proc procStringify,
	xvalue pVal,
	int bFormat,
	uint32 iRounds,
	uint32 iWarmup,
	xrt_json_xson_bench_result_t* pResult)
{
	double fStart;
	double fEnd;
	uint64 iBytes = 0;

	if ( sName == NULL || procStringify == NULL || pVal == NULL || iRounds == 0 || pResult == NULL ) {
		return 1;
	}

	for ( uint32 i = 0; i < iWarmup; i++ ) {
		size_t iTextSize = 0;
		str sText = procStringify(pVal, bFormat, &iTextSize);
		if ( sText == NULL ) {
			return 2;
		}
		__g_iJSONXSONBenchSink += (uint64)iTextSize;
		xrtFree(sText);
	}

	fStart = xrtTimer();
	for ( uint32 i = 0; i < iRounds; i++ ) {
		size_t iTextSize = 0;
		str sText = procStringify(pVal, bFormat, &iTextSize);
		if ( sText == NULL ) {
			return 3;
		}
		__g_iJSONXSONBenchSink += (uint64)iTextSize;
		iBytes += (uint64)iTextSize;
		pResult->iTextSize = iTextSize;
		xrtFree(sText);
	}
	fEnd = xrtTimer();

	pResult->sName = sName;
	pResult->fSeconds = fEnd - fStart;
	pResult->iRounds = iRounds;
	pResult->iBytes = iBytes;
	return 0;
}


static xvalue __Test_JSONXSONBench_CreateJSONPayload(uint32 iItemCount)
{
	xvalue pRoot = NULL;
	xvalue pItems = NULL;
	xvalue pSummary = NULL;

	pRoot = xvoCreateTable();
	pItems = xvoCreateArray();
	pSummary = xvoCreateTable();
	if ( pRoot == NULL || pItems == NULL || pSummary == NULL ) {
		if ( pSummary ) {
			xvoUnref(pSummary);
		}
		if ( pItems ) {
			xvoUnref(pItems);
		}
		if ( pRoot ) {
			xvoUnref(pRoot);
		}
		return NULL;
	}

	xvoTableSetText(pRoot, "title", 0, "JSON XSON Benchmark", 0, FALSE);
	xvoTableSetInt(pRoot, "version", 0, 1);
	xvoTableSetBool(pRoot, "compact", 0, TRUE);

	xvoTableSetInt(pSummary, "count", 0, (int64)iItemCount);
	xvoTableSetBool(pSummary, "enabled", 0, TRUE);
	xvoTableSetFloat(pSummary, "ratio", 0, 0.875);
	xvoTableSetValue(pRoot, "summary", 0, pSummary, FALSE);
	xvoUnref(pSummary);

	for ( uint32 i = 0; i < iItemCount; i++ ) {
		xvalue pItem = xvoCreateTable();
		xvalue pTags = xvoCreateArray();
		xvalue pMetrics = xvoCreateTable();
		str sName;
		str sNote;

		if ( pItem == NULL || pTags == NULL || pMetrics == NULL ) {
			if ( pMetrics ) {
				xvoUnref(pMetrics);
			}
			if ( pTags ) {
				xvoUnref(pTags);
			}
			if ( pItem ) {
				xvoUnref(pItem);
			}
			xvoUnref(pItems);
			xvoUnref(pRoot);
			return NULL;
		}

		sName = xrtFormat("item-%04u", i);
		sNote = xrtFormat("line-%u\\nbench-value-%u", i, i * 3);

		xvoTableSetInt(pItem, "id", 0, (int64)i);
		xvoTableSetText(pItem, "name", 0, sName, 0, FALSE);
		xvoTableSetBool(pItem, "active", 0, (i & 1u) == 0u);
		xvoTableSetFloat(pItem, "score", 0, ((double)(i % 97u) * 1.25) + 0.5);
		xvoTableSetText(pItem, "note", 0, sNote, 0, FALSE);

		xrtFree(sName);
		xrtFree(sNote);

		xvoArrayAppendText(pTags, "alpha", 0, FALSE);
		xvoArrayAppendText(pTags, "beta", 0, FALSE);
		xvoArrayAppendText(pTags, (i & 1u) ? "odd" : "even", 0, FALSE);

		xvoTableSetInt(pMetrics, "min", 0, (int64)(i % 7u));
		xvoTableSetInt(pMetrics, "max", 0, (int64)(i + 100u));
		xvoTableSetFloat(pMetrics, "avg", 0, ((double)i * 0.25) + 1.0);

		xvoTableSetValue(pItem, "tags", 0, pTags, FALSE);
		xvoUnref(pTags);
		xvoTableSetValue(pItem, "metrics", 0, pMetrics, FALSE);
		xvoUnref(pMetrics);

		xvoArrayAppendValue(pItems, pItem, FALSE);
		xvoUnref(pItem);
	}

	xvoTableSetValue(pRoot, "items", 0, pItems, FALSE);
	xvoUnref(pItems);
	return pRoot;
}


static xvalue __Test_JSONXSONBench_CreateXSONPayload(uint32 iItemCount)
{
	xvalue pRoot = NULL;
	xvalue pFlags = NULL;
	xvalue pRecords = NULL;
	xvalue pBlob = NULL;

	pRoot = xvoCreateTable();
	pFlags = xvoCreateColl();
	pRecords = xvoCreateArray();
	pBlob = xvoCreateClass(96);
	if ( pRoot == NULL || pFlags == NULL || pRecords == NULL || pBlob == NULL ) {
		if ( pBlob ) {
			xvoUnref(pBlob);
		}
		if ( pRecords ) {
			xvoUnref(pRecords);
		}
		if ( pFlags ) {
			xvoUnref(pFlags);
		}
		if ( pRoot ) {
			xvoUnref(pRoot);
		}
		return NULL;
	}

	xvoTableSetText(pRoot, "title", 0, "XSON Extended Benchmark", 0, FALSE);
	xvoTableSetTimeSerial(pRoot, "created", 0, 2026, 3, 25, 12, 30, 45);

	for ( uint32 i = 0; i < 96; i++ ) {
		((uint8*)pBlob->vStruct)[i] = (uint8)((i * 17u) + (iItemCount % 251u));
	}
	xvoTableSetValue(pRoot, "blob", 0, pBlob, FALSE);
	xvoUnref(pBlob);

	xvoCollSetText(pFlags, "xson", 0, FALSE);
	xvoCollSetText(pFlags, "bench", 0, FALSE);
	xvoCollSetText(pFlags, "extended", 0, FALSE);
	xvoTableSetValue(pRoot, "flags", 0, pFlags, FALSE);
	xvoUnref(pFlags);

	for ( uint32 i = 0; i < iItemCount; i++ ) {
		xvalue pRecord = xvoCreateTable();
		xvalue pSlots = xvoCreateList();
		xvalue pMarks = xvoCreateColl();
		str sLabel;

		if ( pRecord == NULL || pSlots == NULL || pMarks == NULL ) {
			if ( pMarks ) {
				xvoUnref(pMarks);
			}
			if ( pSlots ) {
				xvoUnref(pSlots);
			}
			if ( pRecord ) {
				xvoUnref(pRecord);
			}
			xvoUnref(pRecords);
			xvoUnref(pRoot);
			return NULL;
		}

		sLabel = xrtFormat("node-%04u", i);

		xvoTableSetInt(pRecord, "id", 0, (int64)i);
		xvoTableSetText(pRecord, "label", 0, sLabel, 0, FALSE);
		xvoTableSetTimeSerial(pRecord, "stamp", 0, 2026, 3, 25, 8 + (i % 10u), (i * 3u) % 60u, (i * 7u) % 60u);
		xrtFree(sLabel);

		xvoListSetInt(pSlots, 1, (int64)(i * 10u));
		xvoListSetText(pSlots, 3, (i & 1u) ? "odd" : "even", 0, FALSE);
		xvoListSetTimeSerial(pSlots, 5, 2026, 3, 25, 6 + (i % 12u), (i * 5u) % 60u, (i * 11u) % 60u);

		xvoCollSetText(pMarks, "bench", 0, FALSE);
		xvoCollSetText(pMarks, (i & 1u) ? "hot" : "cold", 0, FALSE);
		xvoCollSetInt(pMarks, (int64)(i % 9u));

		xvoTableSetValue(pRecord, "slots", 0, pSlots, FALSE);
		xvoUnref(pSlots);
		xvoTableSetValue(pRecord, "marks", 0, pMarks, FALSE);
		xvoUnref(pMarks);

		xvoArrayAppendValue(pRecords, pRecord, FALSE);
		xvoUnref(pRecord);
	}

	xvoTableSetValue(pRoot, "records", 0, pRecords, FALSE);
	xvoUnref(pRecords);
	return pRoot;
}


static int Test_JSON_XSON_Bench(void)
{
	uint32 iRounds = __xrtTestParseUint32ExtraArg(0, 100);
	uint32 iItemCount = __xrtTestParseUint32ExtraArg(1, 128);
	uint32 iWarmup = (iRounds >= 20) ? (iRounds / 10) : 5;
	xvalue pJSONValue = NULL;
	xvalue pXSONValue = NULL;
	str sJSONText = NULL;
	str sXSONText = NULL;
	size_t iJSONSize = 0;
	size_t iXSONSize = 0;
	xrt_json_xson_bench_result_t tJSONParse;
	xrt_json_xson_bench_result_t tXSONParse;
	xrt_json_xson_bench_result_t tJSONStringify;
	xrt_json_xson_bench_result_t tXSONStringify;
	xrt_json_xson_bench_result_t tJSONStringifyFormat;
	xrt_json_xson_bench_result_t tXSONStringifyFormat;
	xrt_json_xson_bench_result_t tXSONParseExtended;
	xrt_json_xson_bench_result_t tXSONStringifyExtended;
	xrt_json_xson_bench_result_t tXSONStringifyExtendedFormat;
	int iRet = 0;

	memset(&tJSONParse, 0, sizeof(tJSONParse));
	memset(&tXSONParse, 0, sizeof(tXSONParse));
	memset(&tJSONStringify, 0, sizeof(tJSONStringify));
	memset(&tXSONStringify, 0, sizeof(tXSONStringify));
	memset(&tJSONStringifyFormat, 0, sizeof(tJSONStringifyFormat));
	memset(&tXSONStringifyFormat, 0, sizeof(tXSONStringifyFormat));
	memset(&tXSONParseExtended, 0, sizeof(tXSONParseExtended));
	memset(&tXSONStringifyExtended, 0, sizeof(tXSONStringifyExtended));
	memset(&tXSONStringifyExtendedFormat, 0, sizeof(tXSONStringifyExtendedFormat));

	printf("[json_xson_bench] start\n");
	printf("[json_xson_bench] rounds=%u warmup=%u items=%u\n", iRounds, iWarmup, iItemCount);
	printf("[json_xson_bench] usage: test.exe json_xson_bench [rounds] [items]\n");

	pJSONValue = __Test_JSONXSONBench_CreateJSONPayload(iItemCount);
	pXSONValue = __Test_JSONXSONBench_CreateXSONPayload(iItemCount);
	if ( pJSONValue == NULL || pXSONValue == NULL ) {
		iRet = 1;
		goto cleanup;
	}

	sJSONText = xrtStringifyJSON(pJSONValue, FALSE, &iJSONSize);
	sXSONText = xrtStringifyXSON(pXSONValue, FALSE, 0, &iXSONSize);
	if ( sJSONText == NULL || sXSONText == NULL ) {
		iRet = 2;
		goto cleanup;
	}

	printf("[json_xson_bench] json_text=%llu bytes  xson_text=%llu bytes\n",
		(unsigned long long)iJSONSize,
		(unsigned long long)iXSONSize);

	printf("\n=== JSON-Compatible Payload: Parse ===\n");
	printf("=== 纯 JSON 负载：解析 ===\n");
	iRet = __Test_JSONXSONBench_RunParse("JSON parse", __Test_JSONXSONBench_ParseJSON, sJSONText, iJSONSize, iRounds, iWarmup, &tJSONParse);
	if ( iRet != 0 ) { goto cleanup; }
	iRet = __Test_JSONXSONBench_RunParse("XSON parse", __Test_JSONXSONBench_ParseXSON, sJSONText, iJSONSize, iRounds, iWarmup, &tXSONParse);
	if ( iRet != 0 ) { goto cleanup; }
	__Test_JSONXSONBench_PrintResult(&tJSONParse);
	__Test_JSONXSONBench_PrintResult(&tXSONParse);
	__Test_JSONXSONBench_PrintDelta(&tJSONParse, &tXSONParse);

	printf("\n=== JSON-Compatible Payload: Stringify Compact ===\n");
	printf("=== 纯 JSON 负载：紧凑序列化 ===\n");
	iRet = __Test_JSONXSONBench_RunStringify("JSON stringify", __Test_JSONXSONBench_StringifyJSON, pJSONValue, FALSE, iRounds, iWarmup, &tJSONStringify);
	if ( iRet != 0 ) { goto cleanup; }
	iRet = __Test_JSONXSONBench_RunStringify("XSON stringify", __Test_JSONXSONBench_StringifyXSON, pJSONValue, FALSE, iRounds, iWarmup, &tXSONStringify);
	if ( iRet != 0 ) { goto cleanup; }
	__Test_JSONXSONBench_PrintResult(&tJSONStringify);
	__Test_JSONXSONBench_PrintResult(&tXSONStringify);
	__Test_JSONXSONBench_PrintDelta(&tJSONStringify, &tXSONStringify);

	printf("\n=== JSON-Compatible Payload: Stringify Formatted ===\n");
	printf("=== 纯 JSON 负载：格式化序列化 ===\n");
	iRet = __Test_JSONXSONBench_RunStringify("JSON stringify fmt", __Test_JSONXSONBench_StringifyJSON, pJSONValue, TRUE, iRounds, iWarmup, &tJSONStringifyFormat);
	if ( iRet != 0 ) { goto cleanup; }
	iRet = __Test_JSONXSONBench_RunStringify("XSON stringify fmt", __Test_JSONXSONBench_StringifyXSON, pJSONValue, TRUE, iRounds, iWarmup, &tXSONStringifyFormat);
	if ( iRet != 0 ) { goto cleanup; }
	__Test_JSONXSONBench_PrintResult(&tJSONStringifyFormat);
	__Test_JSONXSONBench_PrintResult(&tXSONStringifyFormat);
	__Test_JSONXSONBench_PrintDelta(&tJSONStringifyFormat, &tXSONStringifyFormat);

	printf("\n=== XSON Extended Payload ===\n");
	printf("=== XSON 扩展负载 ===\n");
	iRet = __Test_JSONXSONBench_RunParse("XSON parse ext", __Test_JSONXSONBench_ParseXSON, sXSONText, iXSONSize, iRounds, iWarmup, &tXSONParseExtended);
	if ( iRet != 0 ) { goto cleanup; }
	iRet = __Test_JSONXSONBench_RunStringify("XSON stringify ext", __Test_JSONXSONBench_StringifyXSON, pXSONValue, FALSE, iRounds, iWarmup, &tXSONStringifyExtended);
	if ( iRet != 0 ) { goto cleanup; }
	iRet = __Test_JSONXSONBench_RunStringify("XSON stringify ext fmt", __Test_JSONXSONBench_StringifyXSON, pXSONValue, TRUE, iRounds, iWarmup, &tXSONStringifyExtendedFormat);
	if ( iRet != 0 ) { goto cleanup; }
	__Test_JSONXSONBench_PrintResult(&tXSONParseExtended);
	__Test_JSONXSONBench_PrintResult(&tXSONStringifyExtended);
	__Test_JSONXSONBench_PrintResult(&tXSONStringifyExtendedFormat);

	printf("\n[json_xson_bench] sink=%llu\n", (unsigned long long)__g_iJSONXSONBenchSink);
	printf("[json_xson_bench] pass\n");
	iRet = 0;

cleanup:
	if ( sXSONText ) {
		xrtFree(sXSONText);
	}
	if ( sJSONText ) {
		xrtFree(sJSONText);
	}
	if ( pXSONValue ) {
		xvoUnref(pXSONValue);
	}
	if ( pJSONValue ) {
		xvoUnref(pJSONValue);
	}

	if ( iRet != 0 ) {
		fprintf(stderr, "[json_xson_bench] FAIL: %d\n", iRet);
	}
	return iRet;
}

#endif
