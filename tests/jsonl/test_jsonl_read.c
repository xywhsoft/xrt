#include "../test.h"

static void expectError(xstrview Text, const xjsonlreadconfig* pConfig, int32 Code)
{
	xvalue* pValue = xrtJsonlRead(Text, pConfig);
	testRequire(pValue == NULL && xrtGetError() != NULL, "expected read failure");
	testRequire(strcmp(xrtErrorDomain(xrtGetError()), "xrt.jsonl") == 0 &&
		xrtErrorCode(xrtGetError()) == Code, "unexpected error domain/code");
	xrtClearError();
}

int main(void)
{
	xjsonlreadconfig Config;
	xjsonllocation Location;
	xvalue* pArray;
	xvalue* pValue;
	xstrview String;
	int64 Value;
	xrtJsonlReadConfigInit(&Config);
	pArray = xrtJsonlParse(XRT_STR_LITERAL("\n\r\n \t\r\n{\"id\":1}\n\n[2,3]\r\nnull\n\"a\\nb\"\ntrue\n18446744073709551615\n"));
	testRequire(pArray != NULL && xrtValueType(pArray) == XVALUE_ARRAY &&
		xrtValueCount(pArray) == 6, "mixed/blank record parse");
	testRequire(xrtValueType(xrtValueArrayGet(pArray, 1)) == XVALUE_ARRAY &&
		xrtValueCount(xrtValueArrayGet(pArray, 1)) == 2, "nested array flattened");
	testRequire(xrtValueType(xrtValueArrayGet(pArray, 2)) == XVALUE_NULL, "null record lost");
	testRequire(xrtValueGetString(xrtValueArrayGet(pArray, 3), &String) &&
		String.Size == 3 && memcmp(String.Data, "a\nb", 3) == 0, "escaped newline split");
	xrtValueRelease(pArray);
	pArray = xrtJsonlParse(XRT_STR_LITERAL("\n\r\n \t\r\n"));
	testRequire(pArray != NULL && xrtValueCount(pArray) == 0, "blank document not empty");
	xrtValueRelease(pArray);
	pArray = xrtJsonlParse((xstrview){ NULL, 0 });
	testRequire(pArray != NULL && xrtValueCount(pArray) == 0, "empty view not empty");
	xrtValueRelease(pArray);
	pArray = xrtJsonlParse(XRT_STR_LITERAL("42"));
	testRequire(pArray != NULL && xrtValueCount(pArray) == 1 &&
		xrtValueGetInt(xrtValueArrayGet(pArray, 0), &Value) && Value == 42, "unterminated last line");
	xrtValueRelease(pArray);
	testRequire(xrtJsonlValid(XRT_STR_LITERAL("\n{}\r\n\r\n[]\n")), "valid rejects blank lines");
	testRequire(!xrtJsonlValid(XRT_STR_LITERAL("{} {}\n")), "valid accepted multiple roots");
	xrtClearError();
	Config.Flags = XJSONL_READ_REJECT_EMPTY_LINES;
	pArray = xrtJsonlRead((xstrview){ NULL, 0 }, &Config);
	testRequire(pArray != NULL && xrtValueCount(pArray) == 0, "strict mode rejected zero-byte input");
	xrtValueRelease(pArray);
	expectError(XRT_STR_LITERAL("\n"), &Config, XJSONL_ERROR_SYNTAX);
	expectError(XRT_STR_LITERAL(" \t\r\n"), &Config, XJSONL_ERROR_SYNTAX);
	expectError(XRT_STR_LITERAL("{}\n\n"), &Config, XJSONL_ERROR_SYNTAX);
	pArray = xrtJsonlRead(XRT_STR_LITERAL("{}\r\n"), &Config);
	testRequire(pArray != NULL && xrtValueCount(pArray) == 1, "final terminator is not an empty record");
	xrtValueRelease(pArray);
	xrtJsonlReadConfigInit(&Config);
	pArray = xrtJsonlRead(XRT_STR_LITERAL("{}\r\n\r\n  ?\n"), &Config);
	testRequire(pArray == NULL && xrtJsonlErrorLocation(xrtGetError(), &Location), "global error position");
	testRequire(Location.Line == 3 && Location.Column == 3 && Location.Offset == 8 &&
		Location.RecordIndex == 1 && xrtErrorCause(xrtGetError()) != NULL, "position/cause mismatch");
	xrtClearError();
	expectError(XRT_STR_LITERAL("{} {}"), &Config, XJSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("{}\r{}"), &Config, XJSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("\xC2\xA0\n"), &Config, XJSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("{\n\"id\":1}\n"), &Config, XJSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("\xEF\xBB\xBF{}"), &Config, XJSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("\v\n"), &Config, XJSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("\"\xFF\"\n"), &Config, XJSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("{}\n{\"id\":"), &Config, XJSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("{\"a\":1,\"a\":2}\n"), &Config, XJSONL_ERROR_RECORD);
	Config.Record.Duplicate = XJSON_DUPLICATE_REPLACE;
	pArray = xrtJsonlRead(XRT_STR_LITERAL("{\"a\":1,\"a\":2}\n"), &Config);
	testRequire(pArray != NULL, "duplicate policy lost");
	pValue = xrtValueObjectGet(xrtValueArrayGet(pArray, 0), XRT_STR_LITERAL("a"));
	testRequire(xrtValueGetInt(pValue, &Value) && Value == 2, "duplicate replace mismatch");
	xrtValueRelease(pArray);
	Config.MaxTotalValues = 2;
	expectError(XRT_STR_LITERAL("{\"a\":1,\"a\":2}\n"), &Config, XJSONL_ERROR_LIMIT);
	xrtJsonlReadConfigInit(&Config);
	Config.MaxRecords = 1;
	expectError(XRT_STR_LITERAL("0\n\n1"), &Config, XJSONL_ERROR_LIMIT);
	Config.MaxRecords = 3;
	Config.MaxTotalValues = 3;
	pArray = xrtJsonlRead(XRT_STR_LITERAL("[0]\n1"), &Config);
	testRequire(pArray != NULL, "exact total budget rejected");
	xrtValueRelease(pArray);
	expectError(XRT_STR_LITERAL("[0]\n[1]"), &Config, XJSONL_ERROR_LIMIT);
	xrtJsonlReadConfigInit(&Config);
	Config.MaxInputBytes = 3;
	expectError(XRT_STR_LITERAL("0\n\n\n"), &Config, XJSONL_ERROR_LIMIT);
	xrtJsonlReadConfigInit(&Config);
	Config.Record.MaxInputBytes = 2;
	expectError(XRT_STR_LITERAL("   \n"), &Config, XJSONL_ERROR_LIMIT);
	pArray = xrtJsonlRead(XRT_STR_LITERAL("{}\r\n"), &Config);
	testRequire(pArray != NULL, "CRLF incorrectly counted in record budget");
	xrtValueRelease(pArray);
	Config.Reserved[0] = 1;
	expectError(XRT_STR_LITERAL(""), &Config, XJSONL_ERROR_CONFIG);
	xrtJsonlReadConfigInit(&Config);
	Config.Record.Reserved[0] = 1;
	expectError(XRT_STR_LITERAL(""), &Config, XJSONL_ERROR_CONFIG);
	xrtJsonlReadConfigInit(&Config);
	Config.Record.Flags = XJSON_READ_COMMENTS | XJSON_READ_TRAILING_COMMA;
	pArray = xrtJsonlRead(XRT_STR_LITERAL("[1,] // comment\n/*ok*/2\n"), &Config);
	testRequire(pArray != NULL && xrtValueCount(pArray) == 2, "record compatibility flags lost");
	xrtValueRelease(pArray);
	expectError(XRT_STR_LITERAL("/*\n*/1\n"), &Config, XJSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("// comment only\n1\n"), &Config, XJSONL_ERROR_RECORD);
	xrtJsonlReadConfigInit(&Config);
	Config.Record.MaxValues = 1;
	Config.Record.MaxContainerItems = 1;
	pArray = xrtJsonlRead(XRT_STR_LITERAL("1\n2\n"), &Config);
	testRequire(pArray != NULL && xrtValueCount(pArray) == 2, "per-record limits applied to aggregate array");
	xrtValueRelease(pArray);
	Config.Record.MaxValues = 3;
	expectError(XRT_STR_LITERAL("[1,2]\n"), &Config, XJSONL_ERROR_LIMIT);

	printf("[PASS] JSONL read\n");
	return 0;
}
