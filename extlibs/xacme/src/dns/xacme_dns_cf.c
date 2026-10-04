#include <xrt/acme_dns_cf.h>

#if defined(XACME_FEATURE_DNS_CF)

#include "../internal/xacme_dnscommon.h"
#include "../internal/xacme_http.h"

#include <xrt/buffer.h>
#include <xrt/memory.h>
#include <xrt/sync.h>

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
	xmutex Lock;
	char sToken[200];
	char sEndpoint[160];
	xacmednsrecords Records;
	bool bUncertain[XACME_DNS_RECORD_MAX];
} xacmednscfcontext;

static void xacmeCfError(xerrkind Kind, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
		sMessage);
}

/* 执行一次 API 调用；pBody 为空表示无请求体。 */
static bool xacmeCfCall(
	xacmednscfcontext* pCtx, cstr sMethod, cstr sPathAndQuery,
	cstr sBody, uint16* pOutStatus, str* pOutBody, size_t* pOutSize)
{
	char sAuth[240];
	char sUrl[512];
	xacmehttpheader Extra[1];
	xacmehttpresponse R;

	pCtx->Http.bWriteUncertain = false;
	snprintf(sAuth, sizeof(sAuth), "Bearer %s", pCtx->sToken);
	if(!xacmeDnsHttpsUrl(sUrl, sizeof(sUrl), pCtx->sEndpoint,
			sPathAndQuery))
	{
		return false;
	}
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
	if(pOutSize != NULL) *pOutSize = R.iBodySize;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	return true;
}

/* 在 zones 列表里按 name 精确查 zone id。 */
static xacmednszoneresult xacmeCfZoneId(
	xacmednscfcontext* pCtx, cstr sZone, char* sOutId, size_t iIdCap)
{
	char sPath[300];
	uint16 iStatus = 0u;
	str sBody = NULL;
	size_t iBodySize = 0u;
	xacmednszoneresult Result;

	snprintf(sPath, sizeof(sPath),
		"/client/v4/zones?name=%s&per_page=5", sZone);
	if(!xacmeCfCall(pCtx, "GET", sPath, NULL, &iStatus, &sBody, &iBodySize))
	{
		return XACME_DNS_ZONE_ERROR;
	}
	if(iStatus == 200u)
		Result = xacmeDnsJsonZoneId(
			(xstrview){ sBody, iBodySize },
			"result", sZone, true, 5u, sOutId, iIdCap);
	else
	{
		Result = XACME_DNS_ZONE_ERROR;
		xacmeCfError((iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION :
			(iStatus == 429u || iStatus >= 500u) ? XERR_AGAIN : XERR_PROTOCOL,
			"acme dns_cf zone query rejected or unavailable");
	}
	xrtFree(sBody);
	return Result;
}

/* 每次从完整属主查找最近的托管区域；父区域不能证明子区域不存在。 */
static bool xacmeCfFindZone(
	xacmednscfcontext* pCtx, cstr sFqdn, char* sOutZone, size_t iZoneCap,
	char* sOutId, size_t iIdCap)
{
	char sCandidate[256];
	xacmednszoneresult Lookup;
	snprintf(sCandidate, sizeof(sCandidate), "%s", sFqdn);
	for(;;)
	{
		char* sDot;
		Lookup = xacmeCfZoneId(pCtx, sCandidate, sOutId, iIdCap);
		if(Lookup == XACME_DNS_ZONE_FOUND)
		{
			snprintf(sOutZone, iZoneCap, "%s", sCandidate);
			return true;
		}
		if(Lookup == XACME_DNS_ZONE_ERROR) return false;
		sDot = strchr(sCandidate, '.');
		if((sDot == NULL) || (strchr(sDot + 1, '.') == NULL)) break;
		memmove(sCandidate, sDot + 1, strlen(sDot + 1) + 1u);
	}
	xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_ZONE,
		"acme dns_cf managed zone not found");
	return false;
}

static bool xacmeCfCreateRejected(const xvalue* pRoot, uint16 iStatus)
{
	if(pRoot == NULL || !xrtValueIs(pRoot, XVALUE_OBJECT)) return false;
	xvalue* pSuccess = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("success"));
	xvalue* pErrors = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("errors"));
	xvalue* pResult = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("result"));
	bool bSuccess = true;
	size_t i;
	if(iStatus < 400u || iStatus >= 500u || !xrtValueGetBool(pSuccess, &bSuccess) || bSuccess ||
		(pResult != NULL && !xrtValueIs(pResult, XVALUE_NULL)) ||
		!xrtValueIs(pErrors, XVALUE_ARRAY) || xrtValueCount(pErrors) == 0u) return false;
	for(i = 0u; i < xrtValueCount(pErrors); i++) {
		xvalue* pError = xrtValueArrayGet(pErrors, i);
		char sMessage[512]; int64 iCode = 0;
		if(!xrtValueIs(pError, XVALUE_OBJECT) ||
			!xrtValueGetInt(xrtValueObjectGet(pError, XRT_STR_LITERAL("code")), &iCode) || iCode <= 0 ||
			!xacmeDnsJsonText(pError, "message", sMessage, sizeof(sMessage)) || sMessage[0] == '\0') return false;
	}
	return true;
}

