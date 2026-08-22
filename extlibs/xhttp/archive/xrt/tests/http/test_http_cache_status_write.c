#include "../test.h"

#include <xrt/http_cache_status.h>



/* 建立常用 Cache-Status 成员描述符。 */
static xhttpstructureditemvalue testCacheStatusValue(
	xhttpstructuredparameterentry* pParameters
)
{
	xhttpstructureditemvalue Status;

	memset(&Status, 0, sizeof(Status));
	Status.Bare.Type = XHTTP_STRUCTURED_TOKEN;
	Status.Bare.Data = XRT_STR_LITERAL("ExampleCache");
	Status.Parameters = pParameters;
	Status.ParameterCount = 3u;
	return Status;
}



/* 验证专用写出器复用规范 Structured Item 表示。 */
static void testCacheStatusWrite(void)
{
	xhttpstructuredparameterentry Parameters[3];
	xhttpstructureditemvalue Item;
	xhttpcachestatuscursor Cursor;
	xhttpcachestatus Parsed;
	char arrValue[128];
	size_t iSize;

	memset(Parameters, 0, sizeof(Parameters));
	Parameters[0].Key = XRT_STR_LITERAL("hit");
	Parameters[0].Value.Type = XHTTP_STRUCTURED_BOOLEAN;
	Parameters[0].Value.Number = 1;
	Parameters[1].Key = XRT_STR_LITERAL("ttl");
	Parameters[1].Value.Type = XHTTP_STRUCTURED_INTEGER;
	Parameters[1].Value.Number = -5;
	Parameters[2].Key = XRT_STR_LITERAL("detail");
	Parameters[2].Value.Type = XHTTP_STRUCTURED_STRING;
	Parameters[2].Value.Data = XRT_STR_LITERAL("disk");
	Item = testCacheStatusValue(Parameters);
	testRequire(
		xrtHttpCacheStatusWrite(
			&Item, NULL, 0, &iSize
		) && (iSize == 37u),
		"Cache-Status writer length mismatch"
	);
	testRequire(
		xrtHttpCacheStatusWrite(
			&Item, arrValue, sizeof(arrValue), &iSize
		) && (memcmp(
			arrValue,
			"ExampleCache;hit;ttl=-5;detail=\"disk\"",
			iSize
		) == 0),
		"Cache-Status writer bytes mismatch"
	);
	xrtHttpCacheStatusCursorInit(&Cursor);
	testRequire(
		(xrtHttpCacheStatusNext(
			(xstrview){ arrValue, iSize }, &Cursor, &Parsed
		) == XHTTP_NEXT_ITEM) && (Parsed.Hit == 1u) &&
		(Parsed.Ttl == -5) &&
		(Parsed.Detail.Type == XHTTP_STRUCTURED_STRING),
		"Cache-Status writer round trip mismatch"
	);
}



/* 验证已知参数生产类型和短缓冲失败原子性。 */
static void testCacheStatusWriteInvalid(void)
{
	xhttpstructuredparameterentry Parameters[3];
	xhttpstructureditemvalue Item;
	char arrValue[64];
	size_t iSize;

	memset(Parameters, 0, sizeof(Parameters));
	Parameters[0].Key = XRT_STR_LITERAL("hit");
	Parameters[0].Value.Type = XHTTP_STRUCTURED_BOOLEAN;
	Parameters[0].Value.Number = 1;
	Parameters[1].Key = XRT_STR_LITERAL("ttl");
	Parameters[1].Value.Type = XHTTP_STRUCTURED_INTEGER;
	Parameters[1].Value.Number = 1;
	Parameters[2].Key = XRT_STR_LITERAL("x-extension");
	Parameters[2].Value.Type = XHTTP_STRUCTURED_TOKEN;
	Parameters[2].Value.Data = XRT_STR_LITERAL("ok");
	Item = testCacheStatusValue(Parameters);
	memset(arrValue, 0xA5, sizeof(arrValue));
	testRequire(
		!xrtHttpCacheStatusWrite(
			&Item, arrValue, 2, &iSize
		) && (iSize > 2u) &&
		((uint8)arrValue[0] == 0xA5u),
		"Cache-Status short output was not atomic"
	);
	xrtClearError();
	Parameters[0].Value.Type = XHTTP_STRUCTURED_INTEGER;
	testRequire(
		!xrtHttpCacheStatusWrite(
			&Item, arrValue, sizeof(arrValue), &iSize
		),
		"Cache-Status writer accepted invalid hit type"
	);
	xrtClearError();
	Item.Bare.Type = XHTTP_STRUCTURED_INTEGER;
	testRequire(
		!xrtHttpCacheStatusWrite(
			&Item, arrValue, sizeof(arrValue), &iSize
		),
		"Cache-Status writer accepted invalid identifier type"
	);
	xrtClearError();
	memset(&Parameters[0], 0, sizeof(Parameters[0]));
	Parameters[0].Key = (xstrview){ NULL, 3u };
	Parameters[0].Value.Type = XHTTP_STRUCTURED_BOOLEAN;
	Parameters[0].Value.Number = 1;
	Item.Bare.Type = XHTTP_STRUCTURED_TOKEN;
	Item.Parameters = Parameters;
	Item.ParameterCount = 1u;
	testRequire(
		!xrtHttpCacheStatusWrite(
			&Item, arrValue, sizeof(arrValue), &iSize
		),
		"Cache-Status writer dereferenced an invalid key view"
	);
	xrtClearError();
}



/* 运行 Cache-Status 规范写出测试。 */
int main(void)
{
	testCacheStatusWrite();
	testCacheStatusWriteInvalid();
	printf("[PASS] http_cache_status_write\n");
	return 0;
}
