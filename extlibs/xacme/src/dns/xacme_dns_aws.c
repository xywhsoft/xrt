#include <xrt/acme_dns_aws.h>

#if defined(XACME_FEATURE_DNS_AWS)

#include "../internal/xacme_dnscommon.h"
#include "../internal/xacme_http.h"
#include "../internal/xacme_sigv4.h"

#include <xrt/buffer.h>
#include <xrt/time.h>

#include <stdlib.h>

/*
	AWS Route53 provider（SigV4 + XML API 2013-03-01）：
	  - canonical：host/x-amz-content-sha256/x-amz-date 三头（字典序）；
	  - StringToSign = "AWS4-HMAC-SHA256\n<x-amz-date>\n"
	    "<date>/<region>/route53/aws4_request\n<sha256hex(canonical)>"；
	  - 密钥链：HMAC("AWS4"+SK, date) → region → "route53" →
	    "aws4_request"；payload hash 走 x-amz-content-sha256 头；
	  - zone 发现：GET /hostedzonesbyname?dnsname=<候选>（逐级上探）；
	  - 加 TXT：ChangeResourceRecordSets UPSERT（Value 必须带双引号）；
	  - 删 TXT：同 API 的 DELETE Action（携带与创建相同的值集）；
	  - Change 异步但权威侧近即时，不轮询 INSYNC。
	记录句柄无服务端 id，Remove 用 "zoneid|fqdn|value" 三元组回放删除。
*/

#define XACME_AWS_SERVICE "route53"
#define XACME_AWS_API "2013-04-01"

typedef struct xacmednsawscontext {
	xacmehttp Http;
	char sId[160];
	char sKey[160];
	char sRegion[32];
	char sEndpoint[160];
	xacmednszonecache Zones;
	/* 记录句柄：zone id + fqdn + 值（删除需完整回放）。 */
	struct
	{
		char sZoneId[64];
		char sFqdn[256];
		char sValue[64];
	} Records[XACME_DNS_RECORD_MAX];
	size_t iRecordCount;
} xacmednsawscontext;

/* 执行一次 SigV4 调用（sBody 为 XML 或 NULL）。 */
static bool xacmeAwsCall(
	xacmednsawscontext* pCtx, cstr sMethod, cstr sPathAndQuery,
	cstr sContentType, cstr sBody, uint16* pOutStatus, str* pOutBody)
{
	static const char* sSignedHeaders =
		"host;x-amz-content-sha256;x-amz-date";
	char sDateText[16];      /* YYYYMMDD */
	char sStampText[24];     /* YYYYMMDDTHHMMSSZ */
	char sPayloadHash[XACME_SIG_HASH_TEXT];
	char sCanonical[2048];
	char sHeaders[360];
	char sStringToSign[240];
	char sHex[XACME_SIG_HASH_TEXT];
	char sAuth[640];
	char sUrl[400];
	xacmehttpheader Extra[3];
	xacmehttpresponse R;
	xdatetime Now;
	uint8 kDate[XRT_SHA256_SIZE];
	uint8 kRegion[XRT_SHA256_SIZE];
	uint8 kService[XRT_SHA256_SIZE];
	uint8 kSigning[XRT_SHA256_SIZE];
	uint8 Signature[XRT_SHA256_SIZE];

	if(!xrtTimeSplitAt(xrtNow(), 0, &Now))
	{
		return false;
	}
	snprintf(sDateText, sizeof(sDateText), "%04ld%02d%02d", (long)Now.Year,
		Now.Month, Now.Day);
	snprintf(sStampText, sizeof(sStampText),
		"%04ld%02d%02dT%02d%02d%02dZ", (long)Now.Year, Now.Month, Now.Day,
		Now.Hour, Now.Minute, Now.Second);
	if(!xacmeSigSha256Hex(
			(sBody != NULL) ? sBody : "",
			(sBody != NULL) ? strlen(sBody) : 0u, sPayloadHash))
	{
		return false;
	}
	snprintf(sHeaders, sizeof(sHeaders),
		"host:%s\nx-amz-content-sha256:%s\nx-amz-date:%s\n",
		pCtx->sEndpoint, sPayloadHash, sStampText);
	if(!xacmeSigCanonical(sCanonical, sizeof(sCanonical), sMethod,
			sPathAndQuery, "", sHeaders, sSignedHeaders, sPayloadHash))
	{
		return false;
	}
	if(!xacmeSigSha256Hex(sCanonical, strlen(sCanonical), sHex))
	{
		return false;
	}
	snprintf(sStringToSign, sizeof(sStringToSign),
		"AWS4-HMAC-SHA256\n%s\n%s/%s/" XACME_AWS_SERVICE
		"/aws4_request\n%s",
		sStampText, sDateText, pCtx->sRegion, sHex);
	{
		char sKeySeed[180];
		snprintf(sKeySeed, sizeof(sKeySeed), "AWS4%s", pCtx->sKey);
		if(!xacmeSigHmac((const uint8*)sKeySeed, strlen(sKeySeed),
				sDateText, strlen(sDateText), kDate) ||
			!xacmeSigHmac(kDate, sizeof(kDate), pCtx->sRegion,
				strlen(pCtx->sRegion), kRegion) ||
			!xacmeSigHmac(kRegion, sizeof(kRegion), XACME_AWS_SERVICE,
				strlen(XACME_AWS_SERVICE), kService) ||
			!xacmeSigHmac(kService, sizeof(kService), "aws4_request",
				13u, kSigning) ||
			!xacmeSigHmac(kSigning, sizeof(kSigning), sStringToSign,
				strlen(sStringToSign), Signature))
		{
			return false;
		}
	}
	xacmeSigHex(Signature, sizeof(Signature), sHex);
	snprintf(sAuth, sizeof(sAuth),
		"AWS4-HMAC-SHA256 Credential=%s/%s/%s/" XACME_AWS_SERVICE
		"/aws4_request, SignedHeaders=%s, Signature=%s",
		pCtx->sId, sDateText, pCtx->sRegion, sSignedHeaders, sHex);
	snprintf(sUrl, sizeof(sUrl), "https://%s%s", pCtx->sEndpoint,
		sPathAndQuery);

	Extra[0] = (xacmehttpheader){ "Authorization", sAuth };
	Extra[1] = (xacmehttpheader){ "x-amz-content-sha256", sPayloadHash };
	Extra[2] = (xacmehttpheader){ "x-amz-date", sStampText };
	if(!xacmeHttpExchangeV(
			&pCtx->Http, sMethod, sUrl,
			(sContentType != NULL) ? sContentType : "application/xml",
			(xstrview){ sBody, (sBody != NULL) ? strlen(sBody) : 0u },
			Extra, 3u, &R))
	{
		return false;
	}
	*pOutStatus = R.iStatus;
	*pOutBody = R.sBody;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	return true;
}

