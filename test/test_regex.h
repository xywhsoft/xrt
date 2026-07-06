#ifndef XRT_TEST_REGEX_H
#define XRT_TEST_REGEX_H

#include <stdlib.h>
#include <string.h>

typedef struct
{
	int iAllocCount;
	int iReallocCount;
	int iFreeCount;
	int iMismatchCount;
} __test_regex_alloc_ctx;

typedef struct
{
	size_t iSize;
} __test_regex_alloc_hdr;


// 内部函数：__Test_Regex_Check
static int __Test_Regex_Check(const char* sName, int bOk)
{
	printf("  %s : %s\n", sName, bOk ? "PASS" : "FAIL");
	return bOk ? 0 : 1;
}


// 内部函数：__Test_Regex_SpanEquals
static bool __Test_Regex_SpanEquals(const char* sText, size_t iTextSize, xregexspan tSpan, const char* sExpect)
{
	size_t iExpectSize = strlen(sExpect);

	if ( tSpan.iEnd < tSpan.iBegin ) {
		return false;
	}
	if ( tSpan.iEnd > iTextSize ) {
		return false;
	}
	if ( (tSpan.iEnd - tSpan.iBegin) != iExpectSize ) {
		return false;
	}
	return memcmp(sText + tSpan.iBegin, sExpect, iExpectSize) == 0;
}


// 内部函数：__Test_Regex_HasIndex
static bool __Test_Regex_HasIndex(const uint32* pIndexes, uint32 iCount, uint32 iExpectIndex)
{
	uint32 i;

	if ( pIndexes == NULL ) {
		return false;
	}

	for ( i = 0; i < iCount; i++ ) {
		if ( pIndexes[i] == iExpectIndex ) {
			return true;
		}
	}

	return false;
}


// 内部函数：__Test_Regex_AllocProc
static ptr __Test_Regex_AllocProc(ptr pUserData, ptr pMem, size_t iPrevSize, size_t iNextSize)
{
	__test_regex_alloc_ctx* pCtx = (__test_regex_alloc_ctx*)pUserData;
	__test_regex_alloc_hdr* pHdr = NULL;
	__test_regex_alloc_hdr* pNewHdr = NULL;

	if ( pMem != NULL ) {
		pHdr = ((__test_regex_alloc_hdr*)pMem) - 1;
		if ( pHdr->iSize != iPrevSize ) {
			if ( pCtx != NULL ) {
				pCtx->iMismatchCount++;
			}
		}
	} else if ( iPrevSize != 0u ) {
		if ( pCtx != NULL ) {
			pCtx->iMismatchCount++;
		}
	}

	if ( iNextSize == 0u ) {
		if ( pHdr != NULL ) {
			free(pHdr);
			if ( pCtx != NULL ) {
				pCtx->iFreeCount++;
			}
		}
		return NULL;
	}

	if ( pHdr == NULL ) {
		pNewHdr = (__test_regex_alloc_hdr*)malloc(sizeof(__test_regex_alloc_hdr) + iNextSize);
		if ( pCtx != NULL ) {
			pCtx->iAllocCount++;
		}
	} else {
		pNewHdr = (__test_regex_alloc_hdr*)realloc(pHdr, sizeof(__test_regex_alloc_hdr) + iNextSize);
		if ( pCtx != NULL ) {
			pCtx->iReallocCount++;
		}
	}

	if ( pNewHdr == NULL ) {
		return NULL;
	}

	pNewHdr->iSize = iNextSize;
	return pNewHdr + 1;
}


// 内部函数：__Test_Regex_Basic
static int __Test_Regex_Basic(void)
{
	int iFail = 0;
	xregex* pRegex = NULL;

	printf("\nRegex test subject 1 : basic create and match\n");

	pRegex = xrtRegexCreate("hello");
	iFail += __Test_Regex_Check("regex create", pRegex != NULL);
	if ( pRegex != NULL ) {
		iFail += __Test_Regex_Check("regex match yes", xrtRegexIsMatch(pRegex, "say hello world", strlen("say hello world")) == 1);
		iFail += __Test_Regex_Check("regex match no", xrtRegexIsMatch(pRegex, "goodbye world", strlen("goodbye world")) == 0);
		xrtRegexDestroy(pRegex);
	}

	pRegex = xrtRegexCreate("[invalid");
	iFail += __Test_Regex_Check("regex invalid pattern", pRegex == NULL);
	if ( pRegex != NULL ) {
		xrtRegexDestroy(pRegex);
	}

	return iFail ? 1 : 0;
}


