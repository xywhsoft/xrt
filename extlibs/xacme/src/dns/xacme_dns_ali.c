#include <xrt/acme_dns_ali.h>

#if defined(XACME_FEATURE_DNS_ALI)

#include "../internal/xacme_http.h"
#include "../internal/xacme_dns_ali_internal.h"
#include "../internal/xacme_dnscommon.h"

#include <xrt/codec.h>
#include <xrt/crypto.h>
#include <xrt/json.h>
#include <xrt/memory.h>
#include <xrt/random.h>
#include <xrt/sync.h>
#include <xrt/time.h>
#include <xrt/value.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define XACME_ALI_VERSION "2015-01-09"
#define XACME_ALI_ZONE_MAX 4u
#define XACME_ALI_EMPTY_SHA256 \
	"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

typedef struct xacmednsalicontext {
	xacmehttp Http;
	xmutex Lock;
	char sKeyId[160];
	char sSecret[160];
	char sEndpoint[160];
	/* Exact discovery starts map to verified zones; a parent cannot hide a child. */
	char sZoneStarts[XACME_ALI_ZONE_MAX][256];
	char sZones[XACME_ALI_ZONE_MAX][256];
	size_t iZoneCount;
	size_t iZoneNext;
	/* 本 provider 生命周期内添加的 RecordId 与 DNS-01 值。 */
	xacmednsrecords Records;
	/* An empty id in an uncertain slot is reserved, never free or owned. */
	bool bUncertain[XACME_DNS_RECORD_MAX];
} xacmednsalicontext;

static bool xacmeAliFormat(char* sOut, size_t iCapacity, cstr sFormat, ...)
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
			XACME_DNS_ERROR_ARGUMENT, "alidns request exceeds buffer capacity");
		return false;
	}
	return true;
}

/* DNS-01 的属主与摘要只进入已签名的 query，拒绝分隔符注入。 */
static bool xacmeAliQueryInputValid(xstrview Fqdn, xstrview Txt)
{
	return xacmeDnsChallengeValid(Fqdn, Txt);
}

static bool xacmeAliUncertainError(void)
{
	xerror* pError;
	/* Parsing or diagnostic allocation failure must keep its original error. */
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	pError = xrtErrorWrap(xrtGetError(), XERR_IO, "xrt.acme.dns",
		XACME_DNS_ERROR_UNCERTAIN, "alidns create outcome and ownership are unknown");
	if(pError != NULL) xrtSetErrorTake(pError);
	return false;
}

static bool xacmeAliPairMatches(xacmednsalicontext* pCtx, size_t i,
	xstrview Fqdn, xstrview Txt)
{
	return strlen(pCtx->Records.sFqdns[i]) == Fqdn.Size &&
		memcmp(pCtx->Records.sFqdns[i], Fqdn.Data, Fqdn.Size) == 0 &&
		strlen(pCtx->Records.sTxts[i]) == Txt.Size &&
		memcmp(pCtx->Records.sTxts[i], Txt.Data, Txt.Size) == 0;
}

static bool xacmeAliPairUncertain(xacmednsalicontext* pCtx, xstrview Fqdn, xstrview Txt)
{
	size_t i;
	for(i = 0u; i < pCtx->Records.iCount; i++)
		if(pCtx->bUncertain[i] && xacmeAliPairMatches(pCtx, i, Fqdn, Txt)) return true;
	return false;
}

static size_t xacmeAliOwnedSlot(xacmednsalicontext* pCtx, xstrview Fqdn, xstrview Txt)
{
	size_t i;
	for(i = 0u; i < pCtx->Records.iCount; i++)
		if(!pCtx->bUncertain[i] && pCtx->Records.sIds[i][0] != '\0' &&
			xacmeAliPairMatches(pCtx, i, Fqdn, Txt)) return i;
	return XACME_DNS_RECORD_MAX;
}

static size_t xacmeAliFreeSlot(xacmednsalicontext* pCtx)
{
	size_t i;
	for(i = 0u; i < pCtx->Records.iCount; i++)
		if(!pCtx->bUncertain[i] && pCtx->Records.sIds[i][0] == '\0') return i;
	return pCtx->Records.iCount;
}

/* Inputs and capacity are checked before the write; recording cannot allocate. */
static void xacmeAliStoreRecord(xacmednsalicontext* pCtx, size_t i, cstr sId,
	xstrview Fqdn, xstrview Txt)
{
	if(i == pCtx->Records.iCount) pCtx->Records.iCount++;
	pCtx->bUncertain[i] = sId == NULL;
	if(sId != NULL) strcpy(pCtx->Records.sIds[i], sId);
	else pCtx->Records.sIds[i][0] = '\0';
	memcpy(pCtx->Records.sFqdns[i], Fqdn.Data, Fqdn.Size);
	pCtx->Records.sFqdns[i][Fqdn.Size] = '\0';
	memcpy(pCtx->Records.sTxts[i], Txt.Data, Txt.Size);
	pCtx->Records.sTxts[i][Txt.Size] = '\0';
}

static void xacmeAliHex(const uint8* pData, size_t iSize, char* sOut)
{
	size_t i;
	for(i = 0; i < iSize; i++)
	{
		sprintf(sOut + i * 2u, "%02x", pData[i]);
	}
	sOut[iSize * 2u] = '\0';
}

