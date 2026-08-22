#include "../test.h"



static int testGraphObjectDropCount = 0;
static int testGraphCallableDropCount = 0;



/* 记录运行时对象和 callable 的最终释放次数。 */
static void testGraphObjectDrop(ptr pValue, const xrttype* pType)
{
	(void)pValue;
	(void)pType;
	testGraphObjectDropCount++;
}



/* 释放图测试 callable 的借用环境。 */
static void testGraphCallableDrop(ptr pEnvironment)
{
	(void)pEnvironment;
	testGraphCallableDropCount++;
}



/* 图测试不执行 callable，入口只用于构造完整的不可变句柄。 */
static bool testGraphCallableEntry(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	(void)pEnvironment;
	(void)pFrame;
	(void)pResult;
	return true;
}



/* 验证对象不伪装深拷贝，callable 和弱引用按各自身份策略复制。 */
int main(void)
{
	xrtinstanceops ObjectOps = { .Drop = testGraphObjectDrop };
	xrttype ObjectType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.ValueGraph")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueGraph"),
		.AbiName = XRT_STR_INIT("tests.runtime.ValueGraph"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64),
		.InstanceOps = &ObjectOps
	};
	xrtobject* pObject = xrtObjectCreate(&ObjectType);
	xrtweak Weak = { 0 };
	xvalue* pObjectValue;
	xvalue* pObjectCopy;
	xvalue* pWeakValue;
	xvalue* pWeakCopy;
	xrtcallable* pCallable;
	xvalue* pCallableValue;
	xvalue* pCallableCopy;
	xfuture* pFuture = NULL;
	xpromise* pPromise;
	xvalue* pFutureValue;
	xvalue* pFutureCopy;

	testRequire(pObject != NULL, "runtime Value graph object failed");
	testRequire(xrtWeakInit(&Weak, pObject),
		"runtime Value graph weak init failed");
	pObjectValue = xrtValueRuntimeObject(pObject);
	pWeakValue = xrtValueWeak(&Weak);
	testRequire((pObjectValue != NULL) && (pWeakValue != NULL),
		"runtime Value graph bridge creation failed");

	/* 深复制只克隆引用载体，可变对象仍保持同一稳定身份。 */
	xrtClearError();
	pObjectCopy = xrtValueDeepClone(pObjectValue);
	testRequire(
		(pObjectCopy != NULL) &&
		(pObjectCopy != pObjectValue) &&
		(xrtValueGetRuntimeObject(pObjectCopy) == pObject),
		"runtime object Value deep clone changed object identity"
	);

	/* 弱引用复制控制块引用，但不增加对象强引用。 */
	xrtClearError();
	pWeakCopy = xrtValueDeepClone(pWeakValue);
	testRequire(
		(pWeakCopy != NULL) &&
		(pWeakCopy != pWeakValue) &&
		xrtValueIsWeak(pWeakCopy) &&
		xrtValueScalarEqual(pWeakValue, pWeakCopy),
		"runtime weak Value deep clone mismatch"
	);
	xrtObjectUnref(pObject);
	testRequire(!xrtValueWeakExpired(pWeakCopy),
		"runtime object Value lost its strong reference");
	xrtValueRelease(pObjectValue);
	testRequire(!xrtValueWeakExpired(pWeakCopy),
		"runtime object clone released its shared identity early");
	xrtValueRelease(pObjectCopy);
	testRequire(
		xrtValueWeakExpired(pWeakValue) &&
		xrtValueWeakExpired(pWeakCopy) &&
		(testGraphObjectDropCount == 1),
		"runtime weak Value changed object lifetime"
	);
	xrtValueRelease(pWeakCopy);
	xrtValueRelease(pWeakValue);
	xrtWeakUnit(&Weak);

	/* 不可变 callable 深拷贝共享 callable 身份，但拥有独立 Value 外壳。 */
	pCallable = xrtCallableCreate(
		NULL, testGraphCallableEntry, NULL, testGraphCallableDrop);
	testRequire(pCallable != NULL,
		"runtime Value graph callable creation failed");
	pCallableValue = xrtValueCallable(pCallable);
	pCallableCopy = xrtValueDeepClone(pCallableValue);
	testRequire(
		(pCallableValue != NULL) &&
		(pCallableCopy != NULL) &&
		(pCallableCopy != pCallableValue) &&
		(xrtValueGetCallable(pCallableCopy) == pCallable) &&
		xrtValueScalarEqual(pCallableValue, pCallableCopy),
		"runtime callable Value deep clone mismatch"
	);
	xrtCallableUnref(pCallable);
	xrtValueRelease(pCallableValue);
	testRequire(testGraphCallableDropCount == 0,
		"runtime callable clone released its identity early");
	xrtValueRelease(pCallableCopy);
	testRequire(testGraphCallableDropCount == 1,
		"runtime callable clone drop count mismatch");

	/* Future 深复制增加消费端引用，保留同一异步结果身份。 */
	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire((pPromise != NULL) && (pFuture != NULL),
		"runtime Value graph Future creation failed");
	pFutureValue = xrtValueFuture(pFuture);
	pFutureCopy = xrtValueDeepClone(pFutureValue);
	testRequire(
		(pFutureValue != NULL) &&
		(pFutureCopy != NULL) &&
		(pFutureCopy != pFutureValue) &&
		(xrtValueGetFuture(pFutureCopy) == pFuture) &&
		xrtValueScalarEqual(pFutureValue, pFutureCopy),
		"runtime Future Value deep clone mismatch"
	);
	xrtFutureDestroy(pFuture);
	xrtValueRelease(pFutureValue);
	testRequire(!xrtPromiseDone(pPromise),
		"runtime Future clone changed pending state");
	testRequire(xrtPromiseResolve(pPromise, NULL),
		"runtime Future clone fixture could not resolve");
	testRequire(xrtFutureState(xrtValueGetFuture(pFutureCopy)) == XFUTURE_RESOLVED,
		"runtime Future clone lost shared result identity");
	xrtValueRelease(pFutureCopy);
	xrtPromiseDestroy(pPromise);

	xrtClearError();
	printf("[PASS] runtime Value graph contract\n");
	return 0;
}
