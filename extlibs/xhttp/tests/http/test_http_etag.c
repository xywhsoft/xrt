#include "../test.h"



/* 比较借用字符串视图和固定文本。 */
static bool testHttpETagTextEqual(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 验证强、弱、空正文和 obs-text 标签语法。 */
static void testHttpETagParse(void)
{
	static const char ObsTag[] = {
		'"', (char)0x80, (char)0xFF, '"'
	};
	static const xstrview Invalid[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("*"),
		XRT_STR_INIT("w/\"tag\""),
		XRT_STR_INIT("W /\"tag\""),
		XRT_STR_INIT("W/ \"tag\""),
		XRT_STR_INIT(" \"tag\""),
		XRT_STR_INIT("\"tag\" "),
		XRT_STR_INIT("\"a\"b\""),
		XRT_STR_INIT("\"a b\"")
	};
	xhttpetag Tag;
	size_t i;

	testRequire(xrtHttpETagParse(
		XRT_STR_LITERAL("\"strong\""), &Tag
	) && !Tag.Weak &&
		testHttpETagTextEqual(Tag.Opaque, "strong"),
		"HTTP entity-tag strong parse failed");
	testRequire(xrtHttpETagParse(
		XRT_STR_LITERAL("W/\"weak\""), &Tag
	) && Tag.Weak &&
		testHttpETagTextEqual(Tag.Opaque, "weak"),
		"HTTP entity-tag weak parse failed");
	testRequire(xrtHttpETagParse(
		XRT_STR_LITERAL("\"\""), &Tag
	) && !Tag.Weak && (Tag.Opaque.Size == 0),
		"HTTP entity-tag empty opaque value failed");
	testRequire(xrtHttpETagParse(
		(xstrview){ ObsTag, sizeof(ObsTag) }, &Tag
	) && (Tag.Opaque.Size == 2),
		"HTTP entity-tag rejected obs-text");

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpETagParse(
			Invalid[i], &Tag
		), "HTTP entity-tag accepted malformed syntax");
		xrtClearError();
	}
}



/* 验证实体标签列表、空列表项和星号的完整语法。 */
static void testHttpETagList(void)
{
	xstrview List = XRT_STR_LITERAL(
		", W/\"one\" , , \"two\","
	);
	xhttpetagitem Item;
	xhttpetag Current;
	xhttpnext Next;
	size_t iOffset = 0;

	Next = xrtHttpETagNext(List, &iOffset, &Item);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		(Item.Kind == XHTTP_ETAG_VALUE) &&
		Item.Tag.Weak &&
		testHttpETagTextEqual(Item.Tag.Opaque, "one"),
		"HTTP entity-tag list first item mismatch");
	Next = xrtHttpETagNext(List, &iOffset, &Item);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		(Item.Kind == XHTTP_ETAG_VALUE) &&
		!Item.Tag.Weak &&
		testHttpETagTextEqual(Item.Tag.Opaque, "two"),
		"HTTP entity-tag list second item mismatch");
	testRequire(xrtHttpETagNext(
		List, &iOffset, &Item
	) == XHTTP_NEXT_END,
		"HTTP entity-tag list did not end exactly");

	iOffset = 0;
	testRequire(xrtHttpETagNext(
		XRT_STR_LITERAL(" \t* \t"), &iOffset, &Item
	) == XHTTP_NEXT_ITEM &&
		(Item.Kind == XHTTP_ETAG_ANY),
		"HTTP entity-tag wildcard parse failed");
	testRequire(xrtHttpETagParse(
		XRT_STR_LITERAL("\"two\""), &Current
	), "HTTP entity-tag list fixture failed");
	testRequire(xrtHttpETagListStrongHas(List, &Current),
		"HTTP strong entity-tag list missed a match");
	testRequire(xrtHttpETagListWeakHas(
		XRT_STR_LITERAL("W/\"two\""), &Current
	), "HTTP weak entity-tag list missed a weak match");
	testRequire(!xrtHttpETagListStrongHas(
		XRT_STR_LITERAL("W/\"two\""), &Current
	), "HTTP strong entity-tag list accepted a weak tag");
	xrtClearError();
	testRequire(xrtHttpETagListStrongHas(
		XRT_STR_LITERAL("*"), &Current
	), "HTTP entity-tag list wildcard did not match");
	testRequire(!xrtHttpETagListStrongHas(
		XRT_STR_LITERAL("\"two\", broken"), &Current
	) && (xrtGetError() != NULL),
		"HTTP entity-tag list stopped validating after a match");
	xrtClearError();
	testRequire(!xrtHttpETagListWeakHas(
		XRT_STR_LITERAL("*, \"two\""), &Current
	) && (xrtGetError() != NULL),
		"HTTP entity-tag list accepted a mixed wildcard");
	xrtClearError();
	testRequire(!xrtHttpETagListWeakHas(
		XRT_STR_LITERAL(",, \t"), &Current
	) && (xrtGetError() != NULL),
		"HTTP entity-tag list accepted no real items");
	xrtClearError();
}



