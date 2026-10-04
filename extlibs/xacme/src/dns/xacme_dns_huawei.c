#include <xrt/acme_dns_huawei.h>

#if defined(XACME_FEATURE_DNS_HUAWEI)

#include "../internal/xacme_dnscommon.h"
#include "../internal/xacme_dns_huawei_internal.h"
#include "../internal/xacme_http.h"
#include "../internal/xacme_sigv4.h"

#include <xrt/buffer.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <xrt/time.h>

#include <stdarg.h>
#include <stdlib.h>

/*
	华为云 DNS provider（API v2，SDK-HMAC-SHA256）：
	  - canonical：content-type/host/x-sdk-date 三头（小写字典序）；
	  - StringToSign = "SDK-HMAC-SHA256\n<X-Sdk-Date>\n"
	    "<sha256hex(canonical)>"（无凭据范围）；
	  - 签名：HMAC(SK, StringToSign)，canonical URI 与查询串分开；
	  - zone 发现：GET /v2/zones?name=<候选>&limit=2&search_mode=equal；
	  - 加 TXT：POST /v2/zones/<id>/recordsets
	    （name 带尾点，records 值内嵌双引号）；
	  - 删 TXT：DELETE /v2/zones/<id>/recordsets/<recordset id>。
*/

typedef struct xacmednshuaaweicontext {
	xacmehttp Http;
	xmutex Lock;
	char sAk[160];
	char sSk[160];
	char sEndpoint[160];
	xacmednsrecords Records;
	bool bUncertain[XACME_DNS_RECORD_MAX];
	bool bDeletePending[XACME_DNS_RECORD_MAX];
	bool bDeleteAccepted[XACME_DNS_RECORD_MAX];
} xacmednshuaaweicontext;

static void xacmeHuaweiError(xerrkind Kind, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
		sMessage);
}

bool xacmeDnsHuaweiBuildUrl(
	char* sOutput, size_t iCapacity, cstr sEndpoint, cstr sPathAndQuery)
{
	return xacmeDnsHttpsUrl(sOutput, iCapacity, sEndpoint,
		sPathAndQuery);
}

static bool xacmeHuaweiFormat(char* sOut, size_t iCapacity,
	cstr sFormat, ...)
{
	va_list Args;
	int iWritten;
	va_start(Args, sFormat);
	iWritten = vsnprintf(sOut, iCapacity, sFormat, Args);
	va_end(Args);
	if((iWritten < 0) || ((size_t)iWritten >= iCapacity))
	{
		if((sOut != NULL) && (iCapacity > 0u))
		{
			sOut[0] = '\0';
		}
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_huawei signature input exceeds capacity");
		return false;
	}
	return true;
}

static bool xacmeHuaweiUnreserved(char c)
{
	return ((c >= 'A') && (c <= 'Z')) ||
		((c >= 'a') && (c <= 'z')) ||
		((c >= '0') && (c <= '9')) ||
		(c == '-') || (c == '_') || (c == '.') || (c == '~');
}

