#include "../internal/xacme_flow.h"

#if defined(XACME_FEATURE_ACME_FLOW)

#include <xrt/acme_dns.h>
#include <xrt/codec.h>
#include <xrt/http.h>
#include <xrt/json.h>
#include <xrt/memory.h>
#include <xrt/pem.h>
#include <xrt/time.h>
#include <xrt/value.h>
#include <xrt/x509.h>

#include "../internal/xacme_dnstxt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XACME_FLOW_POLL_MAX 180u
#define XACME_FLOW_URL_MAX ((size_t)sizeof(((xacmeclient*)0)->sKid))

typedef struct xacmeflowurl {
	char sData[512];
	size_t iSize;
} xacmeflowurl;

static void xacmeFlowError(
	xerrkind Kind, xacmeflowerror Code, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.flow", (int32)Code, sMessage);
}

static bool xacmeFlowCopyText(char* sDest, size_t iCapacity, cstr sSource)
{
	if((sSource == NULL) || (strlen(sSource) >= iCapacity))
	{
		return false;
	}
	strcpy(sDest, sSource);
	return true;
}

/* Keep only the CSR public key after the temporary private key is erased. */
typedef struct xacmeflowcertpublic {
	xacmecertkeykind Kind;
	uint8 Data[1040];
	size_t ModulusSize;
	size_t ExponentSize;
} xacmeflowcertpublic;

static void xacmeFlowCertPublic(
	const xacmecertkey* pKey, xacmeflowcertpublic* pPublic)
{
	pPublic->Kind = pKey->Kind;
	if(pKey->Kind == XACME_CERT_KEY_ES256)
		memcpy(pPublic->Data, pKey->Ec.Public, sizeof(pKey->Ec.Public));
	else
	{
		pPublic->ModulusSize = pKey->Rsa.ModulusSize;
		pPublic->ExponentSize = pKey->Rsa.ExponentSize;
		memcpy(pPublic->Data, pKey->Rsa.Modulus, pPublic->ModulusSize);
		memcpy(pPublic->Data + pPublic->ModulusSize,
			pKey->Rsa.Exponent, pPublic->ExponentSize);
	}
}

static bool xacmeFlowCertWhitespace(cstr sText, size_t iSize)
{
	size_t i;
	for(i = 0u; i < iSize; i++)
		if((sText[i] != ' ') && (sText[i] != '\t') &&
			(sText[i] != '\r') && (sText[i] != '\n')) return false;
	return true;
}

static bool xacmeFlowCertType(cstr sType)
{
	cstr sEnd;
	if(sType == NULL) return false;
	while((*sType == ' ') || (*sType == '\t')) sType++;
	sEnd = sType;
	while((*sEnd != '\0') && (*sEnd != ';')) sEnd++;
	while((sEnd > sType) && ((sEnd[-1] == ' ') || (sEnd[-1] == '\t')))
		sEnd--;
	return xrtHttpFieldNameEqual((xstrview){ sType, (size_t)(sEnd - sType) },
		XRT_STR_LITERAL("application/pem-certificate-chain"));
}

static bool xacmeFlowCertLeaf(
	const xx509cert* pCert, const xacmeflowcertpublic* pPublic,
	const xacmeflowurl* pDomains, size_t iDomainCount)
{
	xx509pubkey Key;
	xx509gencursor Names;
	xx509genname Name;
	xx509basicconstraints Constraints;
	xx509result Result;
	uint32 uMatched = 0u;
	xtime Now = xrtNow();
	if(!xrtX509PublicKey(pCert, &Key)) return false;
	if(pPublic->Kind == XACME_CERT_KEY_ES256)
	{
		if((Key.Type != X509_KEY_EC) || (Key.Curve != X509_CURVE_P256) ||
			(Key.Key.Size != 65u) || (memcmp(Key.Key.Data, pPublic->Data, 65u) != 0))
			goto Invalid;
	}
	else if(((Key.Type != X509_KEY_RSA) && (Key.Type != X509_KEY_RSA_PSS)) ||
		(Key.Modulus.Size != pPublic->ModulusSize) ||
		(Key.Exponent.Size != pPublic->ExponentSize) ||
		(memcmp(Key.Modulus.Data, pPublic->Data, Key.Modulus.Size) != 0) ||
		(memcmp(Key.Exponent.Data, pPublic->Data + pPublic->ModulusSize,
			Key.Exponent.Size) != 0)) goto Invalid;
	if((Now < pCert->NotBefore) || (Now > pCert->NotAfter)) goto Invalid;
	Result = xrtX509BasicConstraints(pCert, &Constraints);
	if(Result == X509_ERROR) return false;
	if((Result == X509_VALUE) && Constraints.CA) goto Invalid;
	Result = xrtX509SubjectAltName(pCert, &Names);
	if(Result == X509_ERROR) return false;
	if(Result != X509_VALUE) goto Invalid;
	while((Result = xrtX509GeneralNameRead(&Names, &Name)) == X509_VALUE)
	{
		size_t i;
		bool bFound = false;
		if(Name.Type != X509_NAME_DNS) goto Invalid;
		for(i = 0u; i < iDomainCount; i++)
			if(xrtHttpFieldNameEqual(
				(xstrview){ (cstr)Name.Value.Data, Name.Value.Size },
				(xstrview){ pDomains[i].sData, pDomains[i].iSize }))
			{
				uMatched |= UINT32_C(1) << i;
				bFound = true;
			}
		if(!bFound) goto Invalid;
	}
	if(Result == X509_ERROR) return false;
	if(uMatched == ((UINT32_C(1) << iDomainCount) - 1u)) return true;
Invalid:
	xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CERTIFICATE,
		"acme certificate does not match CSR key, identifiers or validity");
	return false;
}

/* This checks the supplied chain, not trust in a deployment's root store.
 * RFC 8555 certificates contain only CERTIFICATE objects, leaf first. Bounds
 * are independent of the HTTP body cap; at most three DER buffers are live. */
static bool xacmeFlowCertificate(
	const xacmehttpresponse* pR, const xacmeflowcertpublic* pPublic,
	const xacmeflowurl* pDomains, size_t iDomainCount, xbytesview Reference,
	bytes* ppLeaf, size_t* piLeafSize)
{
	xpemcursor Cursor;
	xpemblock Block;
	xpemresult Result;
	xx509cert Previous, Current;
	bytes pLeaf = NULL, pPrevious = NULL, pCurrent = NULL;
	size_t iLeafSize = 0u, iCount = 0u;
	bool bOk = false;
	if((pR->iStatus != 200u) || (pR->sBody == NULL) ||
		(pR->iBodySize == 0u) || !xacmeFlowCertType(pR->sContentType))
		goto Invalid;
	if(!xrtPemInit(&Cursor, pR->sBody, pR->iBodySize)) goto ChildFailure;
	for(;;)
	{
		size_t iOffset = Cursor.Offset, iDerSize = 0u;
		Result = xrtPemRead(&Cursor, &Block);
		if(Result == XPEM_ERROR) goto ChildFailure;
		if(Result == XPEM_DONE)
		{
			if((iCount == 0u) || !xacmeFlowCertWhitespace(
				pR->sBody + iOffset, pR->iBodySize - iOffset)) goto Invalid;
			break;
		}
		if((iCount >= 16u) || (Block.Label.Size != 11u) ||
			(memcmp(Block.Label.Data, "CERTIFICATE", 11u) != 0) ||
			!xacmeFlowCertWhitespace(pR->sBody + iOffset,
				(size_t)(Block.Raw.Data - (pR->sBody + iOffset)))) goto Invalid;
		if(!xrtPemDecode(&Block, NULL, 0u, &iDerSize)) goto ChildFailure;
		if((iDerSize == 0u) || (iDerSize > 256u * 1024u)) goto Invalid;
		pCurrent = xrtPemDecodeNew(&Block, &iDerSize);
		if((pCurrent == NULL) || !xrtX509Parse(pCurrent, iDerSize, &Current))
			goto ChildFailure;
		if(iCount == 0u)
		{
			if(!xacmeFlowCertLeaf(&Current, pPublic, pDomains, iDomainCount))
				goto ChildFailure;
			if((Reference.Data != NULL) && ((Reference.Size != iDerSize) ||
				(memcmp(Reference.Data, pCurrent, iDerSize) != 0))) goto Invalid;
			pLeaf = pCurrent;
			iLeafSize = iDerSize;
		}
		else
		{
			xx509basicconstraints Constraints;
			uint16 uUsage;
			xx509result ConstraintsResult = xrtX509BasicConstraints(&Current, &Constraints);
			xx509result UsageResult, NameResult;
			if(ConstraintsResult == X509_ERROR) goto ChildFailure;
			UsageResult = xrtX509KeyUsage(&Current, &uUsage);
			if(UsageResult == X509_ERROR) goto ChildFailure;
			NameResult = xrtX509NameEqual(Previous.Issuer, Current.Subject);
			if(NameResult == X509_ERROR) goto ChildFailure;
			if((ConstraintsResult != X509_VALUE) || !Constraints.CA ||
				((UsageResult == X509_VALUE) && !(uUsage & X509_USAGE_CERT_SIGN)) ||
				(NameResult != X509_VALUE)) goto Invalid;
			if(!xrtX509CertificateVerify(&Previous, &Current)) goto ChildFailure;
		}
		if(pPrevious != pLeaf) xrtFree(pPrevious);
		pPrevious = pCurrent;
		Previous = Current;
		pCurrent = NULL;
		iCount++;
	}
	bOk = true;
	goto Done;
ChildFailure:
	if((xrtErrorKind(xrtGetError()) == XERR_MEMORY) ||
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED)) goto Done;
Invalid:
	xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CERTIFICATE,
		"acme certificate response has invalid type, objects, identity or chain");
Done:
	xrtFree(pCurrent);
	if(pPrevious != pLeaf) xrtFree(pPrevious);
	if(bOk && (ppLeaf != NULL))
	{
		*ppLeaf = pLeaf;
		*piLeafSize = iLeafSize;
		pLeaf = NULL;
	}
	xrtFree(pLeaf);
	return bOk;
}

/* ---------------- JSON 辅助 ---------------- */

static bool xacmeFlowJsonQuoteAppend(xbuffer* pOut, xstrview sText)
{
	size_t i;
	if(!xrtBufferAppendByte(pOut, '"'))
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
		else if((unsigned char)c < 0x20u)
		{
			char sEscape[6];
			sEscape[0] = '\\';
			sEscape[1] = 'u';
			sEscape[2] = '0';
			sEscape[3] = '0';
			sEscape[4] = "0123456789ABCDEF"[((unsigned char)c >> 4u) & 0xFu];
			sEscape[5] = "0123456789ABCDEF"[(unsigned char)c & 0xFu];
			bOk = xrtBufferAppend(
				pOut, (xbytesview){ (const uint8*)sEscape, 6u });
		}
		else
		{
			bOk = xrtBufferAppendByte(pOut, (uint8)c);
		}
		if(!bOk)
		{
			return false;
		}
	}
	return xrtBufferAppendByte(pOut, '"');
}

/* 取对象字符串成员到固定缓冲。 */
static bool xacmeJsonValueText(
	const xvalue* pObject, cstr sKey, xacmeflowurl* pOut)
{
	xvalue* pMember = xrtValueObjectGet(
		pObject, (xstrview){ sKey, strlen(sKey) });
	xstrview Text;
	if((pMember == NULL) ||
		!xrtValueGetString(pMember, &Text) || (Text.Size == 0u) ||
		(Text.Size >= sizeof(pOut->sData)) ||
		(memchr(Text.Data, '\0', Text.Size) != NULL))
	{
		return false;
	}
	memcpy(pOut->sData, Text.Data, Text.Size);
	pOut->sData[Text.Size] = '\0';
	pOut->iSize = Text.Size;
	return true;
}

/* A malformed peer response is a protocol error. Allocation failures retain
 * the parser's original cause, including its domain and object identity. */
static xvalue* xacmeFlowResponseObject(
	const xacmehttpresponse* pR, xacmeflowerror Code, cstr sMessage)
{
	xvalue* pRoot = NULL;
	if((pR->sBody != NULL) && (pR->iBodySize != 0u))
	{
		pRoot = xrtJsonParse((xstrview){ pR->sBody, pR->iBodySize });
		if((pRoot == NULL) && (xrtErrorKind(xrtGetError()) == XERR_MEMORY))
			return NULL;
	}
	if((pRoot == NULL) || !xrtValueIs(pRoot, XVALUE_OBJECT))
	{
		xrtValueRelease(pRoot);
		xacmeFlowError(XERR_PROTOCOL, Code, sMessage);
		return NULL;
	}
	return pRoot;
}

static bool xacmeFlowProblemType(
	const xacmehttpresponse* pR, xacmeflowurl* pType)
{
	xvalue* pRoot = xacmeFlowResponseObject(pR, XACME_FLOW_ERROR_PROTOCOL,
		"acme problem response must be a JSON object");
	bool bOk;
	if(pRoot == NULL) return false;
	bOk = xacmeJsonValueText(pRoot, "type", pType);
	xrtValueRelease(pRoot);
	if(!bOk)
		xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_PROTOCOL,
			"acme problem response type invalid");
	return bOk;
}