// 内部函数：__Test_Regex_FindAndCaptures
static int __Test_Regex_FindAndCaptures(void)
{
	int iFail = 0;
	xregex* pRegex = NULL;
	xregexspan tSpan;
	xregexspan arrCaptures[3];
	uint32 arrCaptureFlags[3];
	const char* sTextFind = "Order: 12345, Price: 99";
	const char* sTextCapture = "count=42";

	printf("\nRegex test subject 2 : find and captures\n");

	memset(&tSpan, 0, sizeof(tSpan));
	memset(arrCaptureFlags, 0, sizeof(arrCaptureFlags));
	pRegex = xrtRegexCreate("[0-9]+");
	iFail += __Test_Regex_Check("find regex create", pRegex != NULL);
	if ( pRegex != NULL ) {
		iFail += __Test_Regex_Check("find first number", xrtRegexFind(pRegex, sTextFind, strlen(sTextFind), &tSpan) == 1);
		iFail += __Test_Regex_Check("find span text", __Test_Regex_SpanEquals(sTextFind, strlen(sTextFind), tSpan, "12345"));
		iFail += __Test_Regex_Check("find at next number", xrtRegexFindAt(pRegex, sTextFind, strlen(sTextFind), 13u, &tSpan) == 1);
		iFail += __Test_Regex_Check("find at span text", __Test_Regex_SpanEquals(sTextFind, strlen(sTextFind), tSpan, "99"));
		iFail += __Test_Regex_Check("find no match", xrtRegexFind(pRegex, "abc", 3u, &tSpan) == 0);
		xrtRegexDestroy(pRegex);
	}

	memset(arrCaptures, 0, sizeof(arrCaptures));
	pRegex = xrtRegexCreate("([A-Za-z]+)=([0-9]+)");
	iFail += __Test_Regex_Check("captures regex create", pRegex != NULL);
	if ( pRegex != NULL ) {
		iFail += __Test_Regex_Check("captures match", xrtRegexCaptures(pRegex, sTextCapture, strlen(sTextCapture), arrCaptures, 3u) == 1);
		iFail += __Test_Regex_Check("captures which", xrtRegexWhichCaptures(pRegex, sTextCapture, strlen(sTextCapture), arrCaptures, arrCaptureFlags, 3u) == 1);
		iFail += __Test_Regex_Check("captures full span", __Test_Regex_SpanEquals(sTextCapture, strlen(sTextCapture), arrCaptures[0], "count=42"));
		iFail += __Test_Regex_Check("captures key span", __Test_Regex_SpanEquals(sTextCapture, strlen(sTextCapture), arrCaptures[1], "count"));
		iFail += __Test_Regex_Check("captures value span", __Test_Regex_SpanEquals(sTextCapture, strlen(sTextCapture), arrCaptures[2], "42"));
		iFail += __Test_Regex_Check("captures which flags", (arrCaptureFlags[0] == 1u) && (arrCaptureFlags[1] == 1u) && (arrCaptureFlags[2] == 1u));
		xrtRegexDestroy(pRegex);
	}

	return iFail ? 1 : 0;
}


