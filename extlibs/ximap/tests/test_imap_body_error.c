#include "test.h"

typedef struct testbodyerrorcase {
	cstr Name;
	xstrview Text;
	xerrkind Expected;
} testbodyerrorcase;

static const testbodyerrorcase TestBodyErrors[] = {
	{ "outer-list", XRT_STR_LITERAL("("), XERR_PROTOCOL },
	{ "quoted-escape", XRT_STR_LITERAL("(\"bad\\q\")"), XERR_PROTOCOL },
	{ "missing-fields", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\")"), XERR_PROTOCOL },
	{ "parameter-value", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" (\"CHARSET\") NIL NIL \"7BIT\" 1 1)"), XERR_PROTOCOL },
	{ "number-overflow", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 18446744073709551616 1)"), XERR_RANGE },
	{ "multipart-child", XRT_STR_LITERAL("((\"TEXT\") \"MIXED\")"), XERR_PROTOCOL },
	{ "message-child", XRT_STR_LITERAL("(\"MESSAGE\" \"RFC822\" NIL NIL NIL \"7BIT\" 10 (NIL NIL NIL NIL NIL NIL NIL NIL NIL NIL) (\"TEXT\") 1)"), XERR_PROTOCOL },
	{ "md5", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 (NIL))"), XERR_PROTOCOL },
	{ "disposition", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL (\"INLINE\"))"), XERR_PROTOCOL },
	{ "language", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL NIL ())"), XERR_PROTOCOL },
	{ "location", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL NIL NIL 9)"), XERR_PROTOCOL },
	{ "extension", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL NIL NIL NIL ())"), XERR_PROTOCOL },
	{ "trailing-quoted", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1) \"unterminated"), XERR_PROTOCOL },
	{ "type-separator", XRT_STR_LITERAL("(\"TEXT\"\"PLAIN\" NIL NIL NIL \"7BIT\" 1 1)"), XERR_PROTOCOL },
	{ "parameter-separator", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" (\"CHARSET\"\"UTF-8\") NIL NIL \"7BIT\" 1 1)"), XERR_PROTOCOL },
	{ "disposition-separator", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL(\"INLINE\" NIL))"), XERR_PROTOCOL },
	{ "language-separator", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL NIL (\"en\"\"zh\"))"), XERR_PROTOCOL },
	{ "location-separator", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL NIL (\"en\")\"/body\")"), XERR_PROTOCOL },
	{ "extension-separator", XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL NIL NIL NIL (\"one\")(\"two\"))"), XERR_PROTOCOL },
	{ "multipart-subtype-separator", XRT_STR_LITERAL("((\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1)\"MIXED\")"), XERR_PROTOCOL }
};

static void testBodyDiagnostic(cstr Name, bool Inject, bool Result, bool Unchanged, xerrkind Expected)
{
	bool Triggered = xrtMemDebugFailTriggered();
	xerrkind Kind = xrtErrorKind(xrtGetError());
	xrtMemDebugFailClear();
	printf("[diagnostic] BODY %s/%s triggered=%d kind=%d expected=%d result=%d unchanged=%d\n",
		Name, Inject ? "oom" : "control", (int)Triggered, (int)Kind,
		(int)(Inject ? XERR_MEMORY : Expected), (int)Result, (int)Unchanged);
	testRequire(!Result && Unchanged && Kind == (Inject ? XERR_MEMORY : Expected) &&
		Triggered == Inject, "BODY parser replaced a diagnostic or published partial output");
	xrtClearError();
	testRequire(xrtMemDebugReset(), "BODY diagnostic retained logical allocations");
	printf("[PASS] BODY %s/%s\n", Name, Inject ? "oom" : "control");
}

static void testBodyParseError(size_t Index, bool Inject)
{
	ximapbodyview Body, Original;
	memset(&Body, 0xa5, sizeof(Body));
	Original = Body;
	xrtClearError();
	if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "BODY diagnostic injection failed");
	bool Result = xrtImapBodyParse(TestBodyErrors[Index].Text, &Body);
	testBodyDiagnostic(TestBodyErrors[Index].Name, Inject, Result,
		memcmp(&Body, &Original, sizeof(Body)) == 0, TestBodyErrors[Index].Expected);
}

