#include "test.h"

static uint64 testCronBit(unsigned Value) { return (uint64)1u << Value; }

static xcronexpression testCronCompile(xstrview Text, xcrondialect Dialect)
{
	xcronexpression Expression; xcronexpressionissue Issue;
	if (!xcronExpressionCompile(Text, Dialect, &Expression, &Issue)) {
		fprintf(stderr, "unexpected cron error %d at %zu in %.*s\n", (int)Issue.Code, Issue.Offset, (int)Text.Size, Text.Data);
		testRequire(false, "valid cron expression rejected");
	}
	testRequire(Issue.Code == XCRON_EXPRESSION_OK && Issue.Field == XCRON_FIELD_NONE && Issue.Offset == Text.Size,
		"cron success issue");
	return Expression;
}

static void testCronKnown(void)
{
	xcronexpression All = testCronCompile(XRT_STR_LITERAL("* * * * *"), XCRON_UNIX5);
	testRequire(All.Dialect == XCRON_UNIX5 && All.Flags == (XCRON_DAY_MONTH_STAR | XCRON_DAY_WEEK_STAR) &&
		All.Fields[0] == 1 && All.Fields[1] == testCronBit(60) - 1u && All.Fields[2] == 0xffffffu &&
		All.Fields[3] == 0xfffffffeu && All.Fields[4] == 0x1ffeu && All.Fields[5] == 0x7fu, "wildcard bitmaps");
	xcronexpression Spec = testCronCompile(XRT_STR_LITERAL("*/15 0,30 9-17/2 1,15 JAN-MAR MON-FRI"), XCRON_SECONDS6);
	testRequire(Spec.Flags == 0 && Spec.Fields[0] == (testCronBit(0) | testCronBit(15) | testCronBit(30) | testCronBit(45)) &&
		Spec.Fields[1] == (testCronBit(0) | testCronBit(30)) &&
		Spec.Fields[2] == (testCronBit(9) | testCronBit(11) | testCronBit(13) | testCronBit(15) | testCronBit(17)) &&
		Spec.Fields[3] == (testCronBit(1) | testCronBit(15)) && Spec.Fields[4] == 0x0eu && Spec.Fields[5] == 0x3eu,
		"seconds/ranges/names/steps bitmaps");
	xcronexpression Names = testCronCompile(XRT_STR_LITERAL("0 0 * jan,Feb,mAr,apr,MAY,jun,JUL,aug,sep,oct,nov,dec sun,Mon,tue,WED,thu,fri,SAT,7"), XCRON_UNIX5);
	testRequire(Names.Fields[4] == All.Fields[4] && Names.Fields[5] == All.Fields[5] && Names.Flags == XCRON_DAY_MONTH_STAR,
		"case-insensitive names and Sunday alias");
	xcronexpression Mixed = testCronCompile(XRT_STR_LITERAL("0005 0000 1,*/2 * 0,7,0"), XCRON_UNIX5);
	testRequire(Mixed.Fields[1] == testCronBit(5) && Mixed.Fields[3] == 0xaaaaaaaau && Mixed.Fields[5] == 1 &&
		Mixed.Flags == XCRON_DAY_MONTH_STAR, "mixed wildcard list semantics");
	xcronexpression Full = testCronCompile(XRT_STR_LITERAL("0 0 1-31 * 0-7"), XCRON_UNIX5);
	testRequire(Full.Fields[3] == All.Fields[3] && Full.Fields[5] == All.Fields[5] && Full.Flags == 0, "full range lost day restriction");
	xcronexpression Anchored = testCronCompile(XRT_STR_LITERAL("7-59/13 * * * *"), XCRON_UNIX5);
	testRequire(Anchored.Fields[1] == (testCronBit(7) | testCronBit(20) | testCronBit(33) | testCronBit(46) | testCronBit(59)), "range step anchor");
	xcronexpression HugeStep = testCronCompile(XRT_STR_LITERAL("*/4294967295 * * * */7"), XCRON_UNIX5);
	testRequire(HugeStep.Fields[1] == 1 && HugeStep.Fields[5] == 1, "large step or Sunday deduplication");
	static const struct { cstr Alias, Expanded; } Aliases[] = {
		{"@yearly", "0 0 1 1 *"}, {"@annually", "0 0 1 1 *"}, {"@monthly", "0 0 1 * *"},
		{"@weekly", "0 0 * * 0"}, {"@daily", "0 0 * * *"}, {"@midnight", "0 0 * * *"}, {"@hourly", "0 * * * *"}};
	for (size_t Index = 0; Index < sizeof(Aliases) / sizeof(Aliases[0]); ++Index) {
		xcronexpression Expanded = testCronCompile((xstrview){Aliases[Index].Expanded, strlen(Aliases[Index].Expanded)}, XCRON_UNIX5);
		for (int Dialect = XCRON_UNIX5; Dialect <= XCRON_SECONDS6; ++Dialect) {
			xcronexpression Alias = testCronCompile((xstrview){Aliases[Index].Alias, strlen(Aliases[Index].Alias)}, (xcrondialect)Dialect);
			testRequire(memcmp(Alias.Fields, Expanded.Fields, sizeof(Alias.Fields)) == 0 && Alias.Flags == Expanded.Flags &&
				Alias.Dialect == (xcrondialect)Dialect, "alias expansion");
		}
	}
	xcronexpression Whitespace = testCronCompile(XRT_STR_LITERAL(" \t*\t * *\t* * \t"), XCRON_UNIX5);
	testRequire(memcmp(Whitespace.Fields, All.Fields, sizeof(All.Fields)) == 0 && Whitespace.Flags == All.Flags, "space/tab grammar");
	char Owned[] = "1 2 3 4 5"; xcronexpression Saved = testCronCompile((xstrview){Owned, sizeof(Owned)-1u}, XCRON_UNIX5);
	memset(Owned, '?', sizeof(Owned));
	testRequire(Saved.Fields[1] == 2 && Saved.Fields[2] == 4 && Saved.Fields[3] == 8 && Saved.Fields[4] == 16 && Saved.Fields[5] == 32,
		"compiled expression retained input");
}

