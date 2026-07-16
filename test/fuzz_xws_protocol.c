#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../xrt.c"

#define XWS_FUZZ_MAX_INPUT (1024u * 1024u)
#define XWS_FUZZ_TEXT_MAX  (64u * 1024u)
#define XWS_FUZZ_ZLIB_MAX  (64u * 1024u)
#define XWS_FUZZ_BUILD_MAX 4096u

static uint8_t fuzz_byte(const uint8_t* data, size_t size, size_t offset)
{
	return offset < size ? data[offset] : 0u;
}

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

static void fuzz_raw_frame(const uint8_t* data, size_t size)
{
	xnetchain chain;
	xcodecframe frame;
	xcodecwsframeinfo info;
	uint64 limit;
	uint8 allowed_rsv;

	xrtNetChainInit(&chain);
	if ( !append_split(&chain, data, size, fuzz_byte(data, size, 0u)) ) {
		xrtNetChainClear(&chain);
		return;
	}
	limit = (uint64)(fuzz_byte(data, size, 1u) + 1u) * 4096u;
	allowed_rsv = (fuzz_byte(data, size, 2u) & 1u) ? XCODEC_WS_RSV1 : 0u;
	(void)xrtCodecWsParseFrameEx2(&chain, &frame, &info, limit, allowed_rsv);
	(void)__xwsFrameParseErrorCloseCode(&chain, limit, allowed_rsv);
	xrtNetChainClear(&chain);
}

static void fuzz_text_validators(const uint8_t* data, size_t size)
{
	char* text;
	size_t text_len = size < XWS_FUZZ_TEXT_MAX ? size : XWS_FUZZ_TEXT_MAX;
	uint16 close_code = 0u;
	size_t close_len = size < 125u ? size : 125u;

	(void)__xwsValidUtf8(data, size);
	(void)__xwsValidateClosePayload((const char*)data, close_len, &close_code);
	if ( text_len == SIZE_MAX ) { return; }
	text = (char*)malloc(text_len + 1u);
	if ( !text ) { return; }
	if ( text_len > 0u ) { memcpy(text, data, text_len); }
	text[text_len] = '\0';
	(void)__xwsValidToken(text);
	(void)__xwsValidProtocolList(text);
	(void)__xwsValidFieldValue(text);
	(void)__xwsValidClientKey(text);
	free(text);
}

static void fuzz_compressed_message(const uint8_t* data, size_t size)
{
#if XWS_HAS_PERMESSAGE_DEFLATE
	char* output = NULL;
	size_t output_len = 0u;
	size_t compressed_len = size < XWS_FUZZ_ZLIB_MAX ? size : XWS_FUZZ_ZLIB_MAX;

	if ( compressed_len == 0u ) { return; }
	(void)__xwsInflateMessage(data, compressed_len, XWS_FUZZ_ZLIB_MAX,
		&output, &output_len);
	XNET_FREE(output);
#else
	(void)data;
	(void)size;
#endif
}

static void fuzz_frame_round_trip(const uint8_t* data, size_t size)
{
	static const uint8 opcodes[] = {
		XCODEC_WS_OPCODE_CONT,
		XCODEC_WS_OPCODE_TEXT,
		XCODEC_WS_OPCODE_BINARY,
		XCODEC_WS_OPCODE_CLOSE,
		XCODEC_WS_OPCODE_PING,
		XCODEC_WS_OPCODE_PONG
	};
	xnetchain chain;
	xcodecframe frame;
	xcodecwsframeinfo info;
	char* encoded = NULL;
	size_t encoded_len = 0u;
	size_t payload_offset = size > 4u ? 4u : size;
	size_t payload_len = size - payload_offset;
	uint8 opcode = opcodes[fuzz_byte(data, size, 0u) %
		(sizeof(opcodes) / sizeof(opcodes[0]))];
	bool control = opcode >= 0x8u;
	bool fin = control || ((fuzz_byte(data, size, 1u) & 1u) != 0u);
	bool mask = (fuzz_byte(data, size, 2u) & 1u) != 0u;
	bool rsv1 = !control && opcode != XCODEC_WS_OPCODE_CONT &&
		((fuzz_byte(data, size, 3u) & 1u) != 0u);
	xcodecstatus status;

	if ( payload_len > XWS_FUZZ_BUILD_MAX ) { payload_len = XWS_FUZZ_BUILD_MAX; }
	if ( control && payload_len > 125u ) { payload_len = 125u; }
	if ( !__xwsBuildFrameBytesEx2(opcode, fin, mask, rsv1,
		data ? data + payload_offset : NULL, payload_len, &encoded, &encoded_len) ) {
		return;
	}
	xrtNetChainInit(&chain);
	if ( !append_split(&chain, (const uint8_t*)encoded, encoded_len,
		fuzz_byte(data, size, 3u)) ) {
		xrtNetChainClear(&chain);
		XNET_FREE(encoded);
		return;
	}
	status = xrtCodecWsParseFrameEx2(&chain, &frame, &info, XWS_FUZZ_BUILD_MAX,
		rsv1 ? XCODEC_WS_RSV1 : 0u);
	if ( status != XCODEC_STATUS_FRAME || frame.iFrameBytes != encoded_len ||
		info.iOpcode != opcode || info.iPayloadLen != payload_len ) {
		abort();
	}
	xrtNetChainClear(&chain);
	XNET_FREE(encoded);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if ( (!data && size != 0u) || size > XWS_FUZZ_MAX_INPUT ) { return 0; }
	fuzz_raw_frame(data, size);
	fuzz_text_validators(data, size);
	fuzz_compressed_message(data, size);
	fuzz_frame_round_trip(data, size);
	return 0;
}

#if defined(XRT_FUZZ_STANDALONE)
int main(int argc, char** argv)
{
	uint8_t* data;
	uint32_t state = UINT32_C(0x7f4a7c15);
	unsigned long rounds = 1000u;

	if ( argc > 1 ) {
		char* end = NULL;
		unsigned long value = strtoul(argv[1], &end, 10);
		if ( end && *end == '\0' && value > 0u ) { rounds = value; }
	}
	data = (uint8_t*)malloc(XWS_FUZZ_TEXT_MAX);
	if ( !data ) { return 1; }
	(void)LLVMFuzzerTestOneInput(NULL, 0u);
	for ( unsigned long i = 0u; i < rounds; ++i ) {
		size_t size;
		state = state * UINT32_C(1664525) + UINT32_C(1013904223);
		size = (size_t)(state % XWS_FUZZ_TEXT_MAX);
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