static bool xacmeAliUnreserved(char c)
{
	return ((c >= 'A') && (c <= 'Z')) ||
		((c >= 'a') && (c <= 'z')) ||
		((c >= '0') && (c <= '9')) ||
		(c == '-') || (c == '_') || (c == '.') || (c == '~');
}

static int xacmeAliQueryKeyCompare(cstr sA, cstr sB)
{
	const char* pAEnd = strchr(sA, '=');
	const char* pBEnd = strchr(sB, '=');
	size_t iALen = (size_t)(pAEnd - sA);
	size_t iBLen = (size_t)(pBEnd - sB);
	size_t iMin = (iALen < iBLen) ? iALen : iBLen;
	int iOrder = memcmp(sA, sB, iMin);
	if(iOrder != 0)
	{
		return iOrder;
	}
	return (iALen < iBLen) ? -1 : ((iALen > iBLen) ? 1 : 0);
}

bool xacmeDnsAliCanonicalQuery(
	cstr sQuery, char* sOut, size_t iCapacity)
{
	char sParts[1024];
	char* pParts[16];
	size_t iSize;
	size_t iCount = 0u;
	size_t iUsed = 0u;
	size_t i;
	if((sQuery == NULL) || (sOut == NULL) || (iCapacity == 0u))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT, "alidns query arguments are invalid");
		return false;
	}
	sOut[0] = '\0';
	iSize = strlen(sQuery);
	if(iSize == 0u)
	{
		return true;
	}
	if(iSize >= sizeof(sParts))
	{
		goto Invalid;
	}
	for(i = 0u; i < iSize; ++i)
	{
		if(!xacmeAliUnreserved(sQuery[i]) &&
			(sQuery[i] != '=') && (sQuery[i] != '&'))
		{
			goto Invalid;
		}
	}
	memcpy(sParts, sQuery, iSize + 1u);
	pParts[iCount++] = sParts;
	for(i = 0u; i < iSize; ++i)
	{
		if(sParts[i] == '&')
		{
			if((iCount >= (sizeof(pParts) / sizeof(pParts[0]))) ||
				(i == 0u) || (sParts[i - 1u] == '&') ||
				(sParts[i + 1u] == '\0'))
			{
				goto Invalid;
			}
			sParts[i] = '\0';
			pParts[iCount++] = sParts + i + 1u;
		}
	}
	for(i = 0u; i < iCount; ++i)
	{
		char* pEquals = strchr(pParts[i], '=');
		if((pEquals == NULL) || (pEquals == pParts[i]) ||
			(strchr(pEquals + 1u, '=') != NULL))
		{
			goto Invalid;
		}
	}
	for(i = 0u; i < iCount; ++i)
	{
		size_t j;
		for(j = i + 1u; j < iCount; ++j)
		{
			int iOrder = xacmeAliQueryKeyCompare(pParts[i], pParts[j]);
			if(iOrder == 0)
			{
				goto Invalid;
			}
			if(iOrder > 0)
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
		if(iUsed + iPartSize + ((i > 0u) ? 1u : 0u) + 1u >
			iCapacity)
		{
			goto Invalid;
		}
		if(i > 0u)
		{
			sOut[iUsed++] = '&';
		}
		memcpy(sOut + iUsed, pParts[i], iPartSize);
		iUsed += iPartSize;
		sOut[iUsed] = '\0';
	}
	return true;
Invalid:
	sOut[0] = '\0';
	xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
		XACME_DNS_ERROR_ARGUMENT, "alidns query is invalid or too long");
	return false;
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