/* 从 XML 文本提取首个 <Tag>…</Tag> 内容到固定缓冲。 */
static bool xacmeAwsXmlText(
	cstr sBody, cstr sTag, char* sOut, size_t iCapacity)
{
	char sOpen[64];
	char sClose[64];
	const char* pBegin;
	const char* pEnd;
	snprintf(sOpen, sizeof(sOpen), "<%s>", sTag);
	snprintf(sClose, sizeof(sClose), "</%s>", sTag);
	pBegin = (sBody != NULL) ? strstr(sBody, sOpen) : NULL;
	if(pBegin == NULL)
	{
		return false;
	}
	pBegin += strlen(sOpen);
	pEnd = strstr(pBegin, sClose);
	if((pEnd == NULL) || ((size_t)(pEnd - pBegin) >= iCapacity))
	{
		return false;
	}
	memcpy(sOut, pBegin, (size_t)(pEnd - pBegin));
	sOut[pEnd - pBegin] = '\0';
	return true;
}

/* hostedzonesbyname 精确匹配候选 zone（返回 hostedzone id）。 */
static bool xacmeAwsZoneId(
	xacmednsawscontext* pCtx, cstr sZone, char* sOutId, size_t iIdCap)
{
	char sPath[300];
	char sZoneName[300];
	uint16 iStatus = 0u;
	str sBody = NULL;
	bool bOk = false;

	/* Zone 名在 Route53 中带尾点。 */
	snprintf(sZoneName, sizeof(sZoneName), "%s.", sZone);
	snprintf(sPath, sizeof(sPath), "/" XACME_AWS_API
		"/hostedzonesbyname?dnsname=%.250s&maxitems=1", sZoneName);
	if(!xacmeAwsCall(pCtx, "GET", sPath, NULL, NULL, &iStatus, &sBody))
	{
		return false;
	}
	if((iStatus >= 200u) && (iStatus < 300u) && (sBody != NULL))
	{
		/* 标签配对限定在首个 <HostedZone> 块内，避免后续
		   块/分页残留的同名标签串扰。 */
		const char* pBlock = strstr(sBody, "<HostedZone>");
		const char* pBlockEnd = (pBlock != NULL) ?
			strstr(pBlock, "</HostedZone>") : NULL;
		char sScoped[1024];
		cstr sScope = sBody;
		if((pBlock != NULL) && (pBlockEnd != NULL))
		{
			size_t iLen = (size_t)(pBlockEnd - pBlock);
			if(iLen >= sizeof(sScoped))
			{
				iLen = sizeof(sScoped) - 1u;
			}
			memcpy(sScoped, pBlock, iLen);
			sScoped[iLen] = 0;
			sScope = sScoped;
		}
		{
			char sName[300];
			if(xacmeAwsXmlText(sScope, "Name", sName, sizeof(sName)) &&
				(strcmp(sName, sZoneName) == 0) &&
				xacmeAwsXmlText(sScope, "Id", sOutId, iIdCap))
			{
				/* Id 形如 /hostedzone/Z1234；API 调用用裸 Z id。 */
				const char* pSlash = strrchr(sOutId, '/');
				if(pSlash != NULL)
				{
					memmove(sOutId, pSlash + 1, strlen(pSlash + 1) + 1u);
				}
				bOk = true;
			}
		}
	}
	xrtFree(sBody);
	return bOk;
}

