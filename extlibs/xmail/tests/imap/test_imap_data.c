#include "../test.h"



/* 验证通用数据值、嵌套列表、literal 和 quoted 解码。 */
static void testImapDataValues(void)
{
	ximapdatacursor Cursor;
	ximapdataview Value;
	char sDecoded[16];
	size_t iSize;

	testRequire(xrtImapDataCursorInit(
		&Cursor,
		XRT_STR_LITERAL("ATOM 42 NIL \"a\\\"b\\\\c\" (ONE (TWO)) {7}")
	), "IMAP data cursor init failed");
	testRequire((xrtImapDataNext(&Cursor, &Value) == XMAIL_NEXT_ITEM) &&
		(Value.Kind == XIMAP_DATA_ATOM) &&
		testMailViewEqual(Value.Value, XRT_STR_LITERAL("ATOM")),
		"IMAP atom data mismatch");
	testRequire((xrtImapDataNext(&Cursor, &Value) == XMAIL_NEXT_ITEM) &&
		(Value.Kind == XIMAP_DATA_NUMBER) && (Value.Number == UINT64_C(42)),
		"IMAP number data mismatch");
	testRequire((xrtImapDataNext(&Cursor, &Value) == XMAIL_NEXT_ITEM) &&
		(Value.Kind == XIMAP_DATA_NIL) && (Value.Value.Size == 0),
		"IMAP NIL data mismatch");
	testRequire((xrtImapDataNext(&Cursor, &Value) == XMAIL_NEXT_ITEM) &&
		(Value.Kind == XIMAP_DATA_QUOTED) && xrtImapStringWrite(
			&Value,
			sDecoded,
			sizeof(sDecoded),
			&iSize
		) && (iSize == 5u) && (memcmp(sDecoded, "a\"b\\c", 5u) == 0),
		"IMAP quoted data decode mismatch");
	testRequire((xrtImapDataNext(&Cursor, &Value) == XMAIL_NEXT_ITEM) &&
		(Value.Kind == XIMAP_DATA_LIST) &&
		testMailViewEqual(Value.Value, XRT_STR_LITERAL("ONE (TWO)")),
		"IMAP nested list data mismatch");
	testRequire((xrtImapDataNext(&Cursor, &Value) == XMAIL_NEXT_ITEM) &&
		(Value.Kind == XIMAP_DATA_LITERAL) && (Value.LiteralSize == 7u) &&
		!Value.LiteralBinary && !Value.LiteralNonSynchronizing,
		"IMAP literal data mismatch");
	testRequire(xrtImapDataNext(&Cursor, &Value) == XMAIL_NEXT_END,
		"IMAP data cursor did not finish");

	testRequire(xrtImapDataCursorInit(
		&Cursor,
		XRT_STR_LITERAL("\"unterminated")
	) && (xrtImapDataNext(&Cursor, &Value) == XMAIL_NEXT_ERROR),
		"IMAP data accepted an unterminated quoted string");
	testRequire(xrtImapDataCursorInit(
		&Cursor,
		XRT_STR_LITERAL("(unbalanced")
	) && (xrtImapDataNext(&Cursor, &Value) == XMAIL_NEXT_ERROR),
		"IMAP data accepted an unterminated list");
}



/* 验证 LIST 与 FLAGS 共享借用游标。 */
static void testImapListData(void)
{
	ximaplistview List;
	ximapflagcursor Cursor;
	xstrview Flag;
	char sMailbox[32];
	size_t iSize;

	testRequire(xrtImapListParse(
		XRT_STR_LITERAL(
			"LIST (\\HasNoChildren \\Marked) \"/\" \"A\\\"B\" "
			"(CHILDINFO (\"SUBSCRIBED\"))"
		),
		&List
	), "IMAP LIST parse failed");
	testRequire(xrtImapStringWrite(
		&List.Mailbox,
		sMailbox,
		sizeof(sMailbox),
		&iSize
	) && (iSize == 3u) && (memcmp(sMailbox, "A\"B", 3u) == 0) &&
		(List.Extensions.Size != 0), "IMAP LIST mailbox decode failed");
	testRequire(xrtImapFlagCursorInit(
		&Cursor,
		XRT_STR_LITERAL("(\\HasNoChildren \\Marked custom)")
	), "IMAP LIST flag cursor init failed");
	testRequire((xrtImapFlagNext(&Cursor, &Flag) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Flag, XRT_STR_LITERAL("\\HasNoChildren")),
		"IMAP first LIST flag mismatch");
	testRequire((xrtImapFlagNext(&Cursor, &Flag) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Flag, XRT_STR_LITERAL("\\Marked")),
		"IMAP second LIST flag mismatch");
	testRequire((xrtImapFlagNext(&Cursor, &Flag) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Flag, XRT_STR_LITERAL("custom")) &&
		(xrtImapFlagNext(&Cursor, &Flag) == XMAIL_NEXT_END),
		"IMAP keyword flag mismatch");
	testRequire(xrtImapListParse(
		XRT_STR_LITERAL("LIST () NIL {5}"),
		&List
	) && (List.Delimiter.Kind == XIMAP_DATA_NIL) &&
		(List.Mailbox.Kind == XIMAP_DATA_LITERAL) &&
		(List.Mailbox.LiteralSize == 5u),
		"IMAP LIST literal mailbox mismatch");
}



