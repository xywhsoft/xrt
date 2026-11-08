#include "test.h"

typedef struct testReply {
	xredisreplytype Type;
	uint32 Flags;
	size_t Depth;
	uint64 Length;
	int64 Integer;
	bool Boolean;
	xbytesview Data;
} testReply;

typedef struct testReadRecord {
	testReply Reply;
	uint64 BeginLength;
	unsigned char Payload[512];
	size_t Bytes;
	bool Ended;
} testReadRecord;

typedef struct testReadContext {
	xredisreader* Reader;
	testReadRecord Records[128];
	size_t Open[64];
	bool Active[64];
	size_t Count, Events, Ready;
	xbytesview Input;
	bool PauseEvery, Reenter, Reentered;
	size_t FailAt;
	xredisreadaction FailureAction;
} testReadContext;

#define TEST_REPLY(Type, Flags, Depth, Length, Integer, Boolean, Text) \
	{Type, Flags, Depth, Length, Integer, Boolean, XRT_BYTES_INIT(Text)}
#define TEST_SCALAR(Type, Depth, Text) TEST_REPLY(Type, 0, Depth, sizeof(Text)-1u, 0, false, Text)
#define TEST_CONTAINER(Type, Flags, Depth, Length) TEST_REPLY(Type, Flags, Depth, Length, 0, false, "")
#define TEST_INTEGER(Depth, Value, Text) TEST_REPLY(XREDIS_REPLY_INTEGER, 0, Depth, sizeof(Text)-1u, Value, false, Text)

static xredisreadaction testReadEvent(const xredisreadevent* Event, ptr User)
{
	testReadContext* Context = (testReadContext*)User;
	++Context->Events;
	testRequire(Event != NULL && Event->Depth < 64, "reader event/depth");
	if (Context->Reenter && !Context->Reentered) {
		xredisreaderinfo Info = {0}; size_t Consumed = 77; Info.Size = sizeof(Info);
		Context->Reentered = true;
		testRequire(xredisReaderInfo(Context->Reader, &Info), "info inside callback");
		testRequire(xredisReaderFeed(Context->Reader, (xbytesview){NULL, 0}, &Consumed) == XREDIS_READER_ERROR &&
			Consumed == 77, "reentrant Feed mutated consumed");
		testRequire(!xredisReaderReset(Context->Reader) && !xredisReaderFinish(Context->Reader) &&
			!xredisReaderDestroy(Context->Reader), "callback mutation was allowed");
		testRequire(xredisReaderInfo(Context->Reader, &Info) && Info.Error == XREDIS_READER_OK,
			"callback reentry poisoned outer reader");
		xrtClearError();
	}
	if (Event->Kind == XREDIS_READ_BEGIN) {
		testRequire(!Context->Active[Event->Depth] && Context->Count < 128, "unbalanced BEGIN");
		size_t Index = Context->Count++;
		Context->Open[Event->Depth] = Index; Context->Active[Event->Depth] = true;
		testReadRecord* Record = &Context->Records[Index];
		Record->Reply.Type = Event->Type; Record->Reply.Flags = Event->Flags;
		Record->Reply.Depth = Event->Depth; Record->BeginLength = Event->Length;
	} else {
		testRequire(Context->Active[Event->Depth], "event outside active value");
		testReadRecord* Record = &Context->Records[Context->Open[Event->Depth]];
		testRequire(Record->Reply.Type == Event->Type && Record->Reply.Flags == Event->Flags, "event type changed");
		if (Event->Kind == XREDIS_READ_DATA) {
			testRequire(Event->Data.Size != 0 && Event->Data.Data != NULL &&
				(uintptr_t)Event->Data.Data >= (uintptr_t)Context->Input.Data &&
				(uintptr_t)Event->Data.Data - (uintptr_t)Context->Input.Data <= Context->Input.Size &&
				Event->Data.Size <= Context->Input.Size - ((uintptr_t)Event->Data.Data - (uintptr_t)Context->Input.Data),
				"DATA did not borrow this Feed input");
			testRequire(Event->Data.Size <= sizeof(Record->Payload) - Record->Bytes, "test payload capacity");
			memcpy(Record->Payload + Record->Bytes, Event->Data.Data, Event->Data.Size);
			Record->Bytes += Event->Data.Size;
		} else {
			testRequire(Event->Kind == XREDIS_READ_END && !Record->Ended, "unbalanced END");
			Record->Reply.Length = Event->Length; Record->Reply.Integer = Event->Integer;
			Record->Reply.Boolean = Event->Boolean; Record->Ended = true;
			Context->Active[Event->Depth] = false;
		}
	}
	if (Event->Kind != XREDIS_READ_DATA) testRequire(Event->Data.Data == NULL && Event->Data.Size == 0, "framing event data");
	if (Context->Events == Context->FailAt) return Context->FailureAction;
	return Context->PauseEvery ? XREDIS_READ_PAUSE : XREDIS_READ_NEXT;
}

static xredisreader* testReadCreate(testReadContext* Context, const xredisreaderconfig* Config)
{
	xredisreader* Reader = NULL;
	testRequire(xredisReaderCreate(Config, testReadEvent, Context, &Reader), "reader create");
	Context->Reader = Reader;
	return Reader;
}

/* Continues both byte-budget yields and callbacks paused at the last input
 * byte. Empty resumes are essential for an END callback paused at frame end. */
