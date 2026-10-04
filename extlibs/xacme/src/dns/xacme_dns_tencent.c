#include <xrt/acme_dns_tencent.h>

#if defined(XACME_FEATURE_DNS_TENCENT)

#include "../internal/xacme_dnscommon.h"
#include "../internal/xacme_dns_tencent_internal.h"
#include "../internal/xacme_http.h"
#include "../internal/xacme_sigv4.h"

#include <xrt/buffer.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <xrt/time.h>

#include <stdarg.h>
#include <stdlib.h>

/*
	腾讯云 DNSPod provider（API 3.0，TC3-HMAC-SHA256）：
	  - canonical：content-type/host/x-tc-action 三头（小写字典序）；
	  - StringToSign = "TC3-HMAC-SHA256\n<Unix 秒>\n"
	    "<UTC 日期>/dnspod/tc3_request\n" 后接 sha256hex(canonical)；
	  - 密钥链：HMAC("TC3"+SK, date) → "dnspod" → "tc3_request"；
	  - zone 发现：DescribeDomain，绑定不可变 DomainId；
	  - 加 TXT：CreateRecord（RecordLine 必填 "默认"）；
	  - 删 TXT：DescribeRecord 核对后 DeleteRecord（DomainId + RecordId）。
	RecordLine 的 "默认" 是 API 要求的 UTF-8 字面值。
*/

#define XACME_TENCENT_SERVICE "dnspod"
#define XACME_TENCENT_VERSION "2021-03-23"

typedef struct xacmednstencentcontext {
	xacmehttp Http;
	xmutex Lock;
	char sId[160];
	char sKey[160];
	char sEndpoint[160];
	xacmednsrecords Records;
	bool bUncertain[XACME_DNS_RECORD_MAX];
	int64 iDomainIds[XACME_DNS_RECORD_MAX];
	uint64 uCreatedAt[XACME_DNS_RECORD_MAX];
	bool bDeletePending[XACME_DNS_RECORD_MAX];
} xacmednstencentcontext;

static bool xacmeTencentDeleteUncertain(void);

/* 签名输入绝不能静默截断，否则线上的 Authorization 必然无效。 */
static bool xacmeTencentFormat(char* sOut, size_t iCapacity,
	cstr sFormat, ...)
{
	va_list Args;
	int iWritten;
	va_start(Args, sFormat);
	iWritten = vsnprintf(sOut, iCapacity, sFormat, Args);
	va_end(Args);
	if((iWritten < 0) || ((size_t)iWritten >= iCapacity))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_tencent signature input exceeds capacity");
		return false;
	}
	return true;
}

/* TC3 签名与时间头共用同一个时刻，便于离线固定向量验证。 */
bool xacmeDnsTencentAuthorization(
	cstr sId, cstr sKey, cstr sEndpoint, cstr sAction, cstr sBody,
	xtime iNow, char* sAuth, size_t iAuthCapacity,
	char* sTimestamp, size_t iTimestampCapacity)
{
	static const char* sSignedHeaders = "content-type;host;x-tc-action";
	char sDateText[24];      /* YYYY-MM-DD */
	char sPayloadHash[XACME_SIG_HASH_TEXT];
	char sCanonical[1600];
	char sHeaders[320];
	char sStringToSign[200];
	char sHex[XACME_SIG_HASH_TEXT];
	char sKeySeed[180];
	char sLowerAction[64];
	size_t iAction;
	xdatetime Now;
	uint8 kDate[XRT_SHA256_SIZE];
	uint8 kService[XRT_SHA256_SIZE];
	uint8 kSigning[XRT_SHA256_SIZE];
	uint8 Signature[XRT_SHA256_SIZE];
	bool bOk = false;

	if((sId == NULL) || (sKey == NULL) || (sEndpoint == NULL) ||
		(sAction == NULL) || (sBody == NULL) || (sAuth == NULL) ||
		(sTimestamp == NULL) || (iAuthCapacity == 0u) ||
		(iTimestampCapacity == 0u))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_tencent signature arguments are invalid");
		return false;
	}
	sAuth[0] = '\0';
	sTimestamp[0] = '\0';
	if(!xacmeTencentFormat(sLowerAction, sizeof(sLowerAction), "%s", sAction)) return false;
	/* TC3 canonical header values are lowercase, including X-TC-Action. */
	for(iAction = 0u; sLowerAction[iAction] != '\0'; iAction++)
		if(sLowerAction[iAction] >= 'A' && sLowerAction[iAction] <= 'Z')
			sLowerAction[iAction] = (char)(sLowerAction[iAction] + ('a' - 'A'));
	if(!xrtTimeSplitAt(iNow, 0, &Now) ||
		!xacmeTencentFormat(sDateText, sizeof(sDateText),
			"%04ld-%02d-%02d", (long)Now.Year, Now.Month, Now.Day) ||
		!xacmeTencentFormat(sTimestamp, iTimestampCapacity, "%lld",
			(long long)xrtTimeUnix(iNow)))
	{
		return false;
	}
	if(!xacmeSigSha256Hex(sBody, strlen(sBody), sPayloadHash))
	{
		return false;
	}
	if(!xacmeTencentFormat(sHeaders, sizeof(sHeaders),
		"content-type:application/json; charset=utf-8\nhost:%s\n"
		"x-tc-action:%s\n",
		sEndpoint, sLowerAction))
	{
		return false;
	}
	if(!xacmeSigCanonical(sCanonical, sizeof(sCanonical), "POST", "/",
			"", sHeaders, sSignedHeaders, sPayloadHash))
	{
		return false;
	}
	if(!xacmeSigSha256Hex(sCanonical, strlen(sCanonical), sHex))
	{
		return false;
	}
	if(!xacmeTencentFormat(sStringToSign, sizeof(sStringToSign),
		"TC3-HMAC-SHA256\n%s\n%s/" XACME_TENCENT_SERVICE
		"/tc3_request\n%s",
		sTimestamp, sDateText, sHex))
	{
		return false;
	}
	if(!xacmeTencentFormat(sKeySeed, sizeof(sKeySeed), "TC3%s", sKey))
	{
		xrtSecureZero(sKeySeed, sizeof(sKeySeed));
		return false;
	}
	bOk = xacmeSigHmac((const uint8*)sKeySeed, strlen(sKeySeed),
		sDateText, strlen(sDateText), kDate) &&
		xacmeSigHmac(kDate, sizeof(kDate), XACME_TENCENT_SERVICE,
			strlen(XACME_TENCENT_SERVICE), kService) &&
		xacmeSigHmac(kService, sizeof(kService), "tc3_request",
			sizeof("tc3_request") - 1u,
			kSigning) &&
		xacmeSigHmac(kSigning, sizeof(kSigning), sStringToSign,
			strlen(sStringToSign), Signature);
	xrtSecureZero(sKeySeed, sizeof(sKeySeed));
	xrtSecureZero(kDate, sizeof(kDate));
	xrtSecureZero(kService, sizeof(kService));
	xrtSecureZero(kSigning, sizeof(kSigning));
	if(!bOk)
	{
		xrtSecureZero(Signature, sizeof(Signature));
		return false;
	}
	xacmeSigHex(Signature, sizeof(Signature), sHex);
	xrtSecureZero(Signature, sizeof(Signature));
	return xacmeTencentFormat(sAuth, iAuthCapacity,
		"TC3-HMAC-SHA256 Credential=%s/%s/" XACME_TENCENT_SERVICE
		"/tc3_request, SignedHeaders=%s, Signature=%s",
		sId, sDateText, sSignedHeaders, sHex);
}

