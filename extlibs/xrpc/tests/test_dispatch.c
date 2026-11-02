#include "test.h"

typedef struct rpcdispatchfixture {
	char Output[8192]; size_t Bytes, Emitted, Calls, RejectAt;
	xrpcreply* Current;
	bool ReenterReply, SawStack, Nested, AbortSink;
	const xerror* AbortError;
	const xerror* ObservedError;
} rpcdispatchfixture;

static xstrview rpcDispatchView(cstr Text) { return (xstrview){Text, strlen(Text)}; }
static bool rpcDispatchEqual(xstrview Raw, cstr Text)
{ return Raw.Size == strlen(Text) && memcmp(Raw.Data, Text, Raw.Size) == 0; }

static bool rpcDispatchSink(const xrpcoutgoing* Response, bool Batch, size_t EncodedBytes, ptr User)
{
	rpcdispatchfixture* Fixture = User;
	if (Fixture->RejectAt && Fixture->Emitted + 1u == Fixture->RejectAt) return false;
	if (Batch) Fixture->Output[Fixture->Bytes++] = Fixture->Emitted ? ',' : '[';
	size_t Written = 0;
	testRequire(xrpcEncodeWrite(Fixture->Output + Fixture->Bytes, sizeof(Fixture->Output) - Fixture->Bytes,
		Response, 1, false, NULL, &Written) && Written == EncodedBytes, "dispatch sink length/encoding");
	Fixture->Bytes += Written; ++Fixture->Emitted;
	if (Response->Result.Size == 8 && memcmp(Response->Result.Data, "[7,true]", 8) == 0) Fixture->SawStack = true;
	if (Fixture->ReenterReply) {
		testRequire(!xrpcReplyResult(Fixture->Current, XRT_STR_LITERAL("0")), "reply reentry accepted");
		xrtClearError();
	}
	if (Fixture->AbortSink) {
		testRequire(!xrpcReplyAbort(Fixture->Current, Fixture->AbortError), "sink local abort accepted as success");
		xrtClearError();
	}
	return true;
}

static bool rpcDispatchCall(const xrpcmessage* Request, xrpcreply* Reply, ptr User);
static bool rpcDispatchRun(xstrview Input, const xrpcdispatchconfig* Config,
	rpcdispatchfixture* Fixture, xrpcdispatchinfo* Info)
{
	xrpcmessage Scratch[16]; Info->Size = sizeof(*Info);
	bool Done = xrpcDispatch(Input, Config, Scratch, 16, rpcDispatchCall, Fixture, rpcDispatchSink, Fixture, Info);
	if (Done && Info->Replies && Info->Batch) Fixture->Output[Fixture->Bytes++] = ']';
	Fixture->Output[Fixture->Bytes] = 0;
	if (Done) testRequire(Fixture->Bytes == Info->OutputBytes && Fixture->Emitted == Info->Replies, "dispatch aggregate accounting");
	return Done;
}

