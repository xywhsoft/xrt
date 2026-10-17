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

/* rudp_transmit 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_TRANSMIT)
#ifndef XRUDP_FEATURE_RUDP_TRANSMIT
#define XRUDP_FEATURE_RUDP_TRANSMIT
#endif
#ifndef XRUDP_MODULE_RUDP_WIRE
#define XRUDP_MODULE_RUDP_WIRE
#endif
#endif

/* rudp_receive 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_RECEIVE)
#ifndef XRUDP_FEATURE_RUDP_RECEIVE
#define XRUDP_FEATURE_RUDP_RECEIVE
#endif
#ifndef XRUDP_MODULE_RUDP_WIRE
#define XRUDP_MODULE_RUDP_WIRE
#endif
#endif

/* rudp_congestion 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_CONGESTION)
#ifndef XRUDP_FEATURE_RUDP_CONGESTION
#define XRUDP_FEATURE_RUDP_CONGESTION
#endif
#ifndef XRUDP_MODULE_RUDP_SENT
#define XRUDP_MODULE_RUDP_SENT
#endif
#endif

/* rudp_sent 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_SENT)
#ifndef XRUDP_FEATURE_RUDP_SENT
#define XRUDP_FEATURE_RUDP_SENT
#endif
#ifndef XRUDP_MODULE_RUDP_ACK
#define XRUDP_MODULE_RUDP_ACK
#endif
#endif

/* rudp_timing 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_TIMING)
#ifndef XRUDP_FEATURE_RUDP_TIMING
#define XRUDP_FEATURE_RUDP_TIMING
#endif
#ifndef XRUDP_MODULE_RUDP_WIRE
#define XRUDP_MODULE_RUDP_WIRE
#endif
#endif

/* rudp_ack 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_ACK)
#ifndef XRUDP_FEATURE_RUDP_ACK
#define XRUDP_FEATURE_RUDP_ACK
#endif
#ifndef XRUDP_MODULE_RUDP_WIRE
#define XRUDP_MODULE_RUDP_WIRE
#endif
#endif

/* rudp_cookie 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_COOKIE)
#ifndef XRUDP_FEATURE_RUDP_COOKIE
#define XRUDP_FEATURE_RUDP_COOKIE
#endif
#ifndef XRUDP_MODULE_RUDP_WIRE
#define XRUDP_MODULE_RUDP_WIRE
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#endif
#endif

/* rudp_noise 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_NOISE)
#ifndef XRUDP_FEATURE_RUDP_NOISE
#define XRUDP_FEATURE_RUDP_NOISE
#endif
#ifndef XRUDP_MODULE_RUDP_CRYPTO
#define XRUDP_MODULE_RUDP_CRYPTO
#endif
#ifndef XRT_MODULE_CRYPTO_X25519
#define XRT_MODULE_CRYPTO_X25519
#endif
#ifndef XRT_MODULE_CRYPTO_HKDF_SHA256
#define XRT_MODULE_CRYPTO_HKDF_SHA256
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

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
#define XRUDP_STREAM_CHUNK_MAX (XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE - XRUDP_FRAME_HEADER_SIZE - 12u)
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
/* Open reports AUTH_FAILED for untrusted header/body failures. DUPLICATE,
 * TOO_OLD and MALFORMED require a verified AEAD tag; MALFORMED means an
 * authenticated frame/protocol violation. LIMIT denotes local output capacity.
 * Do not turn unauthenticated noise into peer failure or path activity. */
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
/* public: extlibs/xrudp/include/xrudp/noise.h */
/* ========================================================================== */

#ifndef XRUDP_NOISE_H
#define XRUDP_NOISE_H


#if defined(XRUDP_FEATURE_RUDP_NOISE)

#define XRUDP_NOISE_PARAMETERS_MAX 256u
#define XRUDP_NOISE_MESSAGE_MAX (96u + XRUDP_NOISE_PARAMETERS_MAX)

typedef struct xrudpnoise xrudpnoise;
typedef enum xrudprole { XRUDP_INITIATOR = 1, XRUDP_RESPONDER = 2 } xrudprole;

/* Bounded, nonblocking, side-effect-free authorization check. The public key
 * has been cryptographically verified; its account/role must still be trusted.
 * Do not consume a one-use grant here: the connection is not yet established.
 * Do not reenter or destroy the currently executing Noise object. */
typedef bool (*xrudpnoisetrustproc)(const unsigned char* pPeerPublic, void* pContext);

typedef struct xrudpnoiseconfig {
	xrudprole Role;
	const void* StaticPrivate; /* 32 bytes; copied, never logged. */
	const void* PeerPublic; /* 32-byte trusted pin; mandatory for initiator. */
	const void* Prologue; /* Canonical protocol/session/role binding. */
	size_t PrologueSize;
	xrudpnoisetrustproc Trust;
	void* TrustContext; /* Borrowed until Destroy; callback runs on owner. */
} xrudpnoiseconfig;

typedef struct xrudpnoiseinfo {
	unsigned int MessageIndex;
	bool Complete;
	bool PeerVerified;
	bool KeysTaken;
	size_t PeerParameterSize;
	unsigned char PeerPublic[32];
	unsigned char HandshakeHash[32]; /* Available only after Complete. */
} xrudpnoiseinfo;

XRT_EXTERN_C_BEGIN

/* Fixed Noise_IK_25519_ChaChaPoly_SHA256, no negotiation/fallback/0-RTT.
 * Responder requires a trusted peer pin or a Trust callback; if both are
 * provided both must pass. Production ephemeral keys always come from XRT's
 * OS secure random source. The caller handles address cookies, wire framing,
 * cached retransmission, deadlines and final key confirmation.
 * Complete means Noise key derivation completed, NOT application admission
 * or peer liveness confirmation. Never execute handshake parameters as data. */
XRUDP_API xrudpresult xrtRudpNoiseCreate(const xrudpnoiseconfig* pConfig,
	xrudpnoise** ppNoise);
XRUDP_API void xrtRudpNoiseDestroy(xrudpnoise* pNoise);
XRUDP_API xrudpresult xrtRudpNoiseInfo(const xrudpnoise* pNoise, xrudpnoiseinfo* pInfo);

/* Write exactly once for each local flight; cache its returned bytes for loss
 * recovery. A retry of an already successful Write returns BACKPRESSURE/DONE.
 * pParameters contains protocol parameters only. Failure changes no state or
 * output bytes, and *pWritten=0. Buffers/counters must not alias the object. */
XRUDP_API xrudpresult xrtRudpNoiseWrite(xrudpnoise* pNoise,
	const void* pParameters, size_t iParameterSize,
	void* pOutput, size_t iCapacity, size_t* pWritten);
/* Candidate state is discarded on an invalid/forged flight, leaving the prior
 * live state intact. Duplicate flights must be handled by cached wire messages
 * in the connection driver, not by advancing this state machine twice. */
XRUDP_API xrudpresult xrtRudpNoiseRead(xrudpnoise* pNoise,
	const void* pMessage, size_t iMessageSize);
XRUDP_API xrudpresult xrtRudpNoisePeerParameters(const xrudpnoise* pNoise,
	void* pOutput, size_t iCapacity, size_t* pWritten);
/* Transfer keys into exactly ONE packet protector, then clear their copies.
 * This prevents constructing multiple packet-number domains with one key pair.
 * Call only after higher-level identity/parameter policy accepts the handshake.
 * The canonical Prologue must bind the IDs passed here and the application. */
XRUDP_API xrudpresult xrtRudpNoiseCreateCipher(xrudpnoise* pNoise,
	const void* pLocalId, const void* pPeerId, uint64_t iPacketLimit,
	xrudpcipher** ppCipher);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrudp/include/xrudp/cookie.h */
/* ========================================================================== */

#ifndef XRUDP_COOKIE_H
#define XRUDP_COOKIE_H


#if defined(XRUDP_FEATURE_RUDP_COOKIE)

#define XRUDP_COOKIE_SIZE 40u
#define XRUDP_COOKIE_MAX_AGE_US UINT64_C(30000000)

/* API representation only; never memcpy this C structure onto the wire.
 * IPv4 uses Address[0..3] and twelve zero tail bytes, Zone=0.
 * IPv6 uses all 16 address bytes; Zone is the local interface scope ID.
 * Port/Zone are host-order. Unspecified and multicast addresses are rejected. */
typedef struct xrudpaddress {
	uint8_t Family; /* 4 or 6. */
	uint16_t Port;
	uint32_t Zone;
	unsigned char Address[16];
} xrudpaddress;

XRT_EXTERN_C_BEGIN

/* Stateless address-return validation, NOT peer identity or a one-use grant.
 * Key is a 32-byte server secret generated securely per endpoint generation.
 * Scope is a nonzero 16-byte listener/session domain. HelloHash is the 32-byte
 * digest of the canonical client probe, including connection IDs and target.
 * Time uses the server's monotonic microseconds; tokens expire at most 30s
 * after issuance. The driver enforces amplification/CPU/rate limits separately.
 * No allocation, random generation, logging or I/O. Failure preserves output. */
XRUDP_API xrudpresult xrtRudpCookieCreate(const void* pKey,
	const void* pScope, const void* pHelloHash, const xrudpaddress* pAddress,
	uint64_t iNowUs, void* pCookie, size_t iCapacity);
/* PreviousKey may be NULL, or the immediately previous endpoint secret during
 * a bounded rotation grace period. iMaxAgeUs=0 selects the fixed maximum.
 * Authentication is checked before reporting an expired/future timestamp.
 * Keys and caller policy remain unchanged on every result. */
XRUDP_API xrudpresult xrtRudpCookieVerify(const void* pCurrentKey,
	const void* pPreviousKey, const void* pScope, const void* pHelloHash,
	const xrudpaddress* pAddress, uint64_t iNowUs, uint64_t iMaxAgeUs,
	const void* pCookie, size_t iCookieSize);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrudp/include/xrudp/ack.h */
/* ========================================================================== */

#ifndef XRUDP_ACK_H
#define XRUDP_ACK_H


#if defined(XRUDP_FEATURE_RUDP_ACK)

typedef struct xrudpackrange { uint64_t First, Last; } xrudpackrange;
typedef struct xrudpack {
	uint32_t DelayUs;
	uint16_t Count;
	xrudpackrange Ranges[XRUDP_ACK_RANGES_MAX];
} xrudpack;

/* Single-owner state initialized by Init. Only the newest 32 disjoint ranges
 * are retained; missing ranges are not negative acknowledgments. No allocation.
 * A connection records only authenticated, fully admitted packets. This object
 * neither schedules ACKs nor updates receive flow-control credit. */
typedef struct xrudpackset {
	uint16_t Count;
	uint64_t LargestAtUs;
	xrudpackrange Ranges[XRUDP_ACK_RANGES_MAX];
} xrudpackset;

XRT_EXTERN_C_BEGIN

XRUDP_API void xrtRudpAckSetInit(xrudpackset* pSet);
/* Test actual retained transport admission. An authenticated crypto-layer
 * duplicate is not itself proof that stream/resource admission previously
 * succeeded. A driver may re-ACK it only if this history contains it. */
XRUDP_API bool xrtRudpAckSetContains(const xrudpackset* pSet,
	uint64_t iPacketNumber);
/* DUPLICATE/TOO_OLD leave state unchanged. TOO_OLD means a new isolated range
 * fell below the bounded retained history; the sender can recover data using
 * a fresh packet number. Now must not precede the current largest packet time. */
XRUDP_API xrudpresult xrtRudpAckSetRecord(xrudpackset* pSet,
	uint64_t iPacketNumber, uint64_t iNowUs);
/* Encode one complete ACK frame; retains history so lost ACKs can be repeated.
 * The connection controls ACK-eliciting rules to avoid ACK-of-ACK loops.
 * Empty history returns DONE; failure changes no destination bytes, written=0. */
XRUDP_API xrudpresult xrtRudpAckSetEncode(const xrudpackset* pSet,
	uint64_t iNowUs, void* pOutput, size_t iCapacity, size_t* pWritten);
/* Decode exactly one canonical ACK frame, rejecting trailing frames/bytes.
 * This does not check peer authentication or whether packet numbers were sent.
 * Output is unchanged on failure. */
XRUDP_API xrudpresult xrtRudpAckDecode(const void* pFrame, size_t iSize,
	xrudpack* pAck);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrudp/include/xrudp/timing.h */
/* ========================================================================== */

#ifndef XRUDP_TIMING_H
#define XRUDP_TIMING_H


#if defined(XRUDP_FEATURE_RUDP_TIMING)

#define XRUDP_INITIAL_RTT_US UINT64_C(333000)
#define XRUDP_TIMER_GRANULARITY_US UINT64_C(1000)
#define XRUDP_ACK_DELAY_MAX_US 25000u

typedef struct xrudprtt {
	uint64_t LatestUs;
	uint64_t MinimumUs;
	uint64_t SmoothedUs;
	uint64_t VariationUs;
	uint32_t MaxAckDelayUs;
	bool HasSample;
} xrudprtt;

XRT_EXTERN_C_BEGIN

/* All units are monotonic microseconds. InitialRtt=0 selects 333ms.
 * MaxAckDelay may be zero (handshake/low-latency profile) and must not exceed
 * the fixed 25ms protocol ceiling. No global clock, allocation or I/O.
 * Owner serializes calls; failure preserves output/state. */
XRUDP_API xrudpresult xrtRudpRttInit(xrudprtt* pRtt,
	uint64_t iInitialRttUs, uint32_t iMaxAckDelayUs);
