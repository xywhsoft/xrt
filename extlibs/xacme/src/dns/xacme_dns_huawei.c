#include <xrt/acme_dns_huawei.h>

#if defined(XACME_FEATURE_DNS_HUAWEI)

#include "../internal/xacme_dnscommon.h"
#include "../internal/xacme_http.h"
#include "../internal/xacme_sigv4.h"

#include <xrt/buffer.h>
#include <xrt/time.h>

#include <stdlib.h>

/*
	华为云 DNS provider（API v2，SDK-HMAC-SHA256）：
	  - canonical：content-type/host/x-sdk-date 三头（小写字典序）；
	  - StringToSign = "SDK-HMAC-SHA256\n<X-Sdk-Date>\n"
	    "<sha256hex(canonical)>"（无凭据范围）；
	  - 密钥链：单级 HMAC(SK, YYYYMMDD)；
	  - zone 发现：GET /v2/zones?name=<候选>&search_mode=equal；
	  - 加 TXT：POST /v2/zones/<id>/recordsets
	    （name 带尾点，records 值内嵌双引号）；
	  - 删 TXT：DELETE /v2/zones/<id>/recordsets/<recordset id>。
*/

typedef struct xacmednshuaaweicontext {
	xacmehttp Http;
	char sAk[160];
	char sSk[160];
	char sEndpoint[160];
	xacmednszonecache Zones;
	xacmednsrecords Records;
} xacmednshuaaweicontext;

static void xacmeHuaweiError(xerrkind Kind, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
		sMessage);
}

static bool xacmeHuaweiCall(
	xacmednshuaaweicontext* pCtx, cstr sMethod, cstr sPathAndQuery,
	cstr sBody, uint16* pOutStatus, str* pOutBody)
{
	static const char* sSignedHeaders = "content-type;host;x-sdk-date";
	char sStampText[24];     /* YYYYMMDDTHHMMSSZ */
	char sDateText[16];      /* YYYYMMDD */
	char sPayloadHash[XACME_SIG_HASH_TEXT];
	char sCanonical[1600];
	char sHeaders[360];
	char sStringToSign[160];
	char sHex[XACME_SIG_HASH_TEXT];
	char sAuth[560];
	char sUrl[400];
	xacmehttpheader Extra[3];
	xacmehttpresponse R;
	xdatetime Now;
	uint8 Signature[XRT_SHA256_SIZE];

	if(!xrtTimeSplitAt(xrtNow(), 0, &Now))
	{
		return false;
	}
	snprintf(sStampText, sizeof(sStampText),
		"%04ld%02d%02dT%02d%02d%02dZ", (long)Now.Year, Now.Month, Now.Day,
		Now.Hour, Now.Minute, Now.Second);
	snprintf(sDateText, sizeof(sDateText), "%04ld%02d%02d", (long)Now.Year,
		Now.Month, Now.Day);
	if(!xacmeSigSha256Hex(
			(sBody != NULL) ? sBody : "",
			(sBody != NULL) ? strlen(sBody) : 0u, sPayloadHash))
	{
		return false;
	}
	snprintf(sHeaders, sizeof(sHeaders),
		"content-type:application/json\nhost:%s\nx-sdk-date:%s\n",
		pCtx->sEndpoint, sStampText);
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
		"SDK-HMAC-SHA256\n%s\n%s", sStampText, sHex);
	if(!xacmeSigHmac((const uint8*)pCtx->sSk, strlen(pCtx->sSk), sDateText,
			strlen(sDateText), Signature) ||
		!xacmeSigHmac(Signature, XRT_SHA256_SIZE, sStringToSign,
			strlen(sStringToSign), Signature))
	{
		return false;
	}
	xacmeSigHex(Signature, sizeof(Signature), sHex);
	snprintf(sAuth, sizeof(sAuth),
		"SDK-HMAC-SHA256 Access=%s, SignedHeaders=%s, Signature=%s",
		pCtx->sAk, sSignedHeaders, sHex);
	snprintf(sUrl, sizeof(sUrl), "https://%s%s", pCtx->sEndpoint,
		sPathAndQuery);

	Extra[0] = (xacmehttpheader){ "Authorization", sAuth };
	Extra[1] = (xacmehttpheader){ "X-Sdk-Date", sStampText };
	if(!xacmeHttpExchangeV(
			&pCtx->Http, sMethod, sUrl, "application/json",
			(xstrview){ sBody, (sBody != NULL) ? strlen(sBody) : 0u },
			Extra, 2u, &R))
	{
		return false;
	}
	*pOutStatus = R.iStatus;
	*pOutBody = R.sBody;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	return true;
}