static bool xacmeCfAddLocked(
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
	size_t iRespSize = 0u;
	xvalue* pRoot = NULL;
	char sRecordId[64];
	bool bOk = false;
	bool bTracked = false;
	bool bSent = false;
	size_t iSlot;

	if(!xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xacmeCfError(XERR_ARGUMENT,
			"acme dns_cf owner or digest is invalid");
		return false;
	}
	sFqdn = xacmeDnsCanonicalOwner(sFqdn, sFqdnText);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, sFqdn, sTxt))
		return xacmeDnsCreateUncertainError();
	iSlot = xacmeDnsCreateSlot(&pCtx->Records, pCtx->bUncertain);
	if(iSlot >= XACME_DNS_RECORD_MAX)
	{
		xacmeCfError(XERR_RANGE,
			"acme dns_cf record tracking capacity exhausted");
		return false;
	}
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
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL(",\"ttl\":60}")) &&
		xrtBufferAppendByte(&Body, 0u))
	{
		char sPath[128];
		snprintf(sPath, sizeof(sPath), "/client/v4/zones/%s/dns_records",
			sZoneId);
		xacmeDnsCreateReserve(&pCtx->Records, pCtx->bUncertain, iSlot, sFqdn, sTxt);
		bSent = true;
		bOk = xacmeCfCall(pCtx, "POST", sPath, (cstr)Body.Data, &iStatus,
			&sResp, &iRespSize);
	}
	xrtBufferUnit(&Body);
	if(!bOk)
	{
		xrtFree(sResp);
		if(!bSent) return false;
		if(!pCtx->Http.bWriteUncertain) {
			xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
			return false;
		}
		return xacmeDnsCreateUncertainError();
	}
	/* 提取 result.id 供 Remove（记录句柄 = "zoneid/recordid"）。 */
	if(sResp != NULL)
	{
		pRoot = xrtJsonParse((xstrview){ sResp, iRespSize });
	}
	if(xacmeCfCreateRejected(pRoot, iStatus)) {
		xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
		xrtValueRelease(pRoot); xrtFree(sResp);
		xacmeCfError((iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION : XERR_PROTOCOL,
			"acme dns_cf create was rejected");
		return false;
	}
	if(iStatus >= 200u && iStatus < 300u && xacmeDnsJsonSuccess(pRoot))
	{
		xvalue* pResult = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("result"));
		xvalue* pErrors = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("errors"));
		if((pResult != NULL) && xrtValueIs(pResult, XVALUE_OBJECT) &&
			xrtValueIs(pErrors, XVALUE_ARRAY) && xrtValueCount(pErrors) == 0u &&
			xacmeDnsJsonEqual(pResult, "name", sFqdnText) &&
			xacmeDnsJsonEqual(pResult, "type", "TXT") &&
			xacmeDnsJsonEqual(pResult, "content", sTxtText) &&
			xacmeDnsJsonPathId(pResult, "id", sRecordId, sizeof(sRecordId)))
		{
			bTracked = xacmeDnsCreateCommit(&pCtx->Records, pCtx->bUncertain,
				iSlot, sZoneId, '/', sRecordId);
		}
	}
	xrtValueRelease(pRoot);
	xrtFree(sResp);
	return bTracked ? true : xacmeDnsCreateUncertainError();
}

/* GET requires the full envelope; DELETE's documented response also permits
 * just result.id. Present success/errors fields must never contradict it. */
static bool xacmeCfRecordResponseOk(const xvalue* pRoot, bool bFull)
{
	xvalue* pSuccess;
	xvalue* pErrors;
	bool bSuccess = false;
	if(!xrtValueIs(pRoot, XVALUE_OBJECT)) return false;
	pSuccess = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("success"));
	pErrors = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("errors"));
	if((bFull || pSuccess != NULL) &&
		(!xrtValueGetBool(pSuccess, &bSuccess) || !bSuccess)) return false;
	return !(bFull || pErrors != NULL) ||
		(xrtValueIs(pErrors, XVALUE_ARRAY) && xrtValueCount(pErrors) == 0u);
}

/* A bare 404 can be an authentication, zone or proxy failure. Only the DNS
 * record-not-found error in a non-contradictory envelope proves absence. */