static void testReadPart(testReadContext* Context, xbytesview Input)
{
	size_t Offset = 0, Iterations = 0;
	xredisreadstatus Status;
	do {
		xbytesview Rest = {Input.Data == NULL ? NULL : Input.Data + Offset, Input.Size - Offset};
		Context->Input = Rest; size_t Consumed = SIZE_MAX;
		Status = xredisReaderFeed(Context->Reader, Rest, &Consumed);
		if (Status == XREDIS_READER_ERROR) {
			fprintf(stderr, "unexpected reader error: %s\n", xrtErrorMessage(xrtGetError()));
			testRequire(false, "valid reader input rejected");
		}
		testRequire(Consumed <= Rest.Size && ++Iterations < 20000, "reader progress");
		Offset += Consumed;
		if (Status == XREDIS_READER_READY) ++Context->Ready;
	} while (Offset != Input.Size || Status == XREDIS_READER_PAUSED || Status == XREDIS_READER_YIELD);
}

static void testReadExpected(testReadContext* Context, const testReply* Expected, size_t Count)
{
	testRequire(Context->Count == Count, "wrong number of values (attribute counted as parent item?)");
	for (size_t Index = 0; Index < Count; ++Index) {
		testReadRecord* Actual = &Context->Records[Index];
		const testReply* Want = &Expected[Index];
		if (Actual->Reply.Type != Want->Type || Actual->Reply.Flags != Want->Flags ||
			Actual->Reply.Depth != Want->Depth || Actual->Reply.Length != Want->Length)
			fprintf(stderr, "record %zu: type=%d flags=%u depth=%zu length=%llu, expected %d/%u/%zu/%llu\n",
				Index, Actual->Reply.Type, (unsigned)Actual->Reply.Flags, Actual->Reply.Depth,
				(unsigned long long)Actual->Reply.Length, Want->Type, (unsigned)Want->Flags,
				Want->Depth, (unsigned long long)Want->Length);
		testRequire(Actual->Ended && Actual->Reply.Type == Want->Type && Actual->Reply.Flags == Want->Flags &&
			Actual->Reply.Depth == Want->Depth && Actual->Reply.Length == Want->Length &&
			Actual->Reply.Integer == Want->Integer && Actual->Reply.Boolean == Want->Boolean &&
			Actual->Bytes == Want->Data.Size && (Actual->Bytes == 0 || memcmp(Actual->Payload, Want->Data.Data, Actual->Bytes) == 0),
			"reader event tree mismatch");
		testRequire(Actual->BeginLength == XREDIS_LENGTH_UNKNOWN || Actual->BeginLength == Want->Length, "wrong BEGIN length");
	}
	for (size_t Depth = 0; Depth < 64; ++Depth) testRequire(!Context->Active[Depth], "value remained open");
}

static void testReadFixture(xbytesview Wire, const testReply* Expected, size_t Count)
{
	for (size_t Split = 0; Split <= Wire.Size; ++Split) {
		testReadContext Context = {0}; testReadCreate(&Context, NULL);
		testReadPart(&Context, (xbytesview){Wire.Data, Split});
		testReadPart(&Context, (xbytesview){Wire.Data + Split, Wire.Size - Split});
		testReadExpected(&Context, Expected, Count);
		xredisreaderinfo Info = {0}; Info.Size = sizeof(Info);
		testRequire(Context.Ready == 1 && xredisReaderInfo(Context.Reader, &Info) &&
			Info.CompletedFrames == 1 && Info.StreamBytes == Wire.Size && Info.LastFrameBytes == Wire.Size &&
			Info.LastFrameValues == Count && Info.FrameBytes == 0 && Info.OpenContainers == 0 &&
			xredisReaderFinish(Context.Reader), "frame completion counters or EOF");
		testRequire(xredisReaderDestroy(Context.Reader), "fixture destroy");
	}
	for (size_t Budget = 1; Budget <= 3; ++Budget) {
		testReadContext Context = {0}; xredisreaderconfig Config;
		testRequire(xredisReaderConfigInit(&Config), "budget config"); Config.MaxFeedBytes = Budget;
		Config.MaxFrameBytes = Wire.Size; Context.PauseEvery = true; testReadCreate(&Context, &Config);
		for (size_t Index = 0; Index < Wire.Size; ++Index) testReadPart(&Context, (xbytesview){Wire.Data + Index, 1});
		testReadExpected(&Context, Expected, Count);
		testRequire(Context.Ready == 1 && xredisReaderFinish(Context.Reader), "bytewise/pause frame");
		testRequire(xredisReaderDestroy(Context.Reader), "bytewise destroy");
		/* Same budget, coalesced input, forcing YIELD independently of PAUSE. */
		memset(&Context, 0, sizeof(Context)); testReadCreate(&Context, &Config);
		testReadPart(&Context, Wire); testReadExpected(&Context, Expected, Count);
		testRequire(Context.Ready == 1 && xredisReaderFinish(Context.Reader), "coalesced budget frame");
		testRequire(xredisReaderDestroy(Context.Reader), "budget destroy");
	}
}