static bool xacmeFlowLinkTokenChar(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') ||
		(c != 0u && strchr("!#$%&'*+-.^_`|~", c) != NULL);
}

/* Registered relation names are case insensitive. A quoted rel value may
 * contain several space-separated relations and quoted-pair escapes. */
static bool xacmeFlowLinkHasAlternate(const char* p, const char* pEnd)
{
	static const char sRelation[] = "alternate";
	size_t iSize = 0u;
	bool bMatch = true;
	while(p < pEnd)
	{
		unsigned char c = (unsigned char)*p++;
		if(c == '\\' && p < pEnd) c = (unsigned char)*p++;
		if(c == ' ' || c == '\t')
		{
			if(bMatch && iSize == sizeof(sRelation) - 1u) return true;
			iSize = 0u;
			bMatch = true;
			continue;
		}
		if(c >= 'A' && c <= 'Z') c = (unsigned char)(c + 'a' - 'A');
		if(iSize >= sizeof(sRelation) - 1u || c != sRelation[iSize])
			bMatch = false;
		iSize++;
	}
	return bMatch && iSize == sizeof(sRelation) - 1u;
}

/* Parse each RFC 8288 link-value before considering its target. Commas in
 * URI references and quoted strings do not separate links. We do not apply
 * anchor overrides, so links containing anchor are ignored as required by
 * section 3.2. Only the first rel parameter of a link has meaning. */
static bool xacmeFlowLinkAlternate(cstr sLink, char* sOut, size_t iCap)
{
	const char* p = sLink;
	if(sLink == NULL || sOut == NULL || iCap == 0u) return false;
	for(;;)
	{
		const char *pTarget, *pTargetEnd;
		bool bAlternate = false, bRelSeen = false, bAnchor = false;
		while(*p == ' ' || *p == '\t' || *p == ',') p++;
		if(*p != '<') return false;
		pTarget = ++p;
		while(*p != '>' && *p != '\0')
		{
			if((unsigned char)*p <= 0x20u || *p == '<') return false;
			p++;
		}
		if(*p != '>') return false;
		pTargetEnd = p++;
		for(;;)
		{
			const char *pName, *pValue = NULL, *pValueEnd = NULL;
			bool bRel;
			xstrview Name;
			while(*p == ' ' || *p == '\t') p++;
			if(*p == ',' || *p == '\0') break;
			if(*p++ != ';') return false;
			while(*p == ' ' || *p == '\t') p++;
			pName = p;
			while(xacmeFlowLinkTokenChar((unsigned char)*p)) p++;
			if(p == pName) return false;
			Name = (xstrview){ pName, (size_t)(p - pName) };
			bRel = xrtHttpFieldNameEqual(Name, XRT_STR_LITERAL("rel"));
			if(xrtHttpFieldNameEqual(Name, XRT_STR_LITERAL("anchor"))) bAnchor = true;
			while(*p == ' ' || *p == '\t') p++;
			if(*p == '=')
			{
				p++;
				while(*p == ' ' || *p == '\t') p++;
				if(*p == '"')
				{
					pValue = ++p;
					while(*p != '"')
					{
						if(*p == '\0' || ((unsigned char)*p < 0x20u && *p != '\t') ||
							(unsigned char)*p == 0x7fu) return false;
						if(*p == '\\')
						{
							p++;
							if(*p == '\0' || ((unsigned char)*p < 0x20u && *p != '\t') ||
								(unsigned char)*p == 0x7fu) return false;
						}
						p++;
					}
					pValueEnd = p++;
				}
				else
				{
					pValue = p;
					while(xacmeFlowLinkTokenChar((unsigned char)*p)) p++;
					if(p == pValue) return false;
					pValueEnd = p;
				}
			}
			if(bRel && !bRelSeen)
			{
				if(pValue == NULL) return false;
				bAlternate = xacmeFlowLinkHasAlternate(pValue, pValueEnd);
				bRelSeen = true;
			}
		}
		if(bAlternate && !bAnchor)
		{
			size_t iLen = (size_t)(pTargetEnd - pTarget);
			if(iLen == 0u || iLen >= iCap) return false;
			memcpy(sOut, pTarget, iLen);
			sOut[iLen] = '\0';
			return true;
		}
		if(*p == '\0') return false;
		p++;
	}
}

static bool xacmeFlowUriAppend(char* sOut, size_t iCapacity,
	size_t* piSize, xstrview Part)
{
	if(Part.Size >= iCapacity - *piSize) return false;
	if(Part.Size != 0u) memcpy(sOut + *piSize, Part.Data, Part.Size);
	*piSize += Part.Size;
	sOut[*piSize] = '\0';
	return true;
}

/* RFC 3986 section 5.2.4. Only literal dot segments are removed: percent
 * escapes and the query retain their exact spelling for the protected URL. */
static size_t xacmeFlowUriDots(cstr sPath, size_t iSize, char* sOut)
{
	size_t i = 0u, n = 0u;
	while(i < iSize)
	{
		size_t r = iSize - i;
		if((r >= 3u) && (memcmp(sPath + i, "../", 3u) == 0)) i += 3u;
		else if((r >= 2u) && (memcmp(sPath + i, "./", 2u) == 0)) i += 2u;
		else if((r >= 3u) && (memcmp(sPath + i, "/./", 3u) == 0)) i += 2u;
		else if((r == 2u) && (memcmp(sPath + i, "/.", 2u) == 0))
		{
			i += 2u;
			sOut[n++] = '/';
		}
		else if(((r >= 4u) && (memcmp(sPath + i, "/../", 4u) == 0)) ||
			((r == 3u) && (memcmp(sPath + i, "/..", 3u) == 0)))
		{
			i += 3u;
			while((n > 0u) && (sOut[n - 1u] != '/')) n--;
			if(n > 0u) n--;
			if(i == iSize) sOut[n++] = '/';
		}
		else if(((r == 1u) && (sPath[i] == '.')) ||
			((r == 2u) && (memcmp(sPath + i, "..", 2u) == 0))) i = iSize;
		else
		{
			if(sPath[i] == '/') sOut[n++] = sPath[i++];
			while((i < iSize) && (sPath[i] != '/')) sOut[n++] = sPath[i++];
		}
	}
	sOut[n] = '\0';
	return n;
}

static bool xacmeFlowUriText(cstr sText)
{
	size_t i;
	for(i = 0u; sText[i] != '\0'; i++)
	{
		unsigned char c = (unsigned char)sText[i];
		if(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) ||
			((c >= '0') && (c <= '9'))) continue;
		if((c == '%') && sText[i + 1u] && sText[i + 2u])
		{
			size_t k;
			for(k = 1u; k <= 2u; k++)
			{
				unsigned char h = (unsigned char)sText[i + k];
				if(!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') ||
					(h >= 'A' && h <= 'F'))) return false;
			}
			i += 2u;
		}
		else if((c == '%') || (c > 0x7fu) ||
			(strchr("-._~:/?#[]@!$&'()*+,;=", c) == NULL)) return false;
	}
	return true;
}

/* Resolve against the certificate retrieval URL, not the directory URL.
 * Fragments identify no different HTTP resource and are omitted from both
 * the request and its JWS URL. Failure leaves the caller's output untouched. */
static bool xacmeFlowAlternateUrl(cstr sBase, cstr sReference,
	char* sOut, size_t iCapacity)
{
	xhttptarget Base, Ref, Checked;
	xstrview Scheme, Authority, Path, Query;
	char sRef[1024] = {0}, sMerged[1024] = {0}, sPath[1024], sUrl[512] = {0};
	size_t iRefSize, iSize = 0u, iPathSize;
	bool bQuery, bNormalize = true;
	cstr sQuestion;
	size_t iFirstDelimiter;
	if((sBase == NULL) || (sReference == NULL) || (sOut == NULL) ||
		(iCapacity == 0u) || (strlen(sBase) >= sizeof(sUrl)) ||
		(strlen(sReference) >= sizeof(sUrl)) || !xacmeFlowUriText(sReference) ||
		!xrtHttpTargetParse(XRT_STR_LITERAL("POST"),
			(xstrview){ sBase, strlen(sBase) }, &Base) ||
		(Base.Form != XHTTP_TARGET_ABSOLUTE) ||
		!(Base.Flags & XHTTP_TARGET_HAS_AUTHORITY) ||
		(Base.Authority.Size == 0u)) return false;
	iRefSize = strcspn(sReference, "#");
	iFirstDelimiter = strcspn(sReference, ":/?#");
	Scheme = Base.Scheme;
	Authority = Base.Authority;
	if((iFirstDelimiter < iRefSize && sReference[iFirstDelimiter] == ':') ||
		((iRefSize >= 2u) && (memcmp(sReference, "//", 2u) == 0)))
	{
		if((sReference[0] != '/') &&
			((iRefSize - iFirstDelimiter < 3u) ||
			(memcmp(sReference + iFirstDelimiter, "://", 3u) != 0))) return false;
		if(sReference[0] == '/')
		{
			if(!xacmeFlowUriAppend(sRef, sizeof(sRef), &iSize, Scheme) ||
				!xacmeFlowUriAppend(sRef, sizeof(sRef), &iSize, XRT_STR_LITERAL(":")))
				return false;
		}
		if(!xacmeFlowUriAppend(sRef, sizeof(sRef), &iSize,
			(xstrview){ sReference, iRefSize }) ||
			!xrtHttpTargetParse(XRT_STR_LITERAL("POST"),
				(xstrview){ sRef, iSize }, &Ref) ||
			(Ref.Form != XHTTP_TARGET_ABSOLUTE) ||
			!(Ref.Flags & XHTTP_TARGET_HAS_AUTHORITY) ||
			(Ref.Authority.Size == 0u)) return false;
		Scheme = Ref.Scheme;
		Authority = Ref.Authority;
		Path = Ref.Path;
		Query = Ref.Query;
		bQuery = (Ref.Flags & XHTTP_TARGET_HAS_QUERY) != 0u;
	}
	else
	{
		sQuestion = (cstr)memchr(sReference, '?', iRefSize);
		Path = (xstrview){ sReference, sQuestion ? (size_t)(sQuestion - sReference) : iRefSize };
		bQuery = (sQuestion != NULL);
		Query = bQuery ? (xstrview){ sQuestion + 1u,
			iRefSize - (size_t)(sQuestion + 1u - sReference) } : (xstrview){ NULL, 0u };
		if(Path.Size == 0u)
		{
			Path = Base.Path;
			bNormalize = false;
			if(!bQuery)
			{
				Query = Base.Query;
				bQuery = (Base.Flags & XHTTP_TARGET_HAS_QUERY) != 0u;
			}
		}
		else if(Path.Data[0] != '/')
		{
			size_t iPrefix = Base.Path.Size;
			while((iPrefix > 0u) && (Base.Path.Data[iPrefix - 1u] != '/')) iPrefix--;
			iSize = 0u;
			if(!xacmeFlowUriAppend(sMerged, sizeof(sMerged), &iSize,
				Base.Path.Size == 0u ? XRT_STR_LITERAL("/") :
				(xstrview){ Base.Path.Data, iPrefix }) ||
				!xacmeFlowUriAppend(sMerged, sizeof(sMerged), &iSize, Path)) return false;
			Path = (xstrview){ sMerged, iSize };
		}
	}
	if(!xrtHttpFieldNameEqual(Scheme, XRT_STR_LITERAL("http")) &&
		!xrtHttpFieldNameEqual(Scheme, XRT_STR_LITERAL("https"))) return false;
	if(!xrtHttpHostValid(Authority)) return false;
	iPathSize = bNormalize ? xacmeFlowUriDots(Path.Data, Path.Size, sPath) : Path.Size;
	if(bNormalize) Path = (xstrview){ sPath, iPathSize };
	iSize = 0u;
	if(!xacmeFlowUriAppend(sUrl, sizeof(sUrl), &iSize, Scheme) ||
		!xacmeFlowUriAppend(sUrl, sizeof(sUrl), &iSize, XRT_STR_LITERAL("://")) ||
		!xacmeFlowUriAppend(sUrl, sizeof(sUrl), &iSize, Authority) ||
		!xacmeFlowUriAppend(sUrl, sizeof(sUrl), &iSize, Path) ||
		(bQuery && (!xacmeFlowUriAppend(sUrl, sizeof(sUrl), &iSize, XRT_STR_LITERAL("?")) ||
			!xacmeFlowUriAppend(sUrl, sizeof(sUrl), &iSize, Query))) ||
		!xrtHttpTargetParse(XRT_STR_LITERAL("POST"), (xstrview){ sUrl, iSize }, &Checked) ||
		(iSize >= iCapacity)) return false;
	memcpy(sOut, sUrl, iSize + 1u);
	return true;
}

/* Issue 总预算：超限返回 true（已到截止）。 */
static bool xacmeFlowDeadlineHit(const xacmeclient* pClient)
{
	return pClient->bIssueDeadline &&
		(xrtClock() >= (uint64)pClient->IssueDeadline);
}

/* ---------------- nonce 与 POST ---------------- */

