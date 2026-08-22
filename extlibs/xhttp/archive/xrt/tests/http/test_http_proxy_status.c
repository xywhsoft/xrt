#include "../test.h"

#include <xrt/http_proxy_status.h>



/* 按字节比较借用视图。 */
static bool testProxyStatusViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证 RFC 9209 与 RFC 9532 已知参数。 */
static void testProxyStatusKnown(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"revproxy1.example.net;next-hop=\"origin.example\";"
		"next-protocol=h2;received-status=200, "
		"\"Example CDN\";error=connection_timeout;"
		"details=\"dial failed\";"
		"next-hop-aliases=\"alias.example,origin.example\""
	);
	xhttpproxystatuscursor Cursor;
	xhttpproxystatus Status;

	testRequire(
		xrtHttpProxyStatusValid(Value),
		"valid Proxy-Status was rejected"
	);
	xrtHttpProxyStatusCursorInit(&Cursor);
	testRequire(
		(xrtHttpProxyStatusNext(
			Value, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		(Status.Proxy.Type == XHTTP_STRUCTURED_TOKEN) &&
		testProxyStatusViewEqual(
			Status.Proxy.Encoded,
			XRT_STR_LITERAL("revproxy1.example.net")
		) && (Status.NextHop.Type == XHTTP_STRUCTURED_STRING) &&
		(Status.NextProtocol.Type == XHTTP_STRUCTURED_TOKEN) &&
		(Status.ReceivedStatus == 200) &&
		(Status.InvalidFlags == 0),
		"first Proxy-Status member mismatch"
	);
	testRequire(
		(xrtHttpProxyStatusNext(
			Value, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		(Status.Proxy.Type == XHTTP_STRUCTURED_STRING) &&
		testProxyStatusViewEqual(
			Status.Error.Encoded,
			XRT_STR_LITERAL("connection_timeout")
		) && (Status.Details.Type == XHTTP_STRUCTURED_STRING) &&
		(Status.NextHopAliases.Type == XHTTP_STRUCTURED_STRING) &&
		((Status.Flags &
			(XHTTP_PROXY_STATUS_HAS_ERROR |
			 XHTTP_PROXY_STATUS_HAS_DETAILS |
			 XHTTP_PROXY_STATUS_HAS_NEXT_HOP_ALIASES)) ==
			(XHTTP_PROXY_STATUS_HAS_ERROR |
			 XHTTP_PROXY_STATUS_HAS_DETAILS |
			 XHTTP_PROXY_STATUS_HAS_NEXT_HOP_ALIASES)),
		"second Proxy-Status member mismatch"
	);
	testRequire(
		xrtHttpProxyStatusNext(
			Value, &Cursor, &Status
		) == XHTTP_NEXT_END,
		"Proxy-Status iterator did not end"
	);
}



/* 验证 next-protocol 的 Byte Sequence 线路形式和规范类型。 */
static void testProxyStatusProtocolBytes(void)
{
	xhttpproxystatuscursor Cursor;
	xhttpproxystatus Status;
	char arrProtocol[8];
	size_t iSize;

	xrtHttpProxyStatusCursorInit(&Cursor);
	testRequire(
		(xrtHttpProxyStatusNext(
			XRT_STR_LITERAL("Proxy;next-protocol=:aDIg:"),
			&Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		(Status.NextProtocol.Type == XHTTP_STRUCTURED_BYTES) &&
		xrtHttpStructuredBytesDecode(
			&Status.NextProtocol,
			arrProtocol, sizeof(arrProtocol), &iSize
		) && (iSize == 3u) &&
		(memcmp(arrProtocol, "h2 ", 3u) == 0),
		"Proxy-Status Byte Sequence protocol mismatch"
	);
	xrtHttpProxyStatusCursorInit(&Cursor);
	testRequire(
		(xrtHttpProxyStatusNext(
			XRT_STR_LITERAL("Proxy;next-protocol=:aDI=:"),
			&Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		((Status.Flags &
		  XHTTP_PROXY_STATUS_HAS_NEXT_PROTOCOL) == 0) &&
		((Status.InvalidFlags &
		  XHTTP_PROXY_STATUS_HAS_NEXT_PROTOCOL) != 0),
		"Proxy-Status accepted noncanonical Byte Sequence ALPN"
	);
}



/* 验证已知参数类型和范围错误通过 InvalidFlags 暴露。 */
static void testProxyStatusInvalidParameters(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"Proxy;error=\"bad\";next-hop=1;next-protocol=\"h2\";"
		"received-status=99;details=token;next-hop-aliases=token"
	);
	xhttpproxystatuscursor Cursor;
	xhttpproxystatus Status;

	xrtHttpProxyStatusCursorInit(&Cursor);
	testRequire(
		(xrtHttpProxyStatusNext(
			Value, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) && (Status.Flags == 0) &&
		(Status.InvalidFlags ==
			(XHTTP_PROXY_STATUS_HAS_ERROR |
			 XHTTP_PROXY_STATUS_HAS_NEXT_HOP |
			 XHTTP_PROXY_STATUS_HAS_NEXT_PROTOCOL |
			 XHTTP_PROXY_STATUS_HAS_RECEIVED_STATUS |
			 XHTTP_PROXY_STATUS_HAS_DETAILS |
			 XHTTP_PROXY_STATUS_HAS_NEXT_HOP_ALIASES)),
		"Proxy-Status invalid parameter flags mismatch"
	);
}



/* 验证重复参数由最后值决定。 */
static void testProxyStatusDuplicates(void)
{
	xhttpproxystatuscursor Cursor;
	xhttpproxystatus Status;

	xrtHttpProxyStatusCursorInit(&Cursor);
	testRequire(
		(xrtHttpProxyStatusNext(
			XRT_STR_LITERAL(
				"Proxy;error=\"bad\";error=dns_timeout;"
				"received-status=99;received-status=504"
			),
			&Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		testProxyStatusViewEqual(
			Status.Error.Encoded, XRT_STR_LITERAL("dns_timeout")
		) && (Status.ReceivedStatus == 504) &&
		(Status.InvalidFlags == 0),
		"Proxy-Status duplicate final value mismatch"
	);
}



/* 验证重复字段行保持中间节点顺序。 */
static void testProxyStatusFields(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Proxy-Status"), XRT_STR_INIT("OriginProxy") },
		{ XRT_STR_INIT("Other"), XRT_STR_INIT("ignored") },
		{ XRT_STR_INIT("proxy-status"), XRT_STR_INIT("EdgeProxy;error=dns_error") }
	};
	xhttpproxystatusfieldcursor Cursor;
	xhttpproxystatus Status;

	xrtHttpProxyStatusFieldCursorInit(&Cursor);
	testRequire(
		(xrtHttpProxyStatusFieldNext(
			Fields, 3, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		testProxyStatusViewEqual(
			Status.Proxy.Encoded, XRT_STR_LITERAL("OriginProxy")
		),
		"first repeated Proxy-Status field mismatch"
	);
	testRequire(
		(xrtHttpProxyStatusFieldNext(
			Fields, 3, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		testProxyStatusViewEqual(
			Status.Proxy.Encoded, XRT_STR_LITERAL("EdgeProxy")
		) && (xrtHttpProxyStatusFieldNext(
			Fields, 3, &Cursor, &Status
		) == XHTTP_NEXT_END),
		"repeated Proxy-Status field order mismatch"
	);
}



/* 验证完整预校验和失败原子性。 */
static void testProxyStatusFailure(void)
{
	xhttpproxystatuscursor Cursor;
	xhttpproxystatuscursor Saved;
	xhttpproxystatus Status;
	xhttpproxystatus SavedStatus;

	xrtHttpProxyStatusCursorInit(&Cursor);
	Saved = Cursor;
	memset(&Status, 0xA5, sizeof(Status));
	SavedStatus = Status;
	testRequire(
		xrtHttpProxyStatusNext(
			XRT_STR_LITERAL("Good, (bad member)"),
			&Cursor, &Status
		) == XHTTP_NEXT_ERROR,
		"Proxy-Status accepted invalid later member"
	);
	testRequire(
		(memcmp(&Cursor, &Saved, sizeof(Saved)) == 0) &&
		(memcmp(&Status, &SavedStatus, sizeof(Status)) == 0),
		"Proxy-Status failure was not atomic"
	);
	xrtClearError();
}



/* 验证单值游标不能跨等长字段值复用。 */
static void testProxyStatusCursorBinding(void)
{
	char arrFirst[] = "One, Two";
	char arrOther[] = "Six, Ten";
	xhttpproxystatuscursor Cursor;
	xhttpproxystatuscursor SavedCursor;
	xhttpproxystatus Status;
	xhttpproxystatus SavedStatus;

	xrtHttpProxyStatusCursorInit(&Cursor);
	testRequire(
		xrtHttpProxyStatusNext(
			(xstrview){ arrFirst, sizeof(arrFirst) - 1u },
			&Cursor, &Status
		) == XHTTP_NEXT_ITEM,
		"Proxy-Status source binding setup failed"
	);
	SavedCursor = Cursor;
	SavedStatus = Status;
	testRequire(
		xrtHttpProxyStatusNext(
			(xstrview){ arrOther, sizeof(arrOther) - 1u },
			&Cursor, &Status
		) == XHTTP_NEXT_ERROR,
		"Proxy-Status cursor accepted another equal-size value"
	);
	testRequire(
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Status, &SavedStatus, sizeof(Status)) == 0),
		"Proxy-Status source mismatch was not atomic"
	);
	xrtClearError();
	testRequire(
		(xrtHttpProxyStatusNext(
			(xstrview){ arrFirst, sizeof(arrFirst) - 1u },
			&Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		testProxyStatusViewEqual(
			Status.Proxy.Encoded, XRT_STR_LITERAL("Two")
		),
		"Proxy-Status cursor could not resume its source"
	);
}



/* 运行 RFC 9209 Proxy-Status 解析测试。 */
int main(void)
{
	testProxyStatusKnown();
	testProxyStatusProtocolBytes();
	testProxyStatusInvalidParameters();
	testProxyStatusDuplicates();
	testProxyStatusFields();
	testProxyStatusFailure();
	testProxyStatusCursorBinding();
	printf("[PASS] http_proxy_status\n");
	return 0;
}
