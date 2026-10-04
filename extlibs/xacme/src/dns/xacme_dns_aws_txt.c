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

/* Route53's REST-XML error schema. No allocation, DTD or external entities.
 * Only a unique Error/Code in a completely validated envelope can authorize
 * replay. Default namespaces and a consistently bound root prefix are accepted. */
typedef struct xacmeawserrorxml {
	const char* p;
	const char* pEnd;
	xstrview Prefix;
} xacmeawserrorxml;

static bool xacmeAwsXmlAt(const xacmeawserrorxml* pXml, cstr sText)
{
	size_t iSize = strlen(sText);
	return (size_t)(pXml->pEnd - pXml->p) >= iSize &&
		memcmp(pXml->p, sText, iSize) == 0;
}

static bool xacmeAwsXmlChar(uint32 c)
{
	return c == 9u || c == 10u || c == 13u || (c >= 32u && c <= 0xd7ffu) ||
		(c >= 0xe000u && c <= 0xfffdu) || (c >= 0x10000u && c <= 0x10ffffu);
}

/* Validate UTF-8 and XML character references even in ignored messages. */
static bool xacmeAwsXmlCharTake(xacmeawserrorxml* pXml, uint32* pValue, bool bReference)
{
	uint32 c;
	unsigned i, n;
	uint32 iMin;
	if(pXml->p == pXml->pEnd) return false;
	c = (unsigned char)*pXml->p++;
	if(c == '&' && bReference) {
		static const char* Names[] = { "amp;", "lt;", "gt;", "quot;", "apos;" };
		static const uint32 Values[] = { '&', '<', '>', '"', '\'' };
		for(i = 0u; i < 5u; i++) if(xacmeAwsXmlAt(pXml, Names[i])) {
			pXml->p += strlen(Names[i]); *pValue = Values[i]; return true;
		}
		if(!xacmeAwsXmlAt(pXml, "#")) return false;
		pXml->p++; n = 10u; c = 0u; i = 0u;
		if(xacmeAwsXmlAt(pXml, "x")) { n = 16u; pXml->p++; }
		while(pXml->p < pXml->pEnd && *pXml->p != ';') {
			unsigned char b = (unsigned char)*pXml->p++;
			uint32 d = b >= '0' && b <= '9' ? (uint32)(b - '0') :
				n == 16u && b >= 'a' && b <= 'f' ? b - 'a' + 10u :
				n == 16u && b >= 'A' && b <= 'F' ? b - 'A' + 10u : n;
			if(d >= n || c > (0x10ffffu - d) / n) return false;
			c = c * n + d; i++;
		}
		if(i == 0u || pXml->p == pXml->pEnd) return false;
		pXml->p++;
	} else if(c >= 0x80u) {
		if(c >= 0xc2u && c <= 0xdfu) { n = 1u; iMin = 0x80u; c &= 0x1fu; }
		else if(c >= 0xe0u && c <= 0xefu) { n = 2u; iMin = 0x800u; c &= 0x0fu; }
		else if(c >= 0xf0u && c <= 0xf4u) { n = 3u; iMin = 0x10000u; c &= 7u; }
		else return false;
		for(i = 0u; i < n; i++) {
			unsigned char b;
			if(pXml->p == pXml->pEnd) return false;
			b = (unsigned char)*pXml->p++;
			if((b & 0xc0u) != 0x80u) return false;
			c = (c << 6u) | (b & 0x3fu);
		}
		if(c < iMin) return false;
	}
	*pValue = c;
	return xacmeAwsXmlChar(c);
}

static bool xacmeAwsXmlComment(xacmeawserrorxml* pXml)
{
	if(!xacmeAwsXmlAt(pXml, "<!--")) return false;
	pXml->p += 4u;
	while(pXml->p < pXml->pEnd) {
		uint32 c;
		if(xacmeAwsXmlAt(pXml, "-->")) { pXml->p += 3u; return true; }
		if(xacmeAwsXmlAt(pXml, "--")) return false;
		if(!xacmeAwsXmlCharTake(pXml, &c, false)) return false;
	}
	return false;
}