/* Sample is the measured send-to-ACK interval, never a peer-supplied RTT.
 * A zero measured interval is rounded to 1us. Peer ACK delay is capped by
 * local policy and cannot reduce the sample below observed minimum RTT. */
XRUDP_API xrudpresult xrtRudpRttSample(xrudprtt* pRtt,
	uint64_t iMeasuredUs, uint32_t iPeerAckDelayUs);
/* PTO = smoothed + max(4*variation, granularity) + bounded max ACK delay,
 * with exponential backoff. Integer overflow saturates at UINT64_MAX.
 * Reset backoff only on real authenticated acknowledgment progress. */
XRUDP_API xrudpresult xrtRudpRttPto(const xrudprtt* pRtt,
	unsigned int iBackoff, uint64_t* pIntervalUs);
/* ceil(9/8 * max(latest,smoothed)), no less than timer granularity. */
XRUDP_API xrudpresult xrtRudpRttLossDelay(const xrudprtt* pRtt,
	uint64_t* pIntervalUs);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrudp/include/xrudp/sent.h */
/* ========================================================================== */

#ifndef XRUDP_SENT_H
#define XRUDP_SENT_H


#if defined(XRUDP_FEATURE_RUDP_SENT)

#define XRUDP_SENT_CAPACITY_DEFAULT 256u
#define XRUDP_SENT_CAPACITY_MAX 4096u
#define XRUDP_SENT_PROBE_RESERVE 32u
#define XRUDP_SENT_ACK_ELICITING 1u
#define XRUDP_SENT_PROBE 2u
#define XRUDP_SENT_PADDING 4u

typedef struct xrudpsentwindow xrudpsentwindow;
typedef enum xrudpsenteventkind {
	XRUDP_SENT_ACKED = 1,
	XRUDP_SENT_LOST = 2,
	XRUDP_SENT_EXPIRED = 3,
	XRUDP_SENT_CANCELLED = 4
} xrudpsenteventkind;

typedef struct xrudpsentevent {
	uint64_t PacketNumber;
	uint64_t UserTag;
	uint64_t SentAtUs;
	uint32_t Bytes;
	uint32_t Flags;
	uint32_t Kind;
	uint32_t Reserved;
} xrudpsentevent;

typedef struct xrudpsentstate {
	uint64_t LargestSent;
	uint64_t LargestAcked;
	uint64_t BytesInFlight;
	uint64_t LastNowUs; /* Mutation time, not peer liveness/progress. */
	uint32_t NormalCapacity;
	uint32_t TotalCapacity;
	uint32_t StoredPackets;
	uint32_t PendingPackets;
	uint32_t Stopped;
	uint32_t Reserved;
} xrudpsentstate;

typedef struct xrudpackchange {
	uint64_t LargestAcked;
	uint64_t NewlyAckedBytes;
	uint64_t RttSampleUs;
	uint32_t NewlyAckedPackets;
	uint32_t NewlyAckedEliciting;
	uint32_t RttSampleValid;
	uint32_t PeerAckDelayUs;
} xrudpackchange;

XRT_EXTERN_C_BEGIN

/* One fixed allocation, no allocation during processing. Capacity=0 uses 256;
 * 32 additional slots are reserved for PTO probes. Serialized by one owner.
 * All output buffers/objects must be disjoint from inputs and window storage.
 * Numeric UserTags are borrowed identifiers, not owned/released pointers.
 * Each tracked packet produces at most one final ACKED/LOST/EXPIRED/CANCELLED
 * event. A late ACK of a declared-lost packet does not produce a second event.
 * Logical data may retransmit under a new packet; deduplication is stream-level. */
XRUDP_API xrudpresult xrtRudpSentCreate(uint32_t iCapacity, xrudpsentwindow** ppWindow);
XRUDP_API void xrtRudpSentDestroy(xrudpsentwindow* pWindow);
XRUDP_API xrudpresult xrtRudpSentState(const xrudpsentwindow* pWindow, xrudpsentstate* pState);
XRUDP_API bool xrtRudpSentCanRecord(const xrudpsentwindow* pWindow, bool bProbe);
/* Record every DATA packet actually admitted to UDP output, including ACK-only
 * packets. Numbers strictly increase, gaps are permitted for failed sends.
 * Check capacity under the same serialized owner before sealing/sending.
 * Probe must be ACK-eliciting. PADDING marks padded packets, which count toward
 * congestion even when not ACK-eliciting. Bytes is the protected UDP payload. */
XRUDP_API xrudpresult xrtRudpSentRecord(xrudpsentwindow* pWindow,
	uint64_t iPacketNumber, uint64_t iNowUs, uint32_t iBytes,
	uint32_t iFlags, uint64_t iUserTag);
/* Requires an authenticated canonical ACK. Future ACKs and ACKs covering unsent
 * gaps inside retained history fail atomically. Retired history is ignored and
 * can never grant bytes/progress again. Backpressure means event output is too
 * small; nothing is changed and the caller may retry its retained ACK value.
 * Failure leaves change/events unchanged and *pCount=0. */
XRUDP_API xrudpresult xrtRudpSentAcknowledge(xrudpsentwindow* pWindow,
	const xrudpack* pAck, uint64_t iNowUs,
	xrudpsentevent* pEvents, size_t iCapacity, size_t* pCount,
	xrudpackchange* pChange);
/* Packet threshold=3; time threshold only applies below largest acknowledged.
 * ACK-only metadata expires independently without a congestion-loss signal.
 * Delays are monotonic microseconds and must be >=1ms. Does not declare all
 * outstanding data lost merely because a PTO fires; PTO probing is separate. */
XRUDP_API xrudpresult xrtRudpSentDetectLoss(xrudpsentwindow* pWindow,
	uint64_t iLossDelayUs, uint64_t iAckOnlyAgeUs, uint64_t iNowUs,
	xrudpsentevent* pEvents, size_t iCapacity, size_t* pCount);
/* Complete pending packets as CANCELLED, then stop accepting new records.
 * Caller drains/cancels higher-level stream operations before Destroy. */
XRUDP_API xrudpresult xrtRudpSentAbort(xrudpsentwindow* pWindow,
	xrudpsentevent* pEvents, size_t iCapacity, size_t* pCount);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrudp/include/xrudp/congestion.h */
/* ========================================================================== */

#ifndef XRUDP_CONGESTION_H
#define XRUDP_CONGESTION_H


#if defined(XRUDP_FEATURE_RUDP_CONGESTION)

#define XRUDP_CC_INITIAL_WINDOW (10u * XRUDP_PACKET_MAX)
#define XRUDP_CC_MINIMUM_WINDOW (2u * XRUDP_PACKET_MAX)
#define XRUDP_CC_DEFAULT_MAXIMUM (4u * 1024u * 1024u)
#define XRUDP_CC_MAXIMUM_LIMIT (64u * 1024u * 1024u)
#define XRUDP_CC_BURST_BYTES (2u * XRUDP_PACKET_MAX)

/* One owner; initialize before use. Read-only snapshots are ordinary copies.
 * This is local sender policy, never populated from peer-provided state. */
typedef struct xrudpcongestion {
	uint64_t Window;
	uint64_t SlowStartThreshold;
	uint64_t BytesInFlight;
	uint64_t MaximumWindow;
	uint64_t RecoveryThrough;
	uint64_t LargestSent;
	uint64_t AvoidanceCredit;
	uint64_t PacerNextUs;
	uint64_t LastNowUs;
} xrudpcongestion;

XRT_EXTERN_C_BEGIN

/* Maximum=0 defaults to 4MiB; allowed range is initial window..64MiB.
 * Fixed datagram ceiling 1200, initial window=10 datagrams, minimum=2. */
XRUDP_API xrudpresult xrtRudpCongestionInit(xrudpcongestion* pState, uint64_t iMaximumWindow);
/* A pure admission query. BACKPRESSURE: cwnd/hard flight budget; TIMEOUT: wait
 * until *pDueUs for pacing; OK: admitted now. Due is unchanged on other errors.
 * ACK-only bypasses cwnd/pacing; endpoint still must bound control traffic.
 * PROBE may exceed cwnd but not the hard budget and never bypasses pacing.
 * Only the recovery driver may grant up to two PROBE packets per actual PTO. */
XRUDP_API xrudpresult xrtRudpCongestionCanSend(const xrudpcongestion* pState,
	uint64_t iNowUs, uint64_t iSmoothedRttUs, uint32_t iBytes,
	uint32_t iFlags, uint64_t* pDueUs);
/* Commit after UDP admission under the same serialized owner as CanSend and
 * the sent ledger. Records all DATA packet numbers, including ACK-only. */
XRUDP_API xrudpresult xrtRudpCongestionOnSent(xrudpcongestion* pState,
	uint64_t iPacketNumber, uint64_t iNowUs, uint64_t iSmoothedRttUs,
	uint32_t iBytes, uint32_t iFlags);
/* Consume unique final events from Sent exactly once, before any new send.
 * Caller must combine all loss and ACK events from the same ACK processing in
 * one batch (losses are applied before growth regardless of array order).
 * Application-limited or flow-control-limited ACKs do not increase cwnd.
 * No allocations/callbacks. Errors leave the entire state unchanged. */
XRUDP_API xrudpresult xrtRudpCongestionApply(xrudpcongestion* pState,
	const xrudpsentevent* pEvents, size_t iCount, uint64_t iNowUs,
	bool bApplicationLimited);
/* Call only after the recovery driver proves a persistent congestion period,
 * not on a single PTO. The driver owns the consecutive-loss/RTT evidence. */
XRUDP_API xrudpresult xrtRudpCongestionCollapse(xrudpcongestion* pState, uint64_t iNowUs);

XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrudp/include/xrudp/receive.h */
/* ========================================================================== */

#ifndef XRUDP_RECEIVE_H
#define XRUDP_RECEIVE_H


#if defined(XRUDP_FEATURE_RUDP_RECEIVE)

#define XRUDP_RECEIVE_CAPACITY_DEFAULT 65536u
#define XRUDP_RECEIVE_CAPACITY_MIN 256u
#define XRUDP_RECEIVE_CAPACITY_MAX (1024u * 1024u)

typedef struct xrudpreceive xrudpreceive;
typedef struct xrudpreceivestate {
	uint64_t ReadOffset;
	uint64_t HighestOffset;
	uint64_t FinalOffset;
	uint64_t Limit;
	uint64_t BufferedBytes;
	uint32_t Capacity;
	uint32_t HasFinal;
	uint32_t Reset;
	uint32_t ResetReason;
} xrudpreceivestate;

typedef struct xrudpreceivechange {
	uint64_t HighestDelta; /* Connection flow-control cost, includes holes. */
	uint64_t NewBytes; /* Newly buffered bytes, excludes duplicates/consumed data. */
	uint64_t ConsumedDelta; /* RESET discards all outstanding logical offsets. */
	uint32_t NewFinal;
	uint32_t Reserved;
} xrudpreceivechange;

XRT_EXTERN_C_BEGIN

/* Capacity=0 defaults to 64KiB, otherwise a power of two in [256,1MiB].
 * InitialLimit is explicit (zero means zero credit) and <=Capacity.
 * One fixed data ring + per-byte bitmap allocation; no fragment allocations.
 * Owner must reserve aggregate endpoint/connection budgets BEFORE Create. */
XRUDP_API xrudpresult xrtRudpReceiveAllocationSize(uint32_t iCapacity, size_t* pSize);
XRUDP_API xrudpresult xrtRudpReceiveCreate(uint32_t iCapacity,
	uint64_t iInitialLimit, xrudpreceive** ppReceive);
XRUDP_API void xrtRudpReceiveDestroy(xrudpreceive* pReceive);
XRUDP_API xrudpresult xrtRudpReceiveState(const xrudpreceive* pReceive, xrudpreceivestate* pState);
/* Pure preflight for ONE authenticated STREAM frame. At most one datagram's
 * STREAM chunk; total work is bounded by its payload, not its offset/gaps.
 * MALFORMED includes conflicting overlap, final size, offset overflow or credit
 * violation. DUPLICATE is harmless and still fills an all-zero change value.
 * Inputs/outputs must be disjoint. Failure leaves output/state unchanged. */
XRUDP_API xrudpresult xrtRudpReceiveCheck(const xrudpreceive* pReceive,
	uint64_t iOffset, const void* pData, size_t iSize, bool bFin,
	xrudpreceivechange* pChange);
/* Commit one frame; rechecks before writing anything. Connection driver still
 * MUST reserve total connection credit and transact the complete packet before
 * ACK admission. Calling Push successively is not a multi-frame transaction. */
XRUDP_API xrudpresult xrtRudpReceivePush(xrudpreceive* pReceive,
	uint64_t iOffset, const void* pData, size_t iSize, bool bFin,
	xrudpreceivechange* pChange);
/* Copies and consumes only contiguous data. BACKPRESSURE means a receive hole;
 * DONE is clean EOF, CANCELLED is peer RESET, both with zero read count.
 * EOF/RESET are never inferred merely from an empty buffer. Capacity must >0.
 * Successful consumption does not itself advertise additional peer credit. */
XRUDP_API xrudpresult xrtRudpReceiveRead(xrudpreceive* pReceive,
	void* pOutput, size_t iCapacity, size_t* pRead);
/* Monotonic explicit credit, <=ReadOffset+Capacity, saturating at UINT64_MAX.
 * Owner sends/retransmits the latest MAX_STREAM_DATA value. */
