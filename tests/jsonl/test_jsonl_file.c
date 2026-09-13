#include "../test.h"

int main(void)
{
	cstr Path = ".xrt-jsonl-file-test.jsonl";
	xvalue* pArray = xrtValueArray();
	xvalue* pRead;
	xjsonlreadconfig Read;
	xjsonlwriteconfig Write;
	bytes Data;
	size_t Size;
	testRequire(pArray != NULL && xrtValueArrayAppendNew(pArray, xrtValueInt(42)), "file fixture");
	testRequire(xrtJsonlStringifyFile(Path, pArray), "default file write");
	pRead = xrtJsonlParseFile(Path);
	testRequire(pRead != NULL && xrtValueCount(pRead) == 1, "default file read");
	xrtValueRelease(pRead);
	xrtJsonlWriteConfigInit(&Write);
	Write.MaxOutputBytes = 2;
	testRequire(!xrtJsonlWriteFile(Path, pArray, &Write), "file write limit not enforced");
	xrtClearError();
	Data = xrtFileReadAll(Path, &Size);
	testRequire(Data != NULL && Size == 3 && memcmp(Data, "42\n", 3) == 0, "failed write damaged file");
	xrtFree(Data);
	xrtJsonlReadConfigInit(&Read);
	Read.MaxInputBytes = 2;
	testRequire(xrtJsonlReadFile(Path, &Read) == NULL &&
		xrtErrorCode(xrtGetError()) == XJSONL_ERROR_IO && xrtErrorCause(xrtGetError()) != NULL, "file read limit/cause");
	xrtClearError();
	testRequire(xrtFileWriteAll(Path, XRT_BYTES_LITERAL("\r\n42\n\n")), "blank file fixture");
	xrtJsonlReadConfigInit(&Read);
	Read.Flags = XJSONL_READ_REJECT_EMPTY_LINES;
	testRequire(xrtJsonlReadFile(Path, &Read) == NULL, "file ignored strict setting");
	xrtClearError();
	pRead = xrtJsonlParseFile(Path);
	testRequire(pRead != NULL && xrtValueCount(pRead) == 1, "file blank defaults");
	xrtValueRelease(pRead);
	testRequire(xrtValueArrayRemove(pArray, 0, 1) && xrtJsonlStringifyFile(Path, pArray), "empty file write");
	pRead = xrtJsonlParseFile(Path);
	testRequire(pRead != NULL && xrtValueCount(pRead) == 0, "empty file read");
	xrtValueRelease(pRead);
	xrtValueRelease(pArray);
	testRequire(xrtFileDelete(Path), "file cleanup");
	testRequire(xrtJsonlParseFile(Path) == NULL && xrtErrorCause(xrtGetError()) != NULL, "missing file error cause");
	xrtClearError();
	printf("[PASS] JSONL file\n");
	return 0;
}