static void xacmeFlowTakeNonce(xacmeclient* pClient, xacmehttpresponse* pR)
{
	cstr sNonce = pR->sReplayNonce;
	size_t i;
	if((sNonce == NULL) || (sNonce[0] == '\0') ||
		(strlen(sNonce) >= sizeof(pClient->sNonce))) return;
	for(i = 0u; sNonce[i] != '\0'; i++)
	{
		unsigned char c = (unsigned char)sNonce[i];
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || c == '-' || c == '_')) return;
	}
	strcpy(pClient->sNonce, sNonce);
}

static bool xacmeFlowNewNonce(xacmeclient* pClient)
{
	xacmehttpresponse R;
	if(pClient->sNonce[0] != '\0')
	{
		return true;
	}
	if(!xacmeHttpExchange(
		&pClient->Http, "GET", pClient->sNewNonce, NULL,
		(xstrview){ NULL, 0u }, &R))
	{
		return false;
	}
	if((R.iStatus < 200u) || (R.iStatus >= 300u))
	{
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
			"acme flow new-nonce response status invalid");
		return false;
	}
	xacmeFlowTakeNonce(pClient, &R);
	xacmeHttpResponseUnit(&R);
	if(pClient->sNonce[0] == '\0')
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
			"acme flow new-nonce response missing Replay-Nonce");
		return false;
	}
	return true;
}

/*
	执行一次 ACME POST（bKid=false 时保护头嵌 JWK，用于 newAccount）。
	响应由调用方 Unit；本函数顺带收割 nonce。
*/
static bool xacmeFlowPost(
	xacmeclient* pClient, cstr sUrl, xstrview sPayload, bool bKid,
	xacmehttpresponse* pR, uint32 uNonceRetry)
{
	xacmejwsheader H;
	str sToken = NULL;
	bool bOk;

	memset(pR, 0, sizeof(*pR));
	if(!xacmeFlowNewNonce(pClient))
	{
		return false;
	}
	H.Nonce = (xstrview){ pClient->sNonce, strlen(pClient->sNonce) };
	H.Url = (xstrview){ sUrl, strlen(sUrl) };
	if(bKid)
	{
		H.Kid = (xstrview){ pClient->sKid, strlen(pClient->sKid) };
	}
	else
	{
		H.Kid.Data = NULL;
		H.Kid.Size = 0u;
	}
	sToken = xacmeJwsEs256(&pClient->AccountKey, &H, sPayload);
	if(sToken == NULL)
	{
		return false;
	}
	pClient->sNonce[0] = '\0';
	bOk = xacmeHttpExchange(
		&pClient->Http, "POST", sUrl, "application/jose+json",
		(xstrview){ sToken, strlen(sToken) }, pR);
	xrtFree(sToken);
	if(!bOk)
	{
		return false;
	}
	xacmeFlowTakeNonce(pClient, pR);
	/* RFC 8555 §6.5: identify the problem structurally and use the fresh
	 * nonce supplied with that response. Three POST attempts in total. */
	if(pR->iStatus == 400u)
	{
		xacmeflowurl Type;
		if(!xacmeFlowProblemType(pR, &Type))
		{
			xacmeHttpResponseUnit(pR);
			return false;
		}
		if(strcmp(Type.sData, "urn:ietf:params:acme:error:badNonce") == 0)
		{
			if((pClient->sNonce[0] == '\0') || (uNonceRetry >= 2u))
			{
				xacmeHttpResponseUnit(pR);
				xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
					"acme badNonce response missing fresh nonce or retries exhausted");
				return false;
			}
			xacmeHttpResponseUnit(pR);
			return xacmeFlowPost(
				pClient, sUrl, sPayload, bKid, pR, uNonceRetry + 1u);
		}
	}
	return true;
}

/* POST-as-GET：空负载。 */
static bool xacmeFlowPostAsGet(
	xacmeclient* pClient, cstr sUrl, xacmehttpresponse* pR)
{
	uint32 uAttempt;
	for(uAttempt = 1u; uAttempt <= 3u; uAttempt++)
	{
		const xerror* pError;
		cstr sDomain;
		if(xacmeFlowPost(
			pClient, sUrl, (xstrview){ "", 0u }, true, pR, 0u))
		{
			xrtClearError();
			return true;
		}
		pError = xrtGetError();
		sDomain = xrtErrorDomain(pError);
		if((uAttempt == 3u) || xacmeFlowDeadlineHit(pClient) ||
			(xrtErrorCode(pError) != XACME_HTTP_ERROR_UNCERTAIN) ||
			(sDomain == NULL) ||
			(strcmp(sDomain, "xrt.acme.http") != 0))
		{
			return false;
		}
		/* 只读 POST-as-GET 可用新 nonce/JWS 安全重试。 */
		xrtClearError();
		xrtSleep((uAttempt == 1u) ? 500u : 1000u);
	}
	return false;
}

static uint32 xacmeFlowSleepMs(xacmehttpresponse* pR)
{
	long v;
	if((pR->sRetryAfter == NULL) || (pR->sRetryAfter[0] == '\0'))
	{
		return 500u;
	}
	v = atol(pR->sRetryAfter);
	if((v <= 0) || (v > 5))
	{
		v = 1;
	}
	return (uint32)(v * 1000u);
}

/*
	轮询某个 URL 的 JSON status 字段：pending/processing 继续等，
	其他状态（valid/ready/invalid...）即返回该状态的堆副本。
	pFinalizeOut 非空时顺带收割 finalize 字段一次。
*/
static str xacmeFlowWaitStatus(
	xacmeclient* pClient, cstr sUrl, xacmeflowurl* pFinalizeOut)
{
	size_t i;
	for(i = 0; i < XACME_FLOW_POLL_MAX; i++)
	{
		xacmehttpresponse R;
		xvalue* pRoot;
		xacmeflowurl Status;
		uint32 uMs;
		bool bTerminal;
		if(xacmeFlowDeadlineHit(pClient))
		{
			xacmeFlowError(XERR_TIMEOUT, XACME_FLOW_ERROR_PROTOCOL,
				"acme flow issue budget exhausted");
			return NULL;
		}
		if(!xacmeFlowPostAsGet(pClient, sUrl, &R)) return NULL;
		if((R.iStatus < 200u) || (R.iStatus >= 300u))
		{
			char sDetail[280];
			snprintf(sDetail, sizeof(sDetail),
				"acme flow poll error status=%u body=%.180s",
				(unsigned)R.iStatus, R.sBody ? R.sBody : "");
			xacmeHttpResponseUnit(&R);
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_PROTOCOL, sDetail);
			return NULL;
		}
		pRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_PROTOCOL,
			"acme flow poll json invalid");
		if(pRoot == NULL)
		{
			xacmeHttpResponseUnit(&R);
			return NULL;
		}
		if(!xacmeJsonValueText(pRoot, "status", &Status))
		{
			xrtValueRelease(pRoot);
			xacmeHttpResponseUnit(&R);
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_PROTOCOL,
				"acme flow poll status invalid");
			return NULL;
		}
		if(pFinalizeOut != NULL)
		{
			if(xrtValueObjectGet(pRoot, XRT_STR_LITERAL("finalize")) != NULL)
			{
				if(!xacmeJsonValueText(pRoot, "finalize", pFinalizeOut))
				{
					xrtValueRelease(pRoot);
					xacmeHttpResponseUnit(&R);
					xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_FINALIZE,
						"acme flow order finalize url invalid");
					return NULL;
				}
			}
			if(strcmp(Status.sData, "ready") == 0 && pFinalizeOut->sData[0] == 0)
			{
				xrtValueRelease(pRoot);
				xacmeHttpResponseUnit(&R);
				xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_FINALIZE,
					"acme flow ready order missing finalize url");
				return NULL;
			}
		}
		bTerminal = strcmp(Status.sData, "pending") != 0 &&
			strcmp(Status.sData, "processing") != 0;
		xrtValueRelease(pRoot);
		if(bTerminal)
		{
			str sStatus = (str)xrtMalloc(Status.iSize + 1u);
			if(sStatus != NULL) memcpy(sStatus, Status.sData, Status.iSize + 1u);
			xacmeHttpResponseUnit(&R);
			return sStatus;
		}
		uMs = xacmeFlowSleepMs(&R);
		xacmeHttpResponseUnit(&R);
		xrtSleep(uMs);
	}
	xacmeFlowError(XERR_TIMEOUT, XACME_FLOW_ERROR_PROTOCOL,
		"acme flow status poll exhausted");
	return NULL;
}

/* ---------------- DNS 铺设与清理（带重试） ---------------- */

static bool xacmeFlowHttpUncertain(const xerror* pError)
{
	cstr sDomain = xrtErrorDomain(pError);
	return (xrtErrorCode(pError) == XACME_HTTP_ERROR_UNCERTAIN) &&
		(sDomain != NULL) && (strcmp(sDomain, "xrt.acme.http") == 0);
}

static bool xacmeFlowDnsAddTerminal(const xerror* pError)
{
	cstr sDomain = xrtErrorDomain(pError);
	return xrtErrorKind(pError) == XERR_MEMORY || xacmeFlowHttpUncertain(pError) ||
		(xrtErrorCode(pError) == XACME_DNS_ERROR_UNCERTAIN && sDomain != NULL &&
		 strcmp(sDomain, "xrt.acme.dns") == 0);
}

/*
	provider Add/Remove 各最多 3 次尝试（1s/2s 退避）。
	Add 在已发送请求的结果未知时不可重试：重复记录可能失去可清理的
	RecordId；Remove 按 provider 契约幂等。失败保留首个根因。
	提供商层结果未知及内存错误同样不重放 Add。
*/
static bool xacmeFlowDnsAdd(
	const xacmednsprovider* pDns, cstr sFqdn, cstr sTxt)
{
	uint32 uAttempt;
	for(uAttempt = 1u; uAttempt <= 3u; uAttempt++)
	{
		xerror* pFirst;
		if(pDns->Add((xacmednsprovider*)pDns,
				(xstrview){ sFqdn, strlen(sFqdn) },
				(xstrview){ sTxt, strlen(sTxt) }))
		{
			return true;
		}
		pFirst = xrtErrorRef(xrtGetError());
		/* OOM can prevent publishing an uncertainty wrapper; never replay an
		 * Add when its failure could not even be represented completely. */
		if(xacmeFlowDnsAddTerminal(pFirst))
		{
			xrtSetErrorTake(pFirst);
			return false;
		}
		if(uAttempt < 3u)
		{
			if(getenv("XACME_DEBUG"))
			{
				printf("[dns-retry] add attempt=%u fqdn=%s\n",
					(unsigned)uAttempt, sFqdn);
			}
			xrtSleep(1000u * uAttempt);
		}
		xrtSetErrorTake(pFirst);
	}
	return false;
}

static void xacmeFlowDnsRemove(
	const xacmednsprovider* pDns, cstr sFqdn, cstr sTxt)
{
	uint32 uAttempt;
	for(uAttempt = 1u; uAttempt <= 3u; uAttempt++)
	{
		if(pDns->Remove((xacmednsprovider*)pDns,
				(xstrview){ sFqdn, strlen(sFqdn) },
				(xstrview){ sTxt, strlen(sTxt) }))
		{
			return;
		}
		if(uAttempt < 3u)
		{
			xrtSleep(1000u * uAttempt);
		}
		xrtClearError(); /* 清理是尽力而为，不污染主错误。 */
	}
}

/* ---------------- 传播确认 ---------------- */

/*
	解析 resolver 字符串："host"、"host:port" 或 "[v6]:port"。
	无端口段或段非法时回退 53；输出去掉括号的 host 到定长缓冲。
*/
static uint16 xacmeFlowResolverPort(
	cstr sResolver, char* sOutHost, size_t iHostCap)
{
	const char* sColon = strrchr(sResolver, ':');
	size_t iHostLen;
	if((sColon != NULL) && (sColon != sResolver))
	{
		long v = atol(sColon + 1);
		iHostLen = (size_t)(sColon - sResolver);
		if((sResolver[0] == '[') && (iHostLen > 1u) &&
			(sResolver[iHostLen - 1u] == ']'))
		{
			sResolver++;
			iHostLen -= 2u;
		}
		if((v > 0) && (v <= 65535) && (iHostLen < iHostCap))
		{
			memcpy(sOutHost, sResolver, iHostLen);
			sOutHost[iHostLen] = '\0';
			return (uint16)v;
		}
	}
	snprintf(sOutHost, iHostCap, "%s", sResolver);
	return 53u;
}

/* 任一配置 resolver 已返回期望 TXT 值即视为可见。 */
static bool xacmeFlowTxtVisible(
	xacmedns* pDns, const xacmeclient* pClient,
	cstr sFqdn, cstr sExpected)
{
	size_t i;
	for(i = 0; i < pClient->iPropagateResolverCount; i++)
	{
		char sRecords[4][XACME_TXT_RECORD_MAX];
		char sHost[64];
		uint16 iPort = xacmeFlowResolverPort(
			pClient->sPropagateResolvers[i], sHost, sizeof(sHost));
		size_t iCount = 0u;
		size_t j;
		if(!xacmeDnsTxtQuery(
				pDns, sHost, iPort, sFqdn, sRecords, 4u, &iCount))
		{
			if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
			xrtClearError(); /* Advisory reachability failure must not mask a later OOM. */
			continue; /* 单个 resolver 不可达不算失败。 */
		}
		for(j = 0; j < iCount; j++)
		{
			if(strcmp(sRecords[j], sExpected) == 0)
			{
				return true;
			}
		}
	}
	return false;
}