/* 当前 provider 只构造 ASCII unreserved 路径/参数；拒绝未编码的分隔符。 */
bool xacmeDnsHuaweiCanonicalTarget(cstr sTarget,
	char* sUri, size_t iUriCapacity, char* sQuery, size_t iQueryCapacity)
{
	char sParts[320];
	char* pParts[16];
	const char* pQuery;
	size_t iPathSize;
	size_t iQuerySize;
	size_t iCount = 0u;
	size_t i;
	size_t iUsed = 0u;
	if((sTarget == NULL) || (sUri == NULL) || (sQuery == NULL) ||
		(iUriCapacity == 0u) || (iQueryCapacity == 0u))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT, "acme dns_huawei target is invalid");
		return false;
	}
	sUri[0] = '\0';
	sQuery[0] = '\0';
	pQuery = strchr(sTarget, '?');
	iPathSize = (pQuery != NULL) ? (size_t)(pQuery - sTarget) :
		strlen(sTarget);
	if((iPathSize == 0u) || (sTarget[0] != '/') ||
		(iPathSize + ((sTarget[iPathSize - 1u] == '/') ? 1u : 2u) >
			iUriCapacity))
	{
		goto Invalid;
	}
	for(i = 0u; i < iPathSize; ++i)
	{
		if(!xacmeHuaweiUnreserved(sTarget[i]) && (sTarget[i] != '/'))
		{
			goto Invalid;
		}
		if((sTarget[i] == '.') && ((i == 0u) || (sTarget[i - 1u] == '/')) &&
			((i + 1u == iPathSize) || (sTarget[i + 1u] == '/') ||
			((sTarget[i + 1u] == '.') &&
				((i + 2u == iPathSize) || (sTarget[i + 2u] == '/')))))
		{
			goto Invalid;
		}
	}
	memcpy(sUri, sTarget, iPathSize);
	if(sUri[iPathSize - 1u] != '/')
	{
		sUri[iPathSize++] = '/';
	}
	sUri[iPathSize] = '\0';
	if(pQuery == NULL)
	{
		return true;
	}
	++pQuery;
	iQuerySize = strlen(pQuery);
	if((iQuerySize == 0u) || (iQuerySize >= sizeof(sParts)))
	{
		goto Invalid;
	}
	for(i = 0u; i < iQuerySize; ++i)
	{
		if(!xacmeHuaweiUnreserved(pQuery[i]) &&
			(pQuery[i] != '=') && (pQuery[i] != '&'))
		{
			goto Invalid;
		}
	}
	memcpy(sParts, pQuery, iQuerySize + 1u);
	pParts[iCount++] = sParts;
	for(i = 0u; i < iQuerySize; ++i)
	{
		if(sParts[i] == '&')
		{
			if((iCount >= (sizeof(pParts) / sizeof(pParts[0]))) ||
				(i == 0u) || (sParts[i + 1u] == '\0') ||
				(sParts[i - 1u] == '&'))
			{
				goto Invalid;
			}
			sParts[i] = '\0';
			pParts[iCount++] = sParts + i + 1u;
		}
	}
	for(i = 0u; i < iCount; ++i)
	{
		size_t j;
		char* pEquals = strchr(pParts[i], '=');
		if((pEquals == NULL) || (pParts[i][0] == '=') ||
			(strchr(pEquals + 1u, '=') != NULL))
		{
			goto Invalid;
		}
		for(j = i + 1u; j < iCount; ++j)
		{
			if(strcmp(pParts[i], pParts[j]) > 0)
			{
				char* pSwap = pParts[i];
				pParts[i] = pParts[j];
				pParts[j] = pSwap;
			}
		}
	}
	for(i = 0u; i < iCount; ++i)
	{
		size_t iPartSize = strlen(pParts[i]);
		if((iUsed + iPartSize + ((i > 0u) ? 1u : 0u) + 1u) >
			iQueryCapacity)
		{
			goto Invalid;
		}
		if(i > 0u)
		{
			sQuery[iUsed++] = '&';
		}
		memcpy(sQuery + iUsed, pParts[i], iPartSize);
		iUsed += iPartSize;
		sQuery[iUsed] = '\0';
	}
	return true;
Invalid:
	sUri[0] = '\0';
	sQuery[0] = '\0';
	xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
		XACME_DNS_ERROR_ARGUMENT,
		"acme dns_huawei canonical target is invalid or too long");
	return false;
}

bool xacmeDnsHuaweiAuthorization(
	cstr sAk, cstr sSk, cstr sEndpoint, cstr sMethod,
	cstr sPathAndQuery, cstr sBody, xtime iNow,
	char* sAuth, size_t iAuthCapacity,
	char* sStamp, size_t iStampCapacity)
{
	static const char* sSignedHeaders = "content-type;host;x-sdk-date";
	char sPayloadHash[XACME_SIG_HASH_TEXT];
	char sCanonical[1600];
	char sHeaders[360];
	char sStringToSign[160];
	char sHex[XACME_SIG_HASH_TEXT];
	char sUri[512];
	char sQuery[320];
	xdatetime Now;
	uint8 Signature[XRT_SHA256_SIZE];
	if((sAk == NULL) || (sSk == NULL) || (sEndpoint == NULL) ||
		(sMethod == NULL) || (sPathAndQuery == NULL) ||
		(sAuth == NULL) || (sStamp == NULL) ||
		(iAuthCapacity == 0u) || (iStampCapacity == 0u))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_huawei signature arguments are invalid");
		return false;
	}
	sAuth[0] = '\0';
	sStamp[0] = '\0';
	if(!xrtTimeSplitAt(iNow, 0, &Now) ||
		!xacmeHuaweiFormat(sStamp, iStampCapacity,
			"%04ld%02d%02dT%02d%02d%02dZ", (long)Now.Year,
			Now.Month, Now.Day, Now.Hour, Now.Minute, Now.Second) ||
		!xacmeDnsHuaweiCanonicalTarget(sPathAndQuery,
			sUri, sizeof(sUri), sQuery, sizeof(sQuery)) ||
		!xacmeSigSha256Hex((sBody != NULL) ? sBody : "",
			(sBody != NULL) ? strlen(sBody) : 0u, sPayloadHash) ||
		!xacmeHuaweiFormat(sHeaders, sizeof(sHeaders),
			"content-type:application/json\nhost:%s\nx-sdk-date:%s\n",
			sEndpoint, sStamp) ||
		!xacmeSigCanonical(sCanonical, sizeof(sCanonical), sMethod,
			sUri, sQuery, sHeaders, sSignedHeaders, sPayloadHash) ||
		!xacmeSigSha256Hex(sCanonical, strlen(sCanonical), sHex) ||
		!xacmeHuaweiFormat(sStringToSign, sizeof(sStringToSign),
			"SDK-HMAC-SHA256\n%s\n%s", sStamp, sHex))
	{
		return false;
	}
	if(!xacmeSigHmac((const uint8*)sSk, strlen(sSk), sStringToSign,
			strlen(sStringToSign), Signature))
	{
		xrtSecureZero(Signature, sizeof(Signature));
		return false;
	}
	xacmeSigHex(Signature, sizeof(Signature), sHex);
	xrtSecureZero(Signature, sizeof(Signature));
	return xacmeHuaweiFormat(sAuth, iAuthCapacity,
		"SDK-HMAC-SHA256 Access=%s, SignedHeaders=%s, Signature=%s",
		sAk, sSignedHeaders, sHex);
}

