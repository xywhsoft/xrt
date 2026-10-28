#include "test.h"

static xstrview rpcEncodeView(cstr Text) { return (xstrview){Text, strlen(Text)}; }

static void rpcEncodeExpected(const xrpcoutgoing* Messages, size_t Count, bool Batch, cstr Expected)
{
	unsigned char Output[4096]; memset(Output, 0xa5, sizeof(Output));
	size_t Size = 12345, Written = 54321, Length = strlen(Expected);
	testRequire(xrpcEncodeSize(Messages, Count, Batch, NULL, &Size) && Size == Length, "RPC encoded size differs from wire fixture");
	testRequire(xrpcEncodeWrite(Output+1, Length, Messages, Count, Batch, NULL, &Written) && Written == Length &&
		memcmp(Output+1, Expected, Length) == 0 && Output[0] == 0xa5 && Output[Length+1u] == 0xa5, "RPC wire fixture/canary mismatch");
	#if defined(XRPC_FEATURE_XRPC_DECODE)
	xrpcmessage Decoded[8]; xrpcdocument Doc;
	testRequire(xrpcDecode((xstrview){(cstr)Output+1, Length}, NULL, Decoded, 8, &Doc) == XRPC_DECODE_DONE &&
		Doc.Count == Count && Doc.Batch == Batch && Doc.InvalidCount == 0, "encoded RPC not independently classified");
	for (size_t Index = 0; Index < Count; ++Index) testRequire(Decoded[Index].Kind == Messages[Index].Kind, "RPC encode/decode kind changed");
	#endif
}

static void testRpcEncodeKnown(void)
{
	xrpcoutgoing Message = {0}; Message.Kind = XRPC_MESSAGE_CALL;
	Message.Method = XRT_STR_LITERAL("subtract"); Message.Id = XRT_STR_LITERAL("1e9999"); Message.Params = XRT_STR_LITERAL("[42,23]");
	rpcEncodeExpected(&Message, 1, false, "{\"jsonrpc\":\"2.0\",\"method\":\"subtract\",\"params\":[42,23],\"id\":1e9999}");
	Message.Id = XRT_STR_LITERAL("\"a\\u0000\\u0062\""); Message.Params = (xstrview){0};
	rpcEncodeExpected(&Message, 1, true, "[{\"jsonrpc\":\"2.0\",\"method\":\"subtract\",\"id\":\"a\\u0000\\u0062\"}]");
	Message.Kind = XRPC_MESSAGE_NOTIFICATION; Message.Id = (xstrview){0}; Message.Method = (xstrview){0};
	rpcEncodeExpected(&Message, 1, false, "{\"jsonrpc\":\"2.0\",\"method\":\"\"}");
	Message.Method = XRT_STR_LITERAL("m\"\\\n\x00\x01\xe4\xb8\xad\xf0\x9f\x98\x80");
	Message.Params = XRT_STR_LITERAL(" \t{\"dup\":1,\"dup\":2}\n");
	rpcEncodeExpected(&Message, 1, false, "{\"jsonrpc\":\"2.0\",\"method\":\"m\\\"\\\\\\n\\u0000\\u0001\xe4\xb8\xad\xf0\x9f\x98\x80\",\"params\": \t{\"dup\":1,\"dup\":2}\n}");
	Message = (xrpcoutgoing){0}; Message.Kind = XRPC_MESSAGE_RESULT; Message.Id = XRT_STR_LITERAL("-0"); Message.Result = XRT_STR_LITERAL("null");
	rpcEncodeExpected(&Message, 1, false, "{\"jsonrpc\":\"2.0\",\"result\":null,\"id\":-0}");
	Message.Kind = XRPC_MESSAGE_ERROR; Message.Result = (xstrview){0}; Message.Id = XRT_STR_LITERAL("null");
	Message.ErrorCode = XRT_STR_LITERAL(" \t-32601.0e0\r"); Message.ErrorMessage = XRT_STR_LITERAL("Method not found"); Message.ErrorData = XRT_STR_LITERAL("false");
	rpcEncodeExpected(&Message, 1, false, "{\"jsonrpc\":\"2.0\",\"error\":{\"code\": \t-32601.0e0\r,\"message\":\"Method not found\",\"data\":false},\"id\":null}");
	Message.ErrorCode = XRT_STR_LITERAL("1e999999999999999999999"); Message.ErrorMessage = (xstrview){0}; Message.ErrorData = (xstrview){0};
	rpcEncodeExpected(&Message, 1, false, "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":1e999999999999999999999,\"message\":\"\"},\"id\":null}");
	xrpcoutgoing Batch[2] = {{0},{0}};
	Batch[0].Kind = XRPC_MESSAGE_CALL; Batch[0].Method = XRT_STR_LITERAL("rpc.extension"); Batch[0].Id = XRT_STR_LITERAL("1.25");
	Batch[1].Kind = XRPC_MESSAGE_NOTIFICATION; Batch[1].Method = XRT_STR_LITERAL("n");
	rpcEncodeExpected(Batch, 2, true, "[{\"jsonrpc\":\"2.0\",\"method\":\"rpc.extension\",\"id\":1.25},{\"jsonrpc\":\"2.0\",\"method\":\"n\"}]");
	Batch[0].Kind = XRPC_MESSAGE_NOTIFICATION; Batch[0].Id = (xstrview){0};
	rpcEncodeExpected(Batch, 2, true, "[{\"jsonrpc\":\"2.0\",\"method\":\"rpc.extension\"},{\"jsonrpc\":\"2.0\",\"method\":\"n\"}]");
}