// 内部函数：__Test_Regex_BuilderAllocAndClone
static int __Test_Regex_BuilderAllocAndClone(void)
{
	int iFail = 0;
	xregexbuilder* pBuilder = NULL;
	xregex* pRegex = NULL;
	xregex* pClone = NULL;
	xregexspan tSpan;
	xregexspan arrCaptures[2];
	__test_regex_alloc_ctx tAllocCtx;
	xregexalloc tAlloc;
	const char* sText = "x\nAbC\n";

	printf("\nRegex test subject 3 : builder, flags, allocator and clone\n");

	memset(&tAllocCtx, 0, sizeof(tAllocCtx));
	memset(&tAlloc, 0, sizeof(tAlloc));
	tAlloc.pUserData = &tAllocCtx;
	tAlloc.procAlloc = __Test_Regex_AllocProc;

	iFail += __Test_Regex_Check(
		"builder create",
		xrtRegexBuilderCreate(&pBuilder, "^(?<word>abc)$", strlen("^(?<word>abc)$"), &tAlloc) == 0 && pBuilder != NULL);
	if ( pBuilder != NULL ) {
		xrtRegexBuilderSetFlags(pBuilder, XRT_REGEX_FLAG_MULTILINE | XRT_REGEX_FLAG_INSENSITIVE);
		iFail += __Test_Regex_Check(
			"regex create from builder",
			xrtRegexCreateFromBuilder(&pRegex, pBuilder, &tAlloc) == 0 && pRegex != NULL);
		xrtRegexBuilderDestroy(pBuilder);
		pBuilder = NULL;
	}

	if ( pRegex != NULL ) {
		iFail += __Test_Regex_Check("builder flags match", xrtRegexIsMatch(pRegex, sText, strlen(sText)) == 1);
		iFail += __Test_Regex_Check("regex clone", xrtRegexClone(&pClone, pRegex, &tAlloc) == 0 && pClone != NULL);
	}

	if ( pClone != NULL ) {
		memset(&tSpan, 0, sizeof(tSpan));
		memset(arrCaptures, 0, sizeof(arrCaptures));
		iFail += __Test_Regex_Check("clone find", xrtRegexFind(pClone, sText, strlen(sText), &tSpan) == 1);
		iFail += __Test_Regex_Check("clone span text", __Test_Regex_SpanEquals(sText, strlen(sText), tSpan, "AbC"));
		iFail += __Test_Regex_Check("clone captures", xrtRegexCaptures(pClone, sText, strlen(sText), arrCaptures, 2u) == 1);
		iFail += __Test_Regex_Check("clone capture text", __Test_Regex_SpanEquals(sText, strlen(sText), arrCaptures[1], "AbC"));
	}

	if ( pClone != NULL ) {
		xrtRegexDestroy(pClone);
	}
	if ( pRegex != NULL ) {
		xrtRegexDestroy(pRegex);
	}
	if ( pBuilder != NULL ) {
		xrtRegexBuilderDestroy(pBuilder);
	}

	iFail += __Test_Regex_Check("allocator mismatch count", tAllocCtx.iMismatchCount == 0);
	iFail += __Test_Regex_Check("allocator activity", (tAllocCtx.iAllocCount + tAllocCtx.iReallocCount) > 0);
	iFail += __Test_Regex_Check("allocator free path", tAllocCtx.iFreeCount > 0);

	return iFail ? 1 : 0;
}


// 内部函数：__Test_Regex_SetApi
static int __Test_Regex_SetApi(void)
{
	int iFail = 0;
	xregexset* pSet = NULL;
	xregexset* pClone = NULL;
	uint32 arrIndexes[4];
	uint32 iCount = 0u;
	const char* arrPatterns[] = { "apple", "banana", "cherry" };
	const char* sText = "cherry apple";

	printf("\nRegex test subject 4 : regex set and set clone\n");

	memset(arrIndexes, 0, sizeof(arrIndexes));
	pSet = xrtRegexSetCreate(arrPatterns, 3u);
	iFail += __Test_Regex_Check("set create", pSet != NULL);
	if ( pSet != NULL ) {
		iFail += __Test_Regex_Check("set is match yes", xrtRegexSetIsMatch(pSet, "banana split", strlen("banana split")) == 1);
		iFail += __Test_Regex_Check("set is match no", xrtRegexSetIsMatch(pSet, "durian", strlen("durian")) == 0);
		iFail += __Test_Regex_Check("set matches", xrtRegexSetMatches(pSet, sText, strlen(sText), arrIndexes, 4u, &iCount) == 1);
		iFail += __Test_Regex_Check("set match count", iCount == 2u);
		iFail += __Test_Regex_Check("set match index apple", __Test_Regex_HasIndex(arrIndexes, iCount, 0u));
		iFail += __Test_Regex_Check("set match index cherry", __Test_Regex_HasIndex(arrIndexes, iCount, 2u));
		iFail += __Test_Regex_Check("set first", xrtRegexSetFirst(pSet, sText, strlen(sText)) == 0);
		iFail += __Test_Regex_Check("set matches at", xrtRegexSetMatchesAt(pSet, sText, strlen(sText), 3u, arrIndexes, 4u, &iCount) == 1);
		iFail += __Test_Regex_Check("set matches at count", iCount == 1u);
		iFail += __Test_Regex_Check("set matches at apple", __Test_Regex_HasIndex(arrIndexes, iCount, 0u));
		iFail += __Test_Regex_Check("set first at", xrtRegexSetFirstAt(pSet, sText, strlen(sText), 3u) == 0);
		iFail += __Test_Regex_Check("set first none", xrtRegexSetFirst(pSet, "durian", strlen("durian")) == -1);
		iFail += __Test_Regex_Check("set clone", xrtRegexSetClone(&pClone, pSet, NULL) == 0 && pClone != NULL);
	}

	if ( pClone != NULL ) {
		iFail += __Test_Regex_Check("set clone match", xrtRegexSetIsMatch(pClone, "apple pie", strlen("apple pie")) == 1);
		xrtRegexSetDestroy(pClone);
	}
	if ( pSet != NULL ) {
		xrtRegexSetDestroy(pSet);
	}

	return iFail ? 1 : 0;
}