static bool xacmeHuaweiCall(
	xacmednshuaaweicontext* pCtx, cstr sMethod, cstr sPathAndQuery,
	cstr sBody, uint16* pOutStatus, str* pOutBody, size_t* pOutSize)
{
	char sStampText[24];
	char sAuth[560];
	char sUrl[512];
	xacmehttpheader Extra[3];
	xacmehttpresponse R;

	pCtx->Http.bWriteUncertain = false;
	if(!xacmeDnsHuaweiAuthorization(pCtx->sAk, pCtx->sSk,
			pCtx->sEndpoint, sMethod, sPathAndQuery, sBody, xrtNow(),
			sAuth, sizeof(sAuth), sStampText, sizeof(sStampText)) ||
		!xacmeDnsHuaweiBuildUrl(sUrl, sizeof(sUrl), pCtx->sEndpoint,
			sPathAndQuery))
	{
		return false;
	}

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
	if(pOutSize != NULL) *pOutSize = R.iBodySize;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	return true;
}

/* zones?name= 精确匹配候选（zone 名带尾点，比较时剥除）。 */
static xacmednszoneresult xacmeHuaweiZoneId(
	xacmednshuaaweicontext* pCtx, cstr sZone, char* sOutId, size_t iIdCap)
{
	char sPath[300];
	char sWantDot[280];
	uint16 iStatus = 0u;
	str sBody = NULL;
	size_t iBodySize = 0u;
	xacmednszoneresult Result;

	if(!xacmeHuaweiFormat(sPath, sizeof(sPath),
			"/v2/zones?name=%s.&limit=2&search_mode=equal", sZone) ||
		!xacmeHuaweiFormat(sWantDot, sizeof(sWantDot), "%s.", sZone))
	{
		return XACME_DNS_ZONE_ERROR;
	}
	if(!xacmeHuaweiCall(pCtx, "GET", sPath, NULL, &iStatus, &sBody, &iBodySize))
	{
		return XACME_DNS_ZONE_ERROR;
	}
	if(iStatus == 200u)
		Result = xacmeDnsJsonZoneId(
			(xstrview){ sBody, iBodySize },
			"zones", sWantDot, false, 2u, sOutId, iIdCap);
	else
	{
		Result = XACME_DNS_ZONE_ERROR;
		xacmeHuaweiError(
			(iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION :
			(iStatus == 429u || iStatus >= 500u) ? XERR_AGAIN : XERR_PROTOCOL,
			"acme dns_huawei zone query rejected or unavailable");
	}
	xrtFree(sBody);
	return Result;
}

static bool xacmeHuaweiFindZone(
	xacmednshuaaweicontext* pCtx, cstr sFqdn, char* sOutZone,
	size_t iZoneCap, char* sOutId, size_t iIdCap)
{
	char sCandidate[256];
	xacmednszoneresult Lookup;
	snprintf(sCandidate, sizeof(sCandidate), "%s", sFqdn);
	for(;;)
	{
		char* sDot;
		Lookup = xacmeHuaweiZoneId(pCtx, sCandidate, sOutId, iIdCap);
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
		"acme dns_huawei managed zone not found");
	return false;
}

bool xacmeDnsHuaweiBuildCreateBody(
	xbuffer* pBody, xstrview sFqdn, xstrview sTxt)
{
	char sDotted[257];
	char sQuotedTxt[203];
	if((pBody == NULL) || (sFqdn.Data == NULL) || (sTxt.Data == NULL) ||
		(sFqdn.Size == 0u) || (sFqdn.Size > 255u) ||
		(sTxt.Size == 0u) || (sTxt.Size > 200u))
		return false;
	memcpy(sDotted, sFqdn.Data, sFqdn.Size);
	sDotted[sFqdn.Size] = '.';
	sDotted[sFqdn.Size + 1u] = '\0';
	sQuotedTxt[0] = '"';
	memcpy(sQuotedTxt + 1u, sTxt.Data, sTxt.Size);
	sQuotedTxt[sTxt.Size + 1u] = '"';
	sQuotedTxt[sTxt.Size + 2u] = '\0';
	return xrtBufferAppend(pBody, XRT_BYTES_LITERAL("{\"name\":")) &&
		xacmeDnsJsonQuote(pBody,
			(xstrview){ sDotted, sFqdn.Size + 1u }) &&
		xrtBufferAppend(pBody, XRT_BYTES_LITERAL(
			",\"type\":\"TXT\",\"ttl\":60,\"records\":[")) &&
		xacmeDnsJsonQuote(pBody,
			(xstrview){ sQuotedTxt, sTxt.Size + 2u }) &&
		xrtBufferAppend(pBody, XRT_BYTES_LITERAL("]}"));
}

static bool xacmeHuaweiCreateRejected(const xvalue* pRoot, uint16 iStatus)
{
	char sCode[128], sMessage[512];
	return iStatus >= 400u && iStatus < 500u && pRoot != NULL && xrtValueIs(pRoot, XVALUE_OBJECT) &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("id")) == NULL &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("name")) == NULL &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("type")) == NULL &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("zone_id")) == NULL &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("records")) == NULL &&
		xacmeDnsJsonText(pRoot, "error_code", sCode, sizeof(sCode)) && sCode[0] != '\0' &&
		xacmeDnsJsonText(pRoot, "error_msg", sMessage, sizeof(sMessage)) && sMessage[0] != '\0';
}

