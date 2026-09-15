#include "../internal/xacme_flow.h"

#if defined(XACME_FEATURE_ACME_FLOW)

#include <xrt/acme_dns.h>
#include <xrt/codec.h>
#include <xrt/json.h>
#include <xrt/memory.h>
#include <xrt/time.h>
#include <xrt/value.h>

#include "../internal/xacme_dnstxt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XACME_FLOW_POLL_MAX 180u
#define XACME_FLOW_URL_MAX ((size_t)sizeof(((xacmeclient*)0)->sKid))

typedef struct xacmeflowurl {
	char sData[512];
	size_t iSize;
} xacmeflowurl;

static void xacmeFlowError(
	xerrkind Kind, xacmeflowerror Code, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.flow", (int32)Code, sMessage);
}

static bool xacmeFlowCopyText(char* sDest, size_t iCapacity, cstr sSource)
{
	if((sSource == NULL) || (strlen(sSource) >= iCapacity))
	{
		return false;
	}
	strcpy(sDest, sSource);
	return true;
}

/* ---------------- JSON 辅助 ---------------- */

static bool xacmeJsonQuoteAppend(xbuffer* pOut, xstrview sText)
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
		else if((unsigned char)c < 0x20u)
		{
			char sEscape[6];
			sEscape[0] = '\\';
			sEscape[1] = 'u';
			sEscape[2] = '0';
			sEscape[3] = '0';
			sEscape[4] = "0123456789ABCDEF"[((unsigned char)c >> 4u) & 0xFu];
			sEscape[5] = "0123456789ABCDEF"[(unsigned char)c & 0xFu];
			bOk = xrtBufferAppend(
				pOut, (xbytesview){ (const uint8*)sEscape, 6u });
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

/* 取对象字符串成员到固定缓冲。 */
static bool xacmeJsonValueText(
	const xvalue* pObject, cstr sKey, xacmeflowurl* pOut)
{
	xvalue* pMember = xrtValueObjectGet(
		pObject, (xstrview){ sKey, strlen(sKey) });
	xstrview Text;
	if((pMember == NULL) ||
		!xrtValueGetString(pMember, &Text) || (Text.Size >= sizeof(pOut->sData)))
	{
		return false;
	}
	memcpy(pOut->sData, Text.Data, Text.Size);
	pOut->sData[Text.Size] = '\0';
	pOut->iSize = Text.Size;
	return true;
}

/* ---------------- nonce 与 POST ---------------- */

static void xacmeFlowTakeNonce(xacmeclient* pClient, xacmehttpresponse* pR)
{
	if((pR->sReplayNonce != NULL) &&
		(strlen(pR->sReplayNonce) < sizeof(pClient->sNonce)))
	{
		strcpy(pClient->sNonce, pR->sReplayNonce);
	}
}

static bool xacmeFlowNewNonce(xacmeclient* pClient)
{
	xacmehttpresponse R;
	if(pClient->sNonce[0] != '\0')
	{
		return true;
	}
	if(!xacmeHttpExchange(
		&pClient->Http, "GET", pClient->sNewNonce, NULL,
		(xstrview){ NULL, 0u }, &R))
	{
		return false;
	}
	xacmeFlowTakeNonce(pClient, &R);
	xacmeHttpResponseUnit(&R);
	if(pClient->sNonce[0] == '\0')
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
			"acme flow new-nonce response missing Replay-Nonce");
		return false;
	}
	return true;
}

/*
	执行一次 ACME POST（bKid=false 时保护头嵌 JWK，用于 newAccount）。
	响应由调用方 Unit；本函数顺带收割 nonce。
*/
static bool xacmeFlowPost(
	xacmeclient* pClient, cstr sUrl, xstrview sPayload, bool bKid,
	xacmehttpresponse* pR, uint32 uNonceRetry)
{
	xacmejwsheader H;
	str sToken = NULL;
	bool bOk;

	if(!xacmeFlowNewNonce(pClient))
	{
		return false;
	}
	H.Nonce = (xstrview){ pClient->sNonce, strlen(pClient->sNonce) };
	H.Url = (xstrview){ sUrl, strlen(sUrl) };
	if(bKid)
	{
		H.Kid = (xstrview){ pClient->sKid, strlen(pClient->sKid) };
	}
	else
	{
		H.Kid.Data = NULL;
		H.Kid.Size = 0u;
	}
	sToken = xacmeJwsEs256(&pClient->AccountKey, &H, sPayload);
	if(sToken == NULL)
	{
		return false;
	}
	pClient->sNonce[0] = '\0';
	bOk = xacmeHttpExchange(
		&pClient->Http, "POST", sUrl, "application/jose+json",
		(xstrview){ sToken, strlen(sToken) }, pR);
	xrtFree(sToken);
	if(!bOk)
	{
		return false;
	}
	xacmeFlowTakeNonce(pClient, pR);
	/* RFC 8555 badNonce：换新 nonce 重试（上限 3 次）。 */
	if((pR->iStatus == 400u) && (pR->sBody != NULL) &&
		(strstr(pR->sBody, "badNonce") != NULL) && (uNonceRetry < 3u))
	{
		pClient->sNonce[0] = '\0';
		if(xacmeFlowNewNonce(pClient))
		{
			xacmeHttpResponseUnit(pR);
			return xacmeFlowPost(
				pClient, sUrl, sPayload, bKid, pR, uNonceRetry + 1u);
		}
	}
	return true;
}

/* POST-as-GET：空负载。 */
static bool xacmeFlowPostAsGet(
	xacmeclient* pClient, cstr sUrl, xacmehttpresponse* pR)
{
	return xacmeFlowPost(
		pClient, sUrl, (xstrview){ "", 0u }, true, pR, 0u);
}

