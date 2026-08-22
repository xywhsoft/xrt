#include "../xrt.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
	uint64 Count;
} JsonLifecycleCount;


static uint64 JsonLifecycleCountValue(xvalue pValue);


static bool JsonLifecycleCountTable(Dict_Key* pKey, ptr pSlot, ptr pArg)
{
	JsonLifecycleCount* pCount = (JsonLifecycleCount*)pArg;
	xvalue pValue = pSlot ? *(xvalue*)pSlot : NULL;
	(void)pKey;
	pCount->Count += JsonLifecycleCountValue(pValue);
	return FALSE;
}


static uint64 JsonLifecycleCountValue(xvalue pValue)
{
	JsonLifecycleCount tCount = { 1 };
	uint32 i;

	if ( pValue == NULL ) {
		return 0;
	}
	if ( xvoType(pValue) == XVO_DT_ARRAY ) {
		for ( i = 0; i < xvoArrayItemCount(pValue); i++ ) {
			tCount.Count += JsonLifecycleCountValue(xvoArrayGetValue(pValue, i));
		}
	} else if ( xvoType(pValue) == XVO_DT_TABLE ) {
		xvoTableWalk(pValue, JsonLifecycleCountTable, &tCount);
	}
	return tCount.Count;
}


int main(int argc, char** argv)
{
	str sSource;
	size_t iSize = 0;
	int iRepeats;
	int i;
	uint64 iExpected = 0;
	int iResult = 0;

	if ( argc < 2 ) {
		fprintf(stderr, "usage: test_json_lifecycle <file.json> [repeats]\n");
		return 2;
	}
	if ( xrtInit() == NULL ) {
		fprintf(stderr, "xrtInit failed\n");
		return 1;
	}
	sSource = xrtFileGetAll((str)argv[1], &iSize);
	iRepeats = argc > 2 ? (int)strtol(argv[2], NULL, 10) : 20;
	if ( sSource == NULL || iSize == 0 || iRepeats <= 0 ) {
		fprintf(stderr, "invalid input\n");
		iResult = 1;
		goto cleanup;
	}

	for ( i = 0; i < iRepeats; i++ ) {
		double fStart = xrtTimer();
		double fParsed;
		double fWalked;
		double fFreed;
		xvalue pRoot = xrtParseJSON(sSource, iSize);
		uint64 iCount;

		fParsed = xrtTimer();
		if ( pRoot == NULL || xvoIsNull(pRoot) ) {
			fprintf(stderr, "parse failed at round %d\n", i + 1);
			iResult = 1;
			break;
		}
		iCount = JsonLifecycleCountValue(pRoot);
		fWalked = xrtTimer();
		xvoUnref(pRoot);
		fFreed = xrtTimer();
		if ( i == 0 ) {
			iExpected = iCount;
		} else if ( iCount != iExpected ) {
			fprintf(stderr, "node count changed at round %d\n", i + 1);
			iResult = 1;
			break;
		}
		printf("%d parse=%.3fms walk=%.3fms free=%.3fms nodes=%llu\n",
			i + 1,
			(fParsed - fStart) * 1000.0,
			(fWalked - fParsed) * 1000.0,
			(fFreed - fWalked) * 1000.0,
			(unsigned long long)iCount);
	}

cleanup:
	xrtFree(sSource);
	xrtUnit();
	return iResult;
}
