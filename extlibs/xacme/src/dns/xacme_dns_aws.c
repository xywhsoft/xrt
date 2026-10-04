#include <xrt/acme_dns_aws.h>

#if defined(XACME_FEATURE_DNS_AWS)

#include "../internal/xacme_dnscommon.h"
#include "../internal/xacme_dns_aws_internal.h"
#include "../internal/xacme_http.h"
#include "../internal/xacme_sigv4.h"

#include <xrt/buffer.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <xrt/time.h>

#include <stdlib.h>

/*
	AWS Route53 provider（SigV4 + XML API 2013-04-01）：
	  - canonical：host/x-amz-content-sha256/x-amz-date 三头（字典序）；
	  - StringToSign = "AWS4-HMAC-SHA256\n<x-amz-date>\n"
	    "<date>/<region>/route53/aws4_request\n<sha256hex(canonical)>"；
	  - 密钥链：HMAC("AWS4"+SK, date) → region → "route53" →
	    "aws4_request"；payload hash 走 x-amz-content-sha256 头；
	  - zone 发现：GET /hostedzonesbyname?dnsname=<候选>（逐级上探）；
	  - 同名 TXT 使用 ListResourceRecordSets 读取后 DELETE/CREATE 原子替换；
	  - 冲突时重新读取，保留原 TTL 与其他 TXT 值；
	  - Change 异步但权威侧近即时，不轮询 INSYNC。
	记录句柄无服务端 id，Remove 根据保存的 zoneid、fqdn、value 读取并删值。
*/

#define XACME_AWS_SERVICE "route53"
#define XACME_AWS_API "2013-04-01"

typedef struct xacmednsawscontext {
	xacmehttp Http;
	xmutex Lock;
	char sId[160];
	char sKey[160];
	char sRegion[32];
	char sEndpoint[160];
	/* 记录句柄：zone id + fqdn + 值（删除需完整回放）。 */
	struct
	{
		char sZoneId[64];
		char sFqdn[256];
		char sValue[208];
		bool bUncertain;
	} Records[XACME_DNS_RECORD_MAX];
	size_t iRecordCount;
} xacmednsawscontext;

typedef enum xacmeawsmutationresult {
	XACME_AWS_FAILED,
	XACME_AWS_UNCHANGED,
	XACME_AWS_CHANGED,
	XACME_AWS_CONFLICT,
	XACME_AWS_BUSY,
	XACME_AWS_UNCERTAIN
} xacmeawsmutationresult;

static bool xacmeAwsUncertain(void)
{
	xerror* pError = xrtErrorWrap(xrtGetError(), XERR_IO, "xrt.acme.dns",
		XACME_DNS_ERROR_UNCERTAIN,
		"route53 add ownership unknown; reconcile before another add or cleanup");
	if(pError != NULL) xrtSetErrorTake(pError);
	return false;
}

bool xacmeAwsSigningKey(cstr sSecret, cstr sDate, cstr sRegion,
	uint8 pKey[32])
{
	char sKeySeed[180];
	uint8 kDate[XRT_SHA256_SIZE];
	uint8 kRegion[XRT_SHA256_SIZE];
	uint8 kService[XRT_SHA256_SIZE];
	int iWritten;
	bool bOk;

	if((sSecret == NULL) || (sDate == NULL) || (sRegion == NULL) ||
		(pKey == NULL))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_aws signing key arguments are invalid");
		return false;
	}
	xrtSecureZero(pKey, XRT_SHA256_SIZE);
	iWritten = snprintf(sKeySeed, sizeof(sKeySeed), "AWS4%s", sSecret);
	if((iWritten < 0) || ((size_t)iWritten >= sizeof(sKeySeed)))
	{
		xrtSecureZero(sKeySeed, sizeof(sKeySeed));
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_aws secret exceeds signing buffer");
		return false;
	}
	bOk = xacmeSigHmac((const uint8*)sKeySeed, strlen(sKeySeed),
		sDate, strlen(sDate), kDate) &&
		xacmeSigHmac(kDate, sizeof(kDate), sRegion, strlen(sRegion),
			kRegion) &&
		xacmeSigHmac(kRegion, sizeof(kRegion), XACME_AWS_SERVICE,
			strlen(XACME_AWS_SERVICE), kService) &&
		xacmeSigHmac(kService, sizeof(kService), "aws4_request",
			sizeof("aws4_request") - 1u, pKey);
	xrtSecureZero(sKeySeed, sizeof(sKeySeed));
	xrtSecureZero(kDate, sizeof(kDate));
	xrtSecureZero(kRegion, sizeof(kRegion));
	xrtSecureZero(kService, sizeof(kService));
	if(!bOk)
	{
		xrtSecureZero(pKey, XRT_SHA256_SIZE);
	}
	return bOk;
}