static bool xacmeAwsFindZone(
	xacmednsawscontext* pCtx, cstr sFqdn, char* sOutZone, size_t iZoneCap,
	char* sOutId, size_t iIdCap)
{
	char sCandidate[256];
	const char* sCached = xacmeDnsZoneMatch(&pCtx->Zones, sFqdn);
	if(sCached != NULL)
	{
		snprintf(sOutZone, iZoneCap, "%s", sCached);
		return xacmeAwsZoneId(pCtx, sCached, sOutId, iIdCap);
	}
	snprintf(sCandidate, sizeof(sCandidate), "%s", sFqdn);
	for(;;)
	{
		char sRr[200];
		char sZone[256];
		if(!xacmeDnsSplit(sCandidate, sRr, sizeof(sRr), sZone,
				sizeof(sZone)))
		{
			return false;
		}
		if(xacmeAwsZoneId(pCtx, sZone, sOutId, iIdCap))
		{
			snprintf(sOutZone, iZoneCap, "%s", sZone);
			xacmeDnsZoneRemember(&pCtx->Zones, sZone);
			return true;
		}
		snprintf(sCandidate, sizeof(sCandidate), "%s", sZone);
	}
}

/* ChangeResourceRecordSets（UPSERT 或 DELETE）。 */
static bool xacmeAwsChange(
	xacmednsawscontext* pCtx, cstr sAction, cstr sZoneId, cstr sFqdn,
	cstr sValue)
{
	char sBody[640];
	char sPath[128];
	uint16 iStatus = 0u;
	str sResp = NULL;
	bool bOk;
	snprintf(sBody, sizeof(sBody),
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<ChangeResourceRecordSetsRequest xmlns=\"https://route53."
		"amazonaws.com/doc/" XACME_AWS_API "/\">"
		"<ChangeBatch><Changes><Change><Action>%s</Action>"
		"<ResourceRecordSet><Name>%s.</Name><Type>TXT</Type>"
		"<TTL>60</TTL><ResourceRecords><ResourceRecord>"
		"<Value>\"%s\"</Value></ResourceRecord></ResourceRecords>"
		"</ResourceRecordSet></Change></Changes></ChangeBatch>"
		"</ChangeResourceRecordSetsRequest>",
		sAction, sFqdn, sValue);
	snprintf(sPath, sizeof(sPath), "/" XACME_AWS_API "/hostedzone/%s/rrset/",
		sZoneId);
	bOk = xacmeAwsCall(pCtx, "POST", sPath, "application/xml", sBody,
		&iStatus, &sResp);
	xrtFree(sResp);
	return bOk && (iStatus >= 200u) && (iStatus < 300u);
}

