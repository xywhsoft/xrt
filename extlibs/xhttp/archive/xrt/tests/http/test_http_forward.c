#include "../test.h"

#include <xrt/http_forward.h>



/* 验证固定逐跳集合与 Connection 提名字段。 */
static void testHttpHopFields(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Connection"),
			XRT_STR_INIT("close, X-Hop")
		},
		{
			XRT_STR_INIT("X-Hop"),
			XRT_STR_INIT("private")
		}
	};
	static const xhttpfield Invalid = {
		XRT_STR_INIT("Connection"),
		XRT_STR_INIT("(")
	};

	testRequire(
		xrtHttpHopField(
			Fields, 2u, XRT_STR_LITERAL("X-Hop")
		) == XHTTP_NEXT_ITEM,
		"Connection-nominated field was not hop-by-hop"
	);
	testRequire(
		xrtHttpHopField(
			Fields, 2u, XRT_STR_LITERAL("Content-Type")
		) == XHTTP_NEXT_END,
		"end-to-end field was classified as hop-by-hop"
	);
	testRequire(
		xrtHttpHopFieldKnown(
			XRT_STR_LITERAL("Transfer-Encoding")
		) && xrtHttpHopFieldKnown(
			XRT_STR_LITERAL("proxy-connection")
		) && !xrtHttpHopFieldKnown(
			XRT_STR_LITERAL("Trailer")
		),
		"fixed hop-by-hop field classification mismatch"
	);
	testRequire(
		xrtHttpHopField(
			&Invalid, 1u, XRT_STR_LITERAL("X-Hop")
		) == XHTTP_NEXT_ERROR,
		"hop-by-hop lookup accepted an invalid Connection field"
	);
	xrtClearError();
}



/* 验证 Max-Forwards 的边界、更新规则和规范写出。 */
static void testHttpMaxForwards(void)
{
	char sOutput[32];
	uint64 iValue;
	size_t iSize;

	testRequire(
		xrtHttpMaxForwardsParse(
			XRT_STR_LITERAL("18446744073709551615"), &iValue
		) && (iValue == UINT64_MAX),
		"Max-Forwards uint64 boundary parse failed"
	);
	testRequire(
		!xrtHttpMaxForwardsParse(
			XRT_STR_LITERAL("18446744073709551616"), &iValue
		) && (iValue == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"Max-Forwards overflow was not rejected"
	);
	xrtClearError();
	testRequire(
		(xrtHttpMaxForwardsUpdate(
			XRT_STR_LITERAL("0"), 8u, &iValue
		) == XHTTP_FORWARD_FINAL) && (iValue == 0),
		"zero Max-Forwards did not stop forwarding"
	);
	testRequire(
		(xrtHttpMaxForwardsUpdate(
			XRT_STR_LITERAL("9"), 3u, &iValue
		) == XHTTP_FORWARD_NEXT) && (iValue == 3u),
		"Max-Forwards maximum clamp mismatch"
	);
	testRequire(
		xrtHttpMaxForwardsWrite(
			UINT64_MAX, sOutput, sizeof(sOutput), &iSize
		) && (iSize == 20u) &&
		(memcmp(sOutput, "18446744073709551615", 20u) == 0),
		"Max-Forwards writer mismatch"
	);
}



/* 验证写出缓冲与长度输出不能互相重叠。 */
static void testHttpForwardMemory(void)
{
	char sOutput[24];
	size_t* pAlias = (size_t*)(void*)sOutput;

	memset(sOutput, 0xA5, sizeof(sOutput));
	testRequire(
		!xrtHttpMaxForwardsWrite(
			5u, sOutput, sizeof(sOutput), pAlias
		),
		"Max-Forwards writer accepted overlapping size output"
	);
	xrtClearError();
}



int main(void)
{
	testHttpHopFields();
	testHttpMaxForwards();
	testHttpForwardMemory();
	printf("[PASS] http_forward\n");
	return 0;
}
