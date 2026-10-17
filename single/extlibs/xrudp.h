/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xrudp requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XRUDP_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XRUDP_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XRUDP_SINGLE_HEADER_H
#define XRUDP_SINGLE_HEADER_H
#define XRUDP_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xrudp/include/xrudp/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XRUDP_FEATURES_H
#define XRUDP_FEATURES_H

/* rudp_crypto 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_CRYPTO)
#ifndef XRUDP_FEATURE_RUDP_CRYPTO
#define XRUDP_FEATURE_RUDP_CRYPTO
#endif
#ifndef XRUDP_MODULE_RUDP_WIRE
#define XRUDP_MODULE_RUDP_WIRE
#endif
#ifndef XRT_MODULE_CRYPTO_CHACHA20_POLY1305
#define XRT_MODULE_CRYPTO_CHACHA20_POLY1305
#endif
#endif

/* rudp_wire 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_WIRE)
#ifndef XRUDP_FEATURE_RUDP_WIRE
#define XRUDP_FEATURE_RUDP_WIRE
#endif
#endif

#endif /* XRUDP_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xrudp/include/xrudp/export.h */
/* ========================================================================== */

#ifndef XRUDP_EXPORT_H
#define XRUDP_EXPORT_H


#if defined(_WIN32) || defined(_WIN64)
#if defined(XRUDP_BUILD_SHARED)
#define XRUDP_API __declspec(dllexport)
#elif defined(XRUDP_USE_SHARED)
#define XRUDP_API __declspec(dllimport)
#else
#define XRUDP_API
#endif
#elif defined(__GNUC__) || defined(__clang__)
#if defined(XRUDP_BUILD_SHARED)
#define XRUDP_API __attribute__((visibility("default")))
#else
#define XRUDP_API
#endif
#else
#define XRUDP_API
#endif

#endif


/* ========================================================================== */
/* public: extlibs/xrudp/include/xrudp/wire.h */
/* ========================================================================== */

#ifndef XRUDP_WIRE_H
#define XRUDP_WIRE_H


#if defined(XRUDP_FEATURE_RUDP_WIRE)

#define XRUDP_HEADER_SIZE 48u
#define XRUDP_TAG_SIZE 16u
#define XRUDP_PACKET_MAX 1200u
#define XRUDP_FRAME_HEADER_SIZE 4u
#define XRUDP_ACK_RANGES_MAX 32u
#define XRUDP_FRAMES_MAX 64u
#define XRUDP_REPLAY_WORDS 16u
#define XRUDP_REPLAY_BITS (XRUDP_REPLAY_WORDS * 64u)

typedef enum xrudpresult {
	XRUDP_OK = 0,
	XRUDP_DONE,
	XRUDP_ARGUMENT,
	XRUDP_TRUNCATED,
	XRUDP_PROTOCOL_MISMATCH,
	XRUDP_MALFORMED,
	XRUDP_LIMIT,
	XRUDP_DUPLICATE,
	XRUDP_TOO_OLD,
	XRUDP_AUTH_FAILED,
	XRUDP_BACKPRESSURE,
	XRUDP_CANCELLED,
	XRUDP_TIMEOUT,
	XRUDP_CLOSED,
	XRUDP_NO_MEMORY,
	XRUDP_IO_ERROR
} xrudpresult;

typedef enum xrudppackettype {
	XRUDP_PACKET_HELLO = 1,
	XRUDP_PACKET_RETRY = 2,
	XRUDP_PACKET_HANDSHAKE = 3,
	XRUDP_PACKET_DATA = 4
} xrudppackettype;

typedef enum xrudpframetype {
	XRUDP_FRAME_PADDING = 0,
	XRUDP_FRAME_PING = 1,
	XRUDP_FRAME_ACK = 2,
	XRUDP_FRAME_STREAM = 3,
	XRUDP_FRAME_RESET = 4,
	XRUDP_FRAME_MAX_DATA = 5,
	XRUDP_FRAME_MAX_STREAM_DATA = 6,
	XRUDP_FRAME_MAX_STREAMS = 7,
	XRUDP_FRAME_DATAGRAM = 8,
	XRUDP_FRAME_CLOSE = 9,
	XRUDP_FRAME_PATH_CHALLENGE = 10,
	XRUDP_FRAME_PATH_RESPONSE = 11,
	XRUDP_FRAME_KEY_CONFIRM = 12,
	XRUDP_FRAME_OPEN = 13
} xrudpframetype;

#define XRUDP_STREAM_FIN 1u
#define XRUDP_DATAGRAM_SEQUENCED 1u

/* Wire integers are big-endian; no C structure is copied onto the wire.
 * PayloadSize includes the AEAD tag for DATA; all other packet types use epoch 0.
 * ConnectionId is an opaque nonzero destination/rendezvous identifier, not auth.
 * The codec checks structure only. It never authenticates or authorizes a peer. */
