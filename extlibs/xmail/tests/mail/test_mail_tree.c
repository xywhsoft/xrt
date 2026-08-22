#include "../test.h"



static const char sTreeMessage[] =
	"From: sender@example.com\r\n"
	"MIME-Version: 1.0\r\n"
	"Content-Type: multipart/mixed; boundary=outer\r\n"
	"\r\n"
	"preamble\r\n"
	"--outer\r\n"
	"Content-Type: multipart/alternative; boundary=inner\r\n"
	"\r\n"
	"--inner\r\n"
	"Content-Type: text/plain; charset=UTF-8\r\n"
	"Content-Transfer-Encoding: quoted-printable\r\n"
	"\r\n"
	"hello=20world\r\n"
	"--inner\r\n"
	"Content-Type: text/html\r\n"
	"Content-Transfer-Encoding: base64\r\n"
	"\r\n"
	"PHA+aGk8L3A+\r\n"
	"--inner--\r\n"
	"--outer\r\n"
	"Content-Type: application/octet-stream;"
	" name*=UTF-8''%E6%8A%A5%E5%91%8A.bin\r\n"
	"Content-Disposition: attachment;"
	" filename*0*=UTF-8''%E6%8A%A5; filename*1*=%E5%91%8A.bin\r\n"
	"Content-ID: <file@example.com>\r\n"
	"Content-Transfer-Encoding: base64\r\n"
	"\r\n"
	"AAEC\r\n"
	"--outer\r\n"
	"Content-Type: message/rfc822\r\n"
	"\r\n"
	"Subject: nested\r\n"
	"Content-Type: text/plain\r\n"
	"\r\n"
	"nested body\r\n"
	"--outer--\r\n"
	"epilogue";



/* 比较树中的借用文本。 */
static bool testMailTreeText(xstrview Text, const char* sExpected)
{
	return (Text.Size == strlen(sExpected)) &&
		(memcmp(Text.Data, sExpected, Text.Size) == 0);
}



/* 比较树中的借用字节。 */
static bool testMailTreeData(xbytesview Data, const void* pExpected, size_t iSize)
{
	return (Data.Size == iSize) && (memcmp(Data.Data, pExpected, iSize) == 0);
}



/* 验证拥有型 MIME 树、传输解码、附件元数据和嵌套消息。 */
static void testMailTreeComplete(void)
{
	char arrSource[sizeof(sTreeMessage)];
	xmailtree Tree;
	xmailpart* pAlternative;
	xmailpart* pAttachment;
	xmailpart* pEmbedded;
	static const unsigned char arrAttachment[] = { 0u, 1u, 2u };

	memcpy(arrSource, sTreeMessage, sizeof(arrSource));
	testRequire(xrtMailTreeParse(
		testMailViewN(arrSource, sizeof(arrSource) - 1u),
		NULL,
		&Tree
	), "complete MIME tree parse failed");
	memset(arrSource, 'x', sizeof(arrSource));
	testRequire((Tree.Root != NULL) && (Tree.PartCount == 7u) &&
		testMailTreeText(Tree.Root->ContentType.Type, "multipart") &&
		testMailTreeText(Tree.Root->ContentType.Subtype, "mixed") &&
		testMailTreeText(Tree.Root->Preamble, "preamble") &&
		testMailTreeText(Tree.Root->Epilogue, "epilogue") &&
		(Tree.Root->ChildCount == 3u),
		"root MIME tree structure mismatch");

	pAlternative = &Tree.Root->Children[0];
	testRequire((pAlternative->ChildCount == 2u) &&
		testMailTreeData(
			pAlternative->Children[0].Data,
			"hello world",
			11u
		) && pAlternative->Children[0].Decoded &&
		testMailTreeData(
			pAlternative->Children[1].Data,
			"<p>hi</p>",
			9u
		) && pAlternative->Children[1].Decoded,
		"alternative MIME body decode mismatch");

	pAttachment = &Tree.Root->Children[1];
	testRequire(pAttachment->Attachment && !pAttachment->Inline &&
		testMailTreeText(pAttachment->FileName, "报告.bin") &&
		testMailTreeText(pAttachment->ContentId, "file@example.com") &&
		testMailTreeData(
			pAttachment->Data,
			arrAttachment,
			sizeof(arrAttachment)
		), "attachment MIME metadata mismatch");

	pEmbedded = &Tree.Root->Children[2];
	testRequire(pEmbedded->Embedded && (pEmbedded->ChildCount == 1u) &&
		testMailTreeText(
			pEmbedded->Children[0].ContentType.Type,
			"text"
		) && testMailTreeData(
			pEmbedded->Children[0].Data,
			"nested body",
			11u
		), "embedded RFC message tree mismatch");
	testRequire(testMailTreeText(
		(xstrview){ Tree.Source.Data, 4u },
		"From"
	), "MIME tree retained the caller source instead of owning it");
	xrtMailTreeFree(&Tree);
	testRequire((Tree.Root == NULL) && (Tree.Storage == NULL) &&
		(Tree.PartCount == 0), "MIME tree free did not clear the handle");
}



