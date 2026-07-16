#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static size_t g_xhttp_oom_call_count;
static size_t g_xhttp_oom_fail_at = (size_t)-1;
static size_t g_xhttp_oom_live_count;

static void* __testXHttpAlloc(size_t iSize)
{
	void* pMemory;
	g_xhttp_oom_call_count++;
	if ( g_xhttp_oom_call_count == g_xhttp_oom_fail_at ) { return NULL; }
	pMemory = malloc(iSize);
	if ( pMemory ) { g_xhttp_oom_live_count++; }
	return pMemory;
}


static void __testXHttpFree(void* pMemory)
{
	if ( !pMemory ) { return; }
	g_xhttp_oom_live_count--;
	free(pMemory);
}


#define XNET_ALLOC __testXHttpAlloc
#define XNET_FREE __testXHttpFree
#include "../xrt.c"


static size_t __testBuildMessage(char* sOut, size_t iCap)
{
	size_t iLen = 0u;
	int iWritten;
	iWritten = snprintf(sOut + iLen, iCap - iLen,
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n");
	if ( iWritten <= 0 ) { return 0u; }
	iLen += (size_t)iWritten;
	for ( uint32 i = 0u; i < 40u; ++i ) {
		iWritten = snprintf(sOut + iLen, iCap - iLen, "X-Header-%u: value-%u\r\n", (unsigned)i, (unsigned)i);
		if ( iWritten <= 0 || (size_t)iWritten >= iCap - iLen ) { return 0u; }
		iLen += (size_t)iWritten;
	}
	iWritten = snprintf(sOut + iLen, iCap - iLen, "\r\n4\r\ntest\r\n0\r\n");
	if ( iWritten <= 0 || (size_t)iWritten >= iCap - iLen ) { return 0u; }
	iLen += (size_t)iWritten;
	for ( uint32 i = 0u; i < 20u; ++i ) {
		iWritten = snprintf(sOut + iLen, iCap - iLen, "X-Trailer-%u: value-%u\r\n", (unsigned)i, (unsigned)i);
		if ( iWritten <= 0 || (size_t)iWritten >= iCap - iLen ) { return 0u; }
		iLen += (size_t)iWritten;
	}
	if ( iCap - iLen < 3u ) { return 0u; }
	sOut[iLen++] = '\r';
	sOut[iLen++] = '\n';
	sOut[iLen] = '\0';
	return iLen;
}


static bool __testParse(const char* sMessage, size_t iMessageLen, size_t iFailAt,
	xcodecstatus* pStatus, xcodechttp1errorinfo* pError, size_t* pAllocCalls)
{
	xnetchain tChain;
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	xcodechttp1limits tLimits;
	bool bClean;
	xrtNetChainInit(&tChain);
	g_xhttp_oom_fail_at = (size_t)-1;
	g_xhttp_oom_call_count = 0u;
	if ( !xrtNetChainAppendCopy(&tChain, sMessage, iMessageLen) ) { return false; }
	g_xhttp_oom_call_count = 0u;
	g_xhttp_oom_fail_at = iFailAt;
	xrtCodecHttp1LimitsInit(&tLimits);
	tLimits.iMaxHeaderCount = 64u;
	tLimits.iMaxTrailerCount = 32u;
	*pStatus = xrtCodecHttp1ParseEx(&tChain, &tFrame, &tMsg, &tLimits, pError);
	if ( pAllocCalls ) { *pAllocCalls = g_xhttp_oom_call_count; }
	g_xhttp_oom_fail_at = (size_t)-1;
	xrtCodecHttp1MessageUnit(&tMsg);
	xrtNetChainClear(&tChain);
	bClean = g_xhttp_oom_live_count == 0u;
	return bClean;
}


int main(void)
{
	char sMessage[8192];
	size_t iMessageLen = __testBuildMessage(sMessage, sizeof(sMessage));
	size_t iParseAllocCalls = 0u;
	xcodecstatus eStatus;
	xcodechttp1errorinfo tError;
	bool bPass = iMessageLen > 0u;

	if ( bPass ) {
		bPass = __testParse(sMessage, iMessageLen, (size_t)-1, &eStatus, &tError, &iParseAllocCalls) &&
			eStatus == XCODEC_STATUS_FRAME && iParseAllocCalls > 0u;
	}
	for ( size_t i = 1u; bPass && i <= iParseAllocCalls; ++i ) {
		bPass = __testParse(sMessage, iMessageLen, i, &eStatus, &tError, NULL) &&
			eStatus == XCODEC_STATUS_ERROR && tError.eCode == XCODEC_HTTP1_ERROR_OUT_OF_MEMORY;
	}
	printf("xhttp OOM injection (%llu allocation points): %s\n",
		(unsigned long long)iParseAllocCalls, bPass ? "PASS" : "FAIL");
	return bPass ? 0 : 1;
}