static void testReadKnown(void)
{
	static const testReply Simple[] = {TEST_SCALAR(XREDIS_REPLY_SIMPLE_STRING, 0, "OK")};
	static const testReply Empty[] = {TEST_SCALAR(XREDIS_REPLY_SIMPLE_STRING, 0, "")};
	static const testReply Error[] = {TEST_SCALAR(XREDIS_REPLY_SIMPLE_ERROR, 0, "ERR bad")};
	static const testReply Binary[] = {TEST_SCALAR(XREDIS_REPLY_BULK_STRING, 0, "a\0b\rx")};
	static const testReply Integers[] = {
		TEST_CONTAINER(XREDIS_REPLY_ARRAY, 0, 0, 3),
		TEST_INTEGER(1, INT64_MIN, "-9223372036854775808"),
		TEST_INTEGER(1, INT64_MAX, "+9223372036854775807"), TEST_INTEGER(1, 0, "-0")};
	static const testReply Nulls[] = {
		TEST_CONTAINER(XREDIS_REPLY_ARRAY, 0, 0, 3),
		TEST_CONTAINER(XREDIS_REPLY_BULK_STRING, XREDIS_REPLY_NULL_VALUE, 1, 0),
		TEST_CONTAINER(XREDIS_REPLY_ARRAY, XREDIS_REPLY_NULL_VALUE, 1, 0),
		TEST_CONTAINER(XREDIS_REPLY_NULL, XREDIS_REPLY_NULL_VALUE, 1, 0)};
	static const testReply Numbers[] = {
		TEST_CONTAINER(XREDIS_REPLY_ARRAY, 0, 0, 5), TEST_SCALAR(XREDIS_REPLY_DOUBLE, 1, "1.2E-03"),
		TEST_SCALAR(XREDIS_REPLY_DOUBLE, 1, "-inf"), TEST_SCALAR(XREDIS_REPLY_DOUBLE, 1, "inf"),
		TEST_SCALAR(XREDIS_REPLY_DOUBLE, 1, "nan"), TEST_SCALAR(XREDIS_REPLY_BIG_NUMBER, 1, "+123456789012345678901234567890")};
	static const testReply BulkError[] = {TEST_SCALAR(XREDIS_REPLY_BULK_ERROR, 0, "ERR\0bad")};
	static const testReply Verbatim[] = {TEST_SCALAR(XREDIS_REPLY_VERBATIM, 0, "txt:a\0bc")};
	static const testReply Map[] = {
		TEST_CONTAINER(XREDIS_REPLY_MAP, 0, 0, 1), TEST_SCALAR(XREDIS_REPLY_SIMPLE_STRING, 1, "k"),
		TEST_CONTAINER(XREDIS_REPLY_SET, 0, 1, 3), TEST_INTEGER(2, 1, "1"),
		TEST_REPLY(XREDIS_REPLY_BOOLEAN, 0, 2, 1, 0, false, "f"),
		TEST_REPLY(XREDIS_REPLY_BOOLEAN, 0, 2, 1, 0, true, "t")};
	static const testReply Attributes[] = {
		TEST_CONTAINER(XREDIS_REPLY_ATTRIBUTE, 0, 0, 1), TEST_SCALAR(XREDIS_REPLY_SIMPLE_STRING, 1, "ttl"), TEST_INTEGER(1, 9, "9"),
		TEST_CONTAINER(XREDIS_REPLY_ATTRIBUTE, 0, 0, 0), TEST_CONTAINER(XREDIS_REPLY_ARRAY, 0, 0, 2), TEST_INTEGER(1, 1, "1"),
		TEST_CONTAINER(XREDIS_REPLY_ATTRIBUTE, 0, 1, 1), TEST_SCALAR(XREDIS_REPLY_SIMPLE_STRING, 2, "meta"),
		TEST_SCALAR(XREDIS_REPLY_SIMPLE_STRING, 2, "yes"), TEST_INTEGER(1, 2, "2")};
	static const testReply Push[] = {
		TEST_CONTAINER(XREDIS_REPLY_PUSH, 0, 0, 3), TEST_SCALAR(XREDIS_REPLY_SIMPLE_STRING, 1, "message"),
		TEST_SCALAR(XREDIS_REPLY_BULK_STRING, 1, "c"), TEST_SCALAR(XREDIS_REPLY_BULK_STRING, 1, "x")};
	static const testReply Stream[] = {
		TEST_CONTAINER(XREDIS_REPLY_ARRAY, XREDIS_REPLY_STREAMED, 0, 2),
		TEST_CONTAINER(XREDIS_REPLY_MAP, XREDIS_REPLY_STREAMED, 1, 1), TEST_SCALAR(XREDIS_REPLY_SIMPLE_STRING, 2, "k"),
		TEST_REPLY(XREDIS_REPLY_BULK_STRING, XREDIS_REPLY_STREAMED, 2, 5, 0, false, "abc\0d"),
		TEST_CONTAINER(XREDIS_REPLY_SET, XREDIS_REPLY_STREAMED, 1, 0)};
	testReadFixture(XRT_BYTES_LITERAL("+OK\r\n"), Simple, 1);
	testReadFixture(XRT_BYTES_LITERAL("+\r\n"), Empty, 1);
	testReadFixture(XRT_BYTES_LITERAL("-ERR bad\r\n"), Error, 1);
	testReadFixture(XRT_BYTES_LITERAL("$5\r\na\0b\rx\r\n"), Binary, 1);
	testReadFixture(XRT_BYTES_LITERAL("*3\r\n:-9223372036854775808\r\n:+9223372036854775807\r\n:-0\r\n"), Integers, 4);
	testReadFixture(XRT_BYTES_LITERAL("*3\r\n$-1\r\n*-1\r\n_\r\n"), Nulls, 4);
	testReadFixture(XRT_BYTES_LITERAL("*5\r\n,1.2E-03\r\n,-inf\r\n,inf\r\n,nan\r\n(+123456789012345678901234567890\r\n"), Numbers, 6);
	testReadFixture(XRT_BYTES_LITERAL("!7\r\nERR\0bad\r\n"), BulkError, 1);
	testReadFixture(XRT_BYTES_LITERAL("=8\r\ntxt:a\0bc\r\n"), Verbatim, 1);
	testReadFixture(XRT_BYTES_LITERAL("%1\r\n+k\r\n~3\r\n:1\r\n#f\r\n#t\r\n"), Map, 6);
	testReadFixture(XRT_BYTES_LITERAL("|1\r\n+ttl\r\n:9\r\n|0\r\n*2\r\n:1\r\n|1\r\n+meta\r\n+yes\r\n:2\r\n"), Attributes, 10);
	testReadFixture(XRT_BYTES_LITERAL(">3\r\n+message\r\n$1\r\nc\r\n$1\r\nx\r\n"), Push, 4);
	testReadFixture(XRT_BYTES_LITERAL("*?\r\n%?\r\n+k\r\n$?\r\n;2\r\nab\r\n;3\r\nc\0d\r\n;0\r\n.\r\n~?\r\n.\r\n.\r\n"), Stream, 5);
}