static uint32 xacmeFlowSleepMs(xacmehttpresponse* pR)
{
	long v;
	if((pR->sRetryAfter == NULL) || (pR->sRetryAfter[0] == '\0'))
	{
		return 500u;
	}
	v = atol(pR->sRetryAfter);
	if((v <= 0) || (v > 5))
	{
		v = 1;
	}
	return (uint32)(v * 1000u);
}

/*
	轮询某个 URL 的 JSON status 字段：pending/processing 继续等，
	其他状态（valid/ready/invalid...）即返回该状态的堆副本。
	pFinalizeOut 非空时顺带收割 finalize 字段一次。
*/
static str xacmeFlowWaitStatus(
	xacmeclient* pClient, cstr sUrl, xacmeflowurl* pFinalizeOut)
{
	size_t i;
	for(i = 0; i < XACME_FLOW_POLL_MAX; i++)
	{
		xacmehttpresponse R;
		xvalue* pRoot = NULL;
		xacmeflowurl Status;
		str sStatus = NULL;
		bool bTerminal = false;
		if(!xacmeFlowPostAsGet(pClient, sUrl, &R))
		{
			return NULL;
		}
		if((R.sBody != NULL) && (R.iBodySize > 0u))
		{
			pRoot = xrtJsonParse((xstrview){ R.sBody, R.iBodySize });
		}
		if((pRoot != NULL) && xrtValueIs(pRoot, XVALUE_OBJECT) &&
			xacmeJsonValueText(pRoot, "status", &Status))
		{
			sStatus = (str)xrtMalloc(Status.iSize + 1u);
			if(sStatus != NULL)
			{
				memcpy(sStatus, Status.sData, Status.iSize + 1u);
			}
			if((pFinalizeOut != NULL) &&
				xacmeJsonValueText(pRoot, "finalize", pFinalizeOut))
			{
				pFinalizeOut = NULL;
			}
			bTerminal = (strcmp(Status.sData, "pending") != 0) &&
				(strcmp(Status.sData, "processing") != 0);
		}
		if(pRoot != NULL)
		{
			xrtValueRelease(pRoot);
		}
		if((sStatus != NULL) && bTerminal)
		{
			if(strcmp(sStatus, "valid") != 0 && getenv("XACME_DEBUG"))
			{
				printf("[dbg] terminal=%s body=%.1400s\n", sStatus,
					(R.sBody != NULL) ? R.sBody : "");
			}
			xacmeHttpResponseUnit(&R);
			return sStatus;
		}
		xrtFree(sStatus);
		{
			uint32 uMs = xacmeFlowSleepMs(&R);
			bool bAgain = (R.iStatus >= 200u) && (R.iStatus < 300u);
			xacmeHttpResponseUnit(&R);
			if(!bAgain)
			{
				if(getenv("XACME_DEBUG"))
				{
					printf("[dbg] poll resp status=%u body=%.160s\n",
						(unsigned)R.iStatus,
						(R.sBody != NULL) ? R.sBody : "");
				}
				char sDetail[280];
				snprintf(sDetail, sizeof(sDetail),
					"acme flow poll error status=%u body=%.180s",
					(unsigned)R.iStatus,
					(R.sBody != NULL) ? R.sBody : "");
				xacmeFlowError(
					XERR_PROTOCOL, XACME_FLOW_ERROR_PROTOCOL, sDetail);
				return NULL;
			}
			xrtSleep(uMs);
		}
	}
	xacmeFlowError(
		XERR_TIMEOUT, XACME_FLOW_ERROR_PROTOCOL,
		"acme flow status poll exhausted");
	return NULL;
}

/* ---------------- 传播确认 ---------------- */

/* 任一配置 resolver 已返回期望 TXT 值即视为可见。 */
static bool xacmeFlowTxtVisible(
	xacmedns* pDns, const xacmeclient* pClient,
	cstr sFqdn, cstr sExpected)
{
	size_t i;
	for(i = 0; i < pClient->iPropagateResolverCount; i++)
	{
		char sRecords[4][XACME_TXT_RECORD_MAX];
		size_t iCount = 0u;
		size_t j;
		if(!xacmeDnsTxtQuery(
				pDns, pClient->sPropagateResolvers[i], 53u, sFqdn,
				sRecords, 4u, &iCount))
		{
			continue; /* 单个 resolver 不可达不算失败。 */
		}
		for(j = 0; j < iCount; j++)
		{
			if(strcmp(sRecords[j], sExpected) == 0)
			{
				return true;
			}
		}
	}
	return false;
}

/*
	挑战触发前的传播确认门（尽力而为）：
	- provider 带 XACME_DNS_CAP_PROPAGATE 时委托 provider 自证；
	- 否则对公共 resolver 组轮询 TXT（任一可见即通过）；
	- 超时不阻断签发——CA 只查权威侧，公共递归滞后不必然失败，
	  仅在 XACME_DEBUG 下输出提示。
*/
static void xacmeFlowWaitPropagate(
	xacmeclient* pClient, const xacmednsprovider* pDns,
	cstr sFqdn, cstr sTxt)
{
	xacmedns Probe;
	uint64 uDeadline;
	if(((pDns->iCaps & XACME_DNS_CAP_PROPAGATE) != 0u) &&
		(pDns->Propagate != NULL))
	{
		(void)pDns->Propagate((xacmednsprovider*)pDns,
			(xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sTxt, strlen(sTxt) });
		return;
	}
	if(!xacmeDnsInit(&Probe, pClient->Http.pEngine))
	{
		return;
	}
	uDeadline = xrtClock() +
		(uint64)pClient->uPropagateTimeoutMs * UINT64_C(1000);
	while(xrtClock() < uDeadline)
	{
		if(xacmeFlowTxtVisible(&Probe, pClient, sFqdn, sTxt))
		{
			xacmeDnsUnit(&Probe);
			return;
		}
		xrtSleep(2000u);
	}
	xacmeDnsUnit(&Probe);
	if(getenv("XACME_DEBUG"))
	{
		printf("[dbg] propagate confirm timeout fqdn=%s\n", sFqdn);
	}
}

