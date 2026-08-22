#include "../test.h"



/* 安装一个用于验证成功路径隔离的宿主错误。 */
static void testDynamicFieldHostError(void)
{
	xerror* pError = xrtErrorCreate(
		XERR_IO, "test.dynamic-field.host", 41, "host error"
	);

	testRequire(pError != NULL,
		"dynamic field host error fixture failed");
	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 验证迭代期间的对象保留和结构修改拒绝。 */
static void testDynamicFieldIteratorContract(void)
{
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	xrtdynamicfields* pEmpty = xrtDynamicFieldsCreate();
	xrtdynamicfielditer Iterator = { 0 };
	xrtweak Weak = { 0 };
	const xvalue* pValue;
	xstrview Name;

	testRequire(
		(pFields != NULL) && (pEmpty != NULL) &&
		xrtDynamicFieldsSetNew(
			pFields, XRT_STR_LITERAL("value"), xrtValueInt(7)
		) &&
		xrtWeakInit(&Weak, pFields) &&
		xrtDynamicFieldsIterBegin(pFields, &Iterator),
		"dynamic field iterator fixture failed"
	);
	xrtDynamicFieldsUnref(pFields);
	testRequire(!xrtWeakExpired(&Weak),
		"dynamic field iterator did not retain its object");
	testRequire(
		xrtDynamicFieldsMerge(Iterator.Fields, pEmpty, true),
		"empty dynamic field merge was not a no-op"
	);
	pValue = xrtDynamicFieldsIterNext(&Iterator, &Name);
	testRequire(
		(pValue != NULL) && (Name.Size == 5u) &&
		(memcmp(Name.Data, "value", 5u) == 0),
		"dynamic field iterator order mismatch"
	);
	testRequire(
		xrtDynamicFieldsSetNew(
			Iterator.Fields, XRT_STR_LITERAL("later"), xrtValueInt(8)
		) &&
		(xrtDynamicFieldsIterNext(&Iterator, NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"dynamic field iterator did not detect structural modification"
	);
	xrtDynamicFieldsIterEnd(&Iterator);
	testRequire(xrtWeakExpired(&Weak),
		"dynamic field iterator leaked its retained object");
	xrtWeakUnit(&Weak);
	xrtDynamicFieldsUnref(pEmpty);
}



/* 验证集合与事务合并成功时保留调用方已有错误。 */
static void testDynamicFieldErrorIsolation(void)
{
	xrtdynamicfields* pSource = xrtDynamicFieldsCreate();
	xrtdynamicfields* pTarget = xrtDynamicFieldsCreate();
	xvalue* pKeys;
	const xerror* pError;

	testRequire(
		(pSource != NULL) && (pTarget != NULL) &&
		xrtDynamicFieldsSetNew(
			pSource, XRT_STR_LITERAL("value"), xrtValueInt(7)
		),
		"dynamic field error isolation fixture failed"
	);
	testDynamicFieldHostError();
	pKeys = xrtDynamicFieldsKeys(pSource);
	pError = xrtGetError();
	testRequire(
		(pKeys != NULL) && (xrtValueCount(pKeys) == 1u) &&
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "test.dynamic-field.host") == 0) &&
		(xrtErrorCode(pError) == 41),
		"dynamic field collection replaced the caller error"
	);
	xrtValueRelease(pKeys);
	testRequire(
		xrtDynamicFieldsMerge(pTarget, pSource, true) &&
		(xrtDynamicFieldsCount(pTarget) == 1u),
		"dynamic field merge with a caller error failed"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "test.dynamic-field.host") == 0) &&
		(xrtErrorCode(pError) == 41),
		"dynamic field merge replaced the caller error"
	);
	xrtClearError();
	xrtDynamicFieldsUnref(pTarget);
	xrtDynamicFieldsUnref(pSource);
}



/* 验证无效参数、缺失查询和失败所有权保持。 */
static void testDynamicFieldFailureContract(void)
{
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	xvalue* pValue = xrtValueInt(9);
	xstrview Invalid = { NULL, 1u };

	testRequire((pFields != NULL) && (pValue != NULL),
		"dynamic field failure fixture failed");
	testRequire(
		(xrtDynamicFieldsGet(
			pFields, XRT_STR_LITERAL("missing")
		) == NULL) && (xrtGetError() == NULL) &&
		(xrtDynamicFieldsTake(
			pFields, XRT_STR_LITERAL("missing")
		) == NULL) && (xrtGetError() == NULL),
		"missing dynamic field was not a normal result"
	);
	testRequire(
		!xrtDynamicFieldsSetTake(pFields, Invalid, &pValue) &&
		(pValue != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(strcmp(xrtErrorDomain(xrtGetError()),
			"xrt.dynamic-field") == 0),
		"failed dynamic field move changed ownership or error domain"
	);
	xrtValueRelease(pValue);
	xrtDynamicFieldsUnref(pFields);
}



/* 运行动态字段边界契约测试。 */
int main(void)
{
	testDynamicFieldIteratorContract();
	xrtClearError();
	testDynamicFieldErrorIsolation();
	xrtClearError();
	testDynamicFieldFailureContract();
	xrtClearError();
	printf("[PASS] runtime dynamic field contract\n");
	return 0;
}