/* 执行一次 TC3 调用（POST + JSON body）。 */
static bool xacmeTencentCall(
	xacmednstencentcontext* pCtx, cstr sAction, cstr sBody,
	uint16* pOutStatus, str* pOutBody, size_t* pOutSize)
{
	char sAuth[640];
	char sTimestamp[24];
	char sUrl[240];
	xacmehttpheader Extra[4];
	xacmehttpresponse R;

	pCtx->Http.bWriteUncertain = false;
	if(!xacmeDnsTencentAuthorization(
		pCtx->sId, pCtx->sKey, pCtx->sEndpoint, sAction, sBody,
		xrtNow(), sAuth, sizeof(sAuth), sTimestamp, sizeof(sTimestamp)) ||
		!xacmeTencentFormat(sUrl, sizeof(sUrl), "https://%s/",
			pCtx->sEndpoint))
	{
		return false;
	}

	Extra[0] = (xacmehttpheader){ "Authorization", sAuth };
	Extra[1] = (xacmehttpheader){ "X-TC-Action", sAction };
	Extra[2] = (xacmehttpheader){ "X-TC-Version", XACME_TENCENT_VERSION };
	Extra[3] = (xacmehttpheader){ "X-TC-Timestamp", sTimestamp };
	if(!xacmeHttpExchangeV(
			&pCtx->Http, "POST", sUrl, "application/json; charset=utf-8",
			(xstrview){ sBody, strlen(sBody) }, Extra, 4u, &R))
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

/* 取响应 JSON 的 Response 成员（对象）。 */
static xvalue* xacmeTencentResponse(xstrview Body)
{
	/* The JSON parser also accepts a C-string terminator. HTTP bodies are
	 * length-delimited: an embedded NUL must not hide trailing bytes. */
	if(Body.Data == NULL || memchr(Body.Data, 0, Body.Size) != NULL) return NULL;
	xvalue* pRoot = (Body.Data != NULL) ? xrtJsonParse(Body) : NULL;
	xvalue* pResponse = (pRoot != NULL) ?
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response")) : NULL;
	if(!xrtValueIs(pRoot, XVALUE_OBJECT) || xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Error")) != NULL ||
		(pResponse == NULL) || !xrtValueIs(pResponse, XVALUE_OBJECT))
	{
		xrtValueRelease(pRoot);
		return NULL;
	}
	return pRoot; /* 调用方经 Root 再取 Response 并释放 Root。 */
}

bool xacmeDnsTencentResponseSuccess(xstrview sBody)
{
	xvalue* pRoot;
	xvalue* pResponse;
	bool bOk;
	if((sBody.Data == NULL) || (sBody.Size == 0u))
		return false;
	pRoot = xacmeTencentResponse(sBody);
	pResponse = (pRoot != NULL) ?
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response")) : NULL;
	bOk = (pResponse != NULL) && xrtValueIs(pResponse, XVALUE_OBJECT) &&
		(xrtValueObjectGet(pResponse, XRT_STR_LITERAL("Error")) == NULL);
	if(bOk) {
		char sRequest[128];
		bOk = xacmeDnsJsonText(pResponse, "RequestId", sRequest, sizeof(sRequest)) && sRequest[0] != '\0';
	}
	xrtValueRelease(pRoot);
	return bOk;
}

static bool xacmeTencentErrorFamily(cstr sCode, cstr sFamily)
{
	size_t iSize = strlen(sFamily);
	return strncmp(sCode, sFamily, iSize) == 0 &&
		(sCode[iSize] == '\0' || sCode[iSize] == '.');
}

static bool xacmeTencentCreateRejected(const xvalue* pResponse, uint16 iStatus)
{
	xvalue* pError;
	char sCode[128], sMessage[512], sRequest[128];
	if(pResponse == NULL || !(iStatus == 200u || (iStatus >= 400u && iStatus < 500u)) ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) != NULL ||
		!xacmeDnsJsonText(pResponse, "RequestId", sRequest, sizeof(sRequest)) || sRequest[0] == '\0') return false;
	pError = xrtValueObjectGet(pResponse, XRT_STR_LITERAL("Error"));
	if(!xrtValueIs(pError, XVALUE_OBJECT) ||
		!xacmeDnsJsonText(pError, "Code", sCode, sizeof(sCode)) ||
		!xacmeDnsJsonText(pError, "Message", sMessage, sizeof(sMessage)) || sMessage[0] == '\0') return false;
	/* Internal/unknown failures do not establish that the mutation was rejected. */
	return xacmeTencentErrorFamily(sCode, "AuthFailure") ||
		xacmeTencentErrorFamily(sCode, "InvalidParameter") ||
		xacmeTencentErrorFamily(sCode, "ResourceNotFound") ||
		strcmp(sCode, "UnauthorizedOperation") == 0 ||
		xacmeTencentErrorFamily(sCode, "OperationDenied") ||
		xacmeTencentErrorFamily(sCode, "LimitExceeded") ||
		strcmp(sCode, "RequestLimitExceeded") == 0;
}

