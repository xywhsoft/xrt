#include "../test.h"



/* 验证分配式 JSON Helper 返回显式长度和末尾零字节。 */
int main(void)
{
	xlogjsonconfig Config;
	xlogfield Field;
	xlogrecord Record;
	str sJson;
	size_t iSize;

	memset(&Record, 0, sizeof(Record));
	Field = xrtLogFieldBool(XRT_STR_LITERAL("ok"), true);
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("ready");
	Record.Fields = &Field;
	Record.FieldCount = 1u;
	testRequire(
		xrtLogJsonConfigInit(&Config),
		"buffer JSON config failed"
	);
	Config.Flags = XLOG_JSON_MESSAGE | XLOG_JSON_FIELDS | XLOG_JSON_NEWLINE;
	sJson = xrtLogJson(&Record, &Config, &iSize);
	testRequire(
		(sJson != NULL) &&
		(iSize == sizeof("{\"message\":\"ready\",\"fields\":{\"ok\":true}}\n") - 1u) &&
		(memcmp(
			sJson,
			"{\"message\":\"ready\",\"fields\":{\"ok\":true}}\n",
			iSize
		) == 0) &&
		(sJson[iSize] == 0),
		"allocated log JSON mismatch"
	);
	xrtFree(sJson);
	printf("[PASS] Logger JSON buffer\n");
	return 0;
}