static bool xacmeHuaweiCreateIdentity(const xvalue* pRoot, cstr sOwner,
	cstr sZoneId, xstrview Txt)
{
	char sDotted[257], sQuoted[203];
	xvalue* pRecords;
	xstrview Value;
	snprintf(sDotted, sizeof(sDotted), "%s.", sOwner);
	sQuoted[0] = '"'; memcpy(sQuoted + 1u, Txt.Data, Txt.Size);
	sQuoted[Txt.Size + 1u] = '"'; sQuoted[Txt.Size + 2u] = '\0';
	if(pRoot == NULL || !xrtValueIs(pRoot, XVALUE_OBJECT) ||
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("error_code")) != NULL ||
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("error_msg")) != NULL ||
		!xacmeDnsJsonEqual(pRoot, "name", sDotted) || !xacmeDnsJsonEqual(pRoot, "type", "TXT") ||
		!xacmeDnsJsonEqual(pRoot, "zone_id", sZoneId)) return false;
	pRecords = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("records"));
	return xrtValueIs(pRecords, XVALUE_ARRAY) && xrtValueCount(pRecords) == 1u &&
		xrtValueGetString(xrtValueArrayGet(pRecords, 0u), &Value) &&
		Value.Size == Txt.Size + 2u && memcmp(Value.Data, sQuoted, Value.Size) == 0;
}

static bool xacmeHuaweiDeletePendingError(void)
{
	if(xrtErrorKind(xrtGetError()) != XERR_MEMORY)
		xrtSetErrorInfo(XERR_AGAIN, "xrt.acme.dns", XACME_DNS_ERROR_NETWORK,
			"acme dns_huawei deletion is not complete or recordset is not steady");
	return false;
}

