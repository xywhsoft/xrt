#include "test.h"

#define TEST_MESSAGE(Envelope) "(\"MESSAGE\" \"RFC822\" NIL NIL NIL \"7BIT\" 10 " Envelope " (\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 5 1) 1)"
#define TEST_ENVELOPE(From) "(NIL NIL " From " NIL NIL NIL NIL NIL NIL NIL)"

typedef enum testsemanticsop { TEST_BODY, TEST_STATUS_SIZE, TEST_LITERAL } testsemanticsop;
typedef struct testsemanticscase { cstr Name; testsemanticsop Operation; xstrview Text; xerrkind Error; } testsemanticscase;
static const testsemanticscase TestSemantics[] = {
    { "envelope-empty", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE("()")), XERR_PROTOCOL },
    { "envelope-short", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE("(NIL NIL NIL NIL NIL NIL NIL NIL NIL)")), XERR_PROTOCOL },
    { "envelope-extra", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE("(NIL NIL NIL NIL NIL NIL NIL NIL NIL NIL NIL)")), XERR_PROTOCOL },
    { "envelope-date-number", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE("(1 NIL NIL NIL NIL NIL NIL NIL NIL NIL)")), XERR_PROTOCOL },
    { "envelope-subject-atom", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE("(NIL subject NIL NIL NIL NIL NIL NIL NIL NIL)")), XERR_PROTOCOL },
    { "envelope-reply-number", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE("(NIL NIL NIL NIL NIL NIL NIL NIL 1 NIL)")), XERR_PROTOCOL },
    { "envelope-id-list", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE("(NIL NIL NIL NIL NIL NIL NIL NIL NIL (NIL))")), XERR_PROTOCOL },
    { "envelope-from-string", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE(TEST_ENVELOPE("\"from\""))), XERR_PROTOCOL },
    { "envelope-sender-number", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE("(NIL NIL NIL 1 NIL NIL NIL NIL NIL NIL)")), XERR_PROTOCOL },
    { "envelope-reply-list-empty", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE("(NIL NIL NIL NIL () NIL NIL NIL NIL NIL)")), XERR_PROTOCOL },
    { "envelope-to-string", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE("(NIL NIL NIL NIL NIL \"to\" NIL NIL NIL NIL)")), XERR_PROTOCOL },
    { "envelope-cc-number", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE("(NIL NIL NIL NIL NIL NIL 1 NIL NIL NIL)")), XERR_PROTOCOL },
    { "envelope-bcc-empty", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE("(NIL NIL NIL NIL NIL NIL NIL () NIL NIL)")), XERR_PROTOCOL },
    { "address-list-empty", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE(TEST_ENVELOPE("()"))), XERR_PROTOCOL },
    { "address-list-scalar", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE(TEST_ENVELOPE("(NIL)"))), XERR_PROTOCOL },
    { "address-short", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE(TEST_ENVELOPE("((NIL NIL NIL))"))), XERR_PROTOCOL },
    { "address-extra", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE(TEST_ENVELOPE("((NIL NIL NIL NIL NIL))"))), XERR_PROTOCOL },
    { "address-name-number", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE(TEST_ENVELOPE("((1 NIL NIL NIL))"))), XERR_PROTOCOL },
    { "address-route-atom", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE(TEST_ENVELOPE("((NIL route NIL NIL))"))), XERR_PROTOCOL },
    { "address-mailbox-list", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE(TEST_ENVELOPE("((NIL NIL (NIL) NIL))"))), XERR_PROTOCOL },
    { "address-host-number", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE(TEST_ENVELOPE("((NIL NIL NIL 1))"))), XERR_PROTOCOL },
    { "address-separator", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE(TEST_ENVELOPE("((\"name\"\"route\" NIL NIL))"))), XERR_PROTOCOL },
    { "envelope-separator", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE("(\"date\"\"subject\" NIL NIL NIL NIL NIL NIL NIL NIL)")), XERR_PROTOCOL },
    { "envelope-nested-address", TEST_BODY, XRT_STR_LITERAL(TEST_MESSAGE(TEST_ENVELOPE("(((NIL NIL NIL NIL)))"))), XERR_PROTOCOL },
    { "body-type-atom", TEST_BODY, XRT_STR_LITERAL("(TEXT \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1)"), XERR_PROTOCOL },
    { "body-subtype-atom", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" PLAIN NIL NIL NIL \"7BIT\" 1 1)"), XERR_PROTOCOL },
    { "body-param-name-atom", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" (CHARSET \"UTF-8\") NIL NIL \"7BIT\" 1 1)"), XERR_PROTOCOL },
    { "body-param-value-atom", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" (\"CHARSET\" UTF-8) NIL NIL \"7BIT\" 1 1)"), XERR_PROTOCOL },
    { "body-id-atom", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL id NIL \"7BIT\" 1 1)"), XERR_PROTOCOL },
    { "body-desc-atom", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL desc \"7BIT\" 1 1)"), XERR_PROTOCOL },
    { "body-encoding-atom", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL BINARY 1 1)"), XERR_PROTOCOL },
    { "body-md5-atom", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 md5)"), XERR_PROTOCOL },
    { "body-disposition-atom", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL (INLINE NIL))"), XERR_PROTOCOL },
    { "body-language-atom", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL NIL en)"), XERR_PROTOCOL },
    { "body-location-atom", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL NIL NIL path)"), XERR_PROTOCOL },
    { "body-extension-atom", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL NIL NIL NIL future)"), XERR_PROTOCOL },
    { "body-octets-range", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 9223372036854775808 1)"), XERR_RANGE },
    { "body-lines-range", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 9223372036854775808)"), XERR_RANGE },
    { "body-extension-range", TEST_BODY, XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL NIL NIL NIL 9223372036854775808)"), XERR_RANGE },
    { "status-size-range", TEST_STATUS_SIZE, XRT_STR_LITERAL("SIZE 9223372036854775808"), XERR_RANGE },
    { "status-size-max64", TEST_STATUS_SIZE, XRT_STR_LITERAL("SIZE 18446744073709551615"), XERR_RANGE },
    { "literal-range", TEST_LITERAL, XRT_STR_LITERAL("{9223372036854775808}"), XERR_RANGE },
    { "literal-binary-range", TEST_LITERAL, XRT_STR_LITERAL("~{9223372036854775808}"), XERR_RANGE },
    { "literal-max64", TEST_LITERAL, XRT_STR_LITERAL("{18446744073709551615}"), XERR_RANGE },
    { "literal-overflow", TEST_LITERAL, XRT_STR_LITERAL("{18446744073709551616}"), XERR_RANGE }
};

