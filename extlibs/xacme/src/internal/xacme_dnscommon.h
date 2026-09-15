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
	for(i = 0; i < pCache->iCount; i++)
	{
		size_t iZoneLen = strlen(pCache->sZones[i]);
		if((iLen > iZoneLen + 1u) &&
			(sFqdn[iLen - iZoneLen - 1u] == '.') &&
			(strcmp(sFqdn + iLen - iZoneLen, pCache->sZones[i]) == 0))
		{
			return pCache->sZones[i];
		}
	}
	return NULL;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static void xacmeDnsZoneRemember(xacmednszonecache* pCache, cstr sZone)
{
	if((pCache->iCount < XACME_DNS_ZONE_MAX) &&
		(xacmeDnsZoneMatch(pCache, sZone) == NULL))
	{
		snprintf(pCache->sZones[pCache->iCount],
			sizeof(pCache->sZones[pCache->iCount]), "%s", sZone);
		pCache->iCount++;
	}
}

/* 本 provider 生命周期内添加的记录句柄（RecordId 或 FQDN 值对）。 */
typedef struct xacmednsrecords {
	char sIds[XACME_DNS_RECORD_MAX][64];
	size_t iCount;
} xacmednsrecords;

#if defined(__GNUC__)
__attribute__((unused))
#endif
static void xacmeDnsRecordRemember(
	xacmednsrecords* pRecords, cstr sId)
{
	if((pRecords->iCount < XACME_DNS_RECORD_MAX) && (strlen(sId) < 64u))
	{
		strcpy(pRecords->sIds[pRecords->iCount], sId);
		pRecords->iCount++;
	}
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
