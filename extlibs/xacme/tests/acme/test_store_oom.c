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
	testRequire(xrtMemDebugFailTriggered(), sMessage);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, sMessage);
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

static bool testSaveCertRun(void)
{
	return xrtAcmeStoreSaveCert(g_sRoot, "legacy-oom.example.com",
		g_sChain, "https://acme-oom.example/dir");
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

static void testSweepStore(cstr sName, bool (*fn)(void),
	cstr sKeyBeforeCommit)
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
			printf("[diagnostic] STORE_OOM %s point=%llu kind=%u triggered=%u\n",
				sName, (unsigned long long)i, (unsigned)xrtErrorKind(xrtGetError()),
				(unsigned)xrtMemDebugFailTriggered());
			testStoreCheckFailure("store oom failure mismatch");
			iCovered++;
		}
		xrtMemDebugFailClear();
		xrtClearError();
		if(!bRecovered && sKeyBeforeCommit != NULL)
		{
			xacmeissuegrant Current;
			testRequire(xrtAcmeStoreLoadGrant(
				g_sRoot, "oom.example.com", &Current) &&
				strcmp(Current.sKeyPem, sKeyBeforeCommit) == 0,
				"store oom published a partial grant");
			xrtAcmeGrantUnit(&Current);
		}
		testStoreNoLive("store oom leaked storage");
		if(bRecovered)
		{
			break;
		}
		i++;
		testRequire(i < UINT64_C(1024), "store oom sweep exceeded the allocation bound");
	}
	testRequire(iCovered != 0u, "store oom no points");
	printf("[oom] %s points=%zu\n", sName, iCovered);
}

int main(int argc, char** argv)
{
	static char sRoot[340];
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

	testRequire(argc == 1 || argc == 2, "store oom usage: [account-save|cert-save|grant-save|grant-load|need-renew|list-domains]");
	if ( argc == 1 || strcmp(argv[1], "account-save") == 0 )
		testSweepStore("account-save", testSaveAccountRun, NULL);
	if ( argc == 1 || strcmp(argv[1], "cert-save") == 0 )
		testSweepStore("cert-save", testSaveCertRun, NULL);
	g_sKey = "-----BEGIN PRIVATE KEY-----\nreplacement\n-----END PRIVATE KEY-----\n";
	if ( argc == 1 || strcmp(argv[1], "grant-save") == 0 )
		testSweepStore("grant-save", testSaveGrantRun, sPem);
	if ( argc == 1 || strcmp(argv[1], "grant-load") == 0 )
		testSweepStore("grant-load", testLoadGrantRun, NULL);
	if ( argc == 1 || strcmp(argv[1], "need-renew") == 0 )
		testSweepStore("need-renew", testNeedRenewRun, NULL);
	if ( argc == 1 || strcmp(argv[1], "list-domains") == 0 )
		testSweepStore("list-domains", testListRun, NULL);
	if ( argc == 2 ) testRequire(strcmp(argv[1], "account-save") == 0 || strcmp(argv[1], "cert-save") == 0 ||
		strcmp(argv[1], "grant-save") == 0 || strcmp(argv[1], "grant-load") == 0 ||
		strcmp(argv[1], "need-renew") == 0 || strcmp(argv[1], "list-domains") == 0, "unknown store oom operation");

	/* 恢复正常分配后读侧完整可用。 */
	testRequire(testLoadGrantRun(), "store oom recovery load failed");
	testStoreNoLive("store oom recovery live");
	printf("[PASS] store oom sweep\n");
	return 0;
}
