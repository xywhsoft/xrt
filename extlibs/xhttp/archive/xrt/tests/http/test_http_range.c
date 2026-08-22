#include "../test.h"



/* 比较字符串视图与固定文本。 */
static bool testHttpRangeTextEqual(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 生成可重复的轻量伪随机序列。 */
static uint32 testHttpRangeRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 生成覆盖完整线路数值宽度的可重复随机整数。 */
static uint64 testHttpRangeRandom64(uint32* pState)
{
	return ((uint64)testHttpRangeRandom(pState) << 32u) |
		(uint64)testHttpRangeRandom(pState);
}



/* 验证 Range 单位拆分保持扩展单位并拒绝线路歧义。 */
static void testHttpRangeParse(void)
{
	xstrview Unit;
	xstrview Set;

	testRequire(xrtHttpRangeParse(
		XRT_STR_LITERAL(" \tBytes=0-99, -20\t "),
		&Unit,
		&Set
	) && xrtHttpTokenEqual(Unit, XRT_STR_LITERAL("bytes")) &&
		testHttpRangeTextEqual(Set, "0-99, -20"),
		"HTTP Range split failed");
	testRequire(xrtHttpRangeParse(
		XRT_STR_LITERAL("items=1-2"), &Unit, &Set
	) && testHttpRangeTextEqual(Unit, "items") &&
		testHttpRangeTextEqual(Set, "1-2"),
		"HTTP Range split rejected an extension unit");
	testRequire(!xrtHttpRangeParse(
		XRT_STR_LITERAL("bytes =0-1"), &Unit, &Set
	), "HTTP Range split accepted whitespace before equals");
	xrtClearError();
	testRequire(!xrtHttpRangeParse(
		XRT_STR_LITERAL("bytes="), &Unit, &Set
	), "HTTP Range split accepted an empty set");
	xrtClearError();
}



/* 验证闭区间、开放区间、后缀和多范围迭代。 */
static void testHttpByteRangeIterate(void)
{
	xstrview Set = XRT_STR_LITERAL(
		", 0-99, , 200-, -50,"
	);
	xhttprangespec Spec;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount;

	Next = xrtHttpByteRangeNext(Set, &iOffset, &Spec);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		(Spec.Form == XHTTP_RANGE_SPEC_CLOSED) &&
		(Spec.First == 0) && (Spec.Last == 99),
		"HTTP byte range closed item mismatch");
	Next = xrtHttpByteRangeNext(Set, &iOffset, &Spec);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		(Spec.Form == XHTTP_RANGE_SPEC_OPEN) &&
		(Spec.First == 200),
		"HTTP byte range open item mismatch");
	Next = xrtHttpByteRangeNext(Set, &iOffset, &Spec);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		(Spec.Form == XHTTP_RANGE_SPEC_SUFFIX) &&
		(Spec.First == 50),
		"HTTP byte range suffix item mismatch");
	testRequire(xrtHttpByteRangeNext(
		Set, &iOffset, &Spec
	) == XHTTP_NEXT_END,
		"HTTP byte range set did not end exactly");
	testRequire(xrtHttpByteRangeCount(Set, &iCount) &&
		(iCount == 3),
		"HTTP byte range count mismatch");

	testRequire(!xrtHttpByteRangeCount(
		XRT_STR_LITERAL(",, \t"), &iCount
	), "HTTP byte range accepted no real items");
	xrtClearError();
	iOffset = 0;
	testRequire(xrtHttpByteRangeNext(
		XRT_STR_LITERAL("18446744073709551616-"),
		&iOffset,
		&Spec
	) == XHTTP_NEXT_ERROR,
		"HTTP byte range accepted uint64 overflow");
	xrtClearError();
	iOffset = 0;
	testRequire(xrtHttpByteRangeNext(
		XRT_STR_LITERAL("9-8"), &iOffset, &Spec
	) == XHTTP_NEXT_ERROR,
		"HTTP byte range accepted a reversed interval");
	xrtClearError();
}