typedef struct xrudpwireheader {
	xrudppackettype Type;
	uint16_t PayloadSize;
	uint32_t KeyEpoch;
	unsigned char ConnectionId[16];
	uint64_t PacketNumber;
} xrudpwireheader;

typedef struct xrudpwirepacket {
	xrudpwireheader Header;
	const unsigned char* Payload;
} xrudpwirepacket;

typedef struct xrudpwireframe {
	xrudpframetype Type;
	uint8_t Flags;
	uint16_t Size;
	const unsigned char* Data;
} xrudpwireframe;

/* Fixed-size replay state. Initialize before use; each receive direction owns
 * its own window. Do not commit a packet until authentication has succeeded. */
typedef struct xrudpreplay {
	uint64_t Largest;
	uint64_t Bits[XRUDP_REPLAY_WORDS];
} xrudpreplay;

XRT_EXTERN_C_BEGIN

/* No allocation, logging, I/O or state changes. Packet/frame views borrow input.
 * On failure outputs/cursors and destination bytes are unchanged, except that
 * an Encode/Append written count is set to zero. Overlapping output/input is
 * allowed, but header/frame structures must not overlap the destination. */
XRUDP_API xrudpresult xrtRudpPacketDecode(const void* pData, size_t iSize,
	xrudpwirepacket* pPacket);
XRUDP_API xrudpresult xrtRudpPacketEncode(void* pOutput, size_t iCapacity,
	size_t* pWritten, const xrudpwireheader* pHeader,
	const void* pPayload, size_t iPayloadSize);
XRUDP_API xrudpresult xrtRudpFrameNext(const void* pData, size_t iSize,
	size_t* pOffset, xrudpwireframe* pFrame);
XRUDP_API xrudpresult xrtRudpFrameAppend(void* pOutput, size_t iCapacity,
	size_t* pWritten, xrudpframetype iType, uint8_t iFlags,
	const void* pData, size_t iSize);
/* Validate an entire decrypted DATA payload before invoking any application
 * callbacks or changing connection state. Structural checks do not replace
 * connection-specific ACK, stream-credit or authenticated-peer validation. */
XRUDP_API xrudpresult xrtRudpFramesValidate(const void* pData, size_t iSize,
	size_t* pCount);
XRUDP_API void xrtRudpReplayInit(xrudpreplay* pWindow);
XRUDP_API xrudpresult xrtRudpReplayCheck(const xrudpreplay* pWindow,
	uint64_t iPacketNumber);
XRUDP_API xrudpresult xrtRudpReplayAccept(xrudpreplay* pWindow,
	uint64_t iPacketNumber);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrudp/include/xrudp/crypto.h */
/* ========================================================================== */

#ifndef XRUDP_CRYPTO_H
#define XRUDP_CRYPTO_H


#if defined(XRUDP_FEATURE_RUDP_CRYPTO)

#define XRUDP_KEY_SIZE 32u
#define XRUDP_KEY_PACKETS_MAX UINT64_C(16777216)

typedef struct xrudpcipher xrudpcipher;

typedef struct xrudpcipherstate {
	uint32_t SendEpoch;
	uint32_t ReceiveEpoch;
	uint32_t PreviousReceiveEpoch;
	uint64_t NextPacketNumber;
	uint64_t PacketsInSendEpoch;
	uint64_t LargestReceivedPacket;
	bool SendKeyPending;
} xrudpcipherstate;

XRT_EXTERN_C_BEGIN

/* Low-level packet protection for already authenticated directional keys.
 * This constructor does NOT authenticate a peer or implement a handshake.
 * Keys are 32 bytes, connection IDs are 16 nonzero bytes. Send/receive keys
 * must differ. iPacketLimit=0 selects the fixed maximum; lower limits are legal.
 * All methods are serialized by the connection owner. No internal threads.
 * Keys are copied into owned memory and securely cleared on destruction.
 * On failure *ppCipher is unchanged. */
XRUDP_API xrudpresult xrtRudpCipherCreate(const void* pSendKey,
	const void* pReceiveKey, const void* pLocalId, const void* pPeerId,
	uint64_t iPacketLimit, xrudpcipher** ppCipher);
XRUDP_API void xrtRudpCipherDestroy(xrudpcipher* pCipher);
XRUDP_API xrudpresult xrtRudpCipherState(const xrudpcipher* pCipher,
	xrudpcipherstate* pState);

/* Complete decrypted frame payloads are checked before sealing/committing.
 * Open authenticates the entire fixed header and body before producing output.
 * Failure changes neither cipher state nor destination bytes; *pWritten=0.
 * Source and destination buffers may overlap. Output counters/state objects
 * must not alias buffers. Success consumes a fresh packet number exactly once.
 * A successful Open confirms transport reception, not application execution. */
