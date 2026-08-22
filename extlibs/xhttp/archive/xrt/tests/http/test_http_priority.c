#include "../test.h"

#include <xrt/http_priority.h>



/* 验证默认值与显式参数标志相互独立。 */
static void testPriorityDefaults(void)
{
	static const xhttpfield Missing[] = {
		{ XRT_STR_INIT("Other"), XRT_STR_INIT("value") }
	};
	xhttppriority Priority;

	memset(&Priority, 0xA5, sizeof(Priority));
	xrtHttpPriorityInit(&Priority);
	testRequire(
		(Priority.Urgency == XHTTP_PRIORITY_URGENCY_DEFAULT) &&
		(Priority.Incremental == 0) && (Priority.Flags == 0),
		"Priority defaults mismatch"
	);
	testRequire(
		xrtHttpPriorityValueParse(
			XRT_STR_LITERAL(""), &Priority
		) && (Priority.Urgency ==
			XHTTP_PRIORITY_URGENCY_DEFAULT) &&
		(Priority.Flags == 0),
		"empty Priority Dictionary mismatch"
	);
	testRequire(
		xrtHttpPriorityParse(Missing, 1, &Priority) &&
		(Priority.Urgency == XHTTP_PRIORITY_URGENCY_DEFAULT) &&
		(Priority.Flags == 0),
		"missing Priority field mismatch"
	);
}



/* 验证已知参数、未知扩展和 Item 参数的处理。 */
static void testPriorityKnownValues(void)
{
	xhttppriority Priority;

	testRequire(
		xrtHttpPriorityValueParse(
			XRT_STR_LITERAL("u=1;source=edge, i;chunk, x=token"),
			&Priority
		) && (Priority.Urgency == 1u) &&
		(Priority.Incremental == 1u) &&
		(Priority.Flags ==
			(XHTTP_PRIORITY_HAS_URGENCY |
			 XHTTP_PRIORITY_HAS_INCREMENTAL)),
		"Priority known values mismatch"
	);
	testRequire(
		xrtHttpPriorityValueParse(
			XRT_STR_LITERAL("u=8, i=1, future=7"),
			&Priority
		) && (Priority.Urgency ==
			XHTTP_PRIORITY_URGENCY_DEFAULT) &&
		(Priority.Incremental == 0) && (Priority.Flags == 0),
		"Priority invalid known values were not ignored"
	);
	testRequire(
		xrtHttpPriorityValueParse(
			XRT_STR_LITERAL("u=(1), i=?0"), &Priority
		) && (Priority.Urgency ==
			XHTTP_PRIORITY_URGENCY_DEFAULT) &&
		(Priority.Incremental == 0) &&
		(Priority.Flags == XHTTP_PRIORITY_HAS_INCREMENTAL),
		"Priority type handling mismatch"
	);
}



/* 验证重复 key 使用最后一次线路成员，包括无效末值。 */
static void testPriorityDuplicates(void)
{
	xhttppriority Priority;

	testRequire(
		xrtHttpPriorityValueParse(
			XRT_STR_LITERAL("u=9, u=2, i=token, i"),
			&Priority
		) && (Priority.Urgency == 2u) &&
		(Priority.Incremental == 1u) &&
		(Priority.Flags ==
			(XHTTP_PRIORITY_HAS_URGENCY |
			 XHTTP_PRIORITY_HAS_INCREMENTAL)),
		"Priority valid final duplicate mismatch"
	);
	testRequire(
		xrtHttpPriorityValueParse(
			XRT_STR_LITERAL("u=1, u=9, i, i=token"),
			&Priority
		) && (Priority.Urgency ==
			XHTTP_PRIORITY_URGENCY_DEFAULT) &&
		(Priority.Incremental == 0) && (Priority.Flags == 0),
		"Priority invalid final duplicate mismatch"
	);
}



