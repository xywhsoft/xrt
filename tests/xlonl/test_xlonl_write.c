#include "../test.h"

static bool collect(xbytesview Data, ptr pUserData)
{
	return xrtBufferAppend((xbuffer*)pUserData, Data);
}
static bool failSink(xbytesview Data, ptr pUserData)
{
	(void)Data;
	(void)pUserData;
	xrtSetErrorInfo(XERR_IO, "test.sink", 71, "sink failure");
	return false;
}
static bool silentSink(xbytesview Data, ptr pUserData)
{
	(void)Data; (void)pUserData;
	return false;
}
static bool mutateSink(xbytesview Data, ptr pUserData)
{
	xvalue* pArray = (xvalue*)pUserData;
	(void)Data;
	return xrtValueArrayRemove(pArray, 0, xrtValueCount(pArray));
}
typedef struct testxlonlencoder {
	xxlonlwriteconfig* Config;
	int Calls;
	bool Fail;
} testxlonlencoder;

/* 编码器输出转义载荷，并修改原配置来验证跨记录快照。 */
static xxloncoderesult encodeCustom(
	const xvalue* pValue, xstrview* pTag, xstrview* pPayload, ptr pUserData
)
{
	testxlonlencoder* pState = (testxlonlencoder*)pUserData;
	pState->Calls++;
	testRequire(xrtValueType(pValue) == XVALUE_POINTER, "custom encoder type changed");
	pState->Config->Record.Encode = NULL;
	pState->Config->MaxOutputBytes = 1;
	if ( pState->Fail && (pState->Calls == 2) ) {
		xrtSetErrorInfo(XERR_VALUE, "test.xlonl.encode", 73, "encoder failure");
		return XXLON_CODE_ERROR;
	}
	*pTag = XRT_STR_LITERAL("app.ptr");
	*pPayload = XRT_STR_LITERAL("a\nb");
	return XXLON_CODE_OK;
}

static void testCustomWrite(void)
{
	xxlonlwriteconfig Config;
	testxlonlencoder State = { &Config, 0, false };
	xvalue* pArray = xrtValueArray();
	xvalue* pPointer = xrtValuePointer(&State);
	xbuffer Buffer;
	xstrview Expected = XRT_STR_LITERAL("app.ptr(\"a\\nb\")\napp.ptr(\"a\\nb\")\n");
	const xerror* pCause;
	testRequire(pArray != NULL && pPointer != NULL && xrtBufferInit(&Buffer) &&
		xrtValueArrayAppend(pArray, pPointer) && xrtValueArrayAppend(pArray, pPointer), "custom fixture");
	xrtValueRelease(pPointer);
	xrtXlonlWriteConfigInit(&Config);
	Config.Record.Unsupported = XXLON_UNSUPPORTED_SKIP;
	testRequire(!xrtXlonlWrite(pArray, &Config, collect, &Buffer) && Buffer.Size == 0,
		"whole unsupported record skipped");
	xrtClearError();
	xrtXlonlWriteConfigInit(&Config);
	Config.Record.Encode = encodeCustom;
	Config.Record.EncodeData = &State;
	testRequire(xrtXlonlWrite(pArray, &Config, collect, &Buffer) && State.Calls == 2 &&
		Buffer.Size == Expected.Size && memcmp(Buffer.Data, Expected.Data, Expected.Size) == 0,
		"custom write snapshot or single-line escaping lost");
	xrtBufferClear(&Buffer);
	xrtXlonlWriteConfigInit(&Config);
	Config.Record.Encode = encodeCustom;
	Config.Record.EncodeData = &State;
	State.Calls = 0;
	State.Fail = true;
	testRequire(!xrtXlonlWrite(pArray, &Config, collect, &Buffer) && State.Calls == 2 &&
		xrtErrorCode(xrtGetError()) == XXLONL_ERROR_RECORD, "custom write failure not reported");
	pCause = xrtGetError();
	while ( pCause != NULL && xrtErrorCode(pCause) != 73 ) {
		pCause = xrtErrorCause(pCause);
	}
	testRequire(pCause != NULL, "custom encoder cause lost");
	xrtClearError();
	xrtBufferUnit(&Buffer);
	xrtValueRelease(pArray);
}