/* 验证 STATUS 属性对不把未知扩展压入固定对象。 */
static void testImapStatusData(void)
{
	ximapmailboxstatusview Status;
	ximapstatuscursor Cursor;
	ximapstatusitem Item;

	testRequire(xrtImapStatusParse(
		XRT_STR_LITERAL(
			"STATUS \"INBOX\" (MESSAGES 9 UIDNEXT 10 X-VENDOR NIL)"
		),
		&Status
	) && xrtImapStatusCursorInit(&Cursor, &Status),
		"IMAP STATUS parse failed");
	testRequire((xrtImapStatusNext(&Cursor, &Item) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Item.Name, XRT_STR_LITERAL("MESSAGES")) &&
		(Item.Value.Kind == XIMAP_DATA_NUMBER) &&
		(Item.Value.Number == UINT64_C(9)),
		"IMAP STATUS MESSAGES mismatch");
	testRequire((xrtImapStatusNext(&Cursor, &Item) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Item.Name, XRT_STR_LITERAL("UIDNEXT")) &&
		(Item.Value.Number == UINT64_C(10)),
		"IMAP STATUS UIDNEXT mismatch");
	testRequire((xrtImapStatusNext(&Cursor, &Item) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Item.Name, XRT_STR_LITERAL("X-VENDOR")) &&
		(Item.Value.Kind == XIMAP_DATA_NIL) &&
		(xrtImapStatusNext(&Cursor, &Item) == XMAIL_NEXT_END),
		"IMAP STATUS extension mismatch");
	testRequire(xrtImapStatusParse(
		XRT_STR_LITERAL("STATUS INBOX (MESSAGES)"),
		&Status
	) && xrtImapStatusCursorInit(&Cursor, &Status) &&
		(xrtImapStatusNext(&Cursor, &Item) == XMAIL_NEXT_ERROR),
		"IMAP STATUS accepted a missing value");
}



