#include <xrt/acme_dns_cf.h>

#if defined(XACME_FEATURE_DNS_CF)

#include "../internal/xacme_dnscommon.h"
#include "../internal/xacme_http.h"

#include <xrt/buffer.h>

#include <stdlib.h>

/*
	Cloudflare DNS provider（API v4）：
	  - 认证：Authorization: Bearer <API Token>；
	  - zone 发现：GET /zones?name=<候选>（精确匹配，逐级上探）；
	  - 加 TXT：POST /zones/<id>/dns_records（content 不带引号）；
	  - 删 TXT：DELETE /zones/<id>/dns_records/<record id>。
*/

typedef struct xacmednscfcontext {
	xacmehttp Http;
	char sToken[200];
	char sEndpoint[160];
	xacmednszonecache Zones;
	xacmednsrecords Records;
} xacmednscfcontext;

static void xacmeCfError(xerrkind Kind, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
		sMessage);
}

/* 执行一次 API 调用；pBody 为空表示无请求体。 */
static bool xacmeCfCall(
	xacmednscfcontext* pCtx, cstr sMethod, cstr sPathAndQuery,
	cstr sBody, uint16* pOutStatus, str* pOutBody)
{
	char sAuth[240];
	char sUrl[400];
	xacmehttpheader Extra[1];
	xacmehttpresponse R;

	snprintf(sAuth, sizeof(sAuth), "Bearer %s", pCtx->sToken);
	snprintf(sUrl, sizeof(sUrl), "https://%s%s", pCtx->sEndpoint,
		sPathAndQuery);
	Extra[0] = (xacmehttpheader){ "Authorization", sAuth };
	if(!xacmeHttpExchangeV(
			&pCtx->Http, sMethod, sUrl, "application/json",
			(xstrview){ sBody, (sBody != NULL) ? strlen(sBody) : 0u },
			Extra, 1u, &R))
	{
		return false;
	}
	*pOutStatus = R.iStatus;
	*pOutBody = R.sBody;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	return true;
}

/* 在 zones 列表里按 name 精确查 zone id。 */
static bool xacmeCfZoneId(
	xacmednscfcontext* pCtx, cstr sZone, char* sOutId, size_t iIdCap)
{
	char sPath[300];
	uint16 iStatus = 0u;
	str sBody = NULL;
	xvalue* pRoot = NULL;
	xvalue* pResult = NULL;
	bool bOk = false;
	size_t i;

	snprintf(sPath, sizeof(sPath),
		"/client/v4/zones?name=%s&per_page=5", sZone);
	if(!xacmeCfCall(pCtx, "GET", sPath, NULL, &iStatus, &sBody))
	{
		return false;
	}
	if(sBody != NULL)
	{
		pRoot = xrtJsonParse((xstrview){ sBody, strlen(sBody) });
	}
	if((pRoot != NULL) &&
		((pResult = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("result"))) !=
			NULL) &&
		xrtValueIs(pResult, XVALUE_ARRAY))
	{
		for(i = 0; i < xrtValueCount(pResult); i++)
		{
			xvalue* pItem = xrtValueArrayGet(pResult, i);
			char sName[256];
			if((pItem != NULL) && xrtValueIs(pItem, XVALUE_OBJECT) &&
				xacmeDnsJsonText(pItem, "name", sName, sizeof(sName)) &&
				(strcmp(sName, sZone) == 0) &&
				xacmeDnsJsonText(pItem, "id", sOutId, iIdCap))
			{
				bOk = true;
				break;
			}
		}
	}
	xrtValueRelease(pRoot);
	xrtFree(sBody);
	if((iStatus < 200u) || (iStatus >= 300u))
	{
		xacmeCfError(XERR_PROTOCOL, "acme dns_cf zones response invalid");
		return false;
	}
	return bOk;
}

/* zone 逐级上探发现并缓存；返回 zone id。 */
static bool xacmeCfFindZone(
	xacmednscfcontext* pCtx, cstr sFqdn, char* sOutZone, size_t iZoneCap,
	char* sOutId, size_t iIdCap)
{
	char sCandidate[256];
	const char* sCached = xacmeDnsZoneMatch(&pCtx->Zones, sFqdn);
	if(sCached != NULL)
	{
		snprintf(sOutZone, iZoneCap, "%s", sCached);
		return xacmeCfZoneId(pCtx, sCached, sOutId, iIdCap);
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
		if(xacmeCfZoneId(pCtx, sZone, sOutId, iIdCap))
		{
			snprintf(sOutZone, iZoneCap, "%s", sZone);
			xacmeDnsZoneRemember(&pCtx->Zones, sZone);
			return true;
		}
		{
			char* sDot = strchr(sZone, '.');
			if((sDot == NULL) || (strchr(sDot + 1, '.') == NULL))
			{
				return false; /* 剩两段仍失败：放弃 */
			}
			snprintf(sCandidate, sizeof(sCandidate), "%s", sZone);
		}
	}
}