/* 验证范围解析到表示长度时的裁剪和不满足语义。 */
static void testHttpByteRangeResolve(void)
{
	xhttprangespec Spec;
	xhttpbyterange Range;

	Spec = (xhttprangespec){
		XHTTP_RANGE_SPEC_CLOSED, 10, 99
	};
	testRequire(xrtHttpByteRangeResolve(
		&Spec, 50, &Range
	) == XHTTP_RANGE_SATISFIED &&
		(Range.First == 10) && (Range.Last == 49),
		"HTTP byte range did not clip the final position");
	Spec = (xhttprangespec){
		XHTTP_RANGE_SPEC_OPEN, 20, 0
	};
	testRequire(xrtHttpByteRangeResolve(
		&Spec, 50, &Range
	) == XHTTP_RANGE_SATISFIED &&
		(Range.First == 20) && (Range.Last == 49),
		"HTTP open byte range resolution failed");
	Spec = (xhttprangespec){
		XHTTP_RANGE_SPEC_SUFFIX, 20, 0
	};
	testRequire(xrtHttpByteRangeResolve(
		&Spec, 50, &Range
	) == XHTTP_RANGE_SATISFIED &&
		(Range.First == 30) && (Range.Last == 49),
		"HTTP suffix byte range resolution failed");
	Spec.First = 100;
	testRequire(xrtHttpByteRangeResolve(
		&Spec, 50, &Range
	) == XHTTP_RANGE_SATISFIED &&
		(Range.First == 0) && (Range.Last == 49),
		"HTTP oversized suffix did not select the full representation");
	Spec.First = 0;
	testRequire(xrtHttpByteRangeResolve(
		&Spec, 50, &Range
	) == XHTTP_RANGE_UNSATISFIED,
		"HTTP zero suffix was satisfiable");
	Spec = (xhttprangespec){
		XHTTP_RANGE_SPEC_OPEN, 50, 0
	};
	testRequire(xrtHttpByteRangeResolve(
		&Spec, 50, &Range
	) == XHTTP_RANGE_UNSATISFIED,
		"HTTP out-of-bounds range was satisfiable");
	Spec = (xhttprangespec){
		XHTTP_RANGE_SPEC_CLOSED, 9, 8
	};
	testRequire(xrtHttpByteRangeResolve(
		&Spec, 50, &Range
	) == XHTTP_RANGE_ERROR,
		"HTTP reversed range was not invalid");
	xrtClearError();
	Spec = (xhttprangespec){
		XHTTP_RANGE_SPEC_OPEN, 0, 0
	};
	testRequire(xrtHttpByteRangeResolve(
		&Spec, 0, &Range
	) == XHTTP_RANGE_UNSATISFIED,
		"HTTP empty representation satisfied an integer range");
	Spec = (xhttprangespec){
		XHTTP_RANGE_SPEC_SUFFIX, 1, 0
	};
	testRequire(xrtHttpByteRangeResolve(
		&Spec, 0, &Range
	) == XHTTP_RANGE_EMPTY,
		"HTTP empty representation lost suffix-range classification");
}