/* 验证强弱比较矩阵严格遵守 opaque-tag 字节语义。 */
static void testHttpETagCompare(void)
{
	xhttpetag StrongA;
	xhttpetag StrongB;
	xhttpetag WeakA;
	xhttpetag Case;

	testRequire(
		xrtHttpETagParse(XRT_STR_LITERAL("\"same\""), &StrongA) &&
		xrtHttpETagParse(XRT_STR_LITERAL("\"same\""), &StrongB) &&
		xrtHttpETagParse(XRT_STR_LITERAL("W/\"same\""), &WeakA) &&
		xrtHttpETagParse(XRT_STR_LITERAL("\"Same\""), &Case),
		"HTTP entity-tag comparison fixture failed"
	);
	testRequire(xrtHttpETagStrongEqual(&StrongA, &StrongB),
		"HTTP entity-tag strong equality failed");
	testRequire(!xrtHttpETagStrongEqual(&StrongA, &WeakA),
		"HTTP entity-tag strong equality accepted weak input");
	testRequire(xrtHttpETagWeakEqual(&StrongA, &WeakA),
		"HTTP entity-tag weak equality rejected equal opaque tags");
	testRequire(!xrtHttpETagWeakEqual(&StrongA, &Case),
		"HTTP entity-tag comparison ignored byte case");
}



/* 验证无分配写出、容量查询和拥有型便捷构建。 */
static void testHttpETagWrite(void)
{
	xhttpetag Tag = {
		XRT_STR_INIT("asset-42"),
		true
	};
	char Buffer[32];
	str sBuilt;
	size_t iSize = 0;

	testRequire(xrtHttpETagWrite(
		&Tag, NULL, 0, &iSize
	) && (iSize == 12),
		"HTTP entity-tag size query failed");
	testRequire(!xrtHttpETagWrite(
		&Tag, Buffer, 11, &iSize
	) && (iSize == 12),
		"HTTP entity-tag short buffer contract failed");
	xrtClearError();
	testRequire(xrtHttpETagWrite(
		&Tag, Buffer, sizeof(Buffer), &iSize
	) && (iSize == 12) &&
		(memcmp(Buffer, "W/\"asset-42\"", 12) == 0),
		"HTTP entity-tag write failed");
	sBuilt = xrtHttpETagBuild(&Tag, &iSize);
	testRequire((sBuilt != NULL) && (iSize == 12) &&
		(strcmp(sBuilt, "W/\"asset-42\"") == 0),
		"HTTP entity-tag build failed");
	xrtFree(sBuilt);
}



/* 验证解析游标、结构输出和线路字节之间的别名边界。 */
static void testHttpETagAliases(void)
{
	union {
		xhttpetag Tag;
		char Text[64];
	} Parse;
	union {
		xhttpetagitem Item;
		char Text[64];
	} Iterate;
	union {
		size_t Offset;
		xhttpetagitem Item;
	} Outputs;
	xhttpetag Tag;
	char Snapshot[64];
	char Write[32] = "asset";
	size_t iOffset = 0;
	size_t iSize = 77;

	memset(&Parse, 0, sizeof(Parse));
	memcpy(Parse.Text, "\"alias\"", 7);
	memcpy(Snapshot, Parse.Text, sizeof(Snapshot));
	testRequire(!xrtHttpETagParse(
		(xstrview){ Parse.Text, 7 }, &Parse.Tag
	), "HTTP entity-tag parse accepted overlapping output");
	testRequire(memcmp(
		Parse.Text, Snapshot, sizeof(Snapshot)
	) == 0, "HTTP entity-tag alias failure changed input");
	xrtClearError();

	memset(&Iterate, 0, sizeof(Iterate));
	memcpy(Iterate.Text, "\"one\"", 5);
	memcpy(Snapshot, Iterate.Text, sizeof(Snapshot));
	testRequire(xrtHttpETagNext(
		(xstrview){ Iterate.Text, 5 },
		&iOffset,
		&Iterate.Item
	) == XHTTP_NEXT_ERROR,
		"HTTP entity-tag iterator accepted input/output overlap");
	testRequire(memcmp(
		Iterate.Text, Snapshot, sizeof(Snapshot)
	) == 0, "HTTP entity-tag iterator changed aliased input");
	xrtClearError();

	memset(&Outputs, 0, sizeof(Outputs));
	testRequire(xrtHttpETagNext(
		XRT_STR_LITERAL("\"one\""),
		&Outputs.Offset,
		&Outputs.Item
	) == XHTTP_NEXT_ERROR,
		"HTTP entity-tag iterator accepted overlapping outputs");
	xrtClearError();

	Tag = (xhttpetag){
		{ Write, 5 },
		false
	};
	memcpy(Snapshot, Write, sizeof(Write));
	testRequire(!xrtHttpETagWrite(
		&Tag, Write, sizeof(Write), &iSize
	), "HTTP entity-tag writer accepted overlapping input");
	testRequire((iSize == 77) && (memcmp(
		Write, Snapshot, sizeof(Write)
	) == 0), "HTTP entity-tag overlap was not failure-atomic");
	xrtClearError();
}