/*
	挑战触发前的传播确认门（尽力而为）：
	- provider 带 XACME_DNS_CAP_PROPAGATE 时委托 provider 自证；
	- 否则对公共 resolver 组轮询 TXT（任一可见即通过）；
	- 超时不阻断签发——CA 只查权威侧，公共递归滞后不必然失败，
	  仅在 XACME_DEBUG 下输出提示。
*/
static void xacmeFlowWaitPropagate(
	xacmeclient* pClient, const xacmednsprovider* pDns,
	cstr sFqdn, cstr sTxt)
{
	xacmedns Probe;
	uint64 uDeadline;
	if(((pDns->iCaps & XACME_DNS_CAP_PROPAGATE) != 0u) &&
		(pDns->Propagate != NULL))
	{
		(void)pDns->Propagate((xacmednsprovider*)pDns,
			(xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sTxt, strlen(sTxt) });
		return;
	}
	if(!xacmeDnsInit(&Probe, pClient->Http.pEngine))
	{
		return;
	}
	uDeadline = xrtClock() +
		(uint64)pClient->uPropagateTimeoutMs * UINT64_C(1000);
	if(pClient->bIssueDeadline &&
		((uint64)pClient->IssueDeadline < uDeadline))
	{
		uDeadline = pClient->IssueDeadline;
	}
	while(xrtClock() < uDeadline)
	{
		if(xacmeFlowTxtVisible(&Probe, pClient, sFqdn, sTxt))
		{
			xacmeDnsUnit(&Probe);
			return;
		}
		if(xrtErrorKind(xrtGetError()) == XERR_MEMORY)
		{
			xacmeDnsUnit(&Probe);
			return;
		}
		xrtSleep(2000u);
	}
	xacmeDnsUnit(&Probe);
	if(getenv("XACME_DEBUG"))
	{
		printf("[dbg] propagate confirm timeout fqdn=%s\n", sFqdn);
	}
}

/* ---------------- 初始化与签发 ---------------- */

/*
	重新拉取授权对象，提取挑战 error.detail（约 160 字符）进 sOut。
	失败时留空串——诊断增强，不改变失败语义。
*/
static void xacmeFlowChallengeDetail(
	xacmeclient* pClient, cstr sAuthzUrl, char* sOut, size_t iCapacity)
{
	xacmehttpresponse R;
	xvalue* pRoot;
	xvalue* pChallenges;
	size_t j;

	sOut[0] = '\0';
	if(!xacmeFlowPostAsGet(pClient, sAuthzUrl, &R))
	{
		return;
	}
	pRoot = (R.sBody != NULL) ?
		xrtJsonParse((xstrview){ R.sBody, R.iBodySize }) : NULL;
	xacmeHttpResponseUnit(&R);
	if((pRoot == NULL) || !xrtValueIs(pRoot, XVALUE_OBJECT))
	{
		xrtValueRelease(pRoot);
		return;
	}
	pChallenges = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("challenges"));
	for(j = 0; (pChallenges != NULL) &&
		xrtValueIs(pChallenges, XVALUE_ARRAY) &&
		(j < xrtValueCount(pChallenges)); j++)
	{
		xvalue* pChallenge = xrtValueArrayGet(pChallenges, j);
		xvalue* pError;
		xvalue* pDetail;
		xstrview Text;
		if((pChallenge == NULL) || !xrtValueIs(pChallenge, XVALUE_OBJECT))
		{
			continue;
		}
		pError = xrtValueObjectGet(pChallenge, XRT_STR_LITERAL("error"));
		if((pError == NULL) || !xrtValueIs(pError, XVALUE_OBJECT))
		{
			continue;
		}
		pDetail = xrtValueObjectGet(pError, XRT_STR_LITERAL("detail"));
		if((pDetail != NULL) && xrtValueGetString(pDetail, &Text) &&
			(Text.Size > 0u))
		{
			size_t iCopy = (Text.Size < (iCapacity - 1u)) ?
				Text.Size : (iCapacity - 1u);
			memcpy(sOut, Text.Data, iCopy);
			sOut[iCopy] = '\0';
			break;
		}
	}
	xrtValueRelease(pRoot);
}

bool xacmeClientInit(
	xacmeclient* pClient, struct xnetengine* pBorrowedEngine,
	cstr sCaPem, const xacmeaccountconfig* pAccount, uint64 uTimeoutUs)
{
	xacmehttpresponse R;
	xvalue* pRoot = NULL;
	xacmeflowurl NewNonce;
	xacmeflowurl NewAccount;
	xacmeflowurl NewOrder;
	xbuffer Payload;
	bool bOk = false;

	if((pClient == NULL) || (pAccount == NULL) ||
		(pAccount->sDirectoryUrl == NULL) ||
		(pAccount->sDirectoryUrl[0] == '\0'))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme client init requires client and account config");
		return false;
	}
	memset(pClient, 0, sizeof(*pClient));
	if(!xacmeFlowCopyText(
			pClient->sDirectoryUrl, sizeof(pClient->sDirectoryUrl),
			pAccount->sDirectoryUrl))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme client directory url too long");
		return false;
	}
	if(!xacmeHttpInit(&pClient->Http, pBorrowedEngine, sCaPem, uTimeoutUs))
	{
		goto Done;
	}
	if((pAccount->sAccountKeyPem != NULL) &&
		(pAccount->sAccountKeyPem[0] != '\0'))
	{
		if(!xacmeKeyPemRead(
			pAccount->sAccountKeyPem, strlen(pAccount->sAccountKeyPem),
			&pClient->AccountKey))
		{
			goto Done;
		}
	}
	else if(!xacmeEs256Generate(&pClient->AccountKey))
	{
		goto Done;
	}

	/* directory */
	if(!xacmeHttpExchange(
		&pClient->Http, "GET", pAccount->sDirectoryUrl, NULL,
		(xstrview){ NULL, 0u }, &R))
	{
		goto Done;
	}
	if((R.iStatus != 200u) || (R.sBody == NULL))
	{
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_DIRECTORY,
			"acme client directory response invalid");
		goto Done;
	}
	pRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_DIRECTORY,
		"acme client directory json invalid");
	xacmeHttpResponseUnit(&R);
	if(pRoot == NULL) goto Done;
	if(
		!xacmeJsonValueText(pRoot, "newNonce", &NewNonce) ||
		!xacmeJsonValueText(pRoot, "newAccount", &NewAccount) ||
		!xacmeJsonValueText(pRoot, "newOrder", &NewOrder))
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_DIRECTORY,
			"acme client directory json missing endpoints");
		goto Done;
	}
	if(!xacmeFlowCopyText(
			pClient->sNewNonce, sizeof(pClient->sNewNonce), NewNonce.sData) ||
		!xacmeFlowCopyText(
			pClient->sNewAccount, sizeof(pClient->sNewAccount),
			NewAccount.sData) ||
		!xacmeFlowCopyText(
			pClient->sNewOrder, sizeof(pClient->sNewOrder), NewOrder.sData))
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_DIRECTORY,
			"acme client directory url too long");
		goto Done;
	}
	/* revokeCert/keyChange 可选：缺失时对应路径显式报错。 */
	{
		xacmeflowurl Optional;
		if((xrtValueObjectGet(pRoot, XRT_STR_LITERAL("revokeCert")) != NULL &&
			 !xacmeJsonValueText(pRoot, "revokeCert", &Optional)) ||
			(xrtValueObjectGet(pRoot, XRT_STR_LITERAL("keyChange")) != NULL &&
			 !xacmeJsonValueText(pRoot, "keyChange", &Optional)))
		{
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_DIRECTORY,
				"acme client optional directory endpoint invalid");
			goto Done;
		}
		if(xacmeJsonValueText(pRoot, "revokeCert", &Optional))
		{
			(void)xacmeFlowCopyText(
				pClient->sRevokeCert, sizeof(pClient->sRevokeCert),
				Optional.sData);
		}
		if(xacmeJsonValueText(pRoot, "keyChange", &Optional))
		{
			(void)xacmeFlowCopyText(
				pClient->sKeyChange, sizeof(pClient->sKeyChange),
				Optional.sData);
		}
	}

	/* 注册或复用账户：201=新建，200=已存在。载荷按需携带
	   contact 与 externalAccountBinding（RFC 8555 §7.3/§7.3.4）。 */
	xrtBufferInit(&Payload);
	if(!xrtBufferAppend(
		&Payload, XRT_BYTES_LITERAL("{\"termsOfServiceAgreed\":true")))
	{
		xrtBufferUnit(&Payload);
		goto Done;
	}
	if((pAccount->sContactEmail != NULL) &&
		(pAccount->sContactEmail[0] != '\0'))
	{
		xbuffer Mailto;
		bool bContactOk;
		xrtBufferInit(&Mailto);
		bContactOk = xrtBufferAppend(&Mailto, XRT_BYTES_LITERAL("mailto:")) &&
			xrtBufferAppend(&Mailto, (xbytesview){
				(const uint8*)pAccount->sContactEmail,
				strlen(pAccount->sContactEmail) }) &&
			xrtBufferAppend(&Payload, XRT_BYTES_LITERAL(",\"contact\":[")) &&
			xacmeFlowJsonQuoteAppend(&Payload,
				(xstrview){ (cstr)Mailto.Data, Mailto.Size }) &&
			xrtBufferAppendByte(&Payload, (uint8)']');
		xrtBufferUnit(&Mailto);
		if(!bContactOk)
		{
			xrtBufferUnit(&Payload);
			goto Done;
		}
	}
	if((pAccount->Eab.sKid != NULL) && (pAccount->Eab.sKid[0] != '\0'))
	{
		/* base64url 文本 MAC key（兼容带/不带填充）。 */
		static const xbase64config B64UrlPad = {
			NULL, XBASE64_URL | XBASE64_OPTIONAL_PADDING
		};
		uint8 Mac[64];
		size_t iMacSize = 0u;
		str sJwk = NULL;
		str sEab = NULL;
		if((pAccount->Eab.sHmac == NULL) || (pAccount->Eab.sHmac[0] == '\0'))
		{
			xrtBufferUnit(&Payload);
			xacmeFlowError(
				XERR_ARGUMENT, XACME_FLOW_ERROR_ACCOUNT,
				"acme client eab kid without hmac");
			goto Done;
		}
		if(!xrtBase64Decode(
				pAccount->Eab.sHmac, strlen(pAccount->Eab.sHmac), Mac,
				sizeof(Mac), &iMacSize, &B64UrlPad) ||
			(iMacSize == 0u))
		{
			xrtBufferUnit(&Payload);
			xacmeFlowError(
				XERR_ARGUMENT, XACME_FLOW_ERROR_ACCOUNT,
				"acme client eab hmac invalid base64url");
			goto Done;
		}
		sJwk = xacmeJwkEcJson(&pClient->AccountKey);
		if(sJwk != NULL)
		{
			sEab = xacmeJwsEabHs256(
				pAccount->Eab.sKid, pClient->sNewAccount,
				(xstrview){ sJwk, strlen(sJwk) }, Mac, iMacSize);
		}
		xrtFree(sJwk);
		if(sEab == NULL)
		{
			xrtBufferUnit(&Payload);
			goto Done;
		}
		if(!xrtBufferAppend(
				&Payload, XRT_BYTES_LITERAL(",\"externalAccountBinding\":")) ||
			!xrtBufferAppend(
				&Payload,
				(xbytesview){ (const uint8*)sEab, strlen(sEab) }))
		{
			xrtFree(sEab);
			xrtBufferUnit(&Payload);
			goto Done;
		}
		xrtFree(sEab);
	}
	if(!xrtBufferAppendByte(&Payload, (uint8)'}'))
	{
		xrtBufferUnit(&Payload);
		goto Done;
	}
	if(!xacmeFlowPost(
		pClient, pClient->sNewAccount,
		(xstrview){ (cstr)Payload.Data, Payload.Size }, false, &R, 0u))
	{
		xrtBufferUnit(&Payload);
		goto Done;
	}
	xrtBufferUnit(&Payload);
	if((R.iStatus != 200u) && (R.iStatus != 201u))
	{
		char sDetail[160];
		snprintf(sDetail, sizeof(sDetail),
			"acme new-account status=%u body=%.100s",
			(unsigned)R.iStatus,
			(R.sBody != NULL) ? R.sBody : "");
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT, sDetail);
		goto Done;
	}
	if((R.sLocation == NULL) ||
		(R.sLocation[0] == '\0') ||
		!xacmeFlowCopyText(
			pClient->sKid, sizeof(pClient->sKid), R.sLocation))
	{
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT,
			"acme client new-account missing location");
		goto Done;
	}
	xrtValueRelease(pRoot);
	pRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_ACCOUNT,
		"acme client account json invalid");
	xacmeHttpResponseUnit(&R);
	if(pRoot == NULL) goto Done;
	{
		xacmeflowurl Status;
		if(!xacmeJsonValueText(pRoot, "status", &Status) ||
			strcmp(Status.sData, "valid") != 0)
		{
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT,
				"acme client account is not valid");
			goto Done;
		}
	}
	bOk = true;

