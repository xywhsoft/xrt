#include "../internal/xacme_dns_aws_internal.h"

#if defined(XACME_FEATURE_DNS_AWS)

#include <string.h>

#define XACME_AWS_XML_MAX (4u * 1024u * 1024u)
#define XACME_AWS_CHANGE_XML_MAX 262144u
#define XACME_AWS_CHANGE_VALUE_MAX 32000u

static void xacmeAwsSkipSpace(const char** pp, const char* pEnd)
{
	while((*pp < pEnd) && ((**pp == ' ') || (**pp == '\t') ||
		(**pp == '\r') || (**pp == '\n')))
		(*pp)++;
}

static bool xacmeAwsConsume(const char** pp, const char* pEnd, cstr sText)
{
	size_t iSize = strlen(sText);
	xacmeAwsSkipSpace(pp, pEnd);
	if(((size_t)(pEnd - *pp) < iSize) ||
		(memcmp(*pp, sText, iSize) != 0))
		return false;
	*pp += iSize;
	return true;
}

static bool xacmeAwsTake(const char** pp, const char* pEnd,
	cstr sOpen, cstr sClose, xstrview* pText)
{
	const char* pClose;
	if(!xacmeAwsConsume(pp, pEnd, sOpen))
		return false;
	pClose = strstr(*pp, sClose);
	if((pClose == NULL) || (pClose > pEnd) ||
		(memchr(*pp, '<', (size_t)(pClose - *pp)) != NULL))
		return false;
	*pText = (xstrview){ *pp, (size_t)(pClose - *pp) };
	*pp = pClose + strlen(sClose);
	return true;
}

static bool xacmeAwsNameEqual(xstrview Name, cstr sFqdn)
{
	size_t i;
	size_t iLen = strlen(sFqdn);
	if((Name.Size == iLen + 1u) && (Name.Data[iLen] == '.'))
		Name.Size--;
	if(Name.Size != iLen)
		return false;
	for(i = 0u; i < iLen; i++)
	{
		char a = Name.Data[i];
		char b = sFqdn[i];
		if((a >= 'A') && (a <= 'Z')) a = (char)(a + ('a' - 'A'));
		if((b >= 'A') && (b <= 'Z')) b = (char)(b + ('a' - 'A'));
		if(a != b) return false;
	}
	return true;
}

static bool xacmeAwsTagText(const char* pBegin, const char* pEnd,
	cstr sOpen, cstr sClose, xstrview* pText)
{
	const char* p = strstr(pBegin, sOpen);
	const char* pClose;
	if((p == NULL) || (p >= pEnd)) return false;
	p += strlen(sOpen);
	pClose = strstr(p, sClose);
	if((pClose == NULL) || (pClose >= pEnd) ||
		(memchr(p, '<', (size_t)(pClose - p)) != NULL)) return false;
	*pText = (xstrview){ p, (size_t)(pClose - p) };
	return true;
}

