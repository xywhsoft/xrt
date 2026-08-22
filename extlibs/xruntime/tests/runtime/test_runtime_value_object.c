#include "../test.h"



static int testObjectDropCount = 0;



/* 记录运行时对象负载只析构一次。 */
static void testObjectDrop(ptr pValue, const xrttype* pType)
{
	(void)pValue;
	(void)pType;
	testObjectDropCount++;
}



/* 构造对象桥接测试使用的引用类型。 */
static xrttype testObjectType(void)
{
	static const xrtinstanceops Ops = {
		.Drop = testObjectDrop
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.ValueObject")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueObject"),
		.AbiName = XRT_STR_INIT("tests.runtime.ValueObject"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64),
		.InstanceOps = &Ops
	};

	return Type;
}



/* 验证最近一次错误来自运行时 Value 桥接层。 */
static void testRuntimeValueError(
	xruntimevalueerror Code,
	cstr sOperation
)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, "runtime Value error is missing");
	testRequire(
		strcmp(xrtErrorDomain(pError), "xrt.runtime-value") == 0,
		"runtime Value error domain mismatch"
	);
	testRequire(xrtErrorCode(pError) == (int32)Code,
		"runtime Value error code mismatch");
	testRequire(
		strcmp(xrtErrorOperation(pError), sOperation) == 0,
		"runtime Value error operation mismatch"
	);
}



/* 验证对象身份、借用读取、Take、Hash/Equal 和精确析构。 */
int main(void)
{
	xrttype Type = testObjectType();
	xrtobject* pObject = xrtObjectCreate(&Type);
	xrtobject* pOther = xrtObjectCreate(&Type);
	xrtobject* pTaken = xrtObjectCreate(&Type);
	xrtobject* pEmpty = NULL;
	xvalue* pFirst;
	xvalue* pSecond;
	xvalue* pOtherValue;
	xvalue* pTakenValue;
	xvalue* pClone;
	uint64 iFirstHash;
	uint64 iSecondHash;

	testRequire((pObject != NULL) && (pOther != NULL) && (pTaken != NULL),
		"runtime object fixture creation failed");
	*(int64*)xrtObjectData(pObject) = 42;
	pFirst = xrtValueRuntimeObject(pObject);
	pSecond = xrtValueRuntimeObject(pObject);
	pOtherValue = xrtValueRuntimeObject(pOther);
	testRequire(
		(pFirst != NULL) && (pSecond != NULL) && (pOtherValue != NULL),
		"runtime object Value creation failed"
	);
	testRequire(xrtValueIsRuntimeObject(pFirst),
		"runtime object Value kind was not recognized");
	testRequire(!xrtValueIsRuntimeObject(xrtValueBool(true)),
		"boolean was recognized as a runtime object");
	testRequire(xrtValueGetRuntimeObject(pFirst) == pObject,
		"runtime object Value identity mismatch");
	testRequire(
		*(int64*)xrtObjectData(xrtValueGetRuntimeObject(pFirst)) == 42,
		"runtime object Value payload mismatch"
	);
	testRequire(
		xrtValueHash(pFirst, &iFirstHash) &&
		xrtValueHash(pSecond, &iSecondHash) &&
		(iFirstHash == iSecondHash),
		"runtime object Value hash mismatch"
	);
	testRequire(xrtValueScalarEqual(pFirst, pSecond),
		"same runtime object identity was not equal");
	testRequire(!xrtValueScalarEqual(pFirst, pOtherValue),
		"different runtime object identities compared equal");
	pClone = xrtValueClone(pFirst);
	testRequire(pClone == pFirst,
		"ordinary runtime object Value clone changed identity");
	xrtValueRelease(pClone);

	pTakenValue = xrtValueRuntimeObjectTake(&pTaken);
	testRequire((pTakenValue != NULL) && (pTaken == NULL),
		"runtime object Value Take did not consume source");
	xrtClearError();
	testRequire(xrtValueGetRuntimeObject(xrtValueBool(true)) == NULL,
		"non-object Value produced a runtime object");
	testRuntimeValueError(XRUNTIME_VALUE_ERROR_TYPE, "object-get");
	xrtClearError();
	testRequire(xrtValueRuntimeObject(NULL) == NULL,
		"null runtime object produced a Value");
	testRuntimeValueError(XRUNTIME_VALUE_ERROR_OBJECT, "object");
	xrtClearError();
	testRequire(xrtValueRuntimeObjectTake(NULL) == NULL,
		"null runtime object source was accepted");
	testRuntimeValueError(XRUNTIME_VALUE_ERROR_OWNERSHIP, "object-take");
	xrtClearError();
	testRequire(xrtValueRuntimeObjectTake(&pEmpty) == NULL,
		"empty runtime object source was accepted");
	testRuntimeValueError(XRUNTIME_VALUE_ERROR_OWNERSHIP, "object-take");

	xrtObjectUnref(pOther);
	xrtObjectUnref(pObject);
	xrtValueRelease(pTakenValue);
	xrtValueRelease(pOtherValue);
	xrtValueRelease(pSecond);
	xrtValueRelease(pFirst);
	testRequire(testObjectDropCount == 3,
		"runtime object bridge drop count mismatch");
	xrtClearError();
	printf("[PASS] runtime Value object\n");
	return 0;
}