Done:
	if(pRoot != NULL)
	{
		xrtValueRelease(pRoot);
	}
	if(!bOk)
	{
		/* 先保留首个根因，再拆传输（Unit 可能覆盖线程错误）。 */
		xerror* pFirst = xrtErrorRef(xrtGetError());
		/* 失败构造尚未交付拥有者，回滚不复用已经耗尽的请求预算。 */
		if(pClient->Http.uTimeoutUs < UINT64_C(30000000))
			pClient->Http.uTimeoutUs = UINT64_C(30000000);
		xacmeHttpUnit(&pClient->Http);
		if(pFirst != NULL)
		{
			xrtSetErrorTake(pFirst);
		}
	}
	return bOk;
}

bool xacmeClientUnit(xacmeclient* pClient)
{
	if(pClient == NULL)
	{
		return true;
	}
	if(!xacmeHttpUnit(&pClient->Http)) return false;
	xrtSecureZero(pClient, sizeof(*pClient));
	return true;
}

str xacmeClientAccountPem(const xacmeclient* pClient)
{
	if(pClient == NULL)
	{
		return NULL;
	}
	return xacmeKeyPemWrite(&pClient->AccountKey);
}

bool xacmeClientIssue(
	xacmeclient* pClient, const xstrview* pDomains, size_t iDomainCount,
	const struct xacmednsprovider* pDns, xacmeissuegrant* pOut,
	bool bAlt)
{
	xbuffer Payload;
	xacmehttpresponse R;
	xvalue* pRoot = NULL;
	bool bResult = false;
	xerror* pUncertainError = NULL;
	xacmeflowurl Finalize;
	xacmeflowurl OrderUrl;
	Finalize.sData[0] = 0;
	xacmeflowurl Certificate;
	size_t i;
	bool bOk = false;
	xacmeflowcertpublic CertPublic = {0};
	bytes pLeafDer = NULL;
	size_t iLeafDerSize = 0u;

	if(pOut != NULL) memset(pOut, 0, sizeof(*pOut));
	if((pClient == NULL) || (pDomains == NULL) || (iDomainCount == 0u) ||
		(pDns == NULL) || (pDns->Add == NULL) || (pDns->Remove == NULL) ||
		(pOut == NULL) ||
		!xrtAcmeDnsProviderValidate(pDns))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme issue requires client, domains, dns provider and output");
		return false;
	}
	memset(pOut, 0, sizeof(*pOut));
	/* RFC 8555 section 11.1 forbids reusing a known account key in a CSR.
	 * Check at issue time too: account rollover may change this relationship. */
	if((pClient->pCertKey != NULL) &&
		(pClient->pCertKey->Kind == XACME_CERT_KEY_ES256) &&
		(memcmp(pClient->pCertKey->Ec.Public, pClient->AccountKey.Public,
			sizeof(pClient->AccountKey.Public)) == 0))
	{
		xacmeFlowError(XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme certificate key must differ from the account key");
		return false;
	}
	/* 总预算打点：uIssueTimeoutUs 非零时本次 Issue 全程受限。 */
	pClient->bIssueDeadline = (pClient->uIssueTimeoutUs != 0u);
	pClient->IssueDeadline = xrtClock() + pClient->uIssueTimeoutUs;

	/* 1. 新订单；identifier 用完整域名（通配符原样：*.example.com 是
	   独立 identifier，CA 依此返回通配符授权）。去重按完整字符串——
	   RFC 8555 要求订单 identifier 与 CSR SAN 严格一致。 */
	xacmeflowurl Bases[16];
	size_t iBaseCount = 0u;
	/* TXT 统一延后清理：同名多授权（裸域 + 通配符）场景下，先删后加
	   会让 LE 次级验证器的递归缓存仍见旧值而判 Incorrect TXT；多值
	   并存是 LE 明确允许的形态，全部验证完成后再统一 Remove。 */
	struct
	{
		str sFqdn;
		str sTxt;
	} TxtPending[16];
	size_t iTxtPending = 0u;
	struct
	{
		xacmeflowurl Authz;
		xacmeflowurl Challenge;
		str sFqdn;
		str sTxt;
	} Work[16];
	size_t iWorkCount = 0u;
	xrtBufferInit(&Payload);
	for(i = 0; i < iDomainCount; i++)
	{
		xstrview Domain = pDomains[i];
		size_t j;
		bool bDup = false;
		if((Domain.Data == NULL) || (Domain.Size == 0u) ||
			(Domain.Size >= sizeof(Bases[0].sData)) ||
			(memchr(Domain.Data, '\0', Domain.Size) != NULL))
		{
			xacmeFlowError(
				XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
				"acme issue domain list invalid");
			goto Done;
		}
		for(j = 0; j < iBaseCount; j++)
		{
			if((Bases[j].iSize == Domain.Size) &&
				(memcmp(Bases[j].sData, Domain.Data, Domain.Size) == 0))
			{
				bDup = true;
				break;
			}
		}
		if(bDup)
		{
			continue;
		}
		if(iBaseCount >= sizeof(Bases) / sizeof(Bases[0]))
		{
			xacmeFlowError(XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
				"acme issue has too many distinct domains");
			goto Done;
		}
		memcpy(Bases[iBaseCount].sData, Domain.Data, Domain.Size);
		Bases[iBaseCount].sData[Domain.Size] = 0;
		Bases[iBaseCount].iSize = Domain.Size;
		iBaseCount++;
	}
	if(!xrtBufferAppend(
		&Payload, XRT_BYTES_LITERAL("{\"identifiers\":[")))
	{
		goto Done;
	}
	for(i = 0; i < iBaseCount; i++)
	{
		if((i > 0u) && !xrtBufferAppendByte(&Payload, (uint8)','))
		{
			goto Done;
		}
		if(!xrtBufferAppend(
				&Payload, XRT_BYTES_LITERAL("{\"type\":\"dns\",\"value\":")) ||
			!xacmeFlowJsonQuoteAppend(
				&Payload, (xstrview){ Bases[i].sData, Bases[i].iSize }) ||
			!xrtBufferAppendByte(&Payload, (uint8)'}'))
		{
			goto Done;
		}
	}
	if(!xrtBufferAppend(&Payload, XRT_BYTES_LITERAL("]}")))
	{
		goto Done;
	}
	if(!xacmeFlowPost(
		pClient, pClient->sNewOrder,
		(xstrview){ (cstr)Payload.Data, Payload.Size }, true, &R, 0u))
	{
		goto Done;
	}
	if((R.iStatus != 201u) || (R.sLocation == NULL) || (R.sLocation[0] == 0) ||
		!xacmeFlowCopyText(
			OrderUrl.sData, sizeof(OrderUrl.sData), R.sLocation))
	{
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ORDER,
			"acme issue new-order response invalid");
		goto Done;
	}
	OrderUrl.iSize = strlen(OrderUrl.sData);
	pRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_ORDER,
		"acme issue new-order json invalid");
	xacmeHttpResponseUnit(&R);
	if(pRoot == NULL) goto Done;