static bool xacmeHuaweiAddLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednshuaaweicontext* pCtx = (xacmednshuaaweicontext*)pProvider->pContext;
	char sFqdnText[256];
	char sZone[256];
	char sZoneId[80];
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	xvalue* pRoot = NULL;
	char sRecordId[80];
	bool bOk = false;
	bool bTracked = false;
	bool bSent = false;
	size_t iSlot;

	if(!xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_huawei owner or digest is invalid");
		return false;
	}
	sFqdn = xacmeDnsCanonicalOwner(sFqdn, sFqdnText);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, sFqdn, sTxt))
		return xacmeDnsCreateUncertainError();
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bDeletePending, sFqdn, sTxt))
		return xacmeHuaweiDeletePendingError();
	iSlot = xacmeDnsCreateSlot(&pCtx->Records, pCtx->bUncertain);
	if(iSlot >= XACME_DNS_RECORD_MAX)
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_huawei record tracking capacity exhausted");
		return false;
	}

	if(!xacmeHuaweiFindZone(pCtx, sFqdnText, sZone, sizeof(sZone), sZoneId,
			sizeof(sZoneId)))
	{
		return false;
	}

	/* name 带尾点；records 值必须内嵌双引号。 */
	xrtBufferInit(&Body);
	if(xacmeDnsHuaweiBuildCreateBody(&Body, sFqdn, sTxt) &&
		xrtBufferAppendByte(&Body, 0u))
	{
		char sPath[128];
		snprintf(sPath, sizeof(sPath), "/v2/zones/%s/recordsets",
			sZoneId);
		xacmeDnsCreateReserve(&pCtx->Records, pCtx->bUncertain, iSlot, sFqdn, sTxt);
		bSent = true;
		bOk = xacmeHuaweiCall(pCtx, "POST", sPath, (cstr)Body.Data,
			&iStatus, &sResp, &iRespSize);
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
	if(sResp != NULL)
	{
		pRoot = xrtJsonParse((xstrview){ sResp, iRespSize });
	}
	if(xacmeHuaweiCreateRejected(pRoot, iStatus)) {
		xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
		xrtValueRelease(pRoot); xrtFree(sResp);
		xacmeHuaweiError((iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION : XERR_PROTOCOL,
			"acme dns_huawei create was rejected");
		return false;
	}
	if(iStatus >= 200u && iStatus < 300u && xacmeHuaweiCreateIdentity(pRoot, sFqdnText, sZoneId, sTxt) &&
		xacmeDnsJsonPathId(pRoot, "id", sRecordId, sizeof(sRecordId)))
	{
		bTracked = xacmeDnsCreateCommit(&pCtx->Records, pCtx->bUncertain,
			iSlot, sZoneId, '|', sRecordId);
	}
	xrtValueRelease(pRoot);
	xrtFree(sResp);
	return bTracked ? true : xacmeDnsCreateUncertainError();
}

static bool xacmeHuaweiRecordMissing(const xvalue* pRoot, uint16 iStatus)
{
	return iStatus == 404u && xacmeHuaweiCreateRejected(pRoot, iStatus) &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("status")) == NULL &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("default")) == NULL &&
		xacmeDnsJsonEqual(pRoot, "error_code", "DNS.0313");
}

static bool xacmeHuaweiRecordIdentity(const xvalue* pRoot, cstr sOwner,
	cstr sZoneId, cstr sRecordId, xstrview Txt, bool bRequireValues)
{
	char sDotted[257];
	bool bDefault = true;
	snprintf(sDotted, sizeof(sDotted), "%s.", sOwner);
	if(!xrtValueIs(pRoot, XVALUE_OBJECT) ||
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("error_code")) != NULL ||
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("error_msg")) != NULL ||
		!xacmeDnsJsonEqual(pRoot, "id", sRecordId) ||
		!xacmeDnsJsonEqual(pRoot, "name", sDotted) ||
		!xacmeDnsJsonEqual(pRoot, "type", "TXT") ||
		!xacmeDnsJsonEqual(pRoot, "zone_id", sZoneId) ||
		!xrtValueGetBool(xrtValueObjectGet(pRoot, XRT_STR_LITERAL("default")), &bDefault) || bDefault) return false;
	/* The documented DELETE example omits records. Present values must still
	 * match the unique owned TXT; GET always requires the complete value list. */
	return (!bRequireValues && xrtValueObjectGet(pRoot, XRT_STR_LITERAL("records")) == NULL) ||
		xacmeHuaweiCreateIdentity(pRoot, sOwner, sZoneId, Txt);
}

typedef enum xacmehuaweirecordresult {
	XACME_HUAWEI_RECORD_ERROR,
	XACME_HUAWEI_RECORD_FOUND,
	XACME_HUAWEI_RECORD_MISSING,
	XACME_HUAWEI_RECORD_DELETING,
	XACME_HUAWEI_RECORD_BUSY,
	XACME_HUAWEI_RECORD_INACTIVE
} xacmehuaweirecordresult;