static bool rpcDispatchCall(const xrpcmessage* Request, xrpcreply* Reply, ptr User)
{
	rpcdispatchfixture* Fixture = User; ++Fixture->Calls; Fixture->Current = Reply;
	if (rpcDispatchEqual(Request->Method.Raw, "\"echo\"")) {
		xstrview Value = Request->Params.Kind == XRPC_VALUE_MISSING ? XRT_STR_LITERAL("null") : Request->Params.Raw;
		xrpcReplyResult(Reply, Value); return true;
	}
	if (rpcDispatchEqual(Request->Method.Raw, "\"stack\"")) {
		char Local[] = "[7,true]"; xrpcReplyResult(Reply, (xstrview){Local, sizeof(Local)-1u});
		memset(Local, '?', sizeof(Local)); return true;
	}
	if (rpcDispatchEqual(Request->Method.Raw, "\"error\"")) {
		xrpcReplyError(Reply, XRT_STR_LITERAL("1e9999"), XRT_STR_LITERAL("bad\x00value"), XRT_STR_LITERAL("{\"x\":false}")); return true;
	}
	if (rpcDispatchEqual(Request->Method.Raw, "\"double\"")) {
		xrpcReplyResult(Reply, XRT_STR_LITERAL("null"));
		testRequire(!xrpcReplyResult(Reply, XRT_STR_LITERAL("0")), "duplicate reply accepted");
		xrtClearError(); return true;
	}
	if (rpcDispatchEqual(Request->Method.Raw, "\"reject\"")) {
		xrpcReplyResult(Reply, XRT_STR_LITERAL("null")); return false;
	}
	if (rpcDispatchEqual(Request->Method.Raw, "\"bad\"")) {
		xrpcReplyResult(Reply, XRT_STR_LITERAL("[1,]")); xrtClearError(); return true;
	}
	if (rpcDispatchEqual(Request->Method.Raw, "\"badcode\"")) {
		xrpcReplyError(Reply, XRT_STR_LITERAL("1.5"), XRT_STR_LITERAL("bad"), (xstrview){0}); return true;
	}
	if (rpcDispatchEqual(Request->Method.Raw, "\"ignored\"")) {
		testRequire(xrpcReplyResult(Reply, (xstrview){NULL, SIZE_MAX}), "notification inspected suppressed payload"); return true;
	}
	if (rpcDispatchEqual(Request->Method.Raw, "\"ambient\"")) {
		xrtSetErrorInfo(XERR_VALUE, "xrt.test", 778, "handler diagnostic"); xrpcReplyResult(Reply, XRT_STR_LITERAL("null")); return true;
	}
	if (rpcDispatchEqual(Request->Method.Raw, "\"nested\"")) {
		rpcdispatchfixture Child = {0}; xrpcdispatchinfo Info = {0};
		testRequire(rpcDispatchRun(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"stack\",\"id\":1}"), NULL, &Child, &Info) &&
			Info.Replies == 1 && Child.SawStack, "independent nested dispatch");
		Fixture->Nested = true; xrpcReplyResult(Reply, XRT_STR_LITERAL("true")); return true;
	}
	if (rpcDispatchEqual(Request->Method.Raw, "\"abort\"")) {
		xrtSetErrorInfo(XERR_VALUE, "xrt.test.stale", 777, "handler stale diagnostic");
		xrpcReplyAbort(Reply, Fixture->AbortError); xrtClearError();
		testRequire(!xrpcReplyAbort(Reply, NULL), "repeated abort replaced sticky failure"); xrtClearError(); return false;
	}
	if (rpcDispatchEqual(Request->Method.Raw, "\"abort-current\"")) {
		xrtSetErrorInfo(XERR_AGAIN, "xrt.test.abort", 902, "current callback diagnostic");
		Fixture->ObservedError = xrtGetError(); xrpcReplyAbort(Reply, Fixture->ObservedError); xrtClearError(); return false;
	}
	if (rpcDispatchEqual(Request->Method.Raw, "\"abort-after\"")) {
		xrpcReplyResult(Reply, XRT_STR_LITERAL("null"));
		xrpcReplyAbort(Reply, Fixture->AbortError); xrtClearError(); return true;
	}
	return rpcDispatchEqual(Request->Method.Raw, "\"silent\"");
}

static void rpcDispatchExpected(cstr Input, cstr Expected, size_t Calls, size_t Messages, size_t Replies, bool Batch, bool ParseError)
{
	rpcdispatchfixture Fixture = {0}; xrpcdispatchinfo Info = {0};
	testRequire(rpcDispatchRun(rpcDispatchView(Input), NULL, &Fixture, &Info), "dispatch fixture failed");
	testRequire(strcmp(Fixture.Output, Expected) == 0 && Fixture.Calls == Calls && Info.Invoked == Calls &&
		Info.Messages == Messages && Info.Visited == Messages && Info.Replies == Replies && Info.Batch == Batch && Info.ParseError == ParseError,
		"dispatch wire/count/shape fixture mismatch");
}