/* 2. 逐授权域处理 dns-01（两段式：先铺全部 TXT 再统一触发）。
 *
 * 同名多授权（裸域 + 通配符共用一个 TXT 属主）场景下，逐个
 * “加 TXT→触发→轮询→删 TXT” 会让 LE 次级验证器的递归缓存仍持有
 * 只含旧值的 RRset（TTL 未过不再回源），次级视角只见旧值判
 * Incorrect TXT。两段式保证触发任何挑战时全部 TXT 值已并存，
 * 首次回源查询即拿到完整集合；TXT 统一延后到 Done 清理。 */
{
	xvalue* pAuthz = xrtValueObjectGet(
		pRoot, XRT_STR_LITERAL("authorizations"));
	size_t iCount = (pAuthz != NULL) ? xrtValueCount(pAuthz) : 0u;
	char sThumb[44];
	size_t k;
	if((pAuthz == NULL) || !xrtValueIs(pAuthz, XVALUE_ARRAY) ||
		(iCount != iBaseCount))
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ORDER,
			"acme issue order authorizations invalid");
		goto Done;
	}
	if(!xacmeJwkEcThumbprint(&pClient->AccountKey, sThumb)) goto Done;
	/* Pass A：逐授权取 dns-01 挑战，铺 TXT 并确认传播。 */
	for(i = 0; i < iCount; i++)
	{
		xvalue* pUrl = xrtValueArrayGet(pAuthz, i);
		xstrview UrlText;
		xacmeflowurl Authz;
		xacmehttpresponse A;
		xvalue* pAuthRoot = NULL;
		xacmeflowurl Token;
		xvalue* pChallenges;
		size_t j;
		bool bFound = false;
		if((pUrl == NULL) ||
			!xrtValueGetString(pUrl, &UrlText) ||
			(UrlText.Size == 0u) || (UrlText.Size >= sizeof(Authz.sData)) ||
			(memchr(UrlText.Data, '\0', UrlText.Size) != NULL))
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
				"acme issue authorization url invalid");
			goto Done;
		}
		memcpy(Authz.sData, UrlText.Data, UrlText.Size);
		Authz.sData[UrlText.Size] = '\0';
		Authz.iSize = UrlText.Size;
		if(!xacmeFlowPostAsGet(pClient, Authz.sData, &A))
		{
			goto Done;
		}
		if(A.iStatus != 200u)
		{
			xacmeHttpResponseUnit(&A);
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
				"acme authorization response status invalid");
			goto Done;
		}
		pAuthRoot = xacmeFlowResponseObject(&A, XACME_FLOW_ERROR_CHALLENGE,
			"acme authorization json invalid");
		if(pAuthRoot == NULL)
		{
			xacmeHttpResponseUnit(&A);
			goto Done;
		}
		{
			xacmeflowurl AuthzStatus;
			bool bValid;
			if(!xacmeJsonValueText(pAuthRoot, "status", &AuthzStatus))
			{
				xrtValueRelease(pAuthRoot);
				xacmeHttpResponseUnit(&A);
				xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
					"acme authorization status invalid");
				goto Done;
			}
			bValid = strcmp(AuthzStatus.sData, "valid") == 0;
			if(!bValid && strcmp(AuthzStatus.sData, "pending") != 0)
			{
				xrtValueRelease(pAuthRoot);
				xacmeHttpResponseUnit(&A);
				xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
					"acme authorization is not pending or valid");
				goto Done;
			}
			pChallenges = xrtValueObjectGet(
				pAuthRoot, XRT_STR_LITERAL("challenges"));
			if(getenv("XACME_DEBUG"))
			{
				xacmeflowurl AuthzIdent;
				xvalue* pDbgIdent = xrtValueObjectGet(
					pAuthRoot, XRT_STR_LITERAL("identifier"));
				printf("[dbg] authz[%u] status=%s ident=%s challenges=%zu\n",
					(unsigned)i,
					(xacmeJsonValueText(pAuthRoot, "status", &AuthzStatus)) ?
						AuthzStatus.sData : "?",
					((pDbgIdent != NULL) &&
						xacmeJsonValueText(
							pDbgIdent, "value", &AuthzIdent)) ?
						AuthzIdent.sData : "?",
					((pChallenges != NULL) &&
							xrtValueIs(pChallenges, XVALUE_ARRAY)) ?
							xrtValueCount(pChallenges) : 0u);
			}
			if(bValid)
			{
				/* 已有效的授权（如复用）视为已解决。 */
				xrtValueRelease(pAuthRoot);
				xacmeHttpResponseUnit(&A);
				continue;
			}
			for(j = 0; (pChallenges != NULL) &&
				xrtValueIs(pChallenges, XVALUE_ARRAY) &&
				(j < xrtValueCount(pChallenges)); j++)
			{
				xvalue* pChallenge = xrtValueArrayGet(pChallenges, j);
				xacmeflowurl Type;
				xacmeflowurl ChallengeUrl;
				if((pChallenge == NULL) ||
					!xrtValueIs(pChallenge, XVALUE_OBJECT) ||
					!xacmeJsonValueText(pChallenge, "type", &Type) ||
					(strcmp(Type.sData, "dns-01") != 0))
				{
					if(getenv("XACME_DEBUG"))
					{
						printf("[dbg] authz[%u] chal[%u] skip type=%s\n",
							(unsigned)i, (unsigned)j,
							xacmeJsonValueText(pChallenge, "type", &Type) ?
								Type.sData : "?");
					}
					continue;
				}
				if(!xacmeJsonValueText(
						pChallenge, "url", &ChallengeUrl) ||
					!xacmeJsonValueText(pChallenge, "token", &Token))
				{
					if(getenv("XACME_DEBUG"))
					{
						printf("[dbg] dns-01 missing url/token\n");
					}
					continue;
				}
				/* keyAuthz = token '.' thumbprint；TXT = b64url(sha256) */
				{
					uint8 Digest[XRT_SHA256_SIZE];
					char sKeyAuthz[512];
					size_t iKeyAuthz = Token.iSize + 1u + 43u;
					static const xbase64config B64Url = {
						NULL,
						XBASE64_URL | XBASE64_NO_PADDING
					};
					str sTxt;
					str sFqdn = NULL;
					if(iKeyAuthz >= sizeof(sKeyAuthz))
					{
						if(getenv("XACME_DEBUG")) printf("[dbg] keyauthz too long\n");
						continue;
					}
					memcpy(sKeyAuthz, Token.sData, Token.iSize);
					sKeyAuthz[Token.iSize] = '.';
					memcpy(sKeyAuthz + Token.iSize + 1u, sThumb, 43u);
					if(!xrtSha256(sKeyAuthz, iKeyAuthz, Digest) ||
						((sTxt = xrtBase64EncodeNew(
							Digest, sizeof(Digest), &B64Url)) == NULL))
					{
						xrtValueRelease(pAuthRoot);
						xacmeHttpResponseUnit(&A);
						goto Done;
					}
					/* TXT 属主 = _acme-challenge.<授权 identifier.value>
					   （通配符授权返回的 value 已是基础域；协议正源，
					   不依赖订单 identifier 的位置映射）。 */
					{
						xbuffer Fqdn;
						xvalue* pIdent = xrtValueObjectGet(
							pAuthRoot, XRT_STR_LITERAL("identifier"));
						xvalue* pIdentValue = (pIdent != NULL) ?
							xrtValueObjectGet(
								pIdent, XRT_STR_LITERAL("value")) : NULL;
						xstrview Domain = { NULL, 0 };
						bool bFqdnOk;
						if((pIdentValue != NULL) &&
							xrtValueGetString(pIdentValue, &Domain) &&
							(Domain.Size != 0u) &&
							(Domain.Size < sizeof(Bases[0].sData)) &&
							(memchr(Domain.Data, '\0', Domain.Size) == NULL))
						{
							/* 协议正源路径 */
						}
						else
						{
							xrtFree(sTxt);
							xrtValueRelease(pAuthRoot);
							xacmeHttpResponseUnit(&A);
							xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
								"acme authorization identifier invalid");
							goto Done;
						}
						xrtBufferInit(&Fqdn);
						bFqdnOk = xrtBufferAppend(
							&Fqdn,
							XRT_BYTES_LITERAL("_acme-challenge.")) &&
							xrtBufferAppend(
								&Fqdn,
								(xbytesview){
									(const uint8*)Domain.Data,
									Domain.Size }) &&
							xrtBufferAppendByte(&Fqdn, 0u);
						sFqdn = bFqdnOk ? (str)Fqdn.Data : NULL;
						if(!bFqdnOk)
						{
							xrtBufferUnit(&Fqdn);
						}
					}
					bool bCanAdd = (sFqdn != NULL) &&
						(iWorkCount <
							(sizeof(Work) / sizeof(Work[0])));
					if(!bCanAdd ||
						!xacmeFlowDnsAdd(pDns, sFqdn, sTxt))
					{
						if(xacmeFlowDnsAddTerminal(xrtGetError()))
						{
							pUncertainError = xrtErrorRef(xrtGetError());
						}
						if(getenv("XACME_DEBUG")) printf("[dbg] dns add failed fqdn=%s err=%d\n", sFqdn ? sFqdn : "null", (int)xrtErrorKind(xrtGetError()));
						xrtFree(sFqdn);
						xrtFree(sTxt);
						if(pUncertainError != NULL)
							break;
						continue;
					}
					/* 传播确认：全部 TXT 就位后才进入 Pass B 统一触发。 */
					xacmeFlowWaitPropagate(pClient, pDns, sFqdn, sTxt);
					memcpy(&Work[iWorkCount].Authz, &Authz,
						sizeof(xacmeflowurl));
					memcpy(&Work[iWorkCount].Challenge, &ChallengeUrl,
						sizeof(xacmeflowurl));
					Work[iWorkCount].sFqdn = sFqdn;
					Work[iWorkCount].sTxt = sTxt;
					iWorkCount++;
					if(xrtErrorKind(xrtGetError()) == XERR_MEMORY)
					{
						xrtValueRelease(pAuthRoot);
						xacmeHttpResponseUnit(&A);
						goto Done;
					}
					bFound = true;
					break;
				}
			}
			xrtValueRelease(pAuthRoot);
			xacmeHttpResponseUnit(&A);
			if(!bFound)
			{
				if(pUncertainError == NULL)
					xacmeFlowError(
						XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
						"acme issue no dns-01 challenge solved");
				goto Done;
			}
		}
		}
		/* Pass B：全部 TXT 并存的前提下统一触发并轮询。 */
		for(k = 0; k < iWorkCount; k++)
		{
			xacmehttpresponse T;
			str sStatus = NULL;
			bool bValidNow = false;
			if(!xacmeFlowPost(
				pClient, Work[k].Challenge.sData,
				(xstrview){ "{}", 2u }, true, &T, 0u))
			{
				goto Done;
			}
			if(T.iStatus != 200u)
			{
				xacmeHttpResponseUnit(&T);
				xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
					"acme challenge response status invalid");
				goto Done;
			}
			xacmeHttpResponseUnit(&T);
			sStatus = xacmeFlowWaitStatus(pClient, Work[k].Authz.sData, NULL);
			if(sStatus == NULL) goto Done;
			bValidNow =
				(strcmp(sStatus, "valid") == 0);
			if(!bValidNow)
			{
				char sChallengeError[200];
				char sDetail[320];
				xerror* pE = xrtErrorRef(xrtGetError());
				xacmeFlowChallengeDetail(
					pClient, Work[k].Authz.sData, sChallengeError,
					sizeof(sChallengeError));
				if(getenv("XACME_DEBUG"))
				{
					printf("[dbg] authz not valid: "
						"status=%s err=%d %s challenge=%.160s\n",
						(sStatus != NULL) ? sStatus : "null",
						(int)xrtErrorKind(pE),
						(xrtErrorMessage(pE) != NULL) ?
							xrtErrorMessage(pE) : "-",
						sChallengeError);
				}
				snprintf(sDetail, sizeof(sDetail),
					"acme issue authorization status=%.32s "
					"challenge=%.180s",
					(sStatus != NULL) ? sStatus : "null",
					sChallengeError);
				xrtErrorFree(pE);
				xrtFree(sStatus);
				xacmeFlowError(
					XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE, sDetail);
				goto Done;
			}
			xrtFree(sStatus);
		}
		/* 工作表移交 TXT 统一清理（表满退化为立即删）。 */
		for(k = 0; k < iWorkCount; k++)
		{
			if(iTxtPending <
				(sizeof(TxtPending) / sizeof(TxtPending[0])))
			{
				TxtPending[iTxtPending].sFqdn = Work[k].sFqdn;
				TxtPending[iTxtPending].sTxt = Work[k].sTxt;
				iTxtPending++;
				Work[k].sFqdn = NULL;
				Work[k].sTxt = NULL;
			}
			else
			{
				xacmeFlowDnsRemove(
					pDns, Work[k].sFqdn, Work[k].sTxt);
				xrtFree(Work[k].sFqdn);
				xrtFree(Work[k].sTxt);
				Work[k].sFqdn = NULL;
				Work[k].sTxt = NULL;
			}
		}
	}
	xrtValueRelease(pRoot);
	pRoot = NULL;

	/* 3. 订单 ready → finalize。 */
	{
		str sStatus = xacmeFlowWaitStatus(pClient, OrderUrl.sData, &Finalize);
		if(sStatus == NULL) goto Done;
		bool bReady = (sStatus != NULL) &&
			(strcmp(sStatus, "ready") == 0);
		xrtFree(sStatus);
		if(!bReady)
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_ORDER,
				"acme issue order not ready");
			goto Done;
		}
	}
	/* CSR（首个域名为 CN；SAN 全量，含通配符原样）。 */
	{
		xacmecsrconfig Csr;
		xstrview CsrDomains[16];
		xbuffer CsrDer;
		str sCsrB64;
		xacmecertkey StackKey = {0};
		xacmecertkey* pUseKey = pClient->pCertKey;
		static const xbase64config B64Url = {
			NULL, XBASE64_URL | XBASE64_NO_PADDING };
		bool bCsrOk;
		for(i = 0u; i < iBaseCount; i++)
			CsrDomains[i] = (xstrview){ Bases[i].sData, Bases[i].iSize };
		Csr.CommonName = CsrDomains[0];
		Csr.Domains = CsrDomains;
		Csr.DomainCount = iBaseCount;
		xrtBufferInit(&CsrDer);
		if(pUseKey == NULL)
		{
			/* 无宿主密钥：生成一次性 ES256（证书密钥独立于账户
			   密钥，CA 普遍拒绝复用账户钥）。 */
			StackKey.Kind = XACME_CERT_KEY_ES256;
			bCsrOk = xacmeEs256Generate(&StackKey.Ec);
			pUseKey = &StackKey;
		}
		else
		{
			bCsrOk = true;
		}
		if(bCsrOk)
		{
			xacmeFlowCertPublic(pUseKey, &CertPublic);
			bCsrOk = xacmeCsrBuild(pUseKey, &Csr, &CsrDer);
		}
		if(bCsrOk)
		{
			/* 私钥随产物导出（没有它证书不可用）。 */
			pOut->sKeyPem = xacmeCertKeyPemWrite(pUseKey);
			bCsrOk = (pOut->sKeyPem != NULL);
		}
		/* 栈上密钥副本立即擦除。 */
		xacmeCertKeyUnit(&StackKey);
		sCsrB64 = bCsrOk ? xrtBase64EncodeNew(
			CsrDer.Data, CsrDer.Size, &B64Url) : NULL;
		xrtBufferUnit(&CsrDer);
		if(sCsrB64 == NULL)
		{
			goto Done;
		}
		xrtBufferClear(&Payload);
		bOk = xrtBufferAppend(&Payload, XRT_BYTES_LITERAL("{\"csr\":\"")) &&
			xrtBufferAppend(
				&Payload,
				(xbytesview){
					(const uint8*)sCsrB64, strlen(sCsrB64) }) &&
				xrtBufferAppend(&Payload, XRT_BYTES_LITERAL("\"}"));
		xrtFree(sCsrB64);
		if(!bOk)
		{
			goto Done;
		}
		bOk = false;
	}
	if(!xacmeFlowPost(
		pClient, Finalize.sData,
		(xstrview){ (cstr)Payload.Data, Payload.Size }, true, &R, 0u))
	{
		goto Done;
	}
	if((R.iStatus != 200u) && (R.iStatus != 201u))
	{
		char sDetail[300];
		snprintf(sDetail, sizeof(sDetail),
			"acme finalize status=%u body=%.200s",
			(unsigned)R.iStatus,
			(R.sBody != NULL) ? R.sBody : "");
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_FINALIZE, sDetail);
		goto Done;
	}
	xacmeHttpResponseUnit(&R);

	/* 4. 订单 valid → 证书 URL → 下载链。 */
	{
		str sStatus = xacmeFlowWaitStatus(pClient, OrderUrl.sData, NULL);
		if(sStatus == NULL) goto Done;
		bool bValid = (sStatus != NULL) && (strcmp(sStatus, "valid") == 0);
		char sDetail[320];
		snprintf(sDetail, sizeof(sDetail),
			"acme order not valid after finalize (state=%s)",
			(sStatus != NULL) ? sStatus : "null");
		xrtFree(sStatus);
		if(!bValid)
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_FINALIZE, sDetail);
			goto Done;
		}
	}
	if(!xacmeFlowPostAsGet(pClient, OrderUrl.sData, &R))
	{
		goto Done;
	}
	{
		if(R.iStatus != 200u)
		{
			xacmeHttpResponseUnit(&R);
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CERTIFICATE,
				"acme certificate order response status invalid");
			goto Done;
		}
		xvalue* pOrderRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_CERTIFICATE,
			"acme certificate order json invalid");
		if(pOrderRoot == NULL)
		{
			xacmeHttpResponseUnit(&R);
			goto Done;
		}
		bool bCertUrl =
			xacmeJsonValueText(pOrderRoot, "certificate", &Certificate);
		if(pOrderRoot != NULL)
		{
			xrtValueRelease(pOrderRoot);
		}
		xacmeHttpResponseUnit(&R);
		if(!bCertUrl)
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_CERTIFICATE,
				"acme issue order missing certificate url");
			goto Done;
		}
	}
	if(!xacmeFlowPostAsGet(pClient, Certificate.sData, &R))
	{
		goto Done;
	}
	if(!xacmeFlowCertificate(&R, &CertPublic, Bases, iBaseCount,
		(xbytesview){ NULL, 0u }, &pLeafDer, &iLeafDerSize))
	{
		xacmeHttpResponseUnit(&R);
		goto Done;
	}
	pOut->sFullchainPem = R.sBody;
	R.sBody = NULL;
	/* 备用链：Link 头携带 rel="alternate" 的第二下载地址。 */
	if(bAlt && (R.sLink != NULL))
	{
		char sAlternate[512];
		char sReference[512];
		if(xacmeFlowLinkAlternate(R.sLink, sReference, sizeof(sReference)) &&
			xacmeFlowAlternateUrl(Certificate.sData, sReference,
				sAlternate, sizeof(sAlternate)))
		{
			xacmehttpresponse Alt;
			if(xacmeFlowPostAsGet(pClient, sAlternate, &Alt) &&
				xacmeFlowCertificate(&Alt, &CertPublic, Bases, iBaseCount,
					(xbytesview){ pLeafDer, iLeafDerSize }, NULL, NULL))
			{
				xrtFree(pOut->sFullchainPem);
				pOut->sFullchainPem = Alt.sBody;
				Alt.sBody = NULL;
			}
			/* 备用链失败不阻断：主链始终有效。 */
			xacmeHttpResponseUnit(&Alt);
			xrtClearError();
		}
		else xrtClearError();
	}
	xacmeHttpResponseUnit(&R);
	bResult = true;