/* zones?name= 精确匹配候选（zone 名带尾点，比较时剥除）。 */
static bool xacmeHuaweiZoneId(
	xacmednshuaaweicontext* pCtx, cstr sZone, char* sOutId, size_t iIdCap)
{
	char sPath[300];
	uint16 iStatus = 0u;
	str sBody = NULL;
	xvalue* pRoot = NULL;
	xvalue* pZones = NULL;
	bool bOk = false;
	size_t i;

	snprintf(sPath, sizeof(sPath), "/v2/zones?name=%s.&limit=1", sZone);
	if(!xacmeHuaweiCall(pCtx, "GET", sPath, NULL, &iStatus, &sBody))
	{
		return false;
	}
	if(sBody != NULL)
	{
		pRoot = xrtJsonParse((xstrview){ sBody, strlen(sBody) });
	}
	if((pRoot != NULL) &&
		((pZones = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("zones"))) !=
			NULL) &&
		xrtValueIs(pZones, XVALUE_ARRAY))
	{
		char sWantDot[280];
		snprintf(sWantDot, sizeof(sWantDot), "%s.", sZone);
		for(i = 0; i < xrtValueCount(pZones); i++)
		{
			xvalue* pItem = xrtValueArrayGet(pZones, i);
			char sName[280];
			if((pItem != NULL) && xrtValueIs(pItem, XVALUE_OBJECT) &&
				xacmeDnsJsonText(pItem, "name", sName, sizeof(sName)) &&
				(strcmp(sName, sWantDot) == 0) &&
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
		xacmeHuaweiError(
			XERR_PROTOCOL, "acme dns_huawei zones response invalid");
		return false;
	}
	return bOk;
}

static bool xacmeHuaweiFindZone(
	xacmednshuaaweicontext* pCtx, cstr sFqdn, char* sOutZone,
	size_t iZoneCap, char* sOutId, size_t iIdCap)
{
	char sCandidate[256];
	const char* sCached = xacmeDnsZoneMatch(&pCtx->Zones, sFqdn);
	if(sCached != NULL)
	{
		snprintf(sOutZone, iZoneCap, "%s", sCached);
		return xacmeHuaweiZoneId(pCtx, sCached, sOutId, iIdCap);
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
		if(xacmeHuaweiZoneId(pCtx, sZone, sOutId, iIdCap))
		{
			snprintf(sOutZone, iZoneCap, "%s", sZone);
			xacmeDnsZoneRemember(&pCtx->Zones, sZone);
			return true;
		}
		snprintf(sCandidate, sizeof(sCandidate), "%s", sZone);
	}
}

static bool xacmeHuaweiAdd(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednshuaaweicontext* pCtx = (xacmednshuaaweicontext*)pProvider->pContext;
	char sFqdnText[256];
	char sTxtText[208];
	char sZone[256];
	char sZoneId[80];
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	xvalue* pRoot = NULL;
	char sRecordId[80];
	bool bOk = false;

	if((sFqdn.Size >= sizeof(sFqdnText)) || (sTxt.Size > 200u))
	{
		return false;
	}
	memcpy(sFqdnText, sFqdn.Data, sFqdn.Size);
	sFqdnText[sFqdn.Size] = '\0';
	memcpy(sTxtText, sTxt.Data, sTxt.Size);
	sTxtText[sTxt.Size] = '\0';

	if(!xacmeHuaweiFindZone(pCtx, sFqdnText, sZone, sizeof(sZone), sZoneId,
			sizeof(sZoneId)))
	{
		return false;
	}

	/* name 带尾点；records 值必须内嵌双引号。 */
	xrtBufferInit(&Body);
	if(xrtBufferAppend(&Body, XRT_BYTES_LITERAL("{\"name\":")) &&
		xacmeDnsJsonQuote(&Body, sFqdn) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL(".\",\"type\":\"TXT\","
			"\"ttl\":60,\"records\":[\"\\\"")) &&
		xrtBufferAppend(&Body,
			(xbytesview){ (const uint8*)sTxtText, strlen(sTxtText) }) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL("\\\"\"]}")))
	{
		char sPath[128];
		snprintf(sPath, sizeof(sPath), "/v2/zones/%s/recordsets",
			sZoneId);
		bOk = xacmeHuaweiCall(pCtx, "POST", sPath, (cstr)Body.Data,
			&iStatus, &sResp);
	}
	xrtBufferUnit(&Body);
	if(!bOk || (iStatus < 200u) || (iStatus >= 300u))
	{
		xrtFree(sResp);
		return false;
	}
	if(sResp != NULL)
	{
		pRoot = xrtJsonParse((xstrview){ sResp, strlen(sResp) });
	}
	if((pRoot != NULL) && xrtValueIs(pRoot, XVALUE_OBJECT) &&
		xacmeDnsJsonText(pRoot, "id", sRecordId, sizeof(sRecordId)))
	{
		char sHandle[160];
		snprintf(sHandle, sizeof(sHandle), "%.75s|%.75s", sZoneId,
			sRecordId);
		xacmeDnsRecordRemember(&pCtx->Records, sHandle);
	}
	xrtValueRelease(pRoot);
	xrtFree(sResp);
	return true;
}