static void testRpcDispatchWire(void)
{
	rpcDispatchExpected("{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"params\":[42,23],\"id\":123456789012345678901234567890}",
		"{\"jsonrpc\":\"2.0\",\"result\":[42,23],\"id\":123456789012345678901234567890}", 1, 1, 1, false, false);
	rpcDispatchExpected("[{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"id\":null}]",
		"[{\"jsonrpc\":\"2.0\",\"result\":null,\"id\":null}]", 1, 1, 1, true, false);
	rpcDispatchExpected("[{\"jsonrpc\":\"2.0\",\"method\":\"echo\"},1,{\"jsonrpc\":\"2.0\",\"method\":\"unknown\",\"id\":\"a\\u0000b\"},"
		"{\"jsonrpc\":\"2.0\",\"method\":\"silent\",\"id\":1e9999},{\"jsonrpc\":\"2.0\",\"method\":\"error\",\"id\":-0},"
		"{\"jsonrpc\":\"2.0\",\"method\":\"r\\u0070c.x\",\"id\":1.25}]",
		"[{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null},"
		"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Method not found\"},\"id\":\"a\\u0000b\"},"
		"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\"},\"id\":1e9999},"
		"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":1e9999,\"message\":\"bad\\u0000value\",\"data\":{\"x\":false}},\"id\":-0},"
		"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Method not found\"},\"id\":1.25}]", 4, 6, 5, true, false);
	rpcDispatchExpected("[{\"jsonrpc\":\"2.0\",\"method\":\"echo\"},{\"jsonrpc\":\"2.0\",\"method\":\"unknown\"},"
		"{\"jsonrpc\":\"2.0\",\"method\":\"rpc.x\"},{\"jsonrpc\":\"2.0\",\"method\":\"error\"},{\"jsonrpc\":\"2.0\",\"method\":\"ignored\"}]",
		"", 4, 5, 0, true, false);
	rpcDispatchExpected("[]", "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null}", 0, 1, 1, false, false);
	rpcDispatchExpected("[1]", "[{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null}]", 0, 1, 1, true, false);
	rpcDispatchExpected("{\"jsonrpc\":\"2.0\",\"result\":1,\"id\":1}",
		"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null}", 0, 1, 1, false, false);
	rpcDispatchExpected("[{\"jsonrpc\":\"2.0\",\"method\":\"stack\",\"id\":1},",
		"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"Parse error\"},\"id\":null}", 0, 0, 1, false, true);
	rpcdispatchfixture Fixture = {0}; xrpcdispatchinfo Info = {0};
	testRequire(rpcDispatchRun(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"stack\",\"id\":1}"), NULL, &Fixture, &Info) &&
		Fixture.SawStack && strstr(Fixture.Output, "[7,true]") != NULL, "stack response not consumed synchronously");
	Fixture = (rpcdispatchfixture){0};
	testRequire(rpcDispatchRun(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"nested\",\"id\":1}"), NULL, &Fixture, &Info) &&
		Fixture.Nested && Info.Invoked == 1 && Info.Replies == 1, "nested dispatcher corrupted parent");
}

static void testRpcDispatchFailures(void)
{
	static const cstr Methods[] = {"double", "reject", "bad", "badcode"};
	for (size_t Index = 0; Index < 4; ++Index) {
		char Input[256]; snprintf(Input, sizeof(Input), "[{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"id\":1},{\"jsonrpc\":\"2.0\",\"method\":\"stack\",\"id\":2}]", Methods[Index]);
		rpcdispatchfixture Fixture = {0}; xrpcdispatchinfo Info = {0};
		testRequire(!rpcDispatchRun(rpcDispatchView(Input), NULL, &Fixture, &Info) && Fixture.Calls == 1 && Info.Invoked == 1 &&
			Info.Visited == 0 && Info.Replies == (Index < 2 ? 1u : 0u), "failure executed later handler or lost partial info");
		testRequire(xrtGetError() != NULL && (Index >= 2 || xrtErrorCode(xrtGetError()) == XRPC_DISPATCH_STATE), "first reply diagnostic lost");
		xrtClearError();
	}
	xstrview Batch = XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"id\":1},{\"jsonrpc\":\"2.0\",\"method\":\"stack\",\"id\":2}]");
	rpcdispatchfixture Fixture = {0}; Fixture.RejectAt = 1; xrpcdispatchinfo Info = {0};
	xrtSetErrorInfo(XERR_VALUE, "xrt.test", 777, "stale ambient");
	testRequire(!rpcDispatchRun(Batch, NULL, &Fixture, &Info) && Info.Invoked == 1 && Info.Replies == 0 &&
		xrtErrorCode(xrtGetError()) == XRPC_DISPATCH_SINK, "sink rejection used stale error or continued business work"); xrtClearError();
	Fixture = (rpcdispatchfixture){0}; Fixture.ReenterReply = true;
	testRequire(!rpcDispatchRun(Batch, NULL, &Fixture, &Info) && Info.Invoked == 1 && Info.Replies == 1 &&
		xrtErrorCode(xrtGetError()) == XRPC_DISPATCH_STATE, "sink reply reentry lost first failure or accepted output count"); xrtClearError();
}

