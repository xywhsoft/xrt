#include "../test.h"



static int testWeakDropCount = 0;



/* 记录弱引用测试对象最终析构。 */
static void testWeakDrop(ptr pValue, const xrttype* pType)
{
	(void)pValue;
	(void)pType;
	testWeakDropCount++;
}



/* 验证弱引用 Value 的复制、Take、锁定、过期和身份比较。 */
int main(void)
{
	xrtinstanceops Ops = { .Drop = testWeakDrop };
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.ValueWeak")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueWeak"),
		.AbiName = XRT_STR_INIT("tests.runtime.ValueWeak"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64),
		.InstanceOps = &Ops
	};
	xrtobject* pObject = xrtObjectCreate(&Type);
	xrtobject* pLocked;
	xrtweak Weak = { 0 };
	xrtweak Moved = { 0 };
	xrtweak Copied = { 0 };
	xrtweak Empty = { 0 };
	xvalue* pFirst;
	xvalue* pSecond;
	xvalue* pEmpty;
	uint64 iFirstHash;
	uint64 iSecondHash;

	testRequire(pObject != NULL, "weak Value object creation failed");
	*(int64*)xrtObjectData(pObject) = 42;
	testRequire(xrtWeakInit(&Weak, pObject), "weak Value init failed");
	testRequire(xrtWeakCopy(&Moved, &Weak), "weak Value move fixture failed");
	pFirst = xrtValueWeak(&Weak);
	pSecond = xrtValueWeakTake(&Moved);
	testRequire((pFirst != NULL) && (pSecond != NULL),
		"weak Value creation failed");
	testRequire(Moved.Control == NULL,
		"weak Value Take did not clear source");
	testRequire(xrtValueIsWeak(pFirst),
		"weak Value kind was not recognized");
	testRequire(!xrtValueIsWeak(xrtValueBool(false)),
		"boolean was recognized as weak Value");
	testRequire(
		xrtValueHash(pFirst, &iFirstHash) &&
		xrtValueHash(pSecond, &iSecondHash) &&
		(iFirstHash == iSecondHash) &&
		xrtValueScalarEqual(pFirst, pSecond),
		"weak Value identity hash/equality mismatch"
	);
	testRequire(xrtValueGetWeak(pFirst, &Copied),
		"weak Value copy-out failed");
	pLocked = xrtValueWeakLock(pFirst);
	testRequire(
		(pLocked == pObject) &&
		(*(int64*)xrtObjectData(pLocked) == 42),
		"weak Value lock did not preserve object identity"
	);
	xrtObjectUnref(pLocked);
	testRequire(!xrtValueWeakExpired(pFirst),
		"live weak Value reported expired");

	xrtObjectUnref(pObject);
	testRequire(xrtValueWeakExpired(pFirst),
		"weak Value did not expire after final strong release");
	testRequire(xrtValueWeakLock(pFirst) == NULL,
		"expired weak Value became live");
	testRequire(xrtWeakExpired(&Copied),
		"copied weak Value output did not expire");
	testRequire(testWeakDropCount == 1,
		"weak Value extended or duplicated object lifetime");

	pEmpty = xrtValueWeak(&Empty);
	testRequire(
		(pEmpty != NULL) && xrtValueIsWeak(pEmpty) &&
		xrtValueWeakExpired(pEmpty) &&
		(xrtValueWeakLock(pEmpty) == NULL),
		"empty weak Value contract mismatch"
	);
	xrtValueRelease(pEmpty);
	xrtValueRelease(pSecond);
	xrtValueRelease(pFirst);
	xrtWeakUnit(&Copied);
	xrtWeakUnit(&Weak);
	xrtClearError();
	printf("[PASS] runtime Value weak\n");
	return 0;
}