static bool xacmeAwsXmlSpace(xacmeawserrorxml* pXml)
{
	for(;;) {
		xacmeAwsSkipSpace(&pXml->p, pXml->pEnd);
		if(!xacmeAwsXmlAt(pXml, "<!--")) return true;
		if(!xacmeAwsXmlComment(pXml)) return false;
	}
}

static bool xacmeAwsXmlTag(xacmeawserrorxml* pXml, cstr sName,
	bool bClose, bool* pEmpty)
{
	xacmeawserrorxml Next = *pXml;
	if(!xacmeAwsXmlSpace(&Next) || !xacmeAwsXmlAt(&Next, bClose ? "</" : "<")) return false;
	Next.p += bClose ? 2u : 1u;
	if(Next.Prefix.Size != 0u) {
		if((size_t)(Next.pEnd - Next.p) <= Next.Prefix.Size ||
			memcmp(Next.p, Next.Prefix.Data, Next.Prefix.Size) != 0 ||
			Next.p[Next.Prefix.Size] != ':') return false;
		Next.p += Next.Prefix.Size + 1u;
	}
	if(!xacmeAwsXmlAt(&Next, sName)) return false;
	Next.p += strlen(sName);
	xacmeAwsSkipSpace(&Next.p, Next.pEnd);
	if(pEmpty != NULL) *pEmpty = !bClose && xacmeAwsXmlAt(&Next, "/>");
	if(!bClose && pEmpty != NULL && *pEmpty) Next.p++;
	if(!xacmeAwsXmlAt(&Next, ">")) return false;
	Next.p++; *pXml = Next;
	return true;
}

static bool xacmeAwsXmlTextImpl(xacmeawserrorxml* pXml, cstr sName, char* sCode)
{
	bool bEmpty;
	bool bCdata = false;
	size_t iSize = 0u;
	if(!xacmeAwsXmlTag(pXml, sName, false, &bEmpty)) return false;
	if(bEmpty) return sCode == NULL;
	while(pXml->p < pXml->pEnd) {
		uint32 c;
		if(!bCdata && xacmeAwsXmlAt(pXml, "<!--")) {
			if(!xacmeAwsXmlComment(pXml)) return false;
			continue;
		}
		if(!bCdata && xacmeAwsXmlAt(pXml, "<![CDATA[")) { pXml->p += 9u; bCdata = true; continue; }
		if(bCdata && xacmeAwsXmlAt(pXml, "]]>")) { pXml->p += 3u; bCdata = false; continue; }
		if(!bCdata && *pXml->p == '<') break;
		if((!bCdata && xacmeAwsXmlAt(pXml, "]]>")) || !xacmeAwsXmlCharTake(pXml, &c, !bCdata)) return false;
		if(sCode != NULL) {
			if(iSize == 63u || !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == '_')) return false;
			sCode[iSize++] = (char)c;
		}
	}
	if(bCdata) return false;
	if(sCode != NULL) { sCode[iSize] = '\0'; if(iSize == 0u) return false; }
	return xacmeAwsXmlTag(pXml, sName, true, NULL);
}

static bool xacmeAwsXmlText(xacmeawserrorxml* pXml, cstr sName, char* sCode)
{
	xacmeawserrorxml Next = *pXml;
	if(!xacmeAwsXmlTextImpl(&Next, sName, sCode)) return false;
	*pXml = Next;
	return true;
}