static xacmehuaweirecordresult xacmeHuaweiReadRecord(xacmednshuaaweicontext* pCtx,
	cstr sPath, cstr sZoneId, cstr sRecordId, cstr sOwner, xstrview Txt, bool bAdding)
{
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	xvalue* pRoot;
	xacmehuaweirecordresult Result = XACME_HUAWEI_RECORD_ERROR;
	char sStatus[32];
	xrtClearError();
	if(!xacmeHuaweiCall(pCtx, "GET", sPath, NULL, &iStatus, &sResp, &iRespSize)) {
		xrtFree(sResp); return Result;
	}
	pRoot = xrtJsonParse((xstrview){ sResp, iRespSize });
	if(xacmeHuaweiRecordMissing(pRoot, iStatus)) Result = XACME_HUAWEI_RECORD_MISSING;
	else if(iStatus == 200u && xacmeHuaweiRecordIdentity(pRoot, sOwner, sZoneId, sRecordId, Txt, true) &&
		xacmeDnsJsonText(pRoot, "status", sStatus, sizeof(sStatus))) {
		if(strcmp(sStatus, "PENDING_DELETE") == 0) Result = XACME_HUAWEI_RECORD_DELETING;
		else if(strcmp(sStatus, "ACTIVE") == 0) Result = XACME_HUAWEI_RECORD_FOUND;
		else if(strcmp(sStatus, "DISABLE") == 0 ||
			strcmp(sStatus, "FREEZE") == 0 || strcmp(sStatus, "ILLEGAL") == 0 ||
			strcmp(sStatus, "POLICE") == 0 || strcmp(sStatus, "ERROR") == 0)
			Result = bAdding ? XACME_HUAWEI_RECORD_INACTIVE : XACME_HUAWEI_RECORD_FOUND;
		else if(strcmp(sStatus, "PENDING_CREATE") == 0 || strcmp(sStatus, "PENDING_UPDATE") == 0 ||
			strcmp(sStatus, "PENDING_FREEZE") == 0 || strcmp(sStatus, "PENDING_DISABLE") == 0) Result = XACME_HUAWEI_RECORD_BUSY;
	}
	xrtValueRelease(pRoot); xrtFree(sResp);
	if(Result == XACME_HUAWEI_RECORD_BUSY) (void)xacmeHuaweiDeletePendingError();
	else if(Result == XACME_HUAWEI_RECORD_INACTIVE)
		xacmeHuaweiError(XERR_STATE, "acme dns_huawei owned recordset is not active");
	else if(Result != XACME_HUAWEI_RECORD_ERROR) xrtClearError();
	else if(xrtErrorKind(xrtGetError()) != XERR_MEMORY)
		xacmeHuaweiError((iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION : XERR_PROTOCOL,
			"acme dns_huawei record read failed or does not match its tracked identity");
	return Result;
}

static xacmednsownedresult xacmeHuaweiReuseOwned(xacmednshuaaweicontext* pCtx,
	xstrview Owner, xstrview Txt)
{
	char sOwner[256];
	if(!xacmeDnsChallengeValid(Owner, Txt)) return XACME_DNS_OWNED_NONE;
	Owner = xacmeDnsCanonicalOwner(Owner, sOwner);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, Owner, Txt)) {
		(void)xacmeDnsCreateUncertainError(); return XACME_DNS_OWNED_ERROR;
	}
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bDeletePending, Owner, Txt)) {
		(void)xacmeHuaweiDeletePendingError(); return XACME_DNS_OWNED_ERROR;
	}
	for(;;) {
		char sZoneId[80], sRecordId[80], sPath[200];
		size_t iSlot = xacmeDnsRecordFindOwned(&pCtx->Records, Owner, Txt);
		xacmehuaweirecordresult Result;
		if(iSlot == XACME_DNS_RECORD_MAX) return XACME_DNS_OWNED_NONE;
		if(!xacmeDnsRecordSplit(pCtx->Records.sIds[iSlot], '|', sZoneId,
				sizeof(sZoneId), sRecordId, sizeof(sRecordId))) {
			xacmeHuaweiError(XERR_PROTOCOL, "acme dns_huawei tracked record handle is invalid");
			return XACME_DNS_OWNED_ERROR;
		}
		snprintf(sPath, sizeof(sPath), "/v2/zones/%s/recordsets/%s", sZoneId, sRecordId);
		Result = xacmeHuaweiReadRecord(pCtx, sPath, sZoneId, sRecordId, sOwner, Txt, true);
		if(Result == XACME_HUAWEI_RECORD_FOUND) return XACME_DNS_OWNED_VALID;
		if(Result == XACME_HUAWEI_RECORD_DELETING) {
			pCtx->bDeletePending[iSlot] = true; pCtx->bDeleteAccepted[iSlot] = true;
			(void)xacmeHuaweiDeletePendingError(); return XACME_DNS_OWNED_ERROR;
		}
		if(Result != XACME_HUAWEI_RECORD_MISSING) return XACME_DNS_OWNED_ERROR;
		pCtx->bDeletePending[iSlot] = false; pCtx->bDeleteAccepted[iSlot] = false;
		xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
	}
}

