#include "../test.h"



/* 验证无分配解析视图和 75 字节硬限制。 */
static void testMailWordParse(void)
{
	static const char sWord[] = "=?UTF-8?Q?hello_=E4=B8=AD?=tail";
	static const char sLanguageWord[] = "=?UTF-8*en-US?Q?hello?=";
	char arrLong[77];
	xmailwordview Word;

	testRequire(xrtMailWordParse(
		XRT_STR_LITERAL(sWord),
		&Word
	) == XMAIL_NEXT_ITEM, "mail encoded-word parse failed");
	testRequire(testMailViewEqual(Word.Charset, XRT_STR_LITERAL("UTF-8")) &&
		(Word.Language.Size == 0) &&
		(Word.Encoding == XMAIL_WORD_Q) &&
		testMailViewEqual(Word.Encoded, XRT_STR_LITERAL("hello_=E4=B8=AD")) &&
		(Word.Source.Size == 27u),
		"mail encoded-word parse view mismatch");
	testRequire(xrtMailWordParse(
		XRT_STR_LITERAL(sLanguageWord),
		&Word
	) == XMAIL_NEXT_ITEM &&
		testMailViewEqual(Word.Charset, XRT_STR_LITERAL("UTF-8")) &&
		testMailViewEqual(Word.Language, XRT_STR_LITERAL("en-US")) &&
		testMailViewEqual(Word.Encoded, XRT_STR_LITERAL("hello")),
		"mail encoded-word language view mismatch");
	testRequire(xrtMailWordParse(
		XRT_STR_LITERAL("plain"),
		&Word
	) == XMAIL_NEXT_END, "plain text was parsed as encoded-word");

	memcpy(arrLong, "=?UTF-8?Q?", 10u);
	memset(arrLong + 10u, 'a', 64u);
	memcpy(arrLong + 74u, "?=", 2u);
	arrLong[76] = 0;
	xrtClearError();
	testRequire(xrtMailWordParse(
		testMailViewN(arrLong, 76u),
		&Word
	) == XMAIL_NEXT_ERROR, "oversized encoded-word was accepted");
	testRequire(xrtErrorFind(
		xrtGetError(),
		"xrt.mail",
		XMAIL_ERROR_ENCODING
	) != NULL, "oversized encoded-word error mismatch");
}



/* 验证 B/Q 编码以及长 UTF-8 文本分片不拆标量。 */
static void testMailWordEncode(void)
{
	static const char sLong[] =
		"中文邮件主题中文邮件主题中文邮件主题中文邮件主题"
		"中文邮件主题中文邮件主题中文邮件主题中文邮件主题";
	char arrOutput[1024];
	char arrDecoded[512];
	size_t iSize;
	size_t iPosition;
	xmailwordview Word;

	testRequire(xrtMailWordEncodeWrite(
		XRT_STR_LITERAL("中文主题"),
		XMAIL_WORD_BASE64,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "=?UTF-8?B?5Lit5paH5Li76aKY?=") == 0),
		"mail Base64 encoded-word mismatch");
	testRequire(xrtMailWordEncodeWrite(
		XRT_STR_LITERAL("hello 中文"),
		XMAIL_WORD_Q,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(
		arrOutput,
		"=?UTF-8?Q?hello_=E4=B8=AD=E6=96=87?="
	) == 0), "mail Q encoded-word mismatch");
	testRequire(xrtMailWordEncodeWrite(
		XRT_STR_LITERAL("plain subject"),
		XMAIL_WORD_BASE64,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "plain subject") == 0),
		"safe ASCII mail word was unnecessarily encoded");

	testRequire(xrtMailWordEncodeWrite(
		XRT_STR_LITERAL(sLong),
		XMAIL_WORD_BASE64,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	), "long UTF-8 encoded-word write failed");
	iPosition = 0;
	while ( iPosition < iSize ) {
		testRequire(xrtMailWordParse(
			testMailViewN(arrOutput + iPosition, iSize - iPosition),
			&Word
		) == XMAIL_NEXT_ITEM, "long encoded-word chunk is malformed");
		testRequire(Word.Source.Size <= 75u,
			"long encoded-word chunk exceeds 75 bytes");
		iPosition += Word.Source.Size;
		if ( iPosition < iSize ) {
			testRequire(arrOutput[iPosition] == ' ',
				"long encoded-word chunks have no separator");
			iPosition++;
		}
	}
	testRequire(xrtMailWordDecodeWrite(
		testMailViewN(arrOutput, iSize),
		XMAIL_WORD_STRICT,
		arrDecoded,
		sizeof(arrDecoded),
		&iSize
	) && (strcmp(arrDecoded, sLong) == 0),
		"long encoded-word round trip mismatch");
}



