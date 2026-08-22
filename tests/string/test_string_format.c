#include "../test.h"



/* 验证独立格式化和构建器直接格式化路径。 */
int main(void)
{
	xstrbuf tBuffer;
	str sText;
	int iWritten = 77;

	sText = xrtFormat("%s:%d:%.2f", "value", 42, 3.5);
	testRequire((sText != NULL) && (strcmp(sText, "value:42:3.50") == 0),
		"formatted string mismatch");
	xrtFree(sText);
	sText = xrtFormat("");
	testRequire((sText != NULL) && (sText[0] == 0), "empty format ownership mismatch");
	xrtFree(sText);
	sText = xrtFormat("%%n");
	testRequire((sText != NULL) && (strcmp(sText, "%n") == 0),
		"escaped percent-n format mismatch");
	xrtFree(sText);

	/* %n 具有写内存副作用，安全格式化层必须在调用 C 运行库前拒绝。 */
	xrtClearError();
	testRequire(xrtFormat("unsafe%n", &iWritten) == NULL, "percent-n format must fail");
	testRequire(iWritten == 77, "percent-n format changed caller memory");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtErrorCode(xrtGetError()) == XSTR_ERROR_FORMAT),
		"percent-n format error mismatch");
	xrtClearError();
	testRequire(xrtFormat("%1$n", &iWritten) == NULL,
		"positional percent-n format must fail");
	testRequire(iWritten == 77, "positional percent-n format changed caller memory");
	xrtClearError();

	xrtStrBufInit(&tBuffer);
	testRequire(xrtStrBufAppendFormat(&tBuffer, ""), "empty builder format failed");
	testRequire((tBuffer.Data == NULL) && (tBuffer.Size == 0) && (tBuffer.Capacity == 0),
		"empty builder format changed state");
	testRequire(xrtStrBufAppend(&tBuffer, XRT_STR_LITERAL("prefix")), "format prefix append failed");
	testRequire(xrtStrBufAppendFormat(&tBuffer, "-%08X-%s", 255u, "done"),
		"builder format append failed");
	testRequire((tBuffer.Size == strlen("prefix-000000FF-done")) &&
		(strcmp(tBuffer.Data, "prefix-000000FF-done") == 0), "builder format result mismatch");

	/* 格式参数可以借用构建器当前内容，内部增长不能使参数悬空。 */
	xrtStrBufClear(&tBuffer);
	testRequire(xrtStrBufAppendRepeat(&tBuffer, XRT_STR_LITERAL("01234567"), 8),
		"format alias seed append failed");
	testRequire(xrtStrBufAppendFormat(&tBuffer, ":%s", tBuffer.Data),
		"builder self format failed");
	testRequire((tBuffer.Size == 129) && (tBuffer.Data[64] == ':') &&
		(memcmp(tBuffer.Data, tBuffer.Data + 65, 64) == 0),
		"builder self format result mismatch");
	xrtStrBufFree(&tBuffer);

	xrtClearError();
	testRequire(xrtFormat(NULL) == NULL, "null format must fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "null format error mismatch");
	xrtClearError();
	printf("[PASS] string-format\n");
	return 0;
}
