#include "../test.h"

/* 输入由长度界定，包含畸形头、截断和二进制字节。 */
static const char sValid[] =
	"Content-Type: multipart/mixed; boundary=b\r\n"
	"\r\n"
	"--b\r\n"
	"Content-Type: text/plain\r\n"
	"\r\n"
	"alpha\r\n--bEvil\r\nomega\r\n"
	"--b--\r\n";

static const char sExpectedBody[] = "alpha\r\n--bEvil\r\nomega";

static void testParseOwned(xstrview Source)
{
	xmailtree Tree = {0};
	bool bParsed = xrtMailTreeParse(Source, NULL, &Tree);
	if ( bParsed ) {
		testRequire(Tree.Root != NULL && Tree.Storage != NULL,
			"adversarial MIME parse published an incomplete tree");
		xrtMailTreeFree(&Tree);
	}
	else {
		testRequire(Tree.Root == NULL && Tree.Storage == NULL &&
			Tree.PartCount == 0u,
			"adversarial MIME failure published a partial tree");
	}
}

static void testReject(xstrview Source, cstr sMessage)
{
	xmailtree Tree = {0};
	Tree.PartCount = 77u;
	testRequire(!xrtMailTreeParse(Source, NULL, &Tree) &&
		Tree.Root == NULL && Tree.Storage == NULL && Tree.PartCount == 77u,
		sMessage);
}

int main(void)
{
	xmailtree Tree = {0};
	xmailpart* pPart;
	testRequire(xrtMailTreeParse(XRT_STR_LITERAL(sValid), NULL, &Tree),
		"boundary-lookalike MIME fixture failed to parse");
	testRequire(Tree.Root != NULL && Tree.Root->ChildCount == 1u,
		"boundary-lookalike changed part count");
	pPart = &Tree.Root->Children[0];
	testRequire(pPart->Data.Size == sizeof(sExpectedBody) - 1u &&
		memcmp(pPart->Data.Data, sExpectedBody, sizeof(sExpectedBody) - 1u) == 0,
		"boundary-lookalike line was treated as a delimiter");
	xrtMailTreeFree(&Tree);

	testReject(XRT_STR_LITERAL(
		"Content-Type: text/plain\r\n"
		"Content-Type: text/html\r\n\r\nbody"),
		"duplicate Content-Type was accepted");
	testReject(XRT_STR_LITERAL(
		"Content-Transfer-Encoding: base64\r\n\r\n@@@"),
		"invalid base64 MIME body was accepted");
	testReject(XRT_STR_LITERAL(
		"Content-Type: multipart/mixed; boundary=b\r\n\r\n"
		"--b\r\n\r\nbody"),
		"unterminated multipart body was accepted");
	{
		static const char sNulHeader[] =
			"Subject: a\0b\r\n\r\nbody";
		testReject(testMailViewN(sNulHeader, sizeof(sNulHeader) - 1u),
			"NUL inside a header was accepted");
	}

	/* 每个截断点和每个单字节控制/高位变异都经过完整解析/释放路径。 */
	for ( size_t i = 0; i < sizeof(sValid) - 1u; i++ ) {
		testParseOwned(testMailViewN(sValid, i));
	}
	{
		char arrMutated[sizeof(sValid)];
		static const unsigned char arrBytes[] = { 0u, '\n', 0x7fu, 0xffu };
		for ( size_t iByte = 0; iByte < sizeof(arrBytes); iByte++ ) {
			for ( size_t i = 0; i < sizeof(sValid) - 1u; i++ ) {
				memcpy(arrMutated, sValid, sizeof(sValid));
				arrMutated[i] = (char)arrBytes[iByte];
				testParseOwned(testMailViewN(arrMutated, sizeof(sValid) - 1u));
			}
		}
	}
	return 0;
}
