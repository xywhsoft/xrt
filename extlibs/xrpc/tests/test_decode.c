#include "test.h"

static bool rpcText(xstrview Actual, cstr Expected)
{
	size_t Bytes = strlen(Expected);
	return Actual.Size == Bytes && (Bytes == 0 || memcmp(Actual.Data, Expected, Bytes) == 0);
}

static xrpcdocument rpcRead(cstr Wire, xrpcdecodemode Mode, xrpcmessage* Messages, size_t Capacity)
{
	xrpcdecodeconfig Config; xrpcDecodeConfigInit(&Config); Config.Mode = Mode; xrpcdocument Document;
	xrpcdecodestatus Status = xrpcDecode((xstrview){Wire, strlen(Wire)}, &Config, Messages, Capacity, &Document);
	if (Status != XRPC_DECODE_DONE) fprintf(stderr, "RPC decode status=%d for %s\n", Status, Wire);
	testRequire(Status == XRPC_DECODE_DONE, "valid JSON RPC decode failed");
	return Document;
}

static void testRpcKnown(void)
{
	xrpcmessage Messages[16]; xrpcdocument Doc;
	Doc = rpcRead("{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"params\":[1,true],\"id\":123456789012345678901234567890}", XRPC_DECODE_REQUESTS, Messages, 16);
	testRequire(Doc.Count == 1 && !Doc.Batch && Doc.InvalidCount == 0 && Messages[0].Kind == XRPC_MESSAGE_CALL &&
		Messages[0].Id.Kind == XRPC_VALUE_NUMBER && rpcText(Messages[0].Id.Raw, "123456789012345678901234567890") &&
		rpcText(Messages[0].Method.Raw, "\"echo\"") && rpcText(Messages[0].Params.Raw, "[1,true]"), "request fields lost");
	Doc = rpcRead("[{\"jsonrpc\":\"2.0\",\"method\":\"n\"},{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":null},1,[],{\"jsonrpc\":\"2.0\",\"method\":\"x\",\"id\":\"a\\u0000b\"}]", XRPC_DECODE_REQUESTS, Messages, 16);
	testRequire(Doc.Batch && Doc.Count == 5 && Doc.InvalidCount == 2 && Messages[0].Kind == XRPC_MESSAGE_NOTIFICATION &&
		Messages[0].Id.Kind == XRPC_VALUE_MISSING && Messages[1].Kind == XRPC_MESSAGE_CALL && Messages[1].Id.Kind == XRPC_VALUE_NULL &&
		Messages[2].Reason == XRPC_INVALID_OBJECT && Messages[3].Reason == XRPC_INVALID_OBJECT && rpcText(Messages[3].Wire, "[]") &&
		rpcText(Messages[4].Id.Raw, "\"a\\u0000b\""), "batch/null/notification/invalid item");
	Doc = rpcRead("[]", XRPC_DECODE_REQUESTS, Messages, 1);
	testRequire(!Doc.Batch && Doc.Count == 1 && Doc.InvalidCount == 1 && Messages[0].Reason == XRPC_INVALID_EMPTY_BATCH, "empty batch shape");
	Doc = rpcRead("null", XRPC_DECODE_REQUESTS, Messages, 1);
	testRequire(!Doc.Batch && Doc.Count == 1 && Messages[0].Reason == XRPC_INVALID_OBJECT && rpcText(Messages[0].Wire, "null"), "scalar root");
	Doc = rpcRead("{\"json\\u0072pc\":\"\\u0032.\\u0030\",\"metho\\u0064\":\"\\u0072pc.test\",\"id\":1.25e9999,\"params\":{\"x\":{\"id\":false}}}", XRPC_DECODE_DUPLEX, Messages, 1);
	testRequire(Doc.InvalidCount == 0 && Messages[0].Flags == XRPC_METHOD_RESERVED && rpcText(Messages[0].Id.Raw, "1.25e9999") &&
		rpcText(Messages[0].Params.Raw, "{\"x\":{\"id\":false}}"), "escaped protocol names or arbitrary number ID");
	Doc = rpcRead("[{\"jsonrpc\":\"2.0\",\"result\":null,\"id\":\"a\"},{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"no\",\"data\":{\"code\":1}},\"id\":1e9999}]", XRPC_DECODE_RESPONSES, Messages, 16);
	testRequire(Doc.InvalidCount == 0 && Messages[0].Kind == XRPC_MESSAGE_RESULT && Messages[0].Result.Kind == XRPC_VALUE_NULL &&
		Messages[1].Kind == XRPC_MESSAGE_ERROR && rpcText(Messages[1].ErrorCode.Raw, "-32601") &&
		rpcText(Messages[1].ErrorMessage.Raw, "\"no\"") && rpcText(Messages[1].ErrorData.Raw, "{\"code\":1}"), "response/error fields");
	Doc = rpcRead("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"result\":0,\"error\":false,\"id\":0}", XRPC_DECODE_REQUESTS, Messages, 1);
	testRequire(Doc.InvalidCount == 0 && Messages[0].Kind == XRPC_MESSAGE_CALL, "request mode extra fields");
	Doc = rpcRead("{\"jsonrpc\":\"2.0\",\"result\":0,\"method\":false,\"params\":false,\"id\":0}", XRPC_DECODE_RESPONSES, Messages, 1);
	testRequire(Doc.InvalidCount == 0 && Messages[0].Kind == XRPC_MESSAGE_RESULT, "response mode extra fields");
	/* Scratch capacity is based on the actual document, not the default batch cap. */
	Doc = rpcRead("{\"jsonrpc\":\"2.0\",\"method\":\"m\"}", XRPC_DECODE_REQUESTS, Messages, 1);
	testRequire(Doc.Count == 1, "single scratch with default message cap");
}

