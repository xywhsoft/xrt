#include "../test.h"



typedef struct testpayload {
	int Value;
	int* DropCount;
} testpayload;



typedef struct testnativepayload {
	ptr Handle;
	int* DropCount;
} testnativepayload;



static int* gDropCount;



/* 验证对象值追踪访问预期的强引用。 */
static bool testObjectValueVisit(xrtobject* pObject, ptr pContext)
{
	return pObject == (xrtobject*)pContext;
}



/* 初始化测试对象负载并绑定析构计数器。 */
static bool testPayloadInit(ptr pValue, const xrttype* pType)
{
	testpayload* pPayload = (testpayload*)pValue;
	(void)pType;

	pPayload->Value = 7;
	pPayload->DropCount = gDropCount;
	return true;
}



/* 销毁测试对象负载且只累计一次。 */
static void testPayloadDrop(ptr pValue, const xrttype* pType)
{
	testpayload* pPayload = (testpayload*)pValue;
	(void)pType;

	if ( pPayload->DropCount != NULL ) {
		(*pPayload->DropCount)++;
		pPayload->DropCount = NULL;
	}
}



/* 释放运行时对象承载的外部 native 句柄，并确保只执行一次。 */
static void testNativePayloadDrop(ptr pValue, const xrttype* pType)
{
	testnativepayload* pPayload = (testnativepayload*)pValue;
	(void)pType;

	if ( pPayload->Handle != NULL ) {
		pPayload->Handle = NULL;
		(*pPayload->DropCount)++;
	}
}



/* 验证弱引用错误使用统一对象错误域和稳定操作名。 */
static bool testWeakError(cstr sOperation)
{
	const xerror* pError = xrtGetError();

	return
		(pError != NULL) &&
		(xrtErrorKind(pError) == XERR_ARGUMENT) &&
		(strcmp(xrtErrorDomain(pError), "xrt.object") == 0) &&
		(xrtErrorCode(pError) == XOBJECT_ERROR_WEAK) &&
		(strcmp(xrtErrorOperation(pError), sOperation) == 0);
}