static void xacmeTencentReadError(uint16 iStatus, const xvalue* pResponse)
{
	char sCode[128] = { 0 };
	xerrkind Kind = XERR_PROTOCOL;
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return;
	if(pResponse != NULL)
		(void)xacmeDnsJsonText(xrtValueObjectGet(pResponse, XRT_STR_LITERAL("Error")), "Code", sCode, sizeof(sCode));
	if(iStatus == 401u || iStatus == 403u || xacmeTencentErrorFamily(sCode, "AuthFailure") ||
		xacmeTencentErrorFamily(sCode, "OperationDenied") || strcmp(sCode, "UnauthorizedOperation") == 0) Kind = XERR_PERMISSION;
	else if(iStatus >= 500u || xacmeTencentErrorFamily(sCode, "InternalError") ||
		xacmeTencentErrorFamily(sCode, "RequestLimitExceeded")) Kind = XERR_AGAIN;
	xrtSetErrorInfo(Kind, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
		"acme dns_tencent request failed or response does not match the tracked identity");
}

/* The API uses POST even for reads. Retry only these explicitly selected RPCs;
 * every attempt signs again. Mutation RPCs never enter this helper. */
static bool xacmeTencentReadCall(xacmednstencentcontext* pCtx, cstr sAction,
	cstr sBody, uint16* pStatus, str* pBody, size_t* pSize)
{
	uint32 i;
	for(i = 0u; i < 3u; i++) {
		xerrkind Kind;
		xrtClearError();
		if(xacmeTencentCall(pCtx, sAction, sBody, pStatus, pBody, pSize)) return true;
		xrtFree(*pBody); *pBody = NULL;
		Kind = xrtErrorKind(xrtGetError());
		if(!pCtx->Http.bWriteUncertain ||
			(Kind != XERR_IO && Kind != XERR_TIMEOUT && Kind != XERR_CLOSED)) return false;
		if(i != 2u) xrtSleep(250u << i);
	}
	return false;
}

static bool xacmeTencentResponseOk(const xvalue* pResponse, uint16 iStatus)
{
	char sRequest[128];
	return iStatus == 200u && xrtValueIs(pResponse, XVALUE_OBJECT) &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("Error")) == NULL &&
		xacmeDnsJsonText(pResponse, "RequestId", sRequest, sizeof(sRequest)) && sRequest[0] != '\0';
}

static bool xacmeTencentErrorCode(const xvalue* pResponse, uint16 iStatus, cstr sExpected)
{
	const xvalue* pError;
	char sRequest[128], sMessage[512];
	if(!(iStatus == 200u || (iStatus >= 400u && iStatus < 500u)) || !xrtValueIs(pResponse, XVALUE_OBJECT) ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) != NULL ||
		!xacmeDnsJsonText(pResponse, "RequestId", sRequest, sizeof(sRequest)) || sRequest[0] == '\0') return false;
	pError = xrtValueObjectGet(pResponse, XRT_STR_LITERAL("Error"));
	return xrtValueIs(pError, XVALUE_OBJECT) && xacmeDnsJsonEqual(pError, "Code", sExpected) &&
		xacmeDnsJsonText(pError, "Message", sMessage, sizeof(sMessage)) && sMessage[0] != '\0';
}

