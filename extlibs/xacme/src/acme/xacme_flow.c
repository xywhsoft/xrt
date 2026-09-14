#include "../internal/xacme_flow.h"

#if defined(XACME_FEATURE_ACME_FLOW)

#include <xrt/acme_dns.h>
#include <xrt/codec.h>
#include <xrt/json.h>
#include <xrt/time.h>
#include <xrt/value.h>

#include <stdio.h>
#include <stdlib.h>
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

/* ---------------- 初始化与签发 ---------------- */

bool xacmeClientInit(
	xacmeclient* pClient, struct xnetengine* pBorrowedEngine,
	cstr sCaPem, cstr sDirectoryUrl, cstr sAccountKeyPem)
{
	xacmehttpresponse R;
	xvalue* pRoot = NULL;
	xacmeflowurl NewNonce;
	xacmeflowurl NewAccount;
	xacmeflowurl NewOrder;
	xbuffer Payload;
	bool bOk = false;

	if((pClient == NULL) || (sDirectoryUrl == NULL) ||
		(sDirectoryUrl[0] == '\0'))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme client init requires client and directory url");
		return false;
	}
	memset(pClient, 0, sizeof(*pClient));
	if(!xacmeHttpInit(&pClient->Http, pBorrowedEngine, sCaPem, 0u))
	{
		goto Done;
	}
	if((sAccountKeyPem != NULL) && (sAccountKeyPem[0] != '\0'))
	{
		if(!xacmeKeyPemRead(
			sAccountKeyPem, strlen(sAccountKeyPem), &pClient->AccountKey))
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
		&pClient->Http, "GET", sDirectoryUrl, NULL,
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

	/* 注册或复用账户：201=新建，200=已存在。 */
	xrtBufferInit(&Payload);
	if(!xrtBufferAppend(
		&Payload, XRT_BYTES_LITERAL("{\"termsOfServiceAgreed\":true}")))
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

str xacmeClientIssue(
	xacmeclient* pClient, const xstrview* pDomains, size_t iDomainCount,
	const struct xacmednsprovider* pDns)
{
	xbuffer Payload;
	xacmehttpresponse R;
	xvalue* pRoot = NULL;
	str sResult = NULL;
	xacmeflowurl Finalize;
	xacmeflowurl OrderUrl;
	Finalize.sData[0] = 0;
	xacmeflowurl Certificate;
	size_t i;
	bool bOk = false;

	if((pClient == NULL) || (pDomains == NULL) || (iDomainCount == 0u) ||
		(pDns == NULL) || (pDns->Add == NULL) || (pDns->Remove == NULL) ||
		!xrtAcmeDnsProviderValidate(pDns))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme issue requires client, domains and dns provider");
		return NULL;
	}

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
		(Authz.sData)[UrlText.Size] = '\0';
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
					"acme authz fetch status=%u body=%.1500s",
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
							xrtFree(sStatus);
						}
						/* TXT 记录使命完成，删除。 */
						(void)pDns->Remove((xacmednsprovider*)pDns,
							(xstrview){ sFqdn, strlen(sFqdn) },
							(xstrview){ sTxt, strlen(sTxt) });
						xrtFree(sFqdn);
						xrtFree(sTxt);
						if(!bValidNow)
						{
							const xerror* pE = xrtGetError();
							if(getenv("XACME_DEBUG"))
							{
								xacmehttpresponse Dbg;
								if(xacmeFlowPostAsGet(
									pClient, Authz.sData, &Dbg) &&
									(Dbg.sBody != NULL))
								{
									printf("[dbg] authz body: %.1400s\n",
										Dbg.sBody);
								}
								xacmeHttpResponseUnit(&Dbg);
								printf(
									"[dbg] authz not valid: status=%s err=%d %s\n",
									(sStatus != NULL) ? sStatus : "null",
									(int)xrtErrorKind(pE),
									xrtErrorMessage(pE) ?
										xrtErrorMessage(pE) : "-");
							}
							xrtValueRelease(pAuthRoot);
							xacmeFlowError(
								XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
								"acme issue authorization not valid");
							goto Done;
						}
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
	sResult = R.sBody;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	bOk = true;

Done:
	if(pRoot != NULL)
	{
		xrtValueRelease(pRoot);
	}
	xrtBufferUnit(&Payload);
	return sResult;
}

#endif

#if defined(XACME_FEATURE_ACME_STORE)
str xacmeClientIssueStored(
	xacmeclient* pClient, const xstrview* pDomains, size_t iDomainCount,
	const struct xacmednsprovider* pDns, cstr sStoreRoot,
	int iRenewalDays, bool* pbRenewed)
{
	bool bNeed = true;
	str sChain;
	char sPrimary[256];
	if((pClient == NULL) || (pDomains == NULL) || (iDomainCount == 0u) ||
		(pbRenewed == NULL) || (pDomains[0].Size >= sizeof(sPrimary)))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme issue stored requires client, domains and outputs");
		return NULL;
	}
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
		return NULL;
	}
	if(!bNeed)
	{
		return xrtAcmeStoreLoadCert(sStoreRoot, sPrimary);
	}
	sChain = xacmeClientIssue(pClient, pDomains, iDomainCount, pDns);
	if(sChain == NULL)
	{
		return NULL;
	}
	if(!xrtAcmeStoreSaveCert(sStoreRoot, sPrimary, sChain, NULL))
	{
		/* 落盘失败不作废已签证书；报告错误由调用方权衡。 */
		xacmeFlowError(
			XERR_IO, XACME_FLOW_ERROR_STORE, "acme issue stored save failed");
		xrtFree(sChain);
		return NULL;
	}
	*pbRenewed = true;
	return sChain;
}
#endif