/* 固定时间与 nonce 的 ACS3 签名入口，测试可对照独立实现。 */
bool xacmeDnsAliAuthorization(
	cstr sKeyId, cstr sSecret, cstr sEndpoint, cstr sAction,
	cstr sQuery, cstr sNonce, xtime iNow,
	char* sAuth, size_t iAuthCapacity,
	char* sDate, size_t iDateCapacity,
	char* sCanonicalQuery, size_t iQueryCapacity)
{
	char sCanonical[2048];
	char sHeaders[512];
	char sStringToSign[160];
	char sHex[72];
	uint8 Digest[XRT_SHA256_SIZE];
	uint8 Mac[XRT_SHA256_SIZE];
	xdatetime Now;
	size_t i;
	static const char* sSignedHeaders =
		"content-type;host;x-acs-action;x-acs-content-sha256;"
		"x-acs-date;x-acs-signature-nonce;x-acs-version";

	if((sKeyId == NULL) || (sSecret == NULL) || (sEndpoint == NULL) ||
		(sAction == NULL) || (sQuery == NULL) || (sNonce == NULL) ||
		(sAuth == NULL) || (sDate == NULL) || (sCanonicalQuery == NULL) ||
		(iAuthCapacity == 0u) || (iDateCapacity == 0u) ||
		(iQueryCapacity == 0u))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT, "alidns signature arguments are invalid");
		return false;
	}
	sAuth[0] = '\0';
	sDate[0] = '\0';
	sCanonicalQuery[0] = '\0';
	if(strlen(sNonce) != 32u)
	{
		goto InvalidNonce;
	}
	for(i = 0u; i < 32u; ++i)
	{
		if(!(((sNonce[i] >= '0') && (sNonce[i] <= '9')) ||
			((sNonce[i] >= 'a') && (sNonce[i] <= 'f'))))
		{
			goto InvalidNonce;
		}
	}
	if(!xrtTimeSplitAt(iNow, 0, &Now) ||
		!xacmeAliFormat(sDate, iDateCapacity,
			"%04ld-%02d-%02dT%02d:%02d:%02dZ",
			(long)Now.Year, Now.Month, Now.Day, Now.Hour, Now.Minute,
			Now.Second) ||
		!xacmeDnsAliCanonicalQuery(sQuery, sCanonicalQuery,
			iQueryCapacity) ||
		!xacmeAliFormat(sHeaders, sizeof(sHeaders),
			"content-type:application/json\n"
			"host:%s\n"
			"x-acs-action:%s\n"
			"x-acs-content-sha256:%s\n"
			"x-acs-date:%s\n"
			"x-acs-signature-nonce:%s\n"
			"x-acs-version:%s\n",
			sEndpoint, sAction, XACME_ALI_EMPTY_SHA256, sDate,
			sNonce, XACME_ALI_VERSION) ||
		!xacmeAliFormat(sCanonical, sizeof(sCanonical),
			"POST\n/\n%s\n%s\n%s\n%s", sCanonicalQuery,
			sHeaders, sSignedHeaders, XACME_ALI_EMPTY_SHA256))
	{
		return false;
	}
	if(!xrtSha256(sCanonical, strlen(sCanonical), Digest))
	{
		return false;
	}
	xacmeAliHex(Digest, sizeof(Digest), sHex);
	if(!xacmeAliFormat(sStringToSign, sizeof(sStringToSign),
		"ACS3-HMAC-SHA256\n%s", sHex))
		return false;
	if(!xrtHmacSha256(
			sSecret, strlen(sSecret),
			sStringToSign, strlen(sStringToSign), Mac))
	{
		xrtSecureZero(Mac, sizeof(Mac));
		return false;
	}
	xacmeAliHex(Mac, sizeof(Mac), sHex);
	xrtSecureZero(Mac, sizeof(Mac));
	return xacmeAliFormat(sAuth, iAuthCapacity,
		"ACS3-HMAC-SHA256 Credential=%s,SignedHeaders=%s,Signature=%s",
		sKeyId, sSignedHeaders, sHex);
InvalidNonce:
	xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
		XACME_DNS_ERROR_ARGUMENT, "alidns signature nonce is invalid");
	return false;
}

/*
	执行一次 alidns V3 调用（POST + query、空 body）。
	成功返回响应体（xrtMalloc，含状态）；HTTP 非 2xx 时也返回 true
	并把状态/体交给调用方判断（如 zone 试探要看 4xx）。
*/
static bool xacmeAliCall(
	xacmednsalicontext* pCtx, cstr sAction, cstr sQuery,
	uint16* pOutStatus, str* pOutBody)
{
	uint8 uNonce[16];
	char sNonce[33];
	char sDate[24];
	char sCanonicalQuery[1024];
	char sAuth[640];
	char sUrl[1200];
	xacmehttpheader Extra[6];
	xacmehttpresponse R;
	uint32 uAttempt;
	bool bReadOnly = strcmp(sAction, "DescribeDomainRecords") == 0 ||
		strcmp(sAction, "DescribeDomainRecordInfo") == 0;
	/* This RPC has not sent anything yet; older unknown writes live in the ledger. */
	pCtx->Http.bWriteUncertain = false;
	for(uAttempt = 1u; uAttempt <= 3u; ++uAttempt)
	{
		xerrkind Kind;
		if(!xrtSecureRandom(uNonce, sizeof(uNonce)))
		{
			return false;
		}
		xacmeAliHex(uNonce, sizeof(uNonce), sNonce);
		xrtSecureZero(uNonce, sizeof(uNonce));
		if(!xacmeDnsAliAuthorization(pCtx->sKeyId, pCtx->sSecret,
				pCtx->sEndpoint, sAction, sQuery, sNonce, xrtNow(),
				sAuth, sizeof(sAuth), sDate, sizeof(sDate),
				sCanonicalQuery, sizeof(sCanonicalQuery)))
		{
			return false;
		}
		if(sCanonicalQuery[0] != 0u)
		{
			if(!xacmeAliFormat(sUrl, sizeof(sUrl), "https://%s/?%s",
				pCtx->sEndpoint, sCanonicalQuery))
			{
				return false;
			}
		}
		else if(!xacmeAliFormat(sUrl, sizeof(sUrl), "https://%s/",
			pCtx->sEndpoint))
		{
			return false;
		}

		Extra[0] = (xacmehttpheader){ "Authorization", sAuth };
		Extra[1] = (xacmehttpheader){ "x-acs-action", sAction };
		Extra[2] = (xacmehttpheader){ "x-acs-content-sha256",
			XACME_ALI_EMPTY_SHA256 };
		Extra[3] = (xacmehttpheader){ "x-acs-date", sDate };
		Extra[4] = (xacmehttpheader){ "x-acs-signature-nonce", sNonce };
		Extra[5] = (xacmehttpheader){ "x-acs-version",
			XACME_ALI_VERSION };

		if(xacmeHttpExchangeOnceV(
				&pCtx->Http, "POST", sUrl, "application/json",
				(xstrview){ "", 0u }, Extra, 6u, &R))
		{
			*pOutStatus = R.iStatus;
			*pOutBody = R.sBody;
			R.sBody = NULL;
			xacmeHttpResponseUnit(&R);
			return true;
		}
		xacmeHttpResponseUnit(&R);
		if(bReadOnly) {
			const xerror* pError = xrtGetError();
			cstr sDomain = xrtErrorDomain(pError);
			/* RPC reads use POST on the wire but cannot create or delete records. */
			if(xrtErrorCode(pError) == XACME_HTTP_ERROR_UNCERTAIN && sDomain != NULL &&
				strcmp(sDomain, "xrt.acme.http") == 0) {
				const xerror* pCause = xrtErrorCause(pError);
				if(pCause != NULL) xrtSetError(pCause);
				else xrtSetErrorKind(XERR_IO);
			}
			pCtx->Http.bWriteUncertain = false;
		}
		Kind = xrtErrorKind(xrtGetError());
		if((uAttempt == 3u) ||
			pCtx->Http.bWriteUncertain ||
			((Kind != XERR_IO) && (Kind != XERR_TIMEOUT)))
		{
			return false;
		}
		xrtSleep((uAttempt == 1u) ? 500u : 1000u);
	}
	return false;
}