/* 验证范围集合统一完成裁剪、忽略不可满足项、排序和合并。 */
static void testHttpByteRangesResolve(void)
{
	xhttpbyterange Ranges[6];
	xhttprangeresult Result;
	size_t iCount = 77;
	uint64 iSelected = 88;

	Result = xrtHttpByteRangesResolve(
		XRT_STR_LITERAL("30-39, 10-19, 18-25, 200-, -5"),
		100,
		Ranges,
		6,
		0,
		&iCount,
		&iSelected
	);
	testRequire(
		(Result == XHTTP_RANGE_SATISFIED) &&
		(iCount == 3) &&
		(iSelected == 31) &&
		(Ranges[0].First == 10) &&
		(Ranges[0].Last == 25) &&
		(Ranges[1].First == 30) &&
		(Ranges[1].Last == 39) &&
		(Ranges[2].First == 95) &&
		(Ranges[2].Last == 99),
		"HTTP byte range set resolution mismatch"
	);

	Result = xrtHttpByteRangesResolve(
		XRT_STR_LITERAL("0-2, 5-7"),
		10,
		Ranges,
		6,
		2,
		&iCount,
		&iSelected
	);
	testRequire(
		(Result == XHTTP_RANGE_SATISFIED) &&
		(iCount == 1) &&
		(iSelected == 8) &&
		(Ranges[0].First == 0) &&
		(Ranges[0].Last == 7),
		"HTTP byte range merge gap mismatch"
	);

	Result = xrtHttpByteRangesResolve(
		XRT_STR_LITERAL("10-, -0"),
		5,
		Ranges,
		6,
		0,
		&iCount,
		&iSelected
	);
	testRequire(
		(Result == XHTTP_RANGE_UNSATISFIED) &&
		(iCount == 0) &&
		(iSelected == 0),
		"HTTP byte range set unsatisfied state mismatch"
	);

	Result = xrtHttpByteRangesResolve(
		XRT_STR_LITERAL("-1"),
		0,
		Ranges,
		6,
		0,
		&iCount,
		&iSelected
	);
	testRequire(
		(Result == XHTTP_RANGE_EMPTY) &&
		(iCount == 0) &&
		(iSelected == 0),
		"HTTP byte range set empty state mismatch"
	);
}



/* 验证 Range 写出覆盖旧 xweb 的单范围能力并支持多范围。 */
static void testHttpRangeWrite(void)
{
	xhttprangespec Specs[] = {
		{ XHTTP_RANGE_SPEC_CLOSED, 0, 99 },
		{ XHTTP_RANGE_SPEC_OPEN, 200, 0 },
		{ XHTTP_RANGE_SPEC_SUFFIX, 50, 0 }
	};
	char Buffer[64];
	str sBuilt;
	size_t iSize;

	testRequire(xrtHttpRangeWrite(
		Specs, 3, NULL, 0, &iSize
	) && (iSize == 21),
		"HTTP Range size query failed");
	testRequire(xrtHttpRangeWrite(
		Specs, 3, Buffer, sizeof(Buffer), &iSize
	) && (iSize == 21) &&
		(memcmp(Buffer, "bytes=0-99, 200-, -50", 21) == 0),
		"HTTP Range write failed");
	sBuilt = xrtHttpRangeBuild(Specs, 3, &iSize);
	testRequire((sBuilt != NULL) && (iSize == 21) &&
		(strcmp(sBuilt, "bytes=0-99, 200-, -50") == 0),
		"HTTP Range build failed");
	xrtFree(sBuilt);
}



