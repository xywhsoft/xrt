#ifndef XACME_DNSCOMMON_H
#define XACME_DNSCOMMON_H

/*
	DNS provider 公共助手：挑战名称校验、记录句柄登记、
	JSON 文本追加/取值。全部 static 实现，供各家 provider 内部复用，
	不进入公开 API。
*/

#include <xrt/core.h>
#include <xrt/acme_dns.h>
#include <xrt/error.h>

#include <xrt/buffer.h>
#include <xrt/json.h>
#include <xrt/memory.h>
#include <xrt/value.h>

#include <stdio.h>
#include <string.h>

#define XACME_DNS_RECORD_MAX 8u
#define XACME_DNS_RECORD_TEXT_CAP 320u

#if defined(XACME_FEATURE_ACME_DNS) && defined(XACME_FEATURE_ACME_HTTP)
#include "xacme_http.h"

/* Unit is serialized by the host. Check the context before touching its lock. */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static ptr xacmeDnsProviderContext(const xacmednsprovider* pProvider)
{
	if(pProvider == NULL)
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_ARGUMENT,
			"acme DNS callback requires a provider");
		return NULL;
	}
	if(pProvider->pContext == NULL)
	{
		xrtSetErrorInfo(XERR_STATE, "xrt.acme.dns", XACME_DNS_ERROR_STATE,
			"acme DNS provider context has been released");
		return NULL;
	}
	return pProvider->pContext;
}

/* A delivered HTTP transport owns all three resources. Unit revokes the
 * resolver and verifier even when engine retirement must be retried. Check
 * this under the provider lock, before reuse, reservations or empty Remove. */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsProviderReady(const xacmehttp* pHttp)
{
	if(pHttp->pEngine != NULL && pHttp->pResolver != NULL && pHttp->pVerifier != NULL)
		return true;
	xrtSetErrorInfo(XERR_STATE, "xrt.acme.dns", XACME_DNS_ERROR_STATE,
		"acme DNS provider cleanup must finish before the instance can be released");
	return false;
}
#endif

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsHttpsUrl(
	char* sOutput, size_t iCapacity, cstr sEndpoint, cstr sPathAndQuery)
{
	int iWritten;
	if((sOutput == NULL) || (iCapacity == 0u) ||
		(sEndpoint == NULL) || (sPathAndQuery == NULL))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT, "DNS HTTPS URL input invalid");
		return false;
	}
	iWritten = snprintf(sOutput, iCapacity, "https://%s%s",
		sEndpoint, sPathAndQuery);
	if((iWritten < 0) || ((size_t)iWritten >= iCapacity))
	{
		sOutput[0] = '\0';
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT, "DNS HTTPS URL too long");
		return false;
	}
	return true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsChallengeValid(xstrview sFqdn, xstrview sTxt)
{
	size_t i;
	bool bDot = false;
	if((sFqdn.Data == NULL) || (sTxt.Data == NULL) ||
		(sFqdn.Size == 0u) || (sFqdn.Size >= 256u) ||
		(sTxt.Size == 0u) || (sTxt.Size > 200u))
		return false;
	for(i = 0u; i < sFqdn.Size; i++)
	{
		unsigned char c = (unsigned char)sFqdn.Data[i];
		if(c == '.')
		{
			if((i == 0u) || (i + 1u == sFqdn.Size) ||
				(sFqdn.Data[i - 1u] == '.'))
				return false;
			bDot = true;
		}
		else if(!((c >= 'A' && c <= 'Z') ||
			(c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
			(c == '-') || (c == '_')))
			return false;
	}
	if(!bDot)
		return false;
	for(i = 0u; i < sTxt.Size; i++)
	{
		unsigned char c = (unsigned char)sTxt.Data[i];
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || (c == '-') || (c == '_')))
			return false;
	}
	return true;
}

/* 本 provider 生命周期内添加的记录句柄及其 DNS-01 属主/值。 */
typedef struct xacmednsrecords {
	char sIds[XACME_DNS_RECORD_MAX][XACME_DNS_RECORD_TEXT_CAP];
	char sFqdns[XACME_DNS_RECORD_MAX][256];
	char sTxts[XACME_DNS_RECORD_MAX][201];
	size_t iCount;
} xacmednsrecords;

