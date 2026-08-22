#include "../test.h"

#include <xrt/http_vary.h>



/* 验证重复字段的计划、游标、查找和原值组合。 */
static void testHttpVaryPlan(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Host"),
			XRT_STR_INIT("example.test")
		},
		{
			XRT_STR_INIT("Vary"),
			XRT_STR_INIT("Accept-Encoding, User-Agent")
		},
		{
			XRT_STR_INIT("vary"),
			XRT_STR_INIT("")
		},
		{
			XRT_STR_INIT("VARY"),
			XRT_STR_INIT("accept-encoding, *")
		}
	};
	static const xstrview Expected[] = {
		XRT_STR_INIT("Accept-Encoding"),
		XRT_STR_INIT("User-Agent"),
		XRT_STR_INIT("accept-encoding"),
		XRT_STR_INIT("*")
	};
	static const char Joined[] =
		"Accept-Encoding, User-Agent, , accept-encoding, *";
	xhttpvaryplan Plan;
	xhttpvarycursor Cursor;
	xhttpvaryitem Item;
	xhttpnext Next;
	char Output[sizeof(Joined)];
	size_t iSize = 0;
	size_t i = 0;

	testRequire(
		xrtHttpVaryPlan(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&Plan
		) &&
		(Plan.FieldCount == 3) &&
		(Plan.ItemCount == 4) &&
		(Plan.NameCount == 3) &&
		(Plan.EmptyFieldCount == 1) &&
		(Plan.JoinedSize == (sizeof(Joined) - 1u)) &&
		(Plan.Flags == (
			XHTTP_VARY_PRESENT |
			XHTTP_VARY_NAMES |
			XHTTP_VARY_WILDCARD |
			XHTTP_VARY_MIXED |
			XHTTP_VARY_EMPTY
		 )),
		"Vary plan facts mismatch"
	);
	xrtHttpVaryCursorInit(&Cursor);
	while ( (Next = xrtHttpVaryNext(
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		&Cursor,
		&Item
	)) == XHTTP_NEXT_ITEM ) {
		testRequire(
			(i < (sizeof(Expected) / sizeof(Expected[0]))) &&
			xrtHttpTokenEqual(Item.Name, Expected[i]) &&
			(Item.Wildcard == (i == 3)),
			"Vary cursor order mismatch"
		);
		i++;
	}
	testRequire(
		(Next == XHTTP_NEXT_END) &&
		(i == (sizeof(Expected) / sizeof(Expected[0]))),
		"Vary cursor did not finish"
	);
	testRequire(
		(xrtHttpVaryFind(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			XRT_STR_LITERAL("USER-AGENT"),
			&Item
		 ) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Item.Name, XRT_STR_LITERAL("User-Agent")
		) &&
		!Item.Wildcard,
		"Vary case-insensitive lookup mismatch"
	);
	testRequire(
		(xrtHttpVaryFind(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			XRT_STR_LITERAL("Origin"),
			&Item
		 ) == XHTTP_NEXT_END) &&
		(Item.Name.Size == 0) &&
		!Item.Wildcard,
		"Vary missing lookup mismatch"
	);
	testRequire(
		xrtHttpVaryWrite(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			Output,
			sizeof(Output),
			&iSize
		) &&
		(iSize == (sizeof(Joined) - 1u)) &&
		(memcmp(Output, Joined, iSize) == 0),
		"Vary joined value mismatch"
	);
}



