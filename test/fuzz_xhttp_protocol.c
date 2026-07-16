#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../xrt.c"

#define XHTTP_FUZZ_MAX_INPUT (1024u * 1024u)
#define XHTTP_FUZZ_TEXT_MAX  (64u * 1024u)

static bool append_split(xnetchain* chain, const uint8_t* data, size_t size,
	uint8_t selector)
{
	size_t offset = 0u;
	size_t chunk = (size_t)(selector % 31u) + 1u;

	while ( offset < size ) {
		size_t take = size - offset;
		if ( take > chunk ) { take = chunk; }
		if ( !xrtNetChainAppendCopy(chain, data + offset, take) ) { return false; }
		offset += take;
		chunk = ((chunk * 5u) + 3u) % 97u + 1u;
	}
	return true;
}

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

	if ( (!pData && iSize != 0u) || iSize > XHTTP_FUZZ_MAX_INPUT ) { return 0; }
	xrtNetChainInit(&tChain);
	if ( !append_split(&tChain, pData, iSize, iSize > 0u ? pData[0] : 0u) ) {
		xrtNetChainClear(&tChain);
		return 0;
	}
	xrtCodecHttp1LimitsInit(&tLimits);
	tLimits.iMaxHeaderBytes = 128u * 1024u;
	tLimits.iMaxTrailerBytes = 64u * 1024u;
	tLimits.iMaxBodyBytes = 512u * 1024u;

	xrtCodecHttp1MessageInit(&tMsg);
	(void)xrtCodecHttp1ParseHeadEx(&tChain, &tFrame, &tMsg, &tLimits, &tError);
	xrtCodecHttp1MessageUnit(&tMsg);
	xrtCodecHttp1MessageInit(&tMsg);
	(void)xrtCodecHttp1ParseEx(&tChain, &tFrame, &tMsg, &tLimits, &tError);
	xrtCodecHttp1MessageUnit(&tMsg);
	xrtCodecHttp1MessageInit(&tMsg);
	(void)xrtCodecHttp1ParseTrailersEx(&tChain, &iConsumed, &tMsg, &tLimits, &tError);
	xrtCodecHttp1MessageUnit(&tMsg);
	(void)xrtCodecWsParseFrameEx(&tChain, &tFrame, &tWsInfo, 512u * 1024u);

	if ( iSize < XHTTP_FUZZ_TEXT_MAX && iSize != SIZE_MAX ) {
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

#if defined(XRT_FUZZ_STANDALONE)
int main(int argc, char** argv)
{
	uint8_t* data;
	uint32_t state = UINT32_C(0x6a09e667);
	unsigned long rounds = 1000u;

	if ( argc > 1 ) {
		char* end = NULL;
		unsigned long value = strtoul(argv[1], &end, 10);
		if ( end && *end == '\0' && value > 0u ) { rounds = value; }
	}
	data = (uint8_t*)malloc(XHTTP_FUZZ_TEXT_MAX);
	if ( !data ) { return 1; }
	(void)LLVMFuzzerTestOneInput(NULL, 0u);
	for ( unsigned long i = 0u; i < rounds; ++i ) {
		size_t size;
		state = state * UINT32_C(1664525) + UINT32_C(1013904223);
		size = (size_t)(state % XHTTP_FUZZ_TEXT_MAX);
		for ( size_t j = 0u; j < size; ++j ) {
			state = state * UINT32_C(1664525) + UINT32_C(1013904223);
			data[j] = (uint8_t)(state >> 24u);
		}
		(void)LLVMFuzzerTestOneInput(data, size);
	}
	free(data);
	return 0;
}
#endif
