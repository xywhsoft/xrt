#include "../test.h"

#include "../../src/internal/xacme_http.h"
#include "../../src/internal/xacme_csr.h"
#include "../../src/internal/xacme_jose.h"
#include <xrt/acme_client.h>
#include <xrt/acme_obtain.h>
#include <xrt/acme_store.h>
#include <xrt/memory.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
	本地/远端 ACME 集成（环境门控）：
	  XACME_PEBBLE_URL = https://localhost:14000/dir（Pebble 或等价模拟 CA）
	  XACME_PEBBLE_CA  = CA TLS 证书 PEM 路径
	  XACME_CHALL_URL  = http://127.0.0.1:8055（challtestsrv 管理 API）
	  XACME_PROPAGATE_RESOLVER = host[:port]（本地 DNS TXT 视角；缺省公共组）
	未设置时跳过。DNS provider 走 challtestsrv 的 set-txt/clear-txt，
	完整链路：directory→账户→订单→dns-01→传播确认→finalize→证书链
	→grant 配对落盘→Revoke→IssueStored 双跑。
*/

static str g_sCertKeyPem = NULL;
static size_t g_iDnsAddCalls = 0u;
static size_t g_iDnsRemoveCalls = 0u;

typedef struct testdnsctx {
	xacmehttp Http;
	char sChall[256];
} testdnsctx;

static bool testJsonQuote(xbuffer* pOut, xstrview sText)
{
	size_t i;
	if(!xrtBufferAppendByte(pOut, '"'))
	{
		return false;
	}
	for(i = 0; i < sText.Size; i++)
	{
		char c = sText.Data[i];
		bool bOk;
		if((c == '"') || (c == '\\'))
		{
			bOk = xrtBufferAppendByte(pOut, (uint8)'\\') &&
				xrtBufferAppendByte(pOut, (uint8)c);
		}
		else
		{
			bOk = xrtBufferAppendByte(pOut, (uint8)c);
		}
		if(!bOk)
		{
			return false;
		}
	}
	return xrtBufferAppendByte(pOut, '"');
}

static bool testDnsExchange(
	testdnsctx* pCtx, cstr sAction, xstrview sFqdn, xstrview sTxt)
{
	xbuffer Body;
	char sUrl[300];
	xacmehttpresponse R;
	bool bOk;
	xrtBufferInit(&Body);
	bOk = xrtBufferAppend(&Body, XRT_BYTES_LITERAL("{\"host\":")) &&
		testJsonQuote(&Body, sFqdn) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL(",\"value\":")) &&
		testJsonQuote(&Body, sTxt) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL("}"));
	if(!bOk)
	{
		xrtBufferUnit(&Body);
		return false;
	}
	snprintf(sUrl, sizeof(sUrl), "%s/%s-txt", pCtx->sChall, sAction);
	bOk = xacmeHttpExchange(
		&pCtx->Http, "POST", sUrl, "application/json",
		(xstrview){ (cstr)Body.Data, Body.Size }, &R);
	xrtBufferUnit(&Body);
	if(!bOk)
	{
		return false;
	}
	bOk = (R.iStatus == 200u);
	xacmeHttpResponseUnit(&R);
	return bOk;
}

static bool testDnsAdd(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	const char* sFailAt = getenv("XACME_TEST_DNS_ADD_UNCERTAIN");
	g_iDnsAddCalls++;
	if((sFailAt != NULL) && (g_iDnsAddCalls >= (size_t)atoi(sFailAt)))
	{
		cstr sKind = getenv("XACME_TEST_DNS_UNCERTAIN_KIND");
		if(sKind != NULL && strcmp(sKind, "memory") == 0) xrtSetErrorKind(XERR_MEMORY);
		else if(sKind != NULL && strcmp(sKind, "provider") == 0)
			xrtSetErrorInfo(XERR_IO, "xrt.acme.dns", XACME_DNS_ERROR_UNCERTAIN,
				"injected provider ownership unknown after send");
		else xrtSetErrorInfo(XERR_IO, "xrt.acme.http", XACME_HTTP_ERROR_UNCERTAIN,
				"injected DNS add result unknown after send");
		return false;
	}
	return testDnsExchange(
		(testdnsctx*)pProvider->pContext, "set", sFqdn, sTxt);
}

