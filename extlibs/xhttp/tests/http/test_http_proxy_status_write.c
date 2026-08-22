#include "../test.h"

#include <xrt/http_proxy_status.h>



/* 验证 Proxy-Status 单成员规范写出和解析往返。 */
static void testProxyStatusWrite(void)
{
	xhttpstructuredparameterentry Parameters[3];
	xhttpstructureditemvalue Item;
	xhttpproxystatuscursor Cursor;
	xhttpproxystatus Parsed;
	xstrview Expected = XRT_STR_LITERAL(
		"EdgeProxy;error=connection_timeout;"
		"next-protocol=h2;details=\"dial failed\""
	);
	char arrValue[160];
	size_t iSize;

	memset(Parameters, 0, sizeof(Parameters));
	Parameters[0].Key = XRT_STR_LITERAL("error");
	Parameters[0].Value.Type = XHTTP_STRUCTURED_TOKEN;
	Parameters[0].Value.Data = XRT_STR_LITERAL("connection_timeout");
	Parameters[1].Key = XRT_STR_LITERAL("next-protocol");
	Parameters[1].Value.Type = XHTTP_STRUCTURED_TOKEN;
	Parameters[1].Value.Data = XRT_STR_LITERAL("h2");
	Parameters[2].Key = XRT_STR_LITERAL("details");
	Parameters[2].Value.Type = XHTTP_STRUCTURED_STRING;
	Parameters[2].Value.Data = XRT_STR_LITERAL("dial failed");
	memset(&Item, 0, sizeof(Item));
	Item.Bare.Type = XHTTP_STRUCTURED_TOKEN;
	Item.Bare.Data = XRT_STR_LITERAL("EdgeProxy");
	Item.Parameters = Parameters;
	Item.ParameterCount = 3u;
	testRequire(
		xrtHttpProxyStatusWrite(
			&Item, NULL, 0, &iSize
		) && (iSize == Expected.Size),
		"Proxy-Status writer length mismatch"
	);
	testRequire(
		xrtHttpProxyStatusWrite(
			&Item, arrValue, sizeof(arrValue), &iSize
		) && (memcmp(arrValue, Expected.Data, iSize) == 0),
		"Proxy-Status writer bytes mismatch"
	);
	xrtHttpProxyStatusCursorInit(&Cursor);
	testRequire(
		(xrtHttpProxyStatusNext(
			(xstrview){ arrValue, iSize }, &Cursor, &Parsed
		) == XHTTP_NEXT_ITEM) &&
		((Parsed.Flags &
			(XHTTP_PROXY_STATUS_HAS_ERROR |
			 XHTTP_PROXY_STATUS_HAS_NEXT_PROTOCOL |
			 XHTTP_PROXY_STATUS_HAS_DETAILS)) ==
			(XHTTP_PROXY_STATUS_HAS_ERROR |
			 XHTTP_PROXY_STATUS_HAS_NEXT_PROTOCOL |
			 XHTTP_PROXY_STATUS_HAS_DETAILS)),
		"Proxy-Status writer round trip mismatch"
	);
}



/* 验证已知参数生产类型和短缓冲失败原子性。 */
static void testProxyStatusWriteInvalid(void)
{
	char arrLongProtocol[256];
	xhttpstructuredparameterentry Parameter;
	xhttpstructureditemvalue Item;
	char arrValue[64];
	size_t iSize;

	memset(&Parameter, 0, sizeof(Parameter));
	Parameter.Key = XRT_STR_LITERAL("received-status");
	Parameter.Value.Type = XHTTP_STRUCTURED_INTEGER;
	Parameter.Value.Number = 502;
	memset(&Item, 0, sizeof(Item));
	Item.Bare.Type = XHTTP_STRUCTURED_TOKEN;
	Item.Bare.Data = XRT_STR_LITERAL("Proxy");
	Item.Parameters = &Parameter;
	Item.ParameterCount = 1u;
	memset(arrValue, 0xA5, sizeof(arrValue));
	testRequire(
		!xrtHttpProxyStatusWrite(
			&Item, arrValue, 2, &iSize
		) && (iSize > 2u) &&
		((uint8)arrValue[0] == 0xA5u),
		"Proxy-Status short output was not atomic"
	);
	xrtClearError();
	Parameter.Value.Number = 99;
	testRequire(
		!xrtHttpProxyStatusWrite(
			&Item, arrValue, sizeof(arrValue), &iSize
		),
		"Proxy-Status writer accepted invalid status"
	);
	xrtClearError();
	Parameter.Value.Number = 502;
	Item.Bare.Type = XHTTP_STRUCTURED_INTEGER;
	testRequire(
		!xrtHttpProxyStatusWrite(
			&Item, arrValue, sizeof(arrValue), &iSize
		),
		"Proxy-Status writer accepted invalid identifier type"
	);
	xrtClearError();
	Parameter.Key = XRT_STR_LITERAL("next-protocol");
	Parameter.Value.Type = XHTTP_STRUCTURED_BYTES;
	Parameter.Value.Data = XRT_STR_LITERAL("h2");
	Item.Bare.Type = XHTTP_STRUCTURED_TOKEN;
	testRequire(
		!xrtHttpProxyStatusWrite(
			&Item, arrValue, sizeof(arrValue), &iSize
		),
		"Proxy-Status writer accepted noncanonical Byte Sequence ALPN"
	);
	xrtClearError();
	Parameter.Value.Data = XRT_STR_LITERAL("h2 ");
	testRequire(
		xrtHttpProxyStatusWrite(
			&Item, arrValue, sizeof(arrValue), &iSize
		) && (iSize == 26u) &&
		(memcmp(
			arrValue, "Proxy;next-protocol=:aDIg:", iSize
		) == 0),
		"Proxy-Status writer rejected opaque Byte Sequence ALPN"
	);
	memset(arrLongProtocol, 'x', sizeof(arrLongProtocol));
	Parameter.Value.Data = (xstrview){
		arrLongProtocol, sizeof(arrLongProtocol)
	};
	testRequire(
		!xrtHttpProxyStatusWrite(
			&Item, arrValue, sizeof(arrValue), &iSize
		),
		"Proxy-Status writer accepted oversized ALPN"
	);
	xrtClearError();
	memset(&Parameter, 0, sizeof(Parameter));
	Parameter.Key = (xstrview){ NULL, 5u };
	Parameter.Value.Type = XHTTP_STRUCTURED_TOKEN;
	Parameter.Value.Data = XRT_STR_LITERAL("dns_error");
	testRequire(
		!xrtHttpProxyStatusWrite(
			&Item, arrValue, sizeof(arrValue), &iSize
		),
		"Proxy-Status writer dereferenced an invalid key view"
	);
	xrtClearError();
}



/* 运行 Proxy-Status 规范写出测试。 */
int main(void)
{
	testProxyStatusWrite();
	testProxyStatusWriteInvalid();
	printf("[PASS] http_proxy_status_write\n");
	return 0;
}
