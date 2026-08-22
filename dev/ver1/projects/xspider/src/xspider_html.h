#ifndef XSPIDER_HTML_H
#define XSPIDER_HTML_H

/*
	HTML link extraction helpers
	HTML 链接提取辅助模块
*/

typedef bool (*xspider_link_sink_fn)(const char* sLink, size_t iLen, ptr pUserData);

bool xspiderHtmlExtractLinks(
	const char* sHtml,
	size_t iHtmlLen,
	xspider_link_sink_fn procSink,
	ptr pUserData,
	int* piOutCount
);

#ifdef XSPIDER_HTML_IMPLEMENTATION

static bool procXSpiderHtmlIsNameBoundary(char ch)
{
	if ( ch == '\0' ) {
		return true;
	}

	if ( (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ) {
		return false;
	}

	if ( (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' ) {
		return false;
	}

	return true;
}


static char procXSpiderHtmlLower(char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) {
		return (char)(ch - 'A' + 'a');
	}

	return ch;
}


static bool procXSpiderHtmlMatchNoCase(const char* sText, size_t iRemain, const char* sNeedle, size_t iNeedleLen)
{
	size_t i = 0;

	if ( iRemain < iNeedleLen ) {
		return false;
	}

	for ( i = 0; i < iNeedleLen; ++i ) {
		if ( procXSpiderHtmlLower(sText[i]) != procXSpiderHtmlLower(sNeedle[i]) ) {
			return false;
		}
	}

	return true;
}


static bool procXSpiderHtmlIsSpace(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f';
}


static const char* procXSpiderHtmlSkipSpaces(const char* pCur, const char* pEnd)
{
	while ( pCur < pEnd && procXSpiderHtmlIsSpace(*pCur) ) {
		++pCur;
	}

	return pCur;
}


bool xspiderHtmlExtractLinks(
	const char* sHtml,
	size_t iHtmlLen,
	xspider_link_sink_fn procSink,
	ptr pUserData,
	int* piOutCount
)
{
	const char* pCur;
	const char* pEnd;
	int iCount = 0;

	if ( piOutCount ) {
		*piOutCount = 0;
	}

	if ( sHtml == NULL || procSink == NULL || iHtmlLen == 0 ) {
		return false;
	}

	pCur = sHtml;
	pEnd = sHtml + iHtmlLen;

	while ( pCur < pEnd ) {
		const char* pNameStart = pCur;
		size_t iRemain = (size_t)(pEnd - pCur);
		size_t iNameLen = 0;
		const char* pValueStart;
		const char* pValueEnd;
		char chQuote = '\0';

		if ( procXSpiderHtmlMatchNoCase(pCur, iRemain, "href", 4u) ) {
			iNameLen = 4u;
		} else if ( procXSpiderHtmlMatchNoCase(pCur, iRemain, "src", 3u) ) {
			iNameLen = 3u;
		} else {
			++pCur;
			continue;
		}

		if ( pNameStart > sHtml && !procXSpiderHtmlIsNameBoundary(*(pNameStart - 1)) ) {
			++pCur;
			continue;
		}

		if ( (pNameStart + iNameLen) < pEnd && !procXSpiderHtmlIsNameBoundary(*(pNameStart + iNameLen)) ) {
			++pCur;
			continue;
		}

		pCur = pNameStart + iNameLen;
		pCur = procXSpiderHtmlSkipSpaces(pCur, pEnd);
		if ( pCur >= pEnd || *pCur != '=' ) {
			continue;
		}

		++pCur;
		pCur = procXSpiderHtmlSkipSpaces(pCur, pEnd);
		if ( pCur >= pEnd ) {
			break;
		}

		if ( *pCur == '"' || *pCur == '\'' ) {
			chQuote = *pCur;
			++pCur;
		}

		pValueStart = pCur;
		if ( chQuote != '\0' ) {
			while ( pCur < pEnd && *pCur != chQuote ) {
				++pCur;
			}
			pValueEnd = pCur;
			if ( pCur < pEnd ) {
				++pCur;
			}
		} else {
			while ( pCur < pEnd ) {
				if ( procXSpiderHtmlIsSpace(*pCur) || *pCur == '>' ) {
					break;
				}
				++pCur;
			}
			pValueEnd = pCur;
		}

		while ( pValueStart < pValueEnd && procXSpiderHtmlIsSpace(*pValueStart) ) {
			++pValueStart;
		}
		while ( pValueEnd > pValueStart && procXSpiderHtmlIsSpace(*(pValueEnd - 1)) ) {
			--pValueEnd;
		}

		if ( pValueEnd > pValueStart ) {
			if ( procSink(pValueStart, (size_t)(pValueEnd - pValueStart), pUserData) ) {
				++iCount;
			}
		}
	}

	if ( piOutCount ) {
		*piOutCount = iCount;
	}

	return true;
}

#endif /* XSPIDER_HTML_IMPLEMENTATION */

#endif /* XSPIDER_HTML_H */
