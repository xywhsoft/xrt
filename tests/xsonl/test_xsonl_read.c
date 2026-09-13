#include "../test.h"

static void expectError(xstrview Text, const xxsonlreadconfig* pConfig, int32 Code)
{
	xvalue* pValue = xrtXsonlRead(Text, pConfig);
	testRequire(pValue == NULL && xrtGetError() != NULL, "expected read failure");
	testRequire(strcmp(xrtErrorDomain(xrtGetError()), "xrt.xsonl") == 0 &&
		xrtErrorCode(xrtGetError()) == Code, "unexpected error domain/code");
	xrtClearError();
}

typedef struct testxsonldecoder {
	xxsonlreadconfig* Config;
	int Calls;
	bool Fail;
} testxsonldecoder;

/* 修改调用方配置，验证后续记录仍使用入口时的快照。 */
static xvalue* decodeCustom(xstrview Tag, xstrview Payload, ptr pUserData)
{
	testxsonldecoder* pState = (testxsonldecoder*)pUserData;
	pState->Calls++;
	testRequire(Tag.Size == 7 && memcmp(Tag.Data, "app.str", 7) == 0, "custom tag changed");
	pState->Config->Record.Decode = NULL;
	pState->Config->MaxRecords = 0;
	if ( pState->Fail && (pState->Calls == 2) ) {
		xrtSetErrorInfo(XERR_VALUE, "test.xsonl.decode", 72, "decoder failure");
		return NULL;
	}
	return xrtValueString(Payload);
}

static void testCustomRead(void)
{
	xxsonlreadconfig Config;
	testxsonldecoder State = { &Config, 0, false };
	xstrview Text = XRT_STR_LITERAL("\napp.str(\"a\\nb\")\r\n\r\napp.str(\"two\")\n");
	xstrview String;
	xxsonllocation Location;
	xvalue* pArray;
	xrtXsonlReadConfigInit(&Config);
	expectError(Text, &Config, XXSONL_ERROR_RECORD);
	Config.Record.Flags = XXSON_READ_CUSTOM;
	Config.Record.Decode = decodeCustom;
	Config.Record.DecodeData = &State;
	pArray = xrtXsonlRead(Text, &Config);
	testRequire(pArray != NULL && xrtValueCount(pArray) == 2 && State.Calls == 2,
		"custom read configuration snapshot lost");
	testRequire(xrtValueGetString(xrtValueArrayGet(pArray, 0), &String) &&
		String.Size == 3 && memcmp(String.Data, "a\nb", 3) == 0, "custom payload escape lost");
	xrtValueRelease(pArray);
	xrtXsonlReadConfigInit(&Config);
	Config.Record.Flags = XXSON_READ_CUSTOM;
	Config.Record.Decode = decodeCustom;
	Config.Record.DecodeData = &State;
	State.Calls = 0;
	State.Fail = true;
	pArray = xrtXsonlRead(Text, &Config);
	testRequire(pArray == NULL && State.Calls == 2 &&
		xrtErrorCode(xrtGetError()) == XXSONL_ERROR_RECORD &&
		xrtErrorCause(xrtGetError()) != NULL &&
		xrtErrorCode(xrtErrorCause(xrtGetError())) == 72, "custom read failure cause lost");
	testRequire(xrtXsonlErrorLocation(xrtGetError(), &Location) &&
		Location.Line == 4 && Location.RecordIndex == 1, "custom failure record position lost");
	xrtClearError();
}