static bool xacmeHuaweiAwaitDelete(xacmednshuaaweicontext* pCtx, size_t iSlot,
	cstr sPath, cstr sZoneId, cstr sRecordId, cstr sOwner, xstrview Txt)
{
	uint32 iAttempt;
	static const uint32 Delays[] = { 500u, 1000u, 2000u };
	for(iAttempt = 0u; iAttempt < 4u; iAttempt++) {
		xacmehuaweirecordresult Result;
		if(iAttempt != 0u) xrtSleep(Delays[iAttempt - 1u]);
		Result = xacmeHuaweiReadRecord(pCtx, sPath, sZoneId, sRecordId, sOwner, Txt, false);
		if(Result == XACME_HUAWEI_RECORD_MISSING) {
			pCtx->bDeletePending[iSlot] = false;
			pCtx->bDeleteAccepted[iSlot] = false;
			return true;
		}
		if(Result == XACME_HUAWEI_RECORD_ERROR || Result == XACME_HUAWEI_RECORD_BUSY) return false;
		if(Result == XACME_HUAWEI_RECORD_DELETING) {
			pCtx->bDeletePending[iSlot] = true;
			pCtx->bDeleteAccepted[iSlot] = true;
		}
		/* Without a verified acceptance, an ACTIVE record does not prove that
		 * the uncertain write committed. Leave its error and handle to caller. */
		if(!pCtx->bDeleteAccepted[iSlot]) return false;
	}
	return xacmeHuaweiDeletePendingError();
}

static bool xacmeHuaweiDeleteUncertain(void)
{
	xerror* pError;
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	pError = xrtErrorWrap(xrtGetError(), XERR_IO, "xrt.acme.dns",
		XACME_DNS_ERROR_UNCERTAIN, "Huawei DNS deletion outcome unknown; original record retained");
	if(pError != NULL) xrtSetErrorTake(pError);
	return false;
}