XRUDP_API xrudpresult xrtRudpCipherSeal(xrudpcipher* pCipher,
	const void* pFrames, size_t iFrameSize, void* pOutput,
	size_t iCapacity, size_t* pWritten);
XRUDP_API xrudpresult xrtRudpCipherOpen(xrudpcipher* pCipher,
	const void* pPacket, size_t iPacketSize, void* pOutput,
	size_t iCapacity, size_t* pWritten);

/* Noise REKEY; packet numbers never reset. Only one unconfirmed send update
 * is allowed. The peer must reliably send KEY_CONFIRM for its receive epoch;
 * Open processes authenticated confirmations. A lost update needs no special
 * packet: subsequent DATA packets carry the new authenticated epoch.
 * The connection driver retires the previous receive key after its bounded
 * reordering grace period. Reliable data then retransmits under a fresh key.
 * This rotation does not provide recovery from a compromised current key. */
XRUDP_API xrudpresult xrtRudpCipherUpdateKey(xrudpcipher* pCipher);
XRUDP_API void xrtRudpCipherRetirePrevious(xrudpcipher* pCipher);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrudp/include/xrudp.h */
/* ========================================================================== */

#ifndef XRUDP_H
#define XRUDP_H


#endif

#endif

#if defined(XRUDP_IMPLEMENTATION) && !defined(XRUDP_IMPLEMENTATION_ONCE)
#define XRUDP_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* source: extlibs/xrudp/src/wire/rudp_wire.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_WIRE)
#include <string.h>

static const unsigned char xrudp__magic[8] = {
	'X', 'R', 'U', 'D', 'P', '0', '0', '1'
};