/* 验证满足、未知长度和 416 形式的 Content-Range。 */
static void testHttpContentRange(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT("bytes 9-8/10"),
		XRT_STR_INIT("bytes 0-10/10"),
		XRT_STR_INIT("bytes */*"),
		XRT_STR_INIT("bytes 0-1"),
		XRT_STR_INIT("bytes\t0-1/2"),
		XRT_STR_INIT("bytes 0 -1/2")
	};
	xhttpcontentrange Range;
	char Buffer[64];
	str sBuilt;
	size_t iSize;
	size_t i;

	testRequire(xrtHttpContentRangeParse(
		XRT_STR_LITERAL("Bytes 0-99/200"), &Range
	) && Range.Satisfied && Range.HasLength &&
		(Range.First == 0) && (Range.Last == 99) &&
		(Range.Length == 200),
		"HTTP Content-Range satisfied parse failed");
	testRequire(xrtHttpContentRangeParse(
		XRT_STR_LITERAL("bytes 100-199/*"), &Range
	) && Range.Satisfied && !Range.HasLength &&
		(Range.First == 100) && (Range.Last == 199),
		"HTTP Content-Range unknown length parse failed");
	testRequire(xrtHttpContentRangeParse(
		XRT_STR_LITERAL("bytes */0"), &Range
	) && !Range.Satisfied && Range.HasLength &&
		(Range.Length == 0),
		"HTTP Content-Range unsatisfied parse failed");

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpContentRangeParse(
			Invalid[i], &Range
		), "HTTP Content-Range accepted malformed syntax");
		xrtClearError();
	}

	Range = (xhttpcontentrange){
		true, true, 20, 29, 100
	};
	testRequire(xrtHttpContentRangeWrite(
		&Range, Buffer, sizeof(Buffer), &iSize
	) && (iSize == 15) &&
		(memcmp(Buffer, "bytes 20-29/100", 15) == 0),
		"HTTP Content-Range write failed");
	Range = (xhttpcontentrange){
		false, true, 0, 0, 100
	};
	sBuilt = xrtHttpContentRangeBuild(&Range, &iSize);
	testRequire((sBuilt != NULL) && (iSize == 11) &&
		(strcmp(sBuilt, "bytes */100") == 0),
		"HTTP Content-Range build failed");
	xrtFree(sBuilt);
}



/* 验证范围解析器的输入、游标和结构输出必须相互独立。 */
static void testHttpRangeAliases(void)
{
	union {
		xstrview View;
		char Text[64];
	} Parse;
	union {
		xhttprangespec Spec;
		char Text[64];
	} Iterate;
	union {
		size_t Offset;
		xhttprangespec Spec;
	} IteratorOutputs;
	union {
		size_t Count;
		char Text[64];
	} Count;
	union {
		xhttprangespec Spec;
		xhttpbyterange Range;
	} Resolve;
	union {
		xhttpcontentrange Range;
		char Text[64];
	} Content;
	xstrview Output;
	xstrview Set;
	char Snapshot[64];
	size_t iOffset = 0;

	memset(&Parse, 0, sizeof(Parse));
	memcpy(Parse.Text, "bytes=0-1", 9);
	memcpy(Snapshot, Parse.Text, sizeof(Snapshot));
	testRequire(!xrtHttpRangeParse(
		(xstrview){ Parse.Text, 9 },
		&Parse.View,
		&Set
	), "HTTP Range parser accepted overlapping input");
	testRequire(memcmp(
		Parse.Text, Snapshot, sizeof(Snapshot)
	) == 0, "HTTP Range alias failure changed input");
	xrtClearError();

	Output = XRT_STR_LITERAL("unchanged");
	testRequire(!xrtHttpRangeParse(
		XRT_STR_LITERAL("bytes=0-1"),
		&Output,
		&Output
	), "HTTP Range parser accepted overlapping outputs");
	testRequire(testHttpRangeTextEqual(
		Output, "unchanged"
	), "HTTP Range output alias failure changed output");
	xrtClearError();

	memset(&Iterate, 0, sizeof(Iterate));
	memcpy(Iterate.Text, "0-1", 3);
	memcpy(Snapshot, Iterate.Text, sizeof(Snapshot));
	testRequire(xrtHttpByteRangeNext(
		(xstrview){ Iterate.Text, 3 },
		&iOffset,
		&Iterate.Spec
	) == XHTTP_NEXT_ERROR,
		"HTTP byte range iterator accepted input/output overlap");
	testRequire(memcmp(
		Iterate.Text, Snapshot, sizeof(Snapshot)
	) == 0, "HTTP byte range iterator changed aliased input");
	xrtClearError();

	memset(&IteratorOutputs, 0, sizeof(IteratorOutputs));
	testRequire(xrtHttpByteRangeNext(
		XRT_STR_LITERAL("0-1"),
		&IteratorOutputs.Offset,
		&IteratorOutputs.Spec
	) == XHTTP_NEXT_ERROR,
		"HTTP byte range iterator accepted overlapping outputs");
	xrtClearError();

	memset(&Count, 0, sizeof(Count));
	memcpy(Count.Text, "0-1", 3);
	memcpy(Snapshot, Count.Text, sizeof(Snapshot));
	testRequire(!xrtHttpByteRangeCount(
		(xstrview){ Count.Text, 3 }, &Count.Count
	), "HTTP byte range count accepted overlapping output");
	testRequire(memcmp(
		Count.Text, Snapshot, sizeof(Snapshot)
	) == 0, "HTTP byte range count changed aliased input");
	xrtClearError();

	memset(&Resolve, 0, sizeof(Resolve));
	Resolve.Spec = (xhttprangespec){
		XHTTP_RANGE_SPEC_CLOSED, 0, 1
	};
	testRequire(xrtHttpByteRangeResolve(
		&Resolve.Spec, 2, &Resolve.Range
	) == XHTTP_RANGE_ERROR,
		"HTTP byte range resolver accepted overlapping objects");
	xrtClearError();

	memset(&Content, 0, sizeof(Content));
	memcpy(Content.Text, "bytes 0-1/2", 11);
	memcpy(Snapshot, Content.Text, sizeof(Snapshot));
	testRequire(!xrtHttpContentRangeParse(
		(xstrview){ Content.Text, 11 },
		&Content.Range
	), "HTTP Content-Range parser accepted overlapping output");
	testRequire(memcmp(
		Content.Text, Snapshot, sizeof(Snapshot)
	) == 0, "HTTP Content-Range alias failure changed input");
	xrtClearError();
}



