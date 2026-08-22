#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 把日志记录格式化为由调用方释放的 JSON 文本。 */
int main(void)
{
	xlogjsonconfig Config;
	xlogrecord Record;
	str sJson;
	size_t iSize;

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("service ready");
	if ( !xrtLogJsonConfigInit(&Config) ) {
		return 1;
	}
	Config.Flags = XLOG_JSON_LEVEL | XLOG_JSON_MESSAGE;
	sJson = xrtLogJson(&Record, &Config, &iSize);
	if ( sJson == NULL ) {
		return 2;
	}
	printf("%.*s\n", (int)iSize, sJson);
	xrtFree(sJson);
	return 0;
}