int xacmeAwsSelectPublicZone(cstr sXml, cstr sZone,
	char* sOutId, size_t iIdCap)
{
	const char* p;
	const char* pEnd;
	xstrview Truncated;
	bool bFound = false;
	if((sXml == NULL) || (sZone == NULL) || (sOutId == NULL) ||
		(iIdCap == 0u) || (strlen(sXml) > XACME_AWS_XML_MAX) ||
		(strstr(sXml, "<ListHostedZonesByNameResponse") == NULL) ||
		(strstr(sXml, "</ListHostedZonesByNameResponse>") == NULL))
		return -1;
	sOutId[0] = '\0';
	p = strstr(sXml, "<HostedZones");
	if(p == NULL) return -1;
	if(strncmp(p, "<HostedZones/>", strlen("<HostedZones/>")) == 0)
	{
		p += strlen("<HostedZones/>");
		pEnd = p;
	}
	else
	{
		if(strncmp(p, "<HostedZones>", strlen("<HostedZones>")) != 0)
			return -1;
		p += strlen("<HostedZones>");
		pEnd = strstr(p, "</HostedZones>");
		if(pEnd == NULL) return -1;
	}
	while(p < pEnd)
	{
		const char* pBlock;
		const char* pBlockEnd;
		xstrview Name;
		xstrview Private;
		xstrview Id;
		size_t iIdLen;
		while((p < pEnd) && ((*p == ' ') || (*p == '\t') ||
			(*p == '\r') || (*p == '\n'))) p++;
		if(p == pEnd) break;
		if(((size_t)(pEnd - p) < strlen("<HostedZone>")) ||
			(memcmp(p, "<HostedZone>", strlen("<HostedZone>")) != 0))
			return -1;
		pBlock = p + strlen("<HostedZone>");
		pBlockEnd = strstr(pBlock, "</HostedZone>");
		if((pBlockEnd == NULL) || (pBlockEnd > pEnd) ||
			!xacmeAwsTagText(pBlock, pBlockEnd,
				"<Name>", "</Name>", &Name)) return -1;
		if(xacmeAwsNameEqual(Name, sZone))
		{
			bool bHasPrivate = xacmeAwsTagText(pBlock, pBlockEnd,
				"<PrivateZone>", "</PrivateZone>", &Private);
			const char* pPrivateTag = strstr(pBlock, "<PrivateZone>");
			if(!bHasPrivate && (pPrivateTag != NULL) &&
				(pPrivateTag < pBlockEnd)) return -1;
			if(!bHasPrivate || ((Private.Size == 5u) &&
				(memcmp(Private.Data, "false", 5u) == 0)))
			{
				size_t i;
				if(bFound || !xacmeAwsTagText(pBlock, pBlockEnd,
						"<Id>", "</Id>", &Id)) return -1;
				if((Id.Size > strlen("/hostedzone/")) &&
					(memcmp(Id.Data, "/hostedzone/",
						strlen("/hostedzone/")) == 0))
				{
					/* API 中 Id 常带 /hostedzone/ 前缀。 */
					Id.Data += strlen("/hostedzone/");
					Id.Size -= strlen("/hostedzone/");
				}
				iIdLen = Id.Size;
				if((iIdLen == 0u) || (iIdLen > 32u) ||
					(iIdLen >= iIdCap)) return -1;
				for(i = 0u; i < iIdLen; i++)
					if(!((Id.Data[i] >= 'A' && Id.Data[i] <= 'Z') ||
						(Id.Data[i] >= 'a' && Id.Data[i] <= 'z') ||
						(Id.Data[i] >= '0' && Id.Data[i] <= '9')))
						return -1;
				memcpy(sOutId, Id.Data, iIdLen);
				sOutId[iIdLen] = '\0';
				bFound = true;
			}
			else if(!((Private.Size == 4u) &&
				(memcmp(Private.Data, "true", 4u) == 0))) return -1;
		}
		p = pBlockEnd + strlen("</HostedZone>");
	}
	if(!xacmeAwsTagText(pEnd, sXml + strlen(sXml),
		"<IsTruncated>", "</IsTruncated>", &Truncated)) return -1;
	if((Truncated.Size == 4u) &&
		(memcmp(Truncated.Data, "true", 4u) == 0))
	{
		xstrview NextName;
		if(!xacmeAwsTagText(pEnd, sXml + strlen(sXml),
			"<NextDNSName>", "</NextDNSName>", &NextName)) return -1;
		if(xacmeAwsNameEqual(NextName, sZone)) return -1;
	}
	else if(!((Truncated.Size == 5u) &&
		(memcmp(Truncated.Data, "false", 5u) == 0))) return -1;
	return bFound ? 1 : 0;
}

