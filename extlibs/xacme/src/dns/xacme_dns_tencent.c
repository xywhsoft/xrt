#include <xrt/acme_dns_tencent.h>

#if defined(XACME_FEATURE_DNS_TENCENT)

#include "../internal/xacme_dnscommon.h"
#include "../internal/xacme_http.h"
#include "../internal/xacme_sigv4.h"

#include <xrt/buffer.h>
#include <xrt/time.h>

#include <stdlib.h>

/*
	腾讯云 DNSPod provider（API 3.0，TC3-HMAC-SHA256）：
	  - canonical：content-type/host/x-tc-action 三头（小写字典序）；
	  - StringToSign = "TC3-HMAC-SHA256\n<ts>\n<date>/dnspod/tc3_request\n"
	    "\n" 后接 sha256hex(canonical)；
	  - 密钥链：HMAC("TC3"+SK, date) → "dnspod" → "tc3_request"；
	  - zone 发现：DescribeRecordList（Domain=候选）2xx 即定；
	  - 加 TXT：CreateRecord（RecordLine 必填 "默认"）；
	  - 删 TXT：DeleteRecord（Domain + RecordId）。
	RecordLine 的 "默认" 是 API 要求的 UTF-8 字面值。
*/

#define XACME_TENCENT_SERVICE "dnspod"
#define XACME_TENCENT_VERSION "2021-03-23"

typedef struct xacmednstencentcontext {
	xacmehttp Http;
	char sId[160];
	char sKey[160];
	char sEndpoint[160];
	xacmednszonecache Zones;
	xacmednsrecords Records;
} xacmednstencentcontext;

/* 执行一次 TC3 调用（POST + JSON body）。 */
static bool xacmeTencentCall(
	xacmednstencentcontext* pCtx, cstr sAction, cstr sBody,
	uint16* pOutStatus, str* pOutBody)
{
	static const char* sSignedHeaders = "content-type;host;x-tc-action";
	char sDateText[24];      /* YYYY-MM-DD */
	char sStampText[24];     /* YYYY-MM-DDTHH:MM:SSZ */
	char sPayloadHash[XACME_SIG_HASH_TEXT];
	char sCanonical[1600];
	char sHeaders[320];
	char sStringToSign[200];
	char sHex[XACME_SIG_HASH_TEXT];
	char sAuth[640];
	char sUrl[240];
	xacmehttpheader Extra[5];
	xacmehttpresponse R;
	xdatetime Now;
	uint8 kService[XRT_SHA256_SIZE];
	uint8 kSigning[XRT_SHA256_SIZE];
	uint8 Signature[XRT_SHA256_SIZE];

	if(!xrtTimeSplitAt(xrtNow(), 0, &Now))
	{
		return false;
	}
	snprintf(sDateText, sizeof(sDateText), "%04ld-%02d-%02d",
		(long)Now.Year, Now.Month, Now.Day);
	snprintf(sStampText, sizeof(sStampText),
		"%04ld-%02d-%02dT%02d:%02d:%02dZ", (long)Now.Year, Now.Month,
		Now.Day, Now.Hour, Now.Minute, Now.Second);
	if(!xacmeSigSha256Hex(sBody, strlen(sBody), sPayloadHash))
	{
		return false;
	}
	snprintf(sHeaders, sizeof(sHeaders),
		"content-type:application/json; charset=utf-8\nhost:%s\n"
		"x-tc-action:%s\n",
		pCtx->sEndpoint, sAction);
	if(!xacmeSigCanonical(sCanonical, sizeof(sCanonical), "POST", "/",
			"", sHeaders, sSignedHeaders, sPayloadHash))
	{
		return false;
	}
	if(!xacmeSigSha256Hex(sCanonical, strlen(sCanonical), sHex))
	{
		return false;
	}
	snprintf(sStringToSign, sizeof(sStringToSign),
		"TC3-HMAC-SHA256\n%s\n%s/" XACME_TENCENT_SERVICE
		"/tc3_request\n%s",
		sStampText, sDateText, sHex);
	{
		char sKeySeed[180];
		snprintf(sKeySeed, sizeof(sKeySeed), "TC3%s", pCtx->sKey);
		if(!xacmeSigHmac((const uint8*)sKeySeed, strlen(sKeySeed),
				sDateText, strlen(sDateText), kService) ||
			!xacmeSigHmac(kService, sizeof(kService),
				XACME_TENCENT_SERVICE, strlen(XACME_TENCENT_SERVICE),
				kService) ||
			!xacmeSigHmac(kService, sizeof(kService), "tc3_request",
				12u, kSigning) ||
			!xacmeSigHmac(kSigning, sizeof(kSigning), sStringToSign,
				strlen(sStringToSign), Signature))
		{
			return false;
		}
	}
	xacmeSigHex(Signature, sizeof(Signature), sHex);
	snprintf(sAuth, sizeof(sAuth),
		"TC3-HMAC-SHA256 Credential=%s/%s/" XACME_TENCENT_SERVICE
		"/tc3_request, SignedHeaders=%s, Signature=%s",
		pCtx->sId, sDateText, sSignedHeaders, sHex);
	snprintf(sUrl, sizeof(sUrl), "https://%s/", pCtx->sEndpoint);

	Extra[0] = (xacmehttpheader){ "Authorization", sAuth };
	Extra[1] = (xacmehttpheader){ "X-TC-Action", sAction };
	Extra[2] = (xacmehttpheader){ "X-TC-Version", XACME_TENCENT_VERSION };
	{
		char sTimestamp[24];
		snprintf(sTimestamp, sizeof(sTimestamp), "%llu",
			(unsigned long long)(xrtNow() / UINT64_C(1000000)));
		Extra[3] = (xacmehttpheader){ "X-TC-Timestamp", sTimestamp };
	}
	if(!xacmeHttpExchangeV(
			&pCtx->Http, "POST", sUrl, "application/json; charset=utf-8",
			(xstrview){ sBody, strlen(sBody) }, Extra, 4u, &R))
	{
		return false;
	}
	*pOutStatus = R.iStatus;
	*pOutBody = R.sBody;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	return true;
}

