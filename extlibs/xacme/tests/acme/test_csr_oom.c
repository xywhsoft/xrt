#include "../test.h"

#include "../../src/internal/xacme_csr.h"

#include <string.h>

/*
	CSR/密钥序列化路径的穷举 OOM 扫描（逻辑故障注入）：PKCS#10
	组装、PKCS#8 PEM 写出、PEM 读回。任一分配点失败必须干净返回
	（MEMORY 类错误 + 调试层零活动分配），全部通过后恢复正常。
	固定密钥 = RFC 6979 A.2.5（同 test_csr/test_jose）。
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

static void testCsrNoLive(cstr sMessage)
{
	xmemdebugsnapshot Snapshot;
	xrtMemDebugSnapshot(&Snapshot);
	testRequire(Snapshot.LiveCount == 0u, sMessage);
	testMemoryDebugDrain(sMessage);
}

static void testCsrCheckFailure(cstr sMessage)
{
	testRequire(xrtMemDebugFailTriggered(), sMessage);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) ||
			(xrtErrorKind(xrtGetError()) == XERR_INTERNAL),
		sMessage);
}

int main(void)
{
	static const char* sPem =
		"-----BEGIN PRIVATE KEY-----\n"
		"MIGTAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBHkwdwIBAQQgya+p2EW6dRZrXCFX\n"
		"Z7HWk05Qw9s26JsSe4piKxIPZyGgCgYIKoZIzj0DAQehRANCAARg/tS6JVqdMclh\n"
		"63TGNW1owEm4kjth+mzmaWIuYPKftnkD/hAIuLyZpBrp6VYovGTy8bIMLX6fUXej\n"
		"wpTURiKZ\n"
		"-----END PRIVATE KEY-----\n";
	size_t iPemSize = strlen(sPem);
	xacmees256key Key;
	xacmecsrconfig Csr;
	xstrview Domains[1];
	str s;

	testHexToBytes(
		"C9AFA9D845BA75166B5C215767B1D6934E50C3DB36E89B127B8A622B120F6721",
		Key.Private, 32u);
	testRequire(
		xacmeEs256FromPrivate(&Key), "csr oom key derive failed");
	Domains[0] = XRT_STR_LITERAL("oom.example.com");
	Csr.CommonName = Domains[0];
	Csr.Domains = Domains;
	Csr.DomainCount = 1u;

	/* CSR 组装。 */
	{
		uint64 i = 0u;
		size_t iCovered = 0u;
		bool bRecovered = false;
		for(;;)
		{
			xbuffer Der;
			bool bOk;
			testRequire(xrtMemDebugFailAfter(i), "csr oom arm failed");
			xrtBufferInit(&Der);
			bOk = xacmeCsrEc(&Key, &Csr, &Der);
			xrtBufferUnit(&Der);
			if(bOk)
			{
				testRequire(
					!xrtMemDebugFailTriggered(),
					"csr oom builder ignored a failure");
				bRecovered = true;
			}
			else
			{
				testCsrCheckFailure("csr oom failure mismatch");
				iCovered++;
			}
			xrtMemDebugFailClear();
			xrtClearError();
			testCsrNoLive("csr oom leaked storage");
			if(bRecovered)
			{
				break;
			}
			i++;
		}
		testRequire(iCovered != 0u, "csr oom no points");
		printf("[oom] csr points=%zu\n", iCovered);
	}

	/* PKCS#8 PEM 写出。 */
	{
		uint64 i = 0u;
		size_t iCovered = 0u;
		bool bRecovered = false;
		for(;;)
		{
			testRequire(xrtMemDebugFailAfter(i), "pem write arm failed");
			s = xacmeKeyPemWrite(&Key);
			if(s != NULL)
			{
				xrtFree(s);
				testRequire(
					!xrtMemDebugFailTriggered(),
					"pem write ignored a failure");
				bRecovered = true;
			}
			else
			{
				testCsrCheckFailure("pem write failure mismatch");
				iCovered++;
			}
			xrtMemDebugFailClear();
			xrtClearError();
			testCsrNoLive("pem write leaked storage");
			if(bRecovered)
			{
				break;
			}
			i++;
		}
		testRequire(iCovered != 0u, "pem write no points");
		printf("[oom] pem write points=%zu\n", iCovered);
	}

	/* PEM 读回。 */
	{
		uint64 i = 0u;
		size_t iCovered = 0u;
		bool bRecovered = false;
		xacmees256key Parsed;
		for(;;)
		{
			testRequire(xrtMemDebugFailAfter(i), "pem read arm failed");
			if(xacmeKeyPemRead(sPem, iPemSize, &Parsed))
			{
				testRequire(
					!xrtMemDebugFailTriggered(),
					"pem read ignored a failure");
				bRecovered = true;
			}
			else
			{
				testCsrCheckFailure("pem read failure mismatch");
				iCovered++;
			}
			xrtMemDebugFailClear();
			xrtClearError();
			testCsrNoLive("pem read leaked storage");
			if(bRecovered)
			{
				break;
			}
			i++;
		}
		testRequire(iCovered != 0u, "pem read no points");
		printf("[oom] pem read points=%zu\n", iCovered);
	}

	/* 恢复正常分配后全部可用。 */
	{
		xbuffer Der;
		bool bOk;
		xrtBufferInit(&Der);
		bOk = xacmeCsrEc(&Key, &Csr, &Der);
		xrtBufferUnit(&Der);
		s = bOk ? xacmeKeyPemWrite(&Key) : NULL;
		testRequire(
			(bOk && (s != NULL)),
			"csr oom recovery failed");
		xrtFree(s);
	}
	testCsrNoLive("csr oom recovery live");
	printf("[PASS] csr oom sweep\n");
	return 0;
}
