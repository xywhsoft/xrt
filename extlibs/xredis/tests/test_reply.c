#include "test.h"
#include <math.h>

typedef struct testOwnedNode {
	xredisreplytype Type;
	uint32 Flags;
	uint64 Length;
	int64 Integer;
	bool Boolean;
	xbytesview Data;
	size_t Children, First, Next, Attributes;
} testOwnedNode;

static xredisreply* testOwnedParse(xbytesview Wire, const xredisreplyreaderconfig* Config,
	size_t Split, bool Bytewise)
{
	xredisreplyreader* Reader = NULL;
	testRequire(xredisReplyReaderCreate(Config, &Reader), "owned create");
	xredisreply* Reply = NULL;
	size_t Offset = 0, Iterations = 0;
	do {
		unsigned char Fragment[8192];
		size_t Count = Offset < Split ? Split - Offset : Wire.Size - Offset;
		if (Bytewise && Count > 1) Count = 1;
		if (Count > sizeof(Fragment)) Count = sizeof(Fragment);
		memcpy(Fragment, Wire.Data + Offset, Count);
		size_t Consumed = SIZE_MAX;
		xredisreply* Output = (xredisreply*)(uintptr_t)1;
		xredisreplystatus Status = xredisReplyReaderFeed(Reader, (xbytesview){Fragment, Count}, &Consumed, &Output);
		memset(Fragment, 0xa5, sizeof(Fragment));
		testRequire(++Iterations < 100000 && Consumed <= Count && Status >= XREDIS_REPLY_MORE,
			"owned valid fragment failed/bounds");
		if (Config != NULL) testRequire(Consumed <= Config->Reader.MaxFeedBytes, "owned feed budget");
		Offset += Consumed;
		if (Status == XREDIS_REPLY_READY) {
			testRequire(Output != (xredisreply*)(uintptr_t)1 && Output != NULL && Offset == Wire.Size,
				"owned early/incomplete publication"); Reply = Output;
		} else testRequire(Output == (xredisreply*)(uintptr_t)1 && (Consumed != 0 || Count == 0), "owned output/progress");
	} while (Reply == NULL);
	xredisreplyreaderinfo State = {0}; State.Size = sizeof(State);
	testRequire(xredisReplyReaderInfo(Reader, &State) && State.Error == XREDIS_REPLY_OK &&
		State.Reader.CompletedFrames == 1 && State.PartialNodes == 0 && State.PartialReservedBytes == 0 &&
		xredisReplyReaderFinish(Reader), "owned complete state");
	testRequire(xredisReplyReaderReset(Reader), "owned reset after transfer");
	xredisReplyReaderDestroy(Reader);
	return Reply;
}

static void testOwnedCompare(xredisreply* Reply, size_t Root, const testOwnedNode* Expected, size_t Count)
{
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xredisReplyInfo(Reply, &Info) && Info.Root == Root && Info.Nodes == Count &&
		Info.ReservedBytes <= Info.PeakBytes, "owned frame metadata");
	size_t Payload = 0;
	for (size_t Index = 0; Index < Count; ++Index) {
		xredisreplyvalue Value;
		testRequire(xredisReplyGet(Reply, Index, &Value), "owned node get");
		const testOwnedNode* Want = &Expected[Index];
		if (!(Value.Type == Want->Type && Value.Flags == Want->Flags && Value.Length == Want->Length &&
			Value.Integer == Want->Integer && Value.Boolean == Want->Boolean && Value.Data.Size == Want->Data.Size &&
			(Value.Data.Size == 0 ? Value.Data.Data == NULL : memcmp(Value.Data.Data, Want->Data.Data, Value.Data.Size) == 0) &&
			Value.ChildCount == Want->Children && Value.FirstChild == Want->First && Value.Next == Want->Next &&
			Value.Attributes == Want->Attributes)) {
			fprintf(stderr, "owned node mismatch at %zu type %d next %zu attrs %zu children %zu\n",
				Index, (int)Value.Type, Value.Next, Value.Attributes, Value.ChildCount);
			testRequire(false, "owned exact node mismatch");
		}
		Payload += Value.Data.Size;
	}
	testRequire(Payload == Info.PayloadBytes, "owned payload accounting");
}

