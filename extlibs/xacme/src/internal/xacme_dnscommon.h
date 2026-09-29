#ifndef XACME_DNSCOMMON_H
#define XACME_DNSCOMMON_H

/*
	DNS provider 公共助手：FQDN 拆分、多 zone 缓存、记录句柄登记、
	JSON 文本追加/取值。全部 static 实现，供各家 provider 内部复用，
	不进入公开 API。
*/

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/buffer.h>
#include <xrt/json.h>
#include <xrt/memory.h>
#include <xrt/value.h>

#include <stdio.h>
#include <string.h>

#define XACME_DNS_ZONE_MAX 4u
#define XACME_DNS_RECORD_MAX 8u
#define XACME_DNS_RECORD_TEXT_CAP 320u

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

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsSplit(
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

/* 多 zone 缓存：后缀命中返回借用指针，未命中返回 NULL。 */
typedef struct xacmednszonecache {
	char sZones[XACME_DNS_ZONE_MAX][256];
	size_t iCount;
} xacmednszonecache;

#if defined(__GNUC__)
__attribute__((unused))
#endif
static const char* xacmeDnsZoneMatch(
	xacmednszonecache* pCache, cstr sFqdn)
{
	size_t i;
	size_t iLen = strlen(sFqdn);
	const char* sBest = NULL;
	size_t iBestLen = 0u;
	for(i = 0; i < pCache->iCount; i++)
	{
		size_t iZoneLen = strlen(pCache->sZones[i]);
		if((iZoneLen > iBestLen) && (iLen > iZoneLen + 1u) &&
			(sFqdn[iLen - iZoneLen - 1u] == '.') &&
			(strcmp(sFqdn + iLen - iZoneLen, pCache->sZones[i]) == 0))
		{
			sBest = pCache->sZones[i];
			iBestLen = iZoneLen;
		}
	}
	return sBest;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static void xacmeDnsZoneRemember(xacmednszonecache* pCache, cstr sZone)
{
	size_t i;
	for(i = 0u; i < pCache->iCount; i++)
		if(strcmp(pCache->sZones[i], sZone) == 0) return;
	if(pCache->iCount < XACME_DNS_ZONE_MAX)
	{
		snprintf(pCache->sZones[pCache->iCount],
			sizeof(pCache->sZones[pCache->iCount]), "%s", sZone);
		pCache->iCount++;
	}
}

/* 本 provider 生命周期内添加的记录句柄及其 DNS-01 属主/值。 */
typedef struct xacmednsrecords {
	char sIds[XACME_DNS_RECORD_MAX][XACME_DNS_RECORD_TEXT_CAP];
	char sFqdns[XACME_DNS_RECORD_MAX][256];
	char sTxts[XACME_DNS_RECORD_MAX][201];
	size_t iCount;
} xacmednsrecords;

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

typedef bool (*xacmednsrecorddeleteproc)(void* pContext, cstr sId);

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
		if(!Delete(pContext, pRecords->sIds[i]))
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
	xvalue* pMember = xrtValueObjectGet(
		pObject, (xstrview){ sKey, strlen(sKey) });
	xstrview Text;
	if((pMember == NULL) || !xrtValueGetString(pMember, &Text) ||
		(Text.Size >= iCapacity))
	{
		return false;
	}
	memcpy(sOut, Text.Data, Text.Size);
	sOut[Text.Size] = '\0';
	return true;
}
#endif

#endif