/* ---------------- 初始化与签发 ---------------- */

/*
	重新拉取授权对象，提取挑战 error.detail（约 160 字符）进 sOut。
	失败时留空串——诊断增强，不改变失败语义。
*/
static void xacmeFlowChallengeDetail(
	xacmeclient* pClient, cstr sAuthzUrl, char* sOut, size_t iCapacity)
{
	xacmehttpresponse R;
	xvalue* pRoot;
	xvalue* pChallenges;
	size_t j;

	sOut[0] = '\0';
	if(!xacmeFlowPostAsGet(pClient, sAuthzUrl, &R))
	{
		return;
	}
	pRoot = (R.sBody != NULL) ?
		xrtJsonParse((xstrview){ R.sBody, R.iBodySize }) : NULL;
	xacmeHttpResponseUnit(&R);
	if((pRoot == NULL) || !xrtValueIs(pRoot, XVALUE_OBJECT))
	{
		return;
	}
	pChallenges = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("challenges"));
	for(j = 0; (pChallenges != NULL) &&
		xrtValueIs(pChallenges, XVALUE_ARRAY) &&
		(j < xrtValueCount(pChallenges)); j++)
	{
		xvalue* pChallenge = xrtValueArrayGet(pChallenges, j);
		xvalue* pError;
		xvalue* pDetail;
		xstrview Text;
		if((pChallenge == NULL) || !xrtValueIs(pChallenge, XVALUE_OBJECT))
		{
			continue;
		}
		pError = xrtValueObjectGet(pChallenge, XRT_STR_LITERAL("error"));
		if((pError == NULL) || !xrtValueIs(pError, XVALUE_OBJECT))
		{
			continue;
		}
		pDetail = xrtValueObjectGet(pError, XRT_STR_LITERAL("detail"));
		if((pDetail != NULL) && xrtValueGetString(pDetail, &Text) &&
			(Text.Size > 0u))
		{
			size_t iCopy = (Text.Size < (iCapacity - 1u)) ?
				Text.Size : (iCapacity - 1u);
			memcpy(sOut, Text.Data, iCopy);
			sOut[iCopy] = '\0';
			break;
		}
	}
	xrtValueRelease(pRoot);
}