static void testRpcInvalid(void)
{
	static const struct { cstr Wire; xrpcinvalidreason Reason; } Cases[] = {
		{"{}", XRPC_INVALID_VERSION}, {"{\"jsonrpc\":2,\"method\":\"m\"}", XRPC_INVALID_VERSION},
		{"{\"jsonrpc\":\"2.0\",\"method\":0}", XRPC_INVALID_METHOD},
		{"{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":true}", XRPC_INVALID_ID},
		{"{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":[]}", XRPC_INVALID_ID},
		{"{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"params\":null}", XRPC_INVALID_PARAMS},
		{"{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1,\"\\u0069d\":2}", XRPC_INVALID_DUPLICATE},
		{"{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"result\":0,\"id\":1}", XRPC_INVALID_AMBIGUOUS},
		{"{\"jsonrpc\":\"2.0\",\"result\":0}", XRPC_INVALID_RESPONSE},
		{"{\"jsonrpc\":\"2.0\",\"result\":0,\"error\":{},\"id\":1}", XRPC_INVALID_RESPONSE},
		{"{\"jsonrpc\":\"2.0\",\"error\":null,\"id\":1}", XRPC_INVALID_ERROR},
		{"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":1},\"id\":1}", XRPC_INVALID_ERROR},
		{"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":1.5,\"message\":\"m\"},\"id\":1}", XRPC_INVALID_ERROR},
		{"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":1,\"co\\u0064e\":2,\"message\":\"m\"},\"id\":1}", XRPC_INVALID_ERROR}
	};
	xrpcmessage Message;
	for (size_t Index = 0; Index < sizeof(Cases)/sizeof(Cases[0]); ++Index) {
		xrpcdocument Doc = rpcRead(Cases[Index].Wire, XRPC_DECODE_DUPLEX, &Message, 1);
		testRequire(Doc.Count == 1 && Doc.InvalidCount == 1 && Message.Kind == XRPC_MESSAGE_INVALID && Message.Reason == Cases[Index].Reason,
			"wrong RPC invalid-envelope reason");
	}
	static const cstr Parse[] = {"", " ", "[", "{", "{\"jsonrpc\":\"2.0\",\"method\":\"m\"} junk", "[{\"jsonrpc\":\"2.0\",\"method\":\"m\"},]", "{\"x\":\"\\uD800\"}", "/*c*/{}"};
	for (size_t Index = 0; Index < sizeof(Parse)/sizeof(Parse[0]); ++Index) {
		xrpcmessage Scratch[2]; xrpcdocument Doc = {true, 99, 99};
		testRequire(xrpcDecode((xstrview){Parse[Index], strlen(Parse[Index])}, NULL, Scratch, 2, &Doc) == XRPC_DECODE_PARSE_ERROR &&
			!Doc.Batch && Doc.Count == 0 && Doc.InvalidCount == 0, "malformed frame published prior messages");
		xrtClearError();
	}
}