static bool xacmeAwsAdd(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsawscontext* pCtx = (xacmednsawscontext*)pProvider->pContext;
	char sFqdnText[256];
	char sTxtText[208];
	char sZone[256];
	char sZoneId[64];
	if((sFqdn.Size >= sizeof(sFqdnText)) || (sTxt.Size > 200u))
	{
		return false;
	}
	memcpy(sFqdnText, sFqdn.Data, sFqdn.Size);
	sFqdnText[sFqdn.Size] = '\0';
	memcpy(sTxtText, sTxt.Data, sTxt.Size);
	sTxtText[sTxt.Size] = '\0';

	if(!xacmeAwsFindZone(pCtx, sFqdnText, sZone, sizeof(sZone), sZoneId,
			sizeof(sZoneId)))
	{
		return false;
	}
	if(!xacmeAwsChange(pCtx, "UPSERT", sZoneId, sFqdnText, sTxtText))
	{
		return false;
	}
	if(pCtx->iRecordCount < XACME_DNS_RECORD_MAX)
	{
		snprintf(pCtx->Records[pCtx->iRecordCount].sZoneId,
			sizeof(pCtx->Records[pCtx->iRecordCount].sZoneId), "%.60s",
			sZoneId);
		snprintf(pCtx->Records[pCtx->iRecordCount].sFqdn,
			sizeof(pCtx->Records[pCtx->iRecordCount].sFqdn), "%s",
			sFqdnText);
		snprintf(pCtx->Records[pCtx->iRecordCount].sValue,
			sizeof(pCtx->Records[pCtx->iRecordCount].sValue), "%.63s",
			sTxtText);
		pCtx->iRecordCount++;
	}
	return true;
}

static bool xacmeAwsRemove(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsawscontext* pCtx = (xacmednsawscontext*)pProvider->pContext;
	size_t i;
	bool bAnyOk = false;
	(void)sFqdn;
	(void)sTxt;
	for(i = 0; i < pCtx->iRecordCount; i++)
	{
		if(pCtx->Records[i].sFqdn[0] == '\0')
		{
			continue;
		}
		if(xacmeAwsChange(pCtx, "DELETE", pCtx->Records[i].sZoneId,
				pCtx->Records[i].sFqdn, pCtx->Records[i].sValue))
		{
			bAnyOk = true;
			pCtx->Records[i].sFqdn[0] = '\0';
		}
	}
	return bAnyOk || (pCtx->iRecordCount == 0u);
}

void xrtAcmeDnsAwsConfigInit(xacmednsawsconfig* pConfig)
{
	if(pConfig == NULL)
	{
		return;
	}
	pConfig->sAccessKeyId = NULL;
	pConfig->sSecretAccessKey = NULL;
	pConfig->sRegion = NULL;
	pConfig->sEndpoint = NULL;
}

bool xrtAcmeDnsAws(
	const xacmednsawsconfig* pConfig, struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider)
{
	xacmednsawscontext* pCtx;
	if((pConfig == NULL) || (pProvider == NULL) ||
		(pConfig->sAccessKeyId == NULL) ||
		(pConfig->sSecretAccessKey == NULL) ||
		(pConfig->sAccessKeyId[0] == '\0') ||
		(pConfig->sSecretAccessKey[0] == '\0'))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_aws requires access key id and secret");
		return false;
	}
	pCtx = (xacmednsawscontext*)xrtCalloc(1, sizeof(*pCtx));
	if(pCtx == NULL)
	{
		return false;
	}
	snprintf(pCtx->sId, sizeof(pCtx->sId), "%s", pConfig->sAccessKeyId);
	snprintf(pCtx->sKey, sizeof(pCtx->sKey), "%s",
		pConfig->sSecretAccessKey);
	snprintf(pCtx->sRegion, sizeof(pCtx->sRegion), "%s",
		((pConfig->sRegion != NULL) && (pConfig->sRegion[0] != '\0')) ?
			pConfig->sRegion : "us-east-1");
	snprintf(pCtx->sEndpoint, sizeof(pCtx->sEndpoint), "%s",
		(pConfig->sEndpoint != NULL) ? pConfig->sEndpoint :
			"route53.amazonaws.com");
	if(!xacmeHttpInit(&pCtx->Http, pBorrowedEngine, NULL, 0u))
	{
		xacmeHttpUnit(&pCtx->Http);
		xrtFree(pCtx);
		return false;
	}
	pProvider->sId = "aws";
	pProvider->iCaps = 0u;
	pProvider->pContext = pCtx;
	pProvider->Add = xacmeAwsAdd;
	pProvider->Remove = xacmeAwsRemove;
	pProvider->Propagate = NULL;
	return true;
}

void xrtAcmeDnsAwsProviderUnit(xacmednsprovider* pProvider)
{
	if((pProvider != NULL) && (pProvider->pContext != NULL))
	{
		xacmednsawscontext* pCtx = (xacmednsawscontext*)pProvider->pContext;
		xacmeHttpUnit(&pCtx->Http);
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