/* 验证重复字段行按一个 Structured Dictionary 组合。 */
static void testPriorityFields(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Priority"), XRT_STR_INIT("u=7, i=?0") },
		{ XRT_STR_INIT("Other"), XRT_STR_INIT("ignored") },
		{ XRT_STR_INIT("priority"), XRT_STR_INIT("u=1, i") }
	};
	static const xhttpfield InvalidLast[] = {
		{ XRT_STR_INIT("Priority"), XRT_STR_INIT("u=1, i") },
		{ XRT_STR_INIT("priority"), XRT_STR_INIT("u=8, i=token") }
	};
	xhttppriority Priority;

	testRequire(
		xrtHttpPriorityParse(Fields, 3, &Priority) &&
		(Priority.Urgency == 1u) &&
		(Priority.Incremental == 1u) &&
		(Priority.Flags ==
			(XHTTP_PRIORITY_HAS_URGENCY |
			 XHTTP_PRIORITY_HAS_INCREMENTAL)),
		"repeated Priority fields mismatch"
	);
	testRequire(
		xrtHttpPriorityParse(InvalidLast, 2, &Priority) &&
		(Priority.Urgency == XHTTP_PRIORITY_URGENCY_DEFAULT) &&
		(Priority.Incremental == 0) && (Priority.Flags == 0),
		"repeated Priority invalid final value mismatch"
	);
}



/* 验证响应或后续信号只覆盖显式参数。 */
static void testPriorityOverlay(void)
{
	xhttppriority Base;
	xhttppriority Update;
	xhttppriority Result;

	testRequire(
		xrtHttpPriorityValueParse(
			XRT_STR_LITERAL("u=5, i"), &Base
		) && xrtHttpPriorityValueParse(
			XRT_STR_LITERAL("u=1"), &Update
		) && xrtHttpPriorityOverlay(
			&Base, &Update, &Result
		),
		"Priority overlay failed"
	);
	testRequire(
		(Result.Urgency == 1u) &&
		(Result.Incremental == 1u) &&
		(Result.Flags ==
			(XHTTP_PRIORITY_HAS_URGENCY |
			 XHTTP_PRIORITY_HAS_INCREMENTAL)),
		"Priority overlay changed omitted parameter"
	);
	testRequire(
		xrtHttpPriorityValueParse(
			XRT_STR_LITERAL("i=?0"), &Update
		) && xrtHttpPriorityOverlay(
			&Result, &Update, &Result
		) && (Result.Urgency == 1u) &&
		(Result.Incremental == 0),
		"Priority overlay alias or explicit false mismatch"
	);
}



/* 验证畸形字段失败时不发布部分结果，并支持未对齐输出。 */
static void testPriorityFailureAndMemory(void)
{
	static const xhttpfield Invalid[] = {
		{ XRT_STR_INIT("Priority"), XRT_STR_INIT("u=1") },
		{ XRT_STR_INIT("priority"), XRT_STR_INIT("") }
	};
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttppriority) + 1u];
	} Storage;
	xhttppriority* pPriority =
		(xhttppriority*)(Storage.Bytes + 1u);
	xhttppriority Saved = { 7u, 1u, 3u };
	xhttppriority Loaded;

	memcpy(pPriority, &Saved, sizeof(Saved));
	testRequire(
		!xrtHttpPriorityValueParse(
			XRT_STR_LITERAL("u=1,"), pPriority
		),
		"malformed Priority value was accepted"
	);
	memcpy(&Loaded, pPriority, sizeof(Loaded));
	testRequire(
		memcmp(&Loaded, &Saved, sizeof(Saved)) == 0,
		"Priority value failure published partial output"
	);
	xrtClearError();
	testRequire(
		!xrtHttpPriorityParse(Invalid, 2, pPriority),
		"malformed repeated Priority field was accepted"
	);
	memcpy(&Loaded, pPriority, sizeof(Loaded));
	testRequire(
		memcmp(&Loaded, &Saved, sizeof(Saved)) == 0,
		"Priority field failure published partial output"
	);
	xrtClearError();
	testRequire(
		xrtHttpPriorityValueParse(
			XRT_STR_LITERAL("u=0, i"), pPriority
		),
		"Priority rejected unaligned output"
	);
	memcpy(&Loaded, pPriority, sizeof(Loaded));
	testRequire(
		(Loaded.Urgency == 0) && (Loaded.Incremental == 1u),
		"Priority unaligned output mismatch"
	);
}



/* 运行 RFC 9218 Priority 解析与组合测试。 */
int main(void)
{
	testPriorityDefaults();
	testPriorityKnownValues();
	testPriorityDuplicates();
	testPriorityFields();
	testPriorityOverlay();
	testPriorityFailureAndMemory();
	printf("[PASS] http_priority\n");
	return 0;
}
