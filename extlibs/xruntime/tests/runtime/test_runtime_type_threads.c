#include "../test.h"
#include "../test_thread.h"



#define TEST_TYPE_THREAD_COUNT 6u
#define TEST_TYPE_DESCRIPTOR_COUNT 128u
#define TEST_TYPE_NAME_SIZE 48u



typedef enum testtypethreadmode {
	TEST_TYPE_ADD,
	TEST_TYPE_FIND,
	TEST_TYPE_REMOVE
} testtypethreadmode;



typedef struct testtypethreadcontext {
	xrttyperegistry* Registry;
	const xrttype* Types;
	size_t TypeCount;
	size_t ThreadIndex;
	size_t ThreadCount;
	testtypethreadmode Mode;
} testtypethreadcontext;



typedef struct testprotocolthreadcontext {
	xrtprotocolregistry* Registry;
	const xrtprotocolwitness* Witnesses;
	size_t WitnessCount;
	size_t ThreadIndex;
	size_t ThreadCount;
	testtypethreadmode Mode;
} testprotocolthreadcontext;



/* 根据阶段重复注册、查询或分片移除类型。 */
static int testTypeThreadRun(ptr pData)
{
	testtypethreadcontext* pContext = (testtypethreadcontext*)pData;

	if ( pContext->Mode == TEST_TYPE_ADD ) {
		for ( size_t i = 0; i < pContext->TypeCount; i++ ) {
			if ( !xrtTypeRegistryAdd(pContext->Registry, &pContext->Types[i]) ) {
				return 1;
			}
		}
		return 0;
	}
	if ( pContext->Mode == TEST_TYPE_FIND ) {
		for ( size_t i = 0; i < pContext->TypeCount; i++ ) {
			const xrttype* pType = &pContext->Types[i];

			if (
				(xrtTypeRegistryFindId(pContext->Registry, pType->Id) != pType) ||
				(xrtTypeRegistryFindName(pContext->Registry, pType->AbiName) != pType)
			) {
				return 2;
			}
		}
		return 0;
	}
	for ( size_t i = pContext->ThreadIndex;
		  i < pContext->TypeCount; i += pContext->ThreadCount ) {
		if ( !xrtTypeRegistryRemove(pContext->Registry, &pContext->Types[i]) ) {
			return 3;
		}
	}
	return 0;
}



/* 根据阶段重复注册、查询或分片移除协议见证。 */
static int testProtocolThreadRun(ptr pData)
{
	testprotocolthreadcontext* pContext = (testprotocolthreadcontext*)pData;

	if ( pContext->Mode == TEST_TYPE_ADD ) {
		for ( size_t i = 0; i < pContext->WitnessCount; i++ ) {
			if ( !xrtProtocolRegistryAdd(
				pContext->Registry, &pContext->Witnesses[i]
			) ) {
				return 1;
			}
		}
		return 0;
	}
	if ( pContext->Mode == TEST_TYPE_FIND ) {
		for ( size_t i = 0; i < pContext->WitnessCount; i++ ) {
			const xrtprotocolwitness* pWitness = &pContext->Witnesses[i];

			if ( xrtProtocolRegistryFind(
				pContext->Registry,
				pWitness->Protocol->Type->Id,
				pWitness->ConcreteType->Id
			) != pWitness ) {
				return 2;
			}
		}
		return 0;
	}
	for ( size_t i = pContext->ThreadIndex;
		  i < pContext->WitnessCount; i += pContext->ThreadCount ) {
		if ( !xrtProtocolRegistryRemove(
			pContext->Registry, &pContext->Witnesses[i]
		) ) {
			return 3;
		}
	}
	return 0;
}