static bool xacmeAliRecordIdCopy(
	xstrview Text, char* sRecordId, size_t iCapacity)
{
	size_t i;
	if((sRecordId == NULL) || (iCapacity == 0u))
	{
		return false;
	}
	sRecordId[0] = '\0';
	if((Text.Data == NULL) || (Text.Size == 0u) ||
		(Text.Size >= iCapacity))
	{
		return false;
	}
	for(i = 0u; i < Text.Size; ++i)
	{
		if(!xacmeAliUnreserved(Text.Data[i]))
		{
			return false;
		}
	}
	memcpy(sRecordId, Text.Data, Text.Size);
	sRecordId[Text.Size] = '\0';
	return true;
}

bool xacmeDnsAliCreateResponseId(
	cstr sBody, char* sRecordId, size_t iCapacity)
{
	xvalue* pRoot;
	xvalue* pId;
	xstrview Text;
	bool bOk = false;
	if((sBody == NULL) || (sRecordId == NULL) || (iCapacity == 0u))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_ARGUMENT,
			"alidns create response arguments are invalid");
		return false;
	}
	sRecordId[0] = '\0';
	pRoot = xrtJsonParse((xstrview){ sBody, strlen(sBody) });
	/* An error envelope never establishes ownership, even if it carries an id. */
	pId = (pRoot != NULL) && xrtValueIs(pRoot, XVALUE_OBJECT) &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Code")) == NULL ?
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("RecordId")) : NULL;
	if((pId != NULL) && xrtValueGetString(pId, &Text))
	{
		bOk = xacmeAliRecordIdCopy(Text, sRecordId, iCapacity);
	}
	if(!bOk && pRoot != NULL)
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
			"alidns create response lacks a usable record id");
	xrtValueRelease(pRoot);
	return bOk;
}

/* 在响应 JSON 里取安全的 RecordId 并存档。 */
static bool xacmeAliSaveRecordId(xacmednsalicontext* pCtx, size_t iSlot, cstr sBody,
	xstrview sFqdn, xstrview sTxt)
{
	char sId[XACME_DNS_RECORD_TEXT_CAP];
	bool bTracked = xacmeDnsAliCreateResponseId(sBody, sId, sizeof(sId));
	if(bTracked) {
		for(size_t i = 0u; i < pCtx->Records.iCount; i++) {
			if(strcmp(pCtx->Records.sIds[i], sId) != 0) continue;
			xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
				"alidns create response reuses a tracked record id");
			bTracked = false;
			break;
		}
	}
	xacmeAliStoreRecord(pCtx, iSlot, bTracked ? sId : NULL, sFqdn, sTxt);
	return bTracked ? true : xacmeAliUncertainError();
}

static bool xacmeAliTextEquals(xstrview Text, cstr sExpected)
{
	return Text.Data != NULL && Text.Size == strlen(sExpected) &&
		memcmp(Text.Data, sExpected, Text.Size) == 0;
}

static unsigned char xacmeAliLower(unsigned char c)
{
	return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}

/* Both entry points validate the bounded ASCII view before copying it. */
static xstrview xacmeAliCanonicalOwner(xstrview Fqdn, char* sOut)
{
	size_t i;
	for(i = 0u; i < Fqdn.Size; i++) sOut[i] = (char)xacmeAliLower((unsigned char)Fqdn.Data[i]);
	sOut[Fqdn.Size] = '\0';
	return (xstrview){ sOut, Fqdn.Size };
}

static bool xacmeAliDomainEquals(cstr sA, cstr sB)
{
	for(; *sA != '\0' && *sB != '\0'; sA++, sB++)
		if(xacmeAliLower((unsigned char)*sA) != xacmeAliLower((unsigned char)*sB)) return false;
	return *sA == *sB;
}