/* 验证混合文本、相邻词和同址收缩解码。 */
static void testMailWordDecode(void)
{
	static const char sAdjacent[] =
		"=?UTF-8?B?5Lit5paH?= \t\r\n "
		"=?US-ASCII?Q?Subject?=";
	char arrOutput[256];
	char arrInPlace[128] = "Re: =?UTF-8?Q?hello_=E4=B8=AD=E6=96=87?=";
	size_t iSize;
	str sDecoded;

	testRequire(xrtMailWordDecodeWrite(
		XRT_STR_LITERAL(sAdjacent),
		XMAIL_WORD_STRICT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "中文Subject") == 0),
		"adjacent encoded-word whitespace was not suppressed");
	testRequire(xrtMailWordDecodeWrite(
		testMailView(arrInPlace),
		XMAIL_WORD_STRICT,
		arrInPlace,
		sizeof(arrInPlace),
		&iSize
	) && (strcmp(arrInPlace, "Re: hello 中文") == 0),
		"in-place encoded-word decode mismatch");

	sDecoded = xrtMailWordDecode(
		XRT_STR_LITERAL("=?UTF-8?B?5Lit5paH?="),
		XMAIL_WORD_STRICT,
		&iSize
	);
	testRequire((sDecoded != NULL) && (strcmp(sDecoded, "中文") == 0),
		"allocated encoded-word decode mismatch");
	xrtFree(sDecoded);
}



/* 验证常见旧字符集和协议上限不会依赖固定魔数。 */
static void testMailWordLegacyCharset(void)
{
	char arrBoundary[76];
	char arrOutput[256];
	size_t iSize;

	testRequire(xrtMailWordDecodeWrite(
		XRT_STR_LITERAL("=?ISO-8859-1?Q?caf=E9?="),
		XMAIL_WORD_STRICT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "caf\xC3\xA9") == 0),
		"ISO-8859-1 encoded-word decode mismatch");
	testRequire(xrtMailWordDecodeWrite(
		XRT_STR_LITERAL("=?windows-1252?Q?=93quote=94_=80?="),
		XMAIL_WORD_STRICT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(
		arrOutput,
		"\xE2\x80\x9Cquote\xE2\x80\x9D \xE2\x82\xAC"
	) == 0), "Windows-1252 encoded-word decode mismatch");
	testRequire(xrtMailWordDecodeWrite(
		XRT_STR_LITERAL("=?UTF-8*en?Q?hello?="),
		XMAIL_WORD_STRICT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "hello") == 0),
		"UTF-8 encoded-word language suffix was not ignored");
	testRequire(xrtMailWordDecodeWrite(
		XRT_STR_LITERAL("=?ISO-8859-1*fr?B?Y2Fm6Q==?="),
		XMAIL_WORD_STRICT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "caf\xC3\xA9") == 0),
		"legacy encoded-word language suffix was not ignored");

	memcpy(arrBoundary, "=?UTF8?Q?", 9u);
	memset(arrBoundary + 9u, 'a', 64u);
	memcpy(arrBoundary + 73u, "?=", 2u);
	arrBoundary[75] = 0;
	testRequire(xrtMailWordDecodeWrite(
		testMailViewN(arrBoundary, 75u),
		XMAIL_WORD_STRICT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == 64u),
		"maximum-size encoded-word decode failed");
}



/* 验证严格失败原子性和显式容错保留策略。 */
static void testMailWordErrors(void)
{
	static const char sUnknown[] = "=?KOI8-R?Q?caf=E9?=";
	static const char sMalformed[] = "=?UTF-8?B?%%%?=";
	char arrOutput[128] = "keep";
	size_t iSize = 0;

	xrtClearError();
	testRequire(!xrtMailWordDecodeWrite(
		XRT_STR_LITERAL(sUnknown),
		XMAIL_WORD_STRICT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "keep") == 0),
		"unsupported charset modified strict output");
	testRequire(xrtErrorFind(
		xrtGetError(),
		"xrt.mail",
		XMAIL_ERROR_CHARSET
	) != NULL, "unsupported charset error mismatch");
	testRequire(xrtMailWordDecodeWrite(
		XRT_STR_LITERAL(sUnknown),
		XMAIL_WORD_RELAXED,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, sUnknown) == 0),
		"relaxed decoder did not preserve unknown charset");

	memcpy(arrOutput, "keep", 5u);
	xrtClearError();
	testRequire(!xrtMailWordDecodeWrite(
		XRT_STR_LITERAL("=?KOI8-R*ru?Q?caf=E9?="),
		XMAIL_WORD_STRICT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "keep") == 0),
		"language suffix bypassed unsupported charset handling");

	memcpy(arrOutput, "keep", 5u);
	xrtClearError();
	testRequire(!xrtMailWordDecodeWrite(
		XRT_STR_LITERAL(sMalformed),
		XMAIL_WORD_STRICT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "keep") == 0),
		"malformed encoded-word modified strict output");
	testRequire(xrtMailWordDecodeWrite(
		XRT_STR_LITERAL(sMalformed),
		XMAIL_WORD_RELAXED,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, sMalformed) == 0),
		"relaxed decoder did not preserve malformed word");

	memcpy(arrOutput, "keep", 5u);
	testRequire(!xrtMailWordDecodeWrite(
		XRT_STR_LITERAL("=?UTF-8?Q?bad=0Aline?="),
		XMAIL_WORD_STRICT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "keep") == 0),
		"encoded header injection was accepted");

	memcpy(arrOutput, "keep", 5u);
	testRequire(!xrtMailWordEncodeWrite(
		XRT_STR_LITERAL("中文"),
		XMAIL_WORD_BASE64,
		arrOutput,
		8u,
		&iSize
	) && (memcmp(arrOutput, "keep", 5u) == 0) && (iSize > 8u),
		"short encoded-word buffer published partial output");
}



/* 运行邮件编码词全部契约测试。 */
int main(void)
{
	testMailWordParse();
	testMailWordEncode();
	testMailWordDecode();
	testMailWordLegacyCharset();
	testMailWordErrors();
	return 0;
}