static bool xacmeTencentDomainBody(xbuffer* pBody, cstr sZone, int64 iDomainId)
{
	char sNumber[64];
	if(!xrtBufferAppend(pBody, XRT_BYTES_LITERAL("{\"Domain\":")) ||
		!xacmeDnsJsonQuote(pBody, (xstrview){ sZone, strlen(sZone) })) return false;
	if(iDomainId != 0) {
		snprintf(sNumber, sizeof(sNumber), ",\"DomainId\":%lld", (long long)iDomainId);
		if(!xrtBufferAppend(pBody, (xbytesview){ (const uint8*)sNumber, strlen(sNumber) })) return false;
	}
	return true;
}

/* Returns 1 for a matching domain, 0 for a narrowly classified missing
 * candidate, and -1 for errors. Existing owned IDs must never fall back. */
static int xacmeTencentReadDomain(xacmednstencentcontext* pCtx, cstr sZone, int64* pDomainId)
{
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iSize = 0u;
	xvalue* pRoot = NULL;
	xvalue* pResponse = NULL;
	int64 iId = 0;
	int Result = -1;
	xrtBufferInit(&Body);
	if(!xacmeTencentDomainBody(&Body, sZone, *pDomainId) ||
		!xrtBufferAppendByte(&Body, '}') || !xrtBufferAppendByte(&Body, 0u) ||
		!xacmeTencentReadCall(pCtx, "DescribeDomain", (cstr)Body.Data, &iStatus, &sResp, &iSize)) goto Done;
	pRoot = xacmeTencentResponse((xstrview){ sResp, iSize });
	pResponse = (pRoot != NULL) ? xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response")) : NULL;
	if(xacmeTencentResponseOk(pResponse, iStatus)) {
		xvalue* pDomain = xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo"));
		if(xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) == NULL &&
			xacmeDnsJsonEqual(pDomain, "Domain", sZone) &&
			xrtValueGetInt(xrtValueObjectGet(pDomain, XRT_STR_LITERAL("DomainId")), &iId) &&
			iId > 0 && (*pDomainId == 0 || iId == *pDomainId)) { *pDomainId = iId; Result = 1; }
	} else if(*pDomainId == 0 &&
		(xacmeTencentErrorCode(pResponse, iStatus, "InvalidParameterValue.DomainNotExists") ||
		 xacmeTencentErrorCode(pResponse, iStatus, "InvalidParameter.DomainInvalid"))) Result = 0;
	if(Result < 0) xacmeTencentReadError(iStatus, pResponse);
	else xrtClearError();
Done:
	xrtValueRelease(pRoot); xrtFree(sResp); xrtBufferUnit(&Body);
	return Result;
}

