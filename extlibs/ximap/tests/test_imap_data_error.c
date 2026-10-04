#include "test.h"

typedef enum testdataoperation {
	TEST_LIST, TEST_FLAGS_INIT, TEST_STATUS, TEST_SEARCH_INIT, TEST_ESEARCH,
	TEST_FETCH, TEST_FLAGS_NEXT, TEST_STATUS_NEXT, TEST_SEARCH_NEXT, TEST_ESEARCH_NEXT,
	TEST_FETCH_NEXT
} testdataoperation;

typedef struct testdataerrorcase {
	cstr Name;
	testdataoperation Operation;
	xstrview Text;
	xerrkind Expected;
} testdataerrorcase;

static const testdataerrorcase TestDataErrors[] = {
	{ "list-prefix", TEST_LIST, XRT_STR_LITERAL("BAD () NIL inbox"), XERR_PROTOCOL },
	{ "list-attributes", TEST_LIST, XRT_STR_LITERAL("LIST ("), XERR_PROTOCOL },
	{ "list-delimiter", TEST_LIST, XRT_STR_LITERAL("LIST () \"bad\\q\" inbox"), XERR_PROTOCOL },
	{ "list-mailbox", TEST_LIST, XRT_STR_LITERAL("LIST () NIL"), XERR_PROTOCOL },
	{ "list-separator", TEST_LIST, XRT_STR_LITERAL("LIST ()\"/\" inbox"), XERR_PROTOCOL },
	{ "list-mailbox-separator", TEST_LIST, XRT_STR_LITERAL("LIST () \"/\"\"inbox\""), XERR_PROTOCOL },
	{ "flags-list", TEST_FLAGS_INIT, XRT_STR_LITERAL("("), XERR_PROTOCOL },
	{ "flags-trailing-quoted", TEST_FLAGS_INIT, XRT_STR_LITERAL("() \"bad\\q\""), XERR_PROTOCOL },
	{ "status-prefix", TEST_STATUS, XRT_STR_LITERAL("BAD inbox ()"), XERR_PROTOCOL },
	{ "status-mailbox", TEST_STATUS, XRT_STR_LITERAL("STATUS \"bad\\q\" ()"), XERR_PROTOCOL },
	{ "status-items", TEST_STATUS, XRT_STR_LITERAL("STATUS inbox ("), XERR_PROTOCOL },
	{ "status-trailing", TEST_STATUS, XRT_STR_LITERAL("STATUS inbox () \"bad\\q\""), XERR_PROTOCOL },
	{ "status-separator", TEST_STATUS, XRT_STR_LITERAL("STATUS \"inbox\"()"), XERR_PROTOCOL },
	{ "search-prefix", TEST_SEARCH_INIT, XRT_STR_LITERAL("BAD 1"), XERR_PROTOCOL },
	{ "esearch-correlator", TEST_ESEARCH, XRT_STR_LITERAL("ESEARCH (BAD \"A1\") COUNT 1"), XERR_PROTOCOL },
	{ "esearch-correlator-missing", TEST_ESEARCH, XRT_STR_LITERAL("ESEARCH (TAG) COUNT 1"), XERR_PROTOCOL },
	{ "esearch-correlator-extra", TEST_ESEARCH, XRT_STR_LITERAL("ESEARCH (TAG \"A1\" extra) COUNT 1"), XERR_PROTOCOL },
	{ "esearch-correlator-separator", TEST_ESEARCH, XRT_STR_LITERAL("ESEARCH (TAG\"A1\") COUNT 1"), XERR_PROTOCOL },
	{ "esearch-uid-separator", TEST_ESEARCH, XRT_STR_LITERAL("ESEARCH (TAG \"A1\")UID COUNT 1"), XERR_PROTOCOL },
	{ "fetch-sequence-range", TEST_FETCH, XRT_STR_LITERAL("18446744073709551616 FETCH ()"), XERR_RANGE },
	{ "fetch-sequence-separator", TEST_FETCH, XRT_STR_LITERAL("1FETCH ()"), XERR_PROTOCOL },
	{ "flags-item", TEST_FLAGS_NEXT, XRT_STR_LITERAL("\"flag\""), XERR_PROTOCOL },
	{ "status-value-range", TEST_STATUS_NEXT, XRT_STR_LITERAL("MESSAGES 18446744073709551616"), XERR_RANGE },
	{ "status-value-quoted", TEST_STATUS_NEXT, XRT_STR_LITERAL("X-VENDOR \"bad\\q\""), XERR_PROTOCOL },
	{ "status-value-missing", TEST_STATUS_NEXT, XRT_STR_LITERAL("MESSAGES"), XERR_PROTOCOL },
	{ "status-value-separator", TEST_STATUS_NEXT, XRT_STR_LITERAL("X-VENDOR(1)"), XERR_PROTOCOL },
	{ "search-modseq-range", TEST_SEARCH_NEXT, XRT_STR_LITERAL("(MODSEQ 18446744073709551616)"), XERR_RANGE },
	{ "search-modseq-missing", TEST_SEARCH_NEXT, XRT_STR_LITERAL("(MODSEQ)"), XERR_PROTOCOL },
	{ "search-modseq-tail", TEST_SEARCH_NEXT, XRT_STR_LITERAL("(MODSEQ 9) 1"), XERR_PROTOCOL },
	{ "search-zero-id", TEST_SEARCH_NEXT, XRT_STR_LITERAL("0"), XERR_PROTOCOL },
	{ "search-large-id", TEST_SEARCH_NEXT, XRT_STR_LITERAL("4294967296"), XERR_RANGE },
	{ "esearch-value-range", TEST_ESEARCH_NEXT, XRT_STR_LITERAL("COUNT 18446744073709551616"), XERR_RANGE },
	{ "esearch-value-quoted", TEST_ESEARCH_NEXT, XRT_STR_LITERAL("X-VENDOR \"bad\\q\""), XERR_PROTOCOL },
	{ "esearch-value-missing", TEST_ESEARCH_NEXT, XRT_STR_LITERAL("COUNT"), XERR_PROTOCOL },
	{ "esearch-value-separator", TEST_ESEARCH_NEXT, XRT_STR_LITERAL("X-VENDOR(1)"), XERR_PROTOCOL },
	{ "fetch-value-missing", TEST_FETCH_NEXT, XRT_STR_LITERAL("(UID "), XERR_PROTOCOL },
	{ "fetch-value-quoted", TEST_FETCH_NEXT, XRT_STR_LITERAL("(X-VENDOR \"bad\\q\")"), XERR_PROTOCOL },
	{ "fetch-value-range", TEST_FETCH_NEXT, XRT_STR_LITERAL("(UID 18446744073709551616)"), XERR_RANGE },
	{ "fetch-attribute", TEST_FETCH_NEXT, XRT_STR_LITERAL("(BODY[HEADER 1)"), XERR_PROTOCOL },
	{ "list-empty-delimiter", TEST_LIST, XRT_STR_LITERAL("LIST () \"\" inbox"), XERR_PROTOCOL },
	{ "list-long-delimiter", TEST_LIST, XRT_STR_LITERAL("LIST () \"//\" inbox"), XERR_PROTOCOL },
	{ "list-surrogate-delimiter", TEST_LIST, XRT_STR_LITERAL("LIST () \"\xed\xa0\x80\" inbox"), XERR_PROTOCOL },
	{ "list-overlong-delimiter", TEST_LIST, XRT_STR_LITERAL("LIST () \"\xc0\xaf\" inbox"), XERR_PROTOCOL },
	{ "status-count-range", TEST_STATUS_NEXT, XRT_STR_LITERAL("MESSAGES 4294967296"), XERR_RANGE },
	{ "status-uidnext-zero", TEST_STATUS_NEXT, XRT_STR_LITERAL("UIDNEXT 0"), XERR_PROTOCOL },
	{ "esearch-min-zero", TEST_ESEARCH_NEXT, XRT_STR_LITERAL("MIN 0"), XERR_PROTOCOL },
	{ "esearch-max-range", TEST_ESEARCH_NEXT, XRT_STR_LITERAL("MAX 4294967296"), XERR_RANGE },
	{ "esearch-count-type", TEST_ESEARCH_NEXT, XRT_STR_LITERAL("COUNT \"1\""), XERR_PROTOCOL },
	{ "esearch-all-dollar", TEST_ESEARCH_NEXT, XRT_STR_LITERAL("ALL $"), XERR_PROTOCOL },
	{ "esearch-all-zero", TEST_ESEARCH_NEXT, XRT_STR_LITERAL("ALL 0"), XERR_PROTOCOL },
	{ "esearch-empty-tag", TEST_ESEARCH, XRT_STR_LITERAL("ESEARCH (TAG \"\") COUNT 1"), XERR_PROTOCOL },
	{ "esearch-invalid-tag", TEST_ESEARCH, XRT_STR_LITERAL("ESEARCH (TAG \"A+1\") COUNT 1"), XERR_PROTOCOL }
};

