#include "../test.h"



/* 比较借用视图与常量文本。 */
static bool testMailParamEqual(xstrview Text, const char* sValue)
{
	size_t iSize = strlen(sValue);

	return (Text.Size == iSize) && (memcmp(Text.Data, sValue, iSize) == 0);
}



/* 验证媒体类型、处置类型和逐参数游标。 */
static void testMailParamCursor(void)
{
	static const char sValue[] =
		"text/plain; charset=UTF-8; format=\"flowed\"";
	xmailmediatypeview MediaType;
	xmaildispositionview Disposition;
	xmailparamcursor Cursor;
	xmailparamview Parameter;

	testRequire(xrtMailMediaTypeParse(
		XRT_STR_LITERAL(sValue),
		&MediaType
	) && testMailParamEqual(MediaType.Type, "text") &&
		testMailParamEqual(MediaType.Subtype, "plain"),
		"mail media type parse mismatch");
	testRequire(xrtMailParamCursorInit(&Cursor, MediaType.Parameters) &&
		(xrtMailParamNext(&Cursor, &Parameter) == XMAIL_NEXT_ITEM) &&
		testMailParamEqual(Parameter.Name, "charset") &&
		testMailParamEqual(Parameter.Value, "UTF-8") &&
		(xrtMailParamNext(&Cursor, &Parameter) == XMAIL_NEXT_ITEM) &&
		Parameter.Quoted && testMailParamEqual(Parameter.Value, "flowed") &&
		(xrtMailParamNext(&Cursor, &Parameter) == XMAIL_NEXT_END),
		"mail parameter cursor mismatch");
	testRequire(xrtMailDispositionParse(
		XRT_STR_LITERAL("attachment; filename=report.txt"),
		&Disposition
	) && testMailParamEqual(Disposition.Type, "attachment"),
		"mail disposition parse mismatch");
}



/* 验证 RFC 2231 扩展参数、连续段和 quoted-pair 合并。 */
static void testMailParamFind(void)
{
	static const char sParameters[] =
		"; filename=legacy.txt"
		"; filename*0*=UTF-8'en'%E4%B8%AD"
		"; filename*1*=%E6%96%87.txt";
	char arrOutput[64];
	xmailparaminfo Info;
	size_t iSize = 0;

	testRequire(xrtMailParamFindWrite(
		XRT_STR_LITERAL(sParameters),
		XRT_STR_LITERAL("filename"),
		arrOutput,
		sizeof(arrOutput),
		&iSize,
		&Info
	) == XMAIL_NEXT_ITEM && (strcmp(arrOutput, "中文.txt") == 0) &&
		Info.Extended && Info.Continued && (Info.Sections == 2u) &&
		testMailParamEqual(Info.Charset, "UTF-8") &&
		testMailParamEqual(Info.Language, "en"),
		"mail extended parameter merge mismatch");
	testRequire(xrtMailParamFindWrite(
		XRT_STR_LITERAL("; filename=\"a\\\"b.txt\""),
		XRT_STR_LITERAL("filename"),
		arrOutput,
		sizeof(arrOutput),
		&iSize,
		&Info
	) == XMAIL_NEXT_ITEM && (strcmp(arrOutput, "a\"b.txt") == 0),
		"mail quoted parameter decode mismatch");
	testRequire(xrtMailParamFindWrite(
		XRT_STR_LITERAL("; charset=UTF-8"),
		XRT_STR_LITERAL("filename"),
		arrOutput,
		sizeof(arrOutput),
		&iSize,
		&Info
	) == XMAIL_NEXT_END, "missing mail parameter did not return END");
}



