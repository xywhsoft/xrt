#include "../test.h"

#include "../../src/internal/xacme_http.h"
#include <xrt/acme_client.h>
#include <xrt/acme_obtain.h>
#include <xrt/acme_store.h>

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
	return testDnsExchange(
		(testdnsctx*)pProvider->pContext, "set", sFqdn, sTxt);
}

static bool testDnsRemove(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	return testDnsExchange(
		(testdnsctx*)pProvider->pContext, "clear", sFqdn, sTxt);
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
		xstrview Domains[2];
		xacmeissuegrant Grant;
		size_t iCerts;
		size_t i;
		str sAccountPem;

		sCaPem = testReadFile(sCaPath, &iCaSize);
		testRequire(sCaPem != NULL, "acme flow ca pem read failed");

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
		ClientConfig.uTimeoutUs = 10000000u;
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

		Provider.sId = "challtestsrv";
		Provider.iCaps = 0u;
		Provider.pContext = &Dns;
		Provider.Add = testDnsAdd;
		Provider.Remove = testDnsRemove;
		Provider.Propagate = NULL;

		Domains[0] = XRT_STR_LITERAL("test.xxrpa.com");
		if(!xrtAcmeClientIssue(
				Client, Domains, 1u, &Provider, &Grant))
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
			Obtain.uTimeoutUs = 10000000u;
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
			testRequire(
				xrtAcmeObtain(
					&Obtain, Domains, 1u, &Provider, &G3, &bRenewed) &&
					!bRenewed && (G3.sKeyPem != NULL),
				"acme flow obtain skip failed");
			xrtAcmeGrantUnit(&G3);
		}

		xrtAcmeClientDestroy(Client);
		xacmeHttpUnit(&Dns.Http);
		xrtFree(sCaPem);
	}

	printf("[PASS] acme flow pebble issuance\n");
	return 0;
}
