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
int main(void)
{
	xjsonlwriteconfig Config;
	xvalue* pArray = xrtValueArray();
	xbuffer Buffer;
	str Text;
	size_t Size = 99;
	testRequire(pArray != NULL && xrtBufferInit(&Buffer), "write fixture");
	testRequire(xrtValueArrayAppendNew(pArray, xrtValueInt(1)) &&
		xrtValueArrayAppendNew(pArray, xrtValueString(XRT_STR_LITERAL("a\nb"))) &&
		xrtValueArrayAppend(pArray, xrtValueNull()) &&
		xrtValueArrayAppendNew(pArray, xrtValueArray()), "write array construction");
	Text = xrtJsonlStringify(pArray, &Size);
	testRequire(Text != NULL && Size == strlen("1\n\"a\\nb\"\nnull\n[]\n") &&
		strcmp(Text, "1\n\"a\\nb\"\nnull\n[]\n") == 0, "canonical LF output");
	xrtFree(Text);
	xrtJsonlWriteConfigInit(&Config);
	testRequire(xrtJsonlWrite(pArray, &Config, collect, &Buffer) && Buffer.Size == Size, "sink output");
	xrtBufferClear(&Buffer);
	Config.Record.Flags |= XJSON_WRITE_PRETTY;
	testRequire(!xrtJsonlWrite(pArray, &Config, collect, &Buffer) && Buffer.Size == 0 &&
		xrtErrorCode(xrtGetError()) == XJSONL_ERROR_CONFIG, "PRETTY accepted");
	xrtClearError();
	xrtJsonlWriteConfigInit(&Config);
	Config.MaxRecords = 1;
	testRequire(!xrtJsonlWrite(pArray, &Config, collect, &Buffer) && Buffer.Size == 0, "records limit");
	xrtClearError();
	xrtJsonlWriteConfigInit(&Config);
	Config.MaxOutputBytes = 1;
	testRequire(!xrtJsonlWrite(pArray, &Config, collect, &Buffer) && Buffer.Size == 1 &&
		xrtErrorCode(xrtGetError()) == XJSONL_ERROR_LIMIT, "LF missing from output budget");
	xrtClearError();
	xrtBufferClear(&Buffer);
	xrtJsonlWriteConfigInit(&Config);
	testRequire(!xrtJsonlWrite(pArray, &Config, failSink, NULL) &&
		xrtErrorCode(xrtGetError()) == XJSONL_ERROR_OUTPUT &&
		xrtErrorCause(xrtGetError()) != NULL &&
		xrtErrorCode(xrtErrorCause(xrtGetError())) == 71, "sink cause lost");
	xrtClearError();
	testRequire(!xrtJsonlWrite(pArray, &Config, silentSink, NULL) &&
		xrtErrorCode(xrtGetError()) == XJSONL_ERROR_OUTPUT, "silent callback missing error");
	xrtClearError();
	Size = 99;
	Text = xrtJsonlStringify(xrtValueNull(), &Size);
	testRequire(Text == NULL && Size == 99 && xrtErrorKind(xrtGetError()) == XERR_TYPE, "non-array accepted");
	xrtClearError();
	testRequire(xrtJsonlWrite(pArray, &Config, mutateSink, pArray) &&
		xrtValueCount(pArray) == 0, "outer array snapshot invalidated");
	Text = xrtJsonlStringify(pArray, &Size);
	testRequire(Text != NULL && Size == 0 && Text[0] == 0, "empty output must be owned empty string");
	xrtFree(Text);
	Config.Record.Unsupported = XJSON_UNSUPPORTED_SKIP;
	testRequire(xrtValueArrayAppendNew(pArray, xrtValueBytes(XRT_BYTES_LITERAL("x"))), "bytes fixture");

	testRequire(!xrtJsonlWrite(pArray, &Config, collect, &Buffer), "root record silently skipped");
	xrtClearError();

	xrtBufferUnit(&Buffer);
	xrtValueRelease(pArray);
	printf("[PASS] JSONL write\n");
	return 0;
}
