#include "../test.h"

#include <xrt/acme_store.h>

#include <stdio.h>
#include <string.h>

/*
	store 路径的穷举 OOM 扫描（逻辑故障注入）：账户存取、签发产物
	整体存取、续签判定（含 x509 解析侧临时分配）与域名枚举。
	任一分配点失败必须干净返回（MEMORY 类错误 + 调试层零活动分配）。
	链夹具为真实自签证书（10 年有效期，NeedRenew 成功路径可信）。
*/

static void testStoreNoLive(cstr sMessage)
{
	xmemdebugsnapshot Snapshot;
	xrtMemDebugSnapshot(&Snapshot);
	testRequire(Snapshot.LiveCount == 0u, sMessage);
	testMemoryDebugDrain(sMessage);
}

static void testStoreCheckFailure(cstr sMessage)
{
	/* store 各入口按语义把底层失败包装为 IO/NOT_FOUND/MEMORY 等类，
	   OOM 保证的核心是：注入命中、有错误、且无泄漏——类别不作限定。 */
	testRequire(xrtMemDebugFailTriggered(), sMessage);
	testRequire(xrtGetError() != NULL, sMessage);
}

static const char* g_sRoot;
static const char* g_sKey;
static const char* g_sChain;

static bool testSaveAccountRun(void)
{
	return xrtAcmeStoreSaveAccount(
		g_sRoot, "https://acme-oom.example/dir", g_sKey);
}

static bool testSaveGrantRun(void)
{
	xacmeissuegrant Grant;
	Grant.sFullchainPem = (str)g_sChain;
	Grant.sKeyPem = (str)g_sKey;
	return xrtAcmeStoreSaveGrant(
		g_sRoot, "oom.example.com", &Grant,
		"https://acme-oom.example/dir");
}

static bool testLoadGrantRun(void)
{
	xacmeissuegrant Grant;
	if(!xrtAcmeStoreLoadGrant(g_sRoot, "oom.example.com", &Grant))
	{
		return false;
	}
	xrtAcmeGrantUnit(&Grant);
	return true;
}

static bool testNeedRenewRun(void)
{
	bool bNeed = false;
	return xrtAcmeStoreNeedRenew(
		g_sRoot, "oom.example.com", 30, &bNeed);
}

static bool testListRun(void)
{
	char sDomains[4][256];
	size_t iCount = 0u;
	return xrtAcmeStoreListDomains(g_sRoot, sDomains, 4u, &iCount);
}

static void testSweepStore(cstr sName, bool (*fn)(void))
{
	uint64 i = 0u;
	size_t iCovered = 0u;
	bool bRecovered = false;
	for(;;)
	{
		testRequire(xrtMemDebugFailAfter(i), "store oom arm failed");
		if(fn())
		{
			testRequire(
				!xrtMemDebugFailTriggered(),
				"store oom op ignored a failure");
			bRecovered = true;
		}
		else
		{
			testStoreCheckFailure("store oom failure mismatch");
			iCovered++;
		}
		xrtMemDebugFailClear();
		xrtClearError();
		testStoreNoLive("store oom leaked storage");
		if(bRecovered)
		{
			break;
		}
		i++;
	}
	testRequire(iCovered != 0u, "store oom no points");
	printf("[oom] %s points=%zu\n", sName, iCovered);
}

int main(void)
{
	static char sRoot[300];
	const char* sPem =
		"-----BEGIN PRIVATE KEY-----\n"
		"MIGTAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBHkwdwIBAQQgya+p2EW6dRZrXCFX\n"
		"Z7HWk05Qw9s26JsSe4piKxIPZyGgCgYIKoZIzj0DAQehRANCAARg/tS6JVqdMclh\n"
		"63TGNW1owEm4kjth+mzmaWIuYPKftnkD/hAIuLyZpBrp6VYovGTy8bIMLX6fUXej\n"
		"wpTURiKZ\n"
		"-----END PRIVATE KEY-----\n";
	const char* sChainPem =
		"-----BEGIN CERTIFICATE-----\n"
		"MIIBMjCB2qADAgECAhRnpbshR8PyZx9UmgZPO1G/wqCRnTAKBggqhkjOPQQDAjAa\n"
		"MRgwFgYDVQQDDA9vb20uZXhhbXBsZS5jb20wHhcNMjYwOTE0MTYwNTU1WhcNMzYw\n"
		"OTEyMTYwNTU1WjAaMRgwFgYDVQQDDA9vb20uZXhhbXBsZS5jb20wWTATBgcqhkjO\n"
		"PQIBBggqhkjOPQMBBwNCAASilhafWwwnrhHr4I45XpStCvTFfCy8XMHub/KZkCcm\n"
		"JmlaEs5JB4D7nF3lrYvrB0WdwPiMlCkvMYMEyeCWYMpOMAoGCCqGSM49BAMCA0cA\n"
		"MEQCIElwyPU8lRTGngaM+FyFmq2uvGS13DCNr5+QYNSYW/MEAiAP9NGUx4LOeWJ0\n"
		"J3cQToQ4hsqs0MI84eXjLzxhEW8Bhg==\n"
		"-----END CERTIFICATE-----\n";

	snprintf(sRoot, sizeof(sRoot), "%s/store_oom", testOutRoot());
	g_sRoot = sRoot;
	g_sKey = sPem;
	g_sChain = sChainPem;

	/* 基线：正常分配下先铺一份完整 store 供读侧扫描。 */
	testRequire(
		xrtAcmeStoreSaveAccount(
			sRoot, "https://acme-oom.example/dir", sPem),
		"store oom baseline account failed");
	testRequire(testSaveGrantRun(), "store oom baseline grant failed");
	testStoreNoLive("store oom baseline live");

	testSweepStore("account save", testSaveAccountRun);
	testSweepStore("grant save", testSaveGrantRun);
	testSweepStore("grant load", testLoadGrantRun);
	testSweepStore("need renew", testNeedRenewRun);
	testSweepStore("list domains", testListRun);

	/* 恢复正常分配后读侧完整可用。 */
	testRequire(testLoadGrantRun(), "store oom recovery load failed");
	testStoreNoLive("store oom recovery live");
	printf("[PASS] store oom sweep\n");
	return 0;
}