/* 执行一次 SigV4 调用（sBody 为 XML 或 NULL）。 */
static bool xacmeAwsCall(
	xacmednsawscontext* pCtx, cstr sMethod, cstr sPathAndQuery,
	cstr sContentType, cstr sBody, uint16* pOutStatus,
	str* pOutBody, size_t* pOutBodySize)
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
	char sUrl[640];
	char sCanonicalPath[440];
	cstr sQuery = "";
	xacmehttpheader Extra[3];
	xacmehttpresponse R;
	xdatetime Now;
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
	{
		const char* pQuestion = strchr(sPathAndQuery, '?');
		size_t iPathSize = (pQuestion != NULL) ?
			(size_t)(pQuestion - sPathAndQuery) : strlen(sPathAndQuery);
		if((iPathSize == 0u) || (iPathSize >= sizeof(sCanonicalPath)))
			return false;
		memcpy(sCanonicalPath, sPathAndQuery, iPathSize);
		sCanonicalPath[iPathSize] = '\0';
		if(pQuestion != NULL) sQuery = pQuestion + 1u;
	}
	if(!xacmeSigCanonical(sCanonical, sizeof(sCanonical), sMethod,
			sCanonicalPath, sQuery, sHeaders, sSignedHeaders, sPayloadHash))
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
	if(!xacmeAwsSigningKey(pCtx->sKey, sDateText, pCtx->sRegion,
			kSigning))
	{
		return false;
	}
	{
		bool bSigned = xacmeSigHmac(kSigning, sizeof(kSigning),
			sStringToSign, strlen(sStringToSign), Signature);
		xrtSecureZero(kSigning, sizeof(kSigning));
		if(!bSigned)
		{
			return false;
		}
	}
	xacmeSigHex(Signature, sizeof(Signature), sHex);
	xrtSecureZero(Signature, sizeof(Signature));
	snprintf(sAuth, sizeof(sAuth),
		"AWS4-HMAC-SHA256 Credential=%s/%s/%s/" XACME_AWS_SERVICE
		"/aws4_request, SignedHeaders=%s, Signature=%s",
		pCtx->sId, sDateText, pCtx->sRegion, sSignedHeaders, sHex);
	{
		int iUrlSize = snprintf(sUrl, sizeof(sUrl), "https://%s%s",
			pCtx->sEndpoint, sPathAndQuery);
		if((iUrlSize < 0) || ((size_t)iUrlSize >= sizeof(sUrl)))
			return false;
	}

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
	*pOutBodySize = R.iBodySize;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	return true;
}