/* 验证参数构建自动选择 token、引号和 UTF-8 扩展表示。 */
static void testMailParamWrite(void)
{
	char arrOutput[128];
	size_t iRequired;
	size_t iSize;
	str sParameter;

	testRequire(xrtMailParamWrite(
		XRT_STR_LITERAL("charset"),
		XRT_STR_LITERAL("UTF-8"),
		XMAIL_PARAM_ENCODING_AUTO,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "; charset=UTF-8") == 0),
		"mail token parameter output mismatch");
	testRequire(xrtMailParamWrite(
		XRT_STR_LITERAL("filename"),
		XRT_STR_LITERAL("a \"b\".txt"),
		XMAIL_PARAM_ENCODING_AUTO,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "; filename=\"a \\\"b\\\".txt\"") == 0),
		"mail quoted parameter output mismatch");
	testRequire(xrtMailParamWrite(
		XRT_STR_LITERAL("filename"),
		XRT_STR_LITERAL("报告 1.txt"),
		XMAIL_PARAM_ENCODING_AUTO,
		NULL,
		0,
		&iRequired
	) && xrtMailParamWrite(
		XRT_STR_LITERAL("filename"),
		XRT_STR_LITERAL("报告 1.txt"),
		XMAIL_PARAM_ENCODING_AUTO,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == iRequired) && (strcmp(
		arrOutput,
		"; filename*=UTF-8''%E6%8A%A5%E5%91%8A%201.txt"
	) == 0), "mail UTF-8 parameter output mismatch");

	sParameter = xrtMailParam(
		XRT_STR_LITERAL("boundary"),
		XRT_STR_LITERAL("mix-123"),
		XMAIL_PARAM_ENCODING_QUOTED,
		&iSize
	);
	testRequire((sParameter != NULL) &&
		(strcmp(sParameter, "; boundary=\"mix-123\"") == 0),
		"owned mail parameter output mismatch");
	xrtFree(sParameter);

	memcpy(arrOutput, "keep", 5u);
	testRequire(!xrtMailParamWrite(
		XRT_STR_LITERAL("filename"),
		XRT_STR_LITERAL("报告 1.txt"),
		XMAIL_PARAM_ENCODING_AUTO,
		arrOutput,
		8u,
		&iSize
	) && (iSize == iRequired) && (memcmp(arrOutput, "keep", 5u) == 0),
		"short mail parameter buffer published partial output");
	testRequire(!xrtMailParamWrite(
		XRT_STR_LITERAL("bad*name"),
		XRT_STR_LITERAL("value"),
		XMAIL_PARAM_ENCODING_AUTO,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	), "mail parameter accepted a reserved name suffix");
	testRequire(!xrtMailParamWrite(
		XRT_STR_LITERAL("name"),
		(xstrview){ "\xC3\x28", 2u },
		XMAIL_PARAM_ENCODING_UTF8,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	), "mail parameter accepted invalid UTF-8");
}



