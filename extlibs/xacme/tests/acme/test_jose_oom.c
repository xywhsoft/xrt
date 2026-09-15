#include "../test.h"

#include "../../src/internal/xacme_jose.h"

#include <string.h>

/*
	JOSE 组装路径的穷举 OOM 扫描（逻辑故障注入）：JWK 序列化、
	ES256 JWS、EAB HS256 内层 JWS。任一分配点失败必须干净返回
	（MEMORY 类错误 + 调试层零活动分配），全部通过后恢复正常。
	固定密钥 = RFC 6979 A.2.5（确定性，期望值与 test_jose 一致）。
*/

static void testHexToBytes(const char* sHex, uint8* pOut, size_t iCount)
{
	size_t i;
	for(i = 0; i < iCount; i++)
	{
		unsigned v = 0;
		sscanf(sHex + i * 2u, "%2x", &v);
		pOut[i] = (uint8)v;
	}
}

/* 当前无活动逻辑分配并排空隔离队列。 */
static void testJoseNoLive(cstr sMessage)
{
	xmemdebugsnapshot Snapshot;
	xrtMemDebugSnapshot(&Snapshot);
	testRequire(Snapshot.LiveCount == 0u, sMessage);
	testMemoryDebugDrain(sMessage);
}

/* 单构建器扫描：FailAfter 从 0 递增直到分配全部通过。 */
static void testSweepJose(cstr sName, str (*fn)(void))
{
	uint64 i = 0u;
	size_t iCovered = 0u;
	bool bRecovered = false;
	for(;;)
	{
		str s;
		testRequire(xrtMemDebugFailAfter(i), "jose oom arm failed");
		s = fn();
		if(s != NULL)
		{
			xrtFree(s);
			testRequire(
				!xrtMemDebugFailTriggered(),
				"jose oom builder ignored a failure");
			bRecovered = true;
		}
		else
		{
			testRequire(
				xrtMemDebugFailTriggered(),
				"jose oom failure not from injection");
			testRequire(
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY) ||
					(xrtErrorKind(xrtGetError()) == XERR_INTERNAL),
				"jose oom failure kind mismatch");
			iCovered++;
		}
		xrtMemDebugFailClear();
		xrtClearError();
		testJoseNoLive(sName);
		if(bRecovered)
		{
			break;
		}
		i++;
	}
	testRequire(iCovered != 0u, sName);
	printf("[oom] %s points=%zu\n", sName, iCovered);
}

static xacmees256key g_Key;
static uint8 g_Mac[32];

static str testJwkBuilder(void)
{
	return xacmeJwkEcJson(&g_Key);
}

static str testJwsBuilder(void)
{
	xacmejwsheader H;
	H.Nonce = XRT_STR_LITERAL("nonce-oom-probe-0123456789ab");
	H.Url = XRT_STR_LITERAL("https://example.com/acme/new-order");
	H.Kid.Data = NULL;
	H.Kid.Size = 0u;
	return xacmeJwsEs256(
		&g_Key, &H,
		XRT_STR_LITERAL(
			"{\"identifiers\":[{\"type\":\"dns\",\"value\":\"oom\"}]}"));
}

static str testEabBuilder(void)
{
	str sJwk = xacmeJwkEcJson(&g_Key);
	str sResult = NULL;
	if(sJwk != NULL)
	{
		sResult = xacmeJwsEabHs256(
			"eab-oom-kid", "https://example.com/acme/new-account",
			(xstrview){ sJwk, strlen(sJwk) }, g_Mac, sizeof(g_Mac));
	}
	xrtFree(sJwk);
	return sResult;
}

int main(void)
{
	str s;
	testHexToBytes(
		"C9AFA9D845BA75166B5C215767B1D6934E50C3DB36E89B127B8A622B120F6721",
		g_Key.Private, 32u);
	testRequire(
		xacmeEs256FromPrivate(&g_Key), "jose oom key derive failed");
	memset(g_Mac, 0xAB, sizeof(g_Mac));

	testSweepJose("jwk", testJwkBuilder);
	testSweepJose("jws", testJwsBuilder);
	testSweepJose("eab", testEabBuilder);

	/* 恢复正常分配后全部可用。 */
	s = testJwsBuilder();
	testRequire(s != NULL, "jose oom recovery failed");
	xrtFree(s);
	testJoseNoLive("jose oom recovery live");
	printf("[PASS] jose oom sweep\n");
	return 0;
}