static bool xacmeCfRecordMissing(const xvalue* pRoot, uint16 iStatus)
{
	xvalue* pErrors;
	size_t i;
	if(iStatus != 404u || !xacmeCfCreateRejected(pRoot, iStatus)) return false;
	pErrors = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("errors"));
	for(i = 0u; i < xrtValueCount(pErrors); i++) {
		int64 iCode = 0;
		if(!xrtValueGetInt(xrtValueObjectGet(xrtValueArrayGet(pErrors, i),
			XRT_STR_LITERAL("code")), &iCode) || iCode != 81044) return false;
	}
	return true;
}

typedef enum xacmecfrecordresult {
	XACME_CF_RECORD_ERROR,
	XACME_CF_RECORD_FOUND,
	XACME_CF_RECORD_MISSING
} xacmecfrecordresult;

static xacmecfrecordresult xacmeCfReadRecord(xacmednscfcontext* pCtx,
	cstr sPath, cstr sRecordId, xstrview sFqdn, xstrview sTxt)
{
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	xvalue* pRoot;
	xacmecfrecordresult Result = XACME_CF_RECORD_ERROR;
	char sOwner[256], sValue[208];
	/* These views have already passed challenge validation. */
	memcpy(sOwner, sFqdn.Data, sFqdn.Size); sOwner[sFqdn.Size] = '\0';
	memcpy(sValue, sTxt.Data, sTxt.Size); sValue[sTxt.Size] = '\0';
	xrtClearError();
	if(!xacmeCfCall(pCtx, "GET", sPath, NULL, &iStatus, &sResp, &iRespSize)) {
		xrtFree(sResp); return Result;
	}
	pRoot = xrtJsonParse((xstrview){ sResp, iRespSize });
	if(xacmeCfRecordMissing(pRoot, iStatus)) Result = XACME_CF_RECORD_MISSING;
	else if(iStatus == 200u && xacmeCfRecordResponseOk(pRoot, true)) {
		xvalue* pRecord = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("result"));
		if(xacmeDnsJsonEqual(pRecord, "id", sRecordId) &&
			xacmeDnsJsonEqual(pRecord, "name", sOwner) &&
			xacmeDnsJsonEqual(pRecord, "type", "TXT") &&
			xacmeDnsJsonEqual(pRecord, "content", sValue)) Result = XACME_CF_RECORD_FOUND;
	}
	xrtValueRelease(pRoot); xrtFree(sResp);
	if(Result != XACME_CF_RECORD_ERROR) xrtClearError();
	else if(xrtErrorKind(xrtGetError()) != XERR_MEMORY)
		xacmeCfError((iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION : XERR_PROTOCOL,
			"acme dns_cf record read failed or no longer matches its tracked identity");
	return Result;
}

static xacmednsownedresult xacmeCfReuseOwned(xacmednscfcontext* pCtx,
	xstrview Owner, xstrview Txt)
{
	char sOwner[256];
	/* Invalid arguments are diagnosed by the ordinary create path. */
	if(!xacmeDnsChallengeValid(Owner, Txt)) return XACME_DNS_OWNED_NONE;
	Owner = xacmeDnsCanonicalOwner(Owner, sOwner);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, Owner, Txt)) {
		(void)xacmeDnsCreateUncertainError(); return XACME_DNS_OWNED_ERROR;
	}
	for(;;) {
		char sZoneId[64], sRecordId[64], sPath[200];
		size_t iSlot = xacmeDnsRecordFindOwned(&pCtx->Records, Owner, Txt);
		xacmecfrecordresult Result;
		if(iSlot == XACME_DNS_RECORD_MAX) return XACME_DNS_OWNED_NONE;
		if(!xacmeDnsRecordSplit(pCtx->Records.sIds[iSlot], '/', sZoneId,
				sizeof(sZoneId), sRecordId, sizeof(sRecordId))) {
			xacmeCfError(XERR_PROTOCOL, "acme dns_cf tracked record handle is invalid");
			return XACME_DNS_OWNED_ERROR;
		}
		snprintf(sPath, sizeof(sPath), "/client/v4/zones/%s/dns_records/%s", sZoneId, sRecordId);
		Result = xacmeCfReadRecord(pCtx, sPath, sRecordId, Owner, Txt);
		if(Result == XACME_CF_RECORD_FOUND) return XACME_DNS_OWNED_VALID;
		if(Result != XACME_CF_RECORD_MISSING) return XACME_DNS_OWNED_ERROR;
		xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
	}
}

static bool xacmeCfDeleteUncertain(void)
{
	xerror* pError;
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	pError = xrtErrorWrap(xrtGetError(), XERR_IO, "xrt.acme.dns",
		XACME_DNS_ERROR_UNCERTAIN, "DNS deletion outcome is unknown; tracked record retained");
	if(pError != NULL) xrtSetErrorTake(pError);
	return false;
}