Done:
	if(pUncertainError == NULL && !bResult)
		pUncertainError = xrtErrorRef(xrtGetError());
	xrtFree(pLeafDer);
	/* TXT 统一清理：全部授权验证完成（或中途失败）后统一 Remove，
	   期间多值并存规避次级验证器缓存竞态。 */
	{
		size_t k;
		for(k = 0; k < iWorkCount; k++)
		{
			if(Work[k].sFqdn == NULL)
				continue;
			xacmeFlowDnsRemove(pDns, Work[k].sFqdn, Work[k].sTxt);
			xrtFree(Work[k].sFqdn);
			xrtFree(Work[k].sTxt);
		}

		for(k = 0; k < iTxtPending; k++)
		{
			xacmeFlowDnsRemove(
				pDns, TxtPending[k].sFqdn, TxtPending[k].sTxt);
			xrtFree(TxtPending[k].sFqdn);
			xrtFree(TxtPending[k].sTxt);
		}
	}
	if(pRoot != NULL)
	{
		xrtValueRelease(pRoot);
	}
	xrtBufferUnit(&Payload);
	/* Cleanup must not replace the primary failure with a DNS removal error. */
	if(pUncertainError != NULL)
		xrtSetErrorTake(pUncertainError);
	if(!bResult)
	{
		/* The common grant destructor also erases the exported private key. */
		xrtAcmeGrantUnit(pOut);
	}
	return bResult;
}

#endif

#if defined(XACME_FEATURE_ACME_STORE)
bool xacmeClientIssueStored(
	xacmeclient* pClient, const xstrview* pDomains, size_t iDomainCount,
	const struct xacmednsprovider* pDns, cstr sStoreRoot,
	int iRenewalDays, xacmeissuegrant* pOut, bool* pbRenewed)
{
	bool bNeed = true;
	char sPrimary[256];
	if((pClient == NULL) || (pDomains == NULL) || (iDomainCount == 0u) ||
		(pbRenewed == NULL) || (pOut == NULL) ||
		(pDomains[0].Data == NULL) || (pDomains[0].Size == 0u) ||
		(pDomains[0].Size >= sizeof(sPrimary)) ||
		(memchr(pDomains[0].Data, '\0', pDomains[0].Size) != NULL))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme issue stored requires client, domains and outputs");
		return false;
	}
	memset(pOut, 0, sizeof(*pOut));
	*pbRenewed = false;
	memcpy(sPrimary, pDomains[0].Data, pDomains[0].Size);
	sPrimary[pDomains[0].Size] = 0;
	if((sStoreRoot == NULL) || (sStoreRoot[0] == 0u))
	{
		sStoreRoot = ".";
	}
	if(!xrtAcmeStoreNeedRenew(
			sStoreRoot, sPrimary, iRenewalDays, &bNeed))
	{
		return false;
	}
	if(!bNeed)
	{
		return xrtAcmeStoreLoadGrant(sStoreRoot, sPrimary, pOut);
	}
	if(!xacmeClientIssue(
			pClient, pDomains, iDomainCount, pDns,
			pOut, false))
	{
		return false;
	}
	if(!xrtAcmeStoreSaveGrant(
			sStoreRoot, sPrimary, pOut, pClient->sDirectoryUrl))
	{
		/* 落盘失败不作废已签证书；报告错误由调用方权衡。 */
		xrtAcmeGrantUnit(pOut);
		return false;
	}
	*pbRenewed = true;
	return true;
}
#endif

#if defined(XACME_FEATURE_ACME_FLOW)
bool xacmeClientRevoke(xacmeclient* pClient, cstr sCertPem, int iReason)
{
	xacmehttpresponse R;
	xpemblock Block;
	size_t iDerSize = 0u;
	bytes pDer = NULL;
	str sCertB64 = NULL;
	xbuffer Payload;
	static const xbase64config B64Url = {
		NULL, XBASE64_URL | XBASE64_NO_PADDING };
	bool bOk = false;

	if((pClient == NULL) || (sCertPem == NULL) || (sCertPem[0] == '\0'))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme revoke requires client and certificate");
		return false;
	}
	if(pClient->sRevokeCert[0] == '\0')
	{
		xacmeFlowError(
			XERR_UNSUPPORTED, XACME_FLOW_ERROR_PROTOCOL,
			"acme revoke requires directory revokeCert endpoint");
		return false;
	}
	if(!xrtPemFind(sCertPem, strlen(sCertPem), "CERTIFICATE", &Block) ||
		((pDer = xrtPemDecodeNew(&Block, &iDerSize)) == NULL))
	{
		return false;
	}
	sCertB64 = xrtBase64EncodeNew(pDer, iDerSize, &B64Url);
	xrtFree(pDer);
	if(sCertB64 == NULL)
	{
		return false;
	}
	xrtBufferInit(&Payload);
	bOk = xrtBufferAppend(&Payload, XRT_BYTES_LITERAL("{\"certificate\":\"")) &&
		xrtBufferAppend(&Payload,
			(xbytesview){ (const uint8*)sCertB64, strlen(sCertB64) }) &&
		xrtBufferAppend(&Payload, XRT_BYTES_LITERAL("\""));
	if(bOk && (iReason >= 0))
	{
		char sReason[24];
		snprintf(sReason, sizeof(sReason), ",\"reason\":%d", iReason);
		bOk = xrtBufferAppend(&Payload,
			(xbytesview){ (const uint8*)sReason, strlen(sReason) });
	}
	if(bOk)
	{
		bOk = xrtBufferAppendByte(&Payload, (uint8)'}');
	}
	if(bOk)
	{
		bOk = xacmeFlowPost(pClient, pClient->sRevokeCert,
			(xstrview){ (cstr)Payload.Data, Payload.Size }, true, &R, 0u);
	}
	xrtBufferUnit(&Payload);
	xrtFree(sCertB64);
	if(!bOk)
	{
		return false;
	}
	/* 200 = 已吊销；400 + alreadyRevoked 视为幂等成功。 */
	if(R.iStatus == 200u)
	{
		xacmeHttpResponseUnit(&R);
		return true;
	}
	if(R.iStatus == 400u)
	{
		xacmeflowurl Type;
		if(!xacmeFlowProblemType(&R, &Type))
		{
			xacmeHttpResponseUnit(&R);
			return false;
		}
		if(strcmp(Type.sData, "urn:ietf:params:acme:error:alreadyRevoked") == 0)
		{
			xacmeHttpResponseUnit(&R);
			return true;
		}
	}
	{
		char sDetail[240];
		snprintf(sDetail, sizeof(sDetail),
			"acme revoke status=%u body=%.160s", (unsigned)R.iStatus,
			(R.sBody != NULL) ? R.sBody : "");
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_PROTOCOL, sDetail);
	}
	return false;
}
#endif

#if defined(XACME_FEATURE_ACME_FLOW)
typedef enum xacmerolloverlookup {
	XACME_ROLLOVER_LOOKUP_UNKNOWN = 0,
	XACME_ROLLOVER_LOOKUP_MATCH,
	XACME_ROLLOVER_LOOKUP_ABSENT,
	XACME_ROLLOVER_LOOKUP_OTHER_ACCOUNT
} xacmerolloverlookup;

/* RFC 8555 §7.3.1：只查询新钥是否已绑定当前账户，不创建账户。 */
static xacmerolloverlookup xacmeFlowRolloverLookup(
	xacmeclient* pClient, const xacmees256key* pNewKey)
{
	static const char* sPayload = "{\"onlyReturnExisting\":true}";
	uint32 uAttempt;
	for(uAttempt = 0u; uAttempt < 3u; uAttempt++)
	{
		xacmejwsheader H;
		xacmehttpresponse R;
		str sToken;
		xacmerolloverlookup Result = XACME_ROLLOVER_LOOKUP_UNKNOWN;
		bool bBadNonce;
		if(!xacmeFlowNewNonce(pClient))
			return Result;
		H.Nonce = (xstrview){
			pClient->sNonce, strlen(pClient->sNonce) };
		H.Url = (xstrview){
			pClient->sNewAccount, strlen(pClient->sNewAccount) };
		H.Kid = (xstrview){ NULL, 0u };
		sToken = xacmeJwsEs256(pNewKey, &H,
			(xstrview){ sPayload, strlen(sPayload) });
		if(sToken == NULL)
			return Result;
		pClient->sNonce[0] = '\0';
		if(!xacmeHttpExchange(&pClient->Http, "POST",
				pClient->sNewAccount, "application/jose+json",
				(xstrview){ sToken, strlen(sToken) }, &R))
		{
			bool bRetry = xacmeFlowHttpUncertain(xrtGetError());
			xrtFree(sToken);
			if(bRetry && (uAttempt + 1u < 3u))
			{
				xrtClearError();
				continue;
			}
			return Result;
		}
		xrtFree(sToken);
		xacmeFlowTakeNonce(pClient, &R);
		bBadNonce = false;
		if(R.iStatus == 400u)
		{
			xacmeflowurl Type;
			if(!xacmeFlowProblemType(&R, &Type))
			{
				xacmeHttpResponseUnit(&R);
				return Result;
			}
			bBadNonce = strcmp(Type.sData, "urn:ietf:params:acme:error:badNonce") == 0;
			if(strcmp(Type.sData, "urn:ietf:params:acme:error:accountDoesNotExist") == 0)
				Result = XACME_ROLLOVER_LOOKUP_ABSENT;
		}
		else if((R.iStatus == 200u) && (R.sLocation != NULL) && R.sLocation[0])
		{
			xvalue* pRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_ACCOUNT,
				"acme rollover lookup account json invalid");
			xacmeflowurl Status;
			if(pRoot == NULL)
			{
				xacmeHttpResponseUnit(&R);
				return Result;
			}
			if(xacmeJsonValueText(pRoot, "status", &Status) && strcmp(Status.sData, "valid") == 0)
				Result = strcmp(R.sLocation, pClient->sKid) == 0 ?
					XACME_ROLLOVER_LOOKUP_MATCH : XACME_ROLLOVER_LOOKUP_OTHER_ACCOUNT;
			xrtValueRelease(pRoot);
		}
		xacmeHttpResponseUnit(&R);
		if(bBadNonce)
		{
			if(uAttempt + 1u == 3u || pClient->sNonce[0] == 0)
			{
				xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
					"acme rollover key lookup nonce retries exhausted");
				return XACME_ROLLOVER_LOOKUP_UNKNOWN;
			}
			continue;
		}
		if(Result != XACME_ROLLOVER_LOOKUP_UNKNOWN)
			xrtClearError();
		else
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT,
				"acme rollover key lookup response inconclusive");
		return Result;
	}
	xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
		"acme rollover key lookup retry exhausted");
	return XACME_ROLLOVER_LOOKUP_UNKNOWN;
}