static xredisreadererror testReadBad(const xredisreaderconfig* Config, xbytesview Wire)
{
	testReadContext Context = {0}; testReadCreate(&Context, Config);
	Context.Input = Wire; size_t Consumed = SIZE_MAX;
	xredisreadstatus Status = xredisReaderFeed(Context.Reader, Wire, &Consumed);
	testRequire(Consumed <= Wire.Size && Status != XREDIS_READER_READY && Status != XREDIS_READER_PAUSED, "bad frame completed");
	if (Status != XREDIS_READER_ERROR) testRequire(!xredisReaderFinish(Context.Reader), "bad/truncated frame accepted EOF");
	xredisreaderinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xredisReaderInfo(Context.Reader, &Info) && Info.Error != XREDIS_READER_OK && Info.ErrorOffset <= Wire.Size,
		"missing persistent parser error");
	Consumed = 77;
	testRequire(xredisReaderFeed(Context.Reader, XRT_BYTES_LITERAL("+OK\r\n"), &Consumed) == XREDIS_READER_ERROR && Consumed == 0,
		"terminal reader consumed new input");
	testRequire(xredisReaderReset(Context.Reader), "reset terminal parser");
	memset(Context.Records, 0, sizeof(Context.Records)); memset(Context.Active, 0, sizeof(Context.Active));
	Context.Count = 0; Context.Events = 0;
	/* The caller's configured limits may deliberately reject any useful frame. */
	testRequire(xredisReaderFinish(Context.Reader) && xredisReaderDestroy(Context.Reader), "reset boundary or destroy");
	xrtClearError();
	return Info.Error;
}

static void testReadMalformed(void)
{
	static const char* const Bad[] = {
		"?\r\n", ".\r\n", ";0\r\n", "+bad\n", "+x\rx", "_x\r\n", "#\r\n", "#T\r\n", "#tt\r\n",
		":\r\n", ":+\r\n", ":--1\r\n", ": 1\r\n", ":9223372036854775808\r\n", ":-9223372036854775809\r\n",
		"(\r\n", "(+\r\n", "(1.0\r\n", ",\r\n", ",.5\r\n", ",1.\r\n", ",1e\r\n", ",1e+\r\n",
		",1e--1\r\n", ",--1\r\n", ",1 0\r\n", ",INF\r\n", ",-nan\r\n", ",NAN\r\n", ",nan(x)\r\n", ",+inf\r\n",
		"$\r\n", "$+1\r\n", "$-0\r\n", "$-01\r\n", "$-2\r\n", "$9223372036854775808\r\n", "$1\r\nxX\n",
		"*+0\r\n", "*-2\r\n", "%1\r\n+k\r\n?", "%-1\r\n", "! -1\r\n", "!-1\r\n", "=-1\r\n", "=3\r\n",
		"=4\r\ntxt;\r\n", "$??\r\n", "$?\r\n?", "$?\r\n;-1\r\n", "$?\r\n;?\r\n", "!?\r\n", "=?\r\n", "|?\r\n", ">?\r\n",
		"*?\r\n.x\n", "%?\r\n:1\r\n.\r\n", "*?\r\n|0\r\n.\r\n", ">0\r\n", ">1\r\n:1\r\n", ">1\r\n$-1\r\n",
		"*1\r\n>1\r\n+x\r\n"
	};
	for (size_t Index = 0; Index < sizeof(Bad) / sizeof(Bad[0]); ++Index) {
		xredisreadererror Error = testReadBad(NULL, (xbytesview){(cbytes)Bad[Index], strlen(Bad[Index])});
		testRequire(Error == XREDIS_READER_PROTOCOL, "wrong malformed protocol diagnostic");
	}
	static const xbytesview Truncated[] = {
		XRT_BYTES_INIT("+hello\r"), XRT_BYTES_INIT("$3\r\nxy"), XRT_BYTES_INIT("*1\r\n"),
		XRT_BYTES_INIT("|0\r\n"), XRT_BYTES_INIT("*1\r\n|0\r\n"), XRT_BYTES_INIT("$?\r\n;1\r\nx\r\n"),
		XRT_BYTES_INIT("%?\r\n+k\r\n"), XRT_BYTES_INIT("*?\r\n.\r")};
	for (size_t Index = 0; Index < sizeof(Truncated) / sizeof(Truncated[0]); ++Index)
		testRequire(testReadBad(NULL, Truncated[Index]) == XREDIS_READER_TRUNCATED, "truncated EOF diagnostic");
	const xbytesview Whole = XRT_BYTES_LITERAL("|1\r\n+k\r\n+v\r\n*1\r\n$3\r\nabc\r\n");
	for (size_t Prefix = 1; Prefix < Whole.Size; ++Prefix)
		testRequire(testReadBad(NULL, (xbytesview){Whole.Data, Prefix}) == XREDIS_READER_TRUNCATED, "every truncated prefix");
}

