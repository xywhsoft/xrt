#include "../test.h"

int main(void)
{
	cstr Path = ".xrt-xlonl-file-test.xlonl";
	xvalue* pArray = xrtValueArray();
	xvalue* pRead;
	xxlonlreadconfig Read;
	xxlonlwriteconfig Write;
	bytes Data;
	size_t Size;
	testRequire(pArray != NULL && xrtValueArrayAppendNew(pArray, xrtValueInt(42)), "file fixture");
	testRequire(xrtXlonlStringifyFile(Path, pArray), "default file write");
	pRead = xrtXlonlParseFile(Path);
	testRequire(pRead != NULL && xrtValueCount(pRead) == 1, "default file read");
	xrtValueRelease(pRead);
	xrtXlonlWriteConfigInit(&Write);
	Write.MaxOutputBytes = 2;
	testRequire(!xrtXlonlWriteFile(Path, pArray, &Write), "file write limit not enforced");
	xrtClearError();
	Data = xrtFileReadAll(Path, &Size);
	testRequire(Data != NULL && Size == 3 && memcmp(Data, "42\n", 3) == 0, "failed write damaged file");
	xrtFree(Data);
	xrtXlonlReadConfigInit(&Read);
	Read.MaxInputBytes = 2;
	testRequire(xrtXlonlReadFile(Path, &Read) == NULL &&
		xrtErrorCode(xrtGetError()) == XXLONL_ERROR_IO && xrtErrorCause(xrtGetError()) != NULL, "file read limit/cause");
	xrtClearError();
	testRequire(xrtFileWriteAll(Path, XRT_BYTES_LITERAL("\r\n42\n\n")), "blank file fixture");
	xrtXlonlReadConfigInit(&Read);
	Read.Flags = XXLONL_READ_REJECT_EMPTY_LINES;
	testRequire(xrtXlonlReadFile(Path, &Read) == NULL, "file ignored strict setting");
	xrtClearError();
	pRead = xrtXlonlParseFile(Path);
	testRequire(pRead != NULL && xrtValueCount(pRead) == 1, "file blank defaults");
	xrtValueRelease(pRead);
	testRequire(xrtValueArrayRemove(pArray, 0, 1) && xrtXlonlStringifyFile(Path, pArray), "empty file write");
	pRead = xrtXlonlParseFile(Path);
	testRequire(pRead != NULL && xrtValueCount(pRead) == 0, "empty file read");
	xrtValueRelease(pRead);
	xrtValueRelease(pArray);
	testRequire(xrtFileDelete(Path), "file cleanup");
	testRequire(xrtXlonlParseFile(Path) == NULL && xrtErrorCause(xrtGetError()) != NULL, "missing file error cause");
	xrtClearError();
	printf("[PASS] XLONL file\n");
	return 0;
}