static bool xacmeTencentAddLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednstencentcontext* pCtx =
		(xacmednstencentcontext*)pProvider->pContext;
	char sFqdnText[256];
	char sTxtText[208];
	char sRr[256];
	char sZone[256];
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	xvalue* pRoot;
	xvalue* pResponse;
	bool bOk = false;
	bool bTracked = false;
	bool bSent = false;
	size_t iSlot;
	int64 iDomainId = 0;

	if(!xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_tencent owner or digest is invalid");
		return false;
	}
	sFqdn = xacmeDnsCanonicalOwner(sFqdn, sFqdnText);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, sFqdn, sTxt))
		return xacmeDnsCreateUncertainError();
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bDeletePending, sFqdn, sTxt))
		return xacmeTencentDeleteUncertain();
	iSlot = xacmeDnsCreateSlot(&pCtx->Records, pCtx->bUncertain);
	if(iSlot >= XACME_DNS_RECORD_MAX)
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_tencent record tracking capacity exhausted");
		return false;
	}
	memcpy(sTxtText, sTxt.Data, sTxt.Size);
	sTxtText[sTxt.Size] = '\0';
	/* Start from the complete TXT owner on every new Add. A parent cache
	 * cannot establish that a more specific hosted domain still does not exist. */
	memcpy(sZone, sFqdnText, sFqdn.Size + 1u);
	for(;;) {
		int Found = xacmeTencentReadDomain(pCtx, sZone, &iDomainId);
		char* sDot;
		if(Found == 1) break;
		if(Found < 0) return false;
		sDot = strchr(sZone, '.');
		if(sDot == NULL || strchr(sDot + 1u, '.') == NULL) {
			xrtSetErrorInfo(XERR_NOT_FOUND, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
				"acme dns_tencent no managed domain found"); return false;
		}
		memmove(sZone, sDot + 1u, strlen(sDot + 1u) + 1u);
	}

	/* DNSPod uses @ when the TXT owner is itself the hosted domain. */
	{
		size_t iZoneLen = strlen(sZone);
		size_t iFqdnLen = strlen(sFqdnText);
		size_t iRrLen;
		if(iFqdnLen == iZoneLen && strcmp(sFqdnText, sZone) == 0)
		{
			memcpy(sRr, "@", 2u);
		}
		else if((iFqdnLen <= iZoneLen + 1u) ||
			(strcmp(sFqdnText + iFqdnLen - iZoneLen, sZone) != 0) ||
			(sFqdnText[iFqdnLen - iZoneLen - 1u] != '.'))
		{
			xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
				"acme dns_tencent hosted domain does not contain the TXT owner");
			return false;
		}
		else
		{
			iRrLen = iFqdnLen - iZoneLen - 1u;
			memcpy(sRr, sFqdnText, iRrLen);
			sRr[iRrLen] = '\0';
		}
	}
	xrtBufferInit(&Body);
	bOk = xacmeTencentDomainBody(&Body, sZone, iDomainId) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL(",\"SubDomain\":")) &&
		xacmeDnsJsonQuote(&Body, (xstrview){ sRr, strlen(sRr) }) &&
		xrtBufferAppend(&Body,
			XRT_BYTES_LITERAL(",\"RecordType\":\"TXT\","
				"\"RecordLine\":\"默认\",\"Value\":")) &&
		xacmeDnsJsonQuote(&Body, (xstrview){ sTxtText, strlen(sTxtText) }) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL("}")) &&
		xrtBufferAppendByte(&Body, 0u);
	if(bOk)
	{
		xacmeDnsCreateReserve(&pCtx->Records, pCtx->bUncertain, iSlot, sFqdn, sTxt);
		pCtx->iDomainIds[iSlot] = iDomainId;
		bSent = true;
		bOk = xacmeTencentCall(pCtx, "CreateRecord", (cstr)Body.Data,
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
	/* RecordId 记档（可能为数值，按文本取）。 */
	pRoot = xacmeTencentResponse((xstrview){ sResp, iRespSize });
	if(pRoot != NULL)
	{
		xvalue* pMember;
		char sRequest[128];
		pResponse = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response"));
		if(xacmeTencentCreateRejected(pResponse, iStatus)) {
			xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
			xacmeTencentReadError(iStatus, pResponse);
			xrtValueRelease(pRoot); xrtFree(sResp);
			return false;
		}
		pMember = (pResponse != NULL) ?
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) : NULL;
		if(iStatus == 200u && pMember != NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("Error")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) == NULL &&
			xacmeDnsJsonText(pResponse, "RequestId", sRequest, sizeof(sRequest)) && sRequest[0] != '\0')
		{
			int64 iId = 0;
			if(xrtValueGetInt(pMember, &iId))
			{
				char sIdText[32];
				if(iId > 0)
				{
					snprintf(sIdText, sizeof(sIdText), "%lld",
						(long long)iId);
					bTracked = xacmeDnsCreateCommit(&pCtx->Records, pCtx->bUncertain,
						iSlot, sZone, '|', sIdText);
					if(bTracked) pCtx->uCreatedAt[iSlot] = xrtClock();
				}
			}
		}
		xrtValueRelease(pRoot);
	}
	xrtFree(sResp);
	return bTracked ? true : xacmeDnsCreateUncertainError();
}

typedef enum xacmetencentrecordresult {
	XACME_TENCENT_RECORD_ERROR,
	XACME_TENCENT_RECORD_FOUND
} xacmetencentrecordresult;

static bool xacmeTencentRecordBody(xbuffer* pBody, cstr sZone, int64 iDomainId, int64 iRecordId)
{
	char sNumber[64];
	snprintf(sNumber, sizeof(sNumber), ",\"RecordId\":%lld}", (long long)iRecordId);
	return xacmeTencentDomainBody(pBody, sZone, iDomainId) &&
		xrtBufferAppend(pBody, (xbytesview){ (const uint8*)sNumber, strlen(sNumber) }) &&
		xrtBufferAppendByte(pBody, 0u);
}

static bool xacmeTencentOwnedHandle(xacmednstencentcontext* pCtx, size_t iSlot,
	char* sZone, size_t iZoneCapacity, int64* pRecordId)
{
	char sRecordId[32];
	cstr p;
	int64 iRecordId = 0;
	if(iSlot >= pCtx->Records.iCount || pCtx->iDomainIds[iSlot] <= 0 ||
		!xacmeDnsRecordSplit(pCtx->Records.sIds[iSlot], '|', sZone, iZoneCapacity,
			sRecordId, sizeof(sRecordId)) || sRecordId[0] == '0') goto Invalid;
	for(p = sRecordId; *p != '\0'; p++) {
		if(*p < '0' || *p > '9' || iRecordId > (INT64_MAX - (*p - '0')) / 10) goto Invalid;
		iRecordId = iRecordId * 10 + (*p - '0');
	}
	*pRecordId = iRecordId;
	return true;
Invalid:
	xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
		"acme dns_tencent tracked domain/record handle is invalid");
	return false;
}