static void testReadGoodConfig(const xredisreaderconfig* Config, xbytesview Wire)
{
	testReadContext Context = {0}; testReadCreate(&Context, Config); testReadPart(&Context, Wire);
	testRequire(Context.Ready == 1 && xredisReaderFinish(Context.Reader) && xredisReaderDestroy(Context.Reader), "config boundary success");
}

static void testReadLimits(void)
{
	xredisreaderconfig Config; testRequire(xredisReaderConfigInit(&Config), "default config");
	Config.MaxDepth = 0;
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("*-1\r\n"));
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL("*0\r\n")) == XREDIS_READER_LIMIT, "zero depth");
	Config.MaxDepth = 2;
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("*1\r\n*1\r\n:0\r\n"));
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL("*1\r\n*1\r\n*0\r\n")) == XREDIS_READER_LIMIT, "deep aggregate");
	xredisReaderConfigInit(&Config); Config.MaxFrameBytes = 3;
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("+\r\n"));
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL("+x\r\n")) == XREDIS_READER_LIMIT, "frame bytes");
	xredisReaderConfigInit(&Config); Config.MaxBulkBytes = 0;
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("$0\r\n\r\n"));
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("$?\r\n;0\r\n"));
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL("$1\r\nx\r\n")) == XREDIS_READER_LIMIT, "bulk bytes");
	Config.MaxBulkBytes = 3;
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("$?\r\n;1\r\nx\r\n;2\r\nyz\r\n;0\r\n"));
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL("$?\r\n;2\r\nxy\r\n;2\r\nzz\r\n;0\r\n")) == XREDIS_READER_LIMIT, "cumulative chunk bytes");
	xredisReaderConfigInit(&Config); Config.MaxLineBytes = 2;
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL(":12\r\n"));
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL(":123\r\n")) == XREDIS_READER_LIMIT, "scalar line bytes");
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL("$100\r\n")) == XREDIS_READER_LIMIT, "header line bytes");
	Config.MaxLineBytes = 0;
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("_\r\n"));
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL("$0\r\n")) == XREDIS_READER_LIMIT, "zero header line");
	xredisReaderConfigInit(&Config); Config.MaxContainerItems = 0;
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("*0\r\n"));
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("~?\r\n.\r\n"));
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL("*1\r\n:0\r\n")) == XREDIS_READER_LIMIT, "declared items");
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL("*?\r\n:0\r\n.\r\n")) == XREDIS_READER_LIMIT, "streamed items");
	Config.MaxContainerItems = 1;
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("%?\r\n+k\r\n+v\r\n.\r\n"));
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL("%?\r\n+k\r\n+v\r\n+k\r\n+v\r\n.\r\n")) == XREDIS_READER_LIMIT, "streamed pair items");
	xredisReaderConfigInit(&Config); Config.MaxValues = 1;
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("*0\r\n"));
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL("*1\r\n:0\r\n")) == XREDIS_READER_LIMIT, "known value lower bound");
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL("|0\r\n:0\r\n")) == XREDIS_READER_LIMIT, "attribute value budget");
	Config.MaxValues = 3;
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("*2\r\n:0\r\n:1\r\n"));
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL("*?\r\n:0\r\n:1\r\n:2\r\n.\r\n")) == XREDIS_READER_LIMIT, "streamed value budget");
	xredisReaderConfigInit(&Config); Config.Protocol = XREDIS_RESP2;
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("*2\r\n$-1\r\n*-1\r\n"));
	static const char* const Resp3[] = {"_\r\n", "#t\r\n", ",0\r\n", "(1\r\n", "!0\r\n\r\n", "=4\r\ntxt:\r\n",
		"%0\r\n", "|0\r\n:0\r\n", "~0\r\n", ">1\r\n+x\r\n", "$?\r\n;0\r\n", "*?\r\n.\r\n"};
	for (size_t Index = 0; Index < sizeof(Resp3) / sizeof(Resp3[0]); ++Index)
		testRequire(testReadBad(&Config, (xbytesview){(cbytes)Resp3[Index], strlen(Resp3[Index])}) == XREDIS_READER_PROTOCOL, "RESP2 accepted RESP3");
	xredisReaderConfigInit(&Config); Config.Flags = XREDIS_READER_LEGACY_NAN;
	testReadGoodConfig(&Config, XRT_BYTES_LITERAL("*4\r\n,-NaN\r\n,+NAN\r\n,nan(Payload_123)\r\n,nan()\r\n"));
	testRequire(testReadBad(&Config, XRT_BYTES_LITERAL(",nan(bad!)\r\n")) == XREDIS_READER_PROTOCOL, "legacy NaN invalid payload");
}