/* 随机验证多范围写出、解析、计数和表示裁剪的不变量。 */
static void testHttpRangeRoundTrips(void)
{
	xhttprangespec Input[4];
	xhttprangespec Output;
	xhttpbyterange Resolved;
	xstrview Unit;
	xstrview Set;
	char Text[256];
	uint32 iState = UINT32_C(0x52414E47);
	size_t iRound;

	for ( iRound = 0; iRound < 6000u; iRound++ ) {
		size_t iCount = (testHttpRangeRandom(&iState) % 4u) + 1u;
		size_t iParsedCount;
		size_t iOffset = 0;
		size_t iSize;
		size_t i;

		if ( iRound == 0 ) {
			iCount = 3;
			Input[0] = (xhttprangespec){
				XHTTP_RANGE_SPEC_CLOSED,
				UINT64_MAX - UINT64_C(1),
				UINT64_MAX
			};
			Input[1] = (xhttprangespec){
				XHTTP_RANGE_SPEC_OPEN,
				UINT64_MAX,
				0
			};
			Input[2] = (xhttprangespec){
				XHTTP_RANGE_SPEC_SUFFIX,
				UINT64_MAX,
				0
			};
		} else {
			for ( i = 0; i < iCount; i++ ) {
				uint64 iLeft = testHttpRangeRandom64(&iState);
				uint64 iRight = testHttpRangeRandom64(&iState);

				Input[i].Form = (xhttprangespecform)(
					(testHttpRangeRandom(&iState) % 3u) + 1u
				);
				if ( Input[i].Form == XHTTP_RANGE_SPEC_CLOSED ) {
					Input[i].First = (iLeft <= iRight) ?
						iLeft :
						iRight;
					Input[i].Last = (iLeft <= iRight) ?
						iRight :
						iLeft;
				} else {
					Input[i].First = iLeft;
					Input[i].Last = 0;
				}
			}
		}

		testRequire(xrtHttpRangeWrite(
			Input, iCount, Text, sizeof(Text), &iSize
		), "HTTP Range property write failed");
		testRequire(xrtHttpRangeParse(
			(xstrview){ Text, iSize }, &Unit, &Set
		) && xrtHttpTokenEqual(
			Unit, XRT_STR_LITERAL("bytes")
		), "HTTP Range property split failed");
		testRequire(xrtHttpByteRangeCount(
			Set, &iParsedCount
		) && (iParsedCount == iCount),
			"HTTP Range property count mismatch");

		for ( i = 0; i < iCount; i++ ) {
			uint64 iLength = testHttpRangeRandom64(&iState);
			xhttprangeresult Result;

			testRequire(xrtHttpByteRangeNext(
				Set, &iOffset, &Output
			) == XHTTP_NEXT_ITEM,
				"HTTP Range property iterator ended early");
			testRequire((Output.Form == Input[i].Form) &&
				(Output.First == Input[i].First) &&
				(Output.Last == Input[i].Last),
				"HTTP Range property round trip mismatch");

			Result = xrtHttpByteRangeResolve(
				&Output, iLength, &Resolved
			);
			if ( Result == XHTTP_RANGE_SATISFIED ) {
				testRequire((iLength != 0) &&
					(Resolved.First <= Resolved.Last) &&
					(Resolved.Last < iLength),
					"HTTP Range resolver published an invalid interval");
			} else if ( Result == XHTTP_RANGE_EMPTY ) {
				testRequire(
					(Output.Form == XHTTP_RANGE_SPEC_SUFFIX) &&
					(Output.First != 0) &&
					(iLength == 0),
					"HTTP Range resolver misclassified an empty range"
				);
			} else {
				testRequire(Result == XHTTP_RANGE_UNSATISFIED,
					"HTTP Range resolver rejected a valid specification");
			}
		}
		testRequire(xrtHttpByteRangeNext(
			Set, &iOffset, &Output
		) == XHTTP_NEXT_END,
			"HTTP Range property iterator left trailing data");
	}
}