bool xacmeAwsParseTxtSet(cstr sXml, cstr sFqdn, xacmeawstxtset* pSet)
{
	const char* pRoot;
	const char* pRootEnd;
	const char* p;
	const char* pEnd;
	xstrview Name;
	xstrview Type;
	xstrview Ttl;
	size_t i;
	if((sXml == NULL) || (sFqdn == NULL) || (pSet == NULL) ||
		(strlen(sXml) > XACME_AWS_XML_MAX))
		return false;
	memset(pSet, 0, sizeof(*pSet));
	if((strstr(sXml, "<ListResourceRecordSetsResponse") == NULL) ||
		(strstr(sXml, "</ListResourceRecordSetsResponse>") == NULL))
		return false;
	pRoot = strstr(sXml, "<ResourceRecordSets");
	if(pRoot == NULL) return false;
	if((strncmp(pRoot, "<ResourceRecordSets/>",
			strlen("<ResourceRecordSets/>")) == 0) ||
		(strncmp(pRoot, "<ResourceRecordSets />",
			strlen("<ResourceRecordSets />")) == 0))
		return true;
	if(strncmp(pRoot, "<ResourceRecordSets>",
		strlen("<ResourceRecordSets>")) != 0) return false;
	pRoot += strlen("<ResourceRecordSets>");
	pRootEnd = strstr(pRoot, "</ResourceRecordSets>");
	if(pRootEnd == NULL) return false;
	p = pRoot;
	xacmeAwsSkipSpace(&p, pRootEnd);
	if(p == pRootEnd) return true;
	if(!xacmeAwsConsume(&p, pRootEnd, "<ResourceRecordSet>"))
		return false;
	pEnd = strstr(p, "</ResourceRecordSet>");
	if((pEnd == NULL) || (pEnd > pRootEnd) ||
		!xacmeAwsTake(&p, pEnd, "<Name>", "</Name>", &Name) ||
		!xacmeAwsTake(&p, pEnd, "<Type>", "</Type>", &Type))
		return false;
	if(!xacmeAwsNameEqual(Name, sFqdn) ||
		(Type.Size != 3u) || (memcmp(Type.Data, "TXT", 3u) != 0))
		return true;
	if(!xacmeAwsTake(&p, pEnd, "<TTL>", "</TTL>", &Ttl) ||
		(Ttl.Size == 0u) || (Ttl.Size >= sizeof(pSet->sTtl)))
		return false;
	for(i = 0u; i < Ttl.Size; i++)
		if((Ttl.Data[i] < '0') || (Ttl.Data[i] > '9')) return false;
	memcpy(pSet->sTtl, Ttl.Data, Ttl.Size);
	pSet->sTtl[Ttl.Size] = '\0';
	if(!xacmeAwsConsume(&p, pEnd, "<ResourceRecords>")) return false;
	for(;;)
	{
		xstrview Value;
		xacmeAwsSkipSpace(&p, pEnd);
		if(xacmeAwsConsume(&p, pEnd, "</ResourceRecords>")) break;
		if((pSet->iCount >= XACME_AWS_TXT_MAX_VALUES) ||
			!xacmeAwsConsume(&p, pEnd, "<ResourceRecord>") ||
			!xacmeAwsTake(&p, pEnd, "<Value>", "</Value>", &Value) ||
			(Value.Size == 0u) ||
			!xacmeAwsConsume(&p, pEnd, "</ResourceRecord>"))
			return false;
		pSet->Values[pSet->iCount++] = Value;
	}
	xacmeAwsSkipSpace(&p, pEnd);
	if((p != pEnd) || (pSet->iCount == 0u)) return false;
	pSet->bPresent = true;
	return true;
}

static bool xacmeAwsAppend(xbuffer* pOut, cstr sText)
{
	return xrtBufferAppend(pOut,
		(xbytesview){ (const uint8*)sText, strlen(sText) });
}

static bool xacmeAwsAppendView(xbuffer* pOut, xstrview Text)
{
	return xrtBufferAppend(pOut,
		(xbytesview){ (const uint8*)Text.Data, Text.Size });
}

static size_t xacmeAwsQuoteWidth(const char* pText, size_t iRemaining)
{
	if((iRemaining >= 1u) && (pText[0] == '"')) return 1u;
	if((iRemaining >= 6u) &&
		(memcmp(pText, "&quot;", 6u) == 0)) return 6u;
	if((iRemaining >= 5u) &&
		(memcmp(pText, "&#34;", 5u) == 0)) return 5u;
	if((iRemaining >= 6u) &&
		((memcmp(pText, "&#x22;", 6u) == 0) ||
		 (memcmp(pText, "&#X22;", 6u) == 0))) return 6u;
	return 0u;
}

static bool xacmeAwsValueEqual(xstrview Value, cstr sValue)
{
	size_t iPrefix;
	size_t iSuffix;
	size_t iSize = strlen(sValue);
	iPrefix = xacmeAwsQuoteWidth(Value.Data, Value.Size);
	if((iPrefix == 0u) || (Value.Size < iPrefix + iSize + 1u))
		return false;
	iSuffix = xacmeAwsQuoteWidth(Value.Data + iPrefix + iSize,
		Value.Size - iPrefix - iSize);
	return (iSuffix > 0u) &&
		(Value.Size == iPrefix + iSize + iSuffix) &&
		(memcmp(Value.Data + iPrefix, sValue, iSize) == 0);
}