static bool xacmeAliJsonUInt(const xvalue* pObject, cstr sKey, uint64* pOut)
{
	xvalue* pValue = xrtValueObjectGet(pObject, (xstrview){ sKey, strlen(sKey) });
	int64 iSigned;
	if(pValue == NULL) return false;
	if(xrtValueIs(pValue, XVALUE_UINT)) return xrtValueGetUInt(pValue, pOut);
	if(!xrtValueIs(pValue, XVALUE_INT) || !xrtValueGetInt(pValue, &iSigned) || iSigned < 0) return false;
	*pOut = (uint64)iSigned;
	return true;
}

static void xacmeAliResponseFailure(uint16 iStatus, xstrview Code)
{
	xerrkind Kind = XERR_PROTOCOL;
	int32 iCode = XACME_DNS_ERROR_PROTOCOL;
	cstr sMessage = "alidns response is invalid";
	if(iStatus >= 500u) {
		Kind = XERR_IO; iCode = XACME_DNS_ERROR_NETWORK;
		sMessage = "alidns server failure";
	} else if(iStatus == 401u || iStatus == 403u ||
		xacmeAliTextEquals(Code, "Forbidden") || xacmeAliTextEquals(Code, "Forbidden.RAM")) {
		Kind = XERR_PERMISSION; iCode = XACME_DNS_ERROR_CREDENTIAL;
		sMessage = "alidns access denied";
	} else if(iStatus == 429u || xacmeAliTextEquals(Code, "Throttling") ||
		xacmeAliTextEquals(Code, "Throttling.User")) {
		Kind = XERR_AGAIN; iCode = XACME_DNS_ERROR_NETWORK;
		sMessage = "alidns request throttled";
	}
	xrtSetErrorInfo(Kind, "xrt.acme.dns", iCode, sMessage);
}

static xacmednsalizoneoutcome xacmeAliZoneFailure(uint16 iStatus, xstrview Code)
{
	xacmeAliResponseFailure(iStatus, Code);
	return XACME_ALI_ZONE_ERROR;
}

xacmednsalirecordoutcome xacmeDnsAliRecordResponse(
	uint16 iStatus, cstr sBody, cstr sId, cstr sFqdn, cstr sTxt, bool* pEnabled)
{
	xvalue* pRoot;
	xvalue* pCode;
	xstrview Code = { NULL, 0u };
	char sText[320], sRr[256], sDomain[256], sOwner[512];
	bool bEnabled = false;
	xacmednsalirecordoutcome Result = XACME_ALI_RECORD_ERROR;
	if(pEnabled != NULL) *pEnabled = false;
	if(sBody == NULL || sId == NULL || sId[0] == '\0' || sFqdn == NULL || sTxt == NULL) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_ARGUMENT,
			"alidns record response arguments are invalid");
		return Result;
	}
	pRoot = xrtJsonParse((xstrview){ sBody, strlen(sBody) });
	if(pRoot == NULL) {
		if(xrtErrorKind(xrtGetError()) != XERR_MEMORY) xacmeAliResponseFailure(iStatus, Code);
		return Result;
	}
	if(!xrtValueIs(pRoot, XVALUE_OBJECT)) goto Invalid;
	pCode = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Code"));
	if(pCode != NULL) {
		if(!xrtValueGetString(pCode, &Code)) goto Invalid;
		if((iStatus == 400u || iStatus == 404u) &&
			(xacmeAliTextEquals(Code, "DomainRecordNotBelongToUser") ||
			 xacmeAliTextEquals(Code, "InvalidRR.NoExist")) &&
			xacmeDnsJsonText(pRoot, "RequestId", sText, sizeof(sText)) && sText[0] != '\0' &&
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("RecordId")) == NULL) {
			Result = XACME_ALI_RECORD_MISSING;
			goto Done;
		}
		goto Invalid;
	}
	if(iStatus < 200u || iStatus >= 300u ||
		!xacmeDnsJsonText(pRoot, "RequestId", sText, sizeof(sText)) || sText[0] == '\0' ||
		!xacmeDnsJsonPathId(pRoot, "RecordId", sText, sizeof(sText)) || strcmp(sId, sText) != 0 ||
		!xacmeDnsJsonText(pRoot, "Type", sText, sizeof(sText)) || strcmp(sText, "TXT") != 0 ||
		!xacmeDnsJsonText(pRoot, "Line", sText, sizeof(sText)) || strcmp(sText, "default") != 0 ||
		!xacmeDnsJsonText(pRoot, "Value", sText, sizeof(sText)) || strcmp(sTxt, sText) != 0 ||
		!xacmeDnsJsonText(pRoot, "RR", sRr, sizeof(sRr)) || sRr[0] == '\0' ||
		!xacmeDnsJsonText(pRoot, "DomainName", sDomain, sizeof(sDomain)) || sDomain[0] == '\0' ||
		!xacmeAliFormat(sOwner, sizeof(sOwner), "%s.%s", sRr, sDomain) ||
		!xacmeAliDomainEquals(sOwner, sFqdn) ||
		!xacmeDnsJsonText(pRoot, "Status", sText, sizeof(sText))) goto Invalid;
	/* The live service also returns uppercase states; accept only the two
	 * complete spellings of each documented state. */
	if(strcmp(sText, "Enable") == 0 || strcmp(sText, "ENABLE") == 0) bEnabled = true;
	else if(strcmp(sText, "Disable") != 0 && strcmp(sText, "DISABLE") != 0) goto Invalid;
	Result = XACME_ALI_RECORD_FOUND;
	if(pEnabled != NULL) *pEnabled = bEnabled;
	goto Done;
