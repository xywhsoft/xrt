#include "../test.h"

int main(void)
{
	cstr Path = ".xrt-xsonl-file-test.xsonl";
	xvalue* pArray = xrtValueArray();
	xvalue* pRead;
	xxsonlreadconfig Read;
	xxsonlwriteconfig Write;
	bytes Data;
	size_t Size;
	testRequire(pArray != NULL && xrtValueArrayAppendNew(pArray, xrtValueInt(42)), "file fixture");
	testRequire(xrtXsonlStringifyFile(Path, pArray), "default file write");
	pRead = xrtXsonlParseFile(Path);
	testRequire(pRead != NULL && xrtValueCount(pRead) == 1, "default file read");
	xrtValueRelease(pRead);
	xrtXsonlWriteConfigInit(&Write);
	Write.MaxOutputBytes = 2;
	testRequire(!xrtXsonlWriteFile(Path, pArray, &Write), "file write limit not enforced");
	xrtClearError();
	Data = xrtFileReadAll(Path, &Size);
	testRequire(Data != NULL && Size == 3 && memcmp(Data, "42\n", 3) == 0, "failed write damaged file");
	xrtFree(Data);
	xrtXsonlReadConfigInit(&Read);
	Read.MaxInputBytes = 2;
	testRequire(xrtXsonlReadFile(Path, &Read) == NULL &&
		xrtErrorCode(xrtGetError()) == XXSONL_ERROR_IO && xrtErrorCause(xrtGetError()) != NULL, "file read limit/cause");
	xrtClearError();
	testRequire(xrtFileWriteAll(Path, XRT_BYTES_LITERAL("\r\n42\n\n")), "blank file fixture");
	xrtXsonlReadConfigInit(&Read);
	Read.Flags = XXSONL_READ_REJECT_EMPTY_LINES;
	testRequire(xrtXsonlReadFile(Path, &Read) == NULL, "file ignored strict setting");
	xrtClearError();
	pRead = xrtXsonlParseFile(Path);
	testRequire(pRead != NULL && xrtValueCount(pRead) == 1, "file blank defaults");
	xrtValueRelease(pRead);
	testRequire(xrtValueArrayRemove(pArray, 0, 1) && xrtXsonlStringifyFile(Path, pArray), "empty file write");
	pRead = xrtXsonlParseFile(Path);
	testRequire(pRead != NULL && xrtValueCount(pRead) == 0, "empty file read");
	xrtValueRelease(pRead);
	xrtValueRelease(pArray);
	testRequire(xrtFileDelete(Path), "file cleanup");
	testRequire(xrtXsonlParseFile(Path) == NULL && xrtErrorCause(xrtGetError()) != NULL, "missing file error cause");
	xrtClearError();
	printf("[PASS] XSONL file\n");
	return 0;
}