/* 验证 SEARCH 与 ESEARCH 采用增量结果，不展开集合。 */
static void testImapSearchData(void)
{
	ximapsearchcursor Search;
	ximapsearchitem SearchItem;
	ximapesearchview ESearch;
	ximapesearchcursor ECursor;
	ximapesearchitem EItem;

	testRequire(xrtImapSearchCursorInit(
		&Search,
		XRT_STR_LITERAL("SEARCH 2 9 (MODSEQ 123)")
	), "IMAP SEARCH cursor init failed");
	testRequire((xrtImapSearchNext(&Search, &SearchItem) == XMAIL_NEXT_ITEM) &&
		(SearchItem.Kind == XIMAP_SEARCH_ID) && (SearchItem.Number == 2u),
		"IMAP first SEARCH ID mismatch");
	testRequire((xrtImapSearchNext(&Search, &SearchItem) == XMAIL_NEXT_ITEM) &&
		(SearchItem.Kind == XIMAP_SEARCH_ID) && (SearchItem.Number == 9u),
		"IMAP second SEARCH ID mismatch");
	testRequire((xrtImapSearchNext(&Search, &SearchItem) == XMAIL_NEXT_ITEM) &&
		(SearchItem.Kind == XIMAP_SEARCH_MODSEQ) &&
		(SearchItem.Number == 123u) &&
		(xrtImapSearchNext(&Search, &SearchItem) == XMAIL_NEXT_END),
		"IMAP SEARCH MODSEQ mismatch");

	testRequire(xrtImapESearchParse(
		XRT_STR_LITERAL(
			"ESEARCH (TAG \"A282\") UID COUNT 3 ALL 2:9 MIN 2 MAX 9"
		),
		&ESearch
	) && ESearch.Uid &&
		testMailViewEqual(ESearch.Correlator, XRT_STR_LITERAL("TAG \"A282\"")) &&
		xrtImapESearchCursorInit(&ECursor, &ESearch),
		"IMAP ESEARCH parse failed");
	testRequire((xrtImapESearchNext(&ECursor, &EItem) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(EItem.Name, XRT_STR_LITERAL("COUNT")) &&
		(EItem.Value.Number == 3u), "IMAP ESEARCH COUNT mismatch");
	testRequire((xrtImapESearchNext(&ECursor, &EItem) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(EItem.Name, XRT_STR_LITERAL("ALL")) &&
		testMailViewEqual(EItem.Value.Source, XRT_STR_LITERAL("2:9")),
		"IMAP ESEARCH ALL mismatch");
	testRequire((xrtImapESearchNext(&ECursor, &EItem) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(EItem.Name, XRT_STR_LITERAL("MIN")) &&
		(EItem.Value.Number == 2u), "IMAP ESEARCH MIN mismatch");
	testRequire((xrtImapESearchNext(&ECursor, &EItem) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(EItem.Name, XRT_STR_LITERAL("MAX")) &&
		(EItem.Value.Number == 9u) &&
		(xrtImapESearchNext(&ECursor, &EItem) == XMAIL_NEXT_END),
		"IMAP ESEARCH MAX mismatch");
}



/* 验证 FETCH 属性游标在 literal 前后保持同一响应状态。 */
static void testImapFetchData(void)
{
	ximapfetchview Fetch;
	ximapfetchcursor Cursor;
	ximapfetchitem Item;
	ximapflagcursor Flags;
	xstrview Flag;

	testRequire(xrtImapFetchParse(
		XRT_STR_LITERAL(
			"23 FETCH (UID 99 FLAGS (\\Seen custom) "
			"BODY[HEADER.FIELDS (DATE FROM)] {12}"
		),
		&Fetch
	) && (Fetch.Sequence == 23u) &&
		xrtImapFetchCursorInit(&Cursor, &Fetch),
		"IMAP FETCH parse failed");
	testRequire((xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Item.Attribute, XRT_STR_LITERAL("UID")) &&
		(Item.Value.Kind == XIMAP_DATA_NUMBER) && (Item.Value.Number == 99u),
		"IMAP FETCH UID mismatch");
	testRequire((xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Item.Attribute, XRT_STR_LITERAL("FLAGS")) &&
		(Item.Value.Kind == XIMAP_DATA_LIST) &&
		xrtImapFlagCursorInit(&Flags, Item.Value.Source) &&
		(xrtImapFlagNext(&Flags, &Flag) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Flag, XRT_STR_LITERAL("\\Seen")),
		"IMAP FETCH FLAGS mismatch");
	testRequire((xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(
			Item.Attribute,
			XRT_STR_LITERAL("BODY[HEADER.FIELDS (DATE FROM)]")
		) && (Item.Value.Kind == XIMAP_DATA_LITERAL) &&
		(Item.Value.LiteralSize == 12u) && Cursor.NeedMore &&
		(xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_END),
		"IMAP FETCH literal boundary mismatch");
	testRequire(xrtImapFetchCursorContinue(
		&Cursor,
		XRT_STR_LITERAL(" RFC822.SIZE 12)")
	), "IMAP FETCH continuation failed");
	testRequire((xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Item.Attribute, XRT_STR_LITERAL("RFC822.SIZE")) &&
		(Item.Value.Number == 12u) &&
		(xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_END) && Cursor.Done,
		"IMAP FETCH continuation item mismatch");

	testRequire(xrtImapFetchParse(
		XRT_STR_LITERAL("1 FETCH (BODY[HEADER 2)"),
		&Fetch
	) && xrtImapFetchCursorInit(&Cursor, &Fetch) &&
		(xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_ERROR),
		"IMAP FETCH accepted an unterminated section");
}



/* 运行零分配 IMAP 数据层测试。 */
int main(void)
{
	testImapDataValues();
	testImapListData();
	testImapStatusData();
	testImapSearchData();
	testImapFetchData();
	return 0;
}