static void rpcEncodeReject(const xrpcoutgoing* Messages, size_t Count, bool Batch, const xrpcencodeconfig* Config, int Code)
{
	unsigned char Output[4096], Before[sizeof(Output)]; memset(Output, 0xa5, sizeof(Output)); memcpy(Before, Output, sizeof(Output));
	size_t Size = 12345, Written = 54321;
	testRequire(!xrpcEncodeSize(Messages, Count, Batch, Config, &Size) && Size == 12345, "RPC failed size published output");
	if (Code) testRequire(xrtErrorCode(xrtGetError()) == Code, "unexpected RPC size diagnostic");
	xrtClearError();
	testRequire(!xrpcEncodeWrite(Output, sizeof(Output), Messages, Count, Batch, Config, &Written) && Written == 54321 &&
		memcmp(Output, Before, sizeof(Output)) == 0, "RPC failed write changed output");
	if (Code) testRequire(xrtErrorCode(xrtGetError()) == Code, "unexpected RPC write diagnostic");
	xrtClearError();
}

static void testRpcEncodeInvalid(void)
{
	xrpcoutgoing Message = {0}; Message.Kind = XRPC_MESSAGE_CALL; Message.Method = XRT_STR_LITERAL("m");
	rpcEncodeReject(&Message, 1, false, NULL, XRPC_CODEC_VALUE);
	static const cstr BadIds[] = {"true", "false", "[]", "{}", " ", "1,\"result\":0", "\"\\uD800\"", "01", "NaN", "["};
	for (size_t Index = 0; Index < sizeof(BadIds)/sizeof(BadIds[0]); ++Index) {
		Message.Id = rpcEncodeView(BadIds[Index]); rpcEncodeReject(&Message, 1, false, NULL, 0);
	}
	Message.Id = XRT_STR_LITERAL("null");
	static const cstr BadParams[] = {"null", "true", "1", "\"s\"", "[1,]", "{/*c*/}", "{}[]", "[1e+ ]"};
	for (size_t Index = 0; Index < sizeof(BadParams)/sizeof(BadParams[0]); ++Index) {
		Message.Params = rpcEncodeView(BadParams[Index]); rpcEncodeReject(&Message, 1, false, NULL, 0);
	}
	Message.Params = (xstrview){0};
	static const cstr BadUtf8[] = {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\x80", "\xe4\xb8"};
	for (size_t Index = 0; Index < sizeof(BadUtf8)/sizeof(BadUtf8[0]); ++Index) {
		Message.Method = rpcEncodeView(BadUtf8[Index]); rpcEncodeReject(&Message, 1, false, NULL, 0);
	}
	Message = (xrpcoutgoing){0}; Message.Kind = XRPC_MESSAGE_ERROR; Message.Id = XRT_STR_LITERAL("0");
	static const cstr BadCodes[] = {"", "null", "\"1\"", "1.5", "1e-999999999999999999", "[1]", "1.5e-1", "01", "1e"};
	for (size_t Index = 0; Index < sizeof(BadCodes)/sizeof(BadCodes[0]); ++Index) {
		Message.ErrorCode = rpcEncodeView(BadCodes[Index]); rpcEncodeReject(&Message, 1, false, NULL, 0);
	}
	Message.ErrorCode = XRT_STR_LITERAL("1"); Message.ErrorMessage = rpcEncodeView(BadUtf8[0]); rpcEncodeReject(&Message, 1, false, NULL, 0);
	Message.ErrorMessage = (xstrview){0}; Message.ErrorData = XRT_STR_LITERAL("false true"); rpcEncodeReject(&Message, 1, false, NULL, 0);
	Message = (xrpcoutgoing){0}; Message.Kind = XRPC_MESSAGE_RESULT; Message.Id = XRT_STR_LITERAL("0"); rpcEncodeReject(&Message, 1, false, NULL, XRPC_CODEC_VALUE);
	Message.Result = XRT_STR_LITERAL("null");
	xrpcoutgoing Batch[2] = {Message, Message}; Batch[1].Kind = XRPC_MESSAGE_NOTIFICATION; Batch[1].Id = Batch[1].Result = (xstrview){0};
	rpcEncodeReject(Batch, 2, true, NULL, XRPC_CODEC_ARGUMENT);
	Batch[1] = Message; Batch[1].Result = XRT_STR_LITERAL("[1,]"); rpcEncodeReject(Batch, 2, true, NULL, 0);
	for (unsigned Index = 0; Index < 7; ++Index) {
		xrpcoutgoing Invalid = Message;
		if (Index == 0) Invalid.Method = XRT_STR_LITERAL("m");
		else if (Index == 1) Invalid.Params = XRT_STR_LITERAL("[]");
		else if (Index == 2) Invalid.ErrorCode = XRT_STR_LITERAL("1");
		else if (Index == 3) Invalid.ErrorMessage = XRT_STR_LITERAL("e");
		else if (Index == 4) Invalid.ErrorData = XRT_STR_LITERAL("null");
		else Invalid.Kind = Index == 5 ? XRPC_MESSAGE_INVALID : (xrpcmessagekind)999;
		rpcEncodeReject(&Invalid, 1, false, NULL, XRPC_CODEC_ARGUMENT);
	}
	Message.Kind = XRPC_MESSAGE_NOTIFICATION; Message.Result = (xstrview){0}; rpcEncodeReject(&Message, 1, false, NULL, XRPC_CODEC_ARGUMENT);
	Message.Kind = XRPC_MESSAGE_CALL; Message.ErrorData = XRT_STR_LITERAL("0"); rpcEncodeReject(&Message, 1, false, NULL, XRPC_CODEC_ARGUMENT);
	Message.Kind = XRPC_MESSAGE_ERROR; Message.ErrorData = (xstrview){0}; Message.Result = XRT_STR_LITERAL("0"); rpcEncodeReject(&Message, 1, false, NULL, XRPC_CODEC_ARGUMENT);
}

static void testRpcEncodeLimits(void)
{
	xrpcencodeconfig Config; xrpcEncodeConfigInit(&Config);
	xrpcoutgoing Message = {0}; Message.Kind = XRPC_MESSAGE_NOTIFICATION; Message.Method = XRT_STR_LITERAL("m"); Message.Params = XRT_STR_LITERAL("[{\"a\":[0]}]");
	size_t Size; testRequire(xrpcEncodeSize(&Message, 1, false, NULL, &Size), "RPC limit fixture");
	Config.MaxOutputBytes = Size-1u; rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_LIMIT);
	Config.MaxOutputBytes = Size; testRequire(xrpcEncodeSize(&Message, 1, false, &Config, &Size), "exact RPC byte cap");
	xrpcEncodeConfigInit(&Config); Config.MaxDepth = 3; rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_LIMIT);
	Config.MaxDepth = 4; Config.MaxValues = 7; Config.MaxStringBytes = 7; Config.MaxContainerItems = 3; Config.MaxMethodBytes = 3;
	testRequire(xrpcEncodeSize(&Message, 1, false, &Config, &Size), "exact RPC aggregate limits");
	rpcEncodeReject(&Message, 1, true, &Config, XRPC_CODEC_LIMIT); /* batch adds depth and value */
	Config.MaxDepth = 5; Config.MaxValues = 8; testRequire(xrpcEncodeSize(&Message, 1, true, &Config, &Size), "batch resource accounting");
	Config.MaxValues = 6; rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_LIMIT);
	xrpcEncodeConfigInit(&Config); Config.MaxContainerItems = 2; rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_LIMIT);
	xrpcEncodeConfigInit(&Config); Config.MaxStringBytes = 6; rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_LIMIT);
	xrpcEncodeConfigInit(&Config); Config.MaxStringBytes = 7; Message.Method = XRT_STR_LITERAL("12345678"); rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_LIMIT);
	xrpcEncodeConfigInit(&Config); Message.Method = XRT_STR_LITERAL("\n"); Config.MaxMethodBytes = 3; rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_LIMIT);
	Config.MaxMethodBytes = 4; testRequire(xrpcEncodeSize(&Message, 1, false, &Config, &Size), "escaped RPC method token cap");
	xrpcEncodeConfigInit(&Config); Message.Method = XRT_STR_LITERAL("m"); Message.Params = XRT_STR_LITERAL("[0,1,2,3]"); Config.MaxContainerItems = 3;
	rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_LIMIT);
	xrpcEncodeConfigInit(&Config); Message.Params = XRT_STR_LITERAL("{\"12345678\":0}"); Config.MaxStringBytes = 7; rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_LIMIT);
	Message.Params = (xstrview){0}; Message.Kind = XRPC_MESSAGE_CALL; Message.Id = XRT_STR_LITERAL("\"a\\u0000\"");
	xrpcEncodeConfigInit(&Config); Config.MaxIdBytes = Message.Id.Size-1u; rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_LIMIT);
	Config.MaxIdBytes = Message.Id.Size; testRequire(xrpcEncodeSize(&Message, 1, false, &Config, &Size), "exact RPC id cap");
	xrpcoutgoing Batch[2] = {Message, Message}; xrpcEncodeConfigInit(&Config); Config.MaxMessages = 1; rpcEncodeReject(Batch, 2, true, &Config, XRPC_CODEC_LIMIT);
	/* Error object and its data have two additional container levels. */
	Message = (xrpcoutgoing){0}; Message.Kind = XRPC_MESSAGE_ERROR; Message.Id = XRT_STR_LITERAL("null"); Message.ErrorCode = XRT_STR_LITERAL("0"); Message.ErrorData = XRT_STR_LITERAL("[]");
	xrpcEncodeConfigInit(&Config); Config.MaxDepth = 2; rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_LIMIT);
	Config.MaxDepth = 3; Config.MaxValues = 7; testRequire(xrpcEncodeSize(&Message, 1, false, &Config, &Size), "RPC error depth/value accounting");
	Config.MaxValues = 6; rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_LIMIT);
}