/* Print observations before asserting, so the same public API probe can prove
 * a pre-fix failure without modifying or replacing the original runtime. */
static void testDataDiagnostic(cstr Name, bool Inject, bool Failed, bool Unchanged, xerrkind Expected)
{
	bool Triggered = xrtMemDebugFailTriggered();
	xerrkind Kind = xrtErrorKind(xrtGetError());
	xrtMemDebugFailClear();
	printf("[diagnostic] DATA %s/%s triggered=%d kind=%d expected=%d failed=%d unchanged=%d\n",
		Name, Inject ? "oom" : "control", (int)Triggered, (int)Kind,
		(int)(Inject ? XERR_MEMORY : Expected), (int)Failed, (int)Unchanged);
	testRequire(Failed && Unchanged && Kind == (Inject ? XERR_MEMORY : Expected) && Triggered == Inject,
		"IMAP data failure lost its diagnostic, cursor state or output");
	xrtClearError();
	testRequire(xrtMemDebugReset(), "IMAP data failure retained logical allocations");
	printf("[PASS] DATA %s/%s\n", Name, Inject ? "oom" : "control");
}

static void testDataError(size_t Index, bool Inject)
{
	const testdataerrorcase* Case = &TestDataErrors[Index];
	union {
		ximaplistview List; ximapflagcursor Flags; ximapmailboxstatusview Status;
		ximapsearchcursor Search; ximapesearchview ESearch; ximapfetchview Fetch;
		xstrview Flag; ximapstatusitem StatusItem; ximapsearchitem SearchItem;
		ximapesearchitem EItem; ximapfetchitem FItem;
	} Output;
	unsigned char Before[sizeof(Output)];
	union { ximapflagcursor Flags; ximapstatuscursor Status; ximapsearchcursor Search;
		ximapesearchcursor ESearch; ximapfetchcursor Fetch; } Cursor;
	unsigned char CursorBefore[sizeof(Cursor)];
	bool IsCursor = Case->Operation >= TEST_FLAGS_NEXT;
	memset(&Output, 0xa5, sizeof(Output)); memcpy(Before, &Output, sizeof(Output));
	memset(&Cursor, 0xa5, sizeof(Cursor));
	if ( IsCursor ) {
		if ( Case->Operation == TEST_FETCH_NEXT ) {
			ximapfetchview View; memset(&View, 0, sizeof(View)); View.Items = Case->Text;
			testRequire(xrtImapFetchCursorInit(&Cursor.Fetch, &View), "DATA fetch setup failed");
		} else testRequire(xrtImapDataCursorInit(&Cursor.Flags.Data, Case->Text), "DATA cursor setup failed");
	}
	memcpy(CursorBefore, &Cursor, sizeof(Cursor));
	xrtClearError();
	if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA diagnostic injection failed");
	bool Failed = false;
	switch ( Case->Operation ) {
	case TEST_LIST: Failed = !xrtImapListParse(Case->Text, &Output.List); break;
	case TEST_FLAGS_INIT: Failed = !xrtImapFlagCursorInit(&Output.Flags, Case->Text); break;
	case TEST_STATUS: Failed = !xrtImapStatusParse(Case->Text, &Output.Status); break;
	case TEST_SEARCH_INIT: Failed = !xrtImapSearchCursorInit(&Output.Search, Case->Text); break;
	case TEST_ESEARCH: Failed = !xrtImapESearchParse(Case->Text, &Output.ESearch); break;
	case TEST_FETCH: Failed = !xrtImapFetchParse(Case->Text, &Output.Fetch); break;
	case TEST_FLAGS_NEXT: Failed = xrtImapFlagNext(&Cursor.Flags, &Output.Flag) == XMAIL_NEXT_ERROR; break;
	case TEST_STATUS_NEXT: Failed = xrtImapStatusNext(&Cursor.Status, &Output.StatusItem) == XMAIL_NEXT_ERROR; break;
	case TEST_SEARCH_NEXT: Failed = xrtImapSearchNext(&Cursor.Search, &Output.SearchItem) == XMAIL_NEXT_ERROR; break;
	case TEST_ESEARCH_NEXT: Failed = xrtImapESearchNext(&Cursor.ESearch, &Output.EItem) == XMAIL_NEXT_ERROR; break;
	case TEST_FETCH_NEXT: Failed = xrtImapFetchNext(&Cursor.Fetch, &Output.FItem) == XMAIL_NEXT_ERROR; break;
	}
	testDataDiagnostic(Case->Name, Inject, Failed, memcmp(Before, &Output, sizeof(Output)) == 0 &&
		(!IsCursor || memcmp(CursorBefore, &Cursor, sizeof(Cursor)) == 0), Case->Expected);
}