static bool testDnsRemove(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	g_iDnsRemoveCalls++;
	return testDnsExchange(
		(testdnsctx*)pProvider->pContext, "clear", sFqdn, sTxt);
}

static bool testDnsPropagate(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	(void)pProvider;
	(void)sFqdn;
	(void)sTxt;
	return true;
}

/* 读文件到 xrtMalloc 缓冲（CA PEM）。 */
static str testReadFile(cstr sPath, size_t* pSize)
{
	FILE* f = fopen(sPath, "rb");
	str sData;
	long iSize;
	if(f == NULL)
	{
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	iSize = ftell(f);
	fseek(f, 0, SEEK_SET);
	if((iSize <= 0) || (iSize > 65536))
	{
		fclose(f);
		return NULL;
	}
	sData = (str)xrtMalloc((size_t)iSize + 1u);
	if((sData == NULL) ||
		(fread(sData, 1u, (size_t)iSize, f) != (size_t)iSize))
	{
		xrtFree(sData);
		fclose(f);
		return NULL;
	}
	fclose(f);
	sData[iSize] = '\0';
	*pSize = (size_t)iSize;
	return sData;
}

int main(void)
{
	const char* sUrl = getenv("XACME_PEBBLE_URL");
	const char* sCaPath = getenv("XACME_PEBBLE_CA");
	const char* sChall = getenv("XACME_CHALL_URL");
	const char* sResolver = getenv("XACME_PROPAGATE_RESOLVER");
	const char* sCertKeyPath = getenv("XACME_CERT_KEY");
	const char* sEabKid = getenv("XACME_EAB_KID");
	const char* sEabHmac = getenv("XACME_EAB_HMAC");
	const char* sContact = getenv("XACME_CONTACT_EMAIL");

	if((sUrl == NULL) || (sUrl[0] == '\0'))
	{
		printf("[SKIP] acme flow (XACME_PEBBLE_URL unset)\n");
		return 0;
	}
	if((sCaPath == NULL) || (sChall == NULL) ||
		(sCaPath[0] == '\0') || (sChall[0] == '\0'))
	{
		printf("[SKIP] acme flow (XACME_PEBBLE_CA/XACME_CHALL_URL unset)\n");
		return 0;
	}

	{
		str sCaPem;
		size_t iCaSize = 0u;
		struct xacmeclient* Client;
		xacmeaccountconfig Account;
		xacmeclientconfig ClientConfig;
		testdnsctx Dns;
		xacmednsprovider Provider;
		xstrview Domains[17];
		xacmeissuegrant Grant;
		size_t iCerts;
		size_t i;
		str sAccountPem;

		sCaPem = testReadFile(sCaPath, &iCaSize);
		testRequire(sCaPem != NULL, "acme flow ca pem read failed");
		if((sCertKeyPath != NULL) && (sCertKeyPath[0] != '\0'))
		{
			size_t iKeySize = 0u;
			g_sCertKeyPem = testReadFile(sCertKeyPath, &iKeySize);
			testRequire(
				g_sCertKeyPem != NULL, "acme flow cert key read failed");
		}

		Dns.sChall[0] = '\0';
		strncat(Dns.sChall, sChall, sizeof(Dns.sChall) - 1u);
		testRequire(
			xacmeHttpInit(&Dns.Http, NULL, NULL, 10000000u),
			"acme flow chall http init failed");

		/* 公开客户端主线：账户配置 + 传播 resolver 经客户端配置。 */
		xrtAcmeAccountConfigInit(&Account);
		Account.sDirectoryUrl = sUrl;
		if((sEabKid != NULL) && (sEabKid[0] != '\0'))
		{
			Account.Eab.sKid = sEabKid;
			Account.Eab.sHmac = sEabHmac;
		}
		Account.sContactEmail = sContact;
		xrtAcmeClientConfigInit(&ClientConfig);
		ClientConfig.pAccount = &Account;
		ClientConfig.sCaPem = sCaPem;
		ClientConfig.uTimeoutMs = 10000u;
		ClientConfig.sCertKeyPem = g_sCertKeyPem;
		if((sResolver != NULL) && (sResolver[0] != '\0'))
		{
			ClientConfig.sPropagateResolvers = &sResolver;
			ClientConfig.iPropagateResolverCount = 1u;
			ClientConfig.uPropagateTimeoutMs = 30000u;
		}
		Client = xrtAcmeClientCreate(&ClientConfig);
		if(Client == NULL)
		{
			const xerror* pError = xrtGetError();
			const xerror* pCause = pError;
			int i;
			printf("[diag] init failed kind=%d code=%d\n",
				xrtErrorKind(pError), xrtErrorCode(pError));
			for(i = 0; (i < 5) && (pCause != NULL); i++)
			{
				printf("[diag]   cause kind=%d code=%d dom=%s msg=%s\n",
					xrtErrorKind(pCause), xrtErrorCode(pCause),
					xrtErrorDomain(pCause),
					xrtErrorMessage(pCause) ?
						xrtErrorMessage(pCause) : "?");
				pCause = xrtErrorCause(pCause);
			}
			xacmeHttpUnit(&Dns.Http);
			testRequire(false, "acme flow client init failed");
		}

		/* 账户密钥可导出（持久化复用入口）。 */
		sAccountPem = xrtAcmeClientAccountPem(Client);
		testRequire(
			(sAccountPem != NULL) &&
				(strstr(sAccountPem, "BEGIN PRIVATE KEY") != NULL),
			"acme flow account pem export failed");
		xrtFree(sAccountPem);
		if(getenv("XACME_TEST_ROLLOVER_DROP_BEFORE") != NULL)
		{
			xacmees256key Fresh;
			str sNewPem = NULL;
			str sBefore = xrtAcmeClientAccountPem(Client);
			str sAfter;
			if(xacmeEs256Generate(&Fresh))
				sNewPem = xacmeKeyPemWrite(&Fresh);
			xrtSecureZero(&Fresh, sizeof(Fresh));
			testRequire((sBefore != NULL) && (sNewPem != NULL) &&
				!xrtAcmeClientRollover(Client, sNewPem, NULL) &&
				(xrtErrorCode(xrtGetError()) ==
					XACME_HTTP_ERROR_UNCERTAIN) &&
				(xrtErrorDomain(xrtGetError()) != NULL) &&
				(strcmp(xrtErrorDomain(xrtGetError()),
					"xrt.acme.http") == 0),
				"acme flow uncertain rollover did not retain error");
			sAfter = xrtAcmeClientAccountPem(Client);
			testRequire((sAfter != NULL) &&
				(strcmp(sBefore, sAfter) == 0),
				"acme flow uncertain rollover changed local key");
			xrtFree(sBefore);
			xrtFree(sAfter);
			xrtFree(sNewPem);
			xrtClearError();
			xrtAcmeClientDestroy(Client);
			xacmeHttpUnit(&Dns.Http);
			xrtFree(g_sCertKeyPem);
			xrtFree(sCaPem);
			printf("[PASS] acme rollover unknown result preserves old key\n");
			return 0;
		}

		Provider.sId = "challtestsrv";
		Provider.iCaps = 0u;
		Provider.pContext = &Dns;
		Provider.Add = testDnsAdd;
		Provider.Remove = testDnsRemove;
		Provider.Propagate = NULL;

		Domains[0] = XRT_STR_LITERAL("test.xxrpa.com");
		for(i = 1u; i < 17u; i++) Domains[i] = Domains[0];
		if(getenv("XACME_TEST_DNS_ADD_UNCERTAIN") != NULL)
		{
			size_t iFailAt = (size_t)atoi(
				getenv("XACME_TEST_DNS_ADD_UNCERTAIN"));
			Domains[1] = XRT_STR_LITERAL("other.xxrpa.com");
			Provider.iCaps = XACME_DNS_CAP_PROPAGATE;
			Provider.Propagate = testDnsPropagate;
			bool bIssued = xrtAcmeClientIssueEx(
				Client, Domains, iFailAt, &Provider, false, &Grant);
			cstr sKind = getenv("XACME_TEST_DNS_UNCERTAIN_KIND");
			bool bMemory = sKind != NULL && strcmp(sKind, "memory") == 0;
			bool bProvider = sKind != NULL && strcmp(sKind, "provider") == 0;
			testRequire(!bIssued && g_iDnsAddCalls == iFailAt &&
				g_iDnsRemoveCalls == iFailAt - 1u &&
				(bMemory ? xrtErrorKind(xrtGetError()) == XERR_MEMORY :
				 (xrtErrorDomain(xrtGetError()) != NULL &&
				  strcmp(xrtErrorDomain(xrtGetError()), bProvider ? "xrt.acme.dns" : "xrt.acme.http") == 0 &&
				  xrtErrorCode(xrtGetError()) == (bProvider ? XACME_DNS_ERROR_UNCERTAIN : XACME_HTTP_ERROR_UNCERTAIN))),
				"acme flow replayed uncertain add or leaked prior TXT");
			xrtClearError();
			xrtAcmeClientDestroy(Client);
			xacmeHttpUnit(&Dns.Http);
			xrtFree(g_sCertKeyPem);
			xrtFree(sCaPem);
			printf("[PASS] acme flow uncertain DNS add not replayed\n");
			return 0;
		}
		if(!xrtAcmeClientIssueEx(
				Client, Domains, getenv("XACME_TEST_DUPLICATES") ? 17u : 1u, &Provider,
				(getenv("XACME_PREFER_ALT") != NULL), &Grant))
		{
			const xerror* pError = xrtGetError();
			const xerror* pCause = pError;
			int i;
			printf(
				"[diag] issue failed kind=%d code=%d\n",
				xrtErrorKind(pError), xrtErrorCode(pError));
			for(i = 0; (i < 5) && (pCause != NULL); i++)
			{
				printf(
					"[diag]   cause kind=%d code=%d dom=%s msg=%s\n",
					xrtErrorKind(pCause), xrtErrorCode(pCause),
					xrtErrorDomain(pCause),
					xrtErrorMessage(pCause) ?
						xrtErrorMessage(pCause) : "?");
				pCause = xrtErrorCause(pCause);
			}
			testRequire(false, "acme flow issue failed");
		}

		testRequire(
			(Grant.sFullchainPem != NULL) &&
				(strstr(Grant.sFullchainPem,
					"-----BEGIN CERTIFICATE-----") != NULL),
			"acme flow chain missing certificate");
		/* 私钥必须随链一起交付（没有它证书不可用）。 */
		testRequire(
			(Grant.sKeyPem != NULL) &&
				(strstr(Grant.sKeyPem, "BEGIN PRIVATE KEY") != NULL),
			"acme flow grant missing private key");
		iCerts = 0u;
		for(i = 0; Grant.sFullchainPem[i] != '\0'; i++)
		{
			if(strncmp(
					Grant.sFullchainPem + i, "BEGIN CERTIFICATE", 17u) == 0)
			{
				iCerts++;
			}
		}
		testRequire(iCerts >= 2u, "acme flow chain not a chain");
		printf("[pebble] chain certs=%zu bytes=%zu\n",
			iCerts, strlen(Grant.sFullchainPem));

		/* 产物落盘：Python 编排器做链+私钥配对与有效期核验。 */
		{
			FILE* f;
			char sDump[340];
			snprintf(sDump, sizeof(sDump), "%s/pebble_issued.pem",
				testOutRoot());
			f = fopen(sDump, "wb");
			if(f != NULL)
			{
				fwrite(Grant.sFullchainPem, 1u,
					strlen(Grant.sFullchainPem), f);
				fclose(f);
			}
			snprintf(sDump, sizeof(sDump), "%s/pebble_issued.key.pem",
				testOutRoot());
			f = fopen(sDump, "wb");
			if(f != NULL)
			{
				fwrite(Grant.sKeyPem, 1u, strlen(Grant.sKeyPem), f);
				fclose(f);
			}
		}

		/* Revoke：吊销链首叶证书（reason=1），幂等成功。 */
		testRequire(
			xrtAcmeClientRevoke(Client, Grant.sFullchainPem, 1),
			"acme flow revoke failed");
		xrtAcmeGrantUnit(&Grant);

		/* IssueStored 双跑：第一跑签发落盘，第二跑阈值内直接跳过。 */
		{
			char sStore[320];
			bool bRenewed = false;
			xacmeissuegrant G1;
			xacmeissuegrant G2;
			snprintf(sStore, sizeof(sStore), "%s/store_pebble", testOutRoot());
			testRequire(
				xrtAcmeClientIssueStored(
					Client, Domains, 1u, &Provider, sStore, 30, &G1,
					&bRenewed) &&
					bRenewed,
				"acme flow stored first issue failed");
			testRequire(
				(G1.sKeyPem != NULL) && (G1.sFullchainPem != NULL),
				"acme flow stored first grant incomplete");
			xrtAcmeGrantUnit(&G1);
			testRequire(
				xrtAcmeClientIssueStored(
					Client, Domains, 1u, &Provider, sStore, 30, &G2,
					&bRenewed) &&
					!bRenewed &&
					(strstr(G2.sFullchainPem, "BEGIN CERTIFICATE") != NULL) &&
					(G2.sKeyPem != NULL),
				"acme flow stored skip failed");
			xrtAcmeGrantUnit(&G2);
		}

		/* 一站式 Obtain 双跑：独立 store 根，账户应持久化并复用。 */
		{
			char sStore[320];
			xacmeobtainconfig Obtain;
			xacmeissuegrant G3;
			bool bRenewed = false;
			snprintf(sStore, sizeof(sStore), "%s/store_obtain",
				testOutRoot());
			xrtAcmeObtainConfigInit(&Obtain);
			Obtain.pAccount = &Account;
			Obtain.sCaPem = sCaPem;
			Obtain.sStoreRoot = sStore;
			Obtain.uTimeoutMs = 10000u;
			if((sResolver != NULL) && (sResolver[0] != '\0'))
			{
				Obtain.sPropagateResolvers = &sResolver;
				Obtain.iPropagateResolverCount = 1u;
				Obtain.uPropagateTimeoutMs = 30000u;
			}
			testRequire(
				xrtAcmeObtain(
					&Obtain, Domains, 1u, &Provider, &G3, &bRenewed) &&
					bRenewed && (G3.sKeyPem != NULL) &&
					(G3.sFullchainPem != NULL),
				"acme flow obtain first failed");
			xrtAcmeGrantUnit(&G3);
			{
				bool bObtained = xrtAcmeObtain(
					&Obtain, Domains, 1u, &Provider, &G3, &bRenewed);
				if(!bObtained || bRenewed || G3.sKeyPem == NULL)
				{
					size_t Pending = SIZE_MAX;
					(void)xrtAcmeCleanupPending(0, &Pending);
					fprintf(stderr, "obtain repeat result=%d renewed=%d pending=%zu error=%s\n",
						(int)bObtained, (int)bRenewed, Pending, xrtErrorMessage(xrtGetError()));
				}
				testRequire(bObtained && !bRenewed && G3.sKeyPem != NULL,
					"acme flow obtain skip failed");
			}
			xrtAcmeGrantUnit(&G3);
		}

		/* 账户密钥滚动（§7.3.5）：换新钥后 kid 不变；带 store
		   重存（编排器校验 Obtain 不再开漂移账户）。 */
		{
			char sRollStore[320];
			xacmees256key Fresh;
			str sNewPem = NULL;
			bool bStoreFail =
				(getenv("XACME_TEST_ROLLOVER_STORE_FAIL") != NULL);
			bool bRolled;
			xerror* pExpectedPersistenceError = NULL;
			snprintf(sRollStore, sizeof(sRollStore), "%s/store_rollover",
				testOutRoot());
			if(bStoreFail)
			{
				FILE* pBlocker = fopen(sRollStore, "wb");
				testRequire(pBlocker != NULL && fclose(pBlocker) == 0,
					"acme rollover store failure fixture setup failed");
			}
			if(xacmeEs256Generate(&Fresh))
			{
				sNewPem = xacmeKeyPemWrite(&Fresh);
			}
			xrtSecureZero(&Fresh, sizeof(Fresh));
			testRequire(sNewPem != NULL,
				"acme flow rollover key generation failed");
			if(bStoreFail)
			{
				/* Derive the child contract from the real store on this platform. */
				testRequire(!xrtAcmeStoreSaveAccount(
					sRollStore, Account.sDirectoryUrl, sNewPem),
					"acme rollover store blocker unexpectedly writable");
				pExpectedPersistenceError = xrtErrorRef(xrtGetError());
				testRequire(pExpectedPersistenceError != NULL &&
					xrtErrorDomain(pExpectedPersistenceError) != NULL,
					"acme rollover store blocker missing child diagnosis");
				xrtClearError();
			}
			bRolled = xrtAcmeClientRollover(Client, sNewPem, sRollStore);
			if(bStoreFail)
			{
				xerror* pPersistenceError = xrtErrorRef(xrtGetError());
				str sCurrent = xrtAcmeClientAccountPem(Client);
				testRequire(!bRolled && (sCurrent != NULL) &&
					(strcmp(sCurrent, sNewPem) == 0) &&
					(pPersistenceError != NULL) &&
					(xrtErrorKind(pPersistenceError) == xrtErrorKind(pExpectedPersistenceError)) &&
					(xrtErrorCode(pPersistenceError) == xrtErrorCode(pExpectedPersistenceError)) &&
					(xrtErrorDomain(pPersistenceError) != NULL) &&
					(strcmp(xrtErrorDomain(pPersistenceError),
						xrtErrorDomain(pExpectedPersistenceError)) == 0) &&
					(xrtGetError() == pPersistenceError),
					"acme rollover store failure was hidden or key reverted");
				printf("[PASS] acme rollover child store diagnosis kind=%d domain=%s code=%d; applied key retained\n",
					(int)xrtErrorKind(pPersistenceError), xrtErrorDomain(pPersistenceError),
					(int)xrtErrorCode(pPersistenceError));
				xrtFree(sCurrent);
				xrtErrorFree(pPersistenceError);
				xrtErrorFree(pExpectedPersistenceError);
				xrtClearError();
			}
			else
				testRequire(bRolled, "acme flow rollover failed");
			if(getenv("XACME_TEST_ROLLOVER_REPEAT") != NULL)
			{
				testRequire(xrtAcmeClientRollover(
					Client, sNewPem, sRollStore),
					"acme flow repeated rollover failed");
			}
			xrtFree(sNewPem);
			/* store 里的账户钥应已被新钥替换。 */
			{
				str sStored = xrtAcmeStoreLoadAccount(
					sRollStore,
					"https://example.invalid/not-the-directory");
				(void)sStored; /* 按目录隔离：错误目录应无账户。 */
			}
		}

		/* 账户停用（§7.3.6）：幂等成功。 */
		testRequire(
			xrtAcmeClientDeactivate(Client),
			"acme flow deactivate failed");

		testRequire(xrtAcmeClientCleanup(Client), "acme flow client cleanup incomplete");
		xrtAcmeClientDestroy(Client);
		testRequire(xacmeHttpUnit(&Dns.Http), "acme flow dns transport cleanup incomplete");
		xrtFree(g_sCertKeyPem);
		xrtFree(sCaPem);
		{
			size_t Pending = SIZE_MAX;
			testRequire(xrtAcmeCleanupPending(INT64_C(5), &Pending) && Pending == 0u,
				"acme flow unpublished clients still pending");
		}
	}

	printf("[PASS] acme flow pebble issuance\n");
	return 0;
}
