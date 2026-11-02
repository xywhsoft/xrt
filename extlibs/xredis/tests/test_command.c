#include "test.h"

static size_t testDecimal(const unsigned char** Cursor, const unsigned char* End)
{
	size_t Value = 0, Digits = 0;
	while (*Cursor != End && **Cursor >= '0' && **Cursor <= '9') {
		Value = Value * 10u + (*(*Cursor)++ - '0'); ++Digits;
	}
	testRequire(Digits != 0 && (size_t)(End - *Cursor) >= 2u &&
		(*Cursor)[0] == '\r' && (*Cursor)[1] == '\n', "invalid decimal framing");
	*Cursor += 2;
	return Value;
}

/* An independent wire reader checks the complete array and every payload. */
static void testWire(const unsigned char* Data, size_t Size,
	const xbytesview* Arguments, size_t Count)
{
	const unsigned char* Cursor = Data;
	const unsigned char* End = Data + Size;
	testRequire(Cursor != End && *Cursor++ == '*', "missing array marker");
	testRequire(testDecimal(&Cursor, End) == Count, "wrong argument count");
	for (size_t Index = 0; Index < Count; ++Index) {
		testRequire(Cursor != End && *Cursor++ == '$', "missing bulk marker");
		size_t Bytes = testDecimal(&Cursor, End);
		testRequire(Bytes == Arguments[Index].Size &&
			(size_t)(End - Cursor) >= Bytes + 2u, "wrong bulk length");
		testRequire(Bytes == 0 || memcmp(Cursor, Arguments[Index].Data, Bytes) == 0,
			"payload changed");
		Cursor += Bytes;
		testRequire(Cursor[0] == '\r' && Cursor[1] == '\n', "missing bulk terminator");
		Cursor += 2;
	}
	testRequire(Cursor == End, "trailing bytes after command");
}

static void testKnownCommands(void)
{
	static const xbytesview Ping[] = { XRT_BYTES_INIT("PING") };
	static const unsigned char Binary[] = { 'x', 0, '\r', '\n', '$', 255 };
	xbytesview Set[] = { XRT_BYTES_LITERAL("SET"), {NULL, 0}, {Binary, sizeof(Binary)} };
	unsigned char Output[128];
	size_t Size = 0, Written = 0;
	testRequire(xredisCommandSize(Ping, 1, &Size) && Size == 14u, "PING size");
	testRequire(xredisCommandWrite(Output, Size, Ping, 1, &Written) && Written == Size,
		"PING write");
	testRequire(memcmp(Output, "*1\r\n$4\r\nPING\r\n", Size) == 0, "PING exact bytes");
	testRequire(xredisCommandSize(Set, 3, &Size), "binary SET size");
	memset(Output, 0xa5, sizeof(Output));
	testRequire(xredisCommandWrite(Output, Size, Set, 3, &Written), "binary SET write");
	testRequire(Output[Size] == 0xa5, "encoder appended a NUL or overran");
	testWire(Output, Written, Set, 3);
	testRequire(xredisCommandWrite(Output, Size, Set, 3, NULL), "optional size output");
}