static void testRpcEncodeApi(void)
{
	xrpcoutgoing Message = {0}; Message.Kind = XRPC_MESSAGE_NOTIFICATION; Message.Method = XRT_STR_LITERAL("m");
	xrpcencodeconfig Config; xrpcEncodeConfigInit(&Config); size_t Size = 12345;
	rpcEncodeReject(NULL, 1, false, NULL, XRPC_CODEC_ARGUMENT); rpcEncodeReject(&Message, 0, true, NULL, XRPC_CODEC_ARGUMENT);
	rpcEncodeReject(&Message, 2, false, NULL, XRPC_CODEC_ARGUMENT); rpcEncodeReject(&Message, SIZE_MAX, true, NULL, XRPC_CODEC_ARGUMENT);
	Message.Method = (xstrview){NULL, 1}; rpcEncodeReject(&Message, 1, false, NULL, XRPC_CODEC_ARGUMENT);
	Message.Method = (xstrview){(cstr)(uintptr_t)(UINTPTR_MAX-1u), 4}; rpcEncodeReject(&Message, 1, false, NULL, XRPC_CODEC_ARGUMENT);
	Message.Method = XRT_STR_LITERAL("m"); Config.Size = 0; rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_ARGUMENT);
	for (unsigned Index = 0; Index < 9; ++Index) {
		xrpcEncodeConfigInit(&Config);
		if (Index == 0) Config.MaxDepth = 0; else if (Index == 1) Config.MaxDepth = XJSON_DEPTH_DEFAULT+1u;
		else if (Index == 2) Config.MaxOutputBytes = 0; else if (Index == 3) Config.MaxStringBytes = 0;
		else if (Index == 4) Config.MaxValues = 0; else if (Index == 5) Config.MaxContainerItems = 0;
		else if (Index == 6) Config.MaxMessages = 0; else if (Index == 7) Config.MaxIdBytes = 0; else Config.MaxMethodBytes = 0;
		rpcEncodeReject(&Message, 1, false, &Config, XRPC_CODEC_ARGUMENT);
	}
	testRequire(!xrpcEncodeConfigInit(NULL) && !xrpcEncodeConfigInit((xrpcencodeconfig*)(uintptr_t)(UINTPTR_MAX-1u)), "invalid RPC config output"); xrtClearError();
	testRequire(!xrpcEncodeSize(&Message, 1, false, NULL, NULL), "NULL RPC size output"); xrtClearError();
	unsigned char Output[1024]; memset(Output, 0xa5, sizeof(Output)); size_t Written = 54321;
	testRequire(xrpcEncodeSize(&Message, 1, false, NULL, &Size), "RPC capacity size");
	for (size_t Capacity = 0; Capacity < Size; ++Capacity) {
		testRequire(!xrpcEncodeWrite(Output, Capacity, &Message, 1, false, NULL, &Written) && Written == 54321 && Output[0] == 0xa5 && Output[Size-1u] == 0xa5 &&
			xrtErrorCode(xrtGetError()) == XRPC_CODEC_CAPACITY, "RPC capacity failure changed output"); xrtClearError();
	}
	testRequire(!xrpcEncodeWrite(NULL, sizeof(Output), &Message, 1, false, NULL, &Written), "NULL RPC wire output"); xrtClearError();
	testRequire(!xrpcEncodeWrite((ptr)(uintptr_t)(UINTPTR_MAX-1u), 4, &Message, 1, false, NULL, &Written), "wrapping RPC output span"); xrtClearError();
	testRequire(xrpcEncodeWrite(Output, sizeof(Output), &Message, 1, false, NULL, NULL), "optional RPC written output");
}

