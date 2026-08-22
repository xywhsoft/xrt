#include "../../../dev/bench/bench_common.h"

#define XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_OBJECT
#include <xruntime.h>



typedef struct xbenchruntimeenv {
	xvalue* Value;
	uint64 Calls;
} xbenchruntimeenv;



/* 返回一个借用的预构建值，使计时集中在动态调用和结果提交本身。 */
static bool xbenchRuntimeCall(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	xbenchruntimeenv* pEnv = (xbenchruntimeenv*)pEnvironment;

	(void)pFrame;
	pEnv->Calls++;
	return xrtCallResultPush(pResult, pEnv->Value);
}



/* 测量类型操作、对象引用和动态调用三条运行时热路径。 */
int main(int argc, char** argv)
{
	uint32 iTypeCount = xbenchArgU32(argc, argv, 1, 10000000u);
	uint32 iReferenceCount = xbenchArgU32(argc, argv, 2, 10000000u);
	uint32 iCallCount = xbenchArgU32(argc, argv, 3, 2000000u);
	const xrttype* pIntType = xrtTypeInt64();
	const xrttype* ReturnTypes[1] = { pIntType };
	xrtfunctionsig Signature = {
		.Name = XRT_STR_INIT("benchmark"),
		.ReturnCount = 1u,
		.ReturnTypes = ReturnTypes
	};
	xrttype ObjectType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("xrt.benchmark.object")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE |
			XRT_TYPE_FLAG_FINAL,
		.Name = XRT_STR_INIT("benchmark_object"),
		.AbiName = XRT_STR_INIT("xrt.benchmark.object"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(uint64),
		.InstanceAlign = sizeof(uint64)
	};
	xbenchruntimeenv Env;
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	xrtcallable* pCallable;
	xrtobject* pObject;
	xbenchtimer Timer;
	uint64 iTypeElapsed;
	uint64 iReferenceElapsed;
	uint64 iCallElapsed;
	uint64 iChecksum = 0u;
	int64 iLeft = 42;
	int64 iRight = 84;
	int iCompare;

	if (
		(iTypeCount == 0u) ||
		(iReferenceCount == 0u) ||
		(iCallCount == 0u)
	) {
		fprintf(stderr, "benchmark counts must be non-zero.\n");
		return 1;
	}

	Env.Value = xrtValueInt(42);
	Env.Calls = 0u;
	pCallable = xrtCallableCreate(
		&Signature,
		xbenchRuntimeCall,
		&Env,
		NULL
	);
	pObject = xrtObjectCreate(&ObjectType);
	if ( (Env.Value == NULL) || (pCallable == NULL) || (pObject == NULL) ) {
		xrtObjectUnref(pObject);
		xrtCallableUnref(pCallable);
		xrtValueRelease(Env.Value);
		return 2;
	}

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iTypeCount; i++ ) {
		uint64 iHash;

		if (
			!xrtTypeHashValue(pIntType, &iLeft, &iHash) ||
			!xrtTypeCompareValue(pIntType, &iLeft, &iRight, &iCompare)
		) {
			return 3;
		}
		iChecksum ^= iHash + (uint64)(iCompare + 1) + (uint64)i;
	}
	xbenchTimerStop(&Timer);
	iTypeElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iReferenceCount; i++ ) {
		xrtobject* pReference = xrtObjectRef(pObject);

		if ( pReference == NULL ) {
			return 4;
		}
		xrtObjectUnref(pReference);
	}
	xbenchTimerStop(&Timer);
	iReferenceElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iCallCount; i++ ) {
		if ( !xrtCallableInvoke(pCallable, NULL, &Result) ) {
			return 5;
		}
	}
	xbenchTimerStop(&Timer);
	iCallElapsed = xbenchTimerElapsedNs(&Timer);

	if (
		(Env.Calls != (uint64)iCallCount) ||
		(xrtCallResultCount(&Result) != 1u)
	) {
		return 6;
	}

	printf("xrt runtime model benchmark\n");
	xbenchPrintMetricDouble(
		"runtime_type_ops_per_sec",
		xbenchSafeRate((uint64)iTypeCount * 2u, iTypeElapsed)
	);
	xbenchPrintMetricDouble(
		"runtime_object_ref_pairs_per_sec",
		xbenchSafeRate(iReferenceCount, iReferenceElapsed)
	);
	xbenchPrintMetricDouble(
		"runtime_calls_per_sec",
		xbenchSafeRate(iCallCount, iCallElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum ^ Env.Calls);

	xrtCallResultUnit(&Result);
	xrtObjectUnref(pObject);
	xrtCallableUnref(pCallable);
	xrtValueRelease(Env.Value);
	return 0;
}