bool xacmeClientInit(
	xacmeclient* pClient, struct xnetengine* pBorrowedEngine,
	cstr sCaPem, const xacmeaccountconfig* pAccount, uint64 uTimeoutUs)
{
	xacmehttpresponse R;
	xvalue* pRoot = NULL;
	xacmeflowurl NewNonce;
	xacmeflowurl NewAccount;
	xacmeflowurl NewOrder;
	xbuffer Payload;
	bool bOk = false;

	if((pClient == NULL) || (pAccount == NULL) ||
		(pAccount->sDirectoryUrl == NULL) ||
		(pAccount->sDirectoryUrl[0] == '\0'))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme client init requires client and account config");
		return false;
	}
	memset(pClient, 0, sizeof(*pClient));
	if(!xacmeFlowCopyText(
			pClient->sDirectoryUrl, sizeof(pClient->sDirectoryUrl),
			pAccount->sDirectoryUrl))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme client directory url too long");
		return false;
	}
	if(!xacmeHttpInit(&pClient->Http, pBorrowedEngine, sCaPem, uTimeoutUs))
	{
		goto Done;
	}
	if((pAccount->sAccountKeyPem != NULL) &&
		(pAccount->sAccountKeyPem[0] != '\0'))
	{
		if(!xacmeKeyPemRead(
			pAccount->sAccountKeyPem, strlen(pAccount->sAccountKeyPem),
			&pClient->AccountKey))
		{
			xacmeFlowError(
				XERR_ARGUMENT, XACME_FLOW_ERROR_ACCOUNT,
				"acme client account pem invalid");
			goto Done;
		}
	}
	else if(!xacmeEs256Generate(&pClient->AccountKey))
	{
		goto Done;
	}

	/* directory */
	if(!xacmeHttpExchange(
		&pClient->Http, "GET", pAccount->sDirectoryUrl, NULL,
		(xstrview){ NULL, 0u }, &R))
	{
		xacmeFlowError(
			XERR_IO, XACME_FLOW_ERROR_DIRECTORY,
			"acme client directory fetch failed");
		goto Done;
	}
	if((R.iStatus != 200u) || (R.sBody == NULL))
	{
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_DIRECTORY,
			"acme client directory response invalid");
		goto Done;
	}
	pRoot = xrtJsonParse((xstrview){ R.sBody, R.iBodySize });
	xacmeHttpResponseUnit(&R);
	if((pRoot == NULL) || !xrtValueIs(pRoot, XVALUE_OBJECT) ||
		!xacmeJsonValueText(pRoot, "newNonce", &NewNonce) ||
		!xacmeJsonValueText(pRoot, "newAccount", &NewAccount) ||
		!xacmeJsonValueText(pRoot, "newOrder", &NewOrder))
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_DIRECTORY,
			"acme client directory json missing endpoints");
		goto Done;
	}
	if(!xacmeFlowCopyText(
			pClient->sNewNonce, sizeof(pClient->sNewNonce), NewNonce.sData) ||
		!xacmeFlowCopyText(
			pClient->sNewAccount, sizeof(pClient->sNewAccount),
			NewAccount.sData) ||
		!xacmeFlowCopyText(
			pClient->sNewOrder, sizeof(pClient->sNewOrder), NewOrder.sData))
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_DIRECTORY,
			"acme client directory url too long");
		goto Done;
	}

	/* 注册或复用账户：201=新建，200=已存在。载荷按需携带
	   contact 与 externalAccountBinding（RFC 8555 §7.3/§7.3.4）。 */
	xrtBufferInit(&Payload);
	if(!xrtBufferAppend(
		&Payload, XRT_BYTES_LITERAL("{\"termsOfServiceAgreed\":true")))
	{
		xrtBufferUnit(&Payload);
		goto Done;
	}
	if((pAccount->sContactEmail != NULL) &&
		(pAccount->sContactEmail[0] != '\0'))
	{
		char sMailto[320];
		snprintf(sMailto, sizeof(sMailto), "mailto:%s",
			pAccount->sContactEmail);
		if(!xrtBufferAppend(&Payload, XRT_BYTES_LITERAL(",\"contact\":[")) ||
			!xacmeJsonQuoteAppend(
				&Payload, (xstrview){ sMailto, strlen(sMailto) }) ||
			!xrtBufferAppendByte(&Payload, (uint8)']'))
		{
			xrtBufferUnit(&Payload);
			goto Done;
		}
	}
	if((pAccount->Eab.sKid != NULL) && (pAccount->Eab.sKid[0] != '\0'))
	{
		/* base64url 文本 MAC key（兼容带/不带填充）。 */
		static const xbase64config B64UrlPad = {
			NULL, XBASE64_URL | XBASE64_OPTIONAL_PADDING
		};
		uint8 Mac[64];
		size_t iMacSize = 0u;
		str sJwk = NULL;
		str sEab = NULL;
		if((pAccount->Eab.sHmac == NULL) || (pAccount->Eab.sHmac[0] == '\0'))
		{
			xrtBufferUnit(&Payload);
			xacmeFlowError(
				XERR_ARGUMENT, XACME_FLOW_ERROR_ACCOUNT,
				"acme client eab kid without hmac");
			goto Done;
		}
		if(!xrtBase64Decode(
				pAccount->Eab.sHmac, strlen(pAccount->Eab.sHmac), Mac,
				sizeof(Mac), &iMacSize, &B64UrlPad) ||
			(iMacSize == 0u))
		{
			xrtBufferUnit(&Payload);
			xacmeFlowError(
				XERR_ARGUMENT, XACME_FLOW_ERROR_ACCOUNT,
				"acme client eab hmac invalid base64url");
			goto Done;
		}
		sJwk = xacmeJwkEcJson(&pClient->AccountKey);
		if(sJwk != NULL)
		{
			sEab = xacmeJwsEabHs256(
				pAccount->Eab.sKid, pClient->sNewAccount,
				(xstrview){ sJwk, strlen(sJwk) }, Mac, iMacSize);
		}
		xrtFree(sJwk);
		if(sEab == NULL)
		{
			xrtBufferUnit(&Payload);
			xacmeFlowError(
				XERR_ARGUMENT, XACME_FLOW_ERROR_ACCOUNT,
				"acme client eab binding build failed");
			goto Done;
		}
		if(!xrtBufferAppend(
				&Payload, XRT_BYTES_LITERAL(",\"externalAccountBinding\":")) ||
			!xrtBufferAppend(
				&Payload,
				(xbytesview){ (const uint8*)sEab, strlen(sEab) }))
		{
			xrtFree(sEab);
			xrtBufferUnit(&Payload);
			goto Done;
		}
		xrtFree(sEab);
	}
	if(!xrtBufferAppendByte(&Payload, (uint8)'}'))
	{
		xrtBufferUnit(&Payload);
		goto Done;
	}
	if(!xacmeFlowPost(
		pClient, pClient->sNewAccount,
		(xstrview){ (cstr)Payload.Data, Payload.Size }, false, &R, 0u))
	{
		xrtBufferUnit(&Payload);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT,
			"acme client new-account post failed");
		goto Done;
	}
	xrtBufferUnit(&Payload);
	if((R.iStatus != 200u) && (R.iStatus != 201u))
	{
		char sDetail[160];
		snprintf(sDetail, sizeof(sDetail),
			"acme new-account status=%u body=%.100s",
			(unsigned)R.iStatus,
			(R.sBody != NULL) ? R.sBody : "");
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT, sDetail);
		goto Done;
	}
	if((R.sLocation == NULL) ||
		!xacmeFlowCopyText(
			pClient->sKid, sizeof(pClient->sKid), R.sLocation))
	{
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT,
			"acme client new-account missing location");
		goto Done;
	}
	xacmeHttpResponseUnit(&R);
	bOk = true;

Done:
	if(pRoot != NULL)
	{
		xrtValueRelease(pRoot);
	}
	if(!bOk)
	{
		/* 先保留首个根因，再拆传输（Unit 可能覆盖线程错误）。 */
		xerror* pFirst = xrtErrorRef(xrtGetError());
		xacmeHttpUnit(&pClient->Http);
		if(pFirst != NULL)
		{
			xrtSetErrorTake(pFirst);
		}
	}
	return bOk;
}

void xacmeClientUnit(xacmeclient* pClient)
{
	if(pClient == NULL)
	{
		return;
	}
	xacmeHttpUnit(&pClient->Http);
	memset(pClient, 0, sizeof(*pClient));
}

str xacmeClientAccountPem(const xacmeclient* pClient)
{
	if(pClient == NULL)
	{
		return NULL;
	}
	return xacmeKeyPemWrite(&pClient->AccountKey);
}