XRUDP_API xrudpresult xrtRudpReceiveExtend(xrudpreceive* pReceive, uint64_t iLimit);
/* Final size is charged even if missing bytes are discarded. Duplicated RESET
 * has no new cost. An inconsistent final size/reason is a peer protocol error.
 * ReadOffset advances to final size and ConsumedDelta lets connection credit
 * release all logical offsets exactly once. Buffered plaintext is cleared.
 * A matching RESET after clean EOF consumption is ignored as DUPLICATE. */
XRUDP_API xrudpresult xrtRudpReceiveReset(xrudpreceive* pReceive,
	uint64_t iFinalOffset, uint32_t iReason, xrudpreceivechange* pChange);

XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrudp/include/xrudp/transmit.h */
/* ========================================================================== */

#ifndef XRUDP_TRANSMIT_H
#define XRUDP_TRANSMIT_H


#if defined(XRUDP_FEATURE_RUDP_TRANSMIT)

#define XRUDP_TRANSMIT_CAPACITY_DEFAULT 65536u
#define XRUDP_TRANSMIT_CAPACITY_MIN 256u
#define XRUDP_TRANSMIT_CAPACITY_MAX (1024u * 1024u)

typedef struct xrudptransmit xrudptransmit;
typedef struct xrudptransmitstate {
	uint64_t BaseOffset;
	uint64_t QueuedEnd;
	uint64_t HighestSent;
	uint64_t PeerLimit;
	uint64_t PendingBytes;
	uint32_t Capacity;
	uint32_t FinQueued;
	uint32_t FinSent;
	uint32_t FinAcknowledged;
	uint32_t Cancelled;
	uint32_t Reserved;
} xrudptransmitstate;

typedef struct xrudptransmitframe {
	uint64_t Offset;
	uint64_t HighestDelta; /* New connection credit required if sent. */
	uint32_t Size;
	uint32_t Fin;
} xrudptransmitframe;

typedef struct xrudptransmitchange {
	uint64_t HighestDelta;
	uint64_t NewlyAckedBytes;
	uint64_t ReleasedBytes; /* Contiguous ring capacity returned. */
	uint32_t NewFinAcknowledged;
	uint32_t Reserved;
} xrudptransmitchange;

XRT_EXTERN_C_BEGIN

/* One fixed ring + bounded bitmaps. Capacity=0 means 64KiB, otherwise a power
 * of two in [256,1MiB]. Reserve aggregate budgets before allocating. */
XRUDP_API xrudpresult xrtRudpTransmitAllocationSize(uint32_t iCapacity, size_t* pSize);
XRUDP_API xrudpresult xrtRudpTransmitCreate(uint32_t iCapacity,
	uint64_t iPeerLimit, xrudptransmit** ppTransmit);
XRUDP_API void xrtRudpTransmitDestroy(xrudptransmit* pTransmit);
XRUDP_API xrudpresult xrtRudpTransmitState(const xrudptransmit* pTransmit, xrudptransmitstate* pState);
/* Copies a bounded prefix; written may be smaller than Size. Buffering success
 * is not peer acknowledgement. Backpressure/other failures set *pWritten=0.
 * No write is accepted after Finish/Cancel. Inputs/outputs must be disjoint. */
XRUDP_API xrudpresult xrtRudpTransmitWrite(xrudptransmit* pTransmit,
	const void* pData, size_t iSize, size_t* pWritten);
XRUDP_API xrudpresult xrtRudpTransmitFinish(xrudptransmit* pTransmit);
/* Old/same advertised limits are harmless DUPLICATE, never a credit decrease. */
XRUDP_API xrudpresult xrtRudpTransmitSetLimit(xrudptransmit* pTransmit, uint64_t iLimit);
/* Copies the first ready chunk (at most one STREAM chunk), without mutation.
 * Probe selects the oldest unacknowledged bytes even if still in flight; ONLY
 * the recovery driver may request this under PTO packet/congestion budgets.
 * Capacity>0; caller packet buffer and frame must be disjoint. Outputs remain
 * unchanged on failure. A successful zero-size frame always carries FIN. */
XRUDP_API xrudpresult xrtRudpTransmitPeek(const xrudptransmit* pTransmit,
	bool bProbe, void* pOutput, size_t iCapacity, xrudptransmitframe* pFrame);
/* Called exactly after packet admission under one serialized owner. The driver
 * retains Offset/Size/Fin in its bounded packet references until final feedback.
 * Lost data is retransmitted in a NEW packet number; this buffer owns no PN. */
XRUDP_API xrudpresult xrtRudpTransmitMarkSent(xrudptransmit* pTransmit,
	uint64_t iOffset, size_t iSize, bool bFin, xrudptransmitchange* pChange);
/* Only trusted ACKED/LOST packet references from the send ledger may resolve.
 * Any copy's ACK retires logical bytes; late loss of another copy cannot revive
 * them. Peer frames must never invoke this by unauthenticated byte offsets.
 * Duplicate/retired feedback is harmless. Errors leave state/output unchanged.
 * FIN acknowledgement completes the write half only after BaseOffset=QueuedEnd. */
XRUDP_API xrudpresult xrtRudpTransmitResolve(xrudptransmit* pTransmit,
	uint64_t iOffset, size_t iSize, bool bFin, bool bAcknowledged,
	xrudptransmitchange* pChange);
/* Clears plaintext and bitmaps, releases all held ring capacity, then rejects
 * future operations. Late ledger events must be drained by the owning driver. */
XRUDP_API xrudpresult xrtRudpTransmitCancel(xrudptransmit* pTransmit, xrudptransmitchange* pChange);

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
/* internal: extlibs/xrudp/src/internal/xrudp_noise.h */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_NOISE)
#ifndef XRUDP_NOISE_INTERNAL_H
#define XRUDP_NOISE_INTERNAL_H

#include <string.h>

struct xrudpnoise {
	xrudprole Role;
	unsigned int Index;
	bool HasPin, Trusted, Taken;
	xrudpnoisetrustproc Trust;
	void* TrustContext;
	unsigned char StaticPrivate[32], StaticPublic[32];
	unsigned char EphemeralPrivate[32], EphemeralPublic[32];
	unsigned char RemoteStatic[32], RemoteEphemeral[32], Pin[32];
	unsigned char Hash[32], Chain[32], CipherKey[32];
	uint64_t Nonce;
	unsigned char SendKey[32], ReceiveKey[32];
	unsigned char PeerParameters[XRUDP_NOISE_PARAMETERS_MAX];
	size_t PeerParameterSize;
};

/* Internal vector-test seam. NULL ephemeral uses the OS secure random source.
 * Not a public/XS symbol; production callers must use xrtRudpNoiseCreate. */
xrudpresult xrudp__noise_create(const xrudpnoiseconfig* config,
	const void* deterministicEphemeral, xrudpnoise** output);

#endif
#endif


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
	xrudpresult replayResult;
	size_t size, count;
	bool update = false, confirmed;
	if (pWritten != NULL) *pWritten = 0;
	if (pCipher == NULL || pPacket == NULL || pOutput == NULL || pWritten == NULL)
		return XRUDP_ARGUMENT;
	result = xrtRudpPacketDecode(pPacket, iPacketSize, &packet);
	/* A wire error before AEAD is untrusted input, not an authenticated peer
	 * violation. The caller must be able to drop it without closing a session. */
	if (result != XRUDP_OK) return XRUDP_AUTH_FAILED;
	if (packet.Header.Type != XRUDP_PACKET_DATA ||
		packet.Header.PacketNumber == UINT64_MAX ||
		!xrtConstTimeEqual(packet.Header.ConnectionId, pCipher->LocalId, 16))
		return XRUDP_AUTH_FAILED;
	size = packet.Header.PayloadSize - XRUDP_TAG_SIZE;
	if (iCapacity < size) return XRUDP_LIMIT;
	replayResult = xrtRudpReplayCheck(&pCipher->Replay, packet.Header.PacketNumber);
	if (replayResult != XRUDP_OK && replayResult != XRUDP_DUPLICATE &&
		replayResult != XRUDP_TOO_OLD) return replayResult;
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
	/* A matching packet number is not proof of authenticity. The driver may
	 * ACK an authenticated duplicate or use it as evidence of path activity. */
	if (replayResult != XRUDP_OK) {
		result = replayResult;
		goto cleanup;
	}
	result = xrtRudpFramesValidate(plain, size, &count);
	if (result != XRUDP_OK) {
		result = XRUDP_MALFORMED;
		goto cleanup;
	}
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


/* ========================================================================== */
/* source: extlibs/xrudp/src/crypto/rudp_noise.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_NOISE)

static bool xrudp__noise_hash(xrudpnoise* n, const void* data, size_t size)
{
	xsha256 hash;
	bool result;
	xrtSha256Init(&hash);
	result = xrtSha256Update(&hash, n->Hash, 32) &&
		xrtSha256Update(&hash, data, size) && xrtSha256Final(&hash, n->Hash);
	xrtSecureZero(&hash, sizeof(hash));
	return result;
}

static bool xrudp__noise_mix(xrudpnoise* n, const unsigned char privateKey[32],
	const unsigned char publicKey[32])
{
	unsigned char shared[32] = {0}, keys[64] = {0};
	bool result = xrtX25519Shared(privateKey, publicKey, shared) &&
		xrtHkdfSha256(n->Chain, 32, shared, 32, NULL, 0, keys, sizeof(keys));
	if (result) {
		memcpy(n->Chain, keys, 32);
		memcpy(n->CipherKey, keys + 32, 32);
		n->Nonce = 0;
	}
	xrtSecureZero(shared, sizeof(shared));
	xrtSecureZero(keys, sizeof(keys));
	return result;
}

static void xrudp__noise_nonce(unsigned char nonce[12], uint64_t number)
{
	memset(nonce, 0, 4);
	for (size_t i = 0; i < 8; ++i) nonce[i + 4] = (unsigned char)(number >> (i * 8));
}

static bool xrudp__noise_encrypt(xrudpnoise* n, const void* plain,
	size_t size, unsigned char* output)
{
	unsigned char nonce[12];
	if (n->Nonce == UINT64_MAX) return false;
	xrudp__noise_nonce(nonce, n->Nonce);
	if (!xrtChaCha20Poly1305Seal(n->CipherKey, nonce, n->Hash, 32,
		plain, size, output, size + XRUDP_TAG_SIZE) ||
		!xrudp__noise_hash(n, output, size + XRUDP_TAG_SIZE)) return false;
	++n->Nonce;
	return true;
}

static bool xrudp__noise_decrypt(xrudpnoise* n, const unsigned char* input,
	size_t size, void* output)
{
	unsigned char nonce[12];
	if (size < XRUDP_TAG_SIZE || n->Nonce == UINT64_MAX) return false;
	xrudp__noise_nonce(nonce, n->Nonce);
	if (!xrtChaCha20Poly1305Open(n->CipherKey, nonce, n->Hash, 32,
		input, size, output, size - XRUDP_TAG_SIZE) ||
		!xrudp__noise_hash(n, input, size)) return false;
	++n->Nonce;
	return true;
}

static bool xrudp__noise_split(xrudpnoise* n)
{
	unsigned char keys[64] = {0};
	bool result = xrtHkdfSha256(n->Chain, 32, NULL, 0, NULL, 0, keys, sizeof(keys));
	if (result) {
		size_t sendOffset = n->Role == XRUDP_INITIATOR ? 0 : 32;
		memcpy(n->SendKey, keys + sendOffset, 32);
		memcpy(n->ReceiveKey, keys + 32 - sendOffset, 32);
		/* No more handshake crypto is needed. Keep only transcript/public
		 * identity/peer parameters and the once-transferable traffic keys. */
		xrtSecureZero(n->StaticPrivate, 32);
		xrtSecureZero(n->EphemeralPrivate, 32);
		xrtSecureZero(n->Chain, 32);
		xrtSecureZero(n->CipherKey, 32);
		n->Nonce = 0;
	}
	xrtSecureZero(keys, sizeof(keys));
	return result;
}

static bool xrudp__noise_authorize(xrudpnoise* n)
{
	if (n->HasPin && !xrtConstTimeEqual(n->RemoteStatic, n->Pin, 32)) return false;
	if (n->Trust != NULL && !n->Trust(n->RemoteStatic, n->TrustContext)) return false;
	n->Trusted = true;
	return true;
}

xrudpresult xrudp__noise_create(const xrudpnoiseconfig* config,
	const void* deterministicEphemeral, xrudpnoise** output)
{
	static const char protocol[] = "Noise_IK_25519_ChaChaPoly_SHA256";
	xrudpnoise* n;
	bool ok;
	if (config == NULL || output == NULL || config->StaticPrivate == NULL ||
		(config->Role != XRUDP_INITIATOR && config->Role != XRUDP_RESPONDER) ||
		(config->PeerPublic == NULL && (config->Role == XRUDP_INITIATOR || config->Trust == NULL)) ||
		(config->Prologue == NULL && config->PrologueSize != 0) ||
		config->PrologueSize > XRUDP_NOISE_PARAMETERS_MAX) return XRUDP_ARGUMENT;
	n = (xrudpnoise*)xrtMalloc(sizeof(*n));
	if (n == NULL) return XRUDP_NO_MEMORY;
	memset(n, 0, sizeof(*n));
	n->Role = config->Role;
	n->Trust = config->Trust;
	n->TrustContext = config->TrustContext;
	n->HasPin = config->PeerPublic != NULL;
	memcpy(n->StaticPrivate, config->StaticPrivate, 32);
	if (n->HasPin) memcpy(n->Pin, config->PeerPublic, 32);
	if (n->Role == XRUDP_INITIATOR) memcpy(n->RemoteStatic, n->Pin, 32);
	if (deterministicEphemeral != NULL) {
		memcpy(n->EphemeralPrivate, deterministicEphemeral, 32);
		ok = true;
	} else ok = xrtSecureRandom(n->EphemeralPrivate, 32);
	ok = ok && xrtX25519Public(n->StaticPrivate, n->StaticPublic) &&
		xrtX25519Public(n->EphemeralPrivate, n->EphemeralPublic);
	if (ok) {
		/* The fixed protocol name is exactly HASHLEN bytes; Noise pads shorter
		 * names and hashes longer ones. No terminator is part of the name. */
		if (sizeof(protocol) - 1 <= sizeof(n->Hash)) memcpy(n->Hash, protocol, sizeof(protocol) - 1);
		else ok = xrtSha256(protocol, sizeof(protocol) - 1, n->Hash);
		memcpy(n->Chain, n->Hash, 32);
		ok = ok && xrudp__noise_hash(n, config->Prologue, config->PrologueSize) &&
			xrudp__noise_hash(n, n->Role == XRUDP_INITIATOR ? n->RemoteStatic : n->StaticPublic, 32);
	}
	if (!ok) {
		xrtRudpNoiseDestroy(n);
		return XRUDP_IO_ERROR;
	}
	*output = n;
	return XRUDP_OK;
}

