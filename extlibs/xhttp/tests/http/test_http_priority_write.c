#include "../test.h"

#include <xrt/http_priority.h>



/* 写出 Priority 并按精确字节比较结果。 */
static void testPriorityWriteEqual(
	const xhttppriority* pPriority,
	xstrview Expected
)
{
	char arrOutput[64];
	size_t iSize;

	testRequire(
		xrtHttpPriorityWrite(
			pPriority, NULL, 0, &iSize
		) && (iSize == Expected.Size),
		"Priority writer length mismatch"
	);
	memset(arrOutput, 0xA5, sizeof(arrOutput));
	testRequire(
		xrtHttpPriorityWrite(
			pPriority, arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == Expected.Size) &&
		((iSize == 0) ||
		 (memcmp(arrOutput, Expected.Data, iSize) == 0)),
		"Priority writer bytes mismatch"
	);
}



/* 验证省略参数、显式默认值和规范成员顺序。 */
static void testPriorityWriteCanonical(void)
{
	xhttppriority Priority;

	xrtHttpPriorityInit(&Priority);
	testPriorityWriteEqual(&Priority, XRT_STR_LITERAL(""));

	Priority.Urgency = 1u;
	Priority.Incremental = 1u;
	Priority.Flags = XHTTP_PRIORITY_HAS_URGENCY |
		XHTTP_PRIORITY_HAS_INCREMENTAL;
	testPriorityWriteEqual(
		&Priority, XRT_STR_LITERAL("u=1, i")
	);

	Priority.Urgency = XHTTP_PRIORITY_URGENCY_DEFAULT;
	Priority.Incremental = 0;
	testPriorityWriteEqual(
		&Priority, XRT_STR_LITERAL("u=3, i=?0")
	);

	Priority.Flags = XHTTP_PRIORITY_HAS_INCREMENTAL;
	Priority.Incremental = 1u;
	testPriorityWriteEqual(&Priority, XRT_STR_LITERAL("i"));
}



/* 验证写出结果可由同一协议层无损解析。 */
static void testPriorityWriteRoundTrip(void)
{
	xhttppriority Input = {
		6u, 0, XHTTP_PRIORITY_HAS_URGENCY |
		XHTTP_PRIORITY_HAS_INCREMENTAL
	};
	xhttppriority Output;
	char arrValue[64];
	size_t iSize;

	testRequire(
		xrtHttpPriorityWrite(
			&Input, arrValue, sizeof(arrValue), &iSize
		) && xrtHttpPriorityValueParse(
			(xstrview){ arrValue, iSize }, &Output
		) && (memcmp(&Input, &Output, sizeof(Input)) == 0),
		"Priority writer round trip mismatch"
	);
}



/* 验证容量、非法描述符和输入别名边界。 */
static void testPriorityWriteMemory(void)
{
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttppriority) + 1u];
	} Storage;
	xhttppriority* pPriority =
		(xhttppriority*)(Storage.Bytes + 1u);
	xhttppriority Priority = {
		2u, 1u, XHTTP_PRIORITY_HAS_URGENCY |
		XHTTP_PRIORITY_HAS_INCREMENTAL
	};
	char arrOutput[16];
	size_t iSize;

	memcpy(pPriority, &Priority, sizeof(Priority));
	memset(arrOutput, 0xA5, sizeof(arrOutput));
	testRequire(
		!xrtHttpPriorityWrite(
			pPriority, arrOutput, 2, &iSize
		) && (iSize == 6u) &&
		((uint8)arrOutput[0] == 0xA5u),
		"Priority writer capacity failure was not atomic"
	);
	xrtClearError();
	testRequire(
		xrtHttpPriorityWrite(
			pPriority, arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == 6u),
		"Priority writer rejected unaligned descriptor"
	);
	Priority.Urgency = 8u;
	memset(arrOutput, 0xA5, sizeof(arrOutput));
	testRequire(
		!xrtHttpPriorityWrite(
			&Priority, arrOutput, sizeof(arrOutput), &iSize
		) && ((uint8)arrOutput[0] == 0xA5u),
		"Priority writer accepted invalid urgency"
	);
	xrtClearError();
}



/* 运行 Priority 规范序列化测试。 */
int main(void)
{
	testPriorityWriteCanonical();
	testPriorityWriteRoundTrip();
	testPriorityWriteMemory();
	printf("[PASS] http_priority_write\n");
	return 0;
}