static void testAtomicFailures(void)
{
	unsigned char Output[128], Before[128];
	xbytesview Argument = XRT_BYTES_LITERAL("PING");
	size_t Written = 12345;
	memset(Output, 0xa5, sizeof(Output)); memcpy(Before, Output, sizeof(Output));
	testRequire(!xredisCommandWrite(Output, 13, &Argument, 1, &Written), "capacity accepted");
	testRequire(Written == 12345 && memcmp(Output, Before, sizeof(Output)) == 0,
		"short output changed");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE &&
		xrtErrorCode(xrtGetError()) == XREDIS_COMMAND_CAPACITY, "capacity diagnostic");
	testRequire(!xredisCommandSize(NULL, 1, &Written) && Written == 12345, "NULL vector");
	testRequire(!xredisCommandSize(&Argument, 0, &Written), "empty command");
	testRequire(!xredisCommandSize(&Argument, 1, NULL), "NULL size output");
	testRequire(!xredisCommandWrite(NULL, sizeof(Output), &Argument, 1, &Written), "NULL wire output");
	Argument.Data = NULL; Argument.Size = 1;
	testRequire(!xredisCommandSize(&Argument, 1, &Written), "NULL nonempty argument");
	Argument.Data = Output; Argument.Size = 4;
	testRequire(!xredisCommandWrite(Output, sizeof(Output), &Argument, 1, &Written), "payload overlap");
	testRequire(xrtErrorCode(xrtGetError()) == XREDIS_COMMAND_OVERLAP, "overlap diagnostic");
	testRequire(memcmp(Output, Before, sizeof(Output)) == 0, "alias output changed");
	Argument = XRT_BYTES_LITERAL("PING");
	testRequire(!xredisCommandWrite(&Argument, sizeof(Output), &Argument, 1, &Written), "descriptor overlap");
	testRequire(Argument.Size == 4u, "descriptor changed");
	testRequire(!xredisCommandSize(&Argument, 1, &Argument.Size), "size aliases descriptor");
	union { size_t Size; unsigned char Data[128]; } Aliased;
	memset(&Aliased, 0xa5, sizeof(Aliased));
	testRequire(!xredisCommandWrite(Aliased.Data, sizeof(Aliased.Data), &Argument, 1,
		&Aliased.Size), "size aliases wire output");
	for (size_t Index = 0; Index < sizeof(Aliased); ++Index)
		testRequire(((unsigned char*)&Aliased)[Index] == 0xa5, "aliased output changed");
	Argument.Data = (const unsigned char*)(uintptr_t)(UINTPTR_MAX - 1u); Argument.Size = 4;
	testRequire(!xredisCommandSize(&Argument, 1, &Written), "wrapped input address");
	Argument = XRT_BYTES_LITERAL("PING");
	testRequire(!xredisCommandWrite((ptr)(uintptr_t)(UINTPTR_MAX - 1u), 128,
		&Argument, 1, &Written), "wrapped output address");
	testRequire(!xredisCommandSize(&Argument, SIZE_MAX, &Written), "oversized count");
	testRequire(!xredisCommandSize((const xbytesview*)(uintptr_t)(UINTPTR_MAX - 1u),
		1, &Written), "wrapped descriptor address");
	if (SIZE_MAX > (uint64)INT64_MAX) {
		Argument.Size = (size_t)INT64_MAX + 1u;
		testRequire(!xredisCommandSize(&Argument, 1, &Written), "wire length exceeds int64");
	}
	xbytesview Huge[] = { {(const unsigned char*)1, SIZE_MAX / 2u},
		{(const unsigned char*)1, SIZE_MAX / 2u} };
	testRequire(!xredisCommandSize(Huge, 2, &Written), "encoded size overflow");
	testRequire(Written == 12345, "failure changed Written");
	xrtClearError();
}

static uint32 testRandom(uint32* State)
{
	*State ^= *State << 13u; *State ^= *State >> 17u; *State ^= *State << 5u;
	return *State;
}