/* 执行 HTTP Range 与 Content-Range 协议测试。 */
/* 验证 Range 核心解析与聚合支持未对齐存储并拒绝回绕区间。 */
static void testHttpRangeMemoryContract(void)
{
	uint8 UnitStorage[sizeof(xstrview) + 2u];
	uint8 SetStorage[sizeof(xstrview) + 2u];
	uint8 OffsetStorage[sizeof(size_t) + 2u];
	uint8 SpecStorage[sizeof(xhttprangespec) + 2u];
	uint8 RangeStorage[(sizeof(xhttpbyterange) * 2u) + 2u];
	uint8 CountStorage[sizeof(size_t) + 2u];
	uint8 SelectedStorage[sizeof(uint64) + 2u];
	uint8 ContentStorage[sizeof(xhttpcontentrange) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	xhttprangespec Spec;
	xhttpbyterange Range[2];
	xhttpcontentrange ContentRange;
	xstrview Unit;
	xstrview Set;
	char Output[64];
	str sBuilt;
	size_t iOffset = 0;
	size_t iCount;
	size_t iSize;
	uint64 iSelected;

	memset(UnitStorage, 0xA5, sizeof(UnitStorage));
	memset(SetStorage, 0xB6, sizeof(SetStorage));
	testRequire(xrtHttpRangeParse(
		XRT_STR_LITERAL("bytes=0-1"),
		(xstrview*)(UnitStorage + 1u),
		(xstrview*)(SetStorage + 1u)
	), "HTTP Range parser rejected unaligned outputs");
	memcpy(&Unit, UnitStorage + 1u, sizeof(Unit));
	memcpy(&Set, SetStorage + 1u, sizeof(Set));
	testRequire(xrtHttpTokenEqual(
		Unit, XRT_STR_LITERAL("bytes")
	) && (Set.Size == 3u) &&
		(UnitStorage[0] == 0xA5u) &&
		(UnitStorage[sizeof(UnitStorage) - 1u] == 0xA5u) &&
		(SetStorage[0] == 0xB6u) &&
		(SetStorage[sizeof(SetStorage) - 1u] == 0xB6u),
		"HTTP Range unaligned parse output mismatch");

	memcpy(OffsetStorage + 1u, &iOffset, sizeof(iOffset));
	memset(SpecStorage, 0xC7, sizeof(SpecStorage));
	testRequire(xrtHttpByteRangeNext(
		Set,
		(size_t*)(OffsetStorage + 1u),
		(xhttprangespec*)(SpecStorage + 1u)
	) == XHTTP_NEXT_ITEM,
		"HTTP Range iterator rejected unaligned outputs");
	memcpy(&Spec, SpecStorage + 1u, sizeof(Spec));
	testRequire((Spec.Form == XHTTP_RANGE_SPEC_CLOSED) &&
		(Spec.First == 0u) && (Spec.Last == 1u),
		"HTTP Range unaligned iterator output mismatch");

	memset(RangeStorage, 0xD8, sizeof(RangeStorage));
	testRequire(xrtHttpByteRangeResolve(
		(const xhttprangespec*)(SpecStorage + 1u),
		10u,
		(xhttpbyterange*)(RangeStorage + 1u)
	) == XHTTP_RANGE_SATISFIED,
		"HTTP Range resolver rejected unaligned structures");
	memcpy(&Range[0], RangeStorage + 1u, sizeof(Range[0]));
	testRequire((Range[0].First == 0u) &&
		(Range[0].Last == 1u),
		"HTTP Range unaligned resolve output mismatch");

	memset(RangeStorage, 0xE9, sizeof(RangeStorage));
	memset(CountStorage, 0x5A, sizeof(CountStorage));
	memset(SelectedStorage, 0x6B, sizeof(SelectedStorage));
	testRequire(xrtHttpByteRangesResolve(
		XRT_STR_LITERAL("4-5, 0-1"),
		10u,
		(xhttpbyterange*)(RangeStorage + 1u),
		2u,
		0,
		(size_t*)(CountStorage + 1u),
		(uint64*)(SelectedStorage + 1u)
	) == XHTTP_RANGE_SATISFIED,
		"HTTP Range set rejected unaligned outputs");
	memcpy(Range, RangeStorage + 1u, sizeof(Range));
	memcpy(&iCount, CountStorage + 1u, sizeof(iCount));
	memcpy(&iSelected, SelectedStorage + 1u, sizeof(iSelected));
	testRequire((iCount == 2u) && (iSelected == 4u) &&
		(Range[0].First == 0u) && (Range[0].Last == 1u) &&
		(Range[1].First == 4u) && (Range[1].Last == 5u) &&
		(RangeStorage[0] == 0xE9u) &&
		(RangeStorage[sizeof(RangeStorage) - 1u] == 0xE9u) &&
		(CountStorage[0] == 0x5Au) &&
		(CountStorage[sizeof(CountStorage) - 1u] == 0x5Au) &&
		(SelectedStorage[0] == 0x6Bu) &&
		(SelectedStorage[sizeof(SelectedStorage) - 1u] == 0x6Bu),
		"HTTP Range unaligned set output mismatch");

	xrtClearError();
	testRequire(xrtHttpByteRangesResolve(
		XRT_STR_LITERAL("0-1"),
		10u,
		(xhttpbyterange*)(uintptr_t)(UINTPTR_MAX - 1u),
		1u,
		0,
		&iCount,
		&iSelected
	) == XHTTP_RANGE_ERROR,
		"HTTP Range set accepted a wrapping range output");
	xrtClearError();
	testRequire(xrtHttpByteRangeNext(
		XRT_STR_LITERAL("0-1"),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u),
		&Spec
	) == XHTTP_NEXT_ERROR,
		"HTTP Range iterator accepted a wrapping offset output");
	xrtClearError();

	memset(SizeStorage, 0x7C, sizeof(SizeStorage));
	testRequire(xrtHttpRangeWrite(
		(const xhttprangespec*)(SpecStorage + 1u),
		1u,
		Output,
		sizeof(Output),
		(size_t*)(SizeStorage + 1u)
	), "HTTP Range writer rejected unaligned storage");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((iSize == 9u) &&
		(memcmp(Output, "bytes=0-1", iSize) == 0) &&
		(SizeStorage[0] == 0x7Cu) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0x7Cu),
		"HTTP Range unaligned writer output mismatch");
	sBuilt = xrtHttpRangeBuild(
		(const xhttprangespec*)(SpecStorage + 1u),
		1u,
		(size_t*)(SizeStorage + 1u)
	);
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((sBuilt != NULL) && (iSize == 9u) &&
		(strcmp(sBuilt, "bytes=0-1") == 0),
		"HTTP Range Build rejected unaligned storage");
	xrtFree(sBuilt);

	memset(ContentStorage, 0x8D, sizeof(ContentStorage));
	testRequire(xrtHttpContentRangeParse(
		XRT_STR_LITERAL("bytes 0-1/10"),
		(xhttpcontentrange*)(ContentStorage + 1u)
	), "HTTP Content-Range parser rejected unaligned output");
	memcpy(
		&ContentRange,
		ContentStorage + 1u,
		sizeof(ContentRange)
	);
	testRequire(ContentRange.Satisfied &&
		ContentRange.HasLength &&
		(ContentRange.First == 0u) &&
		(ContentRange.Last == 1u) &&
		(ContentRange.Length == 10u) &&
		(ContentStorage[0] == 0x8Du) &&
		(ContentStorage[sizeof(ContentStorage) - 1u] == 0x8Du),
		"HTTP Content-Range unaligned parse mismatch");
	testRequire(xrtHttpContentRangeWrite(
		(const xhttpcontentrange*)(ContentStorage + 1u),
		Output,
		sizeof(Output),
		(size_t*)(SizeStorage + 1u)
	), "HTTP Content-Range writer rejected unaligned storage");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((iSize == 12u) &&
		(memcmp(Output, "bytes 0-1/10", iSize) == 0),
		"HTTP Content-Range unaligned writer mismatch");
	sBuilt = xrtHttpContentRangeBuild(
		(const xhttpcontentrange*)(ContentStorage + 1u),
		(size_t*)(SizeStorage + 1u)
	);
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((sBuilt != NULL) && (iSize == 12u) &&
		(strcmp(sBuilt, "bytes 0-1/10") == 0),
		"HTTP Content-Range Build rejected unaligned storage");
	xrtFree(sBuilt);

	xrtClearError();
	testRequire(!xrtHttpRangeWrite(
		(const xhttprangespec*)(uintptr_t)(UINTPTR_MAX - 1u),
		1u,
		Output,
		sizeof(Output),
		&iSize
	), "HTTP Range writer accepted wrapping input");
	xrtClearError();
	testRequire(!xrtHttpRangeWrite(
		&Spec,
		1u,
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		32u,
		&iSize
	), "HTTP Range writer accepted wrapping byte output");
	xrtClearError();
	testRequire(!xrtHttpContentRangeParse(
		XRT_STR_LITERAL("bytes 0-1/10"),
		(xhttpcontentrange*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP Content-Range parser accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpContentRangeWrite(
		(const xhttpcontentrange*)(uintptr_t)(UINTPTR_MAX - 1u),
		Output,
		sizeof(Output),
		&iSize
	), "HTTP Content-Range writer accepted wrapping input");
	xrtClearError();
}



int main(void)
{
	testHttpRangeParse();
	testHttpByteRangeIterate();
	testHttpByteRangeResolve();
	testHttpByteRangesResolve();
	testHttpRangeWrite();
	testHttpContentRange();
	testHttpRangeAliases();
	testHttpRangeRoundTrips();
	testHttpRangeMemoryContract();
	printf("[PASS] http_range\n");
	return 0;
}