static void testDataBorrowed(bool Inject)
{
	union { ximapstatusitem Item; ximapesearchitem EItem; xstrview Flag;
		ximapflagcursor FlagCursor; ximapfetchcursor FetchCursor;
		ximapsearchitem Search; unsigned char Bytes[512]; } Storage;
	unsigned char Before[sizeof(Storage)];
	ximapstatuscursor Status;
	memset(&Storage, 0xa5, sizeof(Storage)); memcpy(Storage.Bytes, "MESSAGES 1", 10);
	testRequire(xrtImapDataCursorInit(&Status.Data, testMailViewN((cstr)Storage.Bytes, 10)), "DATA alias setup failed");
	memcpy(Before, &Storage, sizeof(Storage));
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA alias injection failed");
	xmailnext Next = xrtImapStatusNext(&Status, &Storage.Item);
	testDataDiagnostic("status-output-overlap", Inject, Next == XMAIL_NEXT_ERROR,
		memcmp(Before, &Storage, sizeof(Storage)) == 0 && Status.Data.Position == 0, XERR_ARGUMENT);

	ximapesearchcursor Search;
	memset(&Storage, 0xa5, sizeof(Storage)); memcpy(Storage.Bytes, "COUNT 1", 7);
	testRequire(xrtImapDataCursorInit(&Search.Data, testMailViewN((cstr)Storage.Bytes, 7)), "DATA ESEARCH alias setup failed");
	memcpy(Before, &Storage, sizeof(Storage));
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA alias injection failed");
	Next = xrtImapESearchNext(&Search, &Storage.EItem);
	testDataDiagnostic("esearch-output-overlap", Inject, Next == XMAIL_NEXT_ERROR,
		memcmp(Before, &Storage, sizeof(Storage)) == 0 && Search.Data.Position == 0, XERR_ARGUMENT);

	ximapflagcursor Flags;
	memset(&Storage, 0xa5, sizeof(Storage)); memcpy(Storage.Bytes, "\\Seen", 5);
	testRequire(xrtImapDataCursorInit(&Flags.Data, testMailViewN((cstr)Storage.Bytes, 5)), "DATA flag alias setup failed");
	memcpy(Before, &Storage, sizeof(Storage));
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA alias injection failed");
	Next = xrtImapFlagNext(&Flags, &Storage.Flag);
	testDataDiagnostic("flags-output-overlap", Inject, Next == XMAIL_NEXT_ERROR,
		memcmp(Before, &Storage, sizeof(Storage)) == 0 && Flags.Data.Position == 0, XERR_ARGUMENT);

	ximapsearchcursor Ids;
	memset(&Storage, 0xa5, sizeof(Storage)); memcpy(Storage.Bytes, "1 2", 3);
	testRequire(xrtImapDataCursorInit(&Ids.Data, testMailViewN((cstr)Storage.Bytes, 3)), "DATA SEARCH alias setup failed");
	memcpy(Before, &Storage, sizeof(Storage));
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA alias injection failed");
	Next = xrtImapSearchNext(&Ids, &Storage.Search);
	testDataDiagnostic("search-output-overlap", Inject, Next == XMAIL_NEXT_ERROR,
		memcmp(Before, &Storage, sizeof(Storage)) == 0 && Ids.Data.Position == 0, XERR_ARGUMENT);

	ximapfetchview Fetch;
	memset(&Storage, 0xa5, sizeof(Storage)); memcpy(Storage.Bytes, "(UID 1)", 7);
	memset(&Fetch, 0, sizeof(Fetch)); Fetch.Items = testMailViewN((cstr)Storage.Bytes, 7);
	memcpy(Before, &Storage, sizeof(Storage));
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA alias injection failed");
	bool Result = xrtImapFetchCursorInit(&Storage.FetchCursor, &Fetch);
	testDataDiagnostic("fetch-init-overlap", Inject, !Result,
		memcmp(Before, &Storage, sizeof(Storage)) == 0, XERR_ARGUMENT);

	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA argument injection failed");
	Result = xrtImapSearchCursorInit(NULL, XRT_STR_LITERAL("SEARCH"));
	testDataDiagnostic("search-null-init", Inject, !Result, true, XERR_ARGUMENT);
}