/* A valid zero-allocation parse keeps the caller's diagnostic even with a
 * logical failure armed; adjacency of multipart children is legal IMAP. */
static void testBodyValid(void)
{
	static const xstrview Inputs[] = {
		XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1)"),
		XRT_STR_LITERAL("((\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1)(\"APPLICATION\" \"JSON\" NIL NIL NIL \"8BIT\" 2) \"MIXED\")"),
		XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" (\"CHARSET\" \"UTF-8\") NIL NIL \"7BIT\" 1 1 NIL (\"INLINE\" NIL) (\"en\" \"zh\") NIL (\"future\" 9))")
	};
	for ( size_t i = 0; i < sizeof(Inputs) / sizeof(Inputs[0]); i++ ) {
		ximapbodyview Body;
		xrtSetErrorInfo(XERR_ARGUMENT, "test.body", 91, "caller diagnostic");
		const xerror* pOriginal = xrtGetError();
		testRequire(pOriginal != NULL && xrtMemDebugFailAfter(0), "BODY valid control setup failed");
		bool Result = xrtImapBodyParse(Inputs[i], &Body);
		bool Triggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		testRequire(Result && !Triggered && xrtGetError() == pOriginal,
			"BODY valid parse allocated or replaced the caller diagnostic");
		if ( i == 1u ) testRequire(Body.Kind == XIMAP_BODY_MULTIPART && Body.ChildCount == 2u,
			"BODY rejected adjacent multipart children");
		xrtClearError();
		testRequire(xrtMemDebugReset(), "BODY valid control retained allocations");
		printf("[PASS] BODY valid-%zu/no-allocation\n", i);
	}
}

/* Public borrowed-view cursors must keep child failures and must not write
 * their result over text that the result itself still borrows. */
static void testBodyCursors(bool Inject)
{
	ximapbodyview Parent, Body, Original;
	ximapbodycursor Cursor;
	memset(&Parent, 0, sizeof(Parent));
	Parent.Kind = XIMAP_BODY_MULTIPART;
	Parent.ChildCount = 1u;
	Parent.Children = XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\")");
	testRequire(xrtImapBodyChildCursorInit(&Cursor, &Parent), "BODY child diagnostic setup failed");
	memset(&Body, 0xa5, sizeof(Body)); Original = Body;
	xrtClearError();
	if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "BODY child injection failed");
	xmailnext Next = xrtImapBodyChildNext(&Cursor, &Body);
	testBodyDiagnostic("child-cursor", Inject, Next != XMAIL_NEXT_ERROR,
		memcmp(&Body, &Original, sizeof(Body)) == 0 && Cursor.Remaining == 1u, XERR_PROTOCOL);

	ximapbodyparamcursor Parameters;
	ximapbodyparam Parameter, OriginalParameter;
	testRequire(xrtImapDataCursorInit(&Parameters.Data, XRT_STR_LITERAL("\"CHARSET\"")),
		"BODY parameter diagnostic setup failed");
	memset(&Parameter, 0xa5, sizeof(Parameter)); OriginalParameter = Parameter;
	xrtClearError();
	if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "BODY parameter injection failed");
	Next = xrtImapBodyParamNext(&Parameters, &Parameter);
	testBodyDiagnostic("parameter-cursor", Inject, Next != XMAIL_NEXT_ERROR,
		memcmp(&Parameter, &OriginalParameter, sizeof(Parameter)) == 0, XERR_PROTOCOL);

	ximapdataview View;
	memset(&View, 0, sizeof(View));
	View.Kind = XIMAP_DATA_LIST;
	View.Value = XRT_STR_LITERAL("\"CHARSET\"");
	memset(&Parameters, 0xa5, sizeof(Parameters));
	ximapbodyparamcursor OriginalCursor = Parameters;
	xrtClearError();
	if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "BODY parameter init injection failed");
	bool Result = xrtImapBodyParamCursorInit(&Parameters, &View);
	testBodyDiagnostic("parameter-init", Inject, Result,
		memcmp(&Parameters, &OriginalCursor, sizeof(Parameters)) == 0, XERR_PROTOCOL);
}