static void testRpcIntegral(void)
{
	static const cstr Integer[] = {"0", "-0", "1", "1.0", "100e-2", "1.5e1", "0.000e-99999999999999999999999999", "1e999999999999999999999999999999", "123456789012345678901234567890"};
	static const cstr Fraction[] = {"1.1", "100e-3", "0.1", "1e-999999999999999999999999999999", "1.50e-1"};
	for (unsigned Pass = 0; Pass < 2; ++Pass) {
		const cstr* Values = Pass == 0 ? Integer : Fraction;
		size_t Count = Pass == 0 ? sizeof(Integer)/sizeof(Integer[0]) : sizeof(Fraction)/sizeof(Fraction[0]);
		for (size_t Index = 0; Index < Count; ++Index) {
			char Wire[256]; snprintf(Wire, sizeof(Wire), "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":%s,\"message\":\"m\"},\"id\":null}", Values[Index]);
			xrpcmessage Message; xrpcdocument Doc = rpcRead(Wire, XRPC_DECODE_RESPONSES, &Message, 1);
			testRequire((Pass == 0 && Doc.InvalidCount == 0 && Message.Kind == XRPC_MESSAGE_ERROR) ||
				(Pass == 1 && Doc.InvalidCount == 1 && Message.Reason == XRPC_INVALID_ERROR), "integral error code grammar");
		}
	}
}

static void rpcLocal(xstrview Wire, const xrpcdecodeconfig* Config, xrpcmessage* Scratch, size_t Capacity, xrpccodecerror Code)
{
	xrpcdocument Doc = {true, 77, 88};
	testRequire(xrpcDecode(Wire, Config, Scratch, Capacity, &Doc) == XRPC_DECODE_LOCAL_ERROR && Doc.Batch && Doc.Count == 77 &&
		Doc.InvalidCount == 88 && xrtErrorCode(xrtGetError()) == (int32)Code, "local limit or document atomicity");
	xrtClearError();
}

static void testRpcLimits(void)
{
	xrpcdecodeconfig Config; xrpcDecodeConfigInit(&Config); xrpcmessage Scratch[4];
	xstrview Batch = XRT_STR_LITERAL("[{},{}]");
	rpcLocal(Batch, NULL, Scratch, 1, XRPC_CODEC_CAPACITY);
	Config.MaxMessages = 1; rpcLocal(Batch, &Config, Scratch, 4, XRPC_CODEC_LIMIT);
	xrpcDecodeConfigInit(&Config); Config.MaxInputBytes = 1; rpcLocal(Batch, &Config, Scratch, 4, XRPC_CODEC_LIMIT);
	xrpcDecodeConfigInit(&Config); Config.MaxDepth = 1; rpcLocal(XRT_STR_LITERAL("[{\"x\":0}]"), &Config, Scratch, 4, XRPC_CODEC_LIMIT);
	xrpcDecodeConfigInit(&Config); Config.MaxValues = 1; rpcLocal(Batch, &Config, Scratch, 4, XRPC_CODEC_LIMIT);
	xrpcDecodeConfigInit(&Config); Config.MaxContainerItems = 1; rpcLocal(Batch, &Config, Scratch, 4, XRPC_CODEC_LIMIT);
	xrpcDecodeConfigInit(&Config); Config.MaxStringBytes = 1; rpcLocal(XRT_STR_LITERAL("{\"xx\":0}"), &Config, Scratch, 4, XRPC_CODEC_LIMIT);
	xrpcDecodeConfigInit(&Config); Config.MaxIdBytes = 2; rpcLocal(XRT_STR_LITERAL("{\"id\":100}"), &Config, Scratch, 4, XRPC_CODEC_LIMIT);
	xrpcDecodeConfigInit(&Config); Config.MaxMethodBytes = 2; rpcLocal(XRT_STR_LITERAL("{\"method\":\"abc\"}"), &Config, Scratch, 4, XRPC_CODEC_LIMIT);
	xrpcDecodeConfigInit(&Config); Config.Size = 0; rpcLocal(Batch, &Config, Scratch, 4, XRPC_CODEC_ARGUMENT);
	rpcLocal((xstrview){NULL, 1}, NULL, Scratch, 4, XRPC_CODEC_ARGUMENT);
	rpcLocal((xstrview){(cstr)(uintptr_t)(UINTPTR_MAX-1u), 4}, NULL, Scratch, 4, XRPC_CODEC_ARGUMENT);
	rpcLocal((xstrview){(cstr)Scratch, sizeof(Scratch)}, NULL, Scratch, 4, XRPC_CODEC_ARGUMENT);
	rpcLocal(Batch, NULL, Scratch, SIZE_MAX, XRPC_CODEC_ARGUMENT);
	rpcLocal(Batch, NULL, Scratch, 0, XRPC_CODEC_ARGUMENT);
	testRequire(!xrpcDecodeConfigInit(NULL), "NULL config init"); xrtClearError();
}