static void testRpcDispatchLimits(void)
{
	xstrview Batch = XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"id\":1},{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"id\":2}]");
	xrpcdispatchconfig Config; xrpcDispatchConfigInit(&Config);
	rpcdispatchfixture Fixture = {0}; xrpcdispatchinfo Info = {0};
	testRequire(rpcDispatchRun(Batch, &Config, &Fixture, &Info), "dispatch output bound fixture"); size_t Exact = Info.OutputBytes;
	Fixture = (rpcdispatchfixture){0}; Config.MaxOutputBytes = Exact;
	testRequire(rpcDispatchRun(Batch, &Config, &Fixture, &Info) && Info.OutputBytes == Exact, "exact response byte limit");
	Fixture = (rpcdispatchfixture){0}; Config.MaxOutputBytes = Exact - 1u;
	testRequire(!rpcDispatchRun(Batch, &Config, &Fixture, &Info) && Info.Invoked == 2 && Info.Replies == 1 && Info.Visited == 1 &&
		xrtErrorCode(xrtGetError()) == XRPC_DISPATCH_LIMIT, "aggregate bracket/comma byte limit"); xrtClearError();
	for (unsigned Kind = 0; Kind < 4; ++Kind) {
		xrpcDispatchConfigInit(&Config); Fixture = (rpcdispatchfixture){0};
		if (Kind == 0) Config.MaxReplies = 1;
		else if (Kind == 1) Config.Decode.MaxMessages = 1;
		else if (Kind == 2) Config.Decode.MaxInputBytes = Batch.Size - 1u;
		Info = (xrpcdispatchinfo){0}; Info.Size = sizeof(Info); Info.Messages = 12345;
		xrpcmessage Scratch[4];
		testRequire(!xrpcDispatch(Batch, &Config, Scratch, Kind == 3 ? 1 : 4, rpcDispatchCall, &Fixture,
			rpcDispatchSink, &Fixture, &Info) && Info.Messages == 12345 && !Fixture.Calls && !Fixture.Emitted,
			"admission failure invoked a callback or published Info"); xrtClearError();
	}
	xrpcDispatchConfigInit(&Config); Config.Reply.MaxOutputBytes = 1; Fixture = (rpcdispatchfixture){0};
	testRequire(!rpcDispatchRun(Batch, &Config, &Fixture, &Info) && Info.Invoked == 1 && Info.Replies == 0, "per-object output bound"); xrtClearError();
	xrpcDispatchConfigInit(&Config); Config.MaxOutputBytes = 1; Config.MaxReplies = 1; Fixture = (rpcdispatchfixture){0};
	testRequire(rpcDispatchRun(XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"echo\"},{\"jsonrpc\":\"2.0\",\"method\":\"silent\"}]"), &Config, &Fixture, &Info) &&
		Info.Notifications == 2 && Info.OutputBytes == 0 && Info.Replies == 0, "notification-only batch consumed response quota");
}