xrudpresult xrtRudpNoiseCreate(const xrudpnoiseconfig* pConfig, xrudpnoise** ppNoise)
{
	return xrudp__noise_create(pConfig, NULL, ppNoise);
}

void xrtRudpNoiseDestroy(xrudpnoise* pNoise)
{
	if (pNoise != NULL) {
		xrtSecureZero(pNoise, sizeof(*pNoise));
		xrtFree(pNoise);
	}
}

xrudpresult xrtRudpNoiseInfo(const xrudpnoise* pNoise, xrudpnoiseinfo* pInfo)
{
	xrudpnoiseinfo info;
	if (pNoise == NULL || pInfo == NULL) return XRUDP_ARGUMENT;
	memset(&info, 0, sizeof(info));
	info.MessageIndex = pNoise->Index;
	info.Complete = pNoise->Index == 2;
	info.PeerVerified = pNoise->Trusted;
	info.KeysTaken = pNoise->Taken;
	if (pNoise->Trusted) memcpy(info.PeerPublic, pNoise->RemoteStatic, 32);
	if (info.Complete) {
		memcpy(info.HandshakeHash, pNoise->Hash, 32);
		info.PeerParameterSize = pNoise->PeerParameterSize;
	}
	*pInfo = info;
	return XRUDP_OK;
}

xrudpresult xrtRudpNoiseWrite(xrudpnoise* pNoise,
	const void* pParameters, size_t iParameterSize,
	void* pOutput, size_t iCapacity, size_t* pWritten)
{
	xrudpnoise candidate;
	unsigned char message[XRUDP_NOISE_MESSAGE_MAX];
	xrudpresult result = XRUDP_IO_ERROR;
	size_t prefix, total;
	bool ok;
	if (pWritten != NULL) *pWritten = 0;
	if (pNoise == NULL || pOutput == NULL || pWritten == NULL ||
		(pParameters == NULL && iParameterSize != 0)) return XRUDP_ARGUMENT;
	if (iParameterSize > XRUDP_NOISE_PARAMETERS_MAX) return XRUDP_LIMIT;
	if (pNoise->Index == 2) return XRUDP_DONE;
	if (!((pNoise->Role == XRUDP_INITIATOR && pNoise->Index == 0) ||
		(pNoise->Role == XRUDP_RESPONDER && pNoise->Index == 1))) return XRUDP_BACKPRESSURE;
	prefix = pNoise->Role == XRUDP_INITIATOR ? 80u : 32u;
	total = prefix + iParameterSize + XRUDP_TAG_SIZE;
	if (iCapacity < total) return XRUDP_LIMIT;
	candidate = *pNoise;
	memcpy(message, candidate.EphemeralPublic, 32);
	ok = xrudp__noise_hash(&candidate, message, 32);
	if (candidate.Role == XRUDP_INITIATOR) {
		ok = ok && xrudp__noise_mix(&candidate, candidate.EphemeralPrivate, candidate.RemoteStatic) &&
			xrudp__noise_encrypt(&candidate, candidate.StaticPublic, 32, message + 32) &&
			xrudp__noise_mix(&candidate, candidate.StaticPrivate, candidate.RemoteStatic);
	} else {
		ok = ok && xrudp__noise_mix(&candidate, candidate.EphemeralPrivate, candidate.RemoteEphemeral) &&
			xrudp__noise_mix(&candidate, candidate.EphemeralPrivate, candidate.RemoteStatic);
	}
	ok = ok && xrudp__noise_encrypt(&candidate, pParameters, iParameterSize, message + prefix);
	if (ok && candidate.Role == XRUDP_RESPONDER) ok = xrudp__noise_split(&candidate);
	if (ok) {
		++candidate.Index;
		memmove(pOutput, message, total);
		*pNoise = candidate;
		*pWritten = total;
		result = XRUDP_OK;
	}
	xrtSecureZero(&candidate, sizeof(candidate));
	xrtSecureZero(message, sizeof(message));
	return result;
}

xrudpresult xrtRudpNoiseRead(xrudpnoise* pNoise,
	const void* pMessage, size_t iMessageSize)
{
	const unsigned char* message = (const unsigned char*)pMessage;
	xrudpnoise candidate;
	xrudpresult result = XRUDP_AUTH_FAILED;
	size_t prefix;
	bool ok;
	if (pNoise == NULL || pMessage == NULL) return XRUDP_ARGUMENT;
	if (pNoise->Index == 2) return XRUDP_DONE;
	if (!((pNoise->Role == XRUDP_INITIATOR && pNoise->Index == 1) ||
		(pNoise->Role == XRUDP_RESPONDER && pNoise->Index == 0))) return XRUDP_BACKPRESSURE;
	prefix = pNoise->Role == XRUDP_INITIATOR ? 32u : 80u;
	if (iMessageSize < prefix + XRUDP_TAG_SIZE) return XRUDP_TRUNCATED;
	if (iMessageSize > prefix + XRUDP_TAG_SIZE + XRUDP_NOISE_PARAMETERS_MAX) return XRUDP_LIMIT;
	candidate = *pNoise;
	memcpy(candidate.RemoteEphemeral, message, 32);
	ok = xrudp__noise_hash(&candidate, message, 32);
	if (candidate.Role == XRUDP_INITIATOR) {
		ok = ok && xrudp__noise_mix(&candidate, candidate.EphemeralPrivate, candidate.RemoteEphemeral) &&
			xrudp__noise_mix(&candidate, candidate.StaticPrivate, candidate.RemoteEphemeral);
	} else {
		ok = ok && xrudp__noise_mix(&candidate, candidate.StaticPrivate, candidate.RemoteEphemeral) &&
			xrudp__noise_decrypt(&candidate, message + 32, 48, candidate.RemoteStatic) &&
			xrudp__noise_mix(&candidate, candidate.StaticPrivate, candidate.RemoteStatic);
	}
	candidate.PeerParameterSize = iMessageSize - prefix - XRUDP_TAG_SIZE;
	ok = ok && xrudp__noise_decrypt(&candidate, message + prefix, iMessageSize - prefix,
		candidate.PeerParameters);
	if (ok && !xrudp__noise_authorize(&candidate)) ok = false;
	if (ok && candidate.Role == XRUDP_INITIATOR) ok = xrudp__noise_split(&candidate);
	if (ok) {
		++candidate.Index;
		*pNoise = candidate;
		result = XRUDP_OK;
	}
	xrtSecureZero(&candidate, sizeof(candidate));
	return result;
}

xrudpresult xrtRudpNoisePeerParameters(const xrudpnoise* pNoise,
	void* pOutput, size_t iCapacity, size_t* pWritten)
{
	if (pWritten != NULL) *pWritten = 0;
	if (pNoise == NULL || pWritten == NULL || (pOutput == NULL && iCapacity != 0))
		return XRUDP_ARGUMENT;
	if (pNoise->Index != 2 || !pNoise->Trusted) return XRUDP_BACKPRESSURE;
	if (iCapacity < pNoise->PeerParameterSize) return XRUDP_LIMIT;
	if (pNoise->PeerParameterSize != 0)
		memmove(pOutput, pNoise->PeerParameters, pNoise->PeerParameterSize);
	*pWritten = pNoise->PeerParameterSize;
	return XRUDP_OK;
}

xrudpresult xrtRudpNoiseCreateCipher(xrudpnoise* pNoise,
	const void* pLocalId, const void* pPeerId, uint64_t iPacketLimit,
	xrudpcipher** ppCipher)
{
	xrudpresult result;
	if (pNoise == NULL || ppCipher == NULL) return XRUDP_ARGUMENT;
	if (pNoise->Index != 2 || !pNoise->Trusted) return XRUDP_BACKPRESSURE;
	if (pNoise->Taken) return XRUDP_DONE;
	result = xrtRudpCipherCreate(pNoise->SendKey, pNoise->ReceiveKey,
		pLocalId, pPeerId, iPacketLimit, ppCipher);
	if (result == XRUDP_OK) {
		pNoise->Taken = true;
		xrtSecureZero(pNoise->SendKey, 32);
		xrtSecureZero(pNoise->ReceiveKey, 32);
	}
	return result;
}
#endif


/* ========================================================================== */
/* source: extlibs/xrudp/src/crypto/rudp_cookie.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_COOKIE)
#include <string.h>

static bool xrudp__cookie_nonzero(const unsigned char* p, size_t size)
{
	unsigned char any = 0;
	for (size_t i = 0; i < size; ++i) any |= p[i];
	return any != 0;
}

static void xrudp__cookie_put32(unsigned char* p, uint32_t n)
{
	for (size_t i = 0; i < 4; ++i) p[3 - i] = (unsigned char)(n >> (i * 8));
}

static bool xrudp__cookie_address(const xrudpaddress* address, unsigned char wire[23])
{
	if (address == NULL || address->Port == 0) return false;
	if (address->Family == 4) {
		if (address->Zone != 0 || xrudp__cookie_nonzero(address->Address + 4, 12) ||
			address->Address[0] == 0 || address->Address[0] >= 224) return false;
	} else if (address->Family == 6) {
		if (!xrudp__cookie_nonzero(address->Address, 16) || address->Address[0] == 0xff)
			return false;
	} else return false;
	wire[0] = address->Family;
	wire[1] = (unsigned char)(address->Port >> 8);
	wire[2] = (unsigned char)address->Port;
	xrudp__cookie_put32(wire + 3, address->Zone);
	memcpy(wire + 7, address->Address, 16);
	return true;
}

static bool xrudp__cookie_mac(const void* key, const void* scope,
	const void* helloHash, const unsigned char address[23],
	const unsigned char timestamp[8], unsigned char mac[32])
{
	static const char domain[] = "XRUDP001/address-cookie";
	xhmacsha256 state;
	bool ok = xrtHmacSha256Init(&state, key, 32) &&
		xrtHmacSha256Update(&state, domain, sizeof(domain) - 1) &&
		xrtHmacSha256Update(&state, scope, 16) &&
		xrtHmacSha256Update(&state, helloHash, 32) &&
		xrtHmacSha256Update(&state, address, 23) &&
		xrtHmacSha256Update(&state, timestamp, 8) &&
		xrtHmacSha256Final(&state, mac);
	xrtSecureZero(&state, sizeof(state));
	return ok;
}

xrudpresult xrtRudpCookieCreate(const void* pKey,
	const void* pScope, const void* pHelloHash, const xrudpaddress* pAddress,
	uint64_t iNowUs, void* pCookie, size_t iCapacity)
{
	unsigned char wire[23], cookie[XRUDP_COOKIE_SIZE];
	bool ok;
	if (pKey == NULL || pScope == NULL || pHelloHash == NULL || pCookie == NULL ||
		!xrudp__cookie_nonzero((const unsigned char*)pScope, 16) ||
		!xrudp__cookie_address(pAddress, wire)) return XRUDP_ARGUMENT;
	if (iCapacity < XRUDP_COOKIE_SIZE) return XRUDP_LIMIT;
	for (size_t i = 0; i < 8; ++i) cookie[7 - i] = (unsigned char)(iNowUs >> (i * 8));
	ok = xrudp__cookie_mac(pKey, pScope, pHelloHash, wire, cookie, cookie + 8);
	if (ok) memmove(pCookie, cookie, sizeof(cookie));
	xrtSecureZero(cookie, sizeof(cookie));
	return ok ? XRUDP_OK : XRUDP_IO_ERROR;
}

xrudpresult xrtRudpCookieVerify(const void* pCurrentKey,
	const void* pPreviousKey, const void* pScope, const void* pHelloHash,
	const xrudpaddress* pAddress, uint64_t iNowUs, uint64_t iMaxAgeUs,
	const void* pCookie, size_t iCookieSize)
{
	const unsigned char* cookie = (const unsigned char*)pCookie;
	unsigned char wire[23], current[32] = {0}, previous[32] = {0};
	uint64_t issued = 0;
	bool ok, valid, currentValid, previousValid;
	if (pCurrentKey == NULL || pScope == NULL || pHelloHash == NULL || cookie == NULL ||
		iMaxAgeUs > XRUDP_COOKIE_MAX_AGE_US ||
		!xrudp__cookie_nonzero((const unsigned char*)pScope, 16) ||
		!xrudp__cookie_address(pAddress, wire)) return XRUDP_ARGUMENT;
	if (iCookieSize != XRUDP_COOKIE_SIZE) return XRUDP_MALFORMED;
	/* Always calculate both candidates, without a key-ID/timing oracle. */
	ok = xrudp__cookie_mac(pCurrentKey, pScope, pHelloHash, wire, cookie, current);
	ok = xrudp__cookie_mac(pPreviousKey != NULL ? pPreviousKey : pCurrentKey,
		pScope, pHelloHash, wire, cookie, previous) && ok;
	currentValid = xrtConstTimeEqual(current, cookie + 8, 32);
	previousValid = xrtConstTimeEqual(previous, cookie + 8, 32);
	valid = currentValid || previousValid;
	xrtSecureZero(current, sizeof(current));
	xrtSecureZero(previous, sizeof(previous));
	if (!ok) return XRUDP_IO_ERROR;
	if (!valid) return XRUDP_AUTH_FAILED;
	for (size_t i = 0; i < 8; ++i) issued = (issued << 8) | cookie[i];
	if (iMaxAgeUs == 0) iMaxAgeUs = XRUDP_COOKIE_MAX_AGE_US;
	if (issued > iNowUs || iNowUs - issued > iMaxAgeUs) return XRUDP_TIMEOUT;
	return XRUDP_OK;
}
#endif