static void testReadLifecycle(void)
{
	testReadContext Context = {0}; xredisreaderconfig Config; xredisreader* Sentinel = (xredisreader*)(uintptr_t)1;
	testRequire(xredisReaderConfigInit(&Config), "config init");
	testRequire(!xredisReaderCreate(&Config, NULL, NULL, &Sentinel) && Sentinel == (xredisreader*)(uintptr_t)1, "missing callback");
	Config.Size = 0; testRequire(!xredisReaderCreate(&Config, testReadEvent, &Context, &Sentinel), "invalid config Size");
	xredisReaderConfigInit(&Config); Config.MaxDepth = SIZE_MAX;
	testRequire(!xredisReaderCreate(&Config, testReadEvent, &Context, &Sentinel), "stack size overflow");
	xredisReaderConfigInit(&Config); Config.MaxFeedBytes = 0;
	testRequire(!xredisReaderCreate(&Config, testReadEvent, &Context, &Sentinel), "zero byte work budget");
	xredisReaderConfigInit(&Config); Config.MaxValues = 0;
	testRequire(!xredisReaderCreate(&Config, testReadEvent, &Context, &Sentinel), "zero value budget");
	xredisReaderConfigInit(&Config); Config.MaxFrameBytes = 0;
	testRequire(!xredisReaderCreate(&Config, testReadEvent, &Context, &Sentinel), "zero frame budget");
	xredisReaderConfigInit(&Config); Config.Protocol = (xredisprotocol)4;
	testRequire(!xredisReaderCreate(&Config, testReadEvent, &Context, &Sentinel), "invalid protocol");
	xredisReaderConfigInit(&Config); Config.Flags = UINT32_MAX;
	testRequire(!xredisReaderCreate(&Config, testReadEvent, &Context, &Sentinel), "unknown flags");
	testRequire(Sentinel == (xredisreader*)(uintptr_t)1 && !xredisReaderConfigInit(NULL), "failure result changed");
	testReadCreate(&Context, NULL); size_t Consumed = 77;
	testRequire(xredisReaderFeed(Context.Reader, (xbytesview){NULL, 1}, &Consumed) == XREDIS_READER_ERROR && Consumed == 77, "invalid input span");
	testRequire(xredisReaderFeed(Context.Reader, (xbytesview){(cbytes)(uintptr_t)(UINTPTR_MAX - 1u), 4}, &Consumed) == XREDIS_READER_ERROR &&
		Consumed == 77, "wrapped input span");
	testRequire(xredisReaderFeed(Context.Reader, (xbytesview){(cbytes)&Consumed, sizeof(Consumed)}, &Consumed) == XREDIS_READER_ERROR &&
		Consumed == 77, "input/consumed alias");
	testRequire(xredisReaderFeed(Context.Reader, (xbytesview){(cbytes)Context.Reader, 1}, &Consumed) == XREDIS_READER_ERROR &&
		Consumed == 77, "input/reader alias");
	testRequire(xredisReaderFeed(Context.Reader, (xbytesview){NULL, 0}, (size_t*)Context.Reader) == XREDIS_READER_ERROR,
		"consumed/reader alias");
	xredisreaderinfo Info = {0};
	testRequire(!xredisReaderInfo(Context.Reader, &Info), "Info Size required"); Info.Size = sizeof(Info);
	testRequire(xredisReaderInfo(Context.Reader, &Info) && Info.Error == XREDIS_READER_OK && Info.StreamBytes == 0 &&
		xredisReaderFinish(Context.Reader), "bad API call poisoned reader");
	Context.Reenter = true; testReadPart(&Context, XRT_BYTES_LITERAL("+OK\r\n"));
	testRequire(Context.Reentered && Context.Ready == 1, "reentry outer parse did not complete");
	testRequire(xredisReaderDestroy(Context.Reader) && xredisReaderDestroy(NULL), "lifecycle destroy");
	for (size_t Event = 1; Event <= 3; ++Event) {
		memset(&Context, 0, sizeof(Context)); Context.FailAt = Event; Context.FailureAction = XREDIS_READ_FAIL;
		testReadCreate(&Context, NULL); Context.Input = XRT_BYTES_LITERAL("+OK\r\n");
		testRequire(xredisReaderFeed(Context.Reader, Context.Input, &Consumed) == XREDIS_READER_ERROR &&
			xredisReaderInfo(Context.Reader, &Info) && Info.Error == XREDIS_READER_CALLBACK, "callback failure not terminal");
		testRequire(xredisReaderReset(Context.Reader), "callback failure reset");
		memset(Context.Records, 0, sizeof(Context.Records)); memset(Context.Active, 0, sizeof(Context.Active));
		Context.Count = 0; Context.Events = 0; Context.FailAt = 0;
		testReadPart(&Context, XRT_BYTES_LITERAL("+OK\r\n")); testRequire(Context.Ready == 1, "reset callback failure did not recover");
		testRequire(xredisReaderDestroy(Context.Reader), "callback destroy");
	}
	memset(&Context, 0, sizeof(Context)); Context.FailAt = 1; Context.FailureAction = (xredisreadaction)99;
	testReadCreate(&Context, NULL); Context.Input = XRT_BYTES_LITERAL("+OK\r\n");
	testRequire(xredisReaderFeed(Context.Reader, Context.Input, &Consumed) == XREDIS_READER_ERROR &&
		xrtErrorCode(xrtGetError()) == XREDIS_READER_CALLBACK, "invalid callback action");
	xredisReaderDestroy(Context.Reader);
	memset(&Context, 0, sizeof(Context)); Context.PauseEvery = true; testReadCreate(&Context, NULL);
	Context.Input = XRT_BYTES_LITERAL("+OK\r\n"); size_t Offset = 0;
	for (size_t Event = 0; Event < 3; ++Event) {
		Context.Input = (xbytesview){(cbytes)"+OK\r\n" + Offset, 5u - Offset};
		testRequire(xredisReaderFeed(Context.Reader, Context.Input, &Consumed) == XREDIS_READER_PAUSED, "pause event");
		Offset += Consumed;
		testRequire(!xredisReaderFinish(Context.Reader) && xredisReaderInfo(Context.Reader, &Info) && Info.Error == XREDIS_READER_OK && Info.Paused,
			"paused Finish poisoned reader");
	}
	testRequire(Offset == 5 && xredisReaderFeed(Context.Reader, (xbytesview){NULL, 0}, &Consumed) == XREDIS_READER_READY &&
		Consumed == 0 && Context.Events == 3 && xredisReaderFinish(Context.Reader), "root END pause was redelivered");
	xredisReaderDestroy(Context.Reader); xrtClearError();
}