static void testCronBad(xstrview Text, xcronexpressionerror Code, xcronfield Field, size_t Offset)
{
	xcronexpression Output, Before; xcronexpressionissue Issue;
	memset(&Output, 0xa5, sizeof(Output)); memcpy(&Before, &Output, sizeof(Output));
	testRequire(!xcronExpressionCompile(Text, XCRON_UNIX5, &Output, &Issue), "bad cron accepted");
	if (Issue.Code != Code || Issue.Field != Field || (Offset != SIZE_MAX && Issue.Offset != Offset))
		fprintf(stderr, "cron issue %d/%d/%zu expected %d/%d/%zu in %.*s\n", Issue.Code, Issue.Field, Issue.Offset,
			Code, Field, Offset, (int)Text.Size, Text.Data);
	testRequire(Issue.Code == Code && Issue.Field == Field &&
		(Offset == SIZE_MAX || Issue.Offset == Offset) && Issue.Offset <= Text.Size &&
		memcmp(&Output, &Before, sizeof(Output)) == 0, "cron issue/output atomicity");
	testRequire(xrtErrorCode(xrtGetError()) == (int32)Code && strcmp(xrtErrorDomain(xrtGetError()), "xrt.cron") == 0, "cron XRT diagnostic");
	xrtClearError();
}