static bool xacmeTencentRecordIdentity(const xvalue* pRecord, int64 iDomainId,
	int64 iRecordId, cstr sZone, xstrview Owner, xstrview Txt)
{
	int64 iId = 0, iDomain = 0, iEnabled = -1;
	xstrview Sub, Value;
	size_t iZone = strlen(sZone);
	return xrtValueIs(pRecord, XVALUE_OBJECT) &&
		xrtValueGetInt(xrtValueObjectGet(pRecord, XRT_STR_LITERAL("Id")), &iId) && iId == iRecordId &&
		xrtValueGetInt(xrtValueObjectGet(pRecord, XRT_STR_LITERAL("DomainId")), &iDomain) && iDomain == iDomainId &&
		xacmeDnsJsonEqual(pRecord, "RecordType", "TXT") &&
		xacmeDnsJsonEqual(pRecord, "RecordLine", "默认") && xacmeDnsJsonEqual(pRecord, "RecordLineId", "0") &&
		xrtValueGetInt(xrtValueObjectGet(pRecord, XRT_STR_LITERAL("Enabled")), &iEnabled) && (iEnabled == 0 || iEnabled == 1) &&
		xrtValueGetString(xrtValueObjectGet(pRecord, XRT_STR_LITERAL("SubDomain")), &Sub) &&
		((Owner.Size == iZone && memcmp(Owner.Data, sZone, iZone) == 0 &&
			Sub.Size == 1u && Sub.Data[0] == '@') ||
		(Owner.Size > iZone + 1u && Sub.Size == Owner.Size - iZone - 1u &&
		memcmp(Sub.Data, Owner.Data, Sub.Size) == 0 && Owner.Data[Sub.Size] == '.' &&
		memcmp(Owner.Data + Sub.Size + 1u, sZone, iZone) == 0)) &&
		xrtValueGetString(xrtValueObjectGet(pRecord, XRT_STR_LITERAL("Value")), &Value) &&
		Value.Size == Txt.Size && memcmp(Value.Data, Txt.Data, Txt.Size) == 0;
}

static int xacmeTencentCompareIds(const void* pLeft, const void* pRight)
{
	int64 Left = *(const int64*)pLeft, Right = *(const int64*)pRight;
	return (Left > Right) - (Left < Right);
}

/* The list index has no documented maximum delay. Inspect its inventory for
 * diagnostics after the recommended retry interval, but never release a saved
 * identity based on absence from that index, even after repeated observations. */
static void xacmeTencentInspectMissingCandidate(xacmednstencentcontext* pCtx,
	size_t iSlot, cstr sZone, int64 iRecordId)
{
	uint64 uNow = xrtClock();
	int64 iDomainId = pCtx->iDomainIds[iSlot], iTotal = -1, iListed = -1;
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iSize = 0u, iCount, i;
	int64 Ids[3000];
	xbuffer Body;
	xvalue* pRoot = NULL;
	xvalue* pResponse = NULL;
	xvalue* pList;
	bool bValid = false, bPresent = false;
	if(uNow < pCtx->uCreatedAt[iSlot] || uNow - pCtx->uCreatedAt[iSlot] < 30000000u) {
		xrtSetErrorInfo(XERR_AGAIN, "xrt.acme.dns", XACME_DNS_ERROR_NETWORK,
			"acme dns_tencent record absence cannot be checked during the create index delay"); return;
	}
	if(xacmeTencentReadDomain(pCtx, sZone, &iDomainId) != 1) return;
	xrtBufferInit(&Body);
	if(!xacmeTencentDomainBody(&Body, sZone, iDomainId) ||
		!xrtBufferAppend(&Body, XRT_BYTES_LITERAL(",\"Offset\":0,\"Limit\":3000,\"ErrorOnEmpty\":\"no\"}")) ||
		!xrtBufferAppendByte(&Body, 0u) ||
		!xacmeTencentReadCall(pCtx, "DescribeRecordList", (cstr)Body.Data, &iStatus, &sResp, &iSize)) goto Done;
	pRoot = xacmeTencentResponse((xstrview){ sResp, iSize });
	pResponse = (pRoot != NULL) ? xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response")) : NULL;
	pList = (pResponse != NULL) ? xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) : NULL;
	if(xacmeTencentResponseOk(pResponse, iStatus) && xrtValueIs(pList, XVALUE_ARRAY) &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) == NULL &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) == NULL &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) == NULL) {
		xvalue* pCount = xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo"));
		iCount = xrtValueCount(pList);
		bValid = xrtValueGetInt(xrtValueObjectGet(pCount, XRT_STR_LITERAL("TotalCount")), &iTotal) &&
			xrtValueGetInt(xrtValueObjectGet(pCount, XRT_STR_LITERAL("ListCount")), &iListed) &&
			iTotal >= 0 && iTotal <= 3000 && iTotal == iListed && (uint64)iTotal == iCount;
		for(i = 0u; bValid && i < iCount; i++) {
			int64 iId = 0;
			xvalue* pItem = xrtValueArrayGet(pList, i);
			xstrview Name, Type, Value;
			bValid = xrtValueIs(pItem, XVALUE_OBJECT) &&
				xrtValueGetInt(xrtValueObjectGet(pItem, XRT_STR_LITERAL("RecordId")), &iId) && iId > 0 &&
				xrtValueGetString(xrtValueObjectGet(pItem, XRT_STR_LITERAL("Name")), &Name) && Name.Size != 0u &&
				xrtValueGetString(xrtValueObjectGet(pItem, XRT_STR_LITERAL("Type")), &Type) && Type.Size != 0u &&
				xrtValueGetString(xrtValueObjectGet(pItem, XRT_STR_LITERAL("Value")), &Value) &&
				memchr(Name.Data, 0, Name.Size) == NULL && memchr(Type.Data, 0, Type.Size) == NULL &&
				memchr(Value.Data, 0, Value.Size) == NULL &&
				(xacmeDnsJsonEqual(pItem, "Status", "ENABLE") || xacmeDnsJsonEqual(pItem, "Status", "DISABLE"));
			if(iId == iRecordId) bPresent = true;
			Ids[i] = iId;
		}
		if(bValid) {
			qsort(Ids, iCount, sizeof(Ids[0]), xacmeTencentCompareIds);
			for(i = 1u; i < iCount; i++) if(Ids[i] == Ids[i - 1u]) { bValid = false; break; }
		}
	}
	if(bValid && !bPresent) xrtSetErrorInfo(XERR_AGAIN, "xrt.acme.dns", XACME_DNS_ERROR_NETWORK,
		"acme dns_tencent record absent from a possibly delayed index; original handle retained");
	else if(bValid) xrtSetErrorInfo(XERR_AGAIN, "xrt.acme.dns", XACME_DNS_ERROR_NETWORK,
		"acme dns_tencent record still appears in inventory; original handle retained");
	else xacmeTencentReadError(iStatus, pResponse);
