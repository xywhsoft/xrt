#include "../test.h"

#include "../../src/internal/xacme_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
	Pebble 集成（环境门控）：
	  XACME_PEBBLE_URL = https://localhost:14000/dir
	  XACME_PEBBLE_CA  = pebble TLS 证书 PEM 路径
	  XACME_CHALL_URL  = http://127.0.0.1:8055（challtestsrv 管理 API）
	未设置时跳过。DNS provider 走 challtestsrv 的 set-txt/clear-txt，
	完整链路：directory→账户→订单→dns-01→finalize→证书链。
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
		xacmeclient Client;
		xacmeaccountconfig Account;
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

		xrtAcmeAccountConfigInit(&Account);
		Account.sDirectoryUrl = sUrl;
		if(!xacmeClientInit(&Client, NULL, sCaPem, &Account, 0u))
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
			testRequire(false, "acme flow client init failed");
		}
		printf("[pebble] kid=%s\n", Client.sKid);

		/* 账户密钥可导出（后续 store 里程碑的持久化入口）。 */
		sAccountPem = xacmeClientAccountPem(&Client);
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
		if(!xacmeClientIssue(&Client, Domains, 1u, &Provider, &Grant))
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

		{
			char sDump[320];
			FILE* f;
			snprintf(sDump, sizeof(sDump), "%s/pebble_issued.pem",
				testOutRoot());
			f = fopen(sDump, "wb");
			if(f != NULL)
			{
				fwrite(Grant.sFullchainPem, 1u,
					strlen(Grant.sFullchainPem), f);
				fclose(f);
			}
		}
		xrtAcmeGrantUnit(&Grant);

		/* IssueStored 双跑：第一跑签发落盘，第二跑阈值内直接跳过。 */
		{
			char sStore[320];
			bool bRenewed = false;
			xacmeissuegrant G1;
			xacmeissuegrant G2;
			snprintf(sStore, sizeof(sStore), "%s/store_pebble", testOutRoot());
			testRequire(
				xacmeClientIssueStored(
					&Client, Domains, 1u, &Provider, sStore, 30, &G1,
					&bRenewed) &&
					bRenewed,
				"acme flow stored first issue failed");
			testRequire(
				(G1.sKeyPem != NULL) && (G1.sFullchainPem != NULL),
				"acme flow stored first grant incomplete");
			xrtAcmeGrantUnit(&G1);
			testRequire(
				xacmeClientIssueStored(
					&Client, Domains, 1u, &Provider, sStore, 30, &G2,
					&bRenewed) &&
					!bRenewed &&
					(strstr(G2.sFullchainPem, "BEGIN CERTIFICATE") != NULL) &&
					(G2.sKeyPem != NULL),
				"acme flow stored skip failed");
			xrtAcmeGrantUnit(&G2);
		}

		xacmeClientUnit(&Client);
		xacmeHttpUnit(&Dns.Http);
		xrtFree(sCaPem);
	}

	printf("[PASS] acme flow pebble issuance\n");
	return 0;
}