Invalid:
	xacmeAliResponseFailure(iStatus, Code);
Done:
	xrtValueRelease(pRoot);
	return Result;
}

static xacmednsalirecordoutcome xacmeAliReadRecord(
	xacmednsalicontext* pCtx, size_t iSlot, bool* pEnabled)
{
	char sQuery[384];
	str sBody = NULL;
	uint16 iStatus = 0u;
	xacmednsalirecordoutcome Result;
	if(!xacmeAliFormat(sQuery, sizeof(sQuery), "RecordId=%s", pCtx->Records.sIds[iSlot]))
		return XACME_ALI_RECORD_ERROR;
	if(!xacmeAliCall(pCtx, "DescribeDomainRecordInfo", sQuery, &iStatus, &sBody))
		return XACME_ALI_RECORD_ERROR;
	Result = xacmeDnsAliRecordResponse(iStatus, sBody, pCtx->Records.sIds[iSlot],
		pCtx->Records.sFqdns[iSlot], pCtx->Records.sTxts[iSlot], pEnabled);
	xrtFree(sBody);
	return Result;
}

xacmednsalizoneoutcome xacmeDnsAliZoneResponse(uint16 iStatus, cstr sBody, cstr sDomain)
{
	xvalue* pRoot;
	xvalue* pCode;
	xvalue* pRecords;
	xvalue* pList;
	xstrview Code = { NULL, 0u };
	uint64 iTotal, iPage, iSize;
	char sText[320];
	xacmednsalizoneoutcome Result;
	if(sBody == NULL || sDomain == NULL || sDomain[0] == '\0') {
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_ARGUMENT,
			"alidns zone response arguments are invalid");
		return XACME_ALI_ZONE_ERROR;
	}
	pRoot = xrtJsonParse((xstrview){ sBody, strlen(sBody) });
	if(pRoot == NULL) {
		if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return XACME_ALI_ZONE_ERROR;
		return xacmeAliZoneFailure(iStatus, Code);
	}
	Result = XACME_ALI_ZONE_ERROR;
	if(!xrtValueIs(pRoot, XVALUE_OBJECT)) goto Invalid;
	pCode = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Code"));
	if(pCode != NULL) {
		if(!xrtValueGetString(pCode, &Code)) goto Invalid;
		if((iStatus == 400u || iStatus == 404u) &&
			(xacmeAliTextEquals(Code, "InvalidDomainName.NoExist") ||
			 xacmeAliTextEquals(Code, "DomainNotFound"))) {
			Result = XACME_ALI_ZONE_MISSING;
			goto Done;
		}
		goto Invalid;
	}
	if(iStatus < 200u || iStatus >= 300u) goto Invalid;
	pRecords = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("DomainRecords"));
	pList = pRecords != NULL && xrtValueIs(pRecords, XVALUE_OBJECT) ?
		xrtValueObjectGet(pRecords, XRT_STR_LITERAL("Record")) : NULL;
	if(!xacmeAliJsonUInt(pRoot, "TotalCount", &iTotal) ||
		!xacmeAliJsonUInt(pRoot, "PageNumber", &iPage) || iPage != 1u ||
		!xacmeAliJsonUInt(pRoot, "PageSize", &iSize) || iSize != 1u ||
		!xacmeDnsJsonText(pRoot, "RequestId", sText, sizeof(sText)) || sText[0] == '\0' ||
		pList == NULL || !xrtValueIs(pList, XVALUE_ARRAY) ||
		xrtValueCount(pList) != (iTotal == 0u ? 0u : 1u)) goto Invalid;
	if(iTotal != 0u) {
		xvalue* pItem = xrtValueArrayGet(pList, 0u);
		if(!xacmeDnsJsonText(pItem, "DomainName", sText, sizeof(sText)) ||
			!xacmeAliDomainEquals(sText, sDomain) ||
			!xacmeDnsJsonPathId(pItem, "RecordId", sText, sizeof(sText))) goto Invalid;
	}
	Result = XACME_ALI_ZONE_FOUND;
	goto Done;
Invalid:
	Result = xacmeAliZoneFailure(iStatus, Code);
Done:
	xrtValueRelease(pRoot);
	return Result;
}