static void testDataString(bool Inject)
{
	ximapdataview Value;
	memset(&Value, 0, sizeof(Value)); Value.Kind = XIMAP_DATA_ATOM;
	Value.Source = Value.Value = XRT_STR_LITERAL("abc");
	unsigned char Before[sizeof(Value)]; memcpy(Before, &Value, sizeof(Value));
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA string injection failed");
	bool Result = xrtImapStringWrite(&Value, NULL, 0, &Value.Source.Size);
	testDataDiagnostic("string-size-view-overlap", Inject, !Result,
		memcmp(Before, &Value, sizeof(Value)) == 0, XERR_ARGUMENT);

	union { size_t Size; unsigned char Bytes[32]; } Text;
	memset(&Text, 0xa5, sizeof(Text)); memcpy(Text.Bytes, "abc", 3);
	unsigned char Original[sizeof(Text)]; memcpy(Original, &Text, sizeof(Text));
	Value.Source = testMailViewN(NULL, 0); Value.Value = testMailViewN((cstr)Text.Bytes, 3);
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA string injection failed");
	Result = xrtImapStringWrite(&Value, NULL, 0, &Text.Size);
	testDataDiagnostic("string-size-value-overlap", Inject, !Result,
		memcmp(Original, &Text, sizeof(Text)) == 0, XERR_ARGUMENT);

	union { size_t Size; char Bytes[32]; } Output;
	memset(&Output, 0xa5, sizeof(Output)); unsigned char BufferBefore[sizeof(Output)];
	memcpy(BufferBefore, &Output, sizeof(Output));
	Value.Source = Value.Value = XRT_STR_LITERAL("abc");
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA string injection failed");
	Result = xrtImapStringWrite(&Value, Output.Bytes, sizeof(Output), &Output.Size);
	testDataDiagnostic("string-size-buffer-overlap", Inject, !Result,
		memcmp(BufferBefore, &Output, sizeof(Output)) == 0, XERR_ARGUMENT);

	memset(&Text, 0xa5, sizeof(Text)); memcpy(Text.Bytes, "abc", 3);
	memcpy(Original, &Text, sizeof(Text));
	Value.Source = testMailViewN(NULL, 0); Value.Value = testMailViewN((cstr)Text.Bytes, 3);
	size_t Size = 91;
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA string injection failed");
	Result = xrtImapStringWrite(&Value, (char*)Text.Bytes + 1, sizeof(Text) - 1, &Size);
	testDataDiagnostic("string-buffer-value-overlap", Inject, !Result,
		memcmp(Original, &Text, sizeof(Text)) == 0 && Size == 91, XERR_ARGUMENT);

	Value.Source = Value.Value = XRT_STR_LITERAL("abc"); memcpy(Before, &Value, sizeof(Value)); Size = 91;
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA string injection failed");
	Result = xrtImapStringWrite(&Value, (char*)&Value, sizeof(Value), &Size);
	testDataDiagnostic("string-buffer-view-overlap", Inject, !Result,
		memcmp(Before, &Value, sizeof(Value)) == 0 && Size == 91, XERR_ARGUMENT);

	memset(&Output, 0xa5, sizeof(Output)); memcpy(BufferBefore, &Output, sizeof(Output)); Size = 91;
	Value.Kind = XIMAP_DATA_NUMBER;
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA string injection failed");
	Result = xrtImapStringWrite(&Value, Output.Bytes, sizeof(Output), &Size);
	testDataDiagnostic("string-value-type", Inject, !Result,
		memcmp(BufferBefore, &Output, sizeof(Output)) == 0 && Size == 91, XERR_STATE);

	Value.Kind = XIMAP_DATA_QUOTED;
	Value.Source = XRT_STR_LITERAL("\"bad\\q\""); Value.Value = XRT_STR_LITERAL("bad\\q");
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA string injection failed");
	Result = xrtImapStringWrite(&Value, Output.Bytes, sizeof(Output), &Size);
	testDataDiagnostic("string-invalid-escape", Inject, !Result,
		memcmp(BufferBefore, &Output, sizeof(Output)) == 0 && Size == 91, XERR_PROTOCOL);

	Value.Kind = XIMAP_DATA_ATOM; Value.Source = Value.Value = XRT_STR_LITERAL("abc");
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA string injection failed");
	Result = xrtImapStringWrite(&Value, Output.Bytes, 3, &Size);
	/* Insufficient capacity deliberately reports the required size, while the
	 * destination bytes remain untouched. This is the documented exception. */
	testDataDiagnostic("string-capacity", Inject, !Result,
		memcmp(BufferBefore, &Output, sizeof(Output)) == 0 && Size == 3, XERR_RANGE);
}