/* ========================================================================== */
/* source: extlibs/xrudp/src/recovery/rudp_ack.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_ACK)
#include <string.h>

static void xrudp__ack_put64(unsigned char* p, uint64_t n)
{
	for (size_t i = 0; i < 8; ++i) p[7 - i] = (unsigned char)(n >> (i * 8));
}

static uint64_t xrudp__ack_get64(const unsigned char* p)
{
	uint64_t n = 0;
	for (size_t i = 0; i < 8; ++i) n = (n << 8) | p[i];
	return n;
}

void xrtRudpAckSetInit(xrudpackset* pSet)
{
	if (pSet != NULL) memset(pSet, 0, sizeof(*pSet));
}

bool xrtRudpAckSetContains(const xrudpackset* pSet, uint64_t number)
{
	if (pSet == NULL || number == 0 || pSet->Count > XRUDP_ACK_RANGES_MAX) return false;
	for (size_t i = 0; i < pSet->Count; ++i) {
		if (number >= pSet->Ranges[i].First && number <= pSet->Ranges[i].Last) return true;
	}
	return false;
}

xrudpresult xrtRudpAckSetRecord(xrudpackset* pSet, uint64_t number, uint64_t now)
{
	xrudpackrange sorted[XRUDP_ACK_RANGES_MAX + 1];
	xrudpackrange merged[XRUDP_ACK_RANGES_MAX + 1];
	size_t index = 0, count, used = 0;
	if (pSet == NULL || number == 0 || pSet->Count > XRUDP_ACK_RANGES_MAX ||
		(pSet->Count != 0 && now < pSet->LargestAtUs)) return XRUDP_ARGUMENT;
	count = pSet->Count;
	for (size_t i = 0; i < count; ++i) {
		if (number >= pSet->Ranges[i].First && number <= pSet->Ranges[i].Last)
			return XRUDP_DUPLICATE;
	}
	while (index < count && pSet->Ranges[index].Last > number) ++index;
	for (size_t i = 0; i < index; ++i) sorted[i] = pSet->Ranges[i];
	sorted[index].First = sorted[index].Last = number;
	for (size_t i = index; i < count; ++i) sorted[i + 1] = pSet->Ranges[i];
	for (size_t i = 0; i < count + 1; ++i) {
		if (used != 0 && (sorted[i].Last >= merged[used - 1].First ||
			merged[used - 1].First - sorted[i].Last == 1)) {
			if (sorted[i].First < merged[used - 1].First)
				merged[used - 1].First = sorted[i].First;
		} else merged[used++] = sorted[i];
	}
	if (used > XRUDP_ACK_RANGES_MAX) {
		if (number < merged[XRUDP_ACK_RANGES_MAX - 1].First) return XRUDP_TOO_OLD;
		used = XRUDP_ACK_RANGES_MAX;
	}
	if (count == 0 || number > pSet->Ranges[0].Last) pSet->LargestAtUs = now;
	pSet->Count = (uint16_t)used;
	memcpy(pSet->Ranges, merged, used * sizeof(merged[0]));
	memset(pSet->Ranges + used, 0, (XRUDP_ACK_RANGES_MAX - used) * sizeof(merged[0]));
	return XRUDP_OK;
}

xrudpresult xrtRudpAckSetEncode(const xrudpackset* pSet, uint64_t now,
	void* output, size_t capacity, size_t* written)
{
	unsigned char body[8 + XRUDP_ACK_RANGES_MAX * 16] = {0};
	uint64_t elapsed;
	uint32_t delay;
	if (written != NULL) *written = 0;
	if (pSet == NULL || output == NULL || written == NULL ||
		pSet->Count > XRUDP_ACK_RANGES_MAX) return XRUDP_ARGUMENT;
	if (pSet->Count == 0) return XRUDP_DONE;
	if (now < pSet->LargestAtUs) return XRUDP_ARGUMENT;
	elapsed = now - pSet->LargestAtUs;
	delay = elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed;
	for (size_t i = 0; i < 4; ++i) body[3 - i] = (unsigned char)(delay >> (i * 8));
	body[4] = (unsigned char)(pSet->Count >> 8);
	body[5] = (unsigned char)pSet->Count;
	for (size_t i = 0; i < pSet->Count; ++i) {
		xrudp__ack_put64(body + 8 + i * 16, pSet->Ranges[i].First);
		xrudp__ack_put64(body + 16 + i * 16, pSet->Ranges[i].Last);
	}
	return xrtRudpFrameAppend(output, capacity, written, XRUDP_FRAME_ACK, 0,
		body, 8u + (size_t)pSet->Count * 16u);
}

xrudpresult xrtRudpAckDecode(const void* data, size_t size, xrudpack* ack)
{
	xrudpwireframe frame;
	xrudpack decoded;
	xrudpresult result;
	size_t offset = 0;
	if (ack == NULL) return XRUDP_ARGUMENT;
	result = xrtRudpFrameNext(data, size, &offset, &frame);
	if (result != XRUDP_OK) return result == XRUDP_DONE ? XRUDP_MALFORMED : result;
	if (frame.Type != XRUDP_FRAME_ACK || offset != size) return XRUDP_MALFORMED;
	memset(&decoded, 0, sizeof(decoded));
	for (size_t i = 0; i < 4; ++i) decoded.DelayUs = (decoded.DelayUs << 8) | frame.Data[i];
	decoded.Count = (uint16_t)(((uint16_t)frame.Data[4] << 8) | frame.Data[5]);
	for (size_t i = 0; i < decoded.Count; ++i) {
		decoded.Ranges[i].First = xrudp__ack_get64(frame.Data + 8 + i * 16);
		decoded.Ranges[i].Last = xrudp__ack_get64(frame.Data + 16 + i * 16);
	}
	*ack = decoded;
	return XRUDP_OK;
}
#endif


/* ========================================================================== */
/* source: extlibs/xrudp/src/recovery/rudp_timing.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_TIMING)
#include <string.h>

static uint64_t xrudp__timing_add(uint64_t a, uint64_t b)
{
	return b > UINT64_MAX - a ? UINT64_MAX : a + b;
}

/* Exact floor(((divisor-1)*old + sample)/divisor), without overflowing
 * intermediate products, even at the full uint64_t input range. */
static uint64_t xrudp__timing_average(uint64_t old, uint64_t sample, uint64_t divisor)
{
	return (old / divisor) * (divisor - 1) + sample / divisor +
		((old % divisor) * (divisor - 1) + sample % divisor) / divisor;
}

static bool xrudp__timing_valid(const xrudprtt* rtt)
{
	return rtt != NULL && rtt->SmoothedUs != 0 &&
		rtt->MaxAckDelayUs <= XRUDP_ACK_DELAY_MAX_US;
}

xrudpresult xrtRudpRttInit(xrudprtt* pRtt, uint64_t initial, uint32_t maxDelay)
{
	xrudprtt rtt;
	if (pRtt == NULL || maxDelay > XRUDP_ACK_DELAY_MAX_US) return XRUDP_ARGUMENT;
	memset(&rtt, 0, sizeof(rtt));
	rtt.SmoothedUs = initial == 0 ? XRUDP_INITIAL_RTT_US : initial;
	rtt.VariationUs = rtt.SmoothedUs / 2;
	rtt.MaxAckDelayUs = maxDelay;
	*pRtt = rtt;
	return XRUDP_OK;
}

xrudpresult xrtRudpRttSample(xrudprtt* pRtt, uint64_t measured, uint32_t peerDelay)
{
	uint64_t adjusted, delay, difference;
	if (!xrudp__timing_valid(pRtt)) return XRUDP_ARGUMENT;
	if (measured == 0) measured = 1;
	pRtt->LatestUs = measured;
	if (!pRtt->HasSample) {
		pRtt->MinimumUs = measured;
		pRtt->SmoothedUs = measured;
		pRtt->VariationUs = measured / 2;
		pRtt->HasSample = true;
		return XRUDP_OK;
	}
	if (measured < pRtt->MinimumUs) pRtt->MinimumUs = measured;
	delay = peerDelay < pRtt->MaxAckDelayUs ? peerDelay : pRtt->MaxAckDelayUs;
	adjusted = measured;
	if (measured - pRtt->MinimumUs >= delay) adjusted -= delay;
	difference = adjusted > pRtt->SmoothedUs ? adjusted - pRtt->SmoothedUs :
		pRtt->SmoothedUs - adjusted;
	pRtt->VariationUs = xrudp__timing_average(pRtt->VariationUs, difference, 4);
	pRtt->SmoothedUs = xrudp__timing_average(pRtt->SmoothedUs, adjusted, 8);
	return XRUDP_OK;
}

xrudpresult xrtRudpRttPto(const xrudprtt* pRtt, unsigned int backoff, uint64_t* interval)
{
	uint64_t variance, base;
	if (!xrudp__timing_valid(pRtt) || interval == NULL) return XRUDP_ARGUMENT;
	variance = pRtt->VariationUs > UINT64_MAX / 4 ? UINT64_MAX : pRtt->VariationUs * 4;
	if (variance < XRUDP_TIMER_GRANULARITY_US) variance = XRUDP_TIMER_GRANULARITY_US;
	base = xrudp__timing_add(pRtt->SmoothedUs, variance);
	base = xrudp__timing_add(base, pRtt->MaxAckDelayUs);
	*interval = backoff >= 64 || base > (UINT64_MAX >> backoff) ? UINT64_MAX : base << backoff;
	return XRUDP_OK;
}

xrudpresult xrtRudpRttLossDelay(const xrudprtt* pRtt, uint64_t* interval)
{
	uint64_t largest, fraction, value;
	if (!xrudp__timing_valid(pRtt) || interval == NULL) return XRUDP_ARGUMENT;
	largest = pRtt->LatestUs > pRtt->SmoothedUs ? pRtt->LatestUs : pRtt->SmoothedUs;
	fraction = largest / 8 + (largest % 8 != 0 ? 1u : 0u);
	value = xrudp__timing_add(largest, fraction);
	*interval = value < XRUDP_TIMER_GRANULARITY_US ? XRUDP_TIMER_GRANULARITY_US : value;
	return XRUDP_OK;
}
#endif


