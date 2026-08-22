#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 把日志记录格式化为由调用方释放的单行文本。 */
int main(void)
{
	xlogtextconfig Config;
	xlogrecord Record;
	str sText;
	size_t iSize;

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("service ready");
	if ( !xrtLogTextConfigInit(&Config, XLOG_TEXT_LEVEL |
		XLOG_TEXT_MESSAGE) ) {
		return 1;
	}
	sText = xrtLogText(&Record, &Config, &iSize);
	if ( sText == NULL ) {
		return 2;
	}
	printf("%.*s", (int)iSize, sText);
	xrtFree(sText);
	return 0;
}