bool xacmeClientIssue(
	xacmeclient* pClient, const xstrview* pDomains, size_t iDomainCount,
	const struct xacmednsprovider* pDns, xacmeissuegrant* pOut)
{
	xbuffer Payload;
	xacmehttpresponse R;
	xvalue* pRoot = NULL;
	bool bResult = false;
	xacmeflowurl Finalize;
	xacmeflowurl OrderUrl;
	Finalize.sData[0] = 0;
	xacmeflowurl Certificate;
	size_t i;
	bool bOk = false;

	if((pClient == NULL) || (pDomains == NULL) || (iDomainCount == 0u) ||
		(pDns == NULL) || (pDns->Add == NULL) || (pDns->Remove == NULL) ||
		(pOut == NULL) ||
		!xrtAcmeDnsProviderValidate(pDns))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme issue requires client, domains, dns provider and output");
		return false;
	}
	memset(pOut, 0, sizeof(*pOut));

	/* 1. 新订单；identifier 用去 *.\ 后的基础域并去重（通配符与
	   裸域共用一次授权；通配符语义由 CSR 的 SAN 表达）。 */
	xacmeflowurl Bases[16];
	size_t iBaseCount = 0u;
	xrtBufferInit(&Payload);
	for(i = 0; i < iDomainCount; i++)
	{
		xstrview Domain = pDomains[i];
		size_t j;
		bool bDup = false;
		if((Domain.Data == NULL) || (Domain.Size == 0u) ||
			(iBaseCount >= (sizeof(Bases) / sizeof(Bases[0]))))
		{
			xacmeFlowError(
				XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
				"acme issue domain list invalid");
			goto Done;
		}
		if((Domain.Size >= 3u) && (Domain.Data[0] == '*') &&
			(Domain.Data[1] == '.'))
		{
			Domain.Data += 2u;
			Domain.Size -= 2u;
		}
		for(j = 0; j < iBaseCount; j++)
		{
			if((Bases[j].iSize == Domain.Size) &&
				(memcmp(Bases[j].sData, Domain.Data, Domain.Size) == 0))
			{
				bDup = true;
				break;
			}
		}
		if(bDup)
		{
			continue;
		}
		memcpy(Bases[iBaseCount].sData, Domain.Data, Domain.Size);
		Bases[iBaseCount].sData[Domain.Size] = 0;
		Bases[iBaseCount].iSize = Domain.Size;
		iBaseCount++;
	}
	if(!xrtBufferAppend(
		&Payload, XRT_BYTES_LITERAL("{\"identifiers\":[")))
	{
		goto Done;
	}
	for(i = 0; i < iBaseCount; i++)
	{
		if((i > 0u) && !xrtBufferAppendByte(&Payload, (uint8)','))
		{
			goto Done;
		}
		if(!xrtBufferAppend(
				&Payload, XRT_BYTES_LITERAL("{\"type\":\"dns\",\"value\":")) ||
			!xacmeJsonQuoteAppend(
				&Payload, (xstrview){ Bases[i].sData, Bases[i].iSize }) ||
			!xrtBufferAppendByte(&Payload, (uint8)'}'))
		{
			goto Done;
		}
	}
	if(!xrtBufferAppend(&Payload, XRT_BYTES_LITERAL("]}")))
	{
		goto Done;
	}
	if(!xacmeFlowPost(
		pClient, pClient->sNewOrder,
		(xstrview){ (cstr)Payload.Data, Payload.Size }, true, &R, 0u))
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ORDER,
			"acme issue new-order post failed");
		goto Done;
	}
	if((R.iStatus != 201u) || (R.sLocation == NULL) ||
		!xacmeFlowCopyText(
			OrderUrl.sData, sizeof(OrderUrl.sData), R.sLocation))
	{
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ORDER,
			"acme issue new-order response invalid");
		goto Done;
	}
	OrderUrl.iSize = strlen(OrderUrl.sData);
	if((R.sBody == NULL) ||
		((pRoot = xrtJsonParse((xstrview){ R.sBody, R.iBodySize })) == NULL) ||
		!xrtValueIs(pRoot, XVALUE_OBJECT))
	{
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ORDER,
			"acme issue new-order json invalid");
		goto Done;
	}
	xacmeHttpResponseUnit(&R);

	/* 2. 逐授权域处理 dns-01。 */
	{
		xvalue* pAuthz = xrtValueObjectGet(
			pRoot, XRT_STR_LITERAL("authorizations"));
		size_t iCount = (pAuthz != NULL) ? xrtValueCount(pAuthz) : 0u;
		char sThumb[44];
		if((pAuthz == NULL) || !xrtValueIs(pAuthz, XVALUE_ARRAY) ||
			(iCount != iBaseCount) ||
			!xacmeJwkEcThumbprint(&pClient->AccountKey, sThumb))
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_ORDER,
				"acme issue order authorizations invalid");
			goto Done;
		}
		for(i = 0; i < iCount; i++)
		{
			xvalue* pUrl = xrtValueArrayGet(pAuthz, i);
			xstrview UrlText;
			xacmeflowurl Authz;
			xacmehttpresponse A;
			xvalue* pAuthRoot = NULL;
			xacmeflowurl Token;
			xvalue* pChallenges;
			size_t j;
			bool bChallengeOk = false;
			if((pUrl == NULL) ||
				!xrtValueGetString(pUrl, &UrlText) ||
				(UrlText.Size >= sizeof(Authz.sData)))
			{
				xacmeFlowError(
					XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
					"acme issue authorization url invalid");
				goto Done;
			}
			memcpy(Authz.sData, UrlText.Data, UrlText.Size);
			Authz.sData[UrlText.Size] = '\0';
			Authz.iSize = UrlText.Size;
			if(!xacmeFlowPostAsGet(pClient, Authz.sData, &A))
			{
				xacmeFlowError(
					XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
					"acme issue authorization fetch failed");
				goto Done;
			}
			if(A.sBody != NULL)
			{
				pAuthRoot = xrtJsonParse((xstrview){ A.sBody, A.iBodySize });
			}
			if(pAuthRoot == NULL)
			{
				char sDetail[220];
				snprintf(sDetail, sizeof(sDetail),
					"acme authz fetch status=%u body=%.150s",
					(unsigned)A.iStatus,
					(A.sBody != NULL) ? A.sBody : "");
				xacmeHttpResponseUnit(&A);
				xacmeFlowError(
					XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE, sDetail);
				goto Done;
			}
			{
				xacmeflowurl AuthzStatus;
				bool bValid = xacmeJsonValueText(
					pAuthRoot, "status", &AuthzStatus) &&
					(strcmp(AuthzStatus.sData, "valid") == 0);
				pChallenges = xrtValueObjectGet(
					pAuthRoot, XRT_STR_LITERAL("challenges"));
				if(bValid)
				{
					/* 已有效的授权（如复用）视为已解决。 */
					xrtValueRelease(pAuthRoot);
					xacmeHttpResponseUnit(&A);
					bChallengeOk = true;
					continue;
				}
				bChallengeOk = false;
				for(j = 0; (pChallenges != NULL) &&
					xrtValueIs(pChallenges, XVALUE_ARRAY) &&
					(j < xrtValueCount(pChallenges)); j++)
				{
					xvalue* pChallenge = xrtValueArrayGet(pChallenges, j);
					xacmeflowurl Type;
					xacmeflowurl ChallengeUrl;
					if((pChallenge == NULL) ||
						!xrtValueIs(pChallenge, XVALUE_OBJECT) ||
						!xacmeJsonValueText(pChallenge, "type", &Type) ||
						(strcmp(Type.sData, "dns-01") != 0))
					{
						continue;
					}
						if(!xacmeJsonValueText(
								pChallenge, "url", &ChallengeUrl) ||
							!xacmeJsonValueText(pChallenge, "token", &Token))
						{
							if(getenv("XACME_DEBUG"))
							{
								printf("[dbg] dns-01 missing url/token\n");
							}
							continue;
						}
					/* keyAuthz = token '.' thumbprint；TXT = b64url(sha256) */
					{
						str sStatus = NULL;
						bool bValidNow = false;
						uint8 Digest[XRT_SHA256_SIZE];
						char sKeyAuthz[512];
						size_t iKeyAuthz = Token.iSize + 1u + 43u;
						str sTxt;
						str sFqdn = NULL;
						if(iKeyAuthz >= sizeof(sKeyAuthz))
						{
							if(getenv("XACME_DEBUG")) printf("[dbg] keyauthz too long\n");
							continue;
						}
						memcpy(sKeyAuthz, Token.sData, Token.iSize);
						sKeyAuthz[Token.iSize] = '.';
						memcpy(sKeyAuthz + Token.iSize + 1u, sThumb, 43u);
						if(!xrtSha256(sKeyAuthz, iKeyAuthz, Digest))
						{
							continue;
						}
						{
							static const xbase64config B64Url = {
								NULL,
								XBASE64_URL | XBASE64_NO_PADDING
							};
							sTxt = xrtBase64EncodeNew(
								Digest, sizeof(Digest), &B64Url);
						}
						if(sTxt == NULL)
						{
							if(getenv("XACME_DEBUG")) printf("[dbg] txt b64 null\n");
							continue;
						}
						/* TXT 属主 = _acme-challenge.<基础域> */
						{
							xbuffer Fqdn;
							xstrview Domain = (xstrview){
								Bases[i].sData, Bases[i].iSize };
							bool bFqdnOk;
							xrtBufferInit(&Fqdn);
							bFqdnOk = xrtBufferAppend(
								&Fqdn,
								XRT_BYTES_LITERAL("_acme-challenge.")) &&
								xrtBufferAppend(
									&Fqdn,
									(xbytesview){
										(const uint8*)Domain.Data,
										Domain.Size }) &&
								xrtBufferAppendByte(&Fqdn, 0u);
							sFqdn = bFqdnOk ? (str)Fqdn.Data : NULL;
							if(!bFqdnOk)
							{
								xrtBufferUnit(&Fqdn);
							}
						}
						if((sFqdn == NULL) ||
							!pDns->Add((xacmednsprovider*)pDns,
								(xstrview){ sFqdn, strlen(sFqdn) },
								(xstrview){ sTxt, strlen(sTxt) }))
						{
							if(getenv("XACME_DEBUG")) printf("[dbg] dns add failed fqdn=%s err=%d\n", sFqdn ? sFqdn : "null", (int)xrtErrorKind(xrtGetError()));
							xrtFree(sFqdn);
							xrtFree(sTxt);
							continue;
						}
						/* 传播确认通过后再触发挑战。 */
						xacmeFlowWaitPropagate(
							pClient, pDns, sFqdn, sTxt);
						/* 触发挑战并轮询授权至 valid。 */
						if(xacmeFlowPost(
							pClient, ChallengeUrl.sData,
							(xstrview){ "{}", 2u }, true, &A, 0u))
						{
							xacmeHttpResponseUnit(&A);
							sStatus = xacmeFlowWaitStatus(
								pClient, Authz.sData, NULL);
							bValidNow = (sStatus != NULL) &&
								(strcmp(sStatus, "valid") == 0);
						}
						/* TXT 记录使命完成，删除。 */
						(void)pDns->Remove((xacmednsprovider*)pDns,
							(xstrview){ sFqdn, strlen(sFqdn) },
							(xstrview){ sTxt, strlen(sTxt) });
						xrtFree(sFqdn);
						xrtFree(sTxt);
						if(!bValidNow)
						{
							char sChallengeError[200];
							char sDetail[320];
							const xerror* pE = xrtGetError();
							/* 取挑战 error.detail 作诊断；失败留空。 */
							xacmeFlowChallengeDetail(
								pClient, Authz.sData, sChallengeError,
								sizeof(sChallengeError));
							if(getenv("XACME_DEBUG"))
							{
								printf("[dbg] authz not valid: "
									"status=%s err=%d %s challenge=%.160s\n",
									(sStatus != NULL) ? sStatus : "null",
									(int)xrtErrorKind(pE),
									(xrtErrorMessage(pE) != NULL) ?
										xrtErrorMessage(pE) : "-",
									sChallengeError);
							}
							snprintf(sDetail, sizeof(sDetail),
								"acme issue authorization status=%.32s "
								"challenge=%.180s",
								(sStatus != NULL) ? sStatus : "null",
								sChallengeError);
							xrtFree(sStatus);
							xrtValueRelease(pAuthRoot);
							xacmeFlowError(
								XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
								sDetail);
							goto Done;
						}
						xrtFree(sStatus);
						bChallengeOk = true;
						break;
					}
				}
				xrtValueRelease(pAuthRoot);
				xacmeHttpResponseUnit(&A);
				if(!bChallengeOk)
				{
					xacmeFlowError(
						XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
						"acme issue no dns-01 challenge solved");
					goto Done;
				}
			}
		}
	}
	xrtValueRelease(pRoot);
	pRoot = NULL;

	/* 3. 订单 ready → finalize。 */
	{
		str sStatus = xacmeFlowWaitStatus(pClient, OrderUrl.sData, &Finalize);
		bool bReady = (sStatus != NULL) &&
			((strcmp(sStatus, "ready") == 0) ||
				(strcmp(sStatus, "valid") == 0));
		xrtFree(sStatus);
		if(!bReady)
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_ORDER,
				"acme issue order not ready");
			goto Done;
		}
	}
	/* CSR（首个域名为 CN；SAN 全量，含通配符原样）。 */
	{
		xacmecsrconfig Csr;
		xbuffer CsrDer;
		str sCsrB64;
		xacmees256key CertKey;
		static const xbase64config B64Url = {
			NULL, XBASE64_URL | XBASE64_NO_PADDING };
		bool bCsrOk;
		Csr.CommonName = pDomains[0];
		Csr.Domains = pDomains;
		Csr.DomainCount = iDomainCount;
		xrtBufferInit(&CsrDer);
		/* 证书密钥独立于账户密钥（CA 普遍拒绝复用账户钥）。 */
		bCsrOk = xacmeEs256Generate(&CertKey) &&
			xacmeCsrEc(&CertKey, &Csr, &CsrDer);
		if(bCsrOk)
		{
			/* 私钥随产物导出（没有它证书不可用）。 */
			pOut->sKeyPem = xacmeKeyPemWrite(&CertKey);
			bCsrOk = (pOut->sKeyPem != NULL);
		}
		/* 栈上密钥副本立即擦除。 */
		xrtSecureZero(&CertKey, sizeof(CertKey));
		sCsrB64 = bCsrOk ? xrtBase64EncodeNew(
			CsrDer.Data, CsrDer.Size, &B64Url) : NULL;
		xrtBufferUnit(&CsrDer);
		if(sCsrB64 == NULL)
		{
			xacmeFlowError(
				XERR_INTERNAL, XACME_FLOW_ERROR_FINALIZE,
				"acme issue csr build failed");
			goto Done;
		}
		xrtBufferClear(&Payload);
		bOk = xrtBufferAppend(&Payload, XRT_BYTES_LITERAL("{\"csr\":\"")) &&
			xrtBufferAppend(
				&Payload,
				(xbytesview){
					(const uint8*)sCsrB64, strlen(sCsrB64) }) &&
				xrtBufferAppend(&Payload, XRT_BYTES_LITERAL("\"}"));
		xrtFree(sCsrB64);
		if(!bOk)
		{
			goto Done;
		}
		bOk = false;
	}
	if(!xacmeFlowPost(
		pClient, Finalize.sData,
		(xstrview){ (cstr)Payload.Data, Payload.Size }, true, &R, 0u))
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_FINALIZE,
			"acme issue finalize post failed");
		goto Done;
	}
	if((R.iStatus != 200u) && (R.iStatus != 201u))
	{
		char sDetail[300];
		snprintf(sDetail, sizeof(sDetail),
			"acme finalize status=%u body=%.200s",
			(unsigned)R.iStatus,
			(R.sBody != NULL) ? R.sBody : "");
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_FINALIZE, sDetail);
		goto Done;
	}
	xacmeHttpResponseUnit(&R);

	/* 4. 订单 valid → 证书 URL → 下载链。 */
	{
		str sStatus = xacmeFlowWaitStatus(pClient, OrderUrl.sData, NULL);
		bool bValid = (sStatus != NULL) && (strcmp(sStatus, "valid") == 0);
		char sDetail[320];
		snprintf(sDetail, sizeof(sDetail),
			"acme order not valid after finalize (state=%s)",
			(sStatus != NULL) ? sStatus : "null");
		xrtFree(sStatus);
		if(!bValid)
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_FINALIZE, sDetail);
			goto Done;
		}
	}
	if(!xacmeFlowPostAsGet(pClient, OrderUrl.sData, &R))
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_CERTIFICATE,
			"acme issue order fetch for certificate failed");
		goto Done;
	}
	{
		xvalue* pOrderRoot = (R.sBody != NULL) ?
			xrtJsonParse((xstrview){ R.sBody, R.iBodySize }) : NULL;
		bool bCertUrl = (pOrderRoot != NULL) &&
			xacmeJsonValueText(pOrderRoot, "certificate", &Certificate);
		if(pOrderRoot != NULL)
		{
			xrtValueRelease(pOrderRoot);
		}
		xacmeHttpResponseUnit(&R);
		if(!bCertUrl)
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_CERTIFICATE,
				"acme issue order missing certificate url");
			goto Done;
		}
	}
	if(!xacmeFlowPostAsGet(pClient, Certificate.sData, &R))
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_CERTIFICATE,
			"acme issue certificate download failed");
		goto Done;
	}
	if((R.iStatus != 200u) || (R.sBody == NULL) || (R.iBodySize == 0u))
	{
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_CERTIFICATE,
			"acme issue certificate response invalid");
		goto Done;
	}
	pOut->sFullchainPem = R.sBody;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	bResult = true;