/* 验证缺失、空字段、畸形后缀和失败原子性。 */
static void testHttpVaryEdges(void)
{
	char AliasValue[] = "Accept-Encoding";
	xhttpfield Alias = {
		XRT_STR_LITERAL("Vary"),
		{ AliasValue, sizeof(AliasValue) - 1u }
	};
	static const xhttpfield Empty[] = {
		{
			XRT_STR_INIT("Vary"),
			XRT_STR_INIT("")
		}
	};
	static const xhttpfield Invalid[] = {
		{
			XRT_STR_INIT("Vary"),
			XRT_STR_INIT("Accept-Encoding")
		},
		{
			XRT_STR_INIT("vary"),
			XRT_STR_INIT("User-Agent;bad")
		}
	};
	xhttpvaryplan Plan;
	xhttpvaryplan Before;
	xhttpvaryitem Item;
	char Output[8];
	size_t iSize = 0;

	testRequire(
		xrtHttpVaryPlan(NULL, 0, &Plan) &&
		(Plan.Flags == XHTTP_VARY_NONE) &&
		(Plan.FieldCount == 0) &&
		(Plan.ItemCount == 0) &&
		(Plan.JoinedSize == 0),
		"absent Vary plan mismatch"
	);
	testRequire(
		xrtHttpVaryPlan(Empty, 1, &Plan) &&
		(Plan.Flags == (
			XHTTP_VARY_PRESENT |
			XHTTP_VARY_EMPTY
		 )) &&
		(Plan.FieldCount == 1) &&
		(Plan.EmptyFieldCount == 1) &&
		(Plan.ItemCount == 0) &&
		(Plan.JoinedSize == 0) &&
		xrtHttpVaryWrite(
			Empty, 1, NULL, 0, &iSize
		) &&
		(iSize == 0),
		"empty Vary field mismatch"
	);
	Before = Plan;
	testRequire(
		!xrtHttpVaryPlan(Invalid, 2, &Plan) &&
		(memcmp(&Plan, &Before, sizeof(Plan)) == 0),
		"malformed Vary changed prior plan"
	);
	xrtClearError();
	memset(&Item, 0xff, sizeof(Item));
	testRequire(
		(xrtHttpVaryFind(
			Invalid,
			2,
			XRT_STR_LITERAL("Accept-Encoding"),
			&Item
		 ) == XHTTP_NEXT_ERROR) &&
		(Item.Name.Size == 0) &&
		!Item.Wildcard,
		"Vary lookup accepted malformed tail"
	);
	xrtClearError();
	memset(Output, 'x', sizeof(Output));
	testRequire(
		!xrtHttpVaryWrite(
			&Alias,
			1,
			Output,
			sizeof(Output),
			&iSize
		) &&
		(iSize == (sizeof(AliasValue) - 1u)) &&
		(Output[0] == 'x'),
		"Vary short output was not failure atomic"
	);
	xrtClearError();
	testRequire(
		!xrtHttpVaryWrite(
			&Alias,
			1,
			AliasValue,
			sizeof(AliasValue) - 1u,
			&iSize
		) &&
		(iSize == (sizeof(AliasValue) - 1u)) &&
		(memcmp(
			AliasValue,
			"Accept-Encoding",
			sizeof(AliasValue) - 1u
		) == 0),
		"Vary write accepted overlapping input"
	);
	xrtClearError();
	testRequire(
		xrtHttpVaryFind(
			&Alias,
			1,
			XRT_STR_LITERAL("*"),
			&Item
		) == XHTTP_NEXT_ERROR,
		"Vary lookup accepted wildcard as a field name"
	);
	xrtClearError();
}