/* ========================================================================== */
/* source: extlibs/xrudp/src/recovery/rudp_sent.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_SENT)
#include <string.h>

typedef struct xrudp__sentpacket {
	uint64_t Number, AtUs, Tag;
	uint32_t Bytes, Flags;
	uint32_t Status;
} xrudp__sentpacket;

struct xrudpsentwindow {
	uint64_t LargestSent, LargestAcked, InFlight, LastNow;
	uint32_t NormalCapacity, Capacity, Head, Count, Pending;
	bool Stopped;
	xrudp__sentpacket Packets[];
};

static xrudp__sentpacket* xrudp__sent_at(const xrudpsentwindow* w, size_t index)
{
	return (xrudp__sentpacket*)&w->Packets[(w->Head + index) % w->Capacity];
}

static size_t xrudp__sent_lower(const xrudpsentwindow* w, uint64_t number)
{
	size_t first = 0, last = w->Count;
	while (first < last) {
		size_t middle = first + (last - first) / 2;
		if (xrudp__sent_at(w, middle)->Number < number) first = middle + 1;
		else last = middle;
	}
	return first;
}

static void xrudp__sent_trim(xrudpsentwindow* w)
{
	while (w->Count != 0 && xrudp__sent_at(w, 0)->Status != 0) {
		memset(xrudp__sent_at(w, 0), 0, sizeof(xrudp__sentpacket));
		w->Head = (w->Head + 1) % w->Capacity;
		--w->Count;
	}
}

static void xrudp__sent_finish(xrudpsentwindow* w, xrudp__sentpacket* p,
	uint32_t kind, xrudpsentevent* event)
{
	memset(event, 0, sizeof(*event));
	event->PacketNumber = p->Number; event->UserTag = p->Tag;
	event->SentAtUs = p->AtUs; event->Bytes = p->Bytes;
	event->Flags = p->Flags; event->Kind = kind;
	if ((p->Flags & (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING)) != 0) w->InFlight -= p->Bytes;
	p->Status = kind;
	p->Tag = 0;
	--w->Pending;
}

xrudpresult xrtRudpSentCreate(uint32_t capacity, xrudpsentwindow** output)
{
	xrudpsentwindow* w;
	size_t size;
	if (output == NULL || capacity > XRUDP_SENT_CAPACITY_MAX) return XRUDP_ARGUMENT;
	if (capacity == 0) capacity = XRUDP_SENT_CAPACITY_DEFAULT;
	size = sizeof(*w) + (capacity + XRUDP_SENT_PROBE_RESERVE) * sizeof(xrudp__sentpacket);
	w = (xrudpsentwindow*)xrtMalloc(size);
	if (w == NULL) return XRUDP_NO_MEMORY;
	memset(w, 0, size);
	w->NormalCapacity = capacity; w->Capacity = capacity + XRUDP_SENT_PROBE_RESERVE;
	*output = w;
	return XRUDP_OK;
}

void xrtRudpSentDestroy(xrudpsentwindow* w)
{
	if (w != NULL) {
		size_t size = sizeof(*w) + w->Capacity * sizeof(xrudp__sentpacket);
		xrtSecureZero(w, size);
		xrtFree(w);
	}
}

xrudpresult xrtRudpSentState(const xrudpsentwindow* w, xrudpsentstate* output)
{
	xrudpsentstate state;
	if (w == NULL || output == NULL) return XRUDP_ARGUMENT;
	memset(&state, 0, sizeof(state));
	state.LargestSent = w->LargestSent; state.LargestAcked = w->LargestAcked;
	state.BytesInFlight = w->InFlight; state.LastNowUs = w->LastNow;
	state.NormalCapacity = w->NormalCapacity; state.TotalCapacity = w->Capacity;
	state.StoredPackets = w->Count; state.PendingPackets = w->Pending;
	state.Stopped = w->Stopped ? 1u : 0u;
	*output = state;
	return XRUDP_OK;
}

bool xrtRudpSentCanRecord(const xrudpsentwindow* w, bool probe)
{
	return w != NULL && !w->Stopped && w->Count < (probe ? w->Capacity : w->NormalCapacity);
}

xrudpresult xrtRudpSentRecord(xrudpsentwindow* w, uint64_t number,
	uint64_t now, uint32_t bytes, uint32_t flags, uint64_t tag)
{
	xrudp__sentpacket* p;
	bool probe = (flags & XRUDP_SENT_PROBE) != 0;
	if (w == NULL || number == 0 || number == UINT64_MAX || number <= w->LargestSent ||
		now < w->LastNow || bytes < XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE + XRUDP_FRAME_HEADER_SIZE ||
		bytes > XRUDP_PACKET_MAX || (flags & ~(XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PROBE | XRUDP_SENT_PADDING)) != 0 ||
		(probe && (flags & XRUDP_SENT_ACK_ELICITING) == 0)) return XRUDP_ARGUMENT;
	if (w->Stopped) return XRUDP_CLOSED;
	if (!xrtRudpSentCanRecord(w, probe)) return XRUDP_BACKPRESSURE;
	p = xrudp__sent_at(w, w->Count++);
	memset(p, 0, sizeof(*p));
	p->Number = number; p->AtUs = now; p->Tag = tag; p->Bytes = bytes; p->Flags = flags;
	w->LargestSent = number; w->LastNow = now; ++w->Pending;
	if ((flags & (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING)) != 0) w->InFlight += bytes;
	return XRUDP_OK;
}

static xrudpresult xrudp__sent_ack_check(const xrudpsentwindow* w, const xrudpack* ack)
{
	uint64_t previous = 0;
	if (ack->Count == 0 || ack->Count > XRUDP_ACK_RANGES_MAX) return XRUDP_MALFORMED;
	for (size_t i = 0; i < ack->Count; ++i) {
		uint64_t first = ack->Ranges[i].First, last = ack->Ranges[i].Last;
		size_t lower, upper;
		if (first == 0 || first > last || last > w->LargestSent ||
			(i != 0 && (last >= previous || previous - last <= 1))) return XRUDP_MALFORMED;
		previous = first;
		if (w->Count == 0 || last < xrudp__sent_at(w, 0)->Number) continue;
		if (first < xrudp__sent_at(w, 0)->Number) first = xrudp__sent_at(w, 0)->Number;
		lower = xrudp__sent_lower(w, first);
		upper = xrudp__sent_lower(w, last + 1);
		if ((uint64_t)(upper - lower) != last - first + 1) return XRUDP_MALFORMED;
	}
	return XRUDP_OK;
}

xrudpresult xrtRudpSentAcknowledge(xrudpsentwindow* w, const xrudpack* ack,
	uint64_t now, xrudpsentevent* events, size_t capacity, size_t* count,
	xrudpackchange* change)
{
	xrudpackchange next;
	xrudpresult result;
	size_t needed = 0, written = 0;
	if (count != NULL) *count = 0;
	if (w == NULL || ack == NULL || count == NULL || change == NULL ||
		(events == NULL && capacity != 0) || now < w->LastNow) return XRUDP_ARGUMENT;
	if (w->Stopped) return XRUDP_CLOSED;
	result = xrudp__sent_ack_check(w, ack);
	if (result != XRUDP_OK) return result;
	memset(&next, 0, sizeof(next));
	next.LargestAcked = ack->Ranges[0].Last > w->LargestAcked ? ack->Ranges[0].Last : w->LargestAcked;
	next.PeerAckDelayUs = ack->DelayUs;
	for (size_t i = 0; i < ack->Count; ++i) {
		size_t lower = xrudp__sent_lower(w, ack->Ranges[i].First);
		size_t upper = xrudp__sent_lower(w, ack->Ranges[i].Last + 1);
		for (size_t j = lower; j < upper; ++j) {
			const xrudp__sentpacket* p = xrudp__sent_at(w, j);
			if (p->Status != 0) continue;
			++needed; ++next.NewlyAckedPackets;
			if ((p->Flags & XRUDP_SENT_ACK_ELICITING) != 0) ++next.NewlyAckedEliciting;
			if ((p->Flags & (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING)) != 0) {
				next.NewlyAckedBytes += p->Bytes;
			}
		}
	}
	if (needed > capacity) return XRUDP_BACKPRESSURE;
	if (next.NewlyAckedEliciting != 0) {
		size_t index = xrudp__sent_lower(w, ack->Ranges[0].Last);
		if (index < w->Count) {
			const xrudp__sentpacket* p = xrudp__sent_at(w, index);
			if (p->Number == ack->Ranges[0].Last && p->Status == 0) {
				next.RttSampleUs = now - p->AtUs;
				next.RttSampleValid = 1;
			}
		}
	}
	/* Validation, event capacity and timing checks are complete. */
	for (size_t i = 0; i < ack->Count; ++i) {
		size_t lower = xrudp__sent_lower(w, ack->Ranges[i].First);
		size_t upper = xrudp__sent_lower(w, ack->Ranges[i].Last + 1);
		for (size_t j = lower; j < upper; ++j) {
			xrudp__sentpacket* p = xrudp__sent_at(w, j);
			if (p->Status == 0) xrudp__sent_finish(w, p, XRUDP_SENT_ACKED, &events[written++]);
		}
	}
	w->LargestAcked = next.LargestAcked; w->LastNow = now;
	xrudp__sent_trim(w);
	*change = next; *count = written;
	return XRUDP_OK;
}

static uint32_t xrudp__sent_loss_kind(const xrudpsentwindow* w,
	const xrudp__sentpacket* p, uint64_t lossDelay, uint64_t ackOnlyAge, uint64_t now)
{
	if (p->Status != 0) return 0;
	if ((p->Flags & (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING)) == 0)
		return now - p->AtUs >= ackOnlyAge ? XRUDP_SENT_EXPIRED : 0;
	if (p->Number >= w->LargestAcked) return 0;
	return w->LargestAcked - p->Number >= 3 || now - p->AtUs >= lossDelay ? XRUDP_SENT_LOST : 0;
}

xrudpresult xrtRudpSentDetectLoss(xrudpsentwindow* w,
	uint64_t lossDelay, uint64_t ackOnlyAge, uint64_t now,
	xrudpsentevent* events, size_t capacity, size_t* count)
{
	size_t needed = 0, written = 0;
	if (count != NULL) *count = 0;
	if (w == NULL || count == NULL || (events == NULL && capacity != 0) ||
		lossDelay < 1000 || ackOnlyAge < 1000 || now < w->LastNow) return XRUDP_ARGUMENT;
	if (w->Stopped) return XRUDP_CLOSED;
	for (size_t i = 0; i < w->Count; ++i)
		if (xrudp__sent_loss_kind(w, xrudp__sent_at(w, i), lossDelay, ackOnlyAge, now) != 0) ++needed;
	if (needed > capacity) return XRUDP_BACKPRESSURE;
	if (needed == 0) return XRUDP_DONE;
	for (size_t i = 0; i < w->Count; ++i) {
		xrudp__sentpacket* p = xrudp__sent_at(w, i);
		uint32_t kind = xrudp__sent_loss_kind(w, p, lossDelay, ackOnlyAge, now);
		if (kind != 0) xrudp__sent_finish(w, p, kind, &events[written++]);
	}
	w->LastNow = now;
	xrudp__sent_trim(w);
	*count = written;
	return XRUDP_OK;
}

xrudpresult xrtRudpSentAbort(xrudpsentwindow* w,
	xrudpsentevent* events, size_t capacity, size_t* count)
{
	size_t written = 0;
	if (count != NULL) *count = 0;
	if (w == NULL || count == NULL || (events == NULL && capacity != 0)) return XRUDP_ARGUMENT;
	if (w->Stopped) return XRUDP_DONE;
	if (w->Pending > capacity) return XRUDP_BACKPRESSURE;
	for (size_t i = 0; i < w->Count; ++i) {
		xrudp__sentpacket* p = xrudp__sent_at(w, i);
		if (p->Status == 0) xrudp__sent_finish(w, p, XRUDP_SENT_CANCELLED, &events[written++]);
	}
	w->Stopped = true;
	xrudp__sent_trim(w);
	*count = written;
	return XRUDP_OK;
}
#endif


/* ========================================================================== */
/* source: extlibs/xrudp/src/recovery/rudp_congestion.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_CONGESTION)
#include <string.h>

static bool xrudp__cc_valid(const xrudpcongestion* s)
{
	return s != NULL && s->MaximumWindow >= XRUDP_CC_INITIAL_WINDOW &&
		s->MaximumWindow <= XRUDP_CC_MAXIMUM_LIMIT && s->Window >= XRUDP_CC_MINIMUM_WINDOW &&
		s->Window <= s->MaximumWindow && s->SlowStartThreshold >= XRUDP_CC_MINIMUM_WINDOW &&
		s->RecoveryThrough <= s->LargestSent && s->AvoidanceCredit < s->Window &&
		s->BytesInFlight <= s->MaximumWindow + XRUDP_SENT_PROBE_RESERVE * XRUDP_PACKET_MAX;
}

static bool xrudp__cc_packet(uint32_t bytes, uint32_t flags)
{
	return bytes >= XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE + XRUDP_FRAME_HEADER_SIZE &&
		bytes <= XRUDP_PACKET_MAX && (flags & ~7u) == 0 &&
		((flags & XRUDP_SENT_PROBE) == 0 || (flags & XRUDP_SENT_ACK_ELICITING) != 0);
}

static bool xrudp__cc_flight(uint32_t flags)
{
	return (flags & (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING)) != 0;
}

static uint64_t xrudp__cc_add(uint64_t a, uint64_t b)
{
	return a > UINT64_MAX - b ? UINT64_MAX : a + b;
}

/* ceil(rtt * bytes / window), without a wide-integer compiler dependency.
 * window<=64MiB and bytes<=2400 keep the remainder product bounded. */
static uint64_t xrudp__cc_interval(uint64_t rtt, uint32_t bytes, uint64_t window)
{
	uint64_t quotient = rtt / window, remainder = (rtt % window) * bytes;
	uint64_t tail = remainder / window + (remainder % window != 0 ? 1u : 0u);
	if (bytes != 0 && quotient > UINT64_MAX / bytes) return UINT64_MAX;
	return xrudp__cc_add(quotient * bytes, tail);
}

xrudpresult xrtRudpCongestionInit(xrudpcongestion* s, uint64_t maximum)
{
	xrudpcongestion next;
	if (maximum == 0) maximum = XRUDP_CC_DEFAULT_MAXIMUM;
	if (s == NULL || maximum < XRUDP_CC_INITIAL_WINDOW || maximum > XRUDP_CC_MAXIMUM_LIMIT)
		return XRUDP_ARGUMENT;
	memset(&next, 0, sizeof(next));
	next.Window = XRUDP_CC_INITIAL_WINDOW; next.MaximumWindow = maximum;
	next.SlowStartThreshold = UINT64_MAX;
	*s = next;
	return XRUDP_OK;
}

xrudpresult xrtRudpCongestionCanSend(const xrudpcongestion* s,
	uint64_t now, uint64_t rtt, uint32_t bytes, uint32_t flags, uint64_t* due)
{
	uint64_t limit, tolerance, earliest;
	if (!xrudp__cc_valid(s) || due == NULL || !xrudp__cc_packet(bytes, flags) ||
		rtt == 0 || now < s->LastNowUs) return XRUDP_ARGUMENT;
	if (!xrudp__cc_flight(flags)) { *due = now; return XRUDP_OK; }
	limit = (flags & XRUDP_SENT_PROBE) != 0 ?
		s->MaximumWindow + XRUDP_SENT_PROBE_RESERVE * XRUDP_PACKET_MAX : s->Window;
	if (s->BytesInFlight > limit || bytes > limit - s->BytesInFlight) return XRUDP_BACKPRESSURE;
	/* A two-datagram GCRA burst bound; idle time cannot accumulate more credit. */
	tolerance = xrudp__cc_interval(rtt, XRUDP_CC_BURST_BYTES - bytes, s->Window);
	earliest = s->PacerNextUs > tolerance ? s->PacerNextUs - tolerance : 0;
	*due = earliest > now ? earliest : now;
	return earliest > now ? XRUDP_TIMEOUT : XRUDP_OK;
}