static void testRpcDispatchAbort(void)
{
	xerror* Cause = xrtErrorCreate(XERR_AGAIN, "xrt.test.abort", 901, "explicit local failure");
	testRequire(Cause != NULL, "abort cause setup");
	for (unsigned Mode = 0; Mode < 5; ++Mode) {
		rpcdispatchfixture Fixture = {0}; xrpcdispatchinfo Info = {0};
		Fixture.AbortError = Mode == 0 ? NULL : Cause; Fixture.AbortSink = Mode == 4;
		xstrview Input = Mode == 2 ? XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"abort\"},{\"jsonrpc\":\"2.0\",\"method\":\"stack\",\"id\":2}]") :
			Mode == 3 ? XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"abort-after\",\"id\":1},{\"jsonrpc\":\"2.0\",\"method\":\"stack\",\"id\":2}]") :
			Mode == 4 ? XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"id\":1},{\"jsonrpc\":\"2.0\",\"method\":\"stack\",\"id\":2}]") :
			XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"abort\",\"id\":1},{\"jsonrpc\":\"2.0\",\"method\":\"stack\",\"id\":2}]");
		xrtSetErrorInfo(XERR_VALUE, "xrt.test.stale", 777, "stale must not be adopted");
		testRequire(!rpcDispatchRun(Input, NULL, &Fixture, &Info) && Info.Invoked == 1 && Info.Visited == 0 &&
			Info.Replies == (Mode >= 3 ? 1u : 0u) && Fixture.Calls == 1, "local abort emitted wire error or ran later business handler");
		testRequire(Mode == 0 ? xrtErrorCode(xrtGetError()) == XRPC_DISPATCH_ABORTED : xrtGetError() == Cause,
			"explicit abort cause lost or NULL abort used stale ambient error"); xrtClearError();
	}
	/* Current diagnostic transfers rather than requiring an additional owning
	 * Ref. The callback clears its publication; the dispatch still owns it. */
	rpcdispatchfixture Fixture = {0};
	xrpcdispatchinfo Info = {0};
	testRequire(!rpcDispatchRun(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"abort-current\",\"id\":1}"), NULL, &Fixture, &Info) &&
		xrtGetError() == Fixture.ObservedError && xrtErrorCode(xrtGetError()) == 902, "current abort cause lifetime"); xrtClearError(); xrtErrorFree(Cause);
	testRequire(!xrpcReplyAbort(NULL, NULL), "NULL abort handle accepted"); xrtClearError();
}

static void testRpcDispatchApi(void)
{
	xstrview Input = XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"id\":1}");
	xrpcdispatchconfig Config; xrpcmessage Scratch[2]; xrpcdispatchinfo Info = {0}; Info.Size = sizeof(Info);
	rpcdispatchfixture Fixture = {0};
	testRequire(!xrpcDispatchConfigInit(NULL) && !xrpcReplyResult(NULL, XRT_STR_LITERAL("0")) &&
		!xrpcReplyError(NULL, XRT_STR_LITERAL("1"), (xstrview){0}, (xstrview){0}), "NULL public argument accepted"); xrtClearError();
	for (unsigned Kind = 0; Kind < 16; ++Kind) {
		xrpcDispatchConfigInit(&Config);
		if (Kind == 0) Config.Size = 0;
		else if (Kind == 1) Config.MaxReplies = 0;
		else if (Kind == 2) Config.MaxOutputBytes = 0;
		else if (Kind == 3) Config.Decode.Mode = XRPC_DECODE_DUPLEX;
		else if (Kind == 4) Config.Decode.Size = 0;
		else if (Kind == 5) Config.Reply.Size = 0;
		else if (Kind == 6) Config.Reply.MaxOutputBytes = 0;
		else if (Kind == 7) Config.Reply.MaxDepth = 0;
		else if (Kind == 8) Config.Reply.MaxDepth = XJSON_DEPTH_DEFAULT + 1u;
		else if (Kind == 9) Config.Reply.MaxStringBytes = 0;
		else if (Kind == 10) Config.Reply.MaxValues = 0;
		else if (Kind == 11) Config.Reply.MaxContainerItems = 0;
		else if (Kind == 12) Config.Reply.MaxMessages = 0;
		else if (Kind == 13) Config.Reply.MaxIdBytes = 0;
		else if (Kind == 14) Config.Reply.MaxMethodBytes = 0;
		else Info.Size = 0;
		testRequire(!xrpcDispatch(Input, &Config, Scratch, 2, rpcDispatchCall, &Fixture, rpcDispatchSink, &Fixture, &Info) &&
			!Fixture.Calls && !Fixture.Emitted, "invalid config invoked callback"); xrtClearError(); Info.Size = sizeof(Info);
	}
	testRequire(!xrpcDispatch(Input, NULL, NULL, 2, rpcDispatchCall, &Fixture, rpcDispatchSink, &Fixture, &Info) &&
		!xrpcDispatch(Input, NULL, Scratch, SIZE_MAX, rpcDispatchCall, &Fixture, rpcDispatchSink, &Fixture, &Info) &&
		!xrpcDispatch((xstrview){NULL, 1}, NULL, Scratch, 2, rpcDispatchCall, &Fixture, rpcDispatchSink, &Fixture, &Info) &&
		!xrpcDispatch(Input, NULL, Scratch, 2, rpcDispatchCall, &Fixture, NULL, &Fixture, &Info) &&
		!xrpcDispatch(Input, NULL, Scratch, 2, rpcDispatchCall, &Fixture, rpcDispatchSink, &Fixture, NULL), "invalid dispatch span accepted"); xrtClearError();
	union { xrpcmessage Scratch[2]; xrpcdispatchinfo Info; xrpcdispatchconfig Config; char Input[1024]; } Alias;
	Alias.Info = (xrpcdispatchinfo){0}; Alias.Info.Size = sizeof(Alias.Info);
	testRequire(!xrpcDispatch(Input, NULL, Alias.Scratch, 2, NULL, NULL, rpcDispatchSink, &Fixture, &Alias.Info), "Info/scratch overlap accepted");
	strcpy(Alias.Input, Input.Data);
	testRequire(!xrpcDispatch((xstrview){Alias.Input, Input.Size}, NULL, Alias.Scratch, 2, NULL, NULL, rpcDispatchSink, &Fixture, &Info), "input/scratch overlap accepted");
	xrpcDispatchConfigInit(&Alias.Config);
	testRequire(!xrpcDispatch(Input, &Alias.Config, Alias.Scratch, 2, NULL, NULL, rpcDispatchSink, &Fixture, &Info), "config/scratch overlap accepted");
	Alias.Info.Size = sizeof(Alias.Info);
	testRequire(!xrpcDispatch(Input, &Alias.Config, Scratch, 2, NULL, NULL, rpcDispatchSink, &Fixture, &Alias.Info), "config/Info overlap accepted"); xrtClearError();
	Info.Size = sizeof(Info); Fixture = (rpcdispatchfixture){0};
	testRequire(xrpcDispatch(Input, NULL, Scratch, 2, NULL, NULL, rpcDispatchSink, &Fixture, &Info) &&
		Info.Invoked == 0 && Info.Replies == 1 && strstr(Fixture.Output, "-32601"), "NULL method callback not treated as unknown");
}