#define TN XREDIS_REPLY_NONE
#define TV(Type,Flags,Length,Integer,Boolean,Text,Children,First,Next,Attrs) \
	{XREDIS_REPLY_##Type,Flags,Length,Integer,Boolean,XRT_BYTES_LITERAL(Text),Children,First,Next,Attrs}

static void testOwnedFixture(xbytesview Wire, size_t Root, const testOwnedNode* Nodes, size_t Count)
{
	for (size_t Split = 0; Split <= Wire.Size; ++Split) {
		xredisreply* Reply = testOwnedParse(Wire, NULL, Split, false);
		testOwnedCompare(Reply, Root, Nodes, Count); xredisReplyRelease(Reply);
	}
	for (size_t Budget = 1; Budget <= 3; ++Budget) {
		xredisreplyreaderconfig Config; xredisReplyReaderConfigInit(&Config); Config.Reader.MaxFeedBytes = Budget;
		xredisreply* Reply = testOwnedParse(Wire, &Config, 0, false);
		testOwnedCompare(Reply, Root, Nodes, Count); xredisReplyRelease(Reply);
	}
	xredisreply* Reply = testOwnedParse(Wire, NULL, 0, true);
	testOwnedCompare(Reply, Root, Nodes, Count); xredisReplyRelease(Reply);
}

static void testOwnedKnown(void)
{
	const xbytesview Scalars = XRT_BYTES_LITERAL("*14\r\n+OK\r\n-ERR\r\n:-9223372036854775808\r\n$5\r\na\0b\rx\r\n"
		"$-1\r\n*-1\r\n_\r\n#t\r\n,1.2E-03\r\n(+123456789012345678901234567890\r\n!7\r\nERR\0bad\r\n"
		"=8\r\ntxt:a\0bc\r\n%2\r\n+same\r\n:1\r\n+same\r\n:2\r\n~2\r\n#f\r\n:7\r\n");
	const testOwnedNode All[] = {
		TV(ARRAY,0,14,0,false,"",14,1,TN,TN),
		TV(SIMPLE_STRING,0,2,0,false,"OK",0,TN,2,TN),
		TV(SIMPLE_ERROR,0,3,0,false,"ERR",0,TN,3,TN),
		TV(INTEGER,0,20,INT64_MIN,false,"-9223372036854775808",0,TN,4,TN),
		TV(BULK_STRING,0,5,0,false,"a\0b\rx",0,TN,5,TN),
		TV(BULK_STRING,XREDIS_REPLY_NULL_VALUE,0,0,false,"",0,TN,6,TN),
		TV(ARRAY,XREDIS_REPLY_NULL_VALUE,0,0,false,"",0,TN,7,TN),
		TV(NULL,XREDIS_REPLY_NULL_VALUE,0,0,false,"",0,TN,8,TN),
		TV(BOOLEAN,0,1,0,true,"t",0,TN,9,TN),
		TV(DOUBLE,0,7,0,false,"1.2E-03",0,TN,10,TN),
		TV(BIG_NUMBER,0,31,0,false,"+123456789012345678901234567890",0,TN,11,TN),
		TV(BULK_ERROR,0,7,0,false,"ERR\0bad",0,TN,12,TN),
		TV(VERBATIM,0,8,0,false,"txt:a\0bc",0,TN,13,TN),
		TV(MAP,0,2,0,false,"",4,14,18,TN),
		TV(SIMPLE_STRING,0,4,0,false,"same",0,TN,15,TN),
		TV(INTEGER,0,1,1,false,"1",0,TN,16,TN),
		TV(SIMPLE_STRING,0,4,0,false,"same",0,TN,17,TN),
		TV(INTEGER,0,1,2,false,"2",0,TN,TN,TN),
		TV(SET,0,2,0,false,"",2,19,TN,TN),
		TV(BOOLEAN,0,1,0,false,"f",0,TN,20,TN),
		TV(INTEGER,0,1,7,false,"7",0,TN,TN,TN)
	};
	testOwnedFixture(Scalars, 0, All, sizeof(All) / sizeof(All[0]));
	const testOwnedNode Attrs[] = {
		TV(ATTRIBUTE,0,1,0,false,"",2,1,3,TN), TV(SIMPLE_STRING,0,3,0,false,"ttl",0,TN,2,TN),
		TV(INTEGER,0,1,9,false,"9",0,TN,TN,TN), TV(ATTRIBUTE,0,0,0,false,"",0,TN,TN,TN),
		TV(ARRAY,0,2,0,false,"",2,5,TN,0), TV(INTEGER,0,1,1,false,"1",0,TN,9,TN),
		TV(ATTRIBUTE,0,1,0,false,"",2,7,TN,TN), TV(SIMPLE_STRING,0,4,0,false,"meta",0,TN,8,TN),
		TV(SIMPLE_STRING,0,3,0,false,"yes",0,TN,TN,TN), TV(INTEGER,0,1,2,false,"2",0,TN,TN,6)
	};
	testOwnedFixture(XRT_BYTES_LITERAL("|1\r\n+ttl\r\n:9\r\n|0\r\n*2\r\n:1\r\n|1\r\n+meta\r\n+yes\r\n:2\r\n"),
		4, Attrs, sizeof(Attrs) / sizeof(Attrs[0]));
	const testOwnedNode Nested[] = {
		TV(ATTRIBUTE,0,1,0,false,"",2,2,TN,TN), TV(ATTRIBUTE,0,0,0,false,"",0,TN,TN,TN),
		TV(SIMPLE_STRING,0,1,0,false,"k",0,TN,4,1), TV(ATTRIBUTE,0,0,0,false,"",0,TN,TN,TN),
		TV(SIMPLE_STRING,0,1,0,false,"v",0,TN,TN,3), TV(ARRAY,0,1,0,false,"",1,8,TN,0),
		TV(ATTRIBUTE,0,0,0,false,"",0,TN,7,TN), TV(ATTRIBUTE,0,0,0,false,"",0,TN,TN,TN),
		TV(INTEGER,0,1,3,false,"3",0,TN,TN,6)
	};
	testOwnedFixture(XRT_BYTES_LITERAL("|1\r\n|0\r\n+k\r\n|0\r\n+v\r\n*1\r\n|0\r\n|0\r\n:3\r\n"),
		5, Nested, sizeof(Nested) / sizeof(Nested[0]));
	const testOwnedNode Streams[] = {
		TV(ARRAY,XREDIS_REPLY_STREAMED,3,0,false,"",3,1,TN,TN),
		TV(BULK_STRING,XREDIS_REPLY_STREAMED,5,0,false,"abc\0d",0,TN,2,TN),
		TV(MAP,XREDIS_REPLY_STREAMED,1,0,false,"",2,3,5,TN), TV(SIMPLE_STRING,0,1,0,false,"k",0,TN,4,TN),
		TV(INTEGER,0,1,7,false,"7",0,TN,TN,TN), TV(SET,XREDIS_REPLY_STREAMED,0,0,false,"",0,TN,TN,TN)
	};
	testOwnedFixture(XRT_BYTES_LITERAL("*?\r\n$?\r\n;2\r\nab\r\n;3\r\nc\0d\r\n;0\r\n%?\r\n+k\r\n:7\r\n.\r\n~?\r\n.\r\n.\r\n"),
		0, Streams, sizeof(Streams) / sizeof(Streams[0]));
	const testOwnedNode Push[] = {
		TV(ATTRIBUTE,0,0,0,false,"",0,TN,TN,TN), TV(PUSH,0,3,0,false,"",3,2,TN,0),
		TV(SIMPLE_STRING,0,7,0,false,"message",0,TN,3,TN), TV(BULK_STRING,0,1,0,false,"c",0,TN,4,TN),
		TV(BULK_STRING,0,1,0,false,"x",0,TN,TN,TN)
	};
	testOwnedFixture(XRT_BYTES_LITERAL("|0\r\n>3\r\n+message\r\n$1\r\nc\r\n$1\r\nx\r\n"), 1, Push, 5);
	const testOwnedNode Empty[] = {TV(SIMPLE_STRING,0,0,0,false,"",0,TN,TN,TN)};
	testOwnedFixture(XRT_BYTES_LITERAL("+\r\n"), 0, Empty, 1);
	xredisreplyreaderconfig Config; xredisReplyReaderConfigInit(&Config); Config.Reader.Protocol = XREDIS_RESP2;
	Config.Reader.MaxDepth = 0;
	xredisreply* Reply = testOwnedParse(XRT_BYTES_LITERAL("+\r\n"), &Config, 0, true);
	testOwnedCompare(Reply, 0, Empty, 1); xredisReplyRelease(Reply);
}

static void testOwnedNumbers(void)
{
	const xbytesview Wire = XRT_BYTES_LITERAL("*9\r\n,-0\r\n,inf\r\n,-inf\r\n,nan\r\n,1.2345678901234567\r\n"
		",5e-324\r\n,1e-99999999\r\n,1e99999999\r\n,1.7976931348623157e308\r\n");
	xredisreply* Reply = testOwnedParse(Wire, NULL, 0, true); double Number = 77;
	testRequire(xredisReplyDouble(Reply, 1, &Number) && Number == 0.0 && signbit(Number), "owned negative zero");
	testRequire(xredisReplyDouble(Reply, 2, &Number) && isinf(Number) && Number > 0, "owned inf");
	testRequire(xredisReplyDouble(Reply, 3, &Number) && isinf(Number) && Number < 0, "owned negative inf");
	testRequire(xredisReplyDouble(Reply, 4, &Number) && isnan(Number), "owned nan");
	testRequire(xredisReplyDouble(Reply, 5, &Number) && Number == 1.2345678901234567, "owned rounded decimal");
	testRequire(xredisReplyDouble(Reply, 6, &Number) && Number > 0.0 && Number < 1e-323, "owned subnormal");
	testRequire(xredisReplyDouble(Reply, 7, &Number) && Number == 0.0, "owned underflow"); Number = 77;
	testRequire(!xredisReplyDouble(Reply, 8, &Number) && Number == 77 && xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"owned finite overflow must preserve raw reply/output");
	testRequire(xredisReplyDouble(Reply, 9, &Number) && Number == 1.7976931348623157e308, "owned largest double");
	xredisReplyRelease(Reply); xrtClearError();
	xredisreplyreaderconfig Config; xredisReplyReaderConfigInit(&Config); Config.Reader.Flags = XREDIS_READER_LEGACY_NAN;
	Reply = testOwnedParse(XRT_BYTES_LITERAL("*3\r\n,-NaN(payload_123)\r\n,+NAN()\r\n,NAN\r\n"), &Config, 0, true);
	for (size_t Index = 1; Index <= 3; ++Index)
		testRequire(xredisReplyDouble(Reply, Index, &Number) && isnan(Number) && (Index != 1 || signbit(Number)), "owned legacy nan");
	xredisReplyRelease(Reply);
}

static size_t testOwnedLargeWire(unsigned char* Wire, size_t Capacity)
{
	size_t Used = 0;
	const char* Prefix = "*32\r\n"; memcpy(Wire, Prefix, strlen(Prefix)); Used += strlen(Prefix);
	for (size_t Index = 0; Index < 32; ++Index) {
		const char* Header = "$80\r\n"; size_t Bytes = strlen(Header);
		testRequire(Capacity - Used >= Bytes + 82u, "owned fixture capacity");
		memcpy(Wire + Used, Header, Bytes); Used += Bytes;
		for (size_t Byte = 0; Byte < 80; ++Byte) Wire[Used++] = (unsigned char)(Index * 80u + Byte);
		Wire[Used++] = '\r'; Wire[Used++] = '\n';
	}
	return Used;
}

static void testOwnedGrowth(void)
{
	unsigned char Wire[4096]; size_t Size = testOwnedLargeWire(Wire, sizeof(Wire));
	xredisreply* Whole = testOwnedParse((xbytesview){Wire, Size}, NULL, 0, false);
	xredisreply* Bytewise = testOwnedParse((xbytesview){Wire, Size}, NULL, 0, true);
	xredisreplyinfo A = {0}, B = {0}; A.Size = sizeof(A); B.Size = sizeof(B);
	testRequire(xredisReplyInfo(Whole, &A) && xredisReplyInfo(Bytewise, &B) && A.Nodes == 33 &&
		A.PayloadBytes == 2560 && A.ReservedBytes == B.ReservedBytes && A.PeakBytes == B.PeakBytes,
		"owned known-bulk growth differs by fragmentation");
	for (size_t Index = 0; Index < A.Nodes; ++Index) {
		xredisreplyvalue First, Second; xredisReplyGet(Whole, Index, &First); xredisReplyGet(Bytewise, Index, &Second);
		testRequire(First.Type == Second.Type && First.ChildCount == Second.ChildCount && First.Data.Size == Second.Data.Size &&
			(First.Data.Size == 0 || memcmp(First.Data.Data, Second.Data.Data, First.Data.Size) == 0), "owned growth payload mismatch");
		if (Index != 0) for (size_t Byte = 0; Byte < 80; ++Byte)
			testRequire(First.Data.Data[Byte] == (unsigned char)((Index - 1u) * 80u + Byte), "owned binary copy corrupted");
	}
	xredisReplyRelease(Whole); xredisReplyRelease(Bytewise);
	/* A large unknown-length line and streamed chunks force payload relocation. */
	Wire[0] = '+'; memset(Wire + 1, 'z', 3000); Wire[3001] = '\r'; Wire[3002] = '\n';
	Bytewise = testOwnedParse((xbytesview){Wire, 3003}, NULL, 0, true);
	xredisreplyvalue Value; xredisReplyGet(Bytewise, 0, &Value);
	testRequire(Value.Data.Size == 3000 && Value.Length == 3000, "owned unknown length");
	for (size_t Byte = 0; Byte < 3000; ++Byte) testRequire(Value.Data.Data[Byte] == 'z', "owned relocated bytes");
	xredisReplyRelease(Bytewise);
}

static void testOwnedDepth(void)
{
	unsigned char Wire[8192]; size_t Bytes = 0;
	const size_t Depth = 512;
	for (size_t Index = 0; Index < Depth; ++Index) {
		memcpy(Wire + Bytes, "|0\r\n*1\r\n", 8); Bytes += 8;
	}
	memcpy(Wire + Bytes, "+OK\r\n", 5); Bytes += 5;
	xredisreplyreaderconfig Config; xredisReplyReaderConfigInit(&Config); Config.Reader.MaxDepth = Depth;
	for (size_t Fragment = 0; Fragment < 2; ++Fragment) {
		xredisreply* Reply = testOwnedParse((xbytesview){Wire, Bytes}, &Config, 0, Fragment != 0);
		xredisreplyinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(xredisReplyInfo(Reply, &Info) && Info.Root == 1 && Info.Nodes == Depth * 2u + 1u, "owned deep count/root");
		for (size_t Level = 0; Level < Depth; ++Level) {
			xredisreplyvalue Value, Attribute;
			testRequire(xredisReplyGet(Reply, Level * 2u, &Attribute) && Attribute.Type == XREDIS_REPLY_ATTRIBUTE &&
				Attribute.ChildCount == 0 && Attribute.Next == TN && Attribute.Attributes == TN, "owned deep attribute");
			size_t Child = Level + 1u == Depth ? Depth * 2u : Level * 2u + 3u;
			testRequire(xredisReplyGet(Reply, Level * 2u + 1u, &Value) && Value.Type == XREDIS_REPLY_ARRAY && Value.ChildCount == 1 &&
				Value.FirstChild == Child && Value.Next == TN && Value.Attributes == Level * 2u, "owned deep lazy levels");
		}
		xredisreplyvalue Leaf; xredisReplyGet(Reply, Depth * 2u, &Leaf);
		testRequire(Leaf.Type == XREDIS_REPLY_SIMPLE_STRING && Leaf.Data.Size == 2 && memcmp(Leaf.Data.Data, "OK", 2) == 0 &&
			Leaf.Attributes == TN && Leaf.Next == TN, "owned deep leaf"); xredisReplyRelease(Reply);
	}
}

static void testOwnedErrors(void)
{
	const xbytesview Bad[] = {XRT_BYTES_LITERAL("$3\r\nabc\rX"), XRT_BYTES_LITERAL("*?\r\n|0\r\n.\r\n"),
		XRT_BYTES_LITERAL("%?\r\n+k\r\n.\r\n"), XRT_BYTES_LITERAL("#x\r\n"), XRT_BYTES_LITERAL(">1\r\n:2\r\n")};
	xredisreplyreader* Reader = NULL; xredisReplyReaderCreate(NULL, &Reader);
	for (size_t Index = 0; Index < sizeof(Bad) / sizeof(Bad[0]); ++Index) {
		size_t Used = 999; xredisreply* Reply = (xredisreply*)(uintptr_t)1;
		testRequire(xredisReplyReaderFeed(Reader, Bad[Index], &Used, &Reply) == XREDIS_REPLY_ERROR && Used <= Bad[Index].Size &&
			Reply == (xredisreply*)(uintptr_t)1, "owned malformed output atomicity");
		xredisreplyreaderinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(xredisReplyReaderInfo(Reader, &Info) && Info.Error == XREDIS_REPLY_PROTOCOL &&
			Info.PartialNodes == 0 && Info.PartialReservedBytes == 0, "owned malformed retained partial");
		Used = 99;
		testRequire(xredisReplyReaderFeed(Reader, XRT_BYTES_LITERAL("+OK\r\n"), &Used, &Reply) == XREDIS_REPLY_ERROR && Used == 0 &&
			!xredisReplyReaderFinish(Reader), "owned terminal error resumed");
		xredisReplyReaderReset(Reader); xrtClearError();
	}
	const xbytesview Partial[] = {XRT_BYTES_LITERAL("|0\r\n"), XRT_BYTES_LITERAL("$3\r\nab"), XRT_BYTES_LITERAL("*1\r\n")};
	for (size_t Index = 0; Index < sizeof(Partial) / sizeof(Partial[0]); ++Index) {
		size_t Used; xredisreply* Reply = (xredisreply*)(uintptr_t)1;
		testRequire(xredisReplyReaderFeed(Reader, Partial[Index], &Used, &Reply) == XREDIS_REPLY_MORE &&
			Reply == (xredisreply*)(uintptr_t)1 && !xredisReplyReaderFinish(Reader), "owned truncated publication");
		xredisreplyreaderinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(xredisReplyReaderInfo(Reader, &Info) && Info.Error == XREDIS_REPLY_TRUNCATED && Info.PartialReservedBytes == 0,
			"owned EOF cause/cleanup"); xredisReplyReaderReset(Reader); xrtClearError();
	}
	xredisReplyReaderDestroy(Reader);
	/* Ambient diagnostics may run out of memory without losing a protocol cause. */
	xredisReplyReaderCreate(NULL, &Reader);
	xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
	testRequire(xrtAllocationBudgetCreate(1, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "owned protocol diagnostic quota");
	size_t Used = 99; xredisreply* Reply = (xredisreply*)(uintptr_t)1;
	testRequire(xredisReplyReaderFeed(Reader, XRT_BYTES_LITERAL("?"), &Used, &Reply) == XREDIS_REPLY_ERROR && Used == 0 &&
		Reply == (xredisreply*)(uintptr_t)1 && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "owned protocol diagnostic fallback");
	xredisreplyreaderinfo State = {0}; State.Size = sizeof(State);
	testRequire(xredisReplyReaderInfo(Reader, &State) && State.Error == XREDIS_REPLY_PROTOCOL &&
		State.Reader.Error == XREDIS_READER_PROTOCOL, "owned protocol cause lost under diagnostic OOM");
	xrtClearError(); xallocationbudgetinfo BudgetInfo = {0}; BudgetInfo.Size = sizeof(BudgetInfo);
	testRequire(xrtAllocationBudgetLeave(&Scope) && xrtAllocationBudgetInfo(Budget, &BudgetInfo) && BudgetInfo.LiveBytes == 0,
		"owned protocol diagnostic quota leak"); xrtAllocationBudgetFree(Budget); xredisReplyReaderDestroy(Reader);
}

static void testOwnedLimits(void)
{
	xredisreply* Small = testOwnedParse(XRT_BYTES_LITERAL("+OK\r\n"), NULL, 0, false);
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisReplyInfo(Small, &Info);
	const size_t HeaderBytes = Info.ReservedBytes;
	testRequire(HeaderBytes == Info.PeakBytes, "owned inline scalar allocated vectors"); xredisReplyRelease(Small);
	for (size_t Trial = 0; Trial < 7; ++Trial) {
		xredisreplyreaderconfig Config; xredisReplyReaderConfigInit(&Config);
		xbytesview Wire = XRT_BYTES_LITERAL("+OK\r\n");
		if (Trial == 0) Config.MaxOwnedBytes = HeaderBytes - 1u;
		if (Trial == 1) { Config.MaxOwnedBytes = HeaderBytes; Wire = XRT_BYTES_LITERAL("*1\r\n:1\r\n"); }
		if (Trial == 2) { Config.MaxOwnedBytes = HeaderBytes; Wire = XRT_BYTES_LITERAL("$65\r\n"); }
		if (Trial == 3) Config.Reader.MaxFrameBytes = 4;
		if (Trial == 4) { Config.Reader.MaxValues = 1; Wire = XRT_BYTES_LITERAL("|0\r\n+OK\r\n"); }
		if (Trial == 5) { Config.Reader.MaxDepth = 0; Wire = XRT_BYTES_LITERAL("*1\r\n:1\r\n"); }
		if (Trial == 6) { Config.Reader.MaxBulkBytes = 1; Wire = XRT_BYTES_LITERAL("$2\r\nOK\r\n"); }
		xredisreplyreader* Reader = NULL; xredisReplyReaderCreate(&Config, &Reader);
		size_t Used = 999; xredisreply* Output = (xredisreply*)(uintptr_t)1;
		testRequire(xredisReplyReaderFeed(Reader, Wire, &Used, &Output) == XREDIS_REPLY_ERROR && Output == (xredisreply*)(uintptr_t)1,
			"owned limit did not reject");
		xredisreplyreaderinfo State = {0}; State.Size = sizeof(State);
		testRequire(xredisReplyReaderInfo(Reader, &State) && State.Error == XREDIS_REPLY_LIMIT &&
			State.PeakOwnedBytes <= Config.MaxOwnedBytes && State.PartialReservedBytes == 0, "owned limit state/budget");
		xredisReplyReaderDestroy(Reader); xrtClearError();
	}
	xredisreplyreaderconfig Config; xredisReplyReaderConfigInit(&Config); Config.MaxOwnedBytes = HeaderBytes;
	Small = testOwnedParse(XRT_BYTES_LITERAL("+OK\r\n"), &Config, 0, true); xredisReplyRelease(Small);
}

static void testOwnedLifecycle(void)
{
	xredisreplyreader* Reader = NULL; xredisReplyReaderCreate(NULL, &Reader);
	xredisreply* Reply = (xredisreply*)(uintptr_t)1; size_t Used = 99;
	testRequire(xredisReplyReaderFeed(Reader, (xbytesview){NULL, 1}, &Used, &Reply) == XREDIS_REPLY_ERROR && Used == 99 &&
		Reply == (xredisreply*)(uintptr_t)1, "owned invalid API output mutation");
	testRequire(xredisReplyReaderFeed(Reader, XRT_BYTES_LITERAL("+OK\r\n"), (size_t*)Reader, &Reply) == XREDIS_REPLY_ERROR,
		"owned reader alias accepted");
	union {size_t Used; xredisreply* Reply;} Alias;
	testRequire(xredisReplyReaderFeed(Reader, XRT_BYTES_LITERAL("+OK\r\n"), &Alias.Used, &Alias.Reply) == XREDIS_REPLY_ERROR,
		"owned output/output alias accepted");
	unsigned char Overlap[64] = "+OK\r\n";
	testRequire(xredisReplyReaderFeed(Reader, (xbytesview){Overlap, 5}, (size_t*)Overlap, &Reply) == XREDIS_REPLY_ERROR,
		"owned input/output alias accepted");
	testRequire(xredisReplyReaderFeed(Reader, (xbytesview){(cbytes)(uintptr_t)(UINTPTR_MAX - 1u), 3}, &Used, &Reply) == XREDIS_REPLY_ERROR,
		"owned wrapped input accepted");
	xredisreplyreaderinfo State = {0}; State.Size = sizeof(State);
	testRequire(!xredisReplyReaderInfo(Reader, (xredisreplyreaderinfo*)Reader), "owned reader info alias accepted");
	testRequire(xredisReplyReaderInfo(Reader, &State) && State.Error == XREDIS_REPLY_OK && State.Reader.StreamBytes == 0,
		"owned invalid API poisoned reader"); xrtClearError();
	const xbytesview Frames = XRT_BYTES_LITERAL("+one\r\n|0\r\n>1\r\n+event\r\n:3\r\n");
	xredisreply* Replies[3] = {0}; size_t Offset = 0;
	for (size_t Index = 0; Index < 3; ++Index) {
		testRequire(xredisReplyReaderFeed(Reader, (xbytesview){Frames.Data + Offset, Frames.Size - Offset}, &Used, &Replies[Index]) ==
			XREDIS_REPLY_READY, "owned coalesced frames"); Offset += Used;
	}
	testRequire(Offset == Frames.Size && xredisReplyReaderFinish(Reader), "owned frame consumption");
	xredisReplyReaderDestroy(Reader); xredisReplyReaderDestroy(NULL);
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisReplyInfo(Replies[1], &Info);
	xredisreplyvalue Value; testRequire(xredisReplyGet(Replies[1], Info.Root, &Value) && Value.Type == XREDIS_REPLY_PUSH &&
		Value.Attributes == 0, "owned push lifetime/metadata");
	testRequire(xredisReplyRetain(Replies[0]), "owned retain"); xredisReplyRelease(Replies[0]);
	testRequire(xredisReplyGet(Replies[0], 0, &Value) && Value.Data.Size == 3 && memcmp(Value.Data.Data, "one", 3) == 0,
		"owned retained payload");
	xredisreplyvalue Before; memset(&Before, 0x5a, sizeof(Before)); Value = Before;
	testRequire(!xredisReplyGet(Replies[0], SIZE_MAX, &Value) && memcmp(&Before, &Value, sizeof(Value)) == 0,
		"owned invalid get mutated output");
	testRequire(!xredisReplyGet(Replies[0], 0, (xredisreplyvalue*)Replies[0]), "owned immutable header alias");
	testRequire(!xredisReplyInfo(Replies[0], (xredisreplyinfo*)Replies[0]), "owned immutable info alias");
	double Number = 17; testRequire(!xredisReplyDouble(Replies[0], 0, &Number) && Number == 17, "owned wrong number type");
	testRequire(!xredisReplyRetain(NULL) && !xredisReplyReaderReset(NULL) && !xredisReplyReaderFinish(NULL), "owned NULL API");
	xredisReplyRelease(NULL);
	for (size_t Index = 0; Index < 3; ++Index) xredisReplyRelease(Replies[Index]);
	xrtClearError();
	xredisreplyreaderconfig Config; xredisReplyReaderConfigInit(&Config);
	for (size_t Trial = 0; Trial < 4; ++Trial) {
		xredisreplyreaderconfig Bad = Config;
		if (Trial == 0) Bad.Size = 0;
		if (Trial == 1) Bad.MaxOwnedBytes = 0;
		if (Trial == 2) Bad.Reader.MaxDepth = SIZE_MAX;
		if (Trial == 3) Bad.Reader.Protocol = (xredisprotocol)7;
		Reader = (xredisreplyreader*)(uintptr_t)1;
		testRequire(!xredisReplyReaderCreate(&Bad, &Reader) && Reader == (xredisreplyreader*)(uintptr_t)1, "owned bad create atomicity");
		xrtClearError();
	}
}

static void testOwnedAllocations(void)
{
	xredisreplyreader* Reader = NULL; xredisReplyReaderCreate(NULL, &Reader);
	xrtSetErrorKind(XERR_TIMEOUT); const xerror* Previous = xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before, After; xrtMemStatsEnable(true); xrtMemStatsGet(&Before);
	#endif
	for (size_t Index = 0; Index < 1000; ++Index) {
		xredisreply* Reply = NULL; size_t Used;
		testRequire(xredisReplyReaderFeed(Reader, XRT_BYTES_LITERAL(",1.25\r\n"), &Used, &Reply) == XREDIS_REPLY_READY, "owned warm feed");
		xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisreplyvalue Value; double Number;
		testRequire(xredisReplyInfo(Reply, &Info) && xredisReplyGet(Reply, Info.Root, &Value) &&
			xredisReplyDouble(Reply, Info.Root, &Number) && Number == 1.25 && xredisReplyRetain(Reply), "owned warm access");
		xredisReplyRelease(Reply); xredisReplyRelease(Reply);
		testRequire(xredisReplyReaderFinish(Reader), "owned warm finish");
	}
	testRequire(xrtGetError() == Previous, "owned warm changed ambient error");
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After);
	testRequire(After.CallocCalls - Before.CallocCalls == 1000 && After.MallocCalls == Before.MallocCalls &&
		After.ReallocCalls == Before.ReallocCalls && After.FreeCalls - Before.FreeCalls == 1000,
		"owned scalar needs exactly one allocation/free"); xrtMemStatsEnable(false);
	#endif
	xrtClearError();
	unsigned char Wire[4096]; size_t Size = testOwnedLargeWire(Wire, sizeof(Wire));
	size_t Failures = 0, Successes = 0;
	for (size_t Limit = 1; Limit < 18000; Limit += 73) {
		xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
		testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "owned OOM quota");
		xredisreply* Reply = (xredisreply*)(uintptr_t)1; size_t Used;
		xredisreplystatus Status = xredisReplyReaderFeed(Reader, (xbytesview){Wire, Size}, &Used, &Reply);
		if (Status == XREDIS_REPLY_READY) { ++Successes; xredisReplyRelease(Reply); }
		else {
			++Failures; xredisreplyreaderinfo Info = {0}; Info.Size = sizeof(Info);
			testRequire(Status == XREDIS_REPLY_ERROR && Reply == (xredisreply*)(uintptr_t)1 &&
				xredisReplyReaderInfo(Reader, &Info) && Info.Error == XREDIS_REPLY_MEMORY && Info.PartialReservedBytes == 0,
				"owned OOM lost cause or ownership");
		}
		xredisReplyReaderReset(Reader); xrtClearError();
		xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(xrtAllocationBudgetLeave(&Scope) && xrtAllocationBudgetInfo(Budget, &Info) && Info.LiveBytes == 0,
			"owned OOM leaked frame/diagnostic"); xrtAllocationBudgetFree(Budget);
	}
	testRequire(Failures != 0 && Successes != 0, "owned quota sweep failed to cross allocation boundary");
	xredisReplyReaderDestroy(Reader);
	Failures = 0; Successes = 0;
	for (size_t Limit = 1; Limit < 16000; Limit += 127) {
		xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
		testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "owned create quota");
		Reader = (xredisreplyreader*)(uintptr_t)1;
		if (xredisReplyReaderCreate(NULL, &Reader)) { ++Successes; xredisReplyReaderDestroy(Reader); }
		else { ++Failures; testRequire(Reader == (xredisreplyreader*)(uintptr_t)1, "owned failed creation ownership"); }
		xrtClearError(); xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(xrtAllocationBudgetLeave(&Scope) && xrtAllocationBudgetInfo(Budget, &Info) && Info.LiveBytes == 0,
			"owned create quota leak"); xrtAllocationBudgetFree(Budget);
	}
	testRequire(Failures != 0 && Successes != 0, "owned create quota sweep did not cross boundary");
}