bool xacmeAwsErrorCode(xstrview Xml, char sCode[64])
{
	xacmeawserrorxml X;
	char Code[64] = { 0 };
	unsigned iSeen = 0u;
	bool bEmpty;
	const char* pName;
	if(sCode == NULL) return false;
	sCode[0] = '\0';
	if(Xml.Data == NULL || Xml.Size == 0u || Xml.Size > XACME_AWS_XML_MAX) return false;
	X = (xacmeawserrorxml){ Xml.Data, Xml.Data + Xml.Size, { NULL, 0u } };
	if(xacmeAwsXmlAt(&X, "\xef\xbb\xbf")) X.p += 3u;
	/* XML declaration is accepted only in its documented UTF-8 forms. */
	if(xacmeAwsXmlAt(&X, "<?xml")) {
		if(xacmeAwsXmlAt(&X, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>")) X.p += sizeof("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") - 1u;
		else if(xacmeAwsXmlAt(&X, "<?xml version='1.0' encoding='UTF-8'?>")) X.p += sizeof("<?xml version='1.0' encoding='UTF-8'?>") - 1u;
		else if(xacmeAwsXmlAt(&X, "<?xml version=\"1.0\"?>")) X.p += sizeof("<?xml version=\"1.0\"?>") - 1u;
		else return false;
	}
	if(!xacmeAwsXmlSpace(&X) || !xacmeAwsXmlAt(&X, "<")) return false;
	X.p++; pName = X.p;
	while(X.p < X.pEnd && ((*X.p >= 'A' && *X.p <= 'Z') ||
		(*X.p >= 'a' && *X.p <= 'z') || (*X.p >= '0' && *X.p <= '9') ||
		*X.p == '_' || *X.p == '-')) X.p++;
	if(xacmeAwsXmlAt(&X, ":")) {
		if(X.p == pName || X.p - pName > 32 ||
			!((*pName >= 'A' && *pName <= 'Z') || (*pName >= 'a' && *pName <= 'z') || *pName == '_')) return false;
		X.Prefix = (xstrview){ pName, (size_t)(X.p - pName) }; X.p++; pName = X.p;
		if(!xacmeAwsXmlAt(&X, "ErrorResponse")) return false;
		X.p += 13u;
	}
	if((size_t)(X.p - pName) != 13u || memcmp(pName, "ErrorResponse", 13u) != 0) return false;
	if(X.p < X.pEnd && (*X.p == ' ' || *X.p == '\t' || *X.p == '\n' || *X.p == '\r')) {
		char q;
		xacmeAwsSkipSpace(&X.p, X.pEnd);
		if(xacmeAwsXmlAt(&X, "xmlns")) {
			X.p += 5u;
			if(X.Prefix.Size != 0u) {
				if(!xacmeAwsXmlAt(&X, ":") || (size_t)(X.pEnd - ++X.p) < X.Prefix.Size ||
					memcmp(X.p, X.Prefix.Data, X.Prefix.Size) != 0) return false;
				X.p += X.Prefix.Size;
			}
			xacmeAwsSkipSpace(&X.p, X.pEnd);
			if(!xacmeAwsXmlAt(&X, "=")) return false;
			X.p++; xacmeAwsSkipSpace(&X.p, X.pEnd);
			if(X.p == X.pEnd || (*X.p != '\'' && *X.p != '"')) return false;
			q = *X.p++;
			if(xacmeAwsXmlAt(&X, "https://route53.amazonaws.com/doc/2013-04-01/")) X.p += sizeof("https://route53.amazonaws.com/doc/2013-04-01/") - 1u;
			else if(xacmeAwsXmlAt(&X, "http://route53.amazonaws.com/doc/2013-04-01/")) X.p += sizeof("http://route53.amazonaws.com/doc/2013-04-01/") - 1u;
			else return false;
			if(X.p == X.pEnd || *X.p++ != q) return false;
			xacmeAwsSkipSpace(&X.p, X.pEnd);
		} else if(X.Prefix.Size != 0u) return false;
	} else if(X.Prefix.Size != 0u) return false;
	if(!xacmeAwsXmlAt(&X, ">")) return false;
	X.p++;
	if(!xacmeAwsXmlTag(&X, "Error", false, &bEmpty) || bEmpty) return false;
	for(;;) {
		xacmeawserrorxml Next = X;
		unsigned iField;
		if(xacmeAwsXmlTag(&Next, "Error", true, NULL)) { X = Next; break; }
		if(xacmeAwsXmlText(&X, "Code", Code)) iField = 1u;
		else if(xacmeAwsXmlText(&X, "Type", NULL)) iField = 2u;
		else if(xacmeAwsXmlText(&X, "Message", NULL)) iField = 4u;
		else if(xacmeAwsXmlTag(&X, "Messages", false, &bEmpty)) {
			iField = 8u;
			if(!bEmpty) {
				for(;;) {
					Next = X;
					if(xacmeAwsXmlTag(&Next, "Messages", true, NULL)) { X = Next; break; }
					if(!xacmeAwsXmlText(&X, "Message", NULL)) return false;
				}
			}
		} else return false;
		if((iSeen & iField) != 0u) return false;
		iSeen |= iField;
	}
	if((iSeen & 1u) == 0u) return false;
	/* RequestId is optional in service errors, but duplicate IDs are invalid. */
	{
		xacmeawserrorxml Next = X;
		if(xacmeAwsXmlText(&Next, "RequestId", NULL)) X = Next;
	}
	if(!xacmeAwsXmlTag(&X, "ErrorResponse", true, NULL) ||
		!xacmeAwsXmlSpace(&X) || X.p != X.pEnd) return false;
	memcpy(sCode, Code, strlen(Code) + 1u);
	return true;
}

bool xacmeAwsChangeAccepted(cstr sXml)
{
	const char* p;
	const char* pEnd;
	xstrview Id, Status, Submitted, Comment;
	if(sXml == NULL || strlen(sXml) > XACME_AWS_XML_MAX) return false;
	p = sXml; pEnd = p + strlen(sXml);
	xacmeAwsSkipSpace(&p, pEnd);
	if(strncmp(p, "<?xml ", 6u) == 0) {
		const char* pClose = strstr(p + 6u, "?>");
		if(pClose == NULL) return false;
		p = pClose + 2u;
	}
	if(!xacmeAwsConsume(&p, pEnd, "<ChangeResourceRecordSetsResponse>") &&
		!xacmeAwsConsume(&p, pEnd, "<ChangeResourceRecordSetsResponse xmlns=\"https://route53.amazonaws.com/doc/2013-04-01/\">"))
		return false;
	if(!xacmeAwsConsume(&p, pEnd, "<ChangeInfo>")) return false;
	xacmeAwsSkipSpace(&p, pEnd);
	if(!xacmeAwsConsume(&p, pEnd, "<Comment/>") &&
		!xacmeAwsConsume(&p, pEnd, "<Comment />") && strncmp(p, "<Comment>", 9u) == 0 &&
		!xacmeAwsTake(&p, pEnd, "<Comment>", "</Comment>", &Comment)) return false;
	if(!xacmeAwsTake(&p, pEnd, "<Id>", "</Id>", &Id) || Id.Size == 0u ||
		!xacmeAwsTake(&p, pEnd, "<Status>", "</Status>", &Status) ||
		!((Status.Size == 7u && memcmp(Status.Data, "PENDING", 7u) == 0) ||
		  (Status.Size == 6u && memcmp(Status.Data, "INSYNC", 6u) == 0)) ||
		!xacmeAwsTake(&p, pEnd, "<SubmittedAt>", "</SubmittedAt>", &Submitted) ||
		Submitted.Size == 0u || !xacmeAwsConsume(&p, pEnd, "</ChangeInfo>") ||
		!xacmeAwsConsume(&p, pEnd, "</ChangeResourceRecordSetsResponse>")) return false;
	xacmeAwsSkipSpace(&p, pEnd);
	return p == pEnd;
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
