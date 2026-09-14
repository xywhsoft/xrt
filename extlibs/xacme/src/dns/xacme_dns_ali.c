#include <xrt/acme_dns_ali.h>

#if defined(XACME_FEATURE_DNS_ALI)

#include "../../src/internal/xacme_dnstxt.h"
#include "../../src/internal/xacme_http.h"

#include <xrt/codec.h>
#include <xrt/crypto.h>
#include <xrt/json.h>
#include <xrt/time.h>
#include <xrt/value.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>

#define XACME_ALI_VERSION "2015-01-09"
#define XACME_ALI_RECORD_MAX 8u

typedef struct xacmednsalicontext {
	xacmehttp Http;
	xacmedns Verify;
	char sKeyId[160];
	char sSecret[160];
	char sEndpoint[160];
	char sVerifyResolver[64];
	/* 缓存的 zone（首次 Add 时试探得到）。 */
	char sZone[256];
	/* 本 provider 生命周期内添加的 RecordId。 */
	char sRecordIds[XACME_ALI_RECORD_MAX][64];
	size_t iRecordCount;
} xacmednsalicontext;

static void xacmeAliHex(const uint8* pData, size_t iSize, char* sOut)
{
	size_t i;
	for(i = 0; i < iSize; i++)
	{
		sprintf(sOut + i * 2u, "%02x", pData[i]);
	}
	sOut[iSize * 2u] = '\0';
}

/* UTC ISO8601 秒精度："YYYY-MM-DDTHH:MM:SSZ"。 */
static bool xacmeAliDate(char* sOut)
{
	xdatetime Now;
	xtime t = xrtNow();
	if(!xrtTimeSplitAt(t, 0, &Now))
	{
		return false;
	}
	snprintf(
		sOut, 21u, "%04ld-%02d-%02dT%02d:%02d:%02dZ",
		(long)Now.Year, Now.Month, Now.Day, Now.Hour, Now.Minute,
		Now.Second);
	return true;
}

/* 从完整 FQDN 拆出 RR 与 zone 候选（去最左标签逐级）。 */
static bool xacmeAliSplit(
	cstr sFqdn, char* sRr, size_t iRrCap, char* sZone, size_t iZoneCap)
{
	const char* sDot;
	size_t iLen = strlen(sFqdn);
	if((iLen == 0u) || (iLen >= 512u))
	{
		return false;
	}
	sDot = strchr(sFqdn, '.');
	if((sDot == NULL) || (sDot == sFqdn) ||
		((size_t)(sDot - sFqdn) >= iRrCap))
	{
		return false;
	}
	memcpy(sRr, sFqdn, (size_t)(sDot - sFqdn));
	sRr[sDot - sFqdn] = '\0';
	if((iLen - (size_t)(sDot - sFqdn) - 1u) >= iZoneCap)
	{
		return false;
	}
	strcpy(sZone, sDot + 1);
	return true;
}