static void testDataFetchSeparators(bool Inject)
{
	ximapfetchview View; ximapfetchcursor Cursor; ximapfetchitem Item;
	unsigned char OriginalCursor[sizeof(Cursor)], OriginalItem[sizeof(Item)];
	testRequire(xrtImapFetchParse(XRT_STR_LITERAL("1 FETCH (X-VENDOR \"a\"UID 1)"), &View) &&
		xrtImapFetchCursorInit(&Cursor, &View) && xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_ITEM,
		"DATA fetch separator setup failed");
	memcpy(OriginalCursor, &Cursor, sizeof(Cursor)); memcpy(OriginalItem, &Item, sizeof(Item));
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA separator injection failed");
	xmailnext Next = xrtImapFetchNext(&Cursor, &Item);
	testDataDiagnostic("fetch-item-separator", Inject, Next == XMAIL_NEXT_ERROR,
		memcmp(OriginalCursor, &Cursor, sizeof(Cursor)) == 0 && memcmp(OriginalItem, &Item, sizeof(Item)) == 0, XERR_PROTOCOL);

	testRequire(xrtImapFetchParse(XRT_STR_LITERAL("1 FETCH (BODY[] {1}"), &View) &&
		xrtImapFetchCursorInit(&Cursor, &View) && xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_ITEM &&
		xrtImapFetchCursorContinue(&Cursor, XRT_STR_LITERAL("UID 1)")), "DATA continuation separator setup failed");
	memcpy(OriginalCursor, &Cursor, sizeof(Cursor)); memcpy(OriginalItem, &Item, sizeof(Item));
	xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "DATA separator injection failed");
	Next = xrtImapFetchNext(&Cursor, &Item);
	testDataDiagnostic("fetch-continuation-separator", Inject, Next == XMAIL_NEXT_ERROR,
		memcmp(OriginalCursor, &Cursor, sizeof(Cursor)) == 0 && memcmp(OriginalItem, &Item, sizeof(Item)) == 0, XERR_PROTOCOL);
}