static uint32 testReadRandom(uint32* State)
{
	*State ^= *State << 13u; *State ^= *State >> 17u; *State ^= *State << 5u;
	return *State;
}

static void testReadProperties(void)
{
	uint32 State = 0xb94a517u;
	for (size_t Trial = 0; Trial < 10000; ++Trial) {
		unsigned char Wire[2048], Payload[16][32]; testReply Expected[17] = {0}; size_t Length = 0;
		size_t Count = testReadRandom(&State) % 17u; bool Streamed = (testReadRandom(&State) & 1u) != 0;
		Expected[0] = (testReply)TEST_CONTAINER(XREDIS_REPLY_ARRAY, Streamed ? XREDIS_REPLY_STREAMED : 0, 0, Count);
		Length += (size_t)snprintf((char*)Wire + Length, sizeof(Wire) - Length, Streamed ? "*?\r\n" : "*%zu\r\n", Count);
		for (size_t Index = 0; Index < Count; ++Index) {
			size_t Bytes = testReadRandom(&State) % 33u;
			for (size_t Byte = 0; Byte < Bytes; ++Byte) Payload[Index][Byte] = (unsigned char)testReadRandom(&State);
			Expected[Index + 1u] = (testReply){XREDIS_REPLY_BULK_STRING, 0, 1, Bytes, 0, false, {Payload[Index], Bytes}};
			Length += (size_t)snprintf((char*)Wire + Length, sizeof(Wire) - Length, "$%zu\r\n", Bytes);
			memcpy(Wire + Length, Payload[Index], Bytes); Length += Bytes; Wire[Length++] = '\r'; Wire[Length++] = '\n';
		}
		if (Streamed) { Wire[Length++] = '.'; Wire[Length++] = '\r'; Wire[Length++] = '\n'; }
		testReadContext Context = {0}; xredisreaderconfig Config; xredisReaderConfigInit(&Config);
		Config.MaxFeedBytes = 1u + testReadRandom(&State) % 37u; Context.PauseEvery = (Trial % 11u) == 0;
		testReadCreate(&Context, &Config);
		for (size_t Offset = 0; Offset < Length;) {
			size_t Part = 1u + testReadRandom(&State) % 71u; if (Part > Length - Offset) Part = Length - Offset;
			testReadPart(&Context, (xbytesview){Wire + Offset, Part}); Offset += Part;
		}
		testReadExpected(&Context, Expected, Count + 1u);
		testRequire(Context.Ready == 1 && xredisReaderFinish(Context.Reader) && xredisReaderDestroy(Context.Reader), "random frame boundary");
	}
	/* Coalesced ordinary replies and pushes must remain separate FIFO frames. */
	testReadContext Context = {0}; testReadCreate(&Context, NULL);
	testReadPart(&Context, XRT_BYTES_LITERAL("+one\r\n>1\r\n+invalidate\r\n|0\r\n:2\r\n$-1\r\n"));
	testRequire(Context.Ready == 4 && Context.Count == 6 && xredisReaderFinish(Context.Reader), "pipeline/push/attribute framing");
	xredisReaderDestroy(Context.Reader);
}

static void testReadAllocations(void)
{
	testReadContext Context = {0}; testReadCreate(&Context, NULL);
	xrtSetErrorKind(XERR_TIMEOUT); const xerror* Previous = xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before, After; xrtMemStatsEnable(true); xrtMemStatsGet(&Before);
	#endif
	for (size_t Iteration = 0; Iteration < 10000; ++Iteration) {
		Context.Count = 0; Context.Events = 0;
		memset(Context.Records, 0, sizeof(Context.Records));
		testReadPart(&Context, XRT_BYTES_LITERAL("*3\r\n:123\r\n$4\r\na\0bc\r\n*?\r\n#t\r\n.\r\n"));
		testRequire(xredisReaderFinish(Context.Reader), "hot Finish");
		xredisreaderinfo Info = {0}; Info.Size = sizeof(Info); testRequire(xredisReaderInfo(Context.Reader, &Info), "hot Info");
		if ((Iteration % 37u) == 0) testRequire(xredisReaderReset(Context.Reader), "hot Reset");
	}
	testRequire(xrtGetError() == Previous, "hot reader changed ambient error");
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After);
	testRequire(After.BlockAllocCalls == Before.BlockAllocCalls && After.MallocCalls == Before.MallocCalls &&
		After.CallocCalls == Before.CallocCalls && After.ReallocCalls == Before.ReallocCalls, "hot reader allocated");
	xrtMemStatsEnable(false);
	#endif
	xrtClearError();
	xallocationbudget* Budget = NULL; xallocationscope Scope = {0}; xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xrtAllocationBudgetCreate(1, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "reader OOM quota");
	xredisreader* Result = (xredisreader*)(uintptr_t)1;
	testRequire(!xredisReaderCreate(NULL, testReadEvent, &Context, &Result) && Result == (xredisreader*)(uintptr_t)1 &&
		xrtErrorKind(xrtGetError()) == XERR_MEMORY, "reader create OOM ownership");
	Context.Count = 0; Context.Events = 0; memset(Context.Records, 0, sizeof(Context.Records));
	testReadPart(&Context, XRT_BYTES_LITERAL("+OK\r\n"));
	Context.Input = XRT_BYTES_LITERAL("?"); size_t Consumed = 77;
	testRequire(xredisReaderFeed(Context.Reader, Context.Input, &Consumed) == XREDIS_READER_ERROR && Consumed == 0 &&
		xrtErrorKind(xrtGetError()) == XERR_MEMORY, "reader diagnostic OOM fallback");
	xredisreaderinfo ReaderInfo = {0}; ReaderInfo.Size = sizeof(ReaderInfo);
	testRequire(xredisReaderInfo(Context.Reader, &ReaderInfo) && ReaderInfo.Error == XREDIS_READER_PROTOCOL, "parser cause lost on diagnostic OOM");
	testRequire(xrtAllocationBudgetLeave(&Scope) && xrtAllocationBudgetInfo(Budget, &Info) && Info.LiveBytes == 0 && Info.Rejections >= 2,
		"reader OOM quota leaked");
	xrtClearError(); xrtAllocationBudgetFree(Budget); xredisReaderDestroy(Context.Reader);
	/* Charge the complete create/destroy lifecycle to a real allocation budget. */
	Budget = NULL; Scope = (xallocationscope){0};
	testRequire(xrtAllocationBudgetCreate(1024u * 1024u, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "lifecycle quota");
	for (size_t Iteration = 0; Iteration < 100; ++Iteration) {
		memset(&Context, 0, sizeof(Context)); testReadCreate(&Context, NULL);
		testReadPart(&Context, XRT_BYTES_LITERAL("$?\r\n;0\r\n")); xredisReaderDestroy(Context.Reader);
	}
	testRequire(xrtAllocationBudgetLeave(&Scope) && xrtAllocationBudgetInfo(Budget, &Info) && Info.LiveBytes == 0,
		"reader lifecycle live allocation leak");
	xrtAllocationBudgetFree(Budget);
}