static bool xacmeCfDeleteRecord(void* pContext, cstr sId,
	xstrview sFqdn, xstrview sTxt)
{
	xacmednscfcontext* pCtx = (xacmednscfcontext*)pContext;
	char sZoneId[64];
	char sRecordId[64];
	char sPath[200];
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	bool bOk;
	xvalue* pRoot = NULL;
	xacmecfrecordresult Result;
	if(!xacmeDnsRecordSplit(sId, '/', sZoneId, sizeof(sZoneId),
			sRecordId, sizeof(sRecordId)))
	{
		xacmeCfError(XERR_PROTOCOL, "acme dns_cf record handle is invalid");
		return false;
	}
	snprintf(sPath, sizeof(sPath),
		"/client/v4/zones/%s/dns_records/%s", sZoneId, sRecordId);
	Result = xacmeCfReadRecord(pCtx, sPath, sRecordId, sFqdn, sTxt);
	if(Result != XACME_CF_RECORD_FOUND) return Result == XACME_CF_RECORD_MISSING;
	bOk = xacmeCfCall(pCtx, "DELETE", sPath, NULL, &iStatus, &sResp, &iRespSize);
	if(!bOk && !pCtx->Http.bWriteUncertain) { xrtFree(sResp); return false; }
	if(bOk) {
		pRoot = xrtJsonParse((xstrview){ sResp, iRespSize });
		bOk = iStatus == 200u && xacmeCfRecordResponseOk(pRoot, false) &&
			xacmeDnsJsonEqual(xrtValueObjectGet(pRoot, XRT_STR_LITERAL("result")), "id", sRecordId);
		if(!bOk && (iStatus == 401u || iStatus == 403u) && xacmeCfCreateRejected(pRoot, iStatus) &&
			xrtErrorKind(xrtGetError()) != XERR_MEMORY) {
			xrtValueRelease(pRoot); xrtFree(sResp);
			xacmeCfError(XERR_PERMISSION, "acme dns_cf record deletion was rejected"); return false;
		}
	}
	xrtValueRelease(pRoot); xrtFree(sResp);
	if(bOk) return true;
	(void)xacmeCfDeleteUncertain();
	/* Allocation failures retain both the diagnosis and handle. A subsequent
 * Remove can reconcile; no write is repeated within this call. */
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	{
		xerror* pWriteError = xrtTakeError();
		Result = xacmeCfReadRecord(pCtx, sPath, sRecordId, sFqdn, sTxt);
		if(Result == XACME_CF_RECORD_FOUND) xrtSetErrorTake(pWriteError);
		else xrtErrorFree(pWriteError);
	}
	return Result == XACME_CF_RECORD_MISSING;
}

static bool xacmeCfRemoveLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednscfcontext* pCtx = (xacmednscfcontext*)pProvider->pContext;
	char sOwner[256];
	if(!xacmeDnsChallengeValid(sFqdn, sTxt)) {
		xacmeCfError(XERR_ARGUMENT, "acme dns_cf owner or digest is invalid"); return false;
	}
	sFqdn = xacmeDnsCanonicalOwner(sFqdn, sOwner);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, sFqdn, sTxt))
		return xacmeDnsCreateUncertainError();
	return xacmeDnsRecordRemoveMatching(&pCtx->Records, sFqdn, sTxt,
		xacmeCfDeleteRecord, pCtx);
}

static bool xacmeCfAdd(xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednscfcontext* pCtx = (xacmednscfcontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	xacmednsownedresult Existing;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	Existing = xacmeCfReuseOwned(pCtx, sFqdn, sTxt);
	bOk = Existing == XACME_DNS_OWNED_VALID ||
		(Existing == XACME_DNS_OWNED_NONE && xacmeCfAddLocked(pProvider, sFqdn, sTxt));
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

static bool xacmeCfRemove(xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednscfcontext* pCtx = (xacmednscfcontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeCfRemoveLocked(pProvider, sFqdn, sTxt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
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
	if(strlen(pConfig->sApiToken) >= sizeof(pCtx->sToken) ||
		((pConfig->sEndpoint != NULL) &&
		 strlen(pConfig->sEndpoint) >= sizeof(pCtx->sEndpoint)))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_cf token or endpoint exceeds capacity");
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
	if(!xrtMutexInit(&pCtx->Lock))
	{
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		return false;
	}
	if(!xacmeHttpInit(&pCtx->Http, pBorrowedEngine, NULL, 0u))
	{
		(void)xrtMutexUnit(&pCtx->Lock);
		if(xacmeHttpUnit(&pCtx->Http))
		{
			xrtSecureZero(pCtx, sizeof(*pCtx));
			xrtFree(pCtx);
		}
		else xacmeHttpDeferOwner(&pCtx->Http, sizeof(*pCtx));
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
		if(!xacmeHttpUnit(&pCtx->Http)) return;
		(void)xrtMutexUnit(&pCtx->Lock);
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