typedef enum xacmednsownedresult {
	XACME_DNS_OWNED_NONE,
	XACME_DNS_OWNED_VALID,
	XACME_DNS_OWNED_ERROR
} xacmednsownedresult;

/* Only a saved, confirmed ID can establish local ownership; matching text alone cannot. */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static size_t xacmeDnsRecordFindOwned(const xacmednsrecords* pRecords,
	xstrview Owner, xstrview Txt)
{
	size_t i;
	for(i = 0u; i < pRecords->iCount; i++)
		if(pRecords->sIds[i][0] != '\0' &&
			strlen(pRecords->sFqdns[i]) == Owner.Size &&
			memcmp(pRecords->sFqdns[i], Owner.Data, Owner.Size) == 0 &&
			strlen(pRecords->sTxts[i]) == Txt.Size &&
			memcmp(pRecords->sTxts[i], Txt.Data, Txt.Size) == 0) return i;
	return XACME_DNS_RECORD_MAX;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsRecordCanAdd(const xacmednsrecords* pRecords)
{
	size_t i;
	for(i = 0u; i < pRecords->iCount; i++)
		if(pRecords->sIds[i][0] == '\0')
			return true;
	return pRecords->iCount < XACME_DNS_RECORD_MAX;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsRecordRemember(
	xacmednsrecords* pRecords, cstr sId, xstrview sFqdn, xstrview sTxt)
{
	size_t iSize = strlen(sId);
	size_t i;
	if(!xacmeDnsRecordCanAdd(pRecords) ||
		(iSize == 0u) || (iSize >= XACME_DNS_RECORD_TEXT_CAP) ||
		!xacmeDnsChallengeValid(sFqdn, sTxt))
		return false;
	for(i = 0u; i < pRecords->iCount; i++)
		if(pRecords->sIds[i][0] == '\0')
			break;
	if(i == pRecords->iCount)
		pRecords->iCount++;
	memcpy(pRecords->sIds[i], sId, iSize + 1u);
	memcpy(pRecords->sFqdns[i], sFqdn.Data, sFqdn.Size);
	pRecords->sFqdns[i][sFqdn.Size] = '\0';
	memcpy(pRecords->sTxts[i], sTxt.Data, sTxt.Size);
	pRecords->sTxts[i][sTxt.Size] = '\0';
	return true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsRecordRememberPair(xacmednsrecords* pRecords,
	cstr sLeft, char cSeparator, cstr sRight,
	xstrview sFqdn, xstrview sTxt)
{
	char sHandle[XACME_DNS_RECORD_TEXT_CAP];
	size_t iLeft = strlen(sLeft);
	size_t iRight = strlen(sRight);
	if((iLeft == 0u) || (iRight == 0u) ||
		(strchr(sLeft, cSeparator) != NULL) ||
		(strchr(sRight, cSeparator) != NULL) ||
		(iRight >= sizeof(sHandle) - 1u) ||
		(iLeft >= sizeof(sHandle) - iRight - 1u))
		return false;
	memcpy(sHandle, sLeft, iLeft);
	sHandle[iLeft] = cSeparator;
	memcpy(sHandle + iLeft + 1u, sRight, iRight + 1u);
	return xacmeDnsRecordRemember(pRecords, sHandle, sFqdn, sTxt);
}

typedef bool (*xacmednsrecorddeleteproc)(void* pContext, cstr sId,
	xstrview sFqdn, xstrview sTxt);

/* A create reservation lives independently of fallible error/JSON allocation.
 * Providers call these helpers while holding their context mutex. */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsCreateUncertainError(void)
{
	xerror* pError;
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	pError = xrtErrorWrap(xrtGetError(), XERR_IO, "xrt.acme.dns",
		XACME_DNS_ERROR_UNCERTAIN, "DNS create outcome and ownership are unknown");
	if(pError != NULL) xrtSetErrorTake(pError);
	return false;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static xstrview xacmeDnsCanonicalOwner(xstrview Owner, char sOut[256])
{
	size_t i;
	for(i = 0u; i < Owner.Size; i++) {
		char c = Owner.Data[i];
		sOut[i] = (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
	}
	sOut[Owner.Size] = '\0';
	return (xstrview){ sOut, Owner.Size };
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsCreateBlocked(const xacmednsrecords* pRecords,
	const bool* pUncertain, xstrview Owner, xstrview Txt)
{
	size_t i;
	for(i = 0u; i < pRecords->iCount; i++)
		if(pUncertain[i] && strlen(pRecords->sFqdns[i]) == Owner.Size &&
			memcmp(pRecords->sFqdns[i], Owner.Data, Owner.Size) == 0 &&
			strlen(pRecords->sTxts[i]) == Txt.Size &&
			memcmp(pRecords->sTxts[i], Txt.Data, Txt.Size) == 0) return true;
	return false;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static size_t xacmeDnsCreateSlot(const xacmednsrecords* pRecords,
	const bool* pUncertain)
{
	size_t i;
	for(i = 0u; i < pRecords->iCount; i++)
		if(!pUncertain[i] && pRecords->sIds[i][0] == '\0') return i;
	return pRecords->iCount;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static void xacmeDnsCreateReserve(xacmednsrecords* pRecords, bool* pUncertain,
	size_t iSlot, xstrview Owner, xstrview Txt)
{
	if(iSlot == pRecords->iCount) pRecords->iCount++;
	pRecords->sIds[iSlot][0] = '\0';
	memcpy(pRecords->sFqdns[iSlot], Owner.Data, Owner.Size);
	pRecords->sFqdns[iSlot][Owner.Size] = '\0';
	memcpy(pRecords->sTxts[iSlot], Txt.Data, Txt.Size);
	pRecords->sTxts[iSlot][Txt.Size] = '\0';
	pUncertain[iSlot] = true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static void xacmeDnsCreateCancel(xacmednsrecords* pRecords, bool* pUncertain,
	size_t iSlot)
{
	pRecords->sIds[iSlot][0] = '\0';
	pRecords->sFqdns[iSlot][0] = '\0';
	pRecords->sTxts[iSlot][0] = '\0';
	pUncertain[iSlot] = false;
	while(pRecords->iCount != 0u &&
		!pUncertain[pRecords->iCount - 1u] &&
		pRecords->sIds[pRecords->iCount - 1u][0] == '\0') pRecords->iCount--;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsCreateCommit(xacmednsrecords* pRecords, bool* pUncertain,
	size_t iSlot, cstr sLeft, char Separator, cstr sRight)
{
	char sHandle[XACME_DNS_RECORD_TEXT_CAP];
	size_t iLeft = strlen(sLeft), iRight = strlen(sRight), i;
	if(iLeft == 0u || iRight == 0u || strchr(sLeft, Separator) != NULL ||
		strchr(sRight, Separator) != NULL || iRight >= sizeof(sHandle) - 1u ||
		iLeft >= sizeof(sHandle) - iRight - 1u) return false;
	memcpy(sHandle, sLeft, iLeft); sHandle[iLeft] = Separator;
	memcpy(sHandle + iLeft + 1u, sRight, iRight + 1u);
	/* Reusing an owned ID is not proof that this create is ours. */
	for(i = 0u; i < pRecords->iCount; i++)
		if(i != iSlot && strcmp(pRecords->sIds[i], sHandle) == 0) return false;
	memcpy(pRecords->sIds[iSlot], sHandle, iLeft + iRight + 2u);
	pUncertain[iSlot] = false;
	return true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsRecordRemoveMatching(xacmednsrecords* pRecords,
	xstrview sFqdn, xstrview sTxt, xacmednsrecorddeleteproc Delete,
	void* pContext)
{
	size_t i;
	if((Delete == NULL) || !xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme DNS-01 owner or digest is invalid");
		return false;
	}
	for(i = 0u; i < pRecords->iCount; i++)
	{
		if((pRecords->sIds[i][0] == '\0') ||
			(strlen(pRecords->sFqdns[i]) != sFqdn.Size) ||
			(memcmp(pRecords->sFqdns[i], sFqdn.Data, sFqdn.Size) != 0) ||
			(strlen(pRecords->sTxts[i]) != sTxt.Size) ||
			(memcmp(pRecords->sTxts[i], sTxt.Data, sTxt.Size) != 0))
			continue;
		if(!Delete(pContext, pRecords->sIds[i], sFqdn, sTxt))
		{
			if(xrtErrorKind(xrtGetError()) == XERR_NONE)
				xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
					XACME_DNS_ERROR_PROTOCOL,
					"acme DNS-01 record deletion failed");
			return false;
		}
		pRecords->sIds[i][0] = '\0';
		pRecords->sFqdns[i][0] = '\0';
		pRecords->sTxts[i][0] = '\0';
	}
	return true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsRecordSplit(cstr sHandle, char cSeparator,
	char* sLeft, size_t iLeftCap, char* sRight, size_t iRightCap)
{
	const char* sSeparator = strchr(sHandle, cSeparator);
	size_t iLeft;
	size_t iRight;
	if((sSeparator == NULL) || (sSeparator == sHandle) ||
		(sSeparator[1] == '\0') ||
		(strchr(sSeparator + 1u, cSeparator) != NULL))
		return false;
	iLeft = (size_t)(sSeparator - sHandle);
	iRight = strlen(sSeparator + 1u);
	if((iLeft >= iLeftCap) || (iRight >= iRightCap))
		return false;
	memcpy(sLeft, sHandle, iLeft);
	sLeft[iLeft] = '\0';
	memcpy(sRight, sSeparator + 1u, iRight + 1u);
	return true;
}

/* 把借用文本按 JSON 字符串 token（含引号）转义追加。 */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsJsonQuote(xbuffer* pOut, xstrview sText)
{
	size_t i;
	if(!xrtBufferAppendByte(pOut, (uint8)'"'))
	{
		return false;
	}
	for(i = 0; i < sText.Size; i++)
	{
		char c = sText.Data[i];
		bool bOk;
		if((c == '"') || (c == '\\'))
		{
			bOk = xrtBufferAppendByte(pOut, (uint8)'\\') &&
				xrtBufferAppendByte(pOut, (uint8)c);
		}
		else
		{
			/* 域名与 base64url 值不含控制字符；其余原样透传。 */
			bOk = xrtBufferAppendByte(pOut, (uint8)c);
		}
		if(!bOk)
		{
			return false;
		}
	}
	return xrtBufferAppendByte(pOut, (uint8)'"');
}

/* 取 JSON 对象字符串成员到固定缓冲（含末尾零）；失败返回 false。 */
#if defined(XRT_FEATURE_VALUE_CONTAINER)
#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsJsonText(
	const xvalue* pObject, cstr sKey, char* sOut, size_t iCapacity)
{
	xvalue* pMember;
	xstrview Text;
	size_t i;
	if((pObject == NULL) || !xrtValueIs(pObject, XVALUE_OBJECT) ||
		(sKey == NULL) || (sOut == NULL) || (iCapacity == 0u))
		return false;
	sOut[0] = '\0';
	pMember = xrtValueObjectGet(
		pObject, (xstrview){ sKey, strlen(sKey) });
	if((pMember == NULL) || !xrtValueGetString(pMember, &Text) ||
		(Text.Size >= iCapacity) ||
		((Text.Size != 0u) && (Text.Data == NULL)))
	{
		return false;
	}
	/* 下游使用 C 字符串；不能让 JSON 的转义 NUL 改变比较或路径。 */
	for(i = 0u; i < Text.Size; i++)
	{
		unsigned char c = (unsigned char)Text.Data[i];
		if((c < 0x20u) || (c == 0x7fu)) return false;
	}
	if(Text.Size != 0u) memcpy(sOut, Text.Data, Text.Size);
	sOut[Text.Size] = '\0';
	return true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsJsonPathId(
	const xvalue* pObject, cstr sKey, char* sOut, size_t iCapacity)
{
	size_t i;
	if(!xacmeDnsJsonText(pObject, sKey, sOut, iCapacity)) return false;
	if(sOut[0] == '\0') return false;
	if((strcmp(sOut, ".") == 0) || (strcmp(sOut, "..") == 0))
	{
		sOut[0] = '\0';
		return false;
	}
	for(i = 0u; sOut[i] != '\0'; i++)
	{
		unsigned char c = (unsigned char)sOut[i];
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || (c == '-') || (c == '_') ||
			(c == '.') || (c == '~')))
		{
			sOut[0] = '\0';
			return false;
		}
	}
	return true;
}

typedef enum xacmednszoneresult {
	XACME_DNS_ZONE_ERROR = -1,
	XACME_DNS_ZONE_MISSING = 0,
	XACME_DNS_ZONE_FOUND = 1
} xacmednszoneresult;

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsJsonEqual(const xvalue* pObject, cstr sKey, cstr sWanted)
{
	char sText[512];
	return xacmeDnsJsonText(pObject, sKey, sText, sizeof(sText)) &&
		strcmp(sText, sWanted) == 0;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsJsonSuccess(const xvalue* pRoot)
{
	bool bSuccess = false;
	xvalue* pSuccess = (pRoot != NULL &&
		xrtValueIs(pRoot, XVALUE_OBJECT)) ?
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("success")) : NULL;
	return (pSuccess != NULL) && xrtValueGetBool(pSuccess, &bSuccess) &&
		bSuccess;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static xacmednszoneresult xacmeDnsJsonZoneId(
	xstrview Body, cstr sArrayKey, cstr sWantedName, bool bRequireSuccess,
	size_t iPageLimit,
	char* sOutId, size_t iIdCapacity)
{
	xvalue* pRoot = (Body.Data != NULL) ? xrtJsonParse(Body) : NULL;
	xvalue* pZones = (pRoot != NULL && xrtValueIs(pRoot, XVALUE_OBJECT)) ?
		xrtValueObjectGet(pRoot, (xstrview){ sArrayKey, strlen(sArrayKey) }) :
		NULL;
	xacmednszoneresult Result = XACME_DNS_ZONE_MISSING;
	size_t i;
	/* A parser allocation failure is the first cause, not a zone miss or a
	 * schema error. Successful parsing must not preserve an unrelated old OOM. */
	if(pRoot == NULL && Body.Data != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_MEMORY)
	{
		if(sOutId != NULL && iIdCapacity != 0u) sOutId[0] = '\0';
		return XACME_DNS_ZONE_ERROR;
	}
	if((bRequireSuccess && !xacmeDnsJsonSuccess(pRoot)) ||
		(pZones == NULL) || !xrtValueIs(pZones, XVALUE_ARRAY))
	{
		Result = XACME_DNS_ZONE_ERROR;
		goto Done;
	}
	/* 页已填满时可能还有未读取的同名 zone，不能选择首项。 */
	if((iPageLimit != 0u) && (xrtValueCount(pZones) >= iPageLimit))
	{
		Result = XACME_DNS_ZONE_ERROR;
		goto Done;
	}
	for(i = 0u; i < xrtValueCount(pZones); i++)
	{
		xvalue* pItem = xrtValueArrayGet(pZones, i);
		char sName[280];
		if((pItem == NULL) || !xrtValueIs(pItem, XVALUE_OBJECT) ||
			!xacmeDnsJsonText(pItem, "name", sName, sizeof(sName)))
		{
			Result = XACME_DNS_ZONE_ERROR;
			break;
		}
		if(strcmp(sName, sWantedName) == 0)
		{
			if(Result == XACME_DNS_ZONE_FOUND)
			{
				Result = XACME_DNS_ZONE_ERROR;
				break;
			}
			Result = xacmeDnsJsonPathId(pItem, "id", sOutId,
				iIdCapacity) ? XACME_DNS_ZONE_FOUND : XACME_DNS_ZONE_ERROR;
			if(Result == XACME_DNS_ZONE_ERROR) break;
		}
	}
Done:
	if((sOutId != NULL) && (iIdCapacity != 0u) &&
		(Result != XACME_DNS_ZONE_FOUND)) sOutId[0] = '\0';
	xrtValueRelease(pRoot);
	if(Result == XACME_DNS_ZONE_ERROR)
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
			"acme DNS managed zone response invalid");
	return Result;
}
#endif

#endif