/*
	执行一次 alidns V3 调用（POST + JSON body）。
	成功返回响应体（xrtMalloc，含状态）；HTTP 非 2xx 时也返回 true
	并把状态/体交给调用方判断（如 zone 试探要看 4xx）。
*/
static bool xacmeAliCall(
	xacmednsalicontext* pCtx, cstr sAction, cstr sQuery,
	uint16* pOutStatus, str* pOutBody)
{
	char sDate[24];
	char sPayloadHash[72];
	char sCanonical[1600];
	char sStringToSign[160];
	char sHex[72];
	uint8 Digest[XRT_SHA256_SIZE];
	uint8 Mac[XRT_SHA256_SIZE];
	char sAuth[640];
	char sUrl[700];
	xacmehttpheader Extra[5];
	xacmehttpresponse R;
	static const char* sSignedHeaders =
		"content-type;host;x-acs-action;x-acs-content-sha256;x-acs-date;"
		"x-acs-version";

	if(!xacmeAliDate(sDate))
	{
		return false;
	}
	/* V3 RPC：参数在 query，body 为空；content-sha256 = sha256("")。 */
	if(!xrtSha256("", 0u, Digest))
	{
		return false;
	}
	xacmeAliHex(Digest, sizeof(Digest), sPayloadHash);

	snprintf(
		sCanonical, sizeof(sCanonical),
		"POST\n/\n%s\n"
		"content-type:application/json\n"
		"host:%s\n"
		"x-acs-action:%s\n"
		"x-acs-content-sha256:%s\n"
		"x-acs-date:%s\n"
		"x-acs-version:%s\n"
		"\n%s\n%s",
		sQuery, pCtx->sEndpoint, sAction, sPayloadHash, sDate,
		XACME_ALI_VERSION, sSignedHeaders, sPayloadHash);
	if(!xrtSha256(sCanonical, strlen(sCanonical), Digest))
	{
		return false;
	}
	xacmeAliHex(Digest, sizeof(Digest), sHex);
	snprintf(sStringToSign, sizeof(sStringToSign),
		"ACS3-HMAC-SHA256\n%s", sHex);
	if(!xrtHmacSha256(
			pCtx->sSecret, strlen(pCtx->sSecret),
			sStringToSign, strlen(sStringToSign), Mac))
	{
		return false;
	}
	xacmeAliHex(Mac, sizeof(Mac), sHex);
	snprintf(sAuth, sizeof(sAuth),
		"ACS3-HMAC-SHA256 Credential=%s, SignedHeaders=%s, "
		"Signature=%s",
		pCtx->sKeyId, sSignedHeaders, sHex);
	if(sQuery[0] != 0u)
	{
		snprintf(sUrl, sizeof(sUrl), "https://%s/?%s", pCtx->sEndpoint,
			sQuery);
	}
	else
	{
		snprintf(sUrl, sizeof(sUrl), "https://%s/", pCtx->sEndpoint);
	}

	Extra[0] = (xacmehttpheader){ "Authorization", sAuth };
	Extra[1] = (xacmehttpheader){ "x-acs-action", sAction };
	Extra[2] = (xacmehttpheader){ "x-acs-content-sha256", sPayloadHash };
	Extra[3] = (xacmehttpheader){ "x-acs-date", sDate };
	Extra[4] = (xacmehttpheader){ "x-acs-version", XACME_ALI_VERSION };

	if(!xacmeHttpExchangeV(
		&pCtx->Http, "POST", sUrl, "application/json",
		(xstrview){ "", 0u }, Extra, 5u, &R))
	{
		return false;
	}
	*pOutStatus = R.iStatus;
	*pOutBody = R.sBody;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	return true;
}

/* 在响应 JSON 里取 RecordId 并存档。 */
static void xacmeAliSaveRecordId(xacmednsalicontext* pCtx, cstr sBody)
{
	xvalue* pRoot = (sBody != NULL) ?
		xrtJsonParse((xstrview){ sBody, strlen(sBody) }) : NULL;
	xvalue* pId = (pRoot != NULL) ?
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("RecordId")) : NULL;
	xstrview Text;
	if((pId != NULL) && xrtValueGetString(pId, &Text) &&
		(pCtx->iRecordCount < XACME_ALI_RECORD_MAX) &&
		(Text.Size < 63u))
	{
		memcpy(
			pCtx->sRecordIds[pCtx->iRecordCount], Text.Data, Text.Size);
		pCtx->sRecordIds[pCtx->iRecordCount][Text.Size] = '\0';
		pCtx->iRecordCount++;
	}
	if(pRoot != NULL)
	{
		xrtValueRelease(pRoot);
	}
}