int main(void)
{
	xxlonlwriteconfig Config;
	xvalue* pArray = xrtValueArray();
	xbuffer Buffer;
	str Text;
	size_t Size = 99;
	testRequire(pArray != NULL && xrtBufferInit(&Buffer), "write fixture");
	testRequire(xrtValueArrayAppendNew(pArray, xrtValueInt(1)) &&
		xrtValueArrayAppendNew(pArray, xrtValueString(XRT_STR_LITERAL("a\nb"))) &&
		xrtValueArrayAppend(pArray, xrtValueNull()) &&
		xrtValueArrayAppendNew(pArray, xrtValueArray()), "write array construction");
	Text = xrtXlonlStringify(pArray, &Size);
	testRequire(Text != NULL && Size == strlen("1\n\"a\\nb\"\nnull\n[]\n") &&
		strcmp(Text, "1\n\"a\\nb\"\nnull\n[]\n") == 0, "canonical LF output");
	xrtFree(Text);
	xrtXlonlWriteConfigInit(&Config);
	testRequire(xrtXlonlWrite(pArray, &Config, collect, &Buffer) && Buffer.Size == Size, "sink output");
	xrtBufferClear(&Buffer);
	Config.Record.Flags |= XXLON_WRITE_PRETTY;
	testRequire(!xrtXlonlWrite(pArray, &Config, collect, &Buffer) && Buffer.Size == 0 &&
		xrtErrorCode(xrtGetError()) == XXLONL_ERROR_CONFIG, "PRETTY accepted");
	xrtClearError();
	xrtXlonlWriteConfigInit(&Config);
	Config.MaxRecords = 1;
	testRequire(!xrtXlonlWrite(pArray, &Config, collect, &Buffer) && Buffer.Size == 0, "records limit");
	xrtClearError();
	xrtXlonlWriteConfigInit(&Config);
	Config.MaxOutputBytes = 1;
	testRequire(!xrtXlonlWrite(pArray, &Config, collect, &Buffer) && Buffer.Size == 1 &&
		xrtErrorCode(xrtGetError()) == XXLONL_ERROR_LIMIT, "LF missing from output budget");
	xrtClearError();
	xrtBufferClear(&Buffer);
	xrtXlonlWriteConfigInit(&Config);
	testRequire(!xrtXlonlWrite(pArray, &Config, failSink, NULL) &&
		xrtErrorCode(xrtGetError()) == XXLONL_ERROR_OUTPUT &&
		xrtErrorCause(xrtGetError()) != NULL &&
		xrtErrorCode(xrtErrorCause(xrtGetError())) == 71, "sink cause lost");
	xrtClearError();
	testRequire(!xrtXlonlWrite(pArray, &Config, silentSink, NULL) &&
		xrtErrorCode(xrtGetError()) == XXLONL_ERROR_OUTPUT, "silent callback missing error");
	xrtClearError();
	Size = 99;
	Text = xrtXlonlStringify(xrtValueNull(), &Size);
	testRequire(Text == NULL && Size == 99 && xrtErrorKind(xrtGetError()) == XERR_TYPE, "non-array accepted");
	xrtClearError();
	testRequire(xrtXlonlWrite(pArray, &Config, mutateSink, pArray) &&
		xrtValueCount(pArray) == 0, "outer array snapshot invalidated");
	Text = xrtXlonlStringify(pArray, &Size);
	testRequire(Text != NULL && Size == 0 && Text[0] == 0, "empty output must be owned empty string");
	xrtFree(Text);
	Config.Record.Unsupported = XXLON_UNSUPPORTED_SKIP;
	testRequire(xrtValueArrayAppendNew(pArray, xrtValueBytes(XRT_BYTES_LITERAL("x"))), "bytes fixture");
	testRequire(xrtXlonlWrite(pArray, &Config, collect, &Buffer), "bytes serialization failed");

	xrtBufferUnit(&Buffer);
	xrtValueRelease(pArray);
	testCustomWrite();
	printf("[PASS] XLONL write\n");
	return 0;
}
