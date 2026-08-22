#include "../test.h"



typedef struct testtypeoom {
	bool Fail;
	size_t Live;
} testtypeoom;



#define TEST_TYPE_OOM_HELD 4096u
#define TEST_TYPE_OOM_ARRAY_BYTES (16u * sizeof(ptr))



/* 在允许分配时记录一个活动底层块。 */
static ptr testTypeOomAlloc(ptr pContext, size_t iSize)
{
	testtypeoom* pState = (testtypeoom*)pContext;
	ptr pMemory;

	if ( pState->Fail ) {
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		pState->Live++;
	}
	return pMemory;
}



/* 失败时保留原块，成功时保持活动块数量不变。 */
static ptr testTypeOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testtypeoom* pState = (testtypeoom*)pContext;
	ptr pResult;

	if ( pState->Fail ) {
		return NULL;
	}
	if ( pMemory == NULL ) {
		return testTypeOomAlloc(pContext, iSize);
	}
	pResult = realloc(pMemory, iSize);
	return pResult;
}



/* 释放底层块并维护活动计数。 */
static void testTypeOomFree(ptr pContext, ptr pMemory)
{
	testtypeoom* pState = (testtypeoom*)pContext;

	if ( pMemory != NULL ) {
		testRequire(pState->Live != 0u, "runtime type OOM live count underflow");
		pState->Live--;
		free(pMemory);
	}
}



/* 持有目标尺寸类的全部空闲块，直到下一次底层 span 申请被拒绝。 */
static size_t testTypeOomExhaust(testtypeoom* pState,
	ptr* pHeld, size_t iCapacity)
{
	size_t iCount = 0u;

	pState->Fail = true;
	while ( iCount < iCapacity ) {
		pHeld[iCount] = xrtMalloc(TEST_TYPE_OOM_ARRAY_BYTES);
		if ( pHeld[iCount] == NULL ) {
			break;
		}
		iCount++;
	}
	testRequire(iCount < iCapacity, "runtime type OOM size class was not exhausted");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "size class exhaustion error mismatch");
	xrtClearError();
	return iCount;
}



/* 释放尺寸类耗尽阶段暂时持有的块。 */
static void testTypeOomRelease(testtypeoom* pState, ptr* pHeld, size_t iCount)
{
	pState->Fail = false;
	for ( size_t i = 0; i < iCount; i++ ) {
		xrtFree(pHeld[i]);
	}
}



/* 构造 OOM 注册测试使用的不可变类描述。 */
static xrttype testTypeOomClass(void)
{
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.OomType")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("OomType"),
		.AbiName = XRT_STR_INIT("tests.runtime.OomType"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64)
	};

	return Type;
}



/* 验证两个注册表在创建和增长 OOM 后保持原状态并可继续使用。 */
int main(void)
{
	testtypeoom State = { false, 0u };
	xallocator Allocator = {
		&State,
		testTypeOomAlloc,
		testTypeOomRealloc,
		testTypeOomFree
	};
	xrttype Concrete = testTypeOomClass();
	xrttype ProtocolType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.OomProtocol")),
		.Kind = XRT_TYPE_PROTOCOL,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("OomProtocol"),
		.AbiName = XRT_STR_INIT("tests.runtime.OomProtocol"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(ptr),
		.InstanceAlign = TEST_ALIGNOF(ptr)
	};
	xrtprotocol Protocol = { &ProtocolType, 0u, NULL };
	xrtprotocolwitness Witness = { &Protocol, &Concrete, 0u, NULL };
	xrttyperegistry* pTypes;
	xrtprotocolregistry* pProtocols;
	ptr arrHeld[TEST_TYPE_OOM_HELD];
	size_t iHeld;

	testRequire(xrtSetAllocator(&Allocator), "runtime type OOM allocator install failed");
	State.Fail = true;
	testRequire(xrtTypeRegistryCreate() == NULL, "type registry create survived OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "type registry create OOM mismatch");

	State.Fail = false;
	xrtClearError();
	pTypes = xrtTypeRegistryCreate();
	testRequire(pTypes != NULL, "type registry recovery create failed");
	pProtocols = xrtProtocolRegistryCreate();
	testRequire(pProtocols != NULL, "protocol registry recovery create failed");
	iHeld = testTypeOomExhaust(&State, arrHeld, TEST_TYPE_OOM_HELD);
	testRequire(!xrtTypeRegistryAdd(pTypes, &Concrete), "type registry grow survived OOM");
	testRequire(xrtTypeRegistryCount(pTypes) == 0u, "type registry OOM changed count");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "type registry grow OOM mismatch");
	testTypeOomRelease(&State, arrHeld, iHeld);
	testRequire(xrtTypeRegistryAdd(pTypes, &Concrete), "type registry did not recover from OOM");

	iHeld = testTypeOomExhaust(&State, arrHeld, TEST_TYPE_OOM_HELD);
	testRequire(
		!xrtProtocolRegistryAdd(pProtocols, &Witness),
		"protocol registry grow survived OOM"
	);
	testRequire(
		xrtProtocolRegistryCount(pProtocols) == 0u,
		"protocol registry OOM changed count"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"protocol registry grow OOM mismatch"
	);
	testTypeOomRelease(&State, arrHeld, iHeld);
	testRequire(
		xrtProtocolRegistryAdd(pProtocols, &Witness),
		"protocol registry did not recover from OOM"
	);
	xrtProtocolRegistryDestroy(pProtocols);
	xrtTypeRegistryDestroy(pTypes);
	xrtClearError();
	printf("[PASS] runtime type OOM\n");
	return 0;
}