Done:
	xrtBufferUnit(&Body); xrtValueRelease(pRoot); xrtFree(sResp);
}

static xacmetencentrecordresult xacmeTencentReadRecord(xacmednstencentcontext* pCtx,
	size_t iSlot, cstr sZone, int64 iRecordId, xstrview Owner, xstrview Txt, bool bAdding)
{
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iSize = 0u;
	xvalue* pRoot = NULL;
	xvalue* pResponse = NULL;
	bool bMissingCandidate = false;
	xacmetencentrecordresult Result = XACME_TENCENT_RECORD_ERROR;
	xrtBufferInit(&Body);
	if(!xacmeTencentRecordBody(&Body, sZone, pCtx->iDomainIds[iSlot], iRecordId) ||
		!xacmeTencentReadCall(pCtx, "DescribeRecord", (cstr)Body.Data, &iStatus, &sResp, &iSize)) goto Done;
	pRoot = xacmeTencentResponse((xstrview){ sResp, iSize });
	pResponse = (pRoot != NULL) ? xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response")) : NULL;
	if(xacmeTencentResponseOk(pResponse, iStatus) &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) == NULL &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) == NULL &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) == NULL &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) == NULL &&
		xacmeTencentRecordIdentity(xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")),
			pCtx->iDomainIds[iSlot], iRecordId, sZone, Owner, Txt)) {
		int64 iEnabled = 0;
		if(bAdding && (!xrtValueGetInt(xrtValueObjectGet(
				xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")),
				XRT_STR_LITERAL("Enabled")), &iEnabled) || iEnabled != 1))
			xrtSetErrorInfo(XERR_STATE, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
				"acme dns_tencent owned record is disabled");
		else { Result = XACME_TENCENT_RECORD_FOUND; xrtClearError(); }
	} else if(xacmeTencentErrorCode(pResponse, iStatus, "InvalidParameter.RecordIdInvalid")) bMissingCandidate = true;
	else xacmeTencentReadError(iStatus, pResponse);
Done:
	xrtBufferUnit(&Body); xrtValueRelease(pRoot); xrtFree(sResp);
	if(bMissingCandidate) {
		/* A failed Add revalidation sent no deletion. Allow another read of the
		 * same saved ID, but never release it based on the list index. */
		if(!bAdding) pCtx->bDeletePending[iSlot] = true;
		xacmeTencentInspectMissingCandidate(pCtx, iSlot, sZone, iRecordId);
	}
	return Result;
}

static bool xacmeTencentDeleteUncertain(void)
{
	xerror* pError;
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	pError = xrtErrorWrap(xrtGetError(), XERR_IO, "xrt.acme.dns", XACME_DNS_ERROR_UNCERTAIN,
		"Tencent DNS deletion outcome unknown; original record retained");
	if(pError != NULL) xrtSetErrorTake(pError);
	return false;
}

static xacmednsownedresult xacmeTencentReuseOwned(xacmednstencentcontext* pCtx,
	xstrview Owner, xstrview Txt)
{
	char sOwner[256], sZone[256];
	size_t iSlot;
	int64 iRecordId = 0, iDomainId;
	if(!xacmeDnsChallengeValid(Owner, Txt)) return XACME_DNS_OWNED_NONE;
	Owner = xacmeDnsCanonicalOwner(Owner, sOwner);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, Owner, Txt)) {
		(void)xacmeDnsCreateUncertainError(); return XACME_DNS_OWNED_ERROR;
	}
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bDeletePending, Owner, Txt)) {
		(void)xacmeTencentDeleteUncertain(); return XACME_DNS_OWNED_ERROR;
	}
	iSlot = xacmeDnsRecordFindOwned(&pCtx->Records, Owner, Txt);
	if(iSlot == XACME_DNS_RECORD_MAX) return XACME_DNS_OWNED_NONE;
	if(!xacmeTencentOwnedHandle(pCtx, iSlot, sZone, sizeof(sZone), &iRecordId)) return XACME_DNS_OWNED_ERROR;
	iDomainId = pCtx->iDomainIds[iSlot];
	/* Validate the saved domain as well as the record; do not rediscover a child. */
	if(xacmeTencentReadDomain(pCtx, sZone, &iDomainId) != 1) return XACME_DNS_OWNED_ERROR;
	return xacmeTencentReadRecord(pCtx, iSlot, sZone, iRecordId, Owner, Txt, true) ==
		XACME_TENCENT_RECORD_FOUND ? XACME_DNS_OWNED_VALID : XACME_DNS_OWNED_ERROR;
}