/* hostedzonesbyname 精确匹配候选 zone（返回 hostedzone id）。 */
static int xacmeAwsZoneId(
	xacmednsawscontext* pCtx, cstr sZone, char* sOutId, size_t iIdCap)
{
	char sPath[400];
	uint16 iStatus = 0u;
	str sBody = NULL;
	size_t iBodySize = 0u;
	int iResult;
	int iPathSize;

	/* Zone 名在 Route53 中带尾点。 */
	iPathSize = snprintf(sPath, sizeof(sPath), "/" XACME_AWS_API
		"/hostedzonesbyname?dnsname=%s.&maxitems=100", sZone);
	if((iPathSize < 0) || ((size_t)iPathSize >= sizeof(sPath)) ||
		!xacmeAwsCall(pCtx, "GET", sPath, NULL, NULL,
			&iStatus, &sBody, &iBodySize))
	{
		return -1;
	}
	iResult = ((iStatus >= 200u) && (iStatus < 300u) &&
		(sBody != NULL) && (iBodySize <= 4u * 1024u * 1024u) &&
		(strlen(sBody) == iBodySize)) ?
		xacmeAwsSelectPublicZone(sBody, sZone, sOutId, iIdCap) : -1;
	xrtFree(sBody);
	if(iResult < 0)
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
			XACME_DNS_ERROR_ZONE,
			"acme dns_aws hosted zone listing is invalid or ambiguous");
	return iResult;
}

static bool xacmeAwsFindZone(
	xacmednsawscontext* pCtx, cstr sFqdn, char* sOutId, size_t iIdCap)
{
	cstr sCandidate = sFqdn;
	/* A previously found parent cannot prove that a child zone is absent.
	 * Probe the complete owner too: DNS-01 may be delegated at its apex. */
	for(;;)
	{
		const char* sDot;
		int iZoneResult = xacmeAwsZoneId(pCtx, sCandidate, sOutId, iIdCap);
		if(iZoneResult < 0) return false;
		if(iZoneResult == 1) return true;
		sDot = strchr(sCandidate, '.');
		if(sDot == NULL)
		{
			xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_ZONE,
				"acme dns_aws hosted zone was not found");
			return false;
		}
		sCandidate = sDot + 1u;
	}
}

/* 读取精确 TXT 记录集。最多请求一个结果，首项不匹配即记录不存在。 */
static bool xacmeAwsReadTxt(xacmednsawscontext* pCtx,
	cstr sZoneId, cstr sFqdn, xacmeawstxtset* pSet, str* pBody)
{
	char sPath[440];
	uint16 iStatus = 0u;
	size_t iBodySize = 0u;
	int iPathSize = snprintf(sPath, sizeof(sPath),
		"/" XACME_AWS_API "/hostedzone/%s/rrset?maxitems=1&name=%s.&type=TXT",
		sZoneId, sFqdn);
	*pBody = NULL;
	if((iPathSize < 0) || ((size_t)iPathSize >= sizeof(sPath)) ||
		!xacmeAwsCall(pCtx, "GET", sPath, NULL, NULL,
			&iStatus, pBody, &iBodySize))
		return false;
	if((iStatus < 200u) || (iStatus >= 300u) ||
		(iBodySize > 4u * 1024u * 1024u) ||
		(*pBody == NULL) || (strlen(*pBody) != iBodySize) ||
		!xacmeAwsParseTxtSet(*pBody, sFqdn, pSet))
	{
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
			XACME_DNS_ERROR_PROTOCOL,
			"acme dns_aws TXT listing was rejected or malformed");
		xrtFree(*pBody);
		*pBody = NULL;
		return false;
	}
	return true;
}