Done:
	if(pRoot != NULL)
	{
		xrtValueRelease(pRoot);
	}
	xrtBufferUnit(&Payload);
	if(!bResult)
	{
		xrtFree(pOut->sFullchainPem);
		xrtFree(pOut->sKeyPem);
		memset(pOut, 0, sizeof(*pOut));
	}
	return bResult;
}

#endif

#if defined(XACME_FEATURE_ACME_STORE)
bool xacmeClientIssueStored(
	xacmeclient* pClient, const xstrview* pDomains, size_t iDomainCount,
	const struct xacmednsprovider* pDns, cstr sStoreRoot,
	int iRenewalDays, xacmeissuegrant* pOut, bool* pbRenewed)
{
	bool bNeed = true;
	char sPrimary[256];
	if((pClient == NULL) || (pDomains == NULL) || (iDomainCount == 0u) ||
		(pbRenewed == NULL) || (pOut == NULL) ||
		(pDomains[0].Size == 0u) || (pDomains[0].Size >= sizeof(sPrimary)))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme issue stored requires client, domains and outputs");
		return false;
	}
	memset(pOut, 0, sizeof(*pOut));
	*pbRenewed = false;
	memcpy(sPrimary, pDomains[0].Data, pDomains[0].Size);
	sPrimary[pDomains[0].Size] = 0;
	if((sStoreRoot == NULL) || (sStoreRoot[0] == 0u))
	{
		sStoreRoot = ".";
	}
	if(!xrtAcmeStoreNeedRenew(
			sStoreRoot, sPrimary, iRenewalDays, &bNeed))
	{
		return false;
	}
	if(!bNeed)
	{
		return xrtAcmeStoreLoadGrant(sStoreRoot, sPrimary, pOut);
	}
	if(!xacmeClientIssue(pClient, pDomains, iDomainCount, pDns, pOut))
	{
		return false;
	}
	if(!xrtAcmeStoreSaveGrant(
			sStoreRoot, sPrimary, pOut, pClient->sDirectoryUrl))
	{
		/* 落盘失败不作废已签证书；报告错误由调用方权衡。 */
		xacmeFlowError(
			XERR_IO, XACME_FLOW_ERROR_STORE, "acme issue stored save failed");
		xrtAcmeGrantUnit(pOut);
		return false;
	}
	*pbRenewed = true;
	return true;
}
#endif