static void testSemanticsError(size_t Index, bool Inject)
{
    const testsemanticscase* Case = &TestSemantics[Index];
    union { ximapbodyview Body; ximapstatusitem Status; ximapliteralview Literal; } Output;
    unsigned char Before[sizeof(Output)]; ximapstatuscursor Cursor;
    unsigned char CursorBefore[sizeof(Cursor)];
    memset(&Output, 0xa5, sizeof(Output)); memcpy(Before, &Output, sizeof(Output));
    memset(&Cursor, 0xa5, sizeof(Cursor));
    if ( Case->Operation == TEST_STATUS_SIZE )
        testRequire(xrtImapDataCursorInit(&Cursor.Data, Case->Text), "semantics STATUS setup failed");
    memcpy(CursorBefore, &Cursor, sizeof(Cursor));
    xrtClearError(); if ( Inject ) testRequire(xrtMemDebugFailAfter(0), "semantics injection failed");
    bool Failed;
    if ( Case->Operation == TEST_BODY ) Failed = !xrtImapBodyParse(Case->Text, &Output.Body);
    else if ( Case->Operation == TEST_STATUS_SIZE ) Failed = xrtImapStatusNext(&Cursor, &Output.Status) == XMAIL_NEXT_ERROR;
    else Failed = xrtImapLiteralParse(Case->Text, &Output.Literal) == XMAIL_NEXT_ERROR;
    bool Triggered = xrtMemDebugFailTriggered(); xerrkind Kind = xrtErrorKind(xrtGetError());
    bool Unchanged = memcmp(Before, &Output, sizeof(Output)) == 0 && memcmp(CursorBefore, &Cursor, sizeof(Cursor)) == 0;
    xrtMemDebugFailClear();
    printf("[diagnostic] SEMANTICS %s/%s triggered=%d kind=%d expected=%d failed=%d unchanged=%d\n",
        Case->Name, Inject ? "oom" : "control", (int)Triggered, (int)Kind,
        (int)(Inject ? XERR_MEMORY : Case->Error), (int)Failed, (int)Unchanged);
    testRequire(Failed && Unchanged && Triggered == Inject && Kind == (Inject ? XERR_MEMORY : Case->Error),
        "IMAP semantic validation, diagnostic or transaction failed");
    xrtClearError(); testRequire(xrtMemDebugReset(), "semantics retained allocations");
    printf("[PASS] SEMANTICS %s/%s\n", Case->Name, Inject ? "oom" : "control");
}