/* Return the verified result even when all bounded cache entries are occupied. */
static bool xacmeAliFindZone(xacmednsalicontext* pCtx, cstr sZoneStart, char* sOutZone)
{
	char sZone[256];
	char sStart[256];
	uint16 iStatus = 0u;
	str sBody = NULL;
	char sBodyText[512];
	size_t i;
	for(i = 0u; sZoneStart[i] != '\0'; i++) sStart[i] = (char)xacmeAliLower((unsigned char)sZoneStart[i]);
	sStart[i] = '\0';
	for(i = 0u; i < pCtx->iZoneCount; i++)
		if(strcmp(sStart, pCtx->sZoneStarts[i]) == 0) {
			strcpy(sOutZone, pCtx->sZones[i]); return true;
		}
	strcpy(sZone, sStart);
	for(;;)
	{
		xacmednsalizoneoutcome Result;
		if(!xacmeAliFormat(
			sBodyText, sizeof(sBodyText),
			"DomainName=%s&PageNumber=1&PageSize=1", sZone))
			return false;
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
		Result = xacmeDnsAliZoneResponse(iStatus, sBody, sZone);
		xrtFree(sBody);
		sBody = NULL;
		if(Result == XACME_ALI_ZONE_ERROR) return false;
		if(Result == XACME_ALI_ZONE_FOUND)
		{
			size_t iSlot = pCtx->iZoneNext;
			strcpy(sOutZone, sZone);
			strcpy(pCtx->sZoneStarts[iSlot], sStart);
			strcpy(pCtx->sZones[iSlot], sZone);
			pCtx->iZoneNext = (iSlot + 1u) % XACME_ALI_ZONE_MAX;
			if(pCtx->iZoneCount < XACME_ALI_ZONE_MAX) pCtx->iZoneCount++;
			return true;
		}
		{
			char* sDot = strchr(sZone, '.');
			if((sDot == NULL) || (strchr(sDot + 1, '.') == NULL))
			{
				xrtSetErrorInfo(XERR_NOT_FOUND, "xrt.acme.dns", XACME_DNS_ERROR_ZONE,
					"alidns zone discovery found no managed domain");
				return false;
			}
			memmove(sZone, sDot + 1, strlen(sDot + 1) + 1u);
		}
	}
}

static bool xacmeAliAddLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsalicontext* pCtx =
		(xacmednsalicontext*)pProvider->pContext;
	char sFqdnText[256];
	char sRr[256];
	char sZone[256];
	char sBody[1024];
	char sTxtText[208];
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iSlot;
	if(!xacmeAliQueryInputValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"alidns DNS-01 owner or digest contains invalid characters");
		return false;
	}
	sFqdn = xacmeAliCanonicalOwner(sFqdn, sFqdnText);
	if(xacmeAliPairUncertain(pCtx, sFqdn, sTxt)) return xacmeAliUncertainError();
	iSlot = xacmeAliOwnedSlot(pCtx, sFqdn, sTxt);
	if(iSlot < XACME_DNS_RECORD_MAX) {
		bool bEnabled = false;
		xacmednsalirecordoutcome Result = xacmeAliReadRecord(pCtx, iSlot, &bEnabled);
		if(Result == XACME_ALI_RECORD_ERROR) return false;
		if(Result == XACME_ALI_RECORD_FOUND) {
			if(bEnabled) return true;
			xrtSetErrorInfo(XERR_STATE, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
				"alidns owned record is disabled");
			return false;
		}
		/* Only explicit absence of the tracked id frees it for a new create. */
		pCtx->Records.sIds[iSlot][0] = '\0';
		pCtx->Records.sFqdns[iSlot][0] = '\0';
		pCtx->Records.sTxts[iSlot][0] = '\0';
	}
	iSlot = xacmeAliFreeSlot(pCtx);
	if(iSlot >= XACME_DNS_RECORD_MAX)
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"alidns record tracking capacity exhausted");
		return false;
	}
	if(!xacmeAliSplit(
		sFqdnText, sRr, sizeof(sRr), sZone, sizeof(sZone)))
	{
		return false;
	}
	if(!xacmeAliFindZone(pCtx, sZone, sZone))
	{
		return false;
	}
	{
		size_t iZoneLen;
		size_t iFqdnLen = strlen(sFqdnText);
		size_t iRrLen;
		/* RR = FQDN 去掉 ".zone" 后缀的完整前缀（zone 试探可能
		   剥掉多段，不能只用最左段）。 */
		iZoneLen = strlen(sZone);
		if((iFqdnLen <= iZoneLen + 1u) ||
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
		if(!xacmeAliFormat(
			sBody, sizeof(sBody),
			"DomainName=%s&RR=%s&Type=TXT&Value=%s",
			sZone, sRr, sTxtText))
			return false;
	}
	/* 同名同值并不证明记录所有权；只删除本实例保存的 RecordId。 */
	if(!xacmeAliCall(
		pCtx, "AddDomainRecord", sBody, &iStatus, &sResp))
	{
		if(pCtx->Http.bWriteUncertain)
		{
			xacmeAliStoreRecord(pCtx, iSlot, NULL, sFqdn, sTxt);
			return xacmeAliUncertainError();
		}
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
		if(iStatus >= 500u)
		{
			xacmeAliStoreRecord(pCtx, iSlot, NULL, sFqdn, sTxt);
			xrtSetErrorInfo(XERR_IO, "xrt.acme.dns", XACME_DNS_ERROR_NETWORK,
				"alidns server failure after create request");
			return xacmeAliUncertainError();
		}
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
			"alidns record creation was rejected");
		return false;
	}
	{
		bool bTracked = xacmeAliSaveRecordId(pCtx, iSlot, sResp, sFqdn, sTxt);
		xrtFree(sResp);
		return bTracked;
	}
}