xrudpresult xrtRudpCongestionOnSent(xrudpcongestion* s,
	uint64_t number, uint64_t now, uint64_t rtt, uint32_t bytes, uint32_t flags)
{
	uint64_t due, interval;
	xrudpresult result;
	if (s == NULL || number == 0 || number == UINT64_MAX || number <= s->LargestSent)
		return XRUDP_ARGUMENT;
	result = xrtRudpCongestionCanSend(s, now, rtt, bytes, flags, &due);
	if (result != XRUDP_OK) return result;
	if (xrudp__cc_flight(flags)) {
		interval = xrudp__cc_interval(rtt, bytes, s->Window);
		if (interval == 0) interval = 1;
		s->PacerNextUs = xrudp__cc_add(s->PacerNextUs > now ? s->PacerNextUs : now, interval);
		s->BytesInFlight += bytes;
	}
	s->LargestSent = number; s->LastNowUs = now;
	return XRUDP_OK;
}

xrudpresult xrtRudpCongestionApply(xrudpcongestion* s, const xrudpsentevent* events,
	size_t count, uint64_t now, bool limited)
{
	xrudpcongestion next;
	uint64_t removed = 0, newestLoss = 0;
	if (!xrudp__cc_valid(s) || (events == NULL && count != 0) ||
		count > XRUDP_SENT_CAPACITY_MAX + XRUDP_SENT_PROBE_RESERVE || now < s->LastNowUs)
		return XRUDP_ARGUMENT;
	for (size_t i = 0; i < count; ++i) {
		const xrudpsentevent* p = &events[i];
		if (!xrudp__cc_packet(p->Bytes, p->Flags) || p->PacketNumber == 0 ||
			p->PacketNumber > s->LargestSent || p->SentAtUs > now || p->Reserved != 0 ||
			p->Kind < XRUDP_SENT_ACKED || p->Kind > XRUDP_SENT_CANCELLED ||
			(p->Kind == XRUDP_SENT_EXPIRED && xrudp__cc_flight(p->Flags))) return XRUDP_ARGUMENT;
		if (xrudp__cc_flight(p->Flags)) {
			removed += p->Bytes;
			if (p->Kind == XRUDP_SENT_LOST && p->PacketNumber > newestLoss) newestLoss = p->PacketNumber;
		}
	}
	if (removed > s->BytesInFlight) return XRUDP_ARGUMENT;
	next = *s; next.BytesInFlight -= removed; next.LastNowUs = now;
	if (newestLoss > next.RecoveryThrough) {
		next.RecoveryThrough = next.LargestSent;
		next.Window /= 2;
		if (next.Window < XRUDP_CC_MINIMUM_WINDOW) next.Window = XRUDP_CC_MINIMUM_WINDOW;
		next.SlowStartThreshold = next.Window; next.AvoidanceCredit = 0;
	}
	for (size_t i = 0; i < count; ++i) {
		const xrudpsentevent* p = &events[i];
		if (limited || p->Kind != XRUDP_SENT_ACKED || !xrudp__cc_flight(p->Flags) ||
			p->PacketNumber <= next.RecoveryThrough || next.Window == next.MaximumWindow) continue;
		if (next.Window < next.SlowStartThreshold) {
			next.Window += p->Bytes;
		} else {
			next.AvoidanceCredit += p->Bytes;
			if (next.AvoidanceCredit >= next.Window) {
				next.AvoidanceCredit -= next.Window;
				next.Window += XRUDP_PACKET_MAX;
			}
		}
		if (next.Window >= next.MaximumWindow) {
			next.Window = next.MaximumWindow; next.AvoidanceCredit = 0;
		}
	}
	*s = next;
	return XRUDP_OK;
}

xrudpresult xrtRudpCongestionCollapse(xrudpcongestion* s, uint64_t now)
{
	if (!xrudp__cc_valid(s) || s->LargestSent == 0 || now < s->LastNowUs) return XRUDP_ARGUMENT;
	s->SlowStartThreshold = s->Window / 2;
	if (s->SlowStartThreshold < XRUDP_CC_MINIMUM_WINDOW) s->SlowStartThreshold = XRUDP_CC_MINIMUM_WINDOW;
	s->Window = XRUDP_CC_MINIMUM_WINDOW; s->AvoidanceCredit = 0;
	s->RecoveryThrough = s->LargestSent; s->LastNowUs = now;
	return XRUDP_OK;
}
#endif


/* ========================================================================== */
/* source: extlibs/xrudp/src/stream/rudp_receive.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_RECEIVE)
#include <string.h>

struct xrudpreceive {
	uint64_t ReadOffset, HighestOffset, FinalOffset, Limit, Buffered;
	uint32_t Capacity, ResetReason;
	bool HasFinal, Reset;
	unsigned char Data[]; /* Capacity plaintext bytes, then Capacity/8 bitmap. */
};

static uint32_t xrudp__receive_capacity(uint32_t capacity)
{
	if (capacity == 0) capacity = XRUDP_RECEIVE_CAPACITY_DEFAULT;
	return capacity >= XRUDP_RECEIVE_CAPACITY_MIN && capacity <= XRUDP_RECEIVE_CAPACITY_MAX &&
		(capacity & (capacity - 1)) == 0 ? capacity : 0;
}

static size_t xrudp__receive_slot(const xrudpreceive* r, uint64_t offset)
{
	return (size_t)(offset & (r->Capacity - 1));
}

static bool xrudp__receive_has(const xrudpreceive* r, size_t slot)
{
	return (r->Data[r->Capacity + slot / 8] & (1u << (slot % 8))) != 0;
}

xrudpresult xrtRudpReceiveAllocationSize(uint32_t capacity, size_t* output)
{
	capacity = xrudp__receive_capacity(capacity);
	if (capacity == 0 || output == NULL) return XRUDP_ARGUMENT;
	*output = sizeof(xrudpreceive) + capacity + capacity / 8;
	return XRUDP_OK;
}

xrudpresult xrtRudpReceiveCreate(uint32_t capacity, uint64_t limit, xrudpreceive** output)
{
	xrudpreceive* r;
	size_t size;
	capacity = xrudp__receive_capacity(capacity);
	if (capacity == 0 || output == NULL || limit > capacity) return XRUDP_ARGUMENT;
	(void)xrtRudpReceiveAllocationSize(capacity, &size);
	r = (xrudpreceive*)xrtMalloc(size);
	if (r == NULL) return XRUDP_NO_MEMORY;
	memset(r, 0, size); r->Capacity = capacity; r->Limit = limit;
	*output = r;
	return XRUDP_OK;
}

void xrtRudpReceiveDestroy(xrudpreceive* r)
{
	if (r != NULL) {
		size_t size = sizeof(*r) + r->Capacity + r->Capacity / 8;
		xrtSecureZero(r, size); xrtFree(r);
	}
}

xrudpresult xrtRudpReceiveState(const xrudpreceive* r, xrudpreceivestate* output)
{
	xrudpreceivestate s;
	if (r == NULL || output == NULL) return XRUDP_ARGUMENT;
	memset(&s, 0, sizeof(s));
	s.ReadOffset = r->ReadOffset; s.HighestOffset = r->HighestOffset;
	s.FinalOffset = r->FinalOffset; s.Limit = r->Limit; s.BufferedBytes = r->Buffered;
	s.Capacity = r->Capacity; s.HasFinal = r->HasFinal ? 1u : 0u;
	s.Reset = r->Reset ? 1u : 0u; s.ResetReason = r->ResetReason;
	*output = s;
	return XRUDP_OK;
}

xrudpresult xrtRudpReceiveCheck(const xrudpreceive* r, uint64_t offset,
	const void* data, size_t size, bool fin, xrudpreceivechange* change)
{
	const unsigned char* bytes = (const unsigned char*)data;
	xrudpreceivechange next;
	uint64_t end;
	if (r == NULL || change == NULL || (data == NULL && size != 0) || size > XRUDP_STREAM_CHUNK_MAX)
		return XRUDP_ARGUMENT;
	if ((uint64_t)size > UINT64_MAX - offset) return XRUDP_MALFORMED;
	end = offset + size;
	if (end > r->Limit || (r->HasFinal && (end > r->FinalOffset || (fin && end != r->FinalOffset))) ||
		(fin && end < r->HighestOffset)) return XRUDP_MALFORMED;
	memset(&next, 0, sizeof(next));
	if (r->Reset) { *change = next; return XRUDP_DUPLICATE; }
	next.HighestDelta = end > r->HighestOffset ? end - r->HighestOffset : 0;
	next.NewFinal = fin && !r->HasFinal ? 1u : 0u;
	for (size_t i = 0; i < size; ++i) {
		size_t slot;
		if (offset + i < r->ReadOffset) continue;
		slot = xrudp__receive_slot(r, offset + i);
		if (xrudp__receive_has(r, slot)) {
			if (r->Data[slot] != bytes[i]) return XRUDP_MALFORMED;
		} else ++next.NewBytes;
	}
	*change = next;
	return next.HighestDelta != 0 || next.NewBytes != 0 || next.NewFinal != 0 ? XRUDP_OK : XRUDP_DUPLICATE;
}

xrudpresult xrtRudpReceivePush(xrudpreceive* r, uint64_t offset,
	const void* data, size_t size, bool fin, xrudpreceivechange* change)
{
	xrudpreceivechange next;
	xrudpresult result;
	if (change == NULL) return XRUDP_ARGUMENT;
	result = xrtRudpReceiveCheck(r, offset, data, size, fin, &next);
	if (result != XRUDP_OK && result != XRUDP_DUPLICATE) return result;
	if (result == XRUDP_DUPLICATE) { *change = next; return result; }
	for (size_t i = 0; i < size; ++i) {
		size_t slot;
		if (offset + i < r->ReadOffset) continue;
		slot = xrudp__receive_slot(r, offset + i);
		r->Data[slot] = ((const unsigned char*)data)[i];
		r->Data[r->Capacity + slot / 8] |= (unsigned char)(1u << (slot % 8));
	}
	r->HighestOffset += next.HighestDelta; r->Buffered += next.NewBytes;
	if (fin) { r->HasFinal = true; r->FinalOffset = offset + size; }
	*change = next;
	return XRUDP_OK;
}

xrudpresult xrtRudpReceiveRead(xrudpreceive* r, void* output, size_t capacity, size_t* read)
{
	unsigned char* bytes = (unsigned char*)output;
	size_t count = 0;
	if (read != NULL) *read = 0;
	if (r == NULL || output == NULL || capacity == 0 || read == NULL) return XRUDP_ARGUMENT;
	if (r->Reset) return XRUDP_CANCELLED;
	if (r->HasFinal && r->ReadOffset == r->FinalOffset) return XRUDP_DONE;
	while (count < capacity && r->Buffered != 0) {
		size_t slot = xrudp__receive_slot(r, r->ReadOffset);
		if (!xrudp__receive_has(r, slot)) break;
		bytes[count++] = r->Data[slot]; r->Data[slot] = 0;
		r->Data[r->Capacity + slot / 8] &= (unsigned char)~(1u << (slot % 8));
		++r->ReadOffset; --r->Buffered;
	}
	*read = count;
	return count != 0 ? XRUDP_OK : XRUDP_BACKPRESSURE;
}

xrudpresult xrtRudpReceiveExtend(xrudpreceive* r, uint64_t limit)
{
	uint64_t ceiling;
	if (r == NULL) return XRUDP_ARGUMENT;
	if (r->Reset) return XRUDP_CLOSED;
	ceiling = r->ReadOffset > UINT64_MAX - r->Capacity ? UINT64_MAX : r->ReadOffset + r->Capacity;
	if (limit < r->Limit || limit > ceiling) return XRUDP_ARGUMENT;
	if (limit == r->Limit) return XRUDP_DUPLICATE;
	r->Limit = limit;
	return XRUDP_OK;
}

xrudpresult xrtRudpReceiveReset(xrudpreceive* r,
	uint64_t final, uint32_t reason, xrudpreceivechange* change)
{
	xrudpreceivechange next;
	if (r == NULL || change == NULL) return XRUDP_ARGUMENT;
	if (final > r->Limit || final < r->HighestOffset || (r->HasFinal && final != r->FinalOffset) ||
		(r->Reset && reason != r->ResetReason)) return XRUDP_MALFORMED;
	memset(&next, 0, sizeof(next));
	if (r->Reset) { *change = next; return XRUDP_DUPLICATE; }
	/* A clean terminal result already delivered cannot become a second reset. */
	if (r->HasFinal && r->ReadOffset == r->FinalOffset) { *change = next; return XRUDP_DUPLICATE; }
	next.HighestDelta = final - r->HighestOffset;
	next.ConsumedDelta = final - r->ReadOffset; next.NewFinal = r->HasFinal ? 0u : 1u;
	r->HighestOffset = r->FinalOffset = r->ReadOffset = final;
	r->Reset = r->HasFinal = true; r->ResetReason = reason; r->Buffered = 0;
	xrtSecureZero(r->Data, r->Capacity + r->Capacity / 8);
	*change = next;
	return XRUDP_OK;
}
#endif