typedef struct testOwnedThread {
	xredisreply* Reply;
	uint64 Checksum;
} testOwnedThread;

static int32 testOwnedThreadRun(ptr User)
{
	testOwnedThread* Context = (testOwnedThread*)User;
	for (size_t Iteration = 0; Iteration < 20000; ++Iteration) {
		xredisreplyvalue Value;
		if (!xredisReplyRetain(Context->Reply) || !xredisReplyGet(Context->Reply, 0, &Value) || Value.Data.Size != 5) return 1;
		Context->Checksum += Value.Data.Data[Iteration % 5u]; xredisReplyRelease(Context->Reply);
	}
	xredisReplyRelease(Context->Reply); return 0;
}

static void testOwnedThreads(void)
{
	xredisreply* Reply = testOwnedParse(XRT_BYTES_LITERAL("$5\r\na\0b\rc\r\n"), NULL, 0, true);
	testOwnedThread Contexts[4] = {{0}}; xthread* Threads[4];
	for (size_t Index = 0; Index < 4; ++Index) {
		testRequire(xredisReplyRetain(Reply), "owned thread handoff reference"); Contexts[Index].Reply = Reply;
		Threads[Index] = xrtThreadCreate(testOwnedThreadRun, &Contexts[Index], 0); testRequire(Threads[Index] != NULL, "owned thread create");
	}
	xredisReplyRelease(Reply);
	for (size_t Index = 0; Index < 4; ++Index) {
		testRequire(xrtThreadWait(Threads[Index]) == XWAIT_OK && xrtThreadExitCode(Threads[Index]) == 0 &&
			Contexts[Index].Checksum == 1228000u, "owned concurrent immutable access/refcount"); xrtThreadDestroy(Threads[Index]);
	}
}

int main(void)
{
	testOwnedKnown(); testOwnedNumbers(); testOwnedGrowth(); testOwnedDepth(); testOwnedErrors(); testOwnedLimits();
	testOwnedLifecycle(); testOwnedAllocations(); testOwnedThreads();
	printf("[PASS] xredis owned reply: types/tree/attributes/push/streams/all-splits/copy/growth/limits/OOM/threads\n");
	return 0;
}
