#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 把 JSON Lines 分段直接写到标准输出。 */
static bool exampleLogWrite(xbytesview Data, ptr pUserData)
{
	FILE* pFile = (FILE*)pUserData;

	return fwrite(Data.Data, 1u, Data.Size, pFile) == Data.Size;
}



/* 展示不构造 DOM 的结构化 JSON 日志。 */
int main(void)
{
	xlogjsonconfig Config;
	xlogfield Fields[2];
	xlogrecord Record;

	memset(&Record, 0, sizeof(Record));
	Fields[0] = xrtLogFieldUInt(XRT_STR_LITERAL("request_id"), 42u);
	Fields[1] = xrtLogFieldBool(XRT_STR_LITERAL("cached"), false);
	Record.Time = xrtNow();
	Record.Level = XLOG_INFO;
	Record.Logger = XRT_STR_LITERAL("http");
	Record.Message = XRT_STR_LITERAL("request completed");
	Record.Fields = Fields;
	Record.FieldCount = 2u;
	if (
		!xrtLogJsonConfigInit(&Config) ||
		!xrtLogJsonWrite(
			&Record,
			&Config,
			exampleLogWrite,
			stdout,
			NULL
		)
	) {
		fprintf(stderr, "log JSON failed: %s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}
	return 0;
}