/* 启动一个完整阶段并检查每个工作线程结果。 */
static void testTypeThreadStage(xrttyperegistry* pRegistry,
	const xrttype* pTypes, testtypethreadmode Mode)
{
	testtypethreadcontext arrContext[TEST_TYPE_THREAD_COUNT];
	testthread arrThread[TEST_TYPE_THREAD_COUNT];

	for ( size_t i = 0; i < TEST_TYPE_THREAD_COUNT; i++ ) {
		arrContext[i].Registry = pRegistry;
		arrContext[i].Types = pTypes;
		arrContext[i].TypeCount = TEST_TYPE_DESCRIPTOR_COUNT;
		arrContext[i].ThreadIndex = i;
		arrContext[i].ThreadCount = TEST_TYPE_THREAD_COUNT;
		arrContext[i].Mode = Mode;
		arrThread[i].Proc = testTypeThreadRun;
		arrThread[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThread, TEST_TYPE_THREAD_COUNT);
	testThreadsJoin(arrThread, TEST_TYPE_THREAD_COUNT);
	for ( size_t i = 0; i < TEST_TYPE_THREAD_COUNT; i++ ) {
		testRequire(arrThread[i].Result == 0, "runtime type registry thread failed");
	}
}



/* 启动一个协议注册表阶段并检查每个工作线程结果。 */
static void testProtocolThreadStage(
	xrtprotocolregistry* pRegistry,
	const xrtprotocolwitness* pWitnesses,
	testtypethreadmode Mode
)
{
	testprotocolthreadcontext arrContext[TEST_TYPE_THREAD_COUNT];
	testthread arrThread[TEST_TYPE_THREAD_COUNT];

	for ( size_t i = 0; i < TEST_TYPE_THREAD_COUNT; i++ ) {
		arrContext[i].Registry = pRegistry;
		arrContext[i].Witnesses = pWitnesses;
		arrContext[i].WitnessCount = TEST_TYPE_DESCRIPTOR_COUNT;
		arrContext[i].ThreadIndex = i;
		arrContext[i].ThreadCount = TEST_TYPE_THREAD_COUNT;
		arrContext[i].Mode = Mode;
		arrThread[i].Proc = testProtocolThreadRun;
		arrThread[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThread, TEST_TYPE_THREAD_COUNT);
	testThreadsJoin(arrThread, TEST_TYPE_THREAD_COUNT);
	for ( size_t i = 0; i < TEST_TYPE_THREAD_COUNT; i++ ) {
		testRequire(
			arrThread[i].Result == 0,
			"runtime protocol registry thread failed"
		);
	}
}



/* 验证类型和协议注册表在并发幂等注册、查询和移除下保持完整。 */
int main(void)
{
	char arrName[TEST_TYPE_DESCRIPTOR_COUNT][TEST_TYPE_NAME_SIZE];
	xrttype arrType[TEST_TYPE_DESCRIPTOR_COUNT];
	xrttyperegistry* pRegistry = xrtTypeRegistryCreate();
	xrttype ProtocolType;
	xrtprotocol Protocol;
	xrtprotocolwitness arrWitness[TEST_TYPE_DESCRIPTOR_COUNT];
	xrtprotocolregistry* pProtocolRegistry;

	testRequire(pRegistry != NULL, "threaded type registry create failed");
	for ( size_t i = 0; i < TEST_TYPE_DESCRIPTOR_COUNT; i++ ) {
		int iLength = snprintf(
			arrName[i], TEST_TYPE_NAME_SIZE,
			"tests.runtime.ConcurrentType%03u", (unsigned)i
		);

		testRequire(
			(iLength > 0) && ((size_t)iLength < TEST_TYPE_NAME_SIZE),
			"threaded type name formatting failed"
		);
		memset(&arrType[i], 0, sizeof(arrType[i]));
		arrType[i].Id = xrtTypeId((xstrview){ arrName[i], (size_t)iLength });
		arrType[i].Kind = XRT_TYPE_CLASS;
		arrType[i].Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE;
		arrType[i].Name = (xstrview){ arrName[i], (size_t)iLength };
		arrType[i].AbiName = (xstrview){ arrName[i], (size_t)iLength };
		arrType[i].Size = sizeof(ptr);
		arrType[i].Align = TEST_ALIGNOF(ptr);
		arrType[i].InstanceSize = sizeof(int64);
		arrType[i].InstanceAlign = TEST_ALIGNOF(int64);
	}

	testTypeThreadStage(pRegistry, arrType, TEST_TYPE_ADD);
	testRequire(
		xrtTypeRegistryCount(pRegistry) == TEST_TYPE_DESCRIPTOR_COUNT,
		"concurrent idempotent add count mismatch"
	);
	testTypeThreadStage(pRegistry, arrType, TEST_TYPE_FIND);
	testTypeThreadStage(pRegistry, arrType, TEST_TYPE_REMOVE);
	testRequire(xrtTypeRegistryCount(pRegistry) == 0u, "concurrent remove count mismatch");
	xrtTypeRegistryDestroy(pRegistry);

	memset(&ProtocolType, 0, sizeof(ProtocolType));
	ProtocolType.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.ConcurrentProtocol"));
	ProtocolType.Kind = XRT_TYPE_PROTOCOL;
	ProtocolType.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE;
	ProtocolType.Name = XRT_STR_LITERAL("ConcurrentProtocol");
	ProtocolType.AbiName = XRT_STR_LITERAL("tests.runtime.ConcurrentProtocol");
	ProtocolType.Size = sizeof(ptr);
	ProtocolType.Align = TEST_ALIGNOF(ptr);
	ProtocolType.InstanceSize = sizeof(ptr);
	ProtocolType.InstanceAlign = TEST_ALIGNOF(ptr);
	Protocol.Type = &ProtocolType;
	Protocol.RequirementCount = 0u;
	Protocol.Requirements = NULL;
	testRequire(xrtProtocolValidate(&Protocol), "concurrent protocol validation failed");

	for ( size_t i = 0; i < TEST_TYPE_DESCRIPTOR_COUNT; i++ ) {
		arrWitness[i].Protocol = &Protocol;
		arrWitness[i].ConcreteType = &arrType[i];
		arrWitness[i].EntryCount = 0u;
		arrWitness[i].Entries = NULL;
	}
	pProtocolRegistry = xrtProtocolRegistryCreate();
	testRequire(pProtocolRegistry != NULL, "threaded protocol registry create failed");
	testProtocolThreadStage(pProtocolRegistry, arrWitness, TEST_TYPE_ADD);
	testRequire(
		xrtProtocolRegistryCount(pProtocolRegistry) == TEST_TYPE_DESCRIPTOR_COUNT,
		"concurrent protocol add count mismatch"
	);
	testProtocolThreadStage(pProtocolRegistry, arrWitness, TEST_TYPE_FIND);
	testProtocolThreadStage(pProtocolRegistry, arrWitness, TEST_TYPE_REMOVE);
	testRequire(
		xrtProtocolRegistryCount(pProtocolRegistry) == 0u,
		"concurrent protocol remove count mismatch"
	);
	xrtProtocolRegistryDestroy(pProtocolRegistry);
	printf("[PASS] runtime type threads\n");
	return 0;
}