bool xacmeClientRollover(
	xacmeclient* pClient, cstr sNewKeyPem, cstr sStoreRoot)
{
	xacmees256key NewKey;
	str sOldJwk = NULL;
	str sInner = NULL;
	str sOuter = NULL;
	xacmerolloverlookup Lookup;
	bool bOk = false;

	if((pClient == NULL) || (sNewKeyPem == NULL) ||
		(sNewKeyPem[0] == '\0'))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme rollover requires client and new key pem");
		return false;
	}
	if(pClient->sKeyChange[0] == '\0')
	{
		xacmeFlowError(
			XERR_UNSUPPORTED, XACME_FLOW_ERROR_PROTOCOL,
			"acme rollover requires directory keyChange endpoint");
		return false;
	}
	if(!xacmeKeyPemRead(sNewKeyPem, strlen(sNewKeyPem), &NewKey))
	{
		return false;
	}
	/* 进程重启后重试同一新钥时，先确认服务器是否已经换钥。 */
	Lookup = xacmeFlowRolloverLookup(pClient, &NewKey);
	if(Lookup == XACME_ROLLOVER_LOOKUP_MATCH)
		goto ApplyNewKey;
	if(Lookup != XACME_ROLLOVER_LOOKUP_ABSENT)
	{
		if(Lookup == XACME_ROLLOVER_LOOKUP_OTHER_ACCOUNT)
			xacmeFlowError(XERR_STATE, XACME_FLOW_ERROR_ACCOUNT,
				"acme rollover new key belongs to another account");
		goto Done;
	}
	sOldJwk = xacmeJwkEcJson(&pClient->AccountKey);
	if(sOldJwk == NULL)
	{
		goto Done;
	}
	/* RFC 8555 §7.3.5：内层由新钥签名并嵌新 JWK。 */
	{
		xbuffer Payload;
		xacmejwsheader Inner;
		xrtBufferInit(&Payload);
		if(!xrtBufferAppend(
				&Payload, XRT_BYTES_LITERAL("{\"account\":")) ||
			!xacmeFlowJsonQuoteAppend(
				&Payload,
				(xstrview){ pClient->sKid, strlen(pClient->sKid) }) ||
			!xrtBufferAppend(
				&Payload, XRT_BYTES_LITERAL(",\"oldKey\":")) ||
			!xrtBufferAppend(
				&Payload,
				(xbytesview){
					(const uint8*)sOldJwk, strlen(sOldJwk) }) ||
			!xrtBufferAppendByte(&Payload, (uint8)'}'))
		{
			xrtBufferUnit(&Payload);
			goto Done;
		}
		Inner.Nonce.Data = NULL;
		Inner.Nonce.Size = 0u;
		Inner.Url = (xstrview){
			pClient->sKeyChange, strlen(pClient->sKeyChange) };
		Inner.Kid = (xstrview){ NULL, 0u };
		sInner = xacmeJwsEs256(
			&NewKey, &Inner,
			(xstrview){ (cstr)Payload.Data, Payload.Size });
		xrtBufferUnit(&Payload);
	}
	if(sInner == NULL)
	{
		goto Done;
	}
	/* 外层由旧账户钥签名，保护头携带 kid + nonce；badNonce 时
	   只重建外层 JWS，内层无 nonce 可以复用。 */
	{
		uint32 uNonceRetry;
		bool bPosted = false;
		for(uNonceRetry = 0u; uNonceRetry < 3u; uNonceRetry++)
		{
			xacmehttpresponse R;
			xacmejwsheader Outer;
			if(!xacmeFlowNewNonce(pClient))
			{
				goto Done;
			}
			Outer.Nonce = (xstrview){
				pClient->sNonce, strlen(pClient->sNonce) };
			Outer.Url = (xstrview){
				pClient->sKeyChange, strlen(pClient->sKeyChange) };
			Outer.Kid = (xstrview){
				pClient->sKid, strlen(pClient->sKid) };
			xrtFree(sOuter);
			sOuter = xacmeJwsEs256(
				&pClient->AccountKey, &Outer,
				(xstrview){ sInner, strlen(sInner) });
			if(sOuter == NULL)
			{
				goto Done;
			}
			pClient->sNonce[0] = 0;
			if(!xacmeHttpExchange(
					&pClient->Http, "POST", pClient->sKeyChange,
					"application/jose+json",
					(xstrview){ sOuter, strlen(sOuter) }, &R))
			{
				if(xacmeFlowHttpUncertain(xrtGetError()))
				{
					xerror* pWriteError = xrtErrorRef(xrtGetError());
					Lookup = xacmeFlowRolloverLookup(pClient, &NewKey);
					if(Lookup == XACME_ROLLOVER_LOOKUP_MATCH)
					{
						xrtErrorFree(pWriteError);
						xrtClearError();
						goto ApplyNewKey;
					}
					xrtSetErrorTake(pWriteError);
				}
				goto Done;
			}
			xacmeFlowTakeNonce(pClient, &R);
			if(R.iStatus == 400u)
			{
				xacmeflowurl Type;
				if(!xacmeFlowProblemType(&R, &Type))
				{
					xacmeHttpResponseUnit(&R);
					goto Done;
				}
				if(strcmp(Type.sData, "urn:ietf:params:acme:error:badNonce") == 0)
				{
					xacmeHttpResponseUnit(&R);
					if(pClient->sNonce[0] == 0)
					{
						xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
							"acme rollover badNonce response missing fresh nonce");
						goto Done;
					}
					continue;
				}
			}
			if(R.iStatus != 200u)
			{
				char sDetail[220];
				snprintf(sDetail, sizeof(sDetail),
					"acme rollover status=%u body=%.150s",
					(unsigned)R.iStatus,
					(R.sBody != NULL) ? R.sBody : "");
				xacmeHttpResponseUnit(&R);
				xacmeFlowError(
					XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT, sDetail);
				goto Done;
			}
			xacmeHttpResponseUnit(&R);
			bPosted = true;
			break;
		}
		if(!bPosted)
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
				"acme rollover nonce retries exhausted");
			goto Done;
		}
	}
ApplyNewKey:
	/* 200 响应或只读账户查询证明新钥生效后，才替换内存与存储。 */
	xrtClearError();
	xrtSecureZero(&pClient->AccountKey, sizeof(pClient->AccountKey));
	pClient->AccountKey = NewKey;
	memset(&NewKey, 0, sizeof(NewKey));
	bOk = true;
	/* store 重存失败不能撤销远端换钥；内存继续使用新钥，返回失败
	   让调用方知道持久化尚未完成，并可用同一新钥重试对账。 */
	if((sStoreRoot != NULL) && (sStoreRoot[0] != 0))
	{
#if defined(XACME_FEATURE_ACME_STORE)
		str sNewAccountPem = xacmeKeyPemWrite(&pClient->AccountKey);
		bool bSaved = (sNewAccountPem != NULL) && xrtAcmeStoreSaveAccount(
			sStoreRoot, pClient->sDirectoryUrl, sNewAccountPem);
		xrtFree(sNewAccountPem);
		if(!bSaved)
		{
			bOk = false;
		}
#else
		xacmeFlowError(XERR_UNSUPPORTED, XACME_FLOW_ERROR_STORE,
			"acme rollover store support is not enabled");
		bOk = false;
#endif
	}

Done:
	/* 失败时擦除新钥副本。 */
	xrtSecureZero(&NewKey, sizeof(NewKey));
	xrtFree(sOldJwk);
	xrtFree(sInner);
	xrtFree(sOuter);
	return bOk;
}
#endif

#if defined(XACME_FEATURE_ACME_FLOW)
bool xacmeClientDeactivate(xacmeclient* pClient)
{
	xacmehttpresponse R;
	if(pClient == NULL)
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme deactivate requires client");
		return false;
	}
	if(pClient->sKid[0] == 0)
	{
		xacmeFlowError(
			XERR_STATE, XACME_FLOW_ERROR_ACCOUNT,
			"acme deactivate requires active account");
		return false;
	}
	if(!xacmeFlowPost(
			pClient, pClient->sKid,
			XRT_STR_LITERAL("{\"status\":\"deactivated\"}"), true, &R,
			0u))
	{
		return false;
	}
	/* 200 = 已停用或刚停用；幂等成功。 */
	if(R.iStatus == 200u)
	{
		xvalue* pRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_ACCOUNT,
			"acme deactivate account json invalid");
		xacmeflowurl Status;
		bool bDeactivated;
		xacmeHttpResponseUnit(&R);
		if(pRoot == NULL) return false;
		bDeactivated = xacmeJsonValueText(pRoot, "status", &Status) &&
			strcmp(Status.sData, "deactivated") == 0;
		xrtValueRelease(pRoot);
		if(!bDeactivated)
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT,
				"acme deactivate account is not deactivated");
		return bDeactivated;
	}
	{
		char sDetail[220];
		snprintf(sDetail, sizeof(sDetail),
			"acme deactivate status=%u body=%.150s", (unsigned)R.iStatus,
			(R.sBody != NULL) ? R.sBody : "");
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT, sDetail);
	}
	return false;
}
#endif

/* ---------------- 公开客户端 API（xrt/acme_client.h） ---------------- */

#if defined(XACME_FEATURE_ACME_FLOW)

void xrtAcmeClientConfigInit(xacmeclientconfig* pConfig)
{
	if(pConfig == NULL)
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme client config init requires config");
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
}

struct xacmeclient* xrtAcmeClientCreate(
	const xacmeclientconfig* pConfig)
{
	static const char* sDefaults[XACME_FLOW_RESOLVER_MAX] = {
		"223.5.5.5", "119.29.29.29", "8.8.8.8", NULL
	};
	const cstr* sResolvers = NULL;
	size_t iResolverCount = 0u;
	xacmeclient* pClient;
	size_t i;

	if((pConfig == NULL) || (pConfig->pAccount == NULL))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme client create requires config with account");
		return NULL;
	}
	if((pConfig->sPropagateResolvers != NULL) &&
		(pConfig->iPropagateResolverCount != 0u))
	{
		sResolvers = pConfig->sPropagateResolvers;
		iResolverCount = pConfig->iPropagateResolverCount;
	}
	else
	{
		sResolvers = sDefaults;
		iResolverCount = 3u;
	}
	/* Init 可在参数校验时返回；失败回滚也必须看到一个有效的空对象。 */
	pClient = (xacmeclient*)xrtCalloc(1u, sizeof(*pClient));
	if(pClient == NULL)
	{
		return NULL;
	}
	if(!xacmeClientInit(
			pClient, pConfig->pBorrowedEngine, pConfig->sCaPem,
			pConfig->pAccount, pConfig->uTimeoutUs))
	{
		xacmeClientDiscard(pClient);
		return NULL;
	}
	if((pConfig->sCertKeyPem != NULL) && (pConfig->sCertKeyPem[0] != 0))
	{
		pClient->pCertKey = (xacmecertkey*)xrtMalloc(
			sizeof(*pClient->pCertKey));
		if((pClient->pCertKey == NULL) ||
			!xacmeCertKeyReadPem(
				pConfig->sCertKeyPem, strlen(pConfig->sCertKeyPem),
				pClient->pCertKey))
		{
			xacmeCertKeyUnit(pClient->pCertKey);
			xrtFree(pClient->pCertKey);
			pClient->pCertKey = NULL;
			if(pClient->Http.uTimeoutUs < UINT64_C(30000000))
				pClient->Http.uTimeoutUs = UINT64_C(30000000);
			xacmeClientDiscard(pClient);
			return NULL;
		}
	}
	for(i = 0; i < iResolverCount; i++)
	{
		if((sResolvers[i] == NULL) ||
			(strlen(sResolvers[i]) >=
				sizeof(pClient->sPropagateResolvers[0])))
		{
			continue;
		}
		strcpy(pClient->sPropagateResolvers[
			pClient->iPropagateResolverCount], sResolvers[i]);
		pClient->iPropagateResolverCount++;
		if(pClient->iPropagateResolverCount >=
			XACME_FLOW_RESOLVER_MAX)
		{
			break;
		}
	}
	pClient->uPropagateTimeoutMs = (pConfig->uPropagateTimeoutMs != 0u) ?
		pConfig->uPropagateTimeoutMs : XACME_FLOW_PROPAGATE_TIMEOUT_MS;
	pClient->uIssueTimeoutUs = pConfig->uIssueTimeoutUs;
	return pClient;
}

bool xrtAcmeClientCleanup(struct xacmeclient* pClient)
{
	if(pClient == NULL)
	{
		return true;
	}
	if(pClient->pCertKey != NULL)
	{
		xacmeCertKeyUnit(pClient->pCertKey);
		xrtFree(pClient->pCertKey);
		pClient->pCertKey = NULL;
	}
	return xacmeClientUnit(pClient);
}

void xacmeClientDiscard(xacmeclient* pClient)
{
	if(pClient == NULL) return;
	if(xrtAcmeClientCleanup(pClient)) xrtFree(pClient);
	else xacmeHttpDeferOwner(&pClient->Http, sizeof(*pClient));
}

void xrtAcmeClientDestroy(struct xacmeclient* pClient)
{
	if(xrtAcmeClientCleanup(pClient)) xrtFree(pClient);
}

str xrtAcmeClientAccountPem(const struct xacmeclient* pClient)
{
	return xacmeClientAccountPem(pClient);
}

bool xrtAcmeClientIssue(
	struct xacmeclient* pClient, const xstrview* pDomains,
	size_t iDomainCount, const xacmednsprovider* pDns,
	xacmeissuegrant* pOut)
{
	return xrtAcmeClientIssueEx(
		pClient, pDomains, iDomainCount, pDns, false, pOut);
}

bool xrtAcmeClientIssueEx(
	struct xacmeclient* pClient, const xstrview* pDomains,
	size_t iDomainCount, const xacmednsprovider* pDns,
	bool bPreferAlternate, xacmeissuegrant* pOut)
{
	return xacmeClientIssue(
		pClient, pDomains, iDomainCount, pDns, pOut, bPreferAlternate);
}

bool xrtAcmeClientRollover(
	struct xacmeclient* pClient, cstr sNewKeyPem, cstr sStoreRoot)
{
	return xacmeClientRollover(pClient, sNewKeyPem, sStoreRoot);
}

bool xrtAcmeClientDeactivate(struct xacmeclient* pClient)
{
	return xacmeClientDeactivate(pClient);
}

bool xrtAcmeClientRevoke(
	struct xacmeclient* pClient, cstr sCertPem, int iReason)
{
	return xacmeClientRevoke(pClient, sCertPem, iReason);
}

#endif

#if defined(XACME_FEATURE_ACME_FLOW) && defined(XACME_FEATURE_ACME_STORE)

bool xrtAcmeClientIssueStored(
	struct xacmeclient* pClient, const xstrview* pDomains,
	size_t iDomainCount, const xacmednsprovider* pDns, cstr sStoreRoot,
	int iRenewalDays, xacmeissuegrant* pOut, bool* pbRenewed)
{
	return xacmeClientIssueStored(
		pClient, pDomains, iDomainCount, pDns, sStoreRoot, iRenewalDays,
		pOut, pbRenewed);
}

#endif