static bool xacmeCfAdd(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednscfcontext* pCtx = (xacmednscfcontext*)pProvider->pContext;
	char sFqdnText[256];
	char sTxtText[208];
	char sZone[256];
	char sZoneId[64];
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	xvalue* pRoot = NULL;
	char sRecordId[64];
	bool bOk = false;

	if((sFqdn.Size >= sizeof(sFqdnText)) || (sTxt.Size > 200u))
	{
		return false;
	}
	memcpy(sFqdnText, sFqdn.Data, sFqdn.Size);
	sFqdnText[sFqdn.Size] = '\0';
	memcpy(sTxtText, sTxt.Data, sTxt.Size);
	sTxtText[sTxt.Size] = '\0';

	if(!xacmeCfFindZone(pCtx, sFqdnText, sZone, sizeof(sZone), sZoneId,
			sizeof(sZoneId)))
	{
		return false;
	}

	xrtBufferInit(&Body);
	if(xrtBufferAppend(&Body, XRT_BYTES_LITERAL("{\"type\":\"TXT\",\"name\":")) &&
		xacmeDnsJsonQuote(&Body, sFqdn) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL(",\"content\":")) &&
		xacmeDnsJsonQuote(&Body, sTxt) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL(",\"ttl\":60}")))
	{
		char sPath[128];
		snprintf(sPath, sizeof(sPath), "/client/v4/zones/%s/dns_records",
			sZoneId);
		bOk = xacmeCfCall(pCtx, "POST", sPath, (cstr)Body.Data, &iStatus,
			&sResp);
	}
	xrtBufferUnit(&Body);
	if(!bOk || (iStatus < 200u) || (iStatus >= 300u))
	{
		xrtFree(sResp);
		return false;
	}
	/* 提取 result.id 供 Remove（记录句柄 = "zoneid/recordid"）。 */
	if(sResp != NULL)
	{
		pRoot = xrtJsonParse((xstrview){ sResp, strlen(sResp) });
	}
	if(pRoot != NULL)
	{
		xvalue* pResult = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("result"));
		if((pResult != NULL) && xrtValueIs(pResult, XVALUE_OBJECT) &&
			xacmeDnsJsonText(pResult, "id", sRecordId, sizeof(sRecordId)))
		{
			char sHandle[64];
			snprintf(sHandle, sizeof(sHandle), "%.31s/%.30s", sZoneId,
				sRecordId);
			xacmeDnsRecordRemember(&pCtx->Records, sHandle);
		}
	}
	xrtValueRelease(pRoot);
	xrtFree(sResp);
	return true;
}

static bool xacmeCfRemove(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednscfcontext* pCtx = (xacmednscfcontext*)pProvider->pContext;
	size_t i;
	bool bAnyOk = false;
	(void)sFqdn;
	(void)sTxt;
	for(i = 0; i < pCtx->Records.iCount; i++)
	{
		char sZoneId[32];
		char sRecordId[32];
		char sPath[128];
		uint16 iStatus = 0u;
		str sResp = NULL;
		const char* sSlash;
		if(pCtx->Records.sIds[i][0] == '\0')
		{
			continue;
		}
		sSlash = strchr(pCtx->Records.sIds[i], '/');
		if((sSlash == NULL) ||
			((size_t)(sSlash - pCtx->Records.sIds[i]) >= sizeof(sZoneId)) ||
			(strlen(sSlash + 1) >= sizeof(sRecordId)))
		{
			continue;
		}
		memcpy(sZoneId, pCtx->Records.sIds[i],
			(size_t)(sSlash - pCtx->Records.sIds[i]));
		sZoneId[sSlash - pCtx->Records.sIds[i]] = '\0';
		{
			size_t iRecordLen = strlen(sSlash + 1);
			if(iRecordLen >= sizeof(sRecordId))
			{
				continue;
			}
			memcpy(sRecordId, sSlash + 1, iRecordLen);
			sRecordId[iRecordLen] = '\0';
		}
		snprintf(sPath, sizeof(sPath),
			"/client/v4/zones/%.32s/dns_records/%.31s", sZoneId,
			sRecordId);
		if(!xacmeCfCall(pCtx, "DELETE", sPath, NULL, &iStatus, &sResp))
		{
			continue;
		}
		xrtFree(sResp);
		if(((iStatus >= 200u) && (iStatus < 300u)) || (iStatus == 404u))
		{
			bAnyOk = true;
			pCtx->Records.sIds[i][0] = '\0';
		}
	}
	return bAnyOk || (pCtx->Records.iCount == 0u);
}

void xrtAcmeDnsCfConfigInit(xacmednscfconfig* pConfig)
{
	if(pConfig == NULL)
	{
		return;
	}
	pConfig->sApiToken = NULL;
	pConfig->sEndpoint = NULL;
}

bool xrtAcmeDnsCf(
	const xacmednscfconfig* pConfig, struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider)
{
	xacmednscfcontext* pCtx;
	if((pConfig == NULL) || (pProvider == NULL) ||
		(pConfig->sApiToken == NULL) || (pConfig->sApiToken[0] == '\0'))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_cf requires api token");
		return false;
	}
	pCtx = (xacmednscfcontext*)xrtCalloc(1, sizeof(*pCtx));
	if(pCtx == NULL)
	{
		return false;
	}
	snprintf(pCtx->sToken, sizeof(pCtx->sToken), "%s", pConfig->sApiToken);
	snprintf(pCtx->sEndpoint, sizeof(pCtx->sEndpoint), "%s",
		(pConfig->sEndpoint != NULL) ? pConfig->sEndpoint :
			"api.cloudflare.com");
	if(!xacmeHttpInit(&pCtx->Http, pBorrowedEngine, NULL, 0u))
	{
		xacmeHttpUnit(&pCtx->Http);
		xrtFree(pCtx);
		return false;
	}
	pProvider->sId = "cf";
	pProvider->iCaps = 0u;
	pProvider->pContext = pCtx;
	pProvider->Add = xacmeCfAdd;
	pProvider->Remove = xacmeCfRemove;
	pProvider->Propagate = NULL;
	return true;
}

void xrtAcmeDnsCfProviderUnit(xacmednsprovider* pProvider)
{
	if((pProvider != NULL) && (pProvider->pContext != NULL))
	{
		xacmednscfcontext* pCtx = (xacmednscfcontext*)pProvider->pContext;
		xacmeHttpUnit(&pCtx->Http);
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