static void testRandomCommands(void)
{
	unsigned char Data[16][64], Output[2048];
	xbytesview Arguments[16];
	uint32 State = 0x4c7751u;
	for (size_t Trial = 0; Trial < 10000u; ++Trial) {
		size_t Count = 1u + testRandom(&State) % 16u, Size, Written;
		for (size_t Index = 0; Index < Count; ++Index) {
			size_t Bytes = testRandom(&State) % 65u;
			for (size_t Byte = 0; Byte < Bytes; ++Byte)
				Data[Index][Byte] = (unsigned char)testRandom(&State);
			Arguments[Index] = (xbytesview){Bytes == 0 ? NULL : Data[Index], Bytes};
		}
		testRequire(xredisCommandSize(Arguments, Count, &Size) && Size < sizeof(Output), "random size");
		memset(Output, 0xa5, sizeof(Output));
		testRequire(xredisCommandWrite(Output, Size, Arguments, Count, &Written) &&
			Written == Size && Output[Size] == 0xa5, "random bounded write");
		testWire(Output, Written, Arguments, Count);
	}
	/* Cover transitions in both the array count and bulk length decimal width. */
	xbytesview Many[101]; unsigned char Large[1001], BigOutput[2048];
	memset(Large, 0x5a, sizeof(Large));
	for (size_t Index = 0; Index < 101; ++Index) Many[Index] = (xbytesview){NULL, 0};
	for (size_t Count = 9; Count <= 101; ++Count) {
		size_t Written;
		testRequire(xredisCommandWrite(BigOutput, sizeof(BigOutput), Many, Count, &Written), "decimal count");
		testWire(BigOutput, Written, Many, Count);
	}
	for (size_t Bytes = 0; Bytes <= sizeof(Large); ++Bytes) {
		xbytesview Argument = {Large, Bytes}; size_t Written;
		testRequire(xredisCommandWrite(BigOutput, sizeof(BigOutput), &Argument, 1, &Written), "decimal length");
		testWire(BigOutput, Written, &Argument, 1);
	}
}

static void testAllocationContract(void)
{
	xbytesview Argument = XRT_BYTES_LITERAL("PING");
	unsigned char Output[32]; size_t Written;
	xrtSetErrorKind(XERR_CANCELLED);
	const xerror* Previous = xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before, After;
	xrtMemStatsEnable(true); xrtMemStatsGet(&Before);
	#endif
	for (size_t Index = 0; Index < 10000u; ++Index)
		testRequire(xredisCommandWrite(Output, sizeof(Output), &Argument, 1, &Written), "hot write");
	testRequire(xrtGetError() == Previous, "successful command changed ambient error");
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After);
	testRequire(After.BlockAllocCalls == Before.BlockAllocCalls &&
		After.MallocCalls == Before.MallocCalls && After.CallocCalls == Before.CallocCalls &&
		After.ReallocCalls == Before.ReallocCalls, "hot encoder allocated");
	xrtMemStatsEnable(false);
	#endif
	xrtClearError();
}

static void testDiagnosticOom(void)
{
	xallocationbudget* Budget = NULL;
	xallocationscope Scope = {0};
	xallocationbudgetinfo Info = {0};
	xbytesview Argument = XRT_BYTES_LITERAL("PING");
	unsigned char Output[32], Before[32]; size_t Written = 0;
	Info.Size = sizeof(Info);
	memset(Output, 0xa5, sizeof(Output));
	testRequire(xrtAllocationBudgetCreate(1u, &Budget), "diagnostic quota create");
	testRequire(xrtAllocationBudgetEnter(Budget, &Scope), "diagnostic quota enter");
	testRequire(xredisCommandWrite(Output, sizeof(Output), &Argument, 1, &Written),
		"encoding under one-byte quota");
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xredisCommandWrite(Output, 1, &Argument, 1, &Written),
		"diagnostic OOM capacity failure");
	testRequire(Written == 14u && memcmp(Output, Before, sizeof(Output)) == 0,
		"diagnostic OOM changed outputs");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "diagnostic OOM fallback");
	testRequire(xrtAllocationBudgetLeave(&Scope), "diagnostic quota leave");
	testRequire(xrtAllocationBudgetInfo(Budget, &Info) && Info.LiveBytes == 0 &&
		Info.Rejections != 0, "diagnostic quota accounting");
	xrtClearError(); xrtAllocationBudgetFree(Budget);
}

int main(void)
{
	testKnownCommands(); testAtomicFailures(); testRandomCommands(); testAllocationContract();
	testDiagnosticOom();
	printf("[PASS] xredis command: exact/binary/overlap/overflow/property/allocation contracts\n");
	return 0;
}