/* ========================================================================== */
/* source: extlibs/xrudp/src/stream/rudp_transmit.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_TRANSMIT)
#include <string.h>

#define XRUDP__TRANSMIT_LEVELS 8

struct xrudptransmit {
	uint64_t Base, End, Highest, Limit, Pending;
	size_t Allocation;
	uint32_t Capacity, Levels;
	bool FinQueued, FinSent, FinAcked, FinReady, Cancelled;
	unsigned char* Acked;
	unsigned char* Sent;
	unsigned char* Ready[XRUDP__TRANSMIT_LEVELS];
	size_t LevelBytes[XRUDP__TRANSMIT_LEVELS];
	unsigned char Data[];
};

static uint32_t xrudp__transmit_capacity(uint32_t capacity)
{
	if (capacity == 0) capacity = XRUDP_TRANSMIT_CAPACITY_DEFAULT;
	return capacity >= XRUDP_TRANSMIT_CAPACITY_MIN && capacity <= XRUDP_TRANSMIT_CAPACITY_MAX &&
		(capacity & (capacity - 1)) == 0 ? capacity : 0;
}

static size_t xrudp__transmit_slot(const xrudptransmit* t, uint64_t offset)
{
	return (size_t)(offset & (t->Capacity - 1));
}

static bool xrudp__transmit_bit(const unsigned char* bits, size_t slot)
{
	return (bits[slot / 8] & (1u << (slot % 8))) != 0;
}

static void xrudp__transmit_set(unsigned char* bits, size_t slot, bool set)
{
	unsigned char mask = (unsigned char)(1u << (slot % 8));
	if (set) bits[slot / 8] |= mask;
	else bits[slot / 8] &= (unsigned char)~mask;
}

static void xrudp__transmit_ready(xrudptransmit* t, size_t slot, bool ready)
{
	for (size_t level = 0; level < t->Levels; ++level) {
		size_t byte = slot / 8;
		bool before = t->Ready[level][byte] != 0;
		xrudp__transmit_set(t->Ready[level], slot, ready);
		ready = t->Ready[level][byte] != 0;
		if (ready == before) break;
		slot = byte;
	}
}

/* Hierarchical readiness avoids scanning a MiB of unrelated in-flight bytes
 * for every one-byte retransmission. At most eight levels with eight bits. */
static bool xrudp__transmit_find(const xrudptransmit* t, size_t level,
	size_t start, size_t* output)
{
	size_t byte = start / 8, bit = 0;
	unsigned int available;
	if (byte >= t->LevelBytes[level]) return false;
	available = t->Ready[level][byte] & (0xffu << (start % 8));
	if (available == 0) {
		if (level + 1 >= t->Levels || !xrudp__transmit_find(t, level + 1, byte + 1, &byte)) return false;
		available = t->Ready[level][byte];
	}
	while ((available & (1u << bit)) == 0) ++bit;
	*output = byte * 8 + bit;
	return true;
}

xrudpresult xrtRudpTransmitAllocationSize(uint32_t capacity, size_t* output)
{
	size_t size, level;
	capacity = xrudp__transmit_capacity(capacity);
	if (capacity == 0 || output == NULL) return XRUDP_ARGUMENT;
	size = sizeof(xrudptransmit) + capacity + capacity / 4;
	level = capacity / 8;
	for (;;) { size += level; if (level == 1) break; level = (level + 7) / 8; }
	*output = size;
	return XRUDP_OK;
}

xrudpresult xrtRudpTransmitCreate(uint32_t capacity, uint64_t limit, xrudptransmit** output)
{
	xrudptransmit* t;
	size_t allocation, level;
	unsigned char* next;
	capacity = xrudp__transmit_capacity(capacity);
	if (capacity == 0 || output == NULL) return XRUDP_ARGUMENT;
	(void)xrtRudpTransmitAllocationSize(capacity, &allocation);
	t = (xrudptransmit*)xrtMalloc(allocation);
	if (t == NULL) return XRUDP_NO_MEMORY;
	memset(t, 0, allocation);
	t->Allocation = allocation; t->Capacity = capacity; t->Limit = limit;
	t->Acked = t->Data + capacity; t->Sent = t->Acked + capacity / 8;
	next = t->Sent + capacity / 8; level = capacity / 8;
	for (;;) {
		t->Ready[t->Levels] = next; t->LevelBytes[t->Levels++] = level; next += level;
		if (level == 1) break;
		level = (level + 7) / 8;
	}
	*output = t;
	return XRUDP_OK;
}

void xrtRudpTransmitDestroy(xrudptransmit* t)
{
	if (t != NULL) { xrtSecureZero(t, t->Allocation); xrtFree(t); }
}

xrudpresult xrtRudpTransmitState(const xrudptransmit* t, xrudptransmitstate* output)
{
	xrudptransmitstate s;
	if (t == NULL || output == NULL) return XRUDP_ARGUMENT;
	memset(&s, 0, sizeof(s));
	s.BaseOffset = t->Base; s.QueuedEnd = t->End; s.HighestSent = t->Highest;
	s.PeerLimit = t->Limit; s.PendingBytes = t->Pending; s.Capacity = t->Capacity;
	s.FinQueued = t->FinQueued ? 1u : 0u; s.FinSent = t->FinSent ? 1u : 0u;
	s.FinAcknowledged = t->FinAcked ? 1u : 0u; s.Cancelled = t->Cancelled ? 1u : 0u;
	*output = s;
	return XRUDP_OK;
}

xrudpresult xrtRudpTransmitWrite(xrudptransmit* t, const void* data, size_t size, size_t* written)
{
	size_t count;
	if (written != NULL) *written = 0;
	if (t == NULL || written == NULL || (data == NULL && size != 0)) return XRUDP_ARGUMENT;
	if (t->Cancelled || t->FinQueued) return XRUDP_CLOSED;
	count = t->Capacity - (size_t)(t->End - t->Base);
	if (count > size) count = size;
	if (size != 0 && count == 0) return XRUDP_BACKPRESSURE;
	if (count > UINT64_MAX - t->End) return XRUDP_LIMIT;
	for (size_t i = 0; i < count; ++i) {
		size_t slot = xrudp__transmit_slot(t, t->End + i);
		t->Data[slot] = ((const unsigned char*)data)[i];
		xrudp__transmit_ready(t, slot, true);
	}
	t->End += count; t->Pending += count; *written = count;
	return XRUDP_OK;
}

xrudpresult xrtRudpTransmitFinish(xrudptransmit* t)
{
	if (t == NULL) return XRUDP_ARGUMENT;
	if (t->Cancelled) return XRUDP_CLOSED;
	if (t->FinQueued) return XRUDP_DUPLICATE;
	t->FinQueued = t->FinReady = true;
	return XRUDP_OK;
}

xrudpresult xrtRudpTransmitSetLimit(xrudptransmit* t, uint64_t limit)
{
	if (t == NULL) return XRUDP_ARGUMENT;
	if (t->Cancelled) return XRUDP_CLOSED;
	if (limit <= t->Limit) return XRUDP_DUPLICATE;
	t->Limit = limit;
	return XRUDP_OK;
}

xrudpresult xrtRudpTransmitPeek(const xrudptransmit* t, bool probe,
	void* output, size_t capacity, xrudptransmitframe* frame)
{
	xrudptransmitframe next;
	size_t slot = 0;
	bool found = false;
	uint64_t end;
	if (t == NULL || output == NULL || capacity == 0 || frame == NULL) return XRUDP_ARGUMENT;
	if (t->Cancelled) return XRUDP_CLOSED;
	memset(&next, 0, sizeof(next));
	if (probe && t->Base < t->End) { next.Offset = t->Base; found = true; }
	else if (t->Base < t->End) {
		size_t baseSlot = xrudp__transmit_slot(t, t->Base);
		found = xrudp__transmit_find(t, 0, baseSlot, &slot) || xrudp__transmit_find(t, 0, 0, &slot);
		if (found) next.Offset = t->Base + ((slot - baseSlot) & (t->Capacity - 1));
	}
	if (capacity > XRUDP_STREAM_CHUNK_MAX) capacity = XRUDP_STREAM_CHUNK_MAX;
	if (found && next.Offset < t->Limit) {
		while (next.Size < capacity && next.Offset + next.Size < t->End && next.Offset + next.Size < t->Limit) {
			slot = xrudp__transmit_slot(t, next.Offset + next.Size);
			if (xrudp__transmit_bit(t->Acked, slot) || (!probe && !xrudp__transmit_bit(t->Ready[0], slot))) break;
			++next.Size;
		}
	}
	end = next.Offset + next.Size;
	if (t->FinQueued && !t->FinAcked && (probe || t->FinReady) && t->End <= t->Limit) {
		if (next.Size == 0) { next.Offset = end = t->End; }
		if (end == t->End) next.Fin = 1;
	}
	if (next.Size == 0 && !next.Fin) return XRUDP_BACKPRESSURE;
	next.HighestDelta = end > t->Highest ? end - t->Highest : 0;
	for (size_t i = 0; i < next.Size; ++i) ((unsigned char*)output)[i] = t->Data[xrudp__transmit_slot(t, next.Offset + i)];
	*frame = next;
	return XRUDP_OK;
}

static xrudpresult xrudp__transmit_range(const xrudptransmit* t,
	uint64_t offset, size_t size, bool fin)
{
	if ((!fin && size == 0) || size > XRUDP_STREAM_CHUNK_MAX || size > UINT64_MAX - offset || offset + size > t->End ||
		(fin && (!t->FinQueued || offset + size != t->End))) return XRUDP_ARGUMENT;
	return t->Cancelled ? XRUDP_CLOSED : XRUDP_OK;
}

xrudpresult xrtRudpTransmitMarkSent(xrudptransmit* t, uint64_t offset,
	size_t size, bool fin, xrudptransmitchange* change)
{
	xrudptransmitchange next;
	xrudpresult result;
	bool pending = false;
	uint64_t end;
	if (t == NULL || change == NULL) return XRUDP_ARGUMENT;
	result = xrudp__transmit_range(t, offset, size, fin);
	if (result != XRUDP_OK) return result;
	end = offset + size;
	if (end > t->Limit) return XRUDP_BACKPRESSURE;
	memset(&next, 0, sizeof(next));
	next.HighestDelta = end > t->Highest ? end - t->Highest : 0;
	for (uint64_t i = offset > t->Base ? offset : t->Base; i < end; ++i) {
		size_t slot = xrudp__transmit_slot(t, i);
		if (xrudp__transmit_bit(t->Acked, slot)) continue;
		xrudp__transmit_set(t->Sent, slot, true); xrudp__transmit_ready(t, slot, false); pending = true;
	}
	if (fin && !t->FinAcked) { t->FinSent = true; t->FinReady = false; pending = true; }
	t->Highest += next.HighestDelta; *change = next;
	return pending || next.HighestDelta != 0 ? XRUDP_OK : XRUDP_DUPLICATE;
}

xrudpresult xrtRudpTransmitResolve(xrudptransmit* t, uint64_t offset,
	size_t size, bool fin, bool ack, xrudptransmitchange* change)
{
	xrudptransmitchange next;
	xrudpresult result;
	bool progress = false;
	uint64_t end, start;
	if (t == NULL || change == NULL) return XRUDP_ARGUMENT;
	result = xrudp__transmit_range(t, offset, size, fin);
	if (result != XRUDP_OK) return result;
	end = offset + size; start = offset > t->Base ? offset : t->Base;
	if (end > t->Highest || (fin && !t->FinSent)) return XRUDP_ARGUMENT;
	for (uint64_t i = start; i < end; ++i)
		if (!xrudp__transmit_bit(t->Sent, xrudp__transmit_slot(t, i))) return XRUDP_ARGUMENT;
	memset(&next, 0, sizeof(next));
	for (uint64_t i = start; i < end; ++i) {
		size_t slot = xrudp__transmit_slot(t, i);
		if (xrudp__transmit_bit(t->Acked, slot)) continue;
		if (ack) {
			xrudp__transmit_set(t->Acked, slot, true); xrudp__transmit_ready(t, slot, false);
			++next.NewlyAckedBytes; --t->Pending; progress = true;
		} else {
			if (!xrudp__transmit_bit(t->Ready[0], slot)) progress = true;
			xrudp__transmit_ready(t, slot, true);
		}
	}
	if (fin && !t->FinAcked) {
		if (ack) { t->FinAcked = true; t->FinReady = false; next.NewFinAcknowledged = 1; progress = true; }
		else { if (!t->FinReady) progress = true; t->FinReady = true; }
	}
	while (t->Base < t->End) {
		size_t slot = xrudp__transmit_slot(t, t->Base);
		if (!xrudp__transmit_bit(t->Acked, slot)) break;
		t->Data[slot] = 0; xrudp__transmit_set(t->Acked, slot, false); xrudp__transmit_set(t->Sent, slot, false);
		++t->Base; ++next.ReleasedBytes;
	}
	*change = next;
	return progress ? XRUDP_OK : XRUDP_DUPLICATE;
}

xrudpresult xrtRudpTransmitCancel(xrudptransmit* t, xrudptransmitchange* change)
{
	xrudptransmitchange next;
	if (t == NULL || change == NULL) return XRUDP_ARGUMENT;
	memset(&next, 0, sizeof(next));
	if (t->Cancelled) { *change = next; return XRUDP_DUPLICATE; }
	next.ReleasedBytes = t->End - t->Base;
	t->Base = t->End; t->Pending = 0; t->Cancelled = true; t->FinReady = false;
	xrtSecureZero(t->Data, t->Allocation - sizeof(*t));
	*change = next;
	return XRUDP_OK;
}
#endif

#endif