static void testRpcAllocation(void)
{
	xrpcmessage Messages[3]; xrpcdocument Doc;
	xstrview Wire = XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"ec\\u0068o\",\"id\":1e9999,\"params\":[\"\\u0061\"]},{\"jsonrpc\":\"2.0\",\"method\":\"n\"}]");
	xrtSetErrorKind(XERR_TIMEOUT); const xerror* Previous = xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before, After; xrtMemStatsEnable(true); xrtMemStatsGet(&Before);
	#endif
	for (size_t Index = 0; Index < 10000; ++Index)
		testRequire(xrpcDecode(Wire, NULL, Messages, 3, &Doc) == XRPC_DECODE_DONE && Doc.Count == 2, "hot RPC decode");
	testRequire(xrtGetError() == Previous, "successful RPC decode changed ambient error");
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After);
	testRequire(After.BlockAllocCalls == Before.BlockAllocCalls && After.MallocCalls == Before.MallocCalls &&
		After.CallocCalls == Before.CallocCalls && After.ReallocCalls == Before.ReallocCalls, "RPC decoder allocated");
	xrtMemStatsEnable(false);
	#endif
	xrtClearError(); xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
	testRequire(xrtAllocationBudgetCreate(1, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "RPC quota");
	testRequire(xrpcDecode(Wire, NULL, Messages, 3, &Doc) == XRPC_DECODE_DONE, "RPC decoding under quota");
	Doc = (xrpcdocument){true,77,88};
	testRequire(xrpcDecode(XRT_STR_LITERAL("[1,]"), NULL, Messages, 3, &Doc) == XRPC_DECODE_LOCAL_ERROR &&
		Doc.Count == 77 && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "parse diagnostic OOM misclassified");
	testRequire(xrtAllocationBudgetLeave(&Scope), "RPC quota leave");
	xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xrtAllocationBudgetInfo(Budget, &Info) && Info.LiveBytes == 0 && Info.Rejections != 0, "RPC quota leak");
	xrtClearError(); xrtAllocationBudgetFree(Budget);
}

static uint32 rpcRandom(uint32* State)
{
	*State ^= *State << 13u; *State ^= *State >> 17u; *State ^= *State << 5u; return *State;
}

static void rpcSpan(xstrview Input, xstrview Part)
{
	testRequire(Part.Data != NULL && (uintptr_t)Part.Data >= (uintptr_t)Input.Data &&
		(uintptr_t)Part.Data - (uintptr_t)Input.Data <= Input.Size &&
		Part.Size <= Input.Size - ((uintptr_t)Part.Data - (uintptr_t)Input.Data), "RPC view not borrowed from input");
}