static bool xacmeTencentDeleteRecord(void* pContext, cstr sId,
	xstrview sFqdn, xstrview sTxt)
{
	xacmednstencentcontext* pCtx = (xacmednstencentcontext*)pContext;
	char sZone[256];
	size_t iSlot;
	int64 iRecordId = 0;
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	bool bOk;
	bool bPreviousPending;
	xvalue* pRoot = NULL;
	xvalue* pResponse;
	xacmetencentrecordresult Result;
	for(iSlot = 0u; iSlot < pCtx->Records.iCount; iSlot++)
		if(strcmp(pCtx->Records.sIds[iSlot], sId) == 0) break;
	if(!xacmeTencentOwnedHandle(pCtx, iSlot, sZone, sizeof(sZone), &iRecordId)) return false;
	Result = xacmeTencentReadRecord(pCtx, iSlot, sZone, iRecordId, sFqdn, sTxt, false);
	if(Result != XACME_TENCENT_RECORD_FOUND) return false;
	xrtBufferInit(&Body);
	if(!xacmeTencentRecordBody(&Body, sZone, pCtx->iDomainIds[iSlot], iRecordId)) { xrtBufferUnit(&Body); return false; }
	bPreviousPending = pCtx->bDeletePending[iSlot]; pCtx->bDeletePending[iSlot] = true;
	bOk = xacmeTencentCall(pCtx, "DeleteRecord", (cstr)Body.Data, &iStatus, &sResp, &iRespSize);
	xrtBufferUnit(&Body);
	if(!bOk && !pCtx->Http.bWriteUncertain) { pCtx->bDeletePending[iSlot] = bPreviousPending; xrtFree(sResp); return false; }
	if(bOk) {
		int64 iAckId = 0;
		xvalue* pId;
		pRoot = xacmeTencentResponse((xstrview){ sResp, iRespSize });
		pResponse = (pRoot != NULL) ? xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response")) : NULL;
		pId = (pResponse != NULL) ? xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) : NULL;
		bOk = xacmeTencentResponseOk(pResponse, iStatus) &&
			(pId == NULL || (xrtValueGetInt(pId, &iAckId) && iAckId == iRecordId)) &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) == NULL;
		/* A verified error must contain no result fields. All other failures
		 * could have happened after the server committed the deletion. */
		if(!bOk && xacmeTencentCreateRejected(pResponse, iStatus) &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) == NULL &&
			!xacmeTencentErrorCode(pResponse, iStatus, "InvalidParameter.RecordIdInvalid") &&
			xrtErrorKind(xrtGetError()) != XERR_MEMORY) {
			xacmeTencentReadError(iStatus, pResponse);
			pCtx->bDeletePending[iSlot] = bPreviousPending;
			xrtValueRelease(pRoot); xrtFree(sResp); return false;
		}
	}
	xrtValueRelease(pRoot); xrtFree(sResp);
	if(bOk) { pCtx->bDeletePending[iSlot] = false; return true; }
	(void)xacmeTencentDeleteUncertain();
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	{
		xerror* pWriteError = xrtTakeError();
		Result = xacmeTencentReadRecord(pCtx, iSlot, sZone, iRecordId, sFqdn, sTxt, false);
		if(Result == XACME_TENCENT_RECORD_FOUND) xrtSetErrorTake(pWriteError);
		else xrtErrorFree(pWriteError);
	}
	return false;
}

static bool xacmeTencentRemoveLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednstencentcontext* pCtx =
		(xacmednstencentcontext*)pProvider->pContext;
	char sOwner[256];
	if(!xacmeDnsChallengeValid(sFqdn, sTxt)) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_ARGUMENT,
			"acme dns_tencent owner or digest is invalid"); return false;
	}
	sFqdn = xacmeDnsCanonicalOwner(sFqdn, sOwner);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, sFqdn, sTxt))
		return xacmeDnsCreateUncertainError();
	return xacmeDnsRecordRemoveMatching(&pCtx->Records, sFqdn, sTxt,
		xacmeTencentDeleteRecord, pCtx);
}

static bool xacmeTencentAdd(xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednstencentcontext* pCtx = (xacmednstencentcontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	xacmednsownedresult Existing;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	Existing = xacmeTencentReuseOwned(pCtx, sFqdn, sTxt);
	bOk = Existing == XACME_DNS_OWNED_VALID ||
		(Existing == XACME_DNS_OWNED_NONE && xacmeTencentAddLocked(pProvider, sFqdn, sTxt));
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

static bool xacmeTencentRemove(xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednstencentcontext* pCtx = (xacmednstencentcontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeTencentRemoveLocked(pProvider, sFqdn, sTxt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
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
	if(strlen(pConfig->sSecretId) >= sizeof(pCtx->sId) ||
		strlen(pConfig->sSecretKey) >= sizeof(pCtx->sKey) ||
		((pConfig->sEndpoint != NULL) &&
		 strlen(pConfig->sEndpoint) >= sizeof(pCtx->sEndpoint)))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_tencent credentials or endpoint exceed capacity");
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
		if(!xacmeHttpUnit(&pCtx->Http)) return;
		(void)xrtMutexUnit(&pCtx->Lock);
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
