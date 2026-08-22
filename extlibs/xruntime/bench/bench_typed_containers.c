#include "../../../dev/bench/bench_common.h"

#define XRUNTIME_MODULE_TYPED_ARRAY
#define XRUNTIME_MODULE_TYPED_DICT
#include <xruntime.h>



/* 测量连续类型数组和文本键类型字典的稳定热路径。 */
int main(int argc, char** argv)
{
	uint32 iArrayCount = xbenchArgU32(argc, argv, 1, 1000000u);
	uint32 iDictOps = xbenchArgU32(argc, argv, 2, 1000000u);
	uint32 iKeyCount = xbenchArgU32(argc, argv, 3, 4096u);
	const xrttype* pType = xrtTypeUInt64();
	xtypedarray Array = { 0 };
	xtypeddict Dict = { 0 };
	xstrview* pKeys;
	char* pKeyStorage;
	xbenchtimer Timer;
	uint64 iArrayElapsed;
	uint64 iDictElapsed;
	uint64 iChecksum = 0u;

	if ( (iArrayCount == 0u) || (iDictOps == 0u) || (iKeyCount == 0u) ) {
		fprintf(stderr, "benchmark counts must be non-zero.\n");
		return 1;
	}
	pKeys = (xstrview*)xrtMalloc((size_t)iKeyCount * sizeof(*pKeys));
	pKeyStorage = (char*)xrtMalloc((size_t)iKeyCount * 32u);
	if ( (pKeys == NULL) || (pKeyStorage == NULL) ) {
		xrtFree(pKeyStorage);
		xrtFree(pKeys);
		return 2;
	}
	for ( uint32 i = 0u; i < iKeyCount; i++ ) {
		char* pKey = pKeyStorage + ((size_t)i * 32u);
		int iSize = snprintf(pKey, 32u, "key-%08" PRIu32, i);

		if ( (iSize <= 0) || (iSize >= 32) ) {
			xrtFree(pKeyStorage);
			xrtFree(pKeys);
			return 3;
		}
		pKeys[i].Data = pKey;
		pKeys[i].Size = (size_t)iSize;
	}
	if (
		!xrtTypedArrayInit(&Array, pType) ||
		!xrtTypedArrayReserve(&Array, iArrayCount) ||
		!xrtTypedDictInit(&Dict, pType) ||
		!xrtTypedDictReserve(&Dict, iKeyCount)
	) {
		xrtTypedArrayUnit(&Array);
		xrtTypedDictUnit(&Dict);
		xrtFree(pKeyStorage);
		xrtFree(pKeys);
		return 4;
	}

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iArrayCount; i++ ) {
		uint64 iValue = i;

		if ( !xrtTypedArrayPush(&Array, &iValue) ) {
			return 5;
		}
	}
	for ( uint32 i = 0u; i < iArrayCount; i++ ) {
		const uint64* pValue = (const uint64*)xrtTypedArrayConstGet(
			&Array, i
		);

		if ( pValue == NULL ) {
			return 6;
		}
		iChecksum ^= *pValue + i;
	}
	xbenchTimerStop(&Timer);
	iArrayElapsed = xbenchTimerElapsedNs(&Timer);

	for ( uint32 i = 0u; i < iKeyCount; i++ ) {
		uint64 iValue = i;

		if ( !xrtTypedDictSet(&Dict, pKeys[i], &iValue) ) {
			return 7;
		}
	}
	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iDictOps; i++ ) {
		uint32 iIndex = i % iKeyCount;
		uint64 iValue = (uint64)i + 1u;
		const uint64* pValue;

		if ( !xrtTypedDictSet(&Dict, pKeys[iIndex], &iValue) ) {
			return 8;
		}
		pValue = (const uint64*)xrtTypedDictConstGet(
			&Dict, pKeys[iIndex]
		);
		if ( pValue == NULL ) {
			return 9;
		}
		iChecksum ^= *pValue + iIndex;
	}
	xbenchTimerStop(&Timer);
	iDictElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt typed container benchmark\n");
	xbenchPrintMetricDouble(
		"typed_array_ops_per_sec",
		xbenchSafeRate((uint64)iArrayCount * 2u, iArrayElapsed)
	);
	xbenchPrintMetricDouble(
		"typed_dict_ops_per_sec",
		xbenchSafeRate((uint64)iDictOps * 2u, iDictElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);

	xrtTypedArrayUnit(&Array);
	xrtTypedDictUnit(&Dict);
	xrtFree(pKeyStorage);
	xrtFree(pKeys);
	return 0;
}