/* 冲突或提交结果不确定时，调用方重新读取当前记录集。 */
static xacmeawsmutationresult xacmeAwsChangeTxt(xacmednsawscontext* pCtx,
	cstr sZoneId, cstr sBody)
{
	char sPath[128];
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	bool bOk;
	xacmeawsmutationresult Result = XACME_AWS_FAILED;
	int iPathSize = snprintf(sPath, sizeof(sPath),
		"/" XACME_AWS_API "/hostedzone/%s/rrset/", sZoneId);
	if((iPathSize < 0) || ((size_t)iPathSize >= sizeof(sPath)))
		return XACME_AWS_FAILED;
	bOk = xacmeAwsCall(pCtx, "POST", sPath, "application/xml", sBody,
		&iStatus, &sResp, &iRespSize);
	/* 响应丢失时提交结果不确定；下轮读取权威当前状态再判定。 */
	if(!bOk && pCtx->Http.bWriteUncertain) Result = XACME_AWS_UNCERTAIN;
	if(bOk && iStatus >= 500u) Result = XACME_AWS_UNCERTAIN;
	if(bOk && (iStatus >= 200u) && (iStatus < 300u) &&
		((sResp == NULL) || (iRespSize > 4u * 1024u * 1024u) ||
		 (strlen(sResp) != iRespSize) || !xacmeAwsChangeAccepted(sResp))) {
		bOk = false;
		Result = XACME_AWS_UNCERTAIN;
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
			"acme dns_aws TXT change acknowledgment was malformed");
	}
	if(bOk && ((iStatus < 200u) || (iStatus >= 300u))) {
		char Code[64];
		bool bError = xacmeAwsErrorCode((xstrview){ sResp, iRespSize }, Code);
		if(iStatus >= 400u && iStatus < 500u && bError) {
			bool bCredential = strcmp(Code, "AccessDenied") == 0 ||
				strcmp(Code, "InvalidClientTokenId") == 0 || strcmp(Code, "SignatureDoesNotMatch") == 0;
			if(iStatus == 400u && strcmp(Code, "InvalidChangeBatch") == 0)
				Result = XACME_AWS_CONFLICT;
			else if(iStatus == 400u && (strcmp(Code, "Throttling") == 0 ||
				strcmp(Code, "PriorRequestNotComplete") == 0)) Result = XACME_AWS_BUSY;
			xrtSetErrorInfo(Result == XACME_AWS_BUSY ? XERR_AGAIN : bCredential ? XERR_PERMISSION : XERR_PROTOCOL,
				"xrt.acme.dns", Result == XACME_AWS_BUSY ? XACME_DNS_ERROR_NETWORK :
				bCredential ? XACME_DNS_ERROR_CREDENTIAL : XACME_DNS_ERROR_PROTOCOL,
				"acme dns_aws TXT change was rejected");
		} else {
			/* A malformed error does not establish atomic rejection. In
			 * particular it cannot authorize either provider or flow replay. */
			Result = XACME_AWS_UNCERTAIN;
			xrtSetErrorInfo(iStatus >= 500u ? XERR_IO : XERR_PROTOCOL, "xrt.acme.dns",
				iStatus >= 500u ? XACME_DNS_ERROR_NETWORK : XACME_DNS_ERROR_PROTOCOL,
				"acme dns_aws TXT change outcome unknown after invalid error response");
		}
	}
	xrtFree(sResp);
	return bOk && (iStatus >= 200u) && (iStatus < 300u) ? XACME_AWS_CHANGED : Result;
}