static void testSemanticsValid(void)
{
    static const xstrview Bodies[] = {
        XRT_STR_LITERAL(TEST_MESSAGE("(NIL NIL NIL NIL NIL NIL NIL NIL NIL NIL)")),
        XRT_STR_LITERAL(TEST_MESSAGE("(\"date\" \"subject\" ((NIL NIL \"Friends\" NIL)(\"Name\" NIL \"123\" \"example.test\")(NIL NIL NIL NIL)) NIL NIL NIL NIL NIL \"reply\" \"id\")")),
        XRT_STR_LITERAL(TEST_MESSAGE("(\"\" \"a\\\"b\" ((\"\" \"\" \"\" \"\") (NIL NIL NIL NIL)) NIL NIL NIL NIL NIL NIL NIL)")),
        XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 9223372036854775807 9223372036854775807 NIL NIL NIL NIL (\"future\" 9223372036854775807))")
    };
    for ( size_t i = 0; i < sizeof(Bodies) / sizeof(Bodies[0]); i++ ) {
        ximapbodyview Body; xrtSetErrorInfo(XERR_ARGUMENT, "test.semantics", 91, "caller diagnostic");
        const xerror* Prior = xrtGetError(); testRequire(xrtMemDebugFailAfter(0), "semantics control setup failed");
        testRequire(xrtImapBodyParse(Bodies[i], &Body) && Body.Source.Data == Bodies[i].Data &&
            Body.Source.Size == Bodies[i].Size && !xrtMemDebugFailTriggered() && xrtGetError() == Prior,
            "valid ENVELOPE/group/63-bit body allocated, lost its source or diagnostic");
        xrtMemDebugFailClear(); xrtClearError(); testRequire(xrtMemDebugReset(), "semantics control retained allocations");
        printf("[PASS] SEMANTICS valid-body-%zu/no-allocation\n", i);
    }
    ximapstatuscursor Status; ximapstatusitem Item; ximapsearchcursor Search; ximapsearchitem Id;
    xrtSetErrorInfo(XERR_ARGUMENT, "test.semantics", 91, "caller diagnostic"); const xerror* Prior = xrtGetError();
    testRequire(xrtMemDebugFailAfter(0), "semantics number control setup failed");
    testRequire(xrtImapDataCursorInit(&Status.Data, XRT_STR_LITERAL("SIZE 9223372036854775807")) &&
        xrtImapStatusNext(&Status, &Item) == XMAIL_NEXT_ITEM && Item.Value.Number == (uint64)INT64_MAX &&
        xrtImapStatusNext(&Status, &Item) == XMAIL_NEXT_END &&
        xrtImapSearchCursorInit(&Search, XRT_STR_LITERAL("SEARCH (MODSEQ 18446744073709551615)")) &&
        xrtImapSearchNext(&Search, &Id) == XMAIL_NEXT_ITEM && Id.Number == UINT64_MAX &&
        xrtImapSearchNext(&Search, &Id) == XMAIL_NEXT_END,
        "63-bit SIZE or separate unsigned 64-bit MODSEQ contract failed");
    ximapliteralview Literal;
    if ( SIZE_MAX >= (uint64)INT64_MAX )
        testRequire(xrtImapLiteralParse(XRT_STR_LITERAL("~{9223372036854775807}"), &Literal) == XMAIL_NEXT_ITEM &&
            Literal.Size == (size_t)INT64_MAX && Literal.Binary, "maximum 63-bit literal failed");
    testRequire(!xrtMemDebugFailTriggered() && xrtGetError() == Prior, "number success allocated or changed diagnostic");
    xrtMemDebugFailClear(); xrtClearError(); testRequire(xrtMemDebugReset(), "number control retained allocations");
    puts("[PASS] SEMANTICS valid-numbers/no-allocation");
}

int main(int argc, char** argv)
{
    if ( argc == 3 ) {
        bool Inject = strcmp(argv[1], "oom") == 0;
        testRequire(Inject || strcmp(argv[1], "control") == 0, "semantics usage: [control|oom case]");
        for ( size_t i = 0; i < sizeof(TestSemantics) / sizeof(TestSemantics[0]); i++ )
            if ( strcmp(argv[2], TestSemantics[i].Name) == 0 ) { testSemanticsError(i, Inject); return 0; }
        testRequire(false, "unknown semantic case");
    }
    testRequire(argc == 1, "semantics usage: [control|oom case]"); testSemanticsValid();
    for ( size_t i = 0; i < sizeof(TestSemantics) / sizeof(TestSemantics[0]); i++ ) {
        testSemanticsError(i, false); testSemanticsError(i, true);
    }
    return 0;
}