static uint32 rpcDispatchRandom(uint32* State) { *State = *State * 1664525u + 1013904223u; return *State; }
static void testRpcDispatchProperties(void)
{
	uint32 State = 0x142857u;
	static const cstr Ids[] = {"null", "-0", "1e9999", "\"a\\u0000b\"", "1.25"};
	for (unsigned Trial = 0; Trial < 10000; ++Trial) {
		char Input[4096], Expected[8192]; size_t In = 0, Out = 0, Replies = 0, Calls = 0, Count = 1u + rpcDispatchRandom(&State) % 8u;
		Input[In++] = '[';
		for (size_t Index = 0; Index < Count; ++Index) {
			unsigned Kind = rpcDispatchRandom(&State) % 5u; cstr Id = Ids[rpcDispatchRandom(&State) % 5u];
			if (Index) Input[In++] = ',';
			if (Kind == 2) Input[In++] = '1';
			else {
				cstr Method = Kind == 3 ? "unknown" : Kind == 4 ? "silent" : "echo";
				int Added = snprintf(Input + In, sizeof(Input) - In, "{\"jsonrpc\":\"2.0\",\"method\":\"%s\"%s%s}", Method, Kind == 1 ? "" : ",\"id\":", Kind == 1 ? "" : Id);
				testRequire(Added > 0 && (size_t)Added < sizeof(Input) - In, "dispatch property input capacity"); In += (size_t)Added; ++Calls;
			}
			if (Kind != 1) {
				Expected[Out++] = Replies++ ? ',' : '[';
				int Added = Kind == 0 ? snprintf(Expected + Out, sizeof(Expected) - Out, "{\"jsonrpc\":\"2.0\",\"result\":null,\"id\":%s}", Id) :
					snprintf(Expected + Out, sizeof(Expected) - Out, "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":%d,\"message\":\"%s\"},\"id\":%s}",
					Kind == 2 ? -32600 : Kind == 3 ? -32601 : -32603, Kind == 2 ? "Invalid Request" : Kind == 3 ? "Method not found" : "Internal error", Kind == 2 ? "null" : Id);
				testRequire(Added > 0 && (size_t)Added < sizeof(Expected) - Out, "dispatch property output capacity"); Out += (size_t)Added;
			}
		}
		Input[In++] = ']'; Input[In] = 0; if (Replies) Expected[Out++] = ']'; Expected[Out] = 0;
		rpcDispatchExpected(Input, Expected, Calls, Count, Replies, true, false);
		if (Trial % 10u == 0) {
			/* A valid earlier item must never execute when the complete document
			 * has a malformed tail. This oracle does not use the codec classifier. */
			Input[In - 1u] = ','; Input[In] = 0;
			rpcDispatchExpected(Input, "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"Parse error\"},\"id\":null}", 0, 0, 1, false, true);
		}
	}
}