static void testRpcEncodeAliases(void)
{
	xrpcoutgoing Message = {0}; Message.Kind = XRPC_MESSAGE_CALL; Message.Id = XRT_STR_LITERAL("0"); Message.Method = XRT_STR_LITERAL("m");
	xrpcencodeconfig Config; xrpcEncodeConfigInit(&Config);
	xrpcoutgoing Snapshot = Message;
	testRequire(!xrpcEncodeSize(&Message, 1, false, NULL, &Message.Id.Size) && memcmp(&Message, &Snapshot, sizeof(Message)) == 0, "RPC size overlaps descriptor"); xrtClearError();
	testRequire(!xrpcEncodeWrite(&Message, sizeof(Message), &Message, 1, false, NULL, NULL) && memcmp(&Message, &Snapshot, sizeof(Message)) == 0, "RPC wire overlaps descriptor"); xrtClearError();
	xrpcencodeconfig SavedConfig = Config;
	testRequire(!xrpcEncodeWrite(&Config, sizeof(Config), &Message, 1, false, &Config, NULL) && memcmp(&Config, &SavedConfig, sizeof(Config)) == 0, "RPC wire overlaps config"); xrtClearError();
	testRequire(!xrpcEncodeSize(&Message, 1, false, &Config, &Config.Size) && memcmp(&Config, &SavedConfig, sizeof(Config)) == 0, "RPC size overlaps config"); xrtClearError();
	unsigned char Output[1024], Before[sizeof(Output)]; memset(Output, 0xa5, sizeof(Output)); memcpy(Before, Output, sizeof(Output));
	testRequire(!xrpcEncodeWrite(Output, sizeof(Output), &Message, 1, false, NULL, (size_t*)Output) && memcmp(Output, Before, sizeof(Output)) == 0, "RPC written overlaps wire"); xrtClearError();
	testRequire(!xrpcEncodeWrite(Output, sizeof(Output), &Message, 1, false, NULL, &Message.Id.Size) && memcmp(&Message, &Snapshot, sizeof(Message)) == 0 && memcmp(Output, Before, sizeof(Output)) == 0, "RPC written overlaps inputs"); xrtClearError();
	/* Every live view is checked, including late id/error fields. */
	for (unsigned Index = 0; Index < 7; ++Index) {
		char Input[1024] = "null"; xrpcoutgoing Item = {0}; Item.Id = XRT_STR_LITERAL("0");
		if (Index < 3) {
			Item.Kind = XRPC_MESSAGE_CALL; Item.Method = XRT_STR_LITERAL("m");
			if (Index == 0) Item.Id = (xstrview){Input, 4};
			if (Index == 1) Item.Method = (xstrview){Input, 4};
			if (Index == 2) { memcpy(Input, "[]", 3); Item.Params = (xstrview){Input, 2}; }
		} else if (Index == 3) { Item.Kind = XRPC_MESSAGE_RESULT; Item.Result = (xstrview){Input, 4}; }
		else {
			Item.Kind = XRPC_MESSAGE_ERROR; Item.ErrorCode = XRT_STR_LITERAL("1"); Item.ErrorMessage = XRT_STR_LITERAL("e");
			if (Index == 4) { Input[0] = '1'; Input[1] = 0; Item.ErrorCode = (xstrview){Input, 1}; }
			if (Index == 5) Item.ErrorMessage = (xstrview){Input, 4};
			if (Index == 6) Item.ErrorData = (xstrview){Input, 4};
		}
		char Saved[sizeof(Input)]; memcpy(Saved, Input, sizeof(Input));
		testRequire(!xrpcEncodeWrite(Input, sizeof(Input), &Item, 1, false, NULL, NULL) && xrtErrorCode(xrtGetError()) == XRPC_CODEC_ARGUMENT && memcmp(Input, Saved, sizeof(Input)) == 0, "RPC wire overlaps a live view"); xrtClearError();
		testRequire(!xrpcEncodeSize(&Item, 1, false, NULL, (size_t*)Input) && memcmp(Input, Saved, sizeof(Input)) == 0, "RPC size overlaps a live view"); xrtClearError();
	}
}