/* 验证未知传输编码只能在显式兼容模式下保留原始正文。 */
static void testMailTreeUnknownTransfer(void)
{
	static const char sMessage[] =
		"Content-Type: application/x-test\r\n"
		"Content-Transfer-Encoding: x-custom\r\n"
		"\r\nraw";
	xmailtreelimits Limits;
	xmailtree Tree;

	testRequire(!xrtMailTreeParse(XRT_STR_LITERAL(sMessage), NULL, &Tree),
		"MIME tree accepted an unknown transfer encoding by default");
	xrtMailTreeLimitsInit(&Limits);
	Limits.Flags = XMAIL_TREE_ALLOW_UNKNOWN_TRANSFER;
	testRequire(xrtMailTreeParse(
		XRT_STR_LITERAL(sMessage),
		&Limits,
		&Tree
	) && !Tree.Root->Decoded &&
		(Tree.Root->Transfer == XMAIL_TRANSFER_UNKNOWN) &&
		testMailTreeData(Tree.Root->Data, "raw", 3u),
		"MIME tree did not preserve an allowed unknown body");
	xrtMailTreeFree(&Tree);
}



/* 验证 multipart/digest 子项缺省为 message/rfc822。 */
static void testMailTreeDigestDefault(void)
{
	static const char sMessage[] =
		"Content-Type: multipart/digest; boundary=d\r\n"
		"\r\n"
		"--d\r\n"
		"\r\n"
		"Subject: nested\r\n"
		"\r\n"
		"digest body\r\n"
		"--d--\r\n";
	xmailtree Tree;

	testRequire(xrtMailTreeParse(
		XRT_STR_LITERAL(sMessage),
		NULL,
		&Tree
	) && (Tree.PartCount == 3u) &&
		(Tree.Root->ChildCount == 1u) &&
		Tree.Root->Children[0].Embedded &&
		testMailTreeText(
			Tree.Root->Children[0].ContentType.Type,
			"message"
		) && testMailTreeData(
			Tree.Root->Children[0].Children[0].Data,
			"digest body",
			11u
		), "multipart/digest default entity mismatch");
	xrtMailTreeFree(&Tree);
}



/* 验证所有整树预算和递归硬上限。 */
static void testMailTreeLimits(void)
{
	xmailtreelimits Limits;
	xmailtree Tree;

	xrtMailTreeLimitsInit(&Limits);
	testRequire(xrtMailTreeLimitsValid(&Limits),
		"default MIME tree limits are invalid");
	Limits.MaxDepth = XMAIL_TREE_DEPTH_MAX + 1u;
	testRequire(!xrtMailTreeLimitsValid(&Limits) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"MIME tree limits validator accepted unsafe depth");

	xrtMailTreeLimitsInit(&Limits);
	Limits.MaxSourceBytes = sizeof(sTreeMessage) - 2u;
	testRequire(!xrtMailTreeParse(
		XRT_STR_LITERAL(sTreeMessage),
		&Limits,
		&Tree
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"MIME tree source budget was ignored");

	xrtMailTreeLimitsInit(&Limits);
	Limits.MaxParts = 2u;
	testRequire(!xrtMailTreeParse(
		XRT_STR_LITERAL(sTreeMessage),
		&Limits,
		&Tree
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"MIME tree part budget was ignored");

	xrtMailTreeLimitsInit(&Limits);
	Limits.MaxDepth = 2u;
	testRequire(!xrtMailTreeParse(
		XRT_STR_LITERAL(sTreeMessage),
		&Limits,
		&Tree
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"MIME tree depth budget was ignored");

	xrtMailTreeLimitsInit(&Limits);
	Limits.MaxDecodedBytes = 2u;
	testRequire(!xrtMailTreeParse(
		XRT_STR_LITERAL(sTreeMessage),
		&Limits,
		&Tree
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"MIME tree decoded-byte budget was ignored");

	xrtMailTreeLimitsInit(&Limits);
	Limits.MaxHeaderBytes = 8u;
	testRequire(!xrtMailTreeParse(
		XRT_STR_LITERAL(sTreeMessage),
		&Limits,
		&Tree
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"MIME tree header-byte budget was ignored");

	xrtMailTreeLimitsInit(&Limits);
	Limits.MaxDepth = XMAIL_TREE_DEPTH_MAX + 1u;
	testRequire(!xrtMailTreeParse(
		XRT_STR_LITERAL("\r\nbody"),
		&Limits,
		&Tree
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"MIME tree accepted an unsafe recursion limit");
}



/* 验证结构错误不会发布半棵树。 */
static void testMailTreeErrors(void)
{
	static const char sMessage[] =
		"Content-Type: multipart/mixed; boundary=missing\r\n"
		"\r\nbody";
	xmailtree Tree;

	memset(&Tree, 0, sizeof(Tree));
	Tree.PartCount = 99u;
	testRequire(!xrtMailTreeParse(
		XRT_STR_LITERAL(sMessage),
		NULL,
		&Tree
	) && (Tree.Root == NULL) && (Tree.Storage == NULL) &&
		(Tree.PartCount == 99u),
		"failed MIME tree parse modified the output handle");
}



/* 运行拥有型 MIME 树全部契约测试。 */
int main(void)
{
	testMailTreeComplete();
	testMailTreeUnknownTransfer();
	testMailTreeDigestDefault();
	testMailTreeLimits();
	testMailTreeErrors();
	return 0;
}