static void testRpcDispatchAllocation(void)
{
	xstrview Input = XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"params\":[1e9999,\"\\u0000\"],\"id\":1e99999}");
	rpcdispatchfixture Fixture = {0}; xrpcdispatchinfo Info = {0};
	testRequire(rpcDispatchRun(Input, NULL, &Fixture, &Info), "dispatch allocation warmup");
	xrtSetErrorInfo(XERR_VALUE, "xrt.test", 777, "ambient"); const xerror* Ambient = xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before, After; xrtMemStatsEnable(true); xrtMemStatsGet(&Before);
	#endif
	for (unsigned Index = 0; Index < 10000; ++Index) {
		Fixture = (rpcdispatchfixture){0};
		testRequire(rpcDispatchRun(Input, NULL, &Fixture, &Info) && xrtGetError() == Ambient, "dispatch success changed ambient error");
	}
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After); xrtMemStatsEnable(false);
	testRequire(After.BlockAllocCalls == Before.BlockAllocCalls && After.MallocCalls == Before.MallocCalls && After.CallocCalls == Before.CallocCalls &&
		After.ReallocCalls == Before.ReallocCalls, "dispatch allocated on ordinary success");
	#endif
	Fixture = (rpcdispatchfixture){0};
	testRequire(rpcDispatchRun(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"ambient\",\"id\":1}"), NULL, &Fixture, &Info) &&
		xrtGetError() == Ambient, "handler ambient error leaked across successful dispatch"); xrtClearError();
	xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
	testRequire(xrtAllocationBudgetCreate(1, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "dispatch quota enter");
	Fixture = (rpcdispatchfixture){0}; testRequire(rpcDispatchRun(Input, NULL, &Fixture, &Info), "dispatch ordinary success under one-byte quota");
	Fixture = (rpcdispatchfixture){0}; Info.Messages = 12345;
	testRequire(!rpcDispatchRun(XRT_STR_LITERAL("{"), NULL, &Fixture, &Info) && !Fixture.Calls && !Fixture.Emitted &&
		Info.Messages == 12345 && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "parse diagnostic OOM mislabeled wire Parse error");
	Fixture = (rpcdispatchfixture){0};
	testRequire(!rpcDispatchRun(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"bad\",\"id\":1}"), NULL, &Fixture, &Info) &&
		Info.Invoked == 1 && Info.Replies == 0 && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "reply diagnostic OOM lost sticky local failure");
	Fixture = (rpcdispatchfixture){0};
	testRequire(!rpcDispatchRun(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"abort\",\"id\":1}"), NULL, &Fixture, &Info) &&
		Info.Invoked == 1 && Info.Replies == 0 && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "abort diagnostic OOM did not stop locally");
	testRequire(xrtAllocationBudgetLeave(&Scope), "dispatch quota leave"); xallocationbudgetinfo BudgetInfo = {0}; BudgetInfo.Size = sizeof(BudgetInfo);
	testRequire(xrtAllocationBudgetInfo(Budget, &BudgetInfo) && BudgetInfo.LiveBytes == 0 && BudgetInfo.Rejections, "dispatch quota leak");
	xrtClearError(); xrtAllocationBudgetFree(Budget);
}

int main(void)
{
	testRpcDispatchWire(); testRpcDispatchFailures(); testRpcDispatchLimits(); testRpcDispatchAbort(); testRpcDispatchApi(); testRpcDispatchProperties(); testRpcDispatchAllocation();
	puts("[PASS] xrpc dispatch: full-validation/batches/notifications/exact-id/stack-lifetime/reentry/sticky-failure/limits/10000-properties/zero-heap/OOM"); return 0;
}