/* 验证 Vary 固定描述符支持未对齐存储并拒绝地址回绕。 */
static void testHttpVaryMemoryContracts(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Vary"), XRT_STR_INIT("Accept-Encoding") },
		{ XRT_STR_INIT("vary"), XRT_STR_INIT("Origin") }
	};
	static const char Expected[] = "Accept-Encoding, Origin";
	uint8 FieldsStorage[sizeof(Fields) + 2u];
	uint8 CursorStorage[sizeof(xhttpvarycursor) + 2u];
	uint8 ItemStorage[sizeof(xhttpvaryitem) + 2u];
	uint8 PlanStorage[sizeof(xhttpvaryplan) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	const xhttpfield* pFields = (const xhttpfield*)(const void*)(
		FieldsStorage + 1u
	);
	xhttpvarycursor Cursor;
	xhttpvaryitem Item;
	xhttpvaryplan Plan;
	size_t iSize;
	char Output[64];

	memset(FieldsStorage, 0xA5, sizeof(FieldsStorage));
	memset(CursorStorage, 0xA5, sizeof(CursorStorage));
	memset(ItemStorage, 0xA5, sizeof(ItemStorage));
	memset(PlanStorage, 0xA5, sizeof(PlanStorage));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memcpy(FieldsStorage + 1u, Fields, sizeof(Fields));
	xrtHttpVaryCursorInit(
		(xhttpvarycursor*)(void*)(CursorStorage + 1u)
	);
	testRequire(xrtHttpVaryNext(
		pFields,
		2u,
		(xhttpvarycursor*)(void*)(CursorStorage + 1u),
		(xhttpvaryitem*)(void*)(ItemStorage + 1u)
	) == XHTTP_NEXT_ITEM,
		"HTTP Vary iterator rejected unaligned descriptors");
	memcpy(&Cursor, CursorStorage + 1u, sizeof(Cursor));
	memcpy(&Item, ItemStorage + 1u, sizeof(Item));
	testRequire((Cursor.Field == 0) && xrtHttpTokenEqual(
		Item.Name, XRT_STR_LITERAL("Accept-Encoding")
	), "HTTP Vary iterator published invalid unaligned state");
	testRequire(xrtHttpVaryPlan(
		pFields,
		2u,
		(xhttpvaryplan*)(void*)(PlanStorage + 1u)
	), "HTTP Vary plan rejected an unaligned output");
	memcpy(&Plan, PlanStorage + 1u, sizeof(Plan));
	testRequire((Plan.FieldCount == 2u) &&
		(Plan.ItemCount == 2u) &&
		(Plan.JoinedSize == (sizeof(Expected) - 1u)),
		"HTTP Vary plan published invalid unaligned facts");
	testRequire(xrtHttpVaryFind(
		pFields,
		2u,
		XRT_STR_LITERAL("origin"),
		(xhttpvaryitem*)(void*)(ItemStorage + 1u)
	) == XHTTP_NEXT_ITEM,
		"HTTP Vary find rejected an unaligned output");
	memcpy(&Item, ItemStorage + 1u, sizeof(Item));
	testRequire(xrtHttpTokenEqual(
		Item.Name, XRT_STR_LITERAL("Origin")
	), "HTTP Vary find published the wrong item");
	testRequire(xrtHttpVaryWrite(
		pFields,
		2u,
		NULL,
		0,
		(size_t*)(void*)(SizeStorage + 1u)
	), "HTTP Vary size query did not succeed");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((iSize == (sizeof(Expected) - 1u)) &&
		xrtHttpVaryWrite(
			pFields,
			2u,
			Output,
			sizeof(Output),
			(size_t*)(void*)(SizeStorage + 1u)
		) && (memcmp(Output, Expected, iSize) == 0),
		"HTTP Vary writer rejected unaligned descriptors"
	);
	testRequire(
		(FieldsStorage[0] == 0xA5) &&
		(FieldsStorage[sizeof(FieldsStorage) - 1u] == 0xA5) &&
		(CursorStorage[0] == 0xA5) &&
		(CursorStorage[sizeof(CursorStorage) - 1u] == 0xA5) &&
		(ItemStorage[0] == 0xA5) &&
		(ItemStorage[sizeof(ItemStorage) - 1u] == 0xA5) &&
		(PlanStorage[0] == 0xA5) &&
		(PlanStorage[sizeof(PlanStorage) - 1u] == 0xA5) &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"HTTP Vary wrote outside unaligned storage"
	);

	xrtHttpVaryCursorInit(
		(xhttpvarycursor*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP Vary cursor init accepted a wrapping output");
	xrtClearError();
	testRequire(xrtHttpVaryNext(
		(const xhttpfield*)(uintptr_t)(UINTPTR_MAX - 1u),
		1u,
		&Cursor,
		&Item
	) == XHTTP_NEXT_ERROR,
		"HTTP Vary iterator accepted a wrapping field array");
	xrtClearError();
	testRequire(!xrtHttpVaryPlan(
		Fields,
		2u,
		(xhttpvaryplan*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP Vary plan accepted a wrapping output");
	xrtClearError();
	testRequire(xrtHttpVaryFind(
		Fields,
		2u,
		XRT_STR_LITERAL("Origin"),
		(xhttpvaryitem*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == XHTTP_NEXT_ERROR,
		"HTTP Vary find accepted a wrapping output");
	xrtClearError();
	testRequire(!xrtHttpVaryWrite(
		Fields,
		2u,
		Output,
		sizeof(Output),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP Vary writer accepted a wrapping size output");
	xrtClearError();
}



/* 运行 Vary 协议事实与安全边界测试。 */
int main(void)
{
	testHttpVaryPlan();
	testHttpVaryEdges();
	testHttpVaryMemoryContracts();
	printf("[PASS] http_vary\n");
	return 0;
}