static void testBodyOverlap(bool Inject)
{
	union { ximapbodyview Body; unsigned char Bytes[sizeof(ximapbodyview) + 128u]; } Storage;
	unsigned char Before[sizeof(Storage)];
	static const char Text[] = "(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1)";
	ximapbodyview Parent;
	ximapbodycursor Cursor;
	memset(&Storage, 0xa5, sizeof(Storage));
	memcpy(Storage.Bytes, Text, sizeof(Text));
	memcpy(Before, &Storage, sizeof(Storage));
	memset(&Parent, 0, sizeof(Parent));
	Parent.Kind = XIMAP_BODY_MULTIPART; Parent.ChildCount = 1u;
	Parent.Children = testMailViewN((cstr)Storage.Bytes, sizeof(Text) - 1u);
	testRequire(xrtImapBodyChildCursorInit(&Cursor, &Parent), "BODY overlap child setup failed");
	xrtClearError();
	if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "BODY overlap injection failed");
	xmailnext Next = xrtImapBodyChildNext(&Cursor, &Storage.Body);
	testBodyDiagnostic("child-overlap", Inject, Next != XMAIL_NEXT_ERROR,
		memcmp(Before, &Storage, sizeof(Storage)) == 0 && Cursor.Remaining == 1u &&
		Cursor.Data.Position == 0, XERR_ARGUMENT);

	union { ximapbodyparam Parameter; unsigned char Bytes[sizeof(ximapbodyparam) + 32u]; } Pair;
	unsigned char OriginalPair[sizeof(Pair)];
	static const char Values[] = "\"CHARSET\" \"UTF-8\"";
	ximapbodyparamcursor Parameters;
	memset(&Pair, 0xa5, sizeof(Pair));
	memcpy(Pair.Bytes, Values, sizeof(Values));
	memcpy(OriginalPair, &Pair, sizeof(Pair));
	testRequire(xrtImapDataCursorInit(&Parameters.Data,
		testMailViewN((cstr)Pair.Bytes, sizeof(Values) - 1u)), "BODY pair overlap setup failed");
	xrtClearError();
	if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "BODY pair overlap injection failed");
	Next = xrtImapBodyParamNext(&Parameters, &Pair.Parameter);
	testBodyDiagnostic("parameter-overlap", Inject, Next != XMAIL_NEXT_ERROR,
		memcmp(OriginalPair, &Pair, sizeof(Pair)) == 0 && Parameters.Data.Position == 0, XERR_ARGUMENT);
}

int main(int argc, char** argv)
{
	if ( argc == 3 ) {
		bool Inject = strcmp(argv[1], "oom") == 0;
		testRequire(Inject || strcmp(argv[1], "control") == 0, "BODY usage: [control|oom case]");
		for ( size_t i = 0; i < sizeof(TestBodyErrors) / sizeof(TestBodyErrors[0]); i++ )
			if ( strcmp(argv[2], TestBodyErrors[i].Name) == 0 ) {
				testBodyParseError(i, Inject);
				return 0;
			}
		testRequire(false, "unknown BODY diagnostic case");
	}
	testRequire(argc == 1, "BODY usage: [control|oom case]");
	testBodyValid();
	for ( size_t i = 0; i < sizeof(TestBodyErrors) / sizeof(TestBodyErrors[0]); i++ ) {
		testBodyParseError(i, false);
		testBodyParseError(i, true);
	}
	testBodyCursors(false); testBodyCursors(true);
	testBodyOverlap(false); testBodyOverlap(true);
	return 0;
}