/* 取响应 JSON 的 Response 成员（对象）。 */
static xvalue* xacmeTencentResponse(str sBody)
{
	xvalue* pRoot = (sBody != NULL) ?
		xrtJsonParse((xstrview){ sBody, strlen(sBody) }) : NULL;
	xvalue* pResponse = (pRoot != NULL) ?
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response")) : NULL;
	if((pResponse == NULL) || !xrtValueIs(pResponse, XVALUE_OBJECT))
	{
		xrtValueRelease(pRoot);
		return NULL;
	}
	return pRoot; /* 调用方经 Root 再取 Response 并释放 Root。 */
}

static bool xacmeTencentAdd(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednstencentcontext* pCtx =
		(xacmednstencentcontext*)pProvider->pContext;
	char sFqdnText[256];
	char sTxtText[208];
	char sRr[200];
	char sZone[256];
	char sZoneStart[256];
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	xvalue* pRoot;
	xvalue* pResponse;
	bool bOk = false;

	if((sFqdn.Size >= sizeof(sFqdnText)) || (sTxt.Size > 200u) ||
		!xacmeDnsSplit((cstr)sFqdn.Data, sRr, sizeof(sRr), sZoneStart,
			sizeof(sZoneStart)))
	{
		return false;
	}
	memcpy(sFqdnText, sFqdn.Data, sFqdn.Size);
	sFqdnText[sFqdn.Size] = '\0';
	memcpy(sTxtText, sTxt.Data, sTxt.Size);
	sTxtText[sTxt.Size] = '\0';

	/* zone 逐级上探：DescribeRecordList 2xx 即该 zone 存在。 */
	{
		const char* sCached = xacmeDnsZoneMatch(&pCtx->Zones, sFqdnText);
		if(sCached != NULL)
		{
			snprintf(sZone, sizeof(sZone), "%s", sCached);
		}
		else
		{
			snprintf(sZone, sizeof(sZone), "%s", sZoneStart);
			for(;;)
			{
				xbuffer Probe;
				xrtBufferInit(&Probe);
				bOk = xrtBufferAppend(
					&Probe, XRT_BYTES_LITERAL("{\"Domain\":")) &&
					xacmeDnsJsonQuote(&Probe,
						(xstrview){ sZone, strlen(sZone) }) &&
					xrtBufferAppend(&Probe, XRT_BYTES_LITERAL("}"));
				if(bOk)
				{
					bOk = xacmeTencentCall(pCtx, "DescribeRecordList",
						(cstr)Probe.Data, &iStatus, &sResp);
				}
				xrtBufferUnit(&Probe);
				if(!bOk)
				{
					return false;
				}
				xrtFree(sResp);
				sResp = NULL;
				if((iStatus >= 200u) && (iStatus < 300u))
				{
					xacmeDnsZoneRemember(&pCtx->Zones, sZone);
					break;
				}
				{
					char* sDot = strchr(sZone, '.');
					if((sDot == NULL) ||
						(strchr(sDot + 1, '.') == NULL))
					{
						return false; /* 剩两段仍失败：放弃 */
					}
					memmove(sZone, sDot + 1, strlen(sDot + 1) + 1u);
				}
			}
		}
	}

	/* CreateRecord；RR = FQDN 去 ".zone"。 */
	{
		size_t iZoneLen = strlen(sZone);
		size_t iFqdnLen = strlen(sFqdnText);
		size_t iRrLen;
		if((iFqdnLen <= iZoneLen + 1u) ||
			(strcmp(sFqdnText + iFqdnLen - iZoneLen, sZone) != 0) ||
			(sFqdnText[iFqdnLen - iZoneLen - 1u] != '.'))
		{
			return false;
		}
		iRrLen = iFqdnLen - iZoneLen - 1u;
		if(iRrLen >= sizeof(sRr))
		{
			return false;
		}
		memcpy(sRr, sFqdnText, iRrLen);
		sRr[iRrLen] = '\0';
	}
	xrtBufferInit(&Body);
	bOk = xrtBufferAppend(&Body, XRT_BYTES_LITERAL("{\"Domain\":")) &&
		xacmeDnsJsonQuote(&Body, (xstrview){ sZone, strlen(sZone) }) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL(",\"SubDomain\":")) &&
		xacmeDnsJsonQuote(&Body, (xstrview){ sRr, strlen(sRr) }) &&
		xrtBufferAppend(&Body,
			XRT_BYTES_LITERAL(",\"RecordType\":\"TXT\","
				"\"RecordLine\":\"默认\",\"Value\":")) &&
		xacmeDnsJsonQuote(&Body, (xstrview){ sTxtText, strlen(sTxtText) }) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL("}"));
	if(bOk)
	{
		bOk = xacmeTencentCall(pCtx, "CreateRecord", (cstr)Body.Data,
			&iStatus, &sResp);
	}
	xrtBufferUnit(&Body);
	if(!bOk || (iStatus < 200u) || (iStatus >= 300u))
	{
		xrtFree(sResp);
		return false;
	}
	/* RecordId 记档（可能为数值，按文本取）。 */
	pRoot = xacmeTencentResponse(sResp);
	if(pRoot != NULL)
	{
		xvalue* pMember;
		pResponse = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response"));
		pMember = (pResponse != NULL) ?
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) : NULL;
		if(pMember != NULL)
		{
			int64 iId = 0;
			if(xrtValueGetInt(pMember, &iId))
			{
				char sHandle[64];
				snprintf(sHandle, sizeof(sHandle), "%.20s|%lld", sZone,
					(long long)iId);
				xacmeDnsRecordRemember(&pCtx->Records, sHandle);
			}
		}
		xrtValueRelease(pRoot);
	}
	xrtFree(sResp);
	return true;
}