// 内部函数：__Test_Regex_SetBuilderApi
static int __Test_Regex_SetBuilderApi(void)
{
	int iFail = 0;
	xregex* pRegexA = NULL;
	xregex* pRegexB = NULL;
	xregexsetbuilder* pBuilder = NULL;
	xregexset* pSet = NULL;

	printf("\nRegex test subject 6 : regex set builder\n");

	pRegexA = xrtRegexCreate("foo");
	pRegexB = xrtRegexCreate("bar");
	iFail += __Test_Regex_Check("set builder regex a", pRegexA != NULL);
	iFail += __Test_Regex_Check("set builder regex b", pRegexB != NULL);
	iFail += __Test_Regex_Check("set builder create", xrtRegexSetBuilderCreate(&pBuilder, NULL) == 0 && pBuilder != NULL);

	if ( (pRegexA != NULL) && (pRegexB != NULL) && (pBuilder != NULL) ) {
		iFail += __Test_Regex_Check("set builder add a", xrtRegexSetBuilderAdd(pBuilder, pRegexA) == 0);
		iFail += __Test_Regex_Check("set builder add b", xrtRegexSetBuilderAdd(pBuilder, pRegexB) == 0);
		iFail += __Test_Regex_Check("set create from builder", xrtRegexSetCreateFromBuilder(&pSet, pBuilder, NULL) == 0 && pSet != NULL);
	}

	if ( pBuilder != NULL ) {
		xrtRegexSetBuilderDestroy(pBuilder);
	}
	if ( pRegexA != NULL ) {
		xrtRegexDestroy(pRegexA);
	}
	if ( pRegexB != NULL ) {
		xrtRegexDestroy(pRegexB);
	}

	if ( pSet != NULL ) {
		iFail += __Test_Regex_Check("set builder result match", xrtRegexSetIsMatch(pSet, "say bar now", strlen("say bar now")) == 1);
		iFail += __Test_Regex_Check("set builder result no match", xrtRegexSetIsMatch(pSet, "baz", strlen("baz")) == 0);
		xrtRegexSetDestroy(pSet);
	}

	return iFail ? 1 : 0;
}


// 内部函数：__Test_Regex_InternalRegression
// 内部函数：__Test_Regex_MatchObject
static int __Test_Regex_MatchObject(void)
{
	int iFail = 0;
	xregex* pRegex = NULL;
	xregexmatch* pMatch = NULL;
	xregexmatch* pNoMatch = NULL;
	const char* sText = "abc你好123";
	const char* sMatchText = NULL;
	str sCopy = NULL;

	printf("\nRegex test subject 7 : lazy match object\n");

	pRegex = xrtRegexCreate("[0-9]+");
	iFail += __Test_Regex_Check("match object regex create", pRegex != NULL);
	if ( pRegex != NULL ) {
		iFail += __Test_Regex_Check("full match no", xrtRegexFullMatch(pRegex, sText, strlen(sText)) == 0);
		pMatch = xrtRegexFindMatch(pRegex, sText, strlen(sText));
		iFail += __Test_Regex_Check("match object create", pMatch != NULL);
		if ( pMatch != NULL ) {
			iFail += __Test_Regex_Check("match object ok", xrtRegexMatchOk(pMatch) == 1);
			iFail += __Test_Regex_Check("match object byte start", xrtRegexMatchByteStart(pMatch) == 9);
			iFail += __Test_Regex_Check("match object byte end", xrtRegexMatchByteEnd(pMatch) == 12);
			iFail += __Test_Regex_Check("match object char start", xrtRegexMatchStart(pMatch) == 5);
			iFail += __Test_Regex_Check("match object char end", xrtRegexMatchEnd(pMatch) == 8);
			sMatchText = xrtRegexMatchText(pMatch);
			iFail += __Test_Regex_Check("match object text", sMatchText != NULL && strcmp(sMatchText, "123") == 0);
			iFail += __Test_Regex_Check("match object text cached", xrtRegexMatchText(pMatch) == sMatchText);
			sCopy = xrtRegexMatchTextCopy(pMatch);
			iFail += __Test_Regex_Check("match object text copy", sCopy != NULL && strcmp(sCopy, "123") == 0);
			xrtFree(sCopy);
		}
		xrtRegexMatchDestroy(pMatch);

		pNoMatch = xrtRegexFindMatch(pRegex, "abc", 3u);
		iFail += __Test_Regex_Check("no match object create", pNoMatch != NULL);
		if ( pNoMatch != NULL ) {
			iFail += __Test_Regex_Check("no match object ok", xrtRegexMatchOk(pNoMatch) == 0);
			iFail += __Test_Regex_Check("no match object start", xrtRegexMatchStart(pNoMatch) == -1);
			iFail += __Test_Regex_Check("no match object text", xrtRegexMatchText(pNoMatch) == NULL);
		}
		xrtRegexMatchDestroy(pNoMatch);
		xrtRegexDestroy(pRegex);
	}

	pRegex = xrtRegexCreateEx("abc", 3u, XRT_REGEX_FLAG_INSENSITIVE);
	iFail += __Test_Regex_Check("regex create ex flags", pRegex != NULL);
	if ( pRegex != NULL ) {
		iFail += __Test_Regex_Check("regex create ex match", xrtRegexFullMatch(pRegex, "ABC", 3u) == 1);
		xrtRegexDestroy(pRegex);
	}

	return iFail ? 1 : 0;
}