static bool xacmeAwsAppendSet(xbuffer* pOut, const xacmeawstxtset* pOld,
	cstr sFqdn, cstr sValue, bool bAdd, bool bNew)
{
	size_t i;
	if(!xacmeAwsAppend(pOut, "<ResourceRecordSet><Name>") ||
		!xacmeAwsAppend(pOut, sFqdn) ||
		!xacmeAwsAppend(pOut, ".</Name><Type>TXT</Type><TTL>") ||
		!xacmeAwsAppend(pOut, pOld->bPresent ? pOld->sTtl : "60") ||
		!xacmeAwsAppend(pOut, "</TTL><ResourceRecords>"))
		return false;
	for(i = 0u; i < pOld->iCount; i++)
	{
		if(bNew && !bAdd && xacmeAwsValueEqual(pOld->Values[i], sValue))
			continue;
		if(!xacmeAwsAppend(pOut, "<ResourceRecord><Value>") ||
			!xacmeAwsAppendView(pOut, pOld->Values[i]) ||
			!xacmeAwsAppend(pOut, "</Value></ResourceRecord>"))
			return false;
	}
	if(bNew && bAdd)
	{
		if(!xacmeAwsAppend(pOut, "<ResourceRecord><Value>\"") ||
			!xacmeAwsAppend(pOut, sValue) ||
			!xacmeAwsAppend(pOut, "\"</Value></ResourceRecord>"))
			return false;
	}
	return xacmeAwsAppend(pOut, "</ResourceRecords></ResourceRecordSet>");
}

static bool xacmeAwsAppendChange(xbuffer* pOut, cstr sAction,
	const xacmeawstxtset* pOld, cstr sFqdn, cstr sValue,
	bool bAdd, bool bNew)
{
	return xacmeAwsAppend(pOut, "<Change><Action>") &&
		xacmeAwsAppend(pOut, sAction) &&
		xacmeAwsAppend(pOut, "</Action>") &&
		xacmeAwsAppendSet(pOut, pOld, sFqdn, sValue, bAdd, bNew) &&
		xacmeAwsAppend(pOut, "</Change>");
}

bool xacmeAwsBuildTxtChange(const xacmeawstxtset* pOld,
	cstr sFqdn, cstr sValue, bool bAdd, xbuffer* pOut, bool* pChanged)
{
	size_t i;
	size_t iOldValueBytes = 0u;
	size_t iNewValueBytes = 0u;
	bool bFound = false;
	bool bKeep;
	if((pOld == NULL) || (sFqdn == NULL) || (sValue == NULL) ||
		(pOut == NULL) || (pChanged == NULL)) return false;
	*pChanged = false;
	for(i = 0u; i < pOld->iCount; i++)
	{
		iOldValueBytes += pOld->Values[i].Size;
		if(xacmeAwsValueEqual(pOld->Values[i], sValue)) bFound = true;
		else iNewValueBytes += pOld->Values[i].Size;
	}
	if((bAdd && bFound) || (!bAdd && !bFound)) return true;
	if(bAdd) iNewValueBytes = iOldValueBytes + strlen(sValue) + 2u;
	if((pOld->iCount > XACME_AWS_TXT_MAX_VALUES) ||
		(bAdd && (pOld->iCount == XACME_AWS_TXT_MAX_VALUES)) ||
		(iOldValueBytes > XACME_AWS_CHANGE_VALUE_MAX) ||
		(iNewValueBytes > XACME_AWS_CHANGE_VALUE_MAX - iOldValueBytes))
		return false;
	bKeep = bAdd || (pOld->iCount > 1u);
	xrtBufferClear(pOut);
	if(!xacmeAwsAppend(pOut,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<ChangeResourceRecordSetsRequest xmlns=\"https://route53."
		"amazonaws.com/doc/2013-04-01/\"><ChangeBatch><Changes>"))
		return false;
	if(pOld->bPresent &&
		!xacmeAwsAppendChange(pOut, "DELETE", pOld, sFqdn, sValue,
			bAdd, false)) return false;
	if(bKeep &&
		!xacmeAwsAppendChange(pOut, "CREATE", pOld, sFqdn, sValue,
			bAdd, true)) return false;
	if(!xacmeAwsAppend(pOut,
		"</Changes></ChangeBatch></ChangeResourceRecordSetsRequest>") ||
		(pOut->Size > XACME_AWS_CHANGE_XML_MAX) ||
		!xrtBufferAppendByte(pOut, 0u)) return false;
	*pChanged = true;
	return true;
}

#endif