static bool xacmeTencentRemove(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednstencentcontext* pCtx =
		(xacmednstencentcontext*)pProvider->pContext;
	size_t i;
	bool bAnyOk = false;
	(void)sTxt;
	for(i = 0; i < pCtx->Records.iCount; i++)
	{
		char sZone[256];
		char sRecordId[32];
		xbuffer Body;
		uint16 iStatus = 0u;
		str sResp = NULL;
		const char* sBar;
		if(pCtx->Records.sIds[i][0] == '\0')
		{
			continue;
		}
		sBar = strchr(pCtx->Records.sIds[i], '|');
		if((sBar == NULL) ||
			((size_t)(sBar - pCtx->Records.sIds[i]) >= sizeof(sZone)) ||
			(strlen(sBar + 1) >= sizeof(sRecordId)))
		{
			continue;
		}
		memcpy(sZone, pCtx->Records.sIds[i],
			(size_t)(sBar - pCtx->Records.sIds[i]));
		sZone[sBar - pCtx->Records.sIds[i]] = '\0';
		snprintf(sRecordId, sizeof(sRecordId), "%s", sBar + 1);
		xrtBufferInit(&Body);
		if(xrtBufferAppend(&Body, XRT_BYTES_LITERAL("{\"Domain\":")) &&
			xacmeDnsJsonQuote(&Body, (xstrview){ sZone, strlen(sZone) }) &&
			xrtBufferAppend(&Body, XRT_BYTES_LITERAL(",\"RecordId\":")) &&
			xrtBufferAppend(&Body,
				(xbytesview){ (const uint8*)sRecordId,
					strlen(sRecordId) }) &&
			xrtBufferAppend(&Body, XRT_BYTES_LITERAL("}")))
		{
			if(xacmeTencentCall(pCtx, "DeleteRecord", (cstr)Body.Data,
					&iStatus, &sResp) &&
				(((iStatus >= 200u) && (iStatus < 300u)) ||
					(iStatus == 400u)))
			{
				bAnyOk = true;
				pCtx->Records.sIds[i][0] = '\0';
			}
		}
		xrtBufferUnit(&Body);
		xrtFree(sResp);
	}
	(void)sFqdn;
	return bAnyOk || (pCtx->Records.iCount == 0u);
}