/* Test-only byte oracle: escape controls/quotes/backslash, copy valid UTF-8.
 * It does not use the production JSON parser or quote writer. */
static size_t rpcEncodeQuoteOracle(char* Output, xstrview Text)
{
	static const char Hex[] = "0123456789ABCDEF"; size_t Size = 0; Output[Size++] = '"';
	for (size_t Index = 0; Index < Text.Size; ++Index) {
		unsigned char Byte = (unsigned char)Text.Data[Index]; char Short = 0;
		if (Byte == '"' || Byte == '\\') Short = (char)Byte;
		else if (Byte == '\b') Short = 'b'; else if (Byte == '\f') Short = 'f'; else if (Byte == '\n') Short = 'n'; else if (Byte == '\r') Short = 'r'; else if (Byte == '\t') Short = 't';
		if (Short) { Output[Size++] = '\\'; Output[Size++] = Short; }
		else if (Byte < 32) { memcpy(Output+Size, "\\u00", 4); Size += 4; Output[Size++] = Hex[Byte >> 4]; Output[Size++] = Hex[Byte & 15u]; }
		else Output[Size++] = (char)Byte;
	}
	Output[Size++] = '"'; Output[Size] = 0; return Size;
}

static uint32 rpcEncodeRandom(uint32* State)
{
	*State ^= *State << 13u; *State ^= *State >> 17u; *State ^= *State << 5u; return *State;
}