/* Successful operations must retain the caller's diagnostic and leave an
 * armed logical allocation failure untouched, including multiple literals. */
static void testDataValid(void)
{
	ximaplistview List; ximapmailboxstatusview Status; ximapesearchview ESearch;
	ximapsearchcursor Search; ximapsearchitem Id;
	static const xstrview Names[] = { XRT_STR_LITERAL("123"), XRT_STR_LITERAL("NIL"),
		XRT_STR_LITERAL("18446744073709551616") };
	for ( size_t i = 0; i < 3; i++ ) {
		char Text[96]; snprintf(Text, sizeof(Text), "LIST () NIL %.*s", (int)Names[i].Size, Names[i].Data);
		xrtSetErrorInfo(XERR_ARGUMENT, "test.data", 91, "caller diagnostic");
		const xerror* Prior = xrtGetError(); testRequire(xrtMemDebugFailAfter(0), "DATA valid setup failed");
		bool Result = xrtImapListParse(testMailView(Text), &List);
		bool Triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
		testRequire(Result && !Triggered && xrtGetError() == Prior && List.Mailbox.Kind == XIMAP_DATA_ATOM &&
			testMailViewEqual(List.Mailbox.Value, Names[i]), "DATA numeric/NIL mailbox was not astring");
		xrtClearError(); testRequire(xrtMemDebugReset(), "DATA valid retained allocations");
		printf("[PASS] DATA valid-mailbox-%zu/no-allocation\n", i);
	}
	xrtSetErrorInfo(XERR_ARGUMENT, "test.data", 91, "caller diagnostic");
	const xerror* Prior = xrtGetError(); testRequire(xrtMemDebugFailAfter(0), "DATA valid setup failed");
	testRequire(xrtImapStatusParse(XRT_STR_LITERAL("STATUS 123 (MESSAGES 0)"), &Status) &&
		xrtImapESearchParse(XRT_STR_LITERAL("ESEARCH (TAG NIL) UID X-VENDOR (1 2)"), &ESearch) &&
		xrtImapSearchCursorInit(&Search, XRT_STR_LITERAL("SEARCH 4294967295 (MODSEQ 18446744073709551615)")) &&
		xrtImapSearchNext(&Search, &Id) == XMAIL_NEXT_ITEM && Id.Number == UINT32_MAX &&
		xrtImapSearchNext(&Search, &Id) == XMAIL_NEXT_ITEM && Id.Kind == XIMAP_SEARCH_MODSEQ && Id.Number == UINT64_MAX &&
		xrtImapSearchNext(&Search, &Id) == XMAIL_NEXT_END, "DATA legal search/status extension control failed");
	testRequire(!xrtMemDebugFailTriggered() && xrtGetError() == Prior, "DATA success replaced diagnostic or allocated");
	xrtMemDebugFailClear(); xrtClearError(); testRequire(xrtMemDebugReset(), "DATA valid retained allocations");
	puts("[PASS] DATA valid-search-status/no-allocation");

	xrtSetErrorInfo(XERR_ARGUMENT, "test.data", 91, "caller diagnostic"); Prior = xrtGetError();
	testRequire(xrtMemDebugFailAfter(0), "DATA scalar control setup failed");
	ximapstatuscursor StatusCursor; ximapstatusitem StatusItem;
	ximapesearchcursor ECursor; ximapesearchitem EItem; ximapflagcursor Flags; xstrview Flag;
	testRequire(xrtImapStatusParse(XRT_STR_LITERAL("STATUS NIL (SIZE 9223372036854775807 UIDVALIDITY 4294967295)"), &Status) &&
		xrtImapStatusCursorInit(&StatusCursor, &Status) && xrtImapStatusNext(&StatusCursor, &StatusItem) == XMAIL_NEXT_ITEM &&
		StatusItem.Value.Number == (uint64)INT64_MAX && xrtImapStatusNext(&StatusCursor, &StatusItem) == XMAIL_NEXT_ITEM &&
		StatusItem.Value.Number == UINT32_MAX && xrtImapStatusNext(&StatusCursor, &StatusItem) == XMAIL_NEXT_END &&
		xrtImapESearchParse(XRT_STR_LITERAL("ESEARCH (TAG 18446744073709551616) UID COUNT 0 MIN 1 MAX 4294967295 X-VENDOR (NIL)"), &ESearch) &&
		xrtImapESearchCursorInit(&ECursor, &ESearch) && xrtImapESearchNext(&ECursor, &EItem) == XMAIL_NEXT_ITEM && EItem.Value.Number == 0 &&
		xrtImapESearchNext(&ECursor, &EItem) == XMAIL_NEXT_ITEM && EItem.Value.Number == 1 &&
		xrtImapESearchNext(&ECursor, &EItem) == XMAIL_NEXT_ITEM && EItem.Value.Number == UINT32_MAX &&
		xrtImapESearchNext(&ECursor, &EItem) == XMAIL_NEXT_ITEM && EItem.Value.Kind == XIMAP_DATA_LIST &&
		xrtImapESearchNext(&ECursor, &EItem) == XMAIL_NEXT_END &&
		xrtImapFlagCursorInit(&Flags, XRT_STR_LITERAL("(\\Seen \\* NIL 18446744073709551616)")) &&
		xrtImapFlagNext(&Flags, &Flag) == XMAIL_NEXT_ITEM && xrtImapFlagNext(&Flags, &Flag) == XMAIL_NEXT_ITEM &&
		xrtImapFlagNext(&Flags, &Flag) == XMAIL_NEXT_ITEM && testMailViewEqual(Flag, XRT_STR_LITERAL("NIL")) &&
		xrtImapFlagNext(&Flags, &Flag) == XMAIL_NEXT_ITEM && testMailViewEqual(Flag, XRT_STR_LITERAL("18446744073709551616")) &&
		xrtImapFlagNext(&Flags, &Flag) == XMAIL_NEXT_END, "DATA scalar maximum or lexical atom control failed");
	testRequire(!xrtMemDebugFailTriggered() && xrtGetError() == Prior, "DATA scalar control allocated or changed diagnostic");
	xrtMemDebugFailClear(); xrtClearError(); testRequire(xrtMemDebugReset(), "DATA scalar control retained allocations");
	puts("[PASS] DATA valid-scalars/no-allocation");

	ximapfetchview Fetch; ximapfetchcursor Cursor; ximapfetchitem Item;
	xrtSetErrorInfo(XERR_ARGUMENT, "test.data", 91, "caller diagnostic"); Prior = xrtGetError();
	testRequire(xrtMemDebugFailAfter(0), "DATA literal setup failed");
	testRequire(xrtImapFetchParse(XRT_STR_LITERAL("1 FETCH (BODY[] {0}"), &Fetch) &&
		xrtImapFetchCursorInit(&Cursor, &Fetch) && xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_ITEM &&
		Item.Value.Kind == XIMAP_DATA_LITERAL && Item.Value.LiteralSize == 0 && Cursor.NeedMore &&
		xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_END &&
		xrtImapFetchCursorContinue(&Cursor, XRT_STR_LITERAL(" BODY[1] ~{3}")) &&
		xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_ITEM && Item.Value.LiteralBinary && Item.Value.LiteralSize == 3 &&
		xrtImapFetchCursorContinue(&Cursor, XRT_STR_LITERAL(")")) &&
		xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_END && Cursor.Done,
		"DATA zero/binary literal continuation control failed");
	testRequire(!xrtMemDebugFailTriggered() && xrtGetError() == Prior, "DATA literal success allocated or changed diagnostic");
	xrtMemDebugFailClear(); xrtClearError(); testRequire(xrtMemDebugReset(), "DATA literal retained allocations");
	puts("[PASS] DATA valid-literals/no-allocation");

	xrtSetErrorInfo(XERR_ARGUMENT, "test.data", 91, "caller diagnostic"); Prior = xrtGetError();
	testRequire(xrtMemDebugFailAfter(0), "DATA string control setup failed");
	ximapdatacursor Data; ximapdataview Value; char Decoded[6]; size_t Size;
	testRequire(xrtImapListParse(XRT_STR_LITERAL("LIST () \"\xe2\x88\x95\" inbox"), &List) &&
		xrtImapListParse(XRT_STR_LITERAL("LIST () \"\\\\\" inbox"), &List) &&
		xrtImapDataCursorInit(&Data, XRT_STR_LITERAL("\"a\\\"b\\\\c\"")) &&
		xrtImapDataNext(&Data, &Value) == XMAIL_NEXT_ITEM && xrtImapStringWrite(&Value, NULL, 0, &Size) && Size == 5 &&
		xrtImapStringWrite(&Value, Decoded, sizeof(Decoded), &Size) && Size == 5 && memcmp(Decoded, "a\"b\\c", 6) == 0,
		"DATA UTF8/escaped delimiter or exact string capacity control failed");
	testRequire(!xrtMemDebugFailTriggered() && xrtGetError() == Prior, "DATA string success allocated or changed diagnostic");
	xrtMemDebugFailClear(); xrtClearError(); testRequire(xrtMemDebugReset(), "DATA string control retained allocations");
	puts("[PASS] DATA valid-strings/no-allocation");
}

int main(int argc, char** argv)
{
	if ( argc == 3 ) {
		bool Inject = strcmp(argv[1], "oom") == 0;
		testRequire(Inject || strcmp(argv[1], "control") == 0, "DATA usage: [control|oom case]");
		for ( size_t i = 0; i < sizeof(TestDataErrors) / sizeof(TestDataErrors[0]); i++ )
			if ( strcmp(argv[2], TestDataErrors[i].Name) == 0 ) { testDataError(i, Inject); return 0; }
		testRequire(false, "unknown DATA error case");
	}
	testRequire(argc == 1, "DATA usage: [control|oom case]");
	testDataValid();
	for ( size_t i = 0; i < sizeof(TestDataErrors) / sizeof(TestDataErrors[0]); i++ ) {
		testDataError(i, false); testDataError(i, true);
	}
	testDataBorrowed(false); testDataBorrowed(true);
	testDataString(false); testDataString(true);
	testDataFetchSeparators(false); testDataFetchSeparators(true);
	return 0;
}