void xrtAcmeDnsTencentConfigInit(xacmednstencentconfig* pConfig)
{
	if(pConfig == NULL)
	{
		return;
	}
	pConfig->sSecretId = NULL;
	pConfig->sSecretKey = NULL;
	pConfig->sEndpoint = NULL;
}

bool xrtAcmeDnsTencent(
	const xacmednstencentconfig* pConfig,
	struct xnetengine* pBorrowedEngine, xacmednsprovider* pProvider)
{
	xacmednstencentcontext* pCtx;
	if((pConfig == NULL) || (pProvider == NULL) ||
		(pConfig->sSecretId == NULL) || (pConfig->sSecretKey == NULL) ||
		(pConfig->sSecretId[0] == '\0') ||
		(pConfig->sSecretKey[0] == '\0'))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_tencent requires secret id and key");
		return false;
	}
	pCtx = (xacmednstencentcontext*)xrtCalloc(1, sizeof(*pCtx));
	if(pCtx == NULL)
	{
		return false;
	}
	snprintf(pCtx->sId, sizeof(pCtx->sId), "%s", pConfig->sSecretId);
	snprintf(pCtx->sKey, sizeof(pCtx->sKey), "%s", pConfig->sSecretKey);
	snprintf(pCtx->sEndpoint, sizeof(pCtx->sEndpoint), "%s",
		(pConfig->sEndpoint != NULL) ? pConfig->sEndpoint :
			"dnspod.tencentcloudapi.com");
	if(!xacmeHttpInit(&pCtx->Http, pBorrowedEngine, NULL, 0u))
	{
		xacmeHttpUnit(&pCtx->Http);
		xrtFree(pCtx);
		return false;
	}
	pProvider->sId = "tencent";
	pProvider->iCaps = 0u;
	pProvider->pContext = pCtx;
	pProvider->Add = xacmeTencentAdd;
	pProvider->Remove = xacmeTencentRemove;
	pProvider->Propagate = NULL;
	return true;
}

void xrtAcmeDnsTencentProviderUnit(xacmednsprovider* pProvider)
{
	if((pProvider != NULL) && (pProvider->pContext != NULL))
	{
		xacmednstencentcontext* pCtx =
			(xacmednstencentcontext*)pProvider->pContext;
		xacmeHttpUnit(&pCtx->Http);
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
