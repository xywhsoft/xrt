#include "../test.h"
#include "../test_thread.h"



#define TEST_FIELD_THREAD_COUNT 8u
#define TEST_FIELD_THREAD_ROUNDS 20000u



typedef struct testfieldthreadvalue {
	int64 Id;
	uint64 Version;
} testfieldthreadvalue;



typedef struct testfieldthreadcontext {
	const xrttype* Type;
	const xrtfielddesc* Id;
	const xrtfielddesc* Version;
	const testfieldthreadvalue* Value;
} testfieldthreadcontext;



/* 重复执行不可变字段表的计数、名称、声明者和地址查询。 */
static int testFieldThreadRun(ptr pData)
{
	const testfieldthreadcontext* pContext =
		(const testfieldthreadcontext*)pData;

	for ( size_t i = 0; i < TEST_FIELD_THREAD_ROUNDS; i++ ) {
		if (
			(xrtTypeFieldCount(pContext->Type) != 2u) ||
			(xrtTypeField(pContext->Type, 0u) != pContext->Id) ||
			(xrtTypeFindField(
				pContext->Type, XRT_STR_LITERAL("version")
			) != pContext->Version) ||
			(xrtTypeFieldOwner(
				pContext->Type, pContext->Version
			) != pContext->Type) ||
			(*(const int64*)xrtFieldConstData(
				pContext->Type, pContext->Id, pContext->Value
			) != 71) ||
			(*(const uint64*)xrtFieldConstData(
				pContext->Type, pContext->Version, pContext->Value
			) != 19u)
		) {
			return 1;
		}
	}
	return 0;
}



/* 验证同一份已发布字段元数据可以被多线程无锁查询。 */
int main(void)
{
	xrtfielddesc arrFields[] = {
		{
			XRT_STR_INIT("id"), xrtTypeInt64(),
			offsetof(testfieldthreadvalue, Id), 0u
		},
		{
			XRT_STR_INIT("version"), xrtTypeUInt64(),
			offsetof(testfieldthreadvalue, Version), XRT_FIELD_FLAG_READONLY
		}
	};
	xrtfieldtable Fields = { 2u, arrFields };
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.field.Concurrent")),
		.Kind = XRT_TYPE_RECORD,
		.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY |
			XRT_TYPE_FLAG_TRIVIAL_DROP | XRT_TYPE_FLAG_COPYABLE,
		.Name = XRT_STR_INIT("Concurrent"),
		.AbiName = XRT_STR_INIT("tests.field.Concurrent"),
		.Size = sizeof(testfieldthreadvalue),
		.Align = TEST_ALIGNOF(testfieldthreadvalue),
		.InstanceSize = sizeof(testfieldthreadvalue),
		.InstanceAlign = TEST_ALIGNOF(testfieldthreadvalue),
		.Fields = &Fields
	};
	testfieldthreadvalue Value = { 71, 19u };
	testfieldthreadcontext Context = {
		&Type, &arrFields[0], &arrFields[1], &Value
	};
	testthread arrThreads[TEST_FIELD_THREAD_COUNT];

	testRequire(xrtTypeFieldsValidate(&Type), "thread field table validation failed");
	for ( size_t i = 0; i < TEST_FIELD_THREAD_COUNT; i++ ) {
		arrThreads[i].Proc = testFieldThreadRun;
		arrThreads[i].Data = &Context;
	}
	testThreadsStart(arrThreads, TEST_FIELD_THREAD_COUNT);
	testThreadsJoin(arrThreads, TEST_FIELD_THREAD_COUNT);
	for ( size_t i = 0; i < TEST_FIELD_THREAD_COUNT; i++ ) {
		testRequire(arrThreads[i].Result == 0, "concurrent field lookup failed");
	}
	printf("[PASS] runtime field threads\n");
	return 0;
}