/* Exercise untrusted control bytes and mutations of deep valid wire. The same
 * input must end with the same parser result/offset for coalesced vs bytewise
 * input. Small independent limits keep every mutation bounded. */
static xredisreaderinfo testReadMutationRun(xbytesview Wire, bool Bytewise)
{
	testReadContext Context = {0}; xredisreaderconfig Config; xredisReaderConfigInit(&Config);
	Config.MaxDepth = 8; Config.MaxFrameBytes = 256; Config.MaxBulkBytes = 64;
	Config.MaxLineBytes = 64; Config.MaxContainerItems = 16; Config.MaxValues = 64; Config.MaxFeedBytes = 13;
	Context.PauseEvery = Bytewise; testReadCreate(&Context, &Config);
	size_t Offset = 0, Iterations = 0; xredisreadstatus Status = XREDIS_READER_MORE;
	do {
		size_t Part = Wire.Size - Offset; if (Bytewise && Part > 1) Part = 1;
		Context.Input = (xbytesview){Wire.Data + Offset, Part}; size_t Consumed = SIZE_MAX;
		Status = xredisReaderFeed(Context.Reader, Context.Input, &Consumed);
		testRequire(Consumed <= Part && Consumed <= Config.MaxFeedBytes && ++Iterations < 4000, "mutation budget/progress");
		Offset += Consumed;
	} while (Status != XREDIS_READER_ERROR &&
		(Offset != Wire.Size || Status == XREDIS_READER_PAUSED || Status == XREDIS_READER_YIELD));
	if (Status != XREDIS_READER_ERROR) (void)xredisReaderFinish(Context.Reader);
	xredisreaderinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xredisReaderInfo(Context.Reader, &Info) && Info.StreamBytes <= Wire.Size &&
		Info.OpenContainers <= Config.MaxDepth, "mutation state bounds");
	xredisReaderDestroy(Context.Reader); xrtClearError();
	return Info;
}

static void testReadMutations(void)
{
	static const unsigned char Seed[] = "|1\r\n+ttl\r\n:5\r\n*?\r\n%1\r\n+k\r\n$?\r\n;3\r\na\0b\r\n;0\r\n~2\r\n#f\r\n,-1.2e+4\r\n.\r\n";
	uint32 State = 0xa57e316du;
	for (size_t Trial = 0; Trial < 20000; ++Trial) {
		unsigned char Wire[sizeof(Seed)]; size_t Size;
		if ((Trial % 4u) == 0) {
			Size = testReadRandom(&State) % sizeof(Wire);
			for (size_t Index = 0; Index < Size; ++Index) Wire[Index] = (unsigned char)testReadRandom(&State);
		} else {
			Size = sizeof(Seed) - 1u; memcpy(Wire, Seed, Size);
			size_t Changes = testReadRandom(&State) % 5u;
			for (size_t Change = 0; Change < Changes; ++Change)
				Wire[testReadRandom(&State) % Size] = (unsigned char)testReadRandom(&State);
			if ((Trial % 5u) == 0) Size = testReadRandom(&State) % Size;
		}
		xredisreaderinfo Whole = testReadMutationRun((xbytesview){Wire, Size}, false);
		xredisreaderinfo Split = testReadMutationRun((xbytesview){Wire, Size}, true);
		testRequire(Whole.Error == Split.Error && Whole.ErrorOffset == Split.ErrorOffset &&
			Whole.StreamBytes == Split.StreamBytes && Whole.CompletedFrames == Split.CompletedFrames,
			"fragmentation changed mutation result");
	}
}

int main(void)
{
	testReadKnown(); testReadMalformed(); testReadLimits(); testReadLifecycle();
	testReadProperties(); testReadAllocations(); testReadMutations();
	printf("[PASS] xredis reader: RESP2/3/all-splits/pause/stream/attribute/push/limits/property/OOM/lifetime\n");
	return 0;
}