/* 试探 zone：候选 DomainName 上 DescribeDomainRecords 成功即定。 */
static bool xacmeAliFindZone(
	xacmednsalicontext* pCtx, cstr sRr, cstr sZoneStart)
{
	char sZone[256];
	uint16 iStatus = 0u;
	str sBody = NULL;
	char sBodyText[320];
	if(pCtx->sZone[0] != '\0')
	{
		return true;
	}
	strcpy(sZone, sZoneStart);
	for(;;)
	{
		snprintf(
			sBodyText, sizeof(sBodyText),
			"DomainName=%s&RRKeyWord=%s", sZone, sRr);
		if(!xacmeAliCall(
			pCtx, "DescribeDomainRecords", sBodyText, &iStatus, &sBody))
		{
			return false;
		}
		if(getenv("XACME_DEBUG"))
		{
			printf("[ali-dbg] zone try %s -> %u body=%.140s\n", sZone,
				(unsigned)iStatus, (sBody != NULL) ? sBody : "");
		}
		xrtFree(sBody);
		sBody = NULL;
		if((iStatus >= 200u) && (iStatus < 300u))
		{
			snprintf(pCtx->sZone, sizeof(pCtx->sZone), "%s", sZone);
			return true;
		}
		{
			char* sDot = strchr(sZone, '.');
			if((sDot == NULL) || (strchr(sDot + 1, '.') == NULL))
			{
				return false; /* 剩两段仍失败：放弃 */
			}
			memmove(sZone, sDot + 1, strlen(sDot + 1) + 1u);
		}
	}
}

