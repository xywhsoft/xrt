#include "../test.h"



typedef struct testcallabletypeenv {
	int DropCount;
} testcallabletypeenv;



/* 空入口用于构造具有独立生命周期的 callable。 */
static bool testCallableTypeEntry(
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



/* 记录 callable 环境只在最后一个类型槽引用释放时析构一次。 */
static void testCallableTypeDrop(ptr pEnvironment)
{
	testcallabletypeenv* pEnv = (testcallabletypeenv*)pEnvironment;

	pEnv->DropCount++;
}



/* 验证 callable 类型描述的复制、移动、身份比较、散列和释放契约。 */
int main(void)
{
	const xrttype* pType = xrtTypeCallable();
	testcallabletypeenv FirstEnv = { 0 };
	testcallabletypeenv SecondEnv = { 0 };
	xrtcallable* pSource = xrtCallableCreate(
		NULL, testCallableTypeEntry, &FirstEnv, testCallableTypeDrop);
	xrtcallable* pCopy = xrtCallableCreate(
		NULL, testCallableTypeEntry, &SecondEnv, testCallableTypeDrop);
	xrtcallable* pClone;
	xrtcallable* pMoved = NULL;
	xrtcallable* pEmpty = (xrtcallable*)(uintptr_t)1u;
	uint64 iSourceHash;
	uint64 iCopyHash;
	int iCompare;

	testRequire((pSource != NULL) && (pCopy != NULL),
		"callable type fixture failed");
	testRequire(
		xrtTypeValidate(pType) &&
		(pType->Id == xrtTypeId(XRT_STR_LITERAL("xrt.callable"))) &&
		(pType->Kind == XRT_TYPE_CALLABLE) &&
		(pType->Name.Size == (sizeof("callable") - 1u)) &&
		(memcmp(pType->Name.Data, "callable", pType->Name.Size) == 0) &&
		(pType->AbiName.Size == (sizeof("xrt.callable") - 1u)) &&
		(memcmp(
			pType->AbiName.Data, "xrt.callable", pType->AbiName.Size
		) == 0) &&
		(pType->Size == sizeof(xrtcallable*)) &&
		xrtTypeIsCopyable(pType) &&
		xrtTypeIsRelocatable(pType) &&
		xrtTypeIsComparable(pType) &&
		xrtTypeIsHashable(pType),
		"callable type descriptor is invalid"
	);
	testRequire(
		xrtTypeInitValue(pType, &pClone) && (pClone == NULL),
		"callable clone slot initialization mismatch"
	);
	testRequire(
		xrtTypeCopyValue(pType, &pCopy, &pSource) &&
		(pCopy == pSource) && (SecondEnv.DropCount == 1),
		"callable type copy ownership mismatch"
	);
	testRequire(
		xrtTypeCloneValue(pType, &pClone, &pSource) &&
		(pClone == pSource),
		"callable type clone ownership mismatch"
	);
	testRequire(
		xrtTypeCompareValue(pType, &pSource, &pCopy, &iCompare) &&
		(iCompare == 0) &&
		xrtTypeHashValue(pType, &pSource, &iSourceHash) &&
		xrtTypeHashValue(pType, &pCopy, &iCopyHash) &&
		(iSourceHash == iCopyHash),
		"callable type identity mismatch"
	);
	testRequire(
		xrtTypeMoveValue(pType, &pMoved, &pCopy) &&
		(pMoved == pSource) && (pCopy == NULL),
		"callable type move ownership mismatch"
	);
	testRequire(
		xrtTypeInitValue(pType, &pEmpty) && (pEmpty == NULL),
		"callable type initialization mismatch"
	);

	xrtTypeDropValue(pType, &pEmpty);
	xrtTypeDropValue(pType, &pClone);
	xrtTypeDropValue(pType, &pMoved);
	testRequire(FirstEnv.DropCount == 0,
		"callable type released the source reference too early");
	xrtCallableUnref(pSource);
	testRequire(FirstEnv.DropCount == 1,
		"callable type leaked or double-dropped its source");
	xrtClearError();
	printf("[PASS] runtime callable type\n");
	return 0;
}