static uint16_t xrudp__get16(const unsigned char* p)
{
	return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t xrudp__get32(const unsigned char* p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
		((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t xrudp__get64(const unsigned char* p)
{
	return ((uint64_t)xrudp__get32(p) << 32) | xrudp__get32(p + 4);
}

static void xrudp__put16(unsigned char* p, uint16_t n)
{
	p[0] = (unsigned char)(n >> 8);
	p[1] = (unsigned char)n;
}

static void xrudp__put32(unsigned char* p, uint32_t n)
{
	p[0] = (unsigned char)(n >> 24);
	p[1] = (unsigned char)(n >> 16);
	p[2] = (unsigned char)(n >> 8);
	p[3] = (unsigned char)n;
}

static void xrudp__put64(unsigned char* p, uint64_t n)
{
	xrudp__put32(p, (uint32_t)(n >> 32));
	xrudp__put32(p + 4, (uint32_t)n);
}

static bool xrudp__nonzero(const unsigned char* p, size_t n)
{
	unsigned char any = 0;
	for (size_t i = 0; i < n; ++i) any |= p[i];
	return any != 0;
}

static xrudpresult xrudp__header_check(const xrudpwireheader* h)
{
	if (h->Type < XRUDP_PACKET_HELLO || h->Type > XRUDP_PACKET_DATA ||
		!xrudp__nonzero(h->ConnectionId, sizeof(h->ConnectionId)) ||
		h->PacketNumber == 0) return XRUDP_MALFORMED;
	if (h->PayloadSize > XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE)
		return XRUDP_LIMIT;
	if (h->Type == XRUDP_PACKET_DATA) {
		if (h->KeyEpoch == 0 || h->PayloadSize < XRUDP_TAG_SIZE)
			return XRUDP_MALFORMED;
	} else if (h->KeyEpoch != 0 || h->PayloadSize == 0) {
		return XRUDP_MALFORMED;
	}
	return XRUDP_OK;
}

xrudpresult xrtRudpPacketDecode(const void* pData, size_t iSize,
	xrudpwirepacket* pPacket)
{
	const unsigned char* p = (const unsigned char*)pData;
	xrudpwirepacket packet;
	xrudpresult result;
	if (p == NULL || pPacket == NULL) return XRUDP_ARGUMENT;
	if (iSize > XRUDP_PACKET_MAX) return XRUDP_LIMIT;
	if (iSize < sizeof(xrudp__magic)) return XRUDP_TRUNCATED;
	if (memcmp(p, xrudp__magic, sizeof(xrudp__magic)) != 0)
		return XRUDP_PROTOCOL_MISMATCH;
	if (iSize < XRUDP_HEADER_SIZE) return XRUDP_TRUNCATED;
	if (p[9] != 0 || xrudp__nonzero(p + 40, 8)) return XRUDP_MALFORMED;
	memset(&packet, 0, sizeof(packet));
	packet.Header.Type = (xrudppackettype)p[8];
	packet.Header.PayloadSize = xrudp__get16(p + 10);
	packet.Header.KeyEpoch = xrudp__get32(p + 12);
	memcpy(packet.Header.ConnectionId, p + 16, 16);
	packet.Header.PacketNumber = xrudp__get64(p + 32);
	result = xrudp__header_check(&packet.Header);
	if (result != XRUDP_OK) return result;
	if (packet.Header.PayloadSize > iSize - XRUDP_HEADER_SIZE)
		return XRUDP_TRUNCATED;
	if (packet.Header.PayloadSize != iSize - XRUDP_HEADER_SIZE)
		return XRUDP_MALFORMED;
	packet.Payload = p + XRUDP_HEADER_SIZE;
	*pPacket = packet;
	return XRUDP_OK;
}

xrudpresult xrtRudpPacketEncode(void* pOutput, size_t iCapacity,
	size_t* pWritten, const xrudpwireheader* pHeader,
	const void* pPayload, size_t iPayloadSize)
{
	unsigned char* p = (unsigned char*)pOutput;
	xrudpresult result;
	if (pWritten != NULL) *pWritten = 0;
	if (p == NULL || pWritten == NULL || pHeader == NULL ||
		(pPayload == NULL && iPayloadSize != 0)) return XRUDP_ARGUMENT;
	if (iPayloadSize > XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE)
		return XRUDP_LIMIT;
	if (pHeader->PayloadSize != iPayloadSize) return XRUDP_ARGUMENT;
	result = xrudp__header_check(pHeader);
	if (result != XRUDP_OK) return result;
	if (iCapacity < XRUDP_HEADER_SIZE + iPayloadSize) return XRUDP_LIMIT;
	memmove(p + XRUDP_HEADER_SIZE, pPayload, iPayloadSize);
	memcpy(p, xrudp__magic, sizeof(xrudp__magic));
	p[8] = (unsigned char)pHeader->Type;
	p[9] = 0;
	xrudp__put16(p + 10, pHeader->PayloadSize);
	xrudp__put32(p + 12, pHeader->KeyEpoch);
	memcpy(p + 16, pHeader->ConnectionId, 16);
	xrudp__put64(p + 32, pHeader->PacketNumber);
	memset(p + 40, 0, 8);
	*pWritten = XRUDP_HEADER_SIZE + iPayloadSize;
	return XRUDP_OK;
}

static xrudpresult xrudp__ack_check(const unsigned char* p, size_t n)
{
	uint16_t count;
	uint64_t previous = 0;
	if (n < 8) return XRUDP_MALFORMED;
	count = xrudp__get16(p + 4);
	if (count == 0 || count > XRUDP_ACK_RANGES_MAX ||
		xrudp__get16(p + 6) != 0 || n != 8u + (size_t)count * 16u)
		return XRUDP_MALFORMED;
	for (size_t i = 0; i < count; ++i) {
		uint64_t first = xrudp__get64(p + 8 + i * 16);
		uint64_t last = xrudp__get64(p + 16 + i * 16);
		if (first == 0 || first > last) return XRUDP_MALFORMED;
		/* Descending, disjoint and nonadjacent, giving one canonical encoding. */
		if (i != 0 && (last >= previous || previous - last <= 1))
			return XRUDP_MALFORMED;
		previous = first;
	}
	return XRUDP_OK;
}

static xrudpresult xrudp__frame_check(xrudpframetype type, uint8_t flags,
	const unsigned char* p, size_t n)
{
	uint64_t offset;
	if (flags != 0 && !((type == XRUDP_FRAME_STREAM && flags == XRUDP_STREAM_FIN) ||
		(type == XRUDP_FRAME_DATAGRAM && flags == XRUDP_DATAGRAM_SEQUENCED)))
		return XRUDP_MALFORMED;
	switch (type) {
	case XRUDP_FRAME_PADDING:
		return n == 0 || !xrudp__nonzero(p, n) ? XRUDP_OK : XRUDP_MALFORMED;
	case XRUDP_FRAME_PING:
		return n == 0 ? XRUDP_OK : XRUDP_MALFORMED;
	case XRUDP_FRAME_ACK:
		return xrudp__ack_check(p, n);
	case XRUDP_FRAME_STREAM:
		if (n < 12 || xrudp__get32(p) == 0) return XRUDP_MALFORMED;
		offset = xrudp__get64(p + 4);
		return (uint64_t)(n - 12) <= UINT64_MAX - offset ? XRUDP_OK : XRUDP_MALFORMED;
	case XRUDP_FRAME_RESET:
		return n == 16 && xrudp__get32(p) != 0 ? XRUDP_OK : XRUDP_MALFORMED;
	case XRUDP_FRAME_MAX_DATA:
		return n == 8 ? XRUDP_OK : XRUDP_MALFORMED;
	case XRUDP_FRAME_MAX_STREAM_DATA:
		return n == 12 && xrudp__get32(p) != 0 ? XRUDP_OK : XRUDP_MALFORMED;
	case XRUDP_FRAME_MAX_STREAMS:
		return n == 4 ? XRUDP_OK : XRUDP_MALFORMED;
	case XRUDP_FRAME_DATAGRAM:
		if (n < 12 || xrudp__get32(p) == 0) return XRUDP_MALFORMED;
		return ((flags == XRUDP_DATAGRAM_SEQUENCED) == (xrudp__get64(p + 4) != 0)) ?
			XRUDP_OK : XRUDP_MALFORMED;
	case XRUDP_FRAME_CLOSE:
		return n == 4 ? XRUDP_OK : XRUDP_MALFORMED;
	case XRUDP_FRAME_PATH_CHALLENGE:
	case XRUDP_FRAME_PATH_RESPONSE:
		return n == 16 ? XRUDP_OK : XRUDP_MALFORMED;
	case XRUDP_FRAME_KEY_CONFIRM:
		return n == 4 && xrudp__get32(p) != 0 ? XRUDP_OK : XRUDP_MALFORMED;
	case XRUDP_FRAME_OPEN:
		/* id:u32, receive credit:u64, kind:u8 (stream/message), priority:u8,
		 * weight:u16. Local policy additionally bounds credit/priority. */
		return n == 16 && xrudp__get32(p) != 0 && p[12] <= 1 &&
			p[13] <= 3 && xrudp__get16(p + 14) != 0 ? XRUDP_OK : XRUDP_MALFORMED;
	default:
		return XRUDP_MALFORMED;
	}
}

xrudpresult xrtRudpFrameNext(const void* pData, size_t iSize,
	size_t* pOffset, xrudpwireframe* pFrame)
{
	const unsigned char* p = (const unsigned char*)pData;
	xrudpwireframe frame;
	xrudpresult result;
	size_t remaining;
	if ((p == NULL && iSize != 0) || pOffset == NULL || pFrame == NULL ||
		*pOffset > iSize) return XRUDP_ARGUMENT;
	if (iSize > XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE)
		return XRUDP_LIMIT;
	remaining = iSize - *pOffset;
	if (remaining == 0) return XRUDP_DONE;
	if (remaining < XRUDP_FRAME_HEADER_SIZE) return XRUDP_TRUNCATED;
	p += *pOffset;
	memset(&frame, 0, sizeof(frame));
	frame.Type = (xrudpframetype)p[0];
	frame.Flags = p[1];
	frame.Size = xrudp__get16(p + 2);
	if (frame.Size > remaining - XRUDP_FRAME_HEADER_SIZE) return XRUDP_TRUNCATED;
	frame.Data = p + XRUDP_FRAME_HEADER_SIZE;
	result = xrudp__frame_check(frame.Type, frame.Flags, frame.Data, frame.Size);
	if (result != XRUDP_OK) return result;
	*pOffset += XRUDP_FRAME_HEADER_SIZE + frame.Size;
	*pFrame = frame;
	return XRUDP_OK;
}

xrudpresult xrtRudpFrameAppend(void* pOutput, size_t iCapacity,
	size_t* pWritten, xrudpframetype iType, uint8_t iFlags,
	const void* pData, size_t iSize)
{
	unsigned char* p = (unsigned char*)pOutput;
	xrudpresult result;
	if (pWritten != NULL) *pWritten = 0;
	if (p == NULL || pWritten == NULL || (pData == NULL && iSize != 0))
		return XRUDP_ARGUMENT;
	if (iSize > XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE -
		XRUDP_FRAME_HEADER_SIZE) return XRUDP_LIMIT;
	result = xrudp__frame_check(iType, iFlags, (const unsigned char*)pData, iSize);
	if (result != XRUDP_OK) return result;
	if (iCapacity < XRUDP_FRAME_HEADER_SIZE + iSize) return XRUDP_LIMIT;
	if (iSize != 0) memmove(p + XRUDP_FRAME_HEADER_SIZE, pData, iSize);
	p[0] = (unsigned char)iType;
	p[1] = iFlags;
	xrudp__put16(p + 2, (uint16_t)iSize);
	*pWritten = XRUDP_FRAME_HEADER_SIZE + iSize;
	return XRUDP_OK;
}

xrudpresult xrtRudpFramesValidate(const void* pData, size_t iSize, size_t* pCount)
{
	xrudpwireframe frame;
	xrudpresult result;
	size_t offset = 0, count = 0;
	if (pCount == NULL) return XRUDP_ARGUMENT;
	while ((result = xrtRudpFrameNext(pData, iSize, &offset, &frame)) == XRUDP_OK) {
		if (++count > XRUDP_FRAMES_MAX) return XRUDP_LIMIT;
	}
	if (result != XRUDP_DONE) return result;
	if (count == 0) return XRUDP_MALFORMED;
	*pCount = count;
	return XRUDP_OK;
}
#endif


/* ========================================================================== */
/* source: extlibs/xrudp/src/wire/rudp_replay.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_WIRE)
#include <string.h>

void xrtRudpReplayInit(xrudpreplay* pWindow)
{
	if (pWindow != NULL) memset(pWindow, 0, sizeof(*pWindow));
}

xrudpresult xrtRudpReplayCheck(const xrudpreplay* pWindow, uint64_t number)
{
	uint64_t distance;
	if (pWindow == NULL || number == 0) return XRUDP_ARGUMENT;
	if (number > pWindow->Largest) return XRUDP_OK;
	distance = pWindow->Largest - number;
	if (distance >= XRUDP_REPLAY_BITS) return XRUDP_TOO_OLD;
	return (pWindow->Bits[distance / 64] & ((uint64_t)1 << (distance % 64))) != 0 ?
		XRUDP_DUPLICATE : XRUDP_OK;
}

xrudpresult xrtRudpReplayAccept(xrudpreplay* pWindow, uint64_t number)
{
	xrudpresult result = xrtRudpReplayCheck(pWindow, number);
	uint64_t distance;
	if (result != XRUDP_OK) return result;
	if (number > pWindow->Largest) {
		distance = number - pWindow->Largest;
		if (distance >= XRUDP_REPLAY_BITS) {
			memset(pWindow->Bits, 0, sizeof(pWindow->Bits));
		} else {
			size_t words = (size_t)(distance / 64);
			unsigned int bits = (unsigned int)(distance % 64);
			for (size_t i = XRUDP_REPLAY_WORDS; i-- > 0;) {
				uint64_t value = 0;
				if (i >= words) {
					value = pWindow->Bits[i - words] << bits;
					if (bits != 0 && i > words)
						value |= pWindow->Bits[i - words - 1] >> (64 - bits);
				}
				pWindow->Bits[i] = value;
			}
		}
		pWindow->Largest = number;
	}
	distance = pWindow->Largest - number;
	pWindow->Bits[distance / 64] |= (uint64_t)1 << (distance % 64);
	return XRUDP_OK;
}
#endif


/* ========================================================================== */
/* source: extlibs/xrudp/src/crypto/rudp_cipher.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_CRYPTO)
#include <string.h>

struct xrudpcipher {
	unsigned char SendKey[XRUDP_KEY_SIZE];
	unsigned char ReceiveKey[XRUDP_KEY_SIZE];
	unsigned char PreviousKey[XRUDP_KEY_SIZE];
	unsigned char LocalId[16];
	unsigned char PeerId[16];
	uint64_t NextPacket;
	uint64_t SendCount;
	uint64_t PacketLimit;
	uint32_t SendEpoch;
	uint32_t ReceiveEpoch;
	uint32_t PreviousEpoch;
	bool SendPending;
	xrudpreplay Replay;
};

static void xrudp__nonce(unsigned char nonce[12], uint64_t number)
{
	memset(nonce, 0, 4);
	for (size_t i = 0; i < 8; ++i) nonce[i + 4] = (unsigned char)(number >> (i * 8));
}

static bool xrudp__rekey(const unsigned char key[32], unsigned char output[32])
{
	unsigned char nonce[12], zeros[32] = {0}, sealed[48];
	bool result;
	xrudp__nonce(nonce, UINT64_MAX);
	result = xrtChaCha20Poly1305Seal(key, nonce, NULL, 0, zeros, sizeof(zeros),
		sealed, sizeof(sealed));
	if (result) memcpy(output, sealed, 32);
	xrtSecureZero(sealed, sizeof(sealed));
	return result;
}

static bool xrudp__cipher_id_valid(const void* pId)
{
	const unsigned char* p = (const unsigned char*)pId;
	unsigned char any = 0;
	for (size_t i = 0; i < 16; ++i) any |= p[i];
	return any != 0;
}

xrudpresult xrtRudpCipherCreate(const void* pSendKey, const void* pReceiveKey,
	const void* pLocalId, const void* pPeerId, uint64_t iPacketLimit,
	xrudpcipher** ppCipher)
{
	xrudpcipher* cipher;
	if (pSendKey == NULL || pReceiveKey == NULL || pLocalId == NULL ||
		pPeerId == NULL || ppCipher == NULL ||
		iPacketLimit > XRUDP_KEY_PACKETS_MAX ||
		!xrudp__cipher_id_valid(pLocalId) || !xrudp__cipher_id_valid(pPeerId) ||
		xrtConstTimeEqual(pSendKey, pReceiveKey, XRUDP_KEY_SIZE)) return XRUDP_ARGUMENT;
	cipher = (xrudpcipher*)xrtMalloc(sizeof(*cipher));
	if (cipher == NULL) return XRUDP_NO_MEMORY;
	memset(cipher, 0, sizeof(*cipher));
	memcpy(cipher->SendKey, pSendKey, XRUDP_KEY_SIZE);
	memcpy(cipher->ReceiveKey, pReceiveKey, XRUDP_KEY_SIZE);
	memcpy(cipher->LocalId, pLocalId, 16);
	memcpy(cipher->PeerId, pPeerId, 16);
	cipher->SendEpoch = cipher->ReceiveEpoch = 1;
	cipher->NextPacket = 1;
	cipher->PacketLimit = iPacketLimit == 0 ? XRUDP_KEY_PACKETS_MAX : iPacketLimit;
	*ppCipher = cipher;
	return XRUDP_OK;
}

void xrtRudpCipherDestroy(xrudpcipher* pCipher)
{
	if (pCipher != NULL) {
		xrtSecureZero(pCipher, sizeof(*pCipher));
		xrtFree(pCipher);
	}
}

xrudpresult xrtRudpCipherState(const xrudpcipher* pCipher, xrudpcipherstate* pState)
{
	xrudpcipherstate state;
	if (pCipher == NULL || pState == NULL) return XRUDP_ARGUMENT;
	memset(&state, 0, sizeof(state));
	state.SendEpoch = pCipher->SendEpoch;
	state.ReceiveEpoch = pCipher->ReceiveEpoch;
	state.PreviousReceiveEpoch = pCipher->PreviousEpoch;
	state.NextPacketNumber = pCipher->NextPacket;
	state.PacketsInSendEpoch = pCipher->SendCount;
	state.LargestReceivedPacket = pCipher->Replay.Largest;
	state.SendKeyPending = pCipher->SendPending;
	*pState = state;
	return XRUDP_OK;
}

xrudpresult xrtRudpCipherSeal(xrudpcipher* pCipher,
	const void* pFrames, size_t iFrameSize, void* pOutput,
	size_t iCapacity, size_t* pWritten)
{
	unsigned char packet[XRUDP_PACKET_MAX] = {0}, nonce[12];
	xrudpwireheader header;
	xrudpresult result;
	size_t count, size = 0;
	if (pWritten != NULL) *pWritten = 0;
	if (pCipher == NULL || pFrames == NULL || pOutput == NULL || pWritten == NULL)
		return XRUDP_ARGUMENT;
	result = xrtRudpFramesValidate(pFrames, iFrameSize, &count);
	if (result != XRUDP_OK) return result;
	if (iCapacity < XRUDP_HEADER_SIZE + iFrameSize + XRUDP_TAG_SIZE)
		return XRUDP_LIMIT;
	if (pCipher->NextPacket == UINT64_MAX || pCipher->SendCount >= pCipher->PacketLimit)
		return XRUDP_LIMIT;
	memset(&header, 0, sizeof(header));
	header.Type = XRUDP_PACKET_DATA;
	header.PayloadSize = (uint16_t)(iFrameSize + XRUDP_TAG_SIZE);
	header.KeyEpoch = pCipher->SendEpoch;
	header.PacketNumber = pCipher->NextPacket;
	memcpy(header.ConnectionId, pCipher->PeerId, 16);
	result = xrtRudpPacketEncode(packet, sizeof(packet), &size, &header,
		packet + XRUDP_HEADER_SIZE, header.PayloadSize);
	if (result != XRUDP_OK) return result;
	xrudp__nonce(nonce, header.PacketNumber);
	if (!xrtChaCha20Poly1305Seal(pCipher->SendKey, nonce, packet, XRUDP_HEADER_SIZE,
		pFrames, iFrameSize, packet + XRUDP_HEADER_SIZE, header.PayloadSize)) {
		xrtSecureZero(packet, sizeof(packet));
		return XRUDP_IO_ERROR;
	}
	memmove(pOutput, packet, size);
	++pCipher->NextPacket;
	++pCipher->SendCount;
	*pWritten = size;
	xrtSecureZero(packet, sizeof(packet));
	return XRUDP_OK;
}

static xrudpresult xrudp__confirm_check(const xrudpcipher* cipher,
	const unsigned char* plain, size_t size, bool* confirmed)
{
	size_t offset = 0;
	xrudpwireframe frame;
	*confirmed = false;
	while (xrtRudpFrameNext(plain, size, &offset, &frame) == XRUDP_OK) {
		if (frame.Type == XRUDP_FRAME_KEY_CONFIRM) {
			uint32_t epoch = ((uint32_t)frame.Data[0] << 24) |
				((uint32_t)frame.Data[1] << 16) | ((uint32_t)frame.Data[2] << 8) |
				frame.Data[3];
			if (epoch > cipher->SendEpoch) return XRUDP_MALFORMED;
			if (epoch == cipher->SendEpoch) *confirmed = true;
		}
	}
	return XRUDP_OK;
}

xrudpresult xrtRudpCipherOpen(xrudpcipher* pCipher,
	const void* pPacket, size_t iPacketSize, void* pOutput,
	size_t iCapacity, size_t* pWritten)
{
	xrudpwirepacket packet;
	xrudpresult result;
	unsigned char plain[XRUDP_PACKET_MAX], nextKey[XRUDP_KEY_SIZE] = {0}, nonce[12];
	const unsigned char* key;
	size_t size, count;
	bool update = false, confirmed;
	if (pWritten != NULL) *pWritten = 0;
	if (pCipher == NULL || pPacket == NULL || pOutput == NULL || pWritten == NULL)
		return XRUDP_ARGUMENT;
	result = xrtRudpPacketDecode(pPacket, iPacketSize, &packet);
	if (result != XRUDP_OK) return result;
	if (packet.Header.Type != XRUDP_PACKET_DATA ||
		packet.Header.PacketNumber == UINT64_MAX ||
		!xrtConstTimeEqual(packet.Header.ConnectionId, pCipher->LocalId, 16))
		return XRUDP_AUTH_FAILED;
	size = packet.Header.PayloadSize - XRUDP_TAG_SIZE;
	if (iCapacity < size) return XRUDP_LIMIT;
	result = xrtRudpReplayCheck(&pCipher->Replay, packet.Header.PacketNumber);
	if (result != XRUDP_OK) return result;
	if (packet.Header.KeyEpoch == pCipher->ReceiveEpoch) key = pCipher->ReceiveKey;
	else if (pCipher->PreviousEpoch != 0 && packet.Header.KeyEpoch == pCipher->PreviousEpoch)
		key = pCipher->PreviousKey;
	else if (pCipher->ReceiveEpoch != UINT32_MAX &&
		packet.Header.KeyEpoch == pCipher->ReceiveEpoch + 1) {
		if (!xrudp__rekey(pCipher->ReceiveKey, nextKey)) return XRUDP_IO_ERROR;
		key = nextKey;
		update = true;
	} else return XRUDP_AUTH_FAILED;
	xrudp__nonce(nonce, packet.Header.PacketNumber);
	if (!xrtChaCha20Poly1305Open(key, nonce, pPacket, XRUDP_HEADER_SIZE,
		packet.Payload, packet.Header.PayloadSize, plain, sizeof(plain))) {
		result = XRUDP_AUTH_FAILED;
		goto cleanup;
	}
	result = xrtRudpFramesValidate(plain, size, &count);
	if (result != XRUDP_OK) goto cleanup;
	result = xrudp__confirm_check(pCipher, plain, size, &confirmed);
	if (result != XRUDP_OK) goto cleanup;
	/* No fallible operations after this commit point. The caller must still
	 * validate connection state/resources before dispatching any application event. */
	(void)xrtRudpReplayAccept(&pCipher->Replay, packet.Header.PacketNumber);
	if (update) {
		memcpy(pCipher->PreviousKey, pCipher->ReceiveKey, XRUDP_KEY_SIZE);
		pCipher->PreviousEpoch = pCipher->ReceiveEpoch;
		memcpy(pCipher->ReceiveKey, nextKey, XRUDP_KEY_SIZE);
		++pCipher->ReceiveEpoch;
	}
	if (confirmed) pCipher->SendPending = false;
	memmove(pOutput, plain, size);
	*pWritten = size;
	result = XRUDP_OK;
cleanup:
	xrtSecureZero(plain, sizeof(plain));
	xrtSecureZero(nextKey, sizeof(nextKey));
	return result;
}

xrudpresult xrtRudpCipherUpdateKey(xrudpcipher* pCipher)
{
	unsigned char key[XRUDP_KEY_SIZE];
	if (pCipher == NULL) return XRUDP_ARGUMENT;
	if (pCipher->SendPending) return XRUDP_BACKPRESSURE;
	if (pCipher->SendEpoch == UINT32_MAX) return XRUDP_LIMIT;
	if (!xrudp__rekey(pCipher->SendKey, key)) return XRUDP_IO_ERROR;
	memcpy(pCipher->SendKey, key, sizeof(key));
	xrtSecureZero(key, sizeof(key));
	++pCipher->SendEpoch;
	pCipher->SendCount = 0;
	pCipher->SendPending = true;
	return XRUDP_OK;
}

void xrtRudpCipherRetirePrevious(xrudpcipher* pCipher)
{
	if (pCipher != NULL) {
		xrtSecureZero(pCipher->PreviousKey, sizeof(pCipher->PreviousKey));
		pCipher->PreviousEpoch = 0;
	}
}
#endif

#endif