/* 模拟初始化回调失败并发布可追踪的下层错误。 */
static bool testPayloadInitFail(ptr pValue, const xrttype* pType)
{
	xerror* pError;
	(void)pValue;
	(void)pType;

	pError = xrtErrorCreate(XERR_VALUE, "test.object.init", 17,
		"test initializer rejected the value");
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 验证引用生命周期、过对齐载荷和完整弱引用值语义。 */
static void testObjectLifetime(void)
{
	static const xrtinstanceops Ops = {
		.Init = testPayloadInit,
		.Drop = testPayloadDrop
	};
	int iDropCount = 0;
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.Managed")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE |
			XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("Managed"),
		.AbiName = XRT_STR_INIT("tests.runtime.Managed"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(testpayload),
		.InstanceAlign = 64u,
		.Ops = xrtObjectValueOps(),
		.InstanceOps = &Ops
	};
	xrtweak Weak = { 0 };
	xrtweak Copy = { 0 };
	xrtweak Moved = { 0 };
	xrtobject* pObject;
	xrtobject* pLocked;
	xrtobject* pSource;
	xrtobject* pCopied = NULL;
	xrtobject* pMoved = NULL;
	testpayload* pPayload;
	int iCompare;
	uint64 iHash;

	gDropCount = &iDropCount;
	pObject = xrtObjectCreateSized(&Type, 128u);
	testRequire(pObject != NULL, "managed object creation failed");
	testRequire(xrtObjectType(pObject) == &Type, "managed object type mismatch");
	testRequire(xrtObjectSize(pObject) == 128u, "managed object size mismatch");
	pPayload = (testpayload*)xrtObjectData(pObject);
	testRequire((pPayload != NULL) && (pPayload->Value == 7), "object init mismatch");
	testRequire(
		(xrtObjectRefCount(pObject) == 1u) && xrtObjectUnique(pObject),
		"new object reference state mismatch"
	);
	testRequire(
		(((uintptr_t)pPayload & UINT64_C(63)) == 0u),
		"runtime object payload alignment mismatch"
	);

	pSource = pObject;
	testRequire(
		xrtTypeCopyValue(&Type, &pCopied, &pSource) &&
		(pCopied == pObject) &&
		(xrtObjectRefCount(pObject) == 2u),
		"object strong-reference value copy failed"
	);
	testRequire(
		xrtTypeCompareValue(&Type, &pSource, &pCopied, &iCompare) &&
		(iCompare == 0) &&
		xrtTypeHashValue(&Type, &pSource, &iHash) &&
		(iHash == (uint64)(uintptr_t)pObject) &&
		xrtTypeTraceValue(&Type, &pCopied, testObjectValueVisit, pObject),
		"object strong-reference value comparison, hash, or trace failed"
	);
	testRequire(
		xrtTypeMoveValue(&Type, &pMoved, &pCopied) &&
		(pMoved == pObject) && (pCopied == NULL),
		"object strong-reference value move failed"
	);
	xrtTypeDropValue(&Type, &pMoved);
	testRequire(
		(pMoved == NULL) && (xrtObjectRefCount(pObject) == 1u),
		"object strong-reference value drop failed"
	);

	testRequire(
		xrtWeakInit(&Weak, pObject) &&
		xrtWeakCopy(&Copy, &Weak) &&
		xrtWeakMove(&Moved, &Copy),
		"weak reference value operation failed"
	);
	testRequire(Copy.Control == NULL, "weak move did not empty its source");
	testRequire(
		xrtWeakCopy(&Weak, &Weak) && xrtWeakMove(&Moved, &Moved),
		"weak self operation failed"
	);
	testRequire(xrtWeakSet(&Copy, pObject), "weak set failed");
	testRequire(xrtWeakCopy(&Moved, &Copy), "weak replacing copy failed");
	testRequire(xrtWeakMove(&Moved, &Copy), "weak replacing move failed");
	testRequire(Copy.Control == NULL, "replacing weak move did not empty source");
	testRequire(xrtWeakSet(&Copy, pObject), "weak reset failed");
	testRequire(xrtWeakSet(&Copy, NULL), "weak clear through set failed");
	testRequire(!xrtWeakExpired(&Weak), "live weak reference expired");
	pLocked = xrtWeakLock(&Weak);
	testRequire(pLocked == pObject, "weak lock failed");
	testRequire(
		(xrtObjectRefCount(pObject) == 2u) && !xrtObjectUnique(pObject),
		"weak lock reference state mismatch"
	);
	testRequire(xrtObjectRef(pObject) == pObject, "strong retain failed");
	xrtObjectUnref(pLocked);
	xrtObjectUnref(pObject);
	testRequire(
		(xrtObjectRefCount(pObject) == 1u) && xrtObjectUnique(pObject),
		"released object reference state mismatch"
	);
	testRequire(iDropCount == 0, "object dropped before its last strong reference");
	xrtObjectUnref(pObject);
	testRequire(iDropCount == 1, "object drop count mismatch");
	testRequire(
		xrtWeakExpired(&Weak) && (xrtWeakLock(&Weak) == NULL),
		"destroyed object remained lockable"
	);
	xrtWeakUnit(&Moved);
	xrtWeakUnit(&Weak);
	testRequire((Moved.Control == NULL) && (Weak.Control == NULL),
		"weak unit mismatch");
}



/* 验证旧版 weakable handle 由统一 native-backed 对象模型完整承接。 */
static void testNativeBackedWeakObject(void)
{
	static const xrtinstanceops Ops = {
		.Drop = testNativePayloadDrop
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.NativeBacked")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("NativeBacked"),
		.AbiName = XRT_STR_INIT("tests.runtime.NativeBacked"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(testnativepayload),
		.InstanceAlign = TEST_ALIGNOF(testnativepayload),
		.InstanceOps = &Ops
	};
	int iNativeValue = 23;
	int iDropCount = 0;
	xrtweak Weak = { 0 };
	xrtobject* pObject = xrtObjectCreate(&Type);
	xrtobject* pLocked;
	testnativepayload* pPayload;

	testRequire(pObject != NULL, "native-backed object creation failed");
	pPayload = (testnativepayload*)xrtObjectData(pObject);
	pPayload->Handle = &iNativeValue;
	pPayload->DropCount = &iDropCount;
	testRequire(xrtWeakInit(&Weak, pObject),
		"native-backed weak reference creation failed");
	pLocked = xrtWeakLock(&Weak);
	testRequire(
		(pLocked == pObject) &&
		(((testnativepayload*)xrtObjectData(pLocked))->Handle == &iNativeValue),
		"native-backed weak lock lost handle identity"
	);

	xrtObjectUnref(pObject);
	testRequire((iDropCount == 0) && !xrtWeakExpired(&Weak),
		"native-backed object dropped while a strong lock remained");
	xrtObjectUnref(pLocked);
	testRequire(
		(iDropCount == 1) && xrtWeakExpired(&Weak) &&
		(xrtWeakLock(&Weak) == NULL),
		"native-backed object weak lifetime mismatch"
	);
	xrtWeakUnit(&Weak);
}



/* 验证对象参数错误和初始化失败原因链。 */
static void testObjectErrors(void)
{
	static const xrtinstanceops FailOps = {
		.Init = testPayloadInitFail
	};
	xrttype ValueType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.ValueObject")),
		.Kind = XRT_TYPE_RECORD,
		.Name = XRT_STR_INIT("ValueObject"),
		.AbiName = XRT_STR_INIT("tests.runtime.ValueObject"),
		.Size = sizeof(int64),
		.Align = TEST_ALIGNOF(int64),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64)
	};
	xrttype FailType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.FailedObject")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("FailedObject"),
		.AbiName = XRT_STR_INIT("tests.runtime.FailedObject"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64),
		.InstanceOps = &FailOps
	};
	const xerror* pError;
	const xerror* pCause;

	xrtClearError();
	testRequire(xrtObjectCreate(&ValueType) == NULL,
		"runtime object accepted a value type");
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.object") == 0) &&
		(xrtErrorKind(pError) == XERR_TYPE) &&
		(xrtErrorCode(pError) == XOBJECT_ERROR_TYPE),
		"runtime object type error mismatch"
	);

	xrtClearError();
	testRequire(xrtObjectCreateSized(&FailType, sizeof(int32)) == NULL,
		"runtime object accepted an undersized payload");
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XOBJECT_ERROR_SIZE),
		"runtime object size error mismatch"
	);

	xrtClearError();
	testRequire(xrtObjectCreate(&FailType) == NULL,
		"runtime object accepted a failed initializer");
	pError = xrtGetError();
	pCause = xrtErrorCause(pError);
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.object") == 0) &&
		(xrtErrorCode(pError) == XOBJECT_ERROR_INITIALIZE) &&
		(pCause != NULL) &&
		(strcmp(xrtErrorDomain(pCause), "test.object.init") == 0) &&
		(xrtErrorCode(pCause) == 17),
		"runtime object initializer cause mismatch"
	);

	xrtClearError();
	testRequire(xrtObjectData(NULL) == NULL,
		"runtime object null data query succeeded");
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) == XOBJECT_ERROR_REFERENCE),
		"runtime object null query error mismatch"
	);
}