static bool xacmeHuaweiRemove(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednshuaaweicontext* pCtx =
		(xacmednshuaaweicontext*)pProvider->pContext;
	size_t i;
	bool bAnyOk = false;
	(void)sFqdn;
	(void)sTxt;
	for(i = 0; i < pCtx->Records.iCount; i++)
	{
		char sZoneId[80];
		char sRecordId[80];
		char sPath[200];
		uint16 iStatus = 0u;
		str sResp = NULL;
		const char* sBar;
		if(pCtx->Records.sIds[i][0] == '\0')
		{
			continue;
		}
		sBar = strchr(pCtx->Records.sIds[i], '|');
		if((sBar == NULL) ||
			((size_t)(sBar - pCtx->Records.sIds[i]) >= sizeof(sZoneId)) ||
			(strlen(sBar + 1) >= sizeof(sRecordId)))
		{
			continue;
		}
		memcpy(sZoneId, pCtx->Records.sIds[i],
			(size_t)(sBar - pCtx->Records.sIds[i]));
		sZoneId[sBar - pCtx->Records.sIds[i]] = '\0';
		snprintf(sRecordId, sizeof(sRecordId), "%s", sBar + 1);
		snprintf(sPath, sizeof(sPath), "/v2/zones/%s/recordsets/%s",
			sZoneId, sRecordId);
		if(xacmeHuaweiCall(pCtx, "DELETE", sPath, NULL, &iStatus, &sResp))
		{
			xrtFree(sResp);
			if(((iStatus >= 200u) && (iStatus < 300u)) ||
				(iStatus == 404u) || (iStatus == 400u))
			{
				bAnyOk = true;
				pCtx->Records.sIds[i][0] = '\0';
			}
		}
	}
	return bAnyOk || (pCtx->Records.iCount == 0u);
}

void xrtAcmeDnsHuaweiConfigInit(xacmednshuaaweiconfig* pConfig)
{
	if(pConfig == NULL)
	{
		return;
	}
	pConfig->sAccessKey = NULL;
	pConfig->sSecretKey = NULL;
	pConfig->sEndpoint = NULL;
}

bool xrtAcmeDnsHuawei(
	const xacmednshuaaweiconfig* pConfig,
	struct xnetengine* pBorrowedEngine, xacmednsprovider* pProvider)
{
	xacmednshuaaweicontext* pCtx;
	if((pConfig == NULL) || (pProvider == NULL) ||
		(pConfig->sAccessKey == NULL) || (pConfig->sSecretKey == NULL) ||
		(pConfig->sAccessKey[0] == '\0') ||
		(pConfig->sSecretKey[0] == '\0'))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_huawei requires access key and secret");
		return false;
	}
	pCtx = (xacmednshuaaweicontext*)xrtCalloc(1, sizeof(*pCtx));
	if(pCtx == NULL)
	{
		return false;
	}
	snprintf(pCtx->sAk, sizeof(pCtx->sAk), "%s", pConfig->sAccessKey);
	snprintf(pCtx->sSk, sizeof(pCtx->sSk), "%s", pConfig->sSecretKey);
	snprintf(pCtx->sEndpoint, sizeof(pCtx->sEndpoint), "%s",
		(pConfig->sEndpoint != NULL) ? pConfig->sEndpoint :
			"dns.myhuaweicloud.com");
	if(!xacmeHttpInit(&pCtx->Http, pBorrowedEngine, NULL, 0u))
	{
		xacmeHttpUnit(&pCtx->Http);
		xrtFree(pCtx);
		return false;
	}
	pProvider->sId = "huawei";
	pProvider->iCaps = 0u;
	pProvider->pContext = pCtx;
	pProvider->Add = xacmeHuaweiAdd;
	pProvider->Remove = xacmeHuaweiRemove;
	pProvider->Propagate = NULL;
	return true;
}

void xrtAcmeDnsHuaweiProviderUnit(xacmednsprovider* pProvider)
{
	if((pProvider != NULL) && (pProvider->pContext != NULL))
	{
		xacmednshuaaweicontext* pCtx =
			(xacmednshuaaweicontext*)pProvider->pContext;
		xacmeHttpUnit(&pCtx->Http);
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