static void rpcViews(xstrview Input, const xrpcmessage* Message)
{
	rpcSpan(Input, Message->Wire);
	const xrpcvalue* Values[] = {&Message->Id, &Message->Method, &Message->Params, &Message->Result,
		&Message->Error, &Message->ErrorCode, &Message->ErrorMessage, &Message->ErrorData};
	for (size_t Index = 0; Index < sizeof(Values)/sizeof(Values[0]); ++Index) {
		if (Values[Index]->Kind != XRPC_VALUE_MISSING) rpcSpan(Input, Values[Index]->Raw);
		else testRequire(Values[Index]->Raw.Data == NULL && Values[Index]->Raw.Size == 0, "missing RPC value retained data");
	}
}

static void testRpcProperties(void)
{
	uint32 State = 0x13579bdfu;
	static const cstr Ids[] = {"null", "1e9999", "-0", "1.25", "\"a\\u0000b\""};
	for (size_t Trial = 0; Trial < 10000; ++Trial) {
		char Wire[2048]; size_t Size = 0, Count = 1u + rpcRandom(&State) % 6u, Invalid = 0;
		xrpcmessagekind Kinds[6]; cstr ExpectedId[6] = {0}; Wire[Size++] = '[';
		for (size_t Item = 0; Item < Count; ++Item) {
			if (Item) Wire[Size++] = ',';
			unsigned Choice = rpcRandom(&State) % 6u; int Bytes;
			cstr Version = (Trial & 1u) ? "\"jsonrpc\":\"2.0\"" : "\"json\\u0072pc\":\"\\u0032.0\"";
			if (Choice == 0) {
				ExpectedId[Item] = Ids[rpcRandom(&State) % 5u]; Kinds[Item] = XRPC_MESSAGE_CALL;
				Bytes = snprintf(Wire+Size, sizeof(Wire)-Size, "{%s,\"method\":\"m\",\"params\":[{\"id\":true}],\"id\":%s}", Version, ExpectedId[Item]);
			} else if (Choice == 1) {
				Kinds[Item] = XRPC_MESSAGE_NOTIFICATION; Bytes = snprintf(Wire+Size, sizeof(Wire)-Size, "{%s,\"method\":\"n\"}", Version);
			} else if (Choice == 2) {
				ExpectedId[Item] = Ids[rpcRandom(&State) % 5u]; Kinds[Item] = XRPC_MESSAGE_RESULT;
				Bytes = snprintf(Wire+Size, sizeof(Wire)-Size, "{%s,\"result\":[\"\\u0000\",{}],\"id\":%s}", Version, ExpectedId[Item]);
			} else if (Choice == 3) {
				ExpectedId[Item] = "0"; Kinds[Item] = XRPC_MESSAGE_ERROR;
				Bytes = snprintf(Wire+Size, sizeof(Wire)-Size, "{%s,\"id\":0,\"error\":{\"message\":\"e\",\"code\":1.0e3,\"data\":[{}]}}", Version);
			} else {
				Kinds[Item] = XRPC_MESSAGE_INVALID; ++Invalid;
				Bytes = snprintf(Wire+Size, sizeof(Wire)-Size, Choice == 4 ? "{%s,\"method\":false}" : "false", Version);
			}
			testRequire(Bytes > 0 && (size_t)Bytes < sizeof(Wire)-Size, "RPC property buffer"); Size += (size_t)Bytes;
		}
		Wire[Size++] = ']'; xrpcmessage Messages[8]; xrpcdocument Doc;
		testRequire(xrpcDecode((xstrview){Wire, Size}, NULL, Messages, 8, &Doc) == XRPC_DECODE_DONE &&
			Doc.Batch && Doc.Count == Count && Doc.InvalidCount == Invalid, "generated batch shape");
		for (size_t Item = 0; Item < Count; ++Item) {
			testRequire(Messages[Item].Kind == Kinds[Item], "generated message classification"); rpcViews((xstrview){Wire,Size}, &Messages[Item]);
			if (ExpectedId[Item]) testRequire(rpcText(Messages[Item].Id.Raw, ExpectedId[Item]), "generated id changed");
		}
	}
	/* An independent exact rational oracle checks error-code integrality. */
	for (size_t Trial = 0; Trial < 10000; ++Trial) {
		unsigned Coefficient = rpcRandom(&State) % 1000001u, Fraction = rpcRandom(&State) % 7u;
		int Exponent = (int)(rpcRandom(&State) % 17u) - 8;
		uint64 Scale = 1; for (unsigned Digit = 0; Digit < Fraction; ++Digit) Scale *= 10u;
		char Number[80], Wire[256];
		if (Fraction) snprintf(Number, sizeof(Number), "%s%u.%0*uE%+d", (Trial&1u)?"-":"", (unsigned)(Coefficient/Scale), (int)Fraction, (unsigned)(Coefficient%Scale), Exponent);
		else snprintf(Number, sizeof(Number), "%s%uE%+d", (Trial&1u)?"-":"", Coefficient, Exponent);
		bool Integral = true;
		if (Exponent < (int)Fraction) {
			uint64 Divisor = 1; for (int Digit = 0; Digit < (int)Fraction-Exponent; ++Digit) Divisor *= 10u;
			Integral = Coefficient % Divisor == 0;
		}
		snprintf(Wire, sizeof(Wire), "{\"jsonrpc\":\"2.0\",\"id\":0,\"error\":{\"message\":\"e\",\"code\":%s}}", Number);
		xrpcmessage Message; xrpcdocument Doc = rpcRead(Wire, XRPC_DECODE_RESPONSES, &Message, 1);
		testRequire((Message.Kind == XRPC_MESSAGE_ERROR) == Integral && Doc.InvalidCount == (Integral ? 0u : 1u), "rational integer oracle");
	}
}