static xacmeawsmutationresult xacmeAwsMutateTxt(xacmednsawscontext* pCtx,
	cstr sZoneId, cstr sFqdn, cstr sValue, bool bAdd)
{
	unsigned iAttempt;
	bool bRetried = false;
	xacmeawsmutationresult Last = XACME_AWS_FAILED;
	for(iAttempt = 0u; iAttempt < 4u; iAttempt++)
	{
		xacmeawstxtset Set;
		xbuffer Request;
		str sReadBody = NULL;
		bool bChanged = false;
		bool bOk;
		xacmeawsmutationresult Result;
		if(!xacmeAwsReadTxt(pCtx, sZoneId, sFqdn, &Set, &sReadBody))
			return XACME_AWS_FAILED;
		xrtBufferInit(&Request);
		bOk = xacmeAwsBuildTxtChange(&Set, sFqdn, sValue, bAdd,
			&Request, &bChanged);
		xrtFree(sReadBody);
		if(!bOk)
		{
			xrtBufferUnit(&Request);
			if(xrtErrorKind(xrtGetError()) == XERR_NONE)
				xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
					XACME_DNS_ERROR_PROTOCOL,
					"acme dns_aws TXT change exceeds supported size");
			return XACME_AWS_FAILED;
		}
		if(!bChanged)
		{
			xrtBufferUnit(&Request);
			if(bRetried) xrtClearError();
			return XACME_AWS_UNCHANGED;
		}
		Result = xacmeAwsChangeTxt(pCtx, sZoneId, (const char*)Request.Data);
		xrtBufferUnit(&Request);
		if(Result == XACME_AWS_CHANGED)
		{
			if(bRetried) xrtClearError();
			return Result;
		}
		/* An observed value after a lost create acknowledgment cannot prove
		 * who created it. Only deletion of an already-owned value is reconciled. */
		if(Result == XACME_AWS_FAILED || (bAdd && Result == XACME_AWS_UNCERTAIN)) return Result;
		Last = Result;
		bRetried = true;
		if(iAttempt + 1u < 4u) xrtSleep(500u << iAttempt);
	}
	if(Last == XACME_AWS_BUSY) return XACME_AWS_FAILED;
	xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
		XACME_DNS_ERROR_PROTOCOL,
		"acme dns_aws TXT record set changed repeatedly");
	return XACME_AWS_FAILED;
}

/* Both entry points validate the bounded ASCII owner before copying it. */
static xstrview xacmeAwsCanonicalOwner(xstrview Fqdn, char* sOut)
{
	size_t i;
	for(i = 0u; i < Fqdn.Size; i++) {
		unsigned char c = (unsigned char)Fqdn.Data[i];
		sOut[i] = (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
	}
	sOut[Fqdn.Size] = '\0';
	return (xstrview){ sOut, Fqdn.Size };
}

static bool xacmeAwsAddLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsawscontext* pCtx = (xacmednsawscontext*)pProvider->pContext;
	char sFqdnText[256];
	char sTxtText[208];
	char sZoneId[64];
	size_t iSlot;
	size_t i;
	bool bExistingOwned = false;
	xacmeawsmutationresult Result;
	if(!xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme DNS-01 owner or digest is invalid");
		return false;
	}
	sFqdn = xacmeAwsCanonicalOwner(sFqdn, sFqdnText);
	for(i = 0u; i < pCtx->iRecordCount; i++) {
		if(strlen(pCtx->Records[i].sFqdn) == sFqdn.Size &&
			memcmp(pCtx->Records[i].sFqdn, sFqdn.Data, sFqdn.Size) == 0 &&
			strlen(pCtx->Records[i].sValue) == sTxt.Size &&
			memcmp(pCtx->Records[i].sValue, sTxt.Data, sTxt.Size) == 0) {
			if(pCtx->Records[i].bUncertain) return xacmeAwsUncertain();
			bExistingOwned = true;
			break;
		}
	}
	for(iSlot = 0u; iSlot < pCtx->iRecordCount; iSlot++)
		if(pCtx->Records[iSlot].sFqdn[0] == '\0') break;
	if(bExistingOwned) iSlot = i;
	if((iSlot == pCtx->iRecordCount) &&
		(pCtx->iRecordCount >= XACME_DNS_RECORD_MAX))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_PROTOCOL,
			"acme dns_aws record slots exhausted");
		return false;
	}
	memcpy(sTxtText, sTxt.Data, sTxt.Size);
	sTxtText[sTxt.Size] = '\0';

	/* Keep the cleanup handle bound to its original zone even if a more
	 * specific zone appears between Add calls. New pairs discover afresh. */
	if(bExistingOwned)
	{
		snprintf(sZoneId, sizeof(sZoneId), "%s", pCtx->Records[i].sZoneId);
	}
	else if(!xacmeAwsFindZone(pCtx, sFqdnText, sZoneId, sizeof(sZoneId)))
	{
		if(xrtErrorKind(xrtGetError()) == XERR_NONE)
			xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
				XACME_DNS_ERROR_ZONE,
				"acme dns_aws hosted zone was not found");
		return false;
	}
	Result = xacmeAwsMutateTxt(pCtx, sZoneId, sFqdnText, sTxtText, true);
	if(Result == XACME_AWS_FAILED)
	{
		return false;
	}
	/* A value already present before our acknowledged write is not ours. A
	 * repeated Add on an owned value keeps the original ownership entry. */
	if(Result == XACME_AWS_UNCHANGED) return true;
	snprintf(pCtx->Records[iSlot].sZoneId,
		sizeof(pCtx->Records[iSlot].sZoneId), "%s",
		sZoneId);
	snprintf(pCtx->Records[iSlot].sFqdn,
		sizeof(pCtx->Records[iSlot].sFqdn), "%s",
		sFqdnText);
	snprintf(pCtx->Records[iSlot].sValue,
		sizeof(pCtx->Records[iSlot].sValue), "%s",
		sTxtText);
	if(iSlot == pCtx->iRecordCount) pCtx->iRecordCount++;
	pCtx->Records[iSlot].bUncertain = Result == XACME_AWS_UNCERTAIN;
	if(pCtx->Records[iSlot].bUncertain) return xacmeAwsUncertain();
	return true;
}

