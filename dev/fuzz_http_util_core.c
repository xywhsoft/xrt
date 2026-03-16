#include "../xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int __xrtFuzzHttpUtilOneInput(const unsigned char* pData, size_t iLen)
{
	char* sText = NULL;
	xrturlview tURL;
	xrturlview tBase;
	xrturlview tTarget;
	xrturlview tAuthority;
	xrtstrview tView;
	xrtheaderpair tHeader;
	xrtheaderpair arrHeaders[16];
	xrtstrview arrHeaderValues[8];
	xrtstrview arrTokens[16];
	xrtquerypair arrQuery[16];
	xrtquerypair arrForm[16];
	xrtcookiepair arrCookie[16];
	xrtsetcookieview tSetCookie;
	xrthttpparam tParam;
	xrtmediatypeview tMediaType;
	xrtcontentdispositionview tDisposition;
	xrtmultipartpartview arrPart[8];
	xrtmultipartstreamconfig tStreamCfg;
	xrtmultipartstream tStream;
	xrtmultipartstreamevent tEvent;
	char sOut[1024];
	size_t iOutLen = 0u;
	size_t iCount = 0u;
	size_t iOffset = 0u;
	size_t iChunkOff = 0u;
	size_t iChunkLen;
	xrtmultipartstreamresult iResult;
	const char* sDefaultBoundary = "fuzz-boundary";

	if ( pData == NULL ) return 0;
	sText = (char*)malloc(iLen + 1u);
	if ( sText == NULL ) return 0;
	if ( iLen > 0u ) memcpy(sText, pData, iLen);
	sText[iLen] = '\0';

	memset(&tURL, 0, sizeof(tURL));
	memset(&tBase, 0, sizeof(tBase));
	memset(&tTarget, 0, sizeof(tTarget));
	memset(&tAuthority, 0, sizeof(tAuthority));
	memset(&tHeader, 0, sizeof(tHeader));
	memset(&arrHeaders, 0, sizeof(arrHeaders));
	memset(&arrHeaderValues, 0, sizeof(arrHeaderValues));
	memset(&arrTokens, 0, sizeof(arrTokens));
	memset(&arrQuery, 0, sizeof(arrQuery));
	memset(&arrForm, 0, sizeof(arrForm));
	memset(&arrCookie, 0, sizeof(arrCookie));
	memset(&tSetCookie, 0, sizeof(tSetCookie));
	memset(&tParam, 0, sizeof(tParam));
	memset(&tMediaType, 0, sizeof(tMediaType));
	memset(&tDisposition, 0, sizeof(tDisposition));
	memset(&arrPart, 0, sizeof(arrPart));

	(void)xrtUrlParseViewN(sText, iLen, &tURL);
	(void)xrtUrlParseTargetN(sText, iLen, &tTarget);
	(void)xrtUrlParseAuthorityN(sText, iLen, &tAuthority);
	if ( xrtUrlParseView("https://example.com/base/path/index.html?q=1", &tBase) ) {
		(void)xrtUrlResolveTo(&tBase, sText, iLen, sOut, sizeof(sOut), &iOutLen);
	}
	(void)xrtUrlNormalizePathTo(sText, iLen, sOut, sizeof(sOut), &iOutLen);

	(void)xrtQueryParseToN(sText, iLen, arrQuery, sizeof(arrQuery) / sizeof(arrQuery[0]), &iCount);
	(void)xrtCookieParseToN(sText, iLen, arrCookie, sizeof(arrCookie) / sizeof(arrCookie[0]), &iCount);
	(void)xrtFormUrlEncodedParseToN(sText, iLen, arrForm, sizeof(arrForm) / sizeof(arrForm[0]), &iCount);
	(void)xrtHttpTokenParseToN(sText, iLen, arrTokens, sizeof(arrTokens) / sizeof(arrTokens[0]), &iCount);
	(void)xrtHttpHeaderSplitLineN(sText, iLen, &tHeader);
	(void)xrtHttpHeaderParseBlockToN(sText, iLen, arrHeaders, sizeof(arrHeaders) / sizeof(arrHeaders[0]), &iCount);
	(void)xrtHttpHeaderFindLineN(sText, iLen, "host", &tHeader);
	(void)xrtHttpHeaderFindAllTo(arrHeaders, iCount, "set-cookie", arrHeaderValues, sizeof(arrHeaderValues) / sizeof(arrHeaderValues[0]));
	(void)xrtSetCookieParseN(sText, iLen, &tSetCookie);
	(void)xrtHttpParamCountN(sText, iLen);
	(void)xrtHttpParamFindN(sText, iLen, "boundary", 8u, &tParam);
	(void)xrtHttpMediaTypeParseN(sText, iLen, &tMediaType);
	(void)xrtHttpContentDispositionParseN(sText, iLen, &tDisposition);
	(void)xrtHttpQuotedStringDecodeToN(sText, iLen, sOut, sizeof(sOut), &iOutLen);
	(void)xrtHttpQuotedStringBuildToN(sText, iLen > 192u ? 192u : iLen, sOut, sizeof(sOut), &iOutLen);
	(void)xrtHttpDecodeExtValueTo(sText, iLen, NULL, NULL, sOut, sizeof(sOut), &iOutLen);
	(void)xrtHttpBuildExtValueTo("UTF-8", "", sText, iLen > 192u ? 192u : iLen, sOut, sizeof(sOut), &iOutLen);

	if ( xrtMultipartBoundaryFromContentTypeN(sText, iLen, &tView) && tView.iLen > 0u ) {
		(void)xrtMultipartParseToN(sText, iLen, tView.sPtr, tView.iLen, arrPart, sizeof(arrPart) / sizeof(arrPart[0]), &iCount);
	} else {
		(void)xrtMultipartParseToN(sText, iLen, sDefaultBoundary, strlen(sDefaultBoundary), arrPart, sizeof(arrPart) / sizeof(arrPart[0]), &iCount);
	}

	xrtMultipartStreamConfigInit(&tStreamCfg);
	tStreamCfg.iMaxBufferedBytes = 8u * 1024u;
	tStreamCfg.iMaxHeaderBytes = 2u * 1024u;
	tStreamCfg.iMaxPartHeaders = 32u;
	if ( xrtMultipartStreamInit(&tStream, sDefaultBoundary, strlen(sDefaultBoundary), &tStreamCfg) ) {
		for ( iChunkOff = 0u; iChunkOff < iLen; iChunkOff += iChunkLen ) {
			iChunkLen = 1u + (size_t)(pData[iChunkOff] % 23u);
			if ( iChunkLen > iLen - iChunkOff ) iChunkLen = iLen - iChunkOff;
			(void)xrtMultipartStreamFeed(&tStream, sText + iChunkOff, iChunkLen);
			do {
				iResult = xrtMultipartStreamNext(&tStream, &tEvent);
			} while ( iResult == XRT_MULTIPART_STREAM_RESULT_PART_BEGIN ||
			          iResult == XRT_MULTIPART_STREAM_RESULT_DATA ||
			          iResult == XRT_MULTIPART_STREAM_RESULT_PART_END );
			if ( iResult == XRT_MULTIPART_STREAM_RESULT_ERROR ) break;
		}
		xrtMultipartStreamFinish(&tStream);
		do {
			iResult = xrtMultipartStreamNext(&tStream, &tEvent);
		} while ( iResult == XRT_MULTIPART_STREAM_RESULT_PART_BEGIN ||
		          iResult == XRT_MULTIPART_STREAM_RESULT_DATA ||
		          iResult == XRT_MULTIPART_STREAM_RESULT_PART_END );
		(void)xrtMultipartStreamError(&tStream);
		xrtMultipartStreamUnit(&tStream);
	}

	free(sText);
	return 0;
}

