#include "../test.h"



/* 验证分配式文本 Helper 返回显式长度和末尾零字节。 */
int main(void)
{
	xlogtextconfig Config;
	xlogfield Field;
	xlogrecord Record;
	str sText;
	size_t iSize;

	memset(&Record, 0, sizeof(Record));
	Field = xrtLogFieldBool(XRT_STR_LITERAL("ok"), true);
	Record.Level = XLOG_INFO;
	Record.Logger = XRT_STR_LITERAL("buffer");
	Record.Message = XRT_STR_LITERAL("ready");
	Record.Fields = &Field;
	Record.FieldCount = 1u;
	testRequire(
		xrtLogTextConfigInit(&Config, XLOG_TEXT_SIMPLE),
		"buffer text config failed"
	);
	sText = xrtLogText(&Record, &Config, &iSize);
	testRequire(
		(sText != NULL) &&
		(iSize == sizeof("INFO buffer - ready ok=true\n") - 1u) &&
		(memcmp(sText, "INFO buffer - ready ok=true\n", iSize) == 0) &&
		(sText[iSize] == 0),
		"allocated log text mismatch"
	);
	xrtFree(sText);
	printf("[PASS] Logger text buffer\n");
	return 0;
}
