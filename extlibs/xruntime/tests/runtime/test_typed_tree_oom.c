#include "../test.h"



typedef struct testtypedtreeoom {
	bool Fail;
} testtypedtreeoom;



/* 按开关分配类型树测试内存。 */
static ptr testTypedTreeOomAlloc(ptr pContext, size_t iSize)
{
	testtypedtreeoom* pState = (testtypedtreeoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 按开关重分配类型树测试内存。 */
static ptr testTypedTreeOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testtypedtreeoom* pState = (testtypedtreeoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放类型树测试内存。 */
static void testTypedTreeOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 初始化 OOM 测试整数。 */
static bool testTypedTreeOomInit(ptr pValue, const xrttype* pType)
{
	(void)pType;
	*(int*)pValue = 0;
	return true;
}



/* 通过 XRT 分配探针失败原子地复制测试整数。 */
static bool testTypedTreeOomCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	ptr pProbe;
	(void)pType;

	pProbe = xrtMalloc(8u * 1024u * 1024u);
	if ( pProbe == NULL ) {
		return false;
	}
	xrtFree(pProbe);
	*(int*)pTarget = *(const int*)pSource;
	return true;
}



/* 移动并清空 OOM 测试整数。 */
static bool testTypedTreeOomMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	(void)pType;
	*(int*)pTarget = *(int*)pSource;
	*(int*)pSource = 0;
	return true;
}



/* 清空 OOM 测试整数。 */
static void testTypedTreeOomDrop(ptr pValue, const xrttype* pType)
{
	(void)pType;
	*(int*)pValue = 0;
}



/* 比较两个 OOM 测试整数。 */
static int testTypedTreeOomCompare(
	const void* pLeft,
	const void* pRight,
	const xrttype* pType
)
{
	int iLeft = *(const int*)pLeft;
	int iRight = *(const int*)pRight;
	(void)pType;

	return (iLeft > iRight) - (iLeft < iRight);
}



/* 构造每次复制都经过分配探针的测试类型。 */
static xrttype testTypedTreeOomType(void)
{
	static const xrttypeops Ops = {
		.Init = testTypedTreeOomInit,
		.Copy = testTypedTreeOomCopy,
		.Move = testTypedTreeOomMove,
		.Drop = testTypedTreeOomDrop,
		.Clone = testTypedTreeOomCopy,
		.Compare = testTypedTreeOomCompare
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-tree.Oom")),
		.Kind = XRT_TYPE_RECORD,
		.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("Oom"),
		.AbiName = XRT_STR_INIT("tests.typed-tree.Oom"),
		.Size = sizeof(int),
		.Align = TEST_ALIGNOF(int),
		.InstanceSize = sizeof(int),
		.InstanceAlign = TEST_ALIGNOF(int),
		.Ops = &Ops
	};

	return Type;
}



/* 验证插入、替换和深复制 OOM 均保留原树。 */
int main(void)
{
	testtypedtreeoom State = { false };
	xallocator Allocator = {
		&State,
		testTypedTreeOomAlloc,
		testTypedTreeOomRealloc,
		testTypedTreeOomFree
	};
	xrttype Type = testTypedTreeOomType();
	xtypedtree Tree;
	xtypedtree* pClone;
	int iFirstKey = 1;
	int iSecondKey = 2;
	int iFirstValue = 10;
	int iReplacement = 20;
	xerrkind Kind;

	testRequire(
		xrtSetAllocator(&Allocator),
		"typed tree OOM allocator install failed"
	);
	testRequire(
		xrtTypedTreeInit(&Tree, xrtTypeInt32(), &Type) &&
		xrtTypedTreeSet(&Tree, &iFirstKey, &iFirstValue),
		"typed tree OOM fixture failed"
	);
	State.Fail = true;
	xrtClearError();
	testRequire(
		!xrtTypedTreeSet(&Tree, &iFirstKey, &iReplacement),
		"typed tree replacement survived forced OOM"
	);
	Kind = xrtErrorKind(xrtGetError());
	testRequire(
		Kind == XERR_MEMORY,
		"typed tree replacement did not preserve the OOM error"
	);
	testRequire(
		*(const int*)xrtTypedTreeConstGet(&Tree, &iFirstKey) == 10,
		"typed tree replacement OOM changed the existing value"
	);
	xrtClearError();
	testRequire(
		!xrtTypedTreeSet(&Tree, &iSecondKey, &iReplacement),
		"typed tree insertion survived forced OOM"
	);
	Kind = xrtErrorKind(xrtGetError());
	testRequire(
		Kind == XERR_MEMORY,
		"typed tree insertion did not preserve the OOM error"
	);
	testRequire(
		(xrtTypedTreeCount(&Tree) == 1u) &&
		!xrtTypedTreeHas(&Tree, &iSecondKey),
		"typed tree insertion OOM changed visible state"
	);
	xrtClearError();
	pClone = xrtTypedTreeClone(&Tree);
	testRequire(
		(pClone == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTypedTreeCount(&Tree) == 1u),
		"typed tree clone OOM changed or lost the source"
	);
	State.Fail = false;
	xrtTypedTreeUnit(&Tree);
	xrtClearError();
	printf("[PASS] typed tree OOM\n");
	return 0;
}
