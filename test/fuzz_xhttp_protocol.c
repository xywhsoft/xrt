#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../xrt.c"

int LLVMFuzzerTestOneInput(const uint8_t* pData, size_t iSize)
{
	xnetchain tChain;
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	xcodechttp1errorinfo tError;
	xcodechttp1limits tLimits;
	xcodecwsframeinfo tWsInfo;
	size_t iConsumed = 0u;
	char* sText = NULL;

	if ( (!pData && iSize != 0u) || iSize > 1024u * 1024u ) { return 0; }
	xrtNetChainInit(&tChain);
	if ( !xrtNetChainAppendCopy(&tChain, pData, iSize) ) { return 0; }
	xrtCodecHttp1LimitsInit(&tLimits);
	tLimits.iMaxHeaderBytes = 128u * 1024u;
	tLimits.iMaxTrailerBytes = 64u * 1024u;
	tLimits.iMaxBodyBytes = 512u * 1024u;

	(void)xrtCodecHttp1ParseHeadEx(&tChain, &tFrame, &tMsg, &tLimits, &tError);
	xrtCodecHttp1MessageUnit(&tMsg);
	(void)xrtCodecHttp1ParseEx(&tChain, &tFrame, &tMsg, &tLimits, &tError);
	xrtCodecHttp1MessageUnit(&tMsg);
	xrtCodecHttp1MessageInit(&tMsg);
	(void)xrtCodecHttp1ParseTrailersEx(&tChain, &iConsumed, &tMsg, &tLimits, &tError);
	xrtCodecHttp1MessageUnit(&tMsg);
	(void)xrtCodecWsParseFrameEx(&tChain, &tFrame, &tWsInfo, 512u * 1024u);

	if ( iSize < 64u * 1024u && iSize != SIZE_MAX ) {
		sText = (char*)malloc(iSize + 1u);
		if ( sText ) {
			if ( iSize > 0u ) { memcpy(sText, pData, iSize); }
			sText[iSize] = '\0';
			(void)__xwsValidClientKey(sText);
			(void)__xwsValidProtocolList(sText);
			(void)__xwsValidFieldValue(sText);
			(void)__xwsValidUtf8(pData, iSize);
			free(sText);
		}
	}
	xrtNetChainClear(&tChain);
	return 0;
}