/* 验证弱引用空值、参数错误、失败原子性和错误保持契约。 */
static void testWeakErrors(void)
{
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.WeakErrors")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("WeakErrors"),
		.AbiName = XRT_STR_INIT("tests.runtime.WeakErrors"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64)
	};
	xrtweak Weak = { 0 };
	xrtweak Empty = { 0 };
	xrtobject* pObject = xrtObjectCreate(&Type);
	xerror* pMarker;
	const xerror* pBefore;

	testRequire(pObject != NULL, "weak error fixture creation failed");
	xrtClearError();
	testRequire(!xrtWeakInit(NULL, pObject) && testWeakError("weak-init"),
		"weak init null destination error mismatch");
	testRequire(xrtWeakInit(&Weak, pObject),
		"weak error fixture initialization failed");
	xrtClearError();
	testRequire(
		!xrtWeakInit(&Weak, pObject) && testWeakError("weak-init") &&
		!xrtWeakExpired(&Weak),
		"repeated weak init changed its destination"
	);

	xrtClearError();
	testRequire(!xrtWeakCopy(NULL, &Weak) && testWeakError("weak-copy"),
		"weak copy null destination error mismatch");
	xrtClearError();
	testRequire(!xrtWeakCopy(&Empty, NULL) && testWeakError("weak-copy"),
		"weak copy null source error mismatch");
	xrtClearError();
	testRequire(!xrtWeakMove(NULL, &Weak) && testWeakError("weak-move"),
		"weak move null destination error mismatch");
	xrtClearError();
	testRequire(!xrtWeakMove(&Empty, NULL) && testWeakError("weak-move"),
		"weak move null source error mismatch");
	xrtClearError();
	testRequire(!xrtWeakSet(NULL, pObject) && testWeakError("weak-set"),
		"weak set null destination error mismatch");
	xrtClearError();
	testRequire(xrtWeakExpired(NULL) && testWeakError("weak-expired"),
		"weak expired null argument error mismatch");
	xrtClearError();
	testRequire((xrtWeakLock(NULL) == NULL) && testWeakError("weak-lock"),
		"weak lock null argument error mismatch");

	pMarker = xrtErrorCreate(
		XERR_VALUE,
		"test.weak.marker",
		31,
		"keep this error"
	);
	testRequire(pMarker != NULL, "weak marker error creation failed");
	xrtSetError(pMarker);
	xrtErrorFree(pMarker);
	pBefore = xrtGetError();
	testRequire(
		xrtWeakExpired(&Empty) && (xrtWeakLock(&Empty) == NULL) &&
		(xrtGetError() == pBefore),
		"empty weak query overwrote the current error"
	);
	xrtWeakUnit(NULL);
	testRequire(xrtGetError() == pBefore,
		"null weak unit overwrote the current error");

	xrtObjectUnref(pObject);
	testRequire(
		xrtWeakExpired(&Weak) && (xrtWeakLock(&Weak) == NULL) &&
		(xrtGetError() == pBefore),
		"expired weak query overwrote the current error"
	);
	xrtWeakUnit(&Weak);
	xrtWeakUnit(&Empty);
	xrtClearError();
}



/* 运行运行时对象的完整常规测试。 */
int main(void)
{
	testObjectLifetime();
	testNativeBackedWeakObject();
	testObjectErrors();
	testWeakErrors();
	xrtClearError();
	printf("[PASS] runtime object\n");
	return 0;
}