static bool xacmeAliDeleteRecord(xacmednsalicontext* pCtx, size_t iSlot)
{
	cstr sId = pCtx->Records.sIds[iSlot];
	char sBody[384];
	char sSafeId[XACME_DNS_RECORD_TEXT_CAP];
	char sReturnedId[XACME_DNS_RECORD_TEXT_CAP];
	uint16 iStatus = 0u;
	str sResp = NULL;
	bool bOk;
	if((sId == NULL) ||
		!xacmeAliRecordIdCopy(
			(xstrview){ sId, strlen(sId) }, sSafeId, sizeof(sSafeId)))
	{
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
			XACME_DNS_ERROR_PROTOCOL, "alidns tracked record id is invalid");
		return false;
	}
	{
		xacmednsalirecordoutcome Result = xacmeAliReadRecord(pCtx, iSlot, NULL);
		if(Result != XACME_ALI_RECORD_FOUND) return Result == XACME_ALI_RECORD_MISSING;
	}
	if(!xacmeAliFormat(sBody, sizeof(sBody), "RecordId=%s", sSafeId))
		return false;
	if(!xacmeAliCall(pCtx, "DeleteDomainRecord", sBody,
			&iStatus, &sResp))
	{
		xrtFree(sResp);
		return false;
	}
	if(iStatus >= 200u && iStatus < 300u) {
		bOk = xacmeDnsAliCreateResponseId(sResp, sReturnedId, sizeof(sReturnedId)) &&
			strcmp(sSafeId, sReturnedId) == 0;
	} else {
		/* A precise account-scoped absence also handles a concurrent delete. */
		bOk = xacmeDnsAliRecordResponse(iStatus, sResp, sSafeId,
			pCtx->Records.sFqdns[iSlot], pCtx->Records.sTxts[iSlot], NULL) == XACME_ALI_RECORD_MISSING;
	}
	xrtFree(sResp);
	if(!bOk && iStatus >= 200u && iStatus < 300u && xrtErrorKind(xrtGetError()) != XERR_MEMORY)
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
			XACME_DNS_ERROR_PROTOCOL, "alidns record deletion failed");
	return bOk;
}

static bool xacmeAliRemoveLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsalicontext* pCtx =
		(xacmednsalicontext*)pProvider->pContext;
	char sFqdnText[256];
	if(!xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_ARGUMENT,
			"alidns DNS-01 owner or digest is invalid");
		return false;
	}
	sFqdn = xacmeAliCanonicalOwner(sFqdn, sFqdnText);
	if(xacmeAliPairUncertain(pCtx, sFqdn, sTxt)) return xacmeAliUncertainError();
	/* Keep the caller's tuple bound to its slot even in an older duplicate-id ledger. */
	for(size_t i = 0u; i < pCtx->Records.iCount; i++) {
		if(pCtx->Records.sIds[i][0] == '\0' || !xacmeAliPairMatches(pCtx, i, sFqdn, sTxt)) continue;
		if(!xacmeAliDeleteRecord(pCtx, i)) return false;
		pCtx->Records.sIds[i][0] = '\0';
		pCtx->Records.sFqdns[i][0] = '\0';
		pCtx->Records.sTxts[i][0] = '\0';
	}
	return true;
}

static bool xacmeAliAdd(xacmednsprovider* pProvider, xstrview Fqdn, xstrview Txt)
{
	xacmednsalicontext* pCtx = (xacmednsalicontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeAliAddLocked(pProvider, Fqdn, Txt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

static bool xacmeAliRemove(xacmednsprovider* pProvider, xstrview Fqdn, xstrview Txt)
{
	xacmednsalicontext* pCtx = (xacmednsalicontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeAliRemoveLocked(pProvider, Fqdn, Txt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
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
}

bool xrtAcmeDnsAli(
	const xacmednaliconfig* pConfig,
	struct xnetengine* pBorrowedEngine, xacmednsprovider* pProvider)
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
	if(strlen(pConfig->sAccessKeyId) >= sizeof(pCtx->sKeyId) ||
		strlen(pConfig->sAccessKeySecret) >= sizeof(pCtx->sSecret) ||
		((pConfig->sEndpoint != NULL) &&
		 strlen(pConfig->sEndpoint) >= sizeof(pCtx->sEndpoint)))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_CREDENTIAL,
			"alidns credentials or endpoint exceed capacity");
		return false;
	}
	pCtx = (xacmednsalicontext*)xrtCalloc(1, sizeof(*pCtx));
	if(pCtx == NULL)
	{
		return false;
	}
	if(!xrtMutexInit(&pCtx->Lock))
	{
		xrtFree(pCtx);
		return false;
	}
	strcpy(pCtx->sKeyId, pConfig->sAccessKeyId);
	strcpy(pCtx->sSecret, pConfig->sAccessKeySecret);
	strcpy(pCtx->sEndpoint, (pConfig->sEndpoint != NULL) ?
		pConfig->sEndpoint : "alidns.aliyuncs.com");
	if(!xacmeHttpInit(&pCtx->Http, pBorrowedEngine, NULL, 0u))
	{
		bool bReady = xacmeHttpUnit(&pCtx->Http);
		(void)xrtMutexUnit(&pCtx->Lock);
		if(bReady)
		{
			xrtSecureZero(pCtx, sizeof(*pCtx));
			xrtFree(pCtx);
		}
		else xacmeHttpDeferOwner(&pCtx->Http, sizeof(*pCtx));
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
		if(!xacmeHttpUnit(&pCtx->Http)) return;
		(void)xrtMutexUnit(&pCtx->Lock);
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