static bool xacmeAwsRemoveLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsawscontext* pCtx = (xacmednsawscontext*)pProvider->pContext;
	char sFqdnText[256];
	size_t i;
	if(!xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme DNS-01 owner or digest is invalid");
		return false;
	}
	sFqdn = xacmeAwsCanonicalOwner(sFqdn, sFqdnText);
	for(i = 0; i < pCtx->iRecordCount; i++)
	{
		if(pCtx->Records[i].sFqdn[0] == '\0')
		{
			continue;
		}
		if((strlen(pCtx->Records[i].sFqdn) != sFqdn.Size) ||
			(memcmp(pCtx->Records[i].sFqdn, sFqdn.Data, sFqdn.Size) != 0) ||
			(strlen(pCtx->Records[i].sValue) != sTxt.Size) ||
			(memcmp(pCtx->Records[i].sValue, sTxt.Data, sTxt.Size) != 0))
			continue;
		if(pCtx->Records[i].bUncertain) return xacmeAwsUncertain();
		if(xacmeAwsMutateTxt(pCtx, pCtx->Records[i].sZoneId,
				pCtx->Records[i].sFqdn, pCtx->Records[i].sValue, false) == XACME_AWS_FAILED) return false;
		pCtx->Records[i].sFqdn[0] = '\0';
	}
	return true;
}

static bool xacmeAwsAdd(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsawscontext* pCtx = (xacmednsawscontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeAwsAddLocked(pProvider, sFqdn, sTxt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

static bool xacmeAwsRemove(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsawscontext* pCtx = (xacmednsawscontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeAwsRemoveLocked(pProvider, sFqdn, sTxt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
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
	if(strlen(pConfig->sAccessKeyId) >= sizeof(pCtx->sId) ||
		strlen(pConfig->sSecretAccessKey) >= sizeof(pCtx->sKey) ||
		((pConfig->sRegion != NULL) &&
		 strlen(pConfig->sRegion) >= sizeof(pCtx->sRegion)) ||
		((pConfig->sEndpoint != NULL) &&
		 strlen(pConfig->sEndpoint) >= sizeof(pCtx->sEndpoint)))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_aws credentials, region or endpoint exceed capacity");
		return false;
	}
	pCtx = (xacmednsawscontext*)xrtCalloc(1, sizeof(*pCtx));
	if(pCtx == NULL)
	{
		return false;
	}
	if(!xrtMutexInit(&pCtx->Lock))
	{
		xrtFree(pCtx);
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
		if(!xacmeHttpUnit(&pCtx->Http)) return;
		(void)xrtMutexUnit(&pCtx->Lock);
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
