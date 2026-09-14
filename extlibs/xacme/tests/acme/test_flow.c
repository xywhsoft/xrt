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
		testdnsctx Dns;
		xacmednsprovider Provider;
		xstrview Domains[2];
		str sChain;
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

		if(!xacmeClientInit(&Client, NULL, sCaPem, sUrl, NULL))
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
		sChain = xacmeClientIssue(&Client, Domains, 1u, &Provider);
		if(sChain == NULL)
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
			strstr(sChain, "-----BEGIN CERTIFICATE-----") != NULL,
			"acme flow chain missing certificate");
		iCerts = 0u;
		for(i = 0; sChain[i] != '\0'; i++)
		{
			if(strncmp(sChain + i, "BEGIN CERTIFICATE", 17u) == 0)
			{
				iCerts++;
			}
		}
		testRequire(iCerts >= 2u, "acme flow chain not a chain");
		printf("[pebble] chain certs=%zu bytes=%zu\n",
			iCerts, strlen(sChain));

		{
			FILE* f = fopen("D:/git/xacme-local/pebble_issued.pem", "wb");
			if(f != NULL)
			{
				fwrite(sChain, 1u, strlen(sChain), f);
				fclose(f);
			}
		}
		xrtFree(sChain);

		/* IssueStored 双跑：第一跑签发落盘，第二跑阈值内直接跳过。 */
		{
			bool bRenewed = false;
			str s1;
			str s2;
			s1 = xacmeClientIssueStored(
				&Client, Domains, 1u, &Provider,
				"D:/git/xacme-local/store_pebble", 30, &bRenewed);
			testRequire(
				(s1 != NULL) && bRenewed,
				"acme flow stored first issue failed");
			xrtFree(s1);
			s2 = xacmeClientIssueStored(
				&Client, Domains, 1u, &Provider,
				"D:/git/xacme-local/store_pebble", 30, &bRenewed);
			testRequire(
				(s2 != NULL) && !bRenewed &&
					(strstr(s2, "BEGIN CERTIFICATE") != NULL),
				"acme flow stored skip failed");
			xrtFree(s2);
		}

		xacmeClientUnit(&Client);
		xacmeHttpUnit(&Dns.Http);
		xrtFree(sCaPem);
	}

	printf("[PASS] acme flow pebble issuance\n");
	return 0;
}