int LLVMFuzzerTestOneInput(const unsigned char* pData, size_t iLen)
{
	return __xrtFuzzHttpUtilOneInput(pData, iLen);
}

#ifndef XRT_FUZZ_LIBFUZZER_ONLY
static int __xrtFuzzHttpUtilRunFile(const char* sPath)
{
	FILE* pFile;
	long iSize;
	unsigned char* pBuf = NULL;

	pFile = fopen(sPath, "rb");
	if ( pFile == NULL ) return 1;
	if ( fseek(pFile, 0, SEEK_END) != 0 ) {
		fclose(pFile);
		return 1;
	}
	iSize = ftell(pFile);
	if ( iSize < 0 ) {
		fclose(pFile);
		return 1;
	}
	if ( fseek(pFile, 0, SEEK_SET) != 0 ) {
		fclose(pFile);
		return 1;
	}
	pBuf = (unsigned char*)malloc((size_t)iSize + 1u);
	if ( pBuf == NULL ) {
		fclose(pFile);
		return 1;
	}
	if ( iSize > 0 && fread(pBuf, 1u, (size_t)iSize, pFile) != (size_t)iSize ) {
		free(pBuf);
		fclose(pFile);
		return 1;
	}
	fclose(pFile);
	pBuf[iSize] = 0u;
	(void)__xrtFuzzHttpUtilOneInput(pBuf, (size_t)iSize);
	free(pBuf);
	return 0;
}

int main(int argc, char** argv)
{
	static const char* const arrSeed[] = {
		"https://example.com/api/v1/chat?model=gpt-5#frag",
		"Host: example.com\r\nConnection: keep-alive\r\nSet-Cookie: sid=abc; Path=/; HttpOnly\r\n\r\n",
		"application/json; charset=UTF-8; profile=\"https://example.com/schema\"",
		"form-data; name=\"file\"; filename*=UTF-8''%E4%B8%AD%E6%96%87.txt",
		"--fuzz-boundary\r\nContent-Disposition: form-data; name=\"field\"\r\n\r\nvalue\r\n--fuzz-boundary--\r\n",
		"a=1&b=2&empty=&dup=x&dup=y",
		"a=1; theme=dark; empty=",
		"gzip, br, deflate"
	};
	int i;

	if ( argc > 1 ) {
		for ( i = 1; i < argc; ++i ) {
			if ( __xrtFuzzHttpUtilRunFile(argv[i]) != 0 ) return 1;
		}
		return 0;
	}

	for ( i = 0; i < (int)(sizeof(arrSeed) / sizeof(arrSeed[0])); ++i ) {
		(void)__xrtFuzzHttpUtilOneInput((const unsigned char*)arrSeed[i], strlen(arrSeed[i]));
	}
	printf("HTTP util fuzz harness smoke passed.\n");
	return 0;
}
#endif