static bool xacmeHuaweiDeleteRecord(void* pContext, cstr sId,
	xstrview sFqdn, xstrview sTxt)
{
	xacmednshuaaweicontext* pCtx = (xacmednshuaaweicontext*)pContext;
	char sZoneId[80];
	char sRecordId[80];
	char sPath[200];
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	bool bOk;
	bool bPreviousPending;
	char sOwner[256];
	size_t iSlot;
	xvalue* pRoot = NULL;
	xacmehuaweirecordresult Result;
	if(!xacmeDnsRecordSplit(sId, '|', sZoneId, sizeof(sZoneId),
			sRecordId, sizeof(sRecordId)))
	{
		xacmeHuaweiError(XERR_PROTOCOL, "acme dns_huawei tracked record handle is invalid"); return false;
	}
	for(iSlot = 0u; iSlot < pCtx->Records.iCount; iSlot++)
		if(strcmp(pCtx->Records.sIds[iSlot], sId) == 0) break;
	if(iSlot == pCtx->Records.iCount) {
		xacmeHuaweiError(XERR_STATE, "acme dns_huawei record is not tracked"); return false;
	}
	memcpy(sOwner, sFqdn.Data, sFqdn.Size); sOwner[sFqdn.Size] = '\0';
	snprintf(sPath, sizeof(sPath), "/v2/zones/%s/recordsets/%s",
		sZoneId, sRecordId);
	Result = xacmeHuaweiReadRecord(pCtx, sPath, sZoneId, sRecordId, sOwner, sTxt, false);
	if(Result == XACME_HUAWEI_RECORD_MISSING) {
		pCtx->bDeletePending[iSlot] = false; pCtx->bDeleteAccepted[iSlot] = false; return true;
	}
	if(Result == XACME_HUAWEI_RECORD_ERROR || Result == XACME_HUAWEI_RECORD_BUSY) return false;
	if(Result == XACME_HUAWEI_RECORD_DELETING) {
		pCtx->bDeletePending[iSlot] = true; pCtx->bDeleteAccepted[iSlot] = true;
	}
	if(pCtx->bDeleteAccepted[iSlot])
		return xacmeHuaweiAwaitDelete(pCtx, iSlot, sPath, sZoneId, sRecordId, sOwner, sTxt);
	bPreviousPending = pCtx->bDeletePending[iSlot];
	pCtx->bDeletePending[iSlot] = true;
	bOk = xacmeHuaweiCall(pCtx, "DELETE", sPath, NULL, &iStatus, &sResp, &iRespSize);
	if(!bOk && !pCtx->Http.bWriteUncertain) {
		pCtx->bDeletePending[iSlot] = bPreviousPending; xrtFree(sResp); return false;
	}
	if(bOk) {
		pRoot = xrtJsonParse((xstrview){ sResp, iRespSize });
		bOk = iStatus == 202u && xacmeHuaweiRecordIdentity(pRoot, sOwner, sZoneId, sRecordId, sTxt, false) &&
			xacmeDnsJsonEqual(pRoot, "status", "PENDING_DELETE");
		if(!bOk && xacmeHuaweiCreateRejected(pRoot, iStatus) && !xacmeHuaweiRecordMissing(pRoot, iStatus) &&
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("status")) == NULL &&
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("default")) == NULL &&
			xrtErrorKind(xrtGetError()) != XERR_MEMORY) {
			bool bBusy = xacmeDnsJsonEqual(pRoot, "error_code", "DNS.0314");
			pCtx->bDeletePending[iSlot] = bPreviousPending;
			xrtValueRelease(pRoot); xrtFree(sResp);
			if(bBusy) return xacmeHuaweiDeletePendingError();
			xacmeHuaweiError((iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION : XERR_PROTOCOL,
				"acme dns_huawei record deletion was rejected"); return false;
		}
	}
	xrtValueRelease(pRoot); xrtFree(sResp);
	if(bOk) pCtx->bDeleteAccepted[iSlot] = true;
	else (void)xacmeHuaweiDeleteUncertain();
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	{
		xerror* pWriteError = xrtTakeError();
		bOk = xacmeHuaweiAwaitDelete(pCtx, iSlot, sPath, sZoneId, sRecordId, sOwner, sTxt);
		if(!bOk && xrtErrorKind(xrtGetError()) == XERR_NONE) xrtSetErrorTake(pWriteError);
		else xrtErrorFree(pWriteError);
	}
	return bOk;
}

static bool xacmeHuaweiRemoveLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednshuaaweicontext* pCtx =
		(xacmednshuaaweicontext*)pProvider->pContext;
	char sOwner[256];
	if(!xacmeDnsChallengeValid(sFqdn, sTxt)) {
		xacmeHuaweiError(XERR_ARGUMENT, "acme dns_huawei owner or digest is invalid"); return false;
	}
	sFqdn = xacmeDnsCanonicalOwner(sFqdn, sOwner);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, sFqdn, sTxt))
		return xacmeDnsCreateUncertainError();
	return xacmeDnsRecordRemoveMatching(&pCtx->Records, sFqdn, sTxt,
		xacmeHuaweiDeleteRecord, pCtx);
}

static bool xacmeHuaweiAdd(xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednshuaaweicontext* pCtx = (xacmednshuaaweicontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	xacmednsownedresult Existing;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	Existing = xacmeHuaweiReuseOwned(pCtx, sFqdn, sTxt);
	bOk = Existing == XACME_DNS_OWNED_VALID ||
		(Existing == XACME_DNS_OWNED_NONE && xacmeHuaweiAddLocked(pProvider, sFqdn, sTxt));
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

static bool xacmeHuaweiRemove(xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednshuaaweicontext* pCtx = (xacmednshuaaweicontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeHuaweiRemoveLocked(pProvider, sFqdn, sTxt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
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
	if(strlen(pConfig->sAccessKey) >= sizeof(pCtx->sAk) ||
		strlen(pConfig->sSecretKey) >= sizeof(pCtx->sSk) ||
		((pConfig->sEndpoint != NULL) &&
		 strlen(pConfig->sEndpoint) >= sizeof(pCtx->sEndpoint)))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_huawei credentials or endpoint exceed capacity");
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
		if(!xacmeHttpUnit(&pCtx->Http)) return;
		(void)xrtMutexUnit(&pCtx->Lock);
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