int main(void)
{
	xxsonlreadconfig Config;
	xxsonllocation Location;
	xvalue* pArray;
	xvalue* pValue;
	xstrview String;
	int64 Value;
	xrtXsonlReadConfigInit(&Config);
	pArray = xrtXsonlParse(XRT_STR_LITERAL("\n\r\n \t\r\n{\"id\":1}\n\n[2,3]\r\nnull\n\"a\\nb\"\ntrue\n18446744073709551615\n"));
	testRequire(pArray != NULL && xrtValueType(pArray) == XVALUE_ARRAY &&
		xrtValueCount(pArray) == 6, "mixed/blank record parse");
	testRequire(xrtValueType(xrtValueArrayGet(pArray, 1)) == XVALUE_ARRAY &&
		xrtValueCount(xrtValueArrayGet(pArray, 1)) == 2, "nested array flattened");
	testRequire(xrtValueType(xrtValueArrayGet(pArray, 2)) == XVALUE_NULL, "null record lost");
	testRequire(xrtValueGetString(xrtValueArrayGet(pArray, 3), &String) &&
		String.Size == 3 && memcmp(String.Data, "a\nb", 3) == 0, "escaped newline split");
	xrtValueRelease(pArray);
	pArray = xrtXsonlParse(XRT_STR_LITERAL("\n\r\n \t\r\n"));
	testRequire(pArray != NULL && xrtValueCount(pArray) == 0, "blank document not empty");
	xrtValueRelease(pArray);
	pArray = xrtXsonlParse((xstrview){ NULL, 0 });
	testRequire(pArray != NULL && xrtValueCount(pArray) == 0, "empty view not empty");
	xrtValueRelease(pArray);
	pArray = xrtXsonlParse(XRT_STR_LITERAL("42"));
	testRequire(pArray != NULL && xrtValueCount(pArray) == 1 &&
		xrtValueGetInt(xrtValueArrayGet(pArray, 0), &Value) && Value == 42, "unterminated last line");
	xrtValueRelease(pArray);
	testRequire(xrtXsonlValid(XRT_STR_LITERAL("\n{}\r\n\r\n[]\n")), "valid rejects blank lines");
	testRequire(!xrtXsonlValid(XRT_STR_LITERAL("{} {}\n")), "valid accepted multiple roots");
	xrtClearError();
	Config.Flags = XXSONL_READ_REJECT_EMPTY_LINES;
	pArray = xrtXsonlRead((xstrview){ NULL, 0 }, &Config);
	testRequire(pArray != NULL && xrtValueCount(pArray) == 0, "strict mode rejected zero-byte input");
	xrtValueRelease(pArray);
	expectError(XRT_STR_LITERAL("\n"), &Config, XXSONL_ERROR_SYNTAX);
	expectError(XRT_STR_LITERAL(" \t\r\n"), &Config, XXSONL_ERROR_SYNTAX);
	expectError(XRT_STR_LITERAL("{}\n\n"), &Config, XXSONL_ERROR_SYNTAX);
	pArray = xrtXsonlRead(XRT_STR_LITERAL("{}\r\n"), &Config);
	testRequire(pArray != NULL && xrtValueCount(pArray) == 1, "final terminator is not an empty record");
	xrtValueRelease(pArray);
	xrtXsonlReadConfigInit(&Config);
	pArray = xrtXsonlRead(XRT_STR_LITERAL("{}\r\n\r\n  ?\n"), &Config);
	testRequire(pArray == NULL && xrtXsonlErrorLocation(xrtGetError(), &Location), "global error position");
	testRequire(Location.Line == 3 && Location.Column == 3 && Location.Offset == 8 &&
		Location.RecordIndex == 1 && xrtErrorCause(xrtGetError()) != NULL, "position/cause mismatch");
	xrtClearError();
	expectError(XRT_STR_LITERAL("{} {}"), &Config, XXSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("{}\r{}"), &Config, XXSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("\xC2\xA0\n"), &Config, XXSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("{\n\"id\":1}\n"), &Config, XXSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("\xEF\xBB\xBF{}"), &Config, XXSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("\v\n"), &Config, XXSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("\"\xFF\"\n"), &Config, XXSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("{}\n{\"id\":"), &Config, XXSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("{\"a\":1,\"a\":2}\n"), &Config, XXSONL_ERROR_RECORD);
	Config.Record.Duplicate = XXSON_DUPLICATE_REPLACE;
	pArray = xrtXsonlRead(XRT_STR_LITERAL("{\"a\":1,\"a\":2}\n"), &Config);
	testRequire(pArray != NULL, "duplicate policy lost");
	pValue = xrtValueObjectGet(xrtValueArrayGet(pArray, 0), XRT_STR_LITERAL("a"));
	testRequire(xrtValueGetInt(pValue, &Value) && Value == 2, "duplicate replace mismatch");
	xrtValueRelease(pArray);
	Config.MaxTotalValues = 2;
	expectError(XRT_STR_LITERAL("{\"a\":1,\"a\":2}\n"), &Config, XXSONL_ERROR_LIMIT);
	xrtXsonlReadConfigInit(&Config);
	Config.MaxRecords = 1;
	expectError(XRT_STR_LITERAL("0\n\n1"), &Config, XXSONL_ERROR_LIMIT);
	Config.MaxRecords = 3;
	Config.MaxTotalValues = 3;
	pArray = xrtXsonlRead(XRT_STR_LITERAL("[0]\n1"), &Config);
	testRequire(pArray != NULL, "exact total budget rejected");
	xrtValueRelease(pArray);
	expectError(XRT_STR_LITERAL("[0]\n[1]"), &Config, XXSONL_ERROR_LIMIT);
	xrtXsonlReadConfigInit(&Config);
	Config.MaxInputBytes = 3;
	expectError(XRT_STR_LITERAL("0\n\n\n"), &Config, XXSONL_ERROR_LIMIT);
	xrtXsonlReadConfigInit(&Config);
	Config.Record.MaxInputBytes = 2;
	expectError(XRT_STR_LITERAL("   \n"), &Config, XXSONL_ERROR_LIMIT);
	pArray = xrtXsonlRead(XRT_STR_LITERAL("{}\r\n"), &Config);
	testRequire(pArray != NULL, "CRLF incorrectly counted in record budget");
	xrtValueRelease(pArray);
	Config.Reserved[0] = 1;
	expectError(XRT_STR_LITERAL(""), &Config, XXSONL_ERROR_CONFIG);
	xrtXsonlReadConfigInit(&Config);
	Config.Record.Reserved[0] = 1;
	expectError(XRT_STR_LITERAL(""), &Config, XXSONL_ERROR_CONFIG);
	xrtXsonlReadConfigInit(&Config);
	Config.Record.Flags = XXSON_READ_COMMENTS | XXSON_READ_TRAILING_COMMA;
	pArray = xrtXsonlRead(XRT_STR_LITERAL("[1,] // comment\n/*ok*/2\n"), &Config);
	testRequire(pArray != NULL && xrtValueCount(pArray) == 2, "record compatibility flags lost");
	xrtValueRelease(pArray);
	expectError(XRT_STR_LITERAL("/*\n*/1\n"), &Config, XXSONL_ERROR_RECORD);
	expectError(XRT_STR_LITERAL("// comment only\n1\n"), &Config, XXSONL_ERROR_RECORD);
	xrtXsonlReadConfigInit(&Config);
	Config.Record.MaxValues = 1;
	Config.Record.MaxContainerItems = 1;
	pArray = xrtXsonlRead(XRT_STR_LITERAL("1\n2\n"), &Config);
	testRequire(pArray != NULL && xrtValueCount(pArray) == 2, "per-record limits applied to aggregate array");
	xrtValueRelease(pArray);
	Config.Record.MaxValues = 3;
	expectError(XRT_STR_LITERAL("[1,2]\n"), &Config, XXSONL_ERROR_LIMIT);

	xrtXsonlReadConfigInit(&Config);
	pArray = xrtXsonlParse(XRT_STR_LITERAL("bytes(\"AQID\")\nset[1,2]\nintmap{2:3}\ntime(\"2000-01-01T00:00:00Z\")\nfloat(\"nan\")\n"));
	testRequire(pArray != NULL && xrtValueCount(pArray) == 5 &&
		xrtValueType(xrtValueArrayGet(pArray, 0)) == XVALUE_BYTES &&
		xrtValueType(xrtValueArrayGet(pArray, 1)) == XVALUE_SET &&
		xrtValueType(xrtValueArrayGet(pArray, 2)) == XVALUE_INT_MAP, "XSON types lost");
	xrtValueRelease(pArray);
	Config.MaxTotalDecodedBytes = 5;
	expectError(XRT_STR_LITERAL("bytes(\"AQID\")\nbytes(\"AQID\")\n"), &Config, XXSONL_ERROR_LIMIT);
	Config.MaxTotalDecodedBytes = 6;
	pArray = xrtXsonlRead(XRT_STR_LITERAL("bytes(\"AQID\")\nbytes(\"AQID\")\n"), &Config);
	testRequire(pArray != NULL, "exact decoded budget rejected");
	xrtValueRelease(pArray);
	testRequire(!xrtXsonlValid(XRT_STR_LITERAL("bytes(\"!!!\")\n")), "invalid built-in tag accepted");
	xrtClearError();

	testCustomRead();
	printf("[PASS] XSONL read\n");
	return 0;
}