/* ---------------- 公开客户端 API（xrt/acme_client.h） ---------------- */

#if defined(XACME_FEATURE_ACME_FLOW)

void xrtAcmeClientConfigInit(xacmeclientconfig* pConfig)
{
	if(pConfig == NULL)
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme client config init requires config");
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
}

struct xacmeclient* xrtAcmeClientCreate(
	const xacmeclientconfig* pConfig)
{
	static const char* sDefaults[XACME_FLOW_RESOLVER_MAX] = {
		"223.5.5.5", "119.29.29.29", "8.8.8.8", NULL
	};
	const cstr* sResolvers = NULL;
	size_t iResolverCount = 0u;
	xacmeclient* pClient;
	size_t i;

	if((pConfig == NULL) || (pConfig->pAccount == NULL))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme client create requires config with account");
		return NULL;
	}
	if((pConfig->sPropagateResolvers != NULL) &&
		(pConfig->iPropagateResolverCount != 0u))
	{
		sResolvers = pConfig->sPropagateResolvers;
		iResolverCount = pConfig->iPropagateResolverCount;
	}
	else
	{
		sResolvers = sDefaults;
		iResolverCount = 3u;
	}
	pClient = (xacmeclient*)xrtMalloc(sizeof(*pClient));
	if(pClient == NULL)
	{
		return NULL;
	}
	if(!xacmeClientInit(
			pClient, pConfig->pBorrowedEngine, pConfig->sCaPem,
			pConfig->pAccount, pConfig->uTimeoutUs))
	{
		xrtFree(pClient);
		return NULL;
	}
	for(i = 0; i < iResolverCount; i++)
	{
		if((sResolvers[i] == NULL) ||
			(strlen(sResolvers[i]) >=
				sizeof(pClient->sPropagateResolvers[0])))
		{
			continue;
		}
		strcpy(pClient->sPropagateResolvers[
			pClient->iPropagateResolverCount], sResolvers[i]);
		pClient->iPropagateResolverCount++;
		if(pClient->iPropagateResolverCount >=
			XACME_FLOW_RESOLVER_MAX)
		{
			break;
		}
	}
	pClient->uPropagateTimeoutMs = (pConfig->uPropagateTimeoutMs != 0u) ?
		pConfig->uPropagateTimeoutMs : XACME_FLOW_PROPAGATE_TIMEOUT_MS;
	return pClient;
}

void xrtAcmeClientDestroy(struct xacmeclient* pClient)
{
	if(pClient == NULL)
	{
		return;
	}
	xacmeClientUnit(pClient);
	xrtFree(pClient);
}

str xrtAcmeClientAccountPem(const struct xacmeclient* pClient)
{
	return xacmeClientAccountPem(pClient);
}

bool xrtAcmeClientIssue(
	struct xacmeclient* pClient, const xstrview* pDomains,
	size_t iDomainCount, const xacmednsprovider* pDns,
	xacmeissuegrant* pOut)
{
	return xacmeClientIssue(pClient, pDomains, iDomainCount, pDns, pOut);
}

#endif

#if defined(XACME_FEATURE_ACME_FLOW) && defined(XACME_FEATURE_ACME_STORE)

bool xrtAcmeClientIssueStored(
	struct xacmeclient* pClient, const xstrview* pDomains,
	size_t iDomainCount, const xacmednsprovider* pDns, cstr sStoreRoot,
	int iRenewalDays, xacmeissuegrant* pOut, bool* pbRenewed)
{
	return xacmeClientIssueStored(
		pClient, pDomains, iDomainCount, pDns, sStoreRoot, iRenewalDays,
		pOut, pbRenewed);
}

#endif