/* 验证长参数会分段写出，并可由解析端无损合并。 */
static void testMailParamWriteSections(void)
{
	char arrToken[XMAIL_PARAM_SECTION_SIZE + 1u];
	char arrQuoted[601];
	char arrUtf8[600];
	char arrOutput[8192];
	char arrDecoded[4097];
	xmailparaminfo Info;
	xmailparamcursor Cursor;
	xmailparamview Parameter;
	size_t iOutputSize;
	size_t iDecodedSize;

	memset(arrToken, 'n', sizeof(arrToken));
	testRequire(xrtMailParamWrite(
		XRT_STR_LITERAL("filename"),
		testMailViewN(arrToken, XMAIL_PARAM_SECTION_SIZE),
		XMAIL_PARAM_ENCODING_TOKEN,
		arrOutput,
		sizeof(arrOutput),
		&iOutputSize
	) && (strstr(arrOutput, "filename*0=") == NULL),
		"mail parameter split an exact-size value");
	testRequire(xrtMailParamWrite(
		XRT_STR_LITERAL("filename"),
		testMailViewN(arrToken, sizeof(arrToken)),
		XMAIL_PARAM_ENCODING_TOKEN,
		arrOutput,
		sizeof(arrOutput),
		&iOutputSize
	) && (strstr(arrOutput, "; filename*0=") == arrOutput) &&
		(strstr(arrOutput, "; filename*1=") != NULL),
		"mail token parameter continuation mismatch");
	testRequire(xrtMailParamFindWrite(
		testMailViewN(arrOutput, iOutputSize),
		XRT_STR_LITERAL("filename"),
		arrDecoded,
		sizeof(arrDecoded),
		&iDecodedSize,
		&Info
	) == XMAIL_NEXT_ITEM && (iDecodedSize == sizeof(arrToken)) &&
		(memcmp(arrDecoded, arrToken, sizeof(arrToken)) == 0) &&
		Info.Continued && (Info.Sections == 2u) && !Info.Extended,
		"mail token parameter continuation did not round-trip");

	for ( size_t i = 0; i < sizeof(arrQuoted); i++ ) {
		static const char arrPattern[] = "a \\\"";

		arrQuoted[i] = arrPattern[i % (sizeof(arrPattern) - 1u)];
	}
	testRequire(xrtMailParamWrite(
		XRT_STR_LITERAL("filename"),
		testMailViewN(arrQuoted, sizeof(arrQuoted)),
		XMAIL_PARAM_ENCODING_QUOTED,
		arrOutput,
		sizeof(arrOutput),
		&iOutputSize
	) && (strstr(arrOutput, "; filename*0=\"") == arrOutput) &&
		(strstr(arrOutput, "; filename*1=\"") != NULL),
		"mail quoted parameter continuation mismatch");
	testRequire(xrtMailParamFindWrite(
		testMailViewN(arrOutput, iOutputSize),
		XRT_STR_LITERAL("filename"),
		arrDecoded,
		sizeof(arrDecoded),
		&iDecodedSize,
		&Info
	) == XMAIL_NEXT_ITEM && (iDecodedSize == sizeof(arrQuoted)) &&
		(memcmp(arrDecoded, arrQuoted, sizeof(arrQuoted)) == 0) &&
		Info.Continued && !Info.Extended,
		"mail quoted parameter continuation did not round-trip");

	for ( size_t i = 0; i < sizeof(arrUtf8); i += 3u ) {
		arrUtf8[i] = (char)0xE4;
		arrUtf8[i + 1u] = (char)0xB8;
		arrUtf8[i + 2u] = (char)0xAD;
	}
	testRequire(xrtMailParamWrite(
		XRT_STR_LITERAL("filename"),
		testMailViewN(arrUtf8, sizeof(arrUtf8)),
		XMAIL_PARAM_ENCODING_UTF8,
		arrOutput,
		sizeof(arrOutput),
		&iOutputSize
	) && (strstr(arrOutput, "; filename*0*=UTF-8''") == arrOutput) &&
		(strstr(arrOutput, "; filename*1*=") != NULL),
		"mail UTF-8 parameter continuation mismatch");
	testRequire(xrtMailParamCursorInit(
		&Cursor,
		testMailViewN(arrOutput, iOutputSize)
	) && (xrtMailParamNext(&Cursor, &Parameter) == XMAIL_NEXT_ITEM) &&
		xrtMailParamDecodeWrite(
			&Parameter,
			arrDecoded,
			sizeof(arrDecoded),
			&iDecodedSize,
			NULL,
			NULL
		) && xrtUtf8Valid(testMailViewN(arrDecoded, iDecodedSize), NULL),
		"mail UTF-8 continuation split a code point");
	testRequire(xrtMailParamFindWrite(
		testMailViewN(arrOutput, iOutputSize),
		XRT_STR_LITERAL("filename"),
		arrDecoded,
		sizeof(arrDecoded),
		&iDecodedSize,
		&Info
	) == XMAIL_NEXT_ITEM && (iDecodedSize == sizeof(arrUtf8)) &&
		(memcmp(arrDecoded, arrUtf8, sizeof(arrUtf8)) == 0) &&
		Info.Continued && Info.Extended &&
		testMailParamEqual(Info.Charset, "UTF-8"),
		"mail UTF-8 parameter continuation did not round-trip");

	memcpy(arrDecoded, "keep", 5u);
	testRequire(!xrtMailParamWrite(
		XRT_STR_LITERAL("filename"),
		testMailViewN(arrUtf8, sizeof(arrUtf8)),
		XMAIL_PARAM_ENCODING_UTF8,
		arrDecoded,
		8u,
		&iDecodedSize
	) && (iDecodedSize == iOutputSize) &&
		(memcmp(arrDecoded, "keep", 5u) == 0),
		"short continuation buffer published partial output");
}