static int __Test_Regex_SetFlagsApi(void)
{
	int iFail = 0;
	xregexset* pSet = NULL;
	const char* arrPatterns[] = { "abc", "def" };

	printf("\nRegex test subject 5 : regex set flags api\n");

	pSet = xrtRegexSetCreateEx(arrPatterns, 2u, XRT_REGEX_FLAG_INSENSITIVE);
	iFail += __Test_Regex_Check("set create ex", pSet != NULL);
	if ( pSet != NULL ) {
		iFail += __Test_Regex_Check("set create ex match", xrtRegexSetIsMatch(pSet, "DEF", strlen("DEF")) == 1);
		iFail += __Test_Regex_Check("set create ex first", xrtRegexSetFirst(pSet, "ABC", strlen("ABC")) == 0);
		xrtRegexSetDestroy(pSet);
	}

	return iFail ? 1 : 0;
}


static int __Test_Regex_InternalRegression(void)
{
	int iFail = 0;
	xregex* pRegex = NULL;
	xregex* pClone = NULL;
	size_t iNameSize = 0u;
	const char* sName = NULL;

	printf("\nRegex test subject 8 : internal regression\n");

	pRegex = xrtRegexCreate("(?<test>AAA)|(?<abcdef>[6]*)");
	iFail += __Test_Regex_Check("internal named regex create", pRegex != NULL);
	if ( pRegex != NULL ) {
		iFail += __Test_Regex_Check("internal clone", xrtRegexClone(&pClone, pRegex, NULL) == 0 && pClone != NULL);
	}

	if ( pClone != NULL ) {
		iFail += __Test_Regex_Check("clone capture count", xrtRegexCaptureCount(pClone) == 3u);
		sName = xrtRegexCaptureName(pClone, 1u, &iNameSize);
		iFail += __Test_Regex_Check("clone capture name #1", sName != NULL && strcmp(sName, "test") == 0 && iNameSize == 4u);
		sName = xrtRegexCaptureName(pClone, 2u, &iNameSize);
		iFail += __Test_Regex_Check("clone capture name #2", sName != NULL && strcmp(sName, "abcdef") == 0 && iNameSize == 6u);
	}

	if ( pClone != NULL ) {
		xrtRegexDestroy(pClone);
	}
	if ( pRegex != NULL ) {
		xrtRegexDestroy(pRegex);
	}

	return iFail ? 1 : 0;
}


// 正则测试
static int Test_Regex(void)
{
	int iFail = 0;

	printf("\n\n------------------------------------\n\n Regex Test:\n");

	iFail += __Test_Regex_Basic();
	iFail += __Test_Regex_FindAndCaptures();
	iFail += __Test_Regex_BuilderAllocAndClone();
	iFail += __Test_Regex_SetApi();
	iFail += __Test_Regex_SetFlagsApi();
	iFail += __Test_Regex_SetBuilderApi();
	iFail += __Test_Regex_MatchObject();
	iFail += __Test_Regex_InternalRegression();

	return iFail ? 1 : 0;
}

#endif