static bool xacmeAliAdd(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsalicontext* pCtx =
		(xacmednsalicontext*)pProvider->pContext;
	char sFqdnText[256];
	char sRr[200];
	char sZone[256];
	char sBody[700];
	uint16 iStatus = 0u;
	str sResp = NULL;
	if((sFqdn.Size >= sizeof(sFqdnText)) || (sTxt.Size > 200u))
	{
		return false;
	}
	memcpy(sFqdnText, sFqdn.Data, sFqdn.Size);
	sFqdnText[sFqdn.Size] = '\0';
	if(!xacmeAliSplit(
		sFqdnText, sRr, sizeof(sRr), sZone, sizeof(sZone)))
	{
		return false;
	}
	if(!xacmeAliFindZone(pCtx, sRr, sZone))
	{
		return false;
	}
	{
		char sTxtText[208];
		/* RR = FQDN 去掉 ".zone" 后缀的完整前缀（zone 试探可能
		   剥掉多段，不能只用最左段）。 */
		size_t iZoneLen = strlen(pCtx->sZone);
		size_t iFqdnLen = strlen(sFqdnText);
		size_t iRrLen;
		if((iFqdnLen <= iZoneLen + 1u) ||
			(strcmp(sFqdnText + iFqdnLen - iZoneLen, pCtx->sZone) != 0) ||
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
		memcpy(sTxtText, sTxt.Data, sTxt.Size);
		sTxtText[sTxt.Size] = '\0';
		snprintf(
			sBody, sizeof(sBody),
			"DomainName=%s&RR=%s&Type=TXT&Value=%s",
			pCtx->sZone, sRr, sTxtText);
	}
	if(!xacmeAliCall(
		pCtx, "AddDomainRecord", sBody, &iStatus, &sResp))
	{
		return false;
	}
	if((iStatus < 200u) || (iStatus >= 300u))
	{
		if(getenv("XACME_DEBUG"))
		{
			printf("[ali-dbg] add -> %u body=%.200s\n", (unsigned)iStatus,
				(sResp != NULL) ? sResp : "");
		}
		xrtFree(sResp);
		return false;
	}
	xacmeAliSaveRecordId(pCtx, sResp);
	xrtFree(sResp);
	/* 可选传播确认（公共 resolver 视角）。 */
	if(pCtx->sVerifyResolver[0] != '\0')
	{
			xrtSleep(20000u); /* 权威集群同步窗口 */
		char sTxtText[208];
		memcpy(sTxtText, sTxt.Data, sTxt.Size);
		sTxtText[sTxt.Size] = '\0';
		(void)xacmeDnsTxtWait(
			&pCtx->Verify, pCtx->sVerifyResolver, 53u, sFqdnText,
			sTxtText, 60000u);
	}
	return true;
}

static bool xacmeAliRemove(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsalicontext* pCtx =
		(xacmednsalicontext*)pProvider->pContext;
	size_t i;
	bool bAnyOk = false;
	(void)sFqdn;
	(void)sTxt;
	for(i = 0; i < pCtx->iRecordCount; i++)
	{
		char sBody[128];
		uint16 iStatus = 0u;
		str sResp = NULL;
		snprintf(
			sBody, sizeof(sBody), "RecordId=%s", pCtx->sRecordIds[i]);
		if(!xacmeAliCall(
			pCtx, "DeleteDomainRecord", sBody, &iStatus, &sResp))
		{
			continue;
		}
		xrtFree(sResp);
		/* 2xx，或"记录不存在"类 4xx 都算删除成功（幂等）。 */
		if(((iStatus >= 200u) && (iStatus < 300u)) ||
			(iStatus == 400u) || (iStatus == 404u))
		{
			bAnyOk = true;
			pCtx->sRecordIds[i][0] = '\0';
		}
	}
	return bAnyOk || (pCtx->iRecordCount == 0u);
}

void xrtAcmeDnsAliConfigInit(xacmednaliconfig* pConfig)
{
	if(pConfig == NULL)
	{
		return;
	}
	pConfig->sAccessKeyId = NULL;
	pConfig->sAccessKeySecret = NULL;
	pConfig->sEndpoint = NULL;
	pConfig->sVerifyResolver = NULL;
}

bool xrtAcmeDnsAli(
	const xacmednaliconfig* pConfig, xacmednsprovider* pProvider)
{
	xacmednsalicontext* pCtx;
	if((pConfig == NULL) || (pProvider == NULL) ||
		(pConfig->sAccessKeyId == NULL) ||
		(pConfig->sAccessKeySecret == NULL) ||
		(pConfig->sAccessKeyId[0] == '\0') ||
		(pConfig->sAccessKeySecret[0] == '\0'))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_ali requires access key id and secret");
		return false;
	}
	pCtx = (xacmednsalicontext*)xrtCalloc(1, sizeof(*pCtx));
	if(pCtx == NULL)
	{
		return false;
	}
	snprintf(pCtx->sKeyId, sizeof(pCtx->sKeyId), "%s",
		pConfig->sAccessKeyId);
	snprintf(pCtx->sSecret, sizeof(pCtx->sSecret), "%s",
		pConfig->sAccessKeySecret);
	snprintf(pCtx->sEndpoint, sizeof(pCtx->sEndpoint), "%s",
		(pConfig->sEndpoint != NULL) ? pConfig->sEndpoint :
			"alidns.aliyuncs.com");
	if((pConfig->sVerifyResolver != NULL) &&
		(pConfig->sVerifyResolver[0] != '\0'))
	{
		snprintf(pCtx->sVerifyResolver, sizeof(pCtx->sVerifyResolver),
			"%s", pConfig->sVerifyResolver);
	}
	if(!xacmeHttpInit(&pCtx->Http, NULL, NULL, 0u) ||
		!xacmeDnsInit(&pCtx->Verify, NULL))
	{
		xacmeHttpUnit(&pCtx->Http);
		xacmeDnsUnit(&pCtx->Verify);
		xrtFree(pCtx);
		return false;
	}
	pProvider->sId = "ali";
	pProvider->iCaps = 0u;
	pProvider->pContext = pCtx;
	pProvider->Add = xacmeAliAdd;
	pProvider->Remove = xacmeAliRemove;
	pProvider->Propagate = NULL;
	return true;
}

void xrtAcmeDnsAliProviderUnit(xacmednsprovider* pProvider)
{
	if((pProvider != NULL) && (pProvider->pContext != NULL))
	{
		xacmednsalicontext* pCtx =
			(xacmednsalicontext*)pProvider->pContext;
		xacmeHttpUnit(&pCtx->Http);
		xacmeDnsUnit(&pCtx->Verify);
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