static xjsonvisitaction rpcEncodeLimitOracle(const xjsonscanevent* Event, ptr User)
{
	(void)Event; (void)User; return XJSON_VISIT_NEXT;
}

static void testRpcEncodeProperties(void)
{
	uint32 State = 0x13572468u;
	static const cstr Ids[] = {"null", "-0", "1e9999", "\"a\\u0000b\"", "1.25", "1234567890123456789012345678901234567890"};
	static const cstr Params[] = {"[]", "{}", " \n[null,\"\\uD83D\\uDE00\",{}]\t", "{\"x\":[0],\"x\":1}"};
	static const cstr Results[] = {"null", "true", "1e9999", "-0.0", "\"s\\u0000\"", "[{\"x\":1}]", "{}"};
	static const cstr Codes[] = {"-32600", "1.0", "1.25e2", "0e-99999", "100e-2", "1e99999"};
	for (size_t Trial = 0; Trial < 10000; ++Trial) {
		char Text[96], Quoted[600], Expected[4096]; size_t TextSize = 0, Size = 0, Count = 1u + rpcEncodeRandom(&State) % 6u;
		bool Request = (Trial & 1u) == 0, Batch = Count > 1 || (Trial & 2u) != 0;
		for (unsigned Index = 0, Length = rpcEncodeRandom(&State) % 60u; Index < Length; ++Index) Text[TextSize++] = (char)(rpcEncodeRandom(&State) % 128u);
		memcpy(Text+TextSize, "\xe4\xb8\xad\xf0\x9f\x98\x80", 7); TextSize += 7;
		rpcEncodeQuoteOracle(Quoted, (xstrview){Text, TextSize});
		xrpcoutgoing Messages[6] = {{0}};
		if (Batch) Expected[Size++] = '[';
		for (size_t Index = 0; Index < Count; ++Index) {
			xrpcoutgoing* Message = &Messages[Index]; cstr Id = Ids[rpcEncodeRandom(&State) % 6u];
			if (Index) Expected[Size++] = ',';
			int Bytes;
			if (Request) {
				bool Call = (rpcEncodeRandom(&State) & 1u) != 0; cstr Param = Params[rpcEncodeRandom(&State) % 4u];
				Message->Kind = Call ? XRPC_MESSAGE_CALL : XRPC_MESSAGE_NOTIFICATION; Message->Method = (xstrview){Text, TextSize}; Message->Params = rpcEncodeView(Param);
				if (Call) Message->Id = rpcEncodeView(Id);
				Bytes = snprintf(Expected+Size, sizeof(Expected)-Size, "{\"jsonrpc\":\"2.0\",\"method\":%s,\"params\":%s%s%s}", Quoted, Param, Call ? ",\"id\":" : "", Call ? Id : "");
			} else {
				Message->Id = rpcEncodeView(Id);
				if (rpcEncodeRandom(&State) & 1u) {
					cstr Result = Results[rpcEncodeRandom(&State) % 7u]; Message->Kind = XRPC_MESSAGE_RESULT; Message->Result = rpcEncodeView(Result);
					Bytes = snprintf(Expected+Size, sizeof(Expected)-Size, "{\"jsonrpc\":\"2.0\",\"result\":%s,\"id\":%s}", Result, Id);
				} else {
					cstr Code = Codes[rpcEncodeRandom(&State) % 6u], Data = Results[rpcEncodeRandom(&State) % 7u];
					Message->Kind = XRPC_MESSAGE_ERROR; Message->ErrorCode = rpcEncodeView(Code); Message->ErrorMessage = (xstrview){Text, TextSize}; Message->ErrorData = rpcEncodeView(Data);
					Bytes = snprintf(Expected+Size, sizeof(Expected)-Size, "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":%s,\"message\":%s,\"data\":%s},\"id\":%s}", Code, Quoted, Data, Id);
				}
			}
			testRequire(Bytes > 0 && (size_t)Bytes < sizeof(Expected)-Size, "RPC encode oracle buffer"); Size += (size_t)Bytes;
		}
		if (Batch) Expected[Size++] = ']';
		Expected[Size] = 0;
		rpcEncodeExpected(Messages, Count, Batch, Expected);
		if (Trial % 20u == 0) {
			/* Compare our aggregate accounting with a complete-document scan,
			 * which sees envelope containers/keys and the batch as ordinary JSON. */
			xrpcencodeconfig Limits; xrpcEncodeConfigInit(&Limits);
			Limits.MaxOutputBytes = (rpcEncodeRandom(&State) & 1u) ? Size : Size/2u;
			Limits.MaxDepth = 1u + rpcEncodeRandom(&State) % 8u;
			Limits.MaxStringBytes = 1u + rpcEncodeRandom(&State) % 96u;
			Limits.MaxValues = 1u + rpcEncodeRandom(&State) % 128u;
			Limits.MaxContainerItems = 1u + rpcEncodeRandom(&State) % 8u;
			Limits.MaxMessages = 1u + rpcEncodeRandom(&State) % 6u;
			xjsonreadconfig Json; xrtJsonReadConfigInit(&Json);
			Json.MaxInputBytes = Limits.MaxOutputBytes; Json.MaxDepth = Limits.MaxDepth; Json.MaxStringBytes = Limits.MaxStringBytes;
			Json.MaxValues = Limits.MaxValues; Json.MaxContainerItems = Limits.MaxContainerItems;
			bool Accept = xrtJsonScan((xstrview){Expected, Size}, &Json, rpcEncodeLimitOracle, NULL) == XJSON_VISIT_DONE && Count <= Limits.MaxMessages;
			xrtClearError(); size_t Required = 12345;
			testRequire(xrpcEncodeSize(Messages, Count, Batch, &Limits, &Required) == Accept && (Accept ? Required == Size : Required == 12345), "RPC aggregate resource accounting disagrees with complete JSON scan");
			xrtClearError();
		}
	}
	/* Arbitrary/mutated raw JSON cannot cause partial output on rejection. */
	static const char Seed[] = "{\"x\":[1e9999,\"\\u0000\",true,null]}";
	for (size_t Trial = 0; Trial < 10000; ++Trial) {
		char Raw[sizeof(Seed)], Output[1024], Before[sizeof(Output)]; size_t Length = sizeof(Seed)-1u;
		memcpy(Raw, Seed, Length);
		if (Trial & 1u) { Length = rpcEncodeRandom(&State) % sizeof(Raw); for (size_t Index = 0; Index < Length; ++Index) Raw[Index] = (char)rpcEncodeRandom(&State); }
		else for (unsigned Changes = 0, Limit = rpcEncodeRandom(&State) % 4u; Changes < Limit; ++Changes) Raw[rpcEncodeRandom(&State) % Length] = (char)rpcEncodeRandom(&State);
		xrpcoutgoing Message = {0}; Message.Kind = XRPC_MESSAGE_RESULT; Message.Id = XRT_STR_LITERAL("0"); Message.Result = (xstrview){Raw, Length};
		memset(Output, 0xa5, sizeof(Output)); memcpy(Before, Output, sizeof(Output)); size_t Required = 12345, Written = 54321;
		bool Measured = xrpcEncodeSize(&Message, 1, false, NULL, &Required); xrtClearError();
		bool Encoded = xrpcEncodeWrite(Output, sizeof(Output), &Message, 1, false, NULL, &Written);
		testRequire(Measured == Encoded, "RPC encode raw mutation changed validation result");
		if (Encoded) testRequire(Required == Written && Written <= sizeof(Output), "RPC encode mutation size mismatch");
		else testRequire(Required == 12345 && Written == 54321 && memcmp(Output, Before, sizeof(Output)) == 0, "RPC encode mutation partial publication");
		xrtClearError();
	}
}