static void testCronInvalid(void)
{
	testCronBad((xstrview){NULL, 0}, XCRON_EXPRESSION_SYNTAX, XCRON_FIELD_MINUTE, 0);
	testCronBad(XRT_STR_LITERAL("* * * *"), XCRON_EXPRESSION_SYNTAX, XCRON_FIELD_WEEKDAY, 7);
	testCronBad(XRT_STR_LITERAL("* * * * * *"), XCRON_EXPRESSION_SYNTAX, XCRON_FIELD_NONE, 10);
	testCronBad(XRT_STR_LITERAL("60 * * * *"), XCRON_EXPRESSION_RANGE, XCRON_FIELD_MINUTE, 0);
	testCronBad(XRT_STR_LITERAL("* 24 * * *"), XCRON_EXPRESSION_RANGE, XCRON_FIELD_HOUR, 2);
	testCronBad(XRT_STR_LITERAL("* * 0 * *"), XCRON_EXPRESSION_RANGE, XCRON_FIELD_DAY, 4);
	testCronBad(XRT_STR_LITERAL("* * * 0 *"), XCRON_EXPRESSION_RANGE, XCRON_FIELD_MONTH, 6);
	testCronBad(XRT_STR_LITERAL("* * * * 8"), XCRON_EXPRESSION_RANGE, XCRON_FIELD_WEEKDAY, 8);
	testCronBad(XRT_STR_LITERAL("59-1 * * * *"), XCRON_EXPRESSION_RANGE, XCRON_FIELD_MINUTE, 4);
	testCronBad(XRT_STR_LITERAL("*/0 * * * *"), XCRON_EXPRESSION_RANGE, XCRON_FIELD_MINUTE, 3);
	testCronBad(XRT_STR_LITERAL("1/2 * * * *"), XCRON_EXPRESSION_SYNTAX, XCRON_FIELD_MINUTE, 1);
	testCronBad(XRT_STR_LITERAL("*/4294967296 * * * *"), XCRON_EXPRESSION_OVERFLOW, XCRON_FIELD_MINUTE, 11);
	testCronBad(XRT_STR_LITERAL("4294967296 * * * *"), XCRON_EXPRESSION_OVERFLOW, XCRON_FIELD_MINUTE, 9);
	testCronBad(XRT_STR_LITERAL("* * * * MONDAY"), XCRON_EXPRESSION_SYNTAX, XCRON_FIELD_WEEKDAY, 11);
	testCronBad(XRT_STR_LITERAL("* * * JANUARY *"), XCRON_EXPRESSION_SYNTAX, XCRON_FIELD_MONTH, 9);
	testCronBad(XRT_STR_LITERAL("* * * * FRI-SUN"), XCRON_EXPRESSION_RANGE, XCRON_FIELD_WEEKDAY, SIZE_MAX);
	testCronBad(XRT_STR_LITERAL("1,,2 * * * *"), XCRON_EXPRESSION_SYNTAX, XCRON_FIELD_MINUTE, 2);
	testCronBad(XRT_STR_LITERAL("1, * * * *"), XCRON_EXPRESSION_SYNTAX, XCRON_FIELD_MINUTE, 2);
	testCronBad(XRT_STR_LITERAL(",1 * * * *"), XCRON_EXPRESSION_SYNTAX, XCRON_FIELD_MINUTE, 0);
	testCronBad(XRT_STR_LITERAL("*/-1 * * * *"), XCRON_EXPRESSION_SYNTAX, XCRON_FIELD_MINUTE, 2);
	testCronBad(XRT_STR_LITERAL("? * * * *"), XCRON_EXPRESSION_UNSUPPORTED, XCRON_FIELD_MINUTE, 0);
	testCronBad(XRT_STR_LITERAL("0 0 L * *"), XCRON_EXPRESSION_UNSUPPORTED, XCRON_FIELD_DAY, 4);
	testCronBad(XRT_STR_LITERAL("0 0 1W * *"), XCRON_EXPRESSION_UNSUPPORTED, XCRON_FIELD_DAY, 5);
	testCronBad(XRT_STR_LITERAL("0 0 * * MON#2"), XCRON_EXPRESSION_UNSUPPORTED, XCRON_FIELD_WEEKDAY, 11);
	testCronBad(XRT_STR_LITERAL("* * * * 1~3"), XCRON_EXPRESSION_UNSUPPORTED, XCRON_FIELD_WEEKDAY, 9);
	testCronBad(XRT_STR_LITERAL("@reboot"), XCRON_EXPRESSION_UNSUPPORTED, XCRON_FIELD_NONE, 0);
	testCronBad(XRT_STR_LITERAL("@DAILY"), XCRON_EXPRESSION_UNSUPPORTED, XCRON_FIELD_NONE, 0);
	testCronBad(XRT_STR_LITERAL("* * * * *\n"), XCRON_EXPRESSION_SYNTAX, XCRON_FIELD_WEEKDAY, SIZE_MAX);
	testCronBad(XRT_STR_LITERAL("* * * * *\0"), XCRON_EXPRESSION_SYNTAX, XCRON_FIELD_WEEKDAY, SIZE_MAX);
	testCronBad(XRT_STR_LITERAL("\xff * * * *"), XCRON_EXPRESSION_SYNTAX, XCRON_FIELD_MINUTE, 0);
	char Maximum[XCRON_EXPRESSION_MAX_BYTES + 1u]; memset(Maximum, ' ', sizeof(Maximum));
	memcpy(Maximum, "* * * * *", 9);
	(void)testCronCompile((xstrview){Maximum, XCRON_EXPRESSION_MAX_BYTES}, XCRON_UNIX5);
	testCronBad((xstrview){Maximum, sizeof(Maximum)}, XCRON_EXPRESSION_LIMIT, XCRON_FIELD_NONE, XCRON_EXPRESSION_MAX_BYTES);
	xcronexpression Output, Before; xcronexpressionissue Issue = {XCRON_EXPRESSION_RANGE, XCRON_FIELD_HOUR, 77};
	memset(&Output, 0xa5, sizeof(Output)); memcpy(&Before, &Output, sizeof(Output));
	testRequire(!xcronExpressionCompile(XRT_STR_LITERAL("* * * * *"), (xcrondialect)0, &Output, &Issue) &&
		memcmp(&Output, &Before, sizeof(Output)) == 0 && Issue.Code == XCRON_EXPRESSION_RANGE &&
		Issue.Field == XCRON_FIELD_HOUR && Issue.Offset == 77, "invalid dialect changed outputs");
	testRequire(!xcronExpressionCompile((xstrview){NULL, 1}, XCRON_UNIX5, &Output, &Issue), "NULL nonempty expression");
	testRequire(!xcronExpressionCompile((xstrview){(cstr)&Output, 9}, XCRON_UNIX5, &Output, &Issue), "text/output overlap");
	testRequire(!xcronExpressionCompile(XRT_STR_LITERAL("* * * * *"), XCRON_UNIX5, &Output, (xcronexpressionissue*)&Output), "outputs overlap");
	testRequire(!xcronExpressionCompile((xstrview){(cstr)&Issue, sizeof(Issue)}, XCRON_UNIX5, &Output, &Issue), "text/issue overlap");
	testRequire(!xcronExpressionCompile((xstrview){(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4}, XCRON_UNIX5, &Output, &Issue), "wrapped input");
	testRequire(!xcronExpressionCompile(XRT_STR_LITERAL("* * * * *"), XCRON_UNIX5, NULL, &Issue), "missing expression output");
	testRequire(memcmp(&Output, &Before, sizeof(Output)) == 0 && Issue.Code == XCRON_EXPRESSION_RANGE &&
		Issue.Field == XCRON_FIELD_HOUR && Issue.Offset == 77, "API misuse changed outputs");
	xrtClearError();
}

static uint32 testCronRandom(uint32* State)
{
	*State ^= *State << 13u; *State ^= *State >> 17u; *State ^= *State << 5u; return *State;
}

static void testCronProperties(void)
{
	/* Membership over every allowed value is an independent mask oracle. */
	for (unsigned First = 0; First < 60; ++First) for (unsigned Last = First; Last < 60; ++Last)
		for (unsigned Step = 1; Step <= 61; ++Step) {
			char Text[80]; int Size = snprintf(Text, sizeof(Text), "%u-%u/%u * * * *", First, Last, Step);
			xcronexpression Expression = testCronCompile((xstrview){Text, (size_t)Size}, XCRON_UNIX5); uint64 Mask = 0;
			for (unsigned Value = 0; Value < 60; ++Value)
				if (Value >= First && Value <= Last && (Value - First) % Step == 0) Mask |= testCronBit(Value);
			testRequire(Expression.Fields[1] == Mask, "exhaustive minute range/step oracle");
		}
	uint32 State = 0xe721d59u;
	static const uint32 Minimum[] = {0, 0, 0, 1, 1, 0}, Maximum[] = {59, 59, 23, 31, 12, 7};
	for (size_t Trial = 0; Trial < 10000; ++Trial) {
		char Text[512]; size_t Size = 0; uint64 Expected[6] = {1, 0, 0, 0, 0, 0}; uint32 Flags = 0;
		xcrondialect Dialect = (Trial & 1u) ? XCRON_UNIX5 : XCRON_SECONDS6;
		for (int Field = Dialect == XCRON_UNIX5 ? 1 : 0; Field < 6; ++Field) {
			unsigned Items = 1u + testCronRandom(&State) % 3u; Expected[Field] = 0;
			for (unsigned Item = 0; Item < Items; ++Item) {
				bool Star = (testCronRandom(&State) % 5u) == 0;
				uint32 First = Minimum[Field] + testCronRandom(&State) % (Maximum[Field] - Minimum[Field] + 1u);
				uint32 Last = First + testCronRandom(&State) % (Maximum[Field] - First + 1u), Step = 1u + testCronRandom(&State) % 65u;
				if (Item != 0) Text[Size++] = ',';
				if (Star) {
					First = Minimum[Field]; Last = Maximum[Field]; Size += (size_t)snprintf(Text + Size, sizeof(Text) - Size, "*/%u", Step);
					if (Field == XCRON_FIELD_DAY) Flags |= XCRON_DAY_MONTH_STAR;
					if (Field == XCRON_FIELD_WEEKDAY) Flags |= XCRON_DAY_WEEK_STAR;
				} else Size += (size_t)snprintf(Text + Size, sizeof(Text) - Size, "%u-%u/%u", First, Last, Step);
				for (uint32 Value = Minimum[Field]; Value <= Maximum[Field]; ++Value)
					if (Value >= First && Value <= Last && (Value - First) % Step == 0)
						Expected[Field] |= testCronBit(Field == XCRON_FIELD_WEEKDAY && Value == 7 ? 0 : Value);
			}
			Text[Size++] = ' ';
		}
		testRequire(Size < sizeof(Text), "random cron test buffer");
		xcronexpression Expression = testCronCompile((xstrview){Text, Size}, Dialect);
		testRequire(memcmp(Expression.Fields, Expected, sizeof(Expected)) == 0 && Expression.Flags == Flags, "random field membership oracle");
	}
	for (size_t Trial = 0; Trial < 20000; ++Trial) {
		char Text[256]; size_t Size = testCronRandom(&State) % sizeof(Text);
		for (size_t Index = 0; Index < Size; ++Index) Text[Index] = (char)testCronRandom(&State);
		xcronexpression Output, Before; xcronexpressionissue Issue; memset(&Output, 0xa5, sizeof(Output)); memcpy(&Before, &Output, sizeof(Output));
		bool Valid = xcronExpressionCompile((xstrview){Text, Size}, XCRON_SECONDS6, &Output, &Issue);
		testRequire(Issue.Offset <= Size && Issue.Field <= XCRON_FIELD_NONE, "raw input diagnostic bounds");
		if (!Valid) testRequire(memcmp(&Output, &Before, sizeof(Output)) == 0, "raw input failure changed expression");
		xrtClearError();
	}
}

static void testCronAllocation(void)
{
	xcronexpression Output; xcronexpressionissue Issue;
	xrtSetErrorKind(XERR_CANCELLED); const xerror* Previous = xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before, After; xrtMemStatsEnable(true); xrtMemStatsGet(&Before);
	#endif
	for (size_t Index = 0; Index < 10000; ++Index)
		testRequire(xcronExpressionCompile(XRT_STR_LITERAL("0,30 9-17 * JAN-MAR MON-FRI"), XCRON_UNIX5, &Output, &Issue), "hot compile");
	testRequire(xrtGetError() == Previous, "successful compile changed ambient error");
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After);
	testRequire(After.BlockAllocCalls == Before.BlockAllocCalls && After.MallocCalls == Before.MallocCalls &&
		After.CallocCalls == Before.CallocCalls && After.ReallocCalls == Before.ReallocCalls, "successful compiler allocated");
	xrtMemStatsEnable(false);
	#endif
	xrtClearError();
	xallocationbudget* Budget = NULL; xallocationscope Scope = {0}; xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xrtAllocationBudgetCreate(1, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "cron quota");
	testRequire(xcronExpressionCompile(XRT_STR_LITERAL("@hourly"), XCRON_SECONDS6, &Output, NULL), "zero-heap alias under quota");
	xcronexpression Saved; memcpy(&Saved, &Output, sizeof(Output));
	testRequire(!xcronExpressionCompile(XRT_STR_LITERAL("60 * * * *"), XCRON_UNIX5, &Output, &Issue) &&
		Issue.Code == XCRON_EXPRESSION_RANGE && Issue.Offset == 0 && xrtErrorKind(xrtGetError()) == XERR_MEMORY &&
		memcmp(&Output, &Saved, sizeof(Output)) == 0, "cron OOM lost issue or changed output");
	testRequire(xrtAllocationBudgetLeave(&Scope) && xrtAllocationBudgetInfo(Budget, &Info) && Info.LiveBytes == 0 && Info.Rejections != 0,
		"cron quota accounting");
	xrtClearError(); xrtAllocationBudgetFree(Budget);
}

int main(void)
{
	testCronKnown(); testCronInvalid(); testCronProperties(); testCronAllocation();
	printf("[PASS] xcron expression: dialects/names/aliases/steps/atomicity/111630 ranges/10k properties/20k raw/OOM\n");
	return 0;
}