static void testRpcMutations(void)
{
	static const char Seed[] = "[{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1e9999,\"params\":[{\"x\":\"\\u0000\"}]},{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-1,\"message\":\"e\"},\"id\":0}]";
	uint32 State = 0xa1b2c3d4u; xrpcdecodeconfig Config; xrpcDecodeConfigInit(&Config);
	Config.MaxDepth = 8; Config.MaxValues = 128; Config.MaxContainerItems = 16; Config.MaxMessages = 16;
	Config.MaxStringBytes = 64; Config.MaxIdBytes = Config.MaxMethodBytes = 32;
	for (size_t Trial = 0; Trial < 20000; ++Trial) {
		char Wire[sizeof(Seed)]; size_t Size = sizeof(Seed)-1u;
		if ((Trial % 4u) == 0) {
			Size = rpcRandom(&State) % sizeof(Wire);
			for (size_t Byte = 0; Byte < Size; ++Byte) Wire[Byte] = (char)rpcRandom(&State);
		} else {
			memcpy(Wire, Seed, Size);
			unsigned Changes = rpcRandom(&State) % 5u;
			for (unsigned Change = 0; Change < Changes; ++Change) Wire[rpcRandom(&State) % Size] = (char)rpcRandom(&State);
			if ((Trial % 5u) == 0) Size = rpcRandom(&State) % Size;
		}
		xrpcmessage Messages[16]; xrpcdocument Doc = {true, 77, 88};
		xrpcdecodestatus Status = xrpcDecode((xstrview){Wire,Size}, &Config, Messages, 16, &Doc);
		if (Status == XRPC_DECODE_DONE) {
			testRequire(Doc.Count > 0 && Doc.Count <= 16 && Doc.InvalidCount <= Doc.Count, "mutation document bounds");
			for (size_t Item = 0; Item < Doc.Count; ++Item) rpcViews((xstrview){Wire,Size}, &Messages[Item]);
		} else if (Status == XRPC_DECODE_PARSE_ERROR) testRequire(Doc.Count == 0 && !Doc.Batch, "mutation partial publication");
		else testRequire(Doc.Count == 77 && Doc.InvalidCount == 88 && Doc.Batch, "mutation local error changed document");
		xrtClearError();
	}
}

int main(void)
{
	testRpcKnown(); testRpcInvalid(); testRpcIntegral(); testRpcLimits(); testRpcAllocation(); testRpcProperties(); testRpcMutations();
	printf("[PASS] xrpc decode: roles/batch/notification/exact-id/error/duplicate/limits/zero-heap/OOM\n");
	return 0;
}