/* 验证所有固定描述符和标量输出支持未对齐存储并拒绝地址回绕。 */
static void testHttpETagMemory(void)
{
	unsigned char TagStorage[sizeof(xhttpetag) + 2u];
	unsigned char OtherStorage[sizeof(xhttpetag) + 2u];
	unsigned char ItemStorage[sizeof(xhttpetagitem) + 2u];
	unsigned char OffsetStorage[sizeof(size_t) + 2u];
	unsigned char SizeStorage[sizeof(size_t) + 2u];
	xhttpetag Tag;
	xhttpetag Other;
	xhttpetagitem Item;
	char Output[32];
	str sBuilt;
	size_t iOffset = 0;
	size_t iSize = 0;

	memset(TagStorage, 0xA5, sizeof(TagStorage));
	testRequire(
		xrtHttpETagParse(
			XRT_STR_LITERAL("W/\"unaligned\""),
			(xhttpetag*)(void*)(TagStorage + 1u)
		),
		"HTTP entity-tag rejected unaligned parse output"
	);
	memcpy(&Tag, TagStorage + 1u, sizeof(Tag));
	testRequire(
		(TagStorage[0] == 0xA5u) &&
		(TagStorage[sizeof(TagStorage) - 1u] == 0xA5u) &&
		Tag.Weak &&
		testHttpETagTextEqual(Tag.Opaque, "unaligned"),
		"HTTP entity-tag unaligned parse output mismatch"
	);

	Other = Tag;
	memset(OtherStorage, 0x5A, sizeof(OtherStorage));
	memcpy(OtherStorage + 1u, &Other, sizeof(Other));
	testRequire(
		xrtHttpETagStrongEqual(
			(const xhttpetag*)(const void*)(TagStorage + 1u),
			(const xhttpetag*)(const void*)(OtherStorage + 1u)
		) == false,
		"HTTP entity-tag strong compare lost weak semantics"
	);
	testRequire(
		xrtHttpETagWeakEqual(
			(const xhttpetag*)(const void*)(TagStorage + 1u),
			(const xhttpetag*)(const void*)(OtherStorage + 1u)
		) &&
		xrtHttpETagListWeakHas(
			XRT_STR_LITERAL("\"unaligned\""),
			(const xhttpetag*)(const void*)(TagStorage + 1u)
		),
		"HTTP entity-tag rejected unaligned compare input"
	);

	memset(ItemStorage, 0xCC, sizeof(ItemStorage));
	memset(OffsetStorage, 0xDD, sizeof(OffsetStorage));
	memcpy(OffsetStorage + 1u, &iOffset, sizeof(iOffset));
	testRequire(
		xrtHttpETagNext(
			XRT_STR_LITERAL("\"one\""),
			(size_t*)(void*)(OffsetStorage + 1u),
			(xhttpetagitem*)(void*)(ItemStorage + 1u)
		) == XHTTP_NEXT_ITEM,
		"HTTP entity-tag rejected unaligned iterator outputs"
	);
	memcpy(&iOffset, OffsetStorage + 1u, sizeof(iOffset));
	memcpy(&Item, ItemStorage + 1u, sizeof(Item));
	testRequire(
		(iOffset == 5u) &&
		(Item.Kind == XHTTP_ETAG_VALUE) &&
		testHttpETagTextEqual(Item.Tag.Opaque, "one") &&
		(ItemStorage[0] == 0xCCu) &&
		(ItemStorage[sizeof(ItemStorage) - 1u] == 0xCCu) &&
		(OffsetStorage[0] == 0xDDu) &&
		(OffsetStorage[sizeof(OffsetStorage) - 1u] == 0xDDu),
		"HTTP entity-tag unaligned iterator output mismatch"
	);

	memset(SizeStorage, 0xEE, sizeof(SizeStorage));
	testRequire(
		xrtHttpETagWrite(
			(const xhttpetag*)(const void*)(TagStorage + 1u),
			Output,
			sizeof(Output),
			(size_t*)(void*)(SizeStorage + 1u)
		),
		"HTTP entity-tag rejected unaligned writer inputs"
	);
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire(
		(iSize == 13u) &&
		(memcmp(Output, "W/\"unaligned\"", iSize) == 0) &&
		(SizeStorage[0] == 0xEEu) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xEEu),
		"HTTP entity-tag unaligned writer output mismatch"
	);
	sBuilt = xrtHttpETagBuild(
		(const xhttpetag*)(const void*)(TagStorage + 1u),
		(size_t*)(void*)(SizeStorage + 1u)
	);
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire(
		(sBuilt != NULL) &&
		(iSize == 13u) &&
		(strcmp(sBuilt, "W/\"unaligned\"") == 0),
		"HTTP entity-tag Build rejected unaligned storage"
	);
	xrtFree(sBuilt);

	xrtClearError();
	testRequire(
		!xrtHttpETagParse(
			(xstrview){
				(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
				4u
			},
			&Tag
		) &&
		!xrtHttpETagParse(
			XRT_STR_LITERAL("\"tag\""),
			(xhttpetag*)(uintptr_t)(UINTPTR_MAX - 1u)
		),
		"HTTP entity-tag parser accepted wrapping range"
	);
	xrtClearError();
	testRequire(
		xrtHttpETagNext(
			XRT_STR_LITERAL("\"tag\""),
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u),
			&Item
		) == XHTTP_NEXT_ERROR,
		"HTTP entity-tag iterator accepted wrapping output"
	);
	xrtClearError();
	testRequire(
		!xrtHttpETagWeakEqual(
			(const xhttpetag*)(uintptr_t)(UINTPTR_MAX - 1u),
			&Tag
		),
		"HTTP entity-tag compare accepted wrapping input"
	);
	xrtClearError();
	Tag.Opaque = (xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
		4u
	};
	Tag.Weak = false;
	testRequire(
		!xrtHttpETagWrite(
			&Tag,
			Output,
			sizeof(Output),
			&iSize
		),
		"HTTP entity-tag writer accepted wrapping text"
	);
	xrtClearError();
	Tag.Opaque = XRT_STR_LITERAL("tag");
	testRequire(
		!xrtHttpETagWrite(
			&Tag,
			(void*)(uintptr_t)(UINTPTR_MAX - 1u),
			16u,
			&iSize
		) &&
		!xrtHttpETagWrite(
			&Tag,
			Output,
			sizeof(Output),
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
		),
		"HTTP entity-tag writer accepted wrapping output"
	);
	xrtClearError();
}