static void testRpcEncodeAllocation(void)
{
	xrpcoutgoing Message = {0}; Message.Kind = XRPC_MESSAGE_CALL; Message.Method = XRT_STR_LITERAL("m\n\x00\xe4\xb8\xad");
	Message.Id = XRT_STR_LITERAL("1234567890123456789012345678901234567890"); Message.Params = XRT_STR_LITERAL("[\"\\u0000\",1e9999]");
	char Output[1024]; size_t Required, Written;
	testRequire(xrpcEncodeWrite(Output, sizeof(Output), &Message, 1, false, NULL, &Written), "RPC encode warmup");
	xrtSetErrorInfo(XERR_VALUE, "xrt.test", 777, "ambient"); const xerror* Ambient = xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before, After; xrtMemStatsEnable(true); xrtMemStatsGet(&Before);
	#endif
	for (size_t Index = 0; Index < 10000; ++Index) {
		testRequire(xrpcEncodeSize(&Message, 1, false, NULL, &Required) && xrpcEncodeWrite(Output, sizeof(Output), &Message, 1, false, NULL, &Written) &&
			Required == Written && xrtGetError() == Ambient, "RPC encode success changed ambient error");
	}
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After); xrtMemStatsEnable(false);
	testRequire(After.BlockAllocCalls == Before.BlockAllocCalls && After.MallocCalls == Before.MallocCalls && After.CallocCalls == Before.CallocCalls &&
		After.ReallocCalls == Before.ReallocCalls, "RPC encoder allocated on success");
	#endif
	xrtClearError(); xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
	testRequire(xrtAllocationBudgetCreate(1, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "RPC encode quota");
	testRequire(xrpcEncodeSize(&Message, 1, true, NULL, &Required) && xrpcEncodeWrite(Output, sizeof(Output), &Message, 1, true, NULL, &Written), "RPC encode under one-byte quota");
	memset(Output, 0xa5, sizeof(Output)); Message.Id = XRT_STR_LITERAL("1e+"); Written = 54321;
	testRequire(!xrpcEncodeWrite(Output, sizeof(Output), &Message, 1, false, NULL, &Written) && Written == 54321 && (unsigned char)Output[0] == 0xa5 &&
		xrtErrorKind(xrtGetError()) == XERR_MEMORY, "RPC encode diagnostic OOM changed output");
	testRequire(xrtAllocationBudgetLeave(&Scope), "RPC encode quota leave"); xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xrtAllocationBudgetInfo(Budget, &Info) && Info.LiveBytes == 0 && Info.Rejections != 0, "RPC encode quota leak");
	xrtClearError(); xrtAllocationBudgetFree(Budget);
}

int main(void)
{
	testRpcEncodeKnown(); testRpcEncodeInvalid(); testRpcEncodeLimits(); testRpcEncodeApi(); testRpcEncodeAliases(); testRpcEncodeProperties(); testRpcEncodeAllocation();
	printf("[PASS] xrpc encode: requests/notifications/responses/batch/escaping/exact-id/limits/atomicity/zero-heap/OOM\n"); return 0;
}