/* 验证连续段数量上限的精确边界。 */
static void testMailParamWriteSectionLimit(void)
{
	char arrValue[(XMAIL_PARAM_SECTION_SIZE * XMAIL_PARAM_SECTIONS_MAX) + 1u];
	char arrOutput[8192];
	char arrDecoded[sizeof(arrValue)];
	xmailparaminfo Info;
	size_t iOutputSize;
	size_t iDecodedSize;

	memset(arrValue, 'n', sizeof(arrValue));
	testRequire(xrtMailParamWrite(
		XRT_STR_LITERAL("filename"),
		testMailViewN(arrValue, sizeof(arrValue) - 1u),
		XMAIL_PARAM_ENCODING_TOKEN,
		arrOutput,
		sizeof(arrOutput),
		&iOutputSize
	), "mail parameter rejected the section limit");
	testRequire(xrtMailParamFindWrite(
		testMailViewN(arrOutput, iOutputSize),
		XRT_STR_LITERAL("filename"),
		arrDecoded,
		sizeof(arrDecoded),
		&iDecodedSize,
		&Info
	) == XMAIL_NEXT_ITEM &&
		(iDecodedSize == (sizeof(arrValue) - 1u)) &&
		(memcmp(arrDecoded, arrValue, iDecodedSize) == 0) &&
		(Info.Sections == XMAIL_PARAM_SECTIONS_MAX),
		"mail parameter section-limit value did not round-trip");
	testRequire(!xrtMailParamWrite(
		XRT_STR_LITERAL("filename"),
		testMailViewN(arrValue, sizeof(arrValue)),
		XMAIL_PARAM_ENCODING_TOKEN,
		arrOutput,
		sizeof(arrOutput),
		&iOutputSize
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"mail parameter accepted too many continuation sections");
}



/* 验证缺段、坏编码和事务式短缓冲。 */
static void testMailParamErrors(void)
{
	char arrOutput[8] = "keep";
	xmailparaminfo Info;
	size_t iSize = 0;

	testRequire(xrtMailParamFindWrite(
		XRT_STR_LITERAL("; name*0*=UTF-8''a; name*2*=c"),
		XRT_STR_LITERAL("name"),
		arrOutput,
		sizeof(arrOutput),
		&iSize,
		&Info
	) == XMAIL_NEXT_ERROR, "mail parameter accepted a missing section");
	testRequire(xrtMailParamFindWrite(
		XRT_STR_LITERAL("; name*=UTF-8''bad%XZ"),
		XRT_STR_LITERAL("name"),
		arrOutput,
		sizeof(arrOutput),
		&iSize,
		&Info
	) == XMAIL_NEXT_ERROR, "mail parameter accepted bad percent encoding");
	testRequire(xrtMailParamFindWrite(
		XRT_STR_LITERAL("; name*=UTF-8''bad%0Dvalue"),
		XRT_STR_LITERAL("name"),
		arrOutput,
		sizeof(arrOutput),
		&iSize,
		&Info
	) == XMAIL_NEXT_ERROR, "mail parameter accepted a decoded control byte");
	memcpy(arrOutput, "keep", 5u);
	testRequire(xrtMailParamFindWrite(
		XRT_STR_LITERAL("; name=long-value"),
		XRT_STR_LITERAL("name"),
		arrOutput,
		5u,
		&iSize,
		&Info
	) == XMAIL_NEXT_ERROR && (memcmp(arrOutput, "keep", 5u) == 0) &&
		(iSize == 10u), "mail parameter short buffer modified output");
}



/* 运行 MIME 参数全部契约测试。 */
int main(void)
{
	testMailParamCursor();
	testMailParamFind();
	testMailParamWrite();
	testMailParamWriteSections();
	testMailParamWriteSectionLimit();
	testMailParamErrors();
	return 0;
}