/* 穷举正文单字节，验证语法集合与写出解析往返完全一致。 */
static void testHttpETagAllBytes(void)
{
	xhttpetag Input;
	xhttpetag Output;
	char Text[8];
	char Byte;
	size_t iSize;
	unsigned int i;

	for ( i = 0; i <= 0xFFu; i++ ) {
		bool bValid = (i == 0x21u) ||
			((i >= 0x23u) && (i <= 0x7Eu)) ||
			(i >= 0x80u);

		Byte = (char)i;
		Input.Opaque = (xstrview){ &Byte, 1 };
		Input.Weak = ((i & 1u) != 0);
		if ( !bValid ) {
			testRequire(!xrtHttpETagWrite(
				&Input, Text, sizeof(Text), &iSize
			), "HTTP entity-tag writer accepted an invalid byte");
			xrtClearError();
			continue;
		}
		testRequire(xrtHttpETagWrite(
			&Input, Text, sizeof(Text), &iSize
		), "HTTP entity-tag all-byte write failed");
		testRequire(xrtHttpETagParse(
			(xstrview){ Text, iSize }, &Output
		) && (Output.Weak == Input.Weak) &&
			(Output.Opaque.Size == 1) &&
			((unsigned char)Output.Opaque.Data[0] == i),
			"HTTP entity-tag all-byte round trip mismatch");
	}
}



/* 执行 HTTP 实体标签协议测试。 */
int main(void)
{
	testHttpETagParse();
	testHttpETagList();
	testHttpETagCompare();
	testHttpETagWrite();
	testHttpETagAliases();
	testHttpETagMemory();
	testHttpETagAllBytes();
	printf("[PASS] http_etag\n");
	return 0;
}
