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

/* rudp_endpoint 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_ENDPOINT)
#ifndef XRUDP_FEATURE_RUDP_ENDPOINT
#define XRUDP_FEATURE_RUDP_ENDPOINT
#endif
#ifndef XRUDP_MODULE_RUDP_SESSION
#define XRUDP_MODULE_RUDP_SESSION
#endif
#ifndef XRT_MODULE_NET_UDP
#define XRT_MODULE_NET_UDP
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#endif

/* rudp_session 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_SESSION)
#ifndef XRUDP_FEATURE_RUDP_SESSION
#define XRUDP_FEATURE_RUDP_SESSION
#endif
#ifndef XRUDP_MODULE_RUDP_HANDSHAKE
#define XRUDP_MODULE_RUDP_HANDSHAKE
#endif
#ifndef XRUDP_MODULE_RUDP_CONGESTION
#define XRUDP_MODULE_RUDP_CONGESTION
#endif
#ifndef XRUDP_MODULE_RUDP_TIMING
#define XRUDP_MODULE_RUDP_TIMING
#endif
#ifndef XRUDP_MODULE_RUDP_TRANSMIT
#define XRUDP_MODULE_RUDP_TRANSMIT
#endif
#ifndef XRUDP_MODULE_RUDP_RECEIVE
#define XRUDP_MODULE_RUDP_RECEIVE
#endif
#endif

/* rudp_handshake 及其直接依赖。 */
#if defined(XRUDP_MODULE_ALL) || defined(XRUDP_MODULE_RUDP_HANDSHAKE)
#ifndef XRUDP_FEATURE_RUDP_HANDSHAKE
#define XRUDP_FEATURE_RUDP_HANDSHAKE
#endif
#ifndef XRUDP_MODULE_RUDP_NOISE
#define XRUDP_MODULE_RUDP_NOISE
#endif
#ifndef XRUDP_MODULE_RUDP_COOKIE
#define XRUDP_MODULE_RUDP_COOKIE
#endif
#endif

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

/* Owned object bytes for aggregate reservations; excludes allocator overhead. */
XRUDP_API size_t xrtRudpCipherAllocationSize(void);

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

/* Owned object bytes for aggregate reservations; excludes allocator overhead. */
XRUDP_API size_t xrtRudpNoiseAllocationSize(void);

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

XRUDP_API xrudpresult xrtRudpSentAllocationSize(uint32_t iCapacity, size_t* pSize);

/* One fixed allocation, no allocation during processing. Capacity=0 uses 256;
 * 32 additional slots are reserved for PTO probes. Serialized by one owner.
 * All output buffers/objects must be disjoint from inputs and window storage.
 * Numeric UserTags are borrowed identifiers, not owned/released pointers.
 * Each tracked packet produces at most one final ACKED/LOST/EXPIRED/CANCELLED
 * event. A late ACK of a declared-lost packet does not produce a second event.
 * Logical data may retransmit under a new packet; deduplication is stream-level. */
XRUDP_API xrudpresult xrtRudpSentCreate(uint32_t iCapacity, xrudpsentwindow** ppWindow);
/* Start after an already retired packet-number prefix (e.g. handshake PN 1).
 * No pending packets/events/flight bytes are imported; ACKs of that prefix never
 * grant progress or RTT samples. Maximum reserved PN UINT64_MAX is invalid.
 * RetiredThrough comes from trusted local state, never from a peer ACK/header. */
XRUDP_API xrudpresult xrtRudpSentCreateAfter(uint32_t iCapacity,
	uint64_t iRetiredThrough, xrudpsentwindow** ppWindow);
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
/* Pure preflight: exact final-event requirement and candidate progress/RTT,
 * with no time/history/ownership mutation. Use before packet-wide admission.
 * EventCount=0 on error; Change is otherwise unchanged. Commit still rechecks.
 * A multi-ACK packet must bound its aggregate scratch and defer callbacks until
 * its complete receive/control transaction has committed. */
XRUDP_API xrudpresult xrtRudpSentCheckAcknowledge(const xrudpsentwindow* pWindow,
	const xrudpack* pAck, uint64_t iNowUs, size_t* pEventCount,
	xrudpackchange* pChange);
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

typedef struct xrudpreceiveframe {
	xrudpreceive* Receive;
	uint64_t Offset;
	const void* Data;
	size_t Size;
	bool Fin;
} xrudpreceiveframe;

typedef struct xrudpreceivebatchchange {
	uint64_t HighestDelta;
	uint64_t NewBytes;
	uint32_t NewFinals;
	uint32_t Streams;
} xrudpreceivebatchchange;

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
/* A Read can return bytes or a terminal EOF/RESET now (no hole wait). */
XRUDP_API bool xrtRudpReceiveReady(const xrudpreceive* pReceive);
/* Reuse quiescent storage for a NEW stream: only fresh, RESET, or consumed EOF.
 * Driver must have drained every old packet reference first. No allocation. */
XRUDP_API xrudpresult xrtRudpReceiveRestart(xrudpreceive* pReceive, uint64_t iInitialLimit);
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
/* Atomic STREAM-only batch across already admitted stream objects. At most 64
 * frames and one datagram's encoded STREAM budget (body + frame headers).
 * Checks cross-frame overlaps/final sizes and sums each stream's maximum offset
 * delta exactly once. Exceeding remaining advertised CONNECTION credit is
 * MALFORMED. Metadata/CPU work is bounded, and neither routine allocates.
 * CheckBatch is pure; PushBatch rechecks and commits only after ALL validation.
 * Inputs and output objects must be disjoint from every receive object.
 * The connection driver must ALSO preflight ACK/control/OPEN/RESET frames,
 * authorization and lifecycle before invoking PushBatch and recording an ACK. */
XRUDP_API xrudpresult xrtRudpReceiveCheckBatch(const xrudpreceiveframe* pFrames,
	size_t iCount, uint64_t iRemainingConnectionCredit, xrudpreceivebatchchange* pChange);
XRUDP_API xrudpresult xrtRudpReceivePushBatch(const xrudpreceiveframe* pFrames,
	size_t iCount, uint64_t iRemainingConnectionCredit, xrudpreceivebatchchange* pChange);
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
/* Reuse only fresh, cancelled, or fully acknowledged FIN storage. The driver
 * MUST first drain all old packet copies; offsets restart for a NEW stream. */
XRUDP_API xrudpresult xrtRudpTransmitRestart(xrudptransmit* pTransmit, uint64_t iPeerLimit);
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
/* public: extlibs/xrudp/include/xrudp/handshake.h */
/* ========================================================================== */

#ifndef XRUDP_HANDSHAKE_H
#define XRUDP_HANDSHAKE_H


#if defined(XRUDP_FEATURE_RUDP_HANDSHAKE)

#define XRUDP_PARAMETERS_SIZE 32u
#define XRUDP_HELLO_SIZE 96u
#define XRUDP_HANDSHAKE_PACKET_MAX 312u
#define XRUDP_HANDSHAKE_DEFAULT_TIMEOUT_US UINT64_C(10000000)
#define XRUDP_HANDSHAKE_MAX_TIMEOUT_US UINT64_C(60000000)

typedef struct xrudpparameters {
	uint64_t ConnectionCredit;
	uint32_t StreamCredit;
	uint32_t MaxStreams;
	uint32_t IdleTimeoutMs;
	uint32_t MaxAckDelayUs;
	uint32_t DatagramSize;
	uint32_t Reserved;
} xrudpparameters;

typedef struct xrudphandshake xrudphandshake;
typedef enum xrudphandshakephase {
	XRUDP_HANDSHAKE_RETRY = 1,
	XRUDP_HANDSHAKE_NOISE = 2,
	XRUDP_HANDSHAKE_PROOF = 3,
	XRUDP_HANDSHAKE_ESTABLISHED = 4,
	XRUDP_HANDSHAKE_FAILED = 5
} xrudphandshakephase;

typedef struct xrudphandshakeconfig {
	const void* StaticPrivate; /* 32 bytes, never logged. */
	const void* LocalPublic; /* Responder cached 32-byte matching static public. */
	const void* PeerPublic; /* Initiator pin mandatory; responder pin or Trust. */
	const void* Scope; /* Nonzero 16-byte public listener/rendezvous ID. */
	const void* ApplicationContext; /* 32 bytes; NULL=all zero for initiator,
	                               * NULL=any bound context for responder. */
	xrudpnoisetrustproc Trust;
	void* TrustContext; /* Borrowed through Destroy; side-effect-free callback. */
	xrudpparameters Parameters;
	uint64_t TimeoutUs; /* 0=10s, otherwise 1s..60s; never extended by input. */
} xrudphandshakeconfig;

typedef struct xrudphandshakeinfo {
	xrudprole Role;
	xrudphandshakephase Phase;
	xrudpresult Failure;
	uint32_t Attempts;
	uint64_t DeadlineUs;
	uint64_t NextSendUs;
	bool KeysTaken;
	unsigned char LocalId[16], PeerId[16], ApplicationContext[32], PeerPublic[32];
	xrudpparameters PeerParameters; /* Authenticated only at PROOF/ESTABLISHED. */
} xrudphandshakeinfo;

XRT_EXTERN_C_BEGIN

XRUDP_API void xrtRudpParametersDefault(xrudpparameters* pParameters);
XRUDP_API xrudpresult xrtRudpParametersEncode(const xrudpparameters* pParameters, void* pOutput, size_t iCapacity);
XRUDP_API xrudpresult xrtRudpParametersDecode(const void* pData, size_t iSize, xrudpparameters* pParameters);
/* Worst simultaneous owned bytes (handshake + Noise + cipher), excluding the
 * allocator's own pools. Reserve endpoint/handshake-slot budgets BEFORE Create
 * or Accept. The core has no sockets, timers, threads, logging or callbacks
 * other than the bounded Noise trust policy. */
XRUDP_API size_t xrtRudpHandshakeAllocationSize(void);
XRUDP_API xrudpresult xrtRudpHandshakeCreate(const xrudphandshakeconfig* pConfig,
	uint64_t iNowUs, xrudphandshake** ppHandshake);
/* Stateless: no allocation or public-key arithmetic. Public is a cached local
 * static public key, Key is a fresh endpoint cookie secret. Malformed/unmatched
 * untrusted packets return AUTH_FAILED. Response <= initial HELLO size.
 * Caller still enforces per-address CPU/rate and aggregate amplification bounds. */
XRUDP_API xrudpresult xrtRudpHandshakeRetry(const void* pCookieKey, const void* pScope,
	const void* pLocalPublic, const xrudpaddress* pSource, uint64_t iNowUs,
	const void* pHello, size_t iHelloSize, void* pOutput, size_t iCapacity, size_t* pWritten);
/* Validate cookie/address/scope/transcript/CID BEFORE allocating or doing DH.
 * Caller routes repeated IDs to the existing object, never repeatedly Accepts
 * the same live connection. PreviousKey is only a bounded rotation grace key.
 * Trust callback does not consume grants; no application event is published
 * until the explicit authenticated DATA proof has completed. */
XRUDP_API xrudpresult xrtRudpHandshakeAccept(const xrudphandshakeconfig* pConfig,
	const void* pCookieKey, const void* pPreviousCookieKey,
	const xrudpaddress* pSource, uint64_t iNowUs,
	const void* pPacket, size_t iSize, xrudphandshake** ppHandshake);
/* Input must come from the original verified peer address; address validation
 * and migration belong to the endpoint. Invalid unauthenticated input does not
 * reset retries/deadline. Authenticated invalid transport parameters fail the
 * handshake. A local NO_MEMORY after authenticating Noise may retain that
 * authenticated progress; Next can retry constructing the cipher before the
 * original deadline. Input/output objects must not alias owned storage. */
XRUDP_API xrudpresult xrtRudpHandshakeInput(xrudphandshake* pHandshake,
	const void* pPacket, size_t iSize, uint64_t iNowUs);
/* Poll a cached exact flight, with 250ms..2s exponential backoff and a finite
 * absolute deadline. No re-encryption/nonces are reused for DIFFERENT bytes.
 * TIMEOUT before Due only means wait; Failed info distinguishes deadline expiry.
 * Local buffer LIMIT does not spend an attempt. written=0 on non-OK.
 * Established responder retains its cached final reply until the deadline so
 * a lost final proof reply can be recovered even after cipher transfer. */
XRUDP_API xrudpresult xrtRudpHandshakeNext(xrudphandshake* pHandshake,
	uint64_t iNowUs, void* pOutput, size_t iCapacity, size_t* pWritten, uint64_t* pDueUs);
XRUDP_API xrudpresult xrtRudpHandshakeInfo(const xrudphandshake* pHandshake, xrudphandshakeinfo* pInfo);
/* Only ESTABLISHED; transfer exactly once. Cipher PN 1 in each direction is
 * reserved for this handshake's final proof. Endpoint starts business at PN 2,
 * seeds accepted receive history and the retired sent prefix with PN 1, and defers all application callbacks
 * until it has admitted the connection and queued the cached final reply.
 * Destroy never destroys a transferred cipher. */
XRUDP_API xrudpresult xrtRudpHandshakeTakeCipher(xrudphandshake* pHandshake, xrudpcipher** ppCipher);
XRUDP_API void xrtRudpHandshakeDestroy(xrudphandshake* pHandshake);

XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrudp/include/xrudp/session.h */
/* ========================================================================== */

#ifndef XRUDP_SESSION_H
#define XRUDP_SESSION_H


#if defined(XRUDP_FEATURE_RUDP_SESSION)

#define XRUDP_SESSION_PACKETS_MAX 256u
#define XRUDP_SESSION_POLL_BURST 8u
#define XRUDP_SESSION_STREAM_CAPACITY_MAX 64u
#define XRUDP_STREAM_OPENED 1u
#define XRUDP_STREAM_READ_READY 2u
#define XRUDP_STREAM_WRITE_READY 4u
#define XRUDP_STREAM_WRITE_DONE 8u
#define XRUDP_STREAM_READ_EOF 16u
#define XRUDP_STREAM_RETIRE_READY 32u

typedef struct xrudpsession xrudpsession;
/* Called only by Poll, serialized with the entire session. Must COPY the
 * datagram before returning OK (UDP admission, not delivery). May return
 * BACKPRESSURE/NO_MEMORY for temporary local admission failure, IO_ERROR for
 * a terminal transport failure. Must not reenter/destroy this session. */
typedef xrudpresult (*xrudpsessionsendproc)(const void* pPacket, size_t iSize, void* pContext);

typedef struct xrudpsessionconfig {
	xrudpparameters Local, Peer; /* Authenticated, already negotiated parameters. */
	xrudprole Role; /* Trusted handshake role, not chosen from a DATA frame. */
	uint32_t StreamCapacity; /* Per-origin pool, includes implicit stream 1; 1..64. */
	uint32_t ReceiveCapacity, TransmitCapacity;
	uint32_t PacketCapacity; /* 1..256 normal slots plus bounded probe reserve. */
	uint32_t ControlPerSecond; /* ACK-only output limit, 40..4096. */
	uint64_t MaximumWindow, InitialRttUs;
	uint64_t ProgressTimeoutUs; /* 1s..10min, default 30s; cannot be refreshed by PING. */
	uint64_t KeyUpdatePackets; /* Local rotation threshold, 8..2^24. */
} xrudpsessionconfig;

typedef struct xrudpsessionstate {
	xrudpresult Reason; /* OK while active; terminal reasons are sticky. */
	xrudpreceivestate Receive;
	xrudptransmitstate Transmit;
	xrudpsentstate Sent;
	xrudpcongestion Congestion;
	xrudprtt Rtt;
	uint64_t ReceivedConnection, ReceiveLimit, SentConnection, PeerLimit;
	uint64_t LastNowUs, LastActivityUs, ProgressAtUs;
	uint64_t ReceivedPackets, SentPackets, LostPackets, ProbeTimeouts, KeyUpdates;
	uint64_t InvalidPackets, OutputBackpressure;
	uint32_t PtoBackoff, OutstandingProbes;
	uint32_t ActiveStreams, PendingStreamEvents;
	uint32_t IncomingLimit, OutgoingLimit;
	bool AckPending, ReadReady;
} xrudpsessionstate;

typedef struct xrudpstreamconfig {
	uint8_t Priority; /* 0..3; bounded weighted deficit service, no starvation. */
	uint16_t Weight; /* 1..16 local policy. Wire weights are capped to 16 for service. */
} xrudpstreamconfig;
typedef struct xrudpstreamstate {
	uint32_t Id, References;
	uint16_t Weight;
	uint8_t Priority;
	bool Local, OpenAcknowledged, ReadReady, WriteDone, RetireReady;
	bool ResetQueued, ResetAcknowledged;
	uint32_t ResetReason;
	uint64_t ResetFinal;
	xrudpreceivestate Receive;
	xrudptransmitstate Transmit;
} xrudpstreamstate;
typedef struct xrudpstreamevent { uint32_t Id, Events; } xrudpstreamevent;

XRT_EXTERN_C_BEGIN

/* Implicit bidirectional byte stream 1 is admitted by the completed handshake.
 * StreamCapacity reserves 2*N-1 fixed receive/transmit pairs at construction;
 * Local.MaxStreams <= N. OPEN/read/write/retire never allocate. Message/game
 * framing and path migration are separate layers, not enabled here yet.
 * No private thread/socket, no callbacks except serialized output admission.
 * Every method requires one owner; all caller buffers must be disjoint. */
XRUDP_API void xrtRudpSessionConfigInit(xrudpsessionconfig* pConfig);
XRUDP_API void xrtRudpStreamConfigInit(xrudpstreamconfig* pConfig);
/* Total logical bytes including the cipher transferred at Create; excludes
 * allocator overhead and the caller's UDP queues. Reserve BEFORE admission. */
XRUDP_API xrudpresult xrtRudpSessionAllocationSize(const xrudpsessionconfig* pConfig, size_t* pSize);
/* Cipher must be exclusively owned and promoted from completed proof: next PN
 * 2, largest received 1, epoch 1. Ownership transfers ONLY on success; output
 * remains unchanged on failure. Session initializes sent/ACK proof prefix 1.
 * Config identity/parameters must come from the authenticated handshake. */
XRUDP_API xrudpresult xrtRudpSessionCreate(const xrudpsessionconfig* pConfig,
	xrudpcipher* pCipher, uint64_t iNowUs, xrudpsessionsendproc pSend,
	void* pContext, xrudpsession** ppSession);
XRUDP_API void xrtRudpSessionDestroy(xrudpsession* pSession);
XRUDP_API xrudpresult xrtRudpSessionState(const xrudpsession* pSession, xrudpsessionstate* pState);
XRUDP_API xrudpresult xrtRudpSessionWrite(xrudpsession* pSession,
	const void* pData, size_t iSize, size_t* pWritten);
XRUDP_API xrudpresult xrtRudpSessionFinish(xrudpsession* pSession);
/* Read releases stream and connection credit; grants are retransmitted until
 * validated ACK. DONE is clean stream EOF, not implicit connection closure. */
XRUDP_API xrudpresult xrtRudpSessionRead(xrudpsession* pSession,
	void* pOutput, size_t iCapacity, size_t* pRead);
/* New bidirectional byte stream. IDs never reused: initiator odd, responder
 * even; default ID 1 consumes ordinal 1 in both pools. Success permits bounded
 * writing before OPEN ACK; output repeats OPEN until acknowledged. */
XRUDP_API xrudpresult xrtRudpSessionStreamOpen(xrudpsession* pSession,
	const xrudpstreamconfig* pConfig, uint32_t* pId);
XRUDP_API xrudpresult xrtRudpSessionStreamState(const xrudpsession* pSession,
	uint32_t iId, xrudpstreamstate* pState);
XRUDP_API xrudpresult xrtRudpSessionStreamWrite(xrudpsession* pSession,
	uint32_t iId, const void* pData, size_t iSize, size_t* pWritten);
XRUDP_API xrudpresult xrtRudpSessionStreamRead(xrudpsession* pSession,
	uint32_t iId, void* pOutput, size_t iCapacity, size_t* pRead);
XRUDP_API xrudpresult xrtRudpSessionStreamFinish(xrudpsession* pSession, uint32_t iId);
/* Reset only the local sending half. Final size is actual highest sent, so
 * discarded, never-sent buffers do not consume peer flow credit. */
XRUDP_API xrudpresult xrtRudpSessionStreamReset(xrudpsession* pSession, uint32_t iId, uint32_t iReason);
/* Reclaim after both halves terminate/consume and ALL packet copies drain.
 * Cannot retire default stream 1. Retiring a peer-origin stream reliably
 * replenishes cumulative MAX_STREAMS; late frames for retired IDs are ignored. */
XRUDP_API xrudpresult xrtRudpSessionStreamRetire(xrudpsession* pSession, uint32_t iId);
/* Coalesced per-stream edge hints, including remote OPEN. DONE when none.
 * No callbacks, no allocation; explicit retirement discards pending hints.
 * RETIRE_READY follows both half terminations and all packet copies draining.
 * Later output may reacquire a control reference: recheck State/Retire. */
XRUDP_API xrudpresult xrtRudpSessionStreamNextEvent(xrudpsession* pSession, xrudpstreamevent* pEvent);
/* Authenticate, preflight ALL ACK/data/reset/control state, then commit once.
 * Malformed authenticated input terminates the session; no partial delivery or
 * false acknowledgement. Noise/forgery/old/duplicate packets do not terminate.
 * BACKPRESSURE means an OPEN gap or unavailable admission slot: no connection
 * frames commit and this PN is NOT ACKed. Retransmit contents with a fresh PN.
 * An authenticated receive key update may still be confirmed without that ACK.
 * Caller must first enforce the original verified source address. */
XRUDP_API xrudpresult xrtRudpSessionInput(xrudpsession* pSession,
	const void* pPacket, size_t iSize, uint64_t iNowUs);
/* Bounded maintenance/output (<=8 datagrams), no allocation. Due is the next
 * monotonic microsecond service deadline, also when output is backpressured.
 * Call again after Write/Finish/Read/Input and at Due; no blocking or sleeps.
 * Time must never go backwards. Finite idle AND useful-progress deadlines apply.
 * New local work after idle starts its progress deadline at the first following
 * Poll/Input clock observation. Additional local operations cannot renew it. */
XRUDP_API xrudpresult xrtRudpSessionPoll(xrudpsession* pSession,
	uint64_t iNowUs, uint64_t* pDueUs);
/* Local cancellation, no output/allocation. Clears plaintext and drains unique
 * packet ownership. Caller coordinates any best-effort remote CLOSE separately. */
XRUDP_API xrudpresult xrtRudpSessionAbort(xrudpsession* pSession, xrudpresult iReason);

XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrudp/include/xrudp/endpoint.h */
/* ========================================================================== */

#ifndef XRUDP_ENDPOINT_H
#define XRUDP_ENDPOINT_H


#if defined(XRUDP_FEATURE_RUDP_ENDPOINT)

#define XRUDP_ENDPOINT_PEERS_MAX 256u
#define XRUDP_ENDPOINT_ADDRESS_BUCKETS 64u
#define XRUDP_PEER_READ_READY 1u
#define XRUDP_PEER_WRITE_READY 2u
#define XRUDP_PEER_WRITE_DONE 4u
#define XRUDP_PEER_READ_EOF 8u
#define XRUDP_PEER_STREAMS_READY 16u
#define XRUDP_PEER_STREAM_EVENT_BURST 8u

typedef struct xrudpendpoint xrudpendpoint;
typedef struct xrudppeer xrudppeer;
typedef enum xrudpendpointstate {
	XRUDP_ENDPOINT_OPENING = 0,
	XRUDP_ENDPOINT_OPEN,
	XRUDP_ENDPOINT_CLOSING,
	XRUDP_ENDPOINT_CLOSED
} xrudpendpointstate;
typedef enum xrudppeerstate {
	XRUDP_PEER_NEGOTIATING = 0,
	XRUDP_PEER_CONFIRMED,
	XRUDP_PEER_CLOSED
} xrudppeerstate;

/* All callbacks run on the bound Worker and borrow their arguments. Ref a peer
 * to keep its handle beyond the callback. Confirmed means identity/address/key
 * proof AND default reliable session admission have completed.
 * Callbacks may close/ref/destroy owned handles, but must not block the Worker.
 * Context remains borrowed until endpoint Close returns. */
typedef struct xrudpendpointevents {
	void (*Open)(xrudpendpoint* pEndpoint, void* pContext);
	void (*Confirmed)(xrudpendpoint* pEndpoint, xrudppeer* pPeer, void* pContext);
	void (*PeerClose)(xrudpendpoint* pEndpoint, xrudppeer* pPeer, xrudpresult iReason, void* pContext);
	void (*Close)(xrudpendpoint* pEndpoint, xrudpresult iReason, const xerror* pError, void* pContext);
	/* Default stream/connection edge hints; drain Read to BACKPRESSURE/EOF/RESET. WRITE_READY means
	 * ACK returned buffer capacity; WRITE_DONE is all bytes+FIN acknowledged.
	 * Mutations inside a callback defer further Changed delivery to another
	 * worker turn; no recursive Changed callbacks. Borrowed peer and context. */
	void (*Changed)(xrudpendpoint* pEndpoint, xrudppeer* pPeer, uint32_t iEvents, void* pContext);
	/* All byte streams, including ID 1; XRUDP_STREAM_* masks. At most eight
	 * coalesced stream hints per dispatch. OPENED is remote-origin only. The
	 * borrowed event is valid only during this callback. Callbacks may perform
	 * stream operations or close/drop owned handles; never recursively invoked.
	 * Hints generated inside callbacks wait for a later dispatch (>=1ms).
	 * A retired ID's captured hint is discarded. Changed independently reports
	 * default FIN/read hints and STREAMS_READY when cumulative peer OPEN credit
	 * grows. After Open backpressure, retry on STREAMS_READY or local retirement.
	 * RETIRE_READY hints require a fresh State/Retire check: another control
	 * output may temporarily acquire a packet reference after the snapshot.
	 * PeerAbort may synchronously call PeerClose; StreamChanged never reenters. */
	void (*StreamChanged)(xrudpendpoint* pEndpoint, xrudppeer* pPeer, const xrudpstreamevent* pEvent, void* pContext);
} xrudpendpointevents;

typedef struct xrudpendpointconfig {
	const void* StaticPrivate; /* 32 bytes, copied; securely erased on Close. */
	const void* Scope; /* Optional nonzero 16 bytes; NULL creates a random scope. */
	const void* PeerPublic; /* Optional listener pin; pin or Trust required when Listen. */
	const void* ApplicationContext; /* Optional exact listener context, copied. */
	xrudpnoisetrustproc Trust;
	void* TrustContext;
	xrudpsessionconfig Session; /* Local advertised; Peer/Role supplied by authenticated handshake. */
	uint64_t HandshakeTimeoutUs;
	size_t MemoryLimit; /* Peak endpoint/peer/handshake/session reservation bytes.
	                    * XRT engine pools/kernel socket buffers have own limits. */
	uint32_t MaxPeers;
	uint32_t MaxHalfOpen;
	uint32_t HandshakesPerSecond;
	uint32_t AddressHandshakesPerSecond; /* IP (not port); fixed 64-entry table. */
	bool Listen;
} xrudpendpointconfig;

typedef struct xrudpconnectconfig {
	const void* PeerPublic; /* Trusted 32-byte public key, mandatory. */
	const void* Scope; /* 16-byte listener scope from trusted discovery. */
	const void* ApplicationContext; /* Optional 32-byte binding, copied into handshake. */
} xrudpconnectconfig;

typedef struct xrudpendpointinfo {
	xnetaddr Local;
	unsigned char Public[32], Scope[16];
	uint32_t Peers, HalfOpen;
	size_t ReservedBytes, MemoryLimit;
	uint64_t Received, Sent, Invalid, RateDropped, AddressDropped, CapacityDropped, IoErrors;
} xrudpendpointinfo;

XRT_EXTERN_C_BEGIN

XRUDP_API void xrtRudpEndpointConfigInit(xrudpendpointconfig* pConfig);
/* Must be called on a running Engine Worker. Binding and all mutable operations
 * are serialized on that Worker; no private engine/thread is created. Native
 * IPv4/IPv6 are supported as separate endpoints; IPv6 is v6-only.
 * Bind success returns one owned reference plus an internal I/O lifetime.
 * The endpoint owns UDP queues (64 packets/64KiB send; bounded push receive).
 * Context/Open callback cannot run before Bind returns on this Worker. */
XRUDP_API xrudpresult xrtRudpEndpointBind(xnetworker* pWorker, const xnetaddr* pLocal,
	const xrudpendpointconfig* pConfig, const xrudpendpointevents* pEvents,
	void* pContext, xrudpendpoint** ppEndpoint);
XRUDP_API xrudpendpoint* xrtRudpEndpointRef(xrudpendpoint* pEndpoint);
XRUDP_API void xrtRudpEndpointDestroy(xrudpendpoint* pEndpoint);
/* Thread-safe states. Info requires the owner while live; after CLOSED it is
 * an immutable snapshot and may be read even after the Engine is destroyed. */
XRUDP_API xrudpendpointstate xrtRudpEndpointState(const xrudpendpoint* pEndpoint);
XRUDP_API xrudppeerstate xrtRudpPeerState(const xrudppeer* pPeer);
XRUDP_API xrudpresult xrtRudpEndpointInfo(const xrudpendpoint* pEndpoint, xrudpendpointinfo* pInfo);
XRUDP_API xrudpresult xrtRudpPeerInfo(const xrudppeer* pPeer, xrudphandshakeinfo* pInfo, xnetaddr* pAddress);
/* Owner-only; success returns an additional owned peer reference. Account ACL,
 * peer pin and scope must come from trusted application discovery. */
XRUDP_API xrudpresult xrtRudpEndpointConnect(xrudpendpoint* pEndpoint, const xnetaddr* pRemote,
	const xrudpconnectconfig* pConfig, xrudppeer** ppPeer);
XRUDP_API xrudppeer* xrtRudpPeerRef(xrudppeer* pPeer);
XRUDP_API void xrtRudpPeerDestroy(xrudppeer* pPeer);
/* Owner-only nonblocking default stream. Write copies a bounded prefix;
 * success is buffering, not delivery. Finish forbids further writes. Read
 * returns DONE at clean EOF, CANCELLED at peer RESET. Confirmed allows the
 * first write; subsequent capacity/EOF changes arrive as Changed hints. */
XRUDP_API xrudpresult xrtRudpPeerWrite(xrudppeer* pPeer, const void* pData, size_t iSize, size_t* pWritten);
XRUDP_API xrudpresult xrtRudpPeerRead(xrudppeer* pPeer, void* pOutput, size_t iCapacity, size_t* pRead);
XRUDP_API xrudpresult xrtRudpPeerFinish(xrudppeer* pPeer);
/* Owner-only advanced byte streams; capacity/IDs/half-close/retirement follow
 * SessionStream*. No per-stream heap allocation. Before Confirmed return
 * BACKPRESSURE; after peer close return CLOSED, without accessing the Engine.
 * A stream state is live-only (the aggregate session snapshot survives Close).
 * Open output is unchanged on failure; Read/Write always set their count=0
 * before failing. Read RESET is CANCELLED without closing the connection. */
XRUDP_API xrudpresult xrtRudpPeerStreamOpen(xrudppeer* pPeer, const xrudpstreamconfig* pConfig, uint32_t* pId);
XRUDP_API xrudpresult xrtRudpPeerStreamState(const xrudppeer* pPeer, uint32_t iId, xrudpstreamstate* pState);
XRUDP_API xrudpresult xrtRudpPeerStreamWrite(xrudppeer* pPeer, uint32_t iId, const void* pData, size_t iSize, size_t* pWritten);
XRUDP_API xrudpresult xrtRudpPeerStreamRead(xrudppeer* pPeer, uint32_t iId, void* pOutput, size_t iCapacity, size_t* pRead);
XRUDP_API xrudpresult xrtRudpPeerStreamFinish(xrudppeer* pPeer, uint32_t iId);
XRUDP_API xrudpresult xrtRudpPeerStreamReset(xrudppeer* pPeer, uint32_t iId, uint32_t iReason);
XRUDP_API xrudpresult xrtRudpPeerStreamRetire(xrudppeer* pPeer, uint32_t iId);
/* Owner while live; immutable snapshot after CLOSED, safe after Engine destroy.
 * BACKPRESSURE before admission, CLOSED when no session was ever admitted. */
XRUDP_API xrudpresult xrtRudpPeerSessionState(const xrudppeer* pPeer, xrudpsessionstate* pState);
XRUDP_API xrudpresult xrtRudpPeerAbort(xrudppeer* pPeer);
/* Thread-safe, idempotent request, no allocation. Close callback runs once after
 * peer callbacks and timer cleanup. Dropping a handle alone does not close it.
 * XRT refuses Engine Stop while UDP objects are live: request endpoint Close
 * and await its terminal callback BEFORE Stop. Closed external handles retain
 * no engine resources and may be destroyed after the Engine. */
XRUDP_API xrudpresult xrtRudpEndpointClose(xrudpendpoint* pEndpoint);

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

size_t xrtRudpCipherAllocationSize(void) { return sizeof(xrudpcipher); }

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
	if (!xrtChaCha20Poly1305TryOpen(key, nonce, pPacket, XRUDP_HEADER_SIZE,
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
	if (!xrtChaCha20Poly1305TryOpen(n->CipherKey, nonce, n->Hash, 32,
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

size_t xrtRudpNoiseAllocationSize(void) { return sizeof(xrudpnoise); }

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
	return xrtRudpSentCreateAfter(capacity, 0, output);
}

xrudpresult xrtRudpSentAllocationSize(uint32_t capacity, size_t* output)
{
	if (output == NULL || capacity > XRUDP_SENT_CAPACITY_MAX) return XRUDP_ARGUMENT;
	if (capacity == 0) capacity = XRUDP_SENT_CAPACITY_DEFAULT;
	*output = sizeof(xrudpsentwindow) + (capacity + XRUDP_SENT_PROBE_RESERVE) * sizeof(xrudp__sentpacket);
	return XRUDP_OK;
}

xrudpresult xrtRudpSentCreateAfter(uint32_t capacity, uint64_t retired, xrudpsentwindow** output)
{
	xrudpsentwindow* w;
	size_t size;
	if (output == NULL || capacity > XRUDP_SENT_CAPACITY_MAX || retired == UINT64_MAX) return XRUDP_ARGUMENT;
	if (capacity == 0) capacity = XRUDP_SENT_CAPACITY_DEFAULT;
	(void)xrtRudpSentAllocationSize(capacity, &size);
	w = (xrudpsentwindow*)xrtMalloc(size);
	if (w == NULL) return XRUDP_NO_MEMORY;
	memset(w, 0, size);
	w->NormalCapacity = capacity; w->Capacity = capacity + XRUDP_SENT_PROBE_RESERVE;
	w->LargestSent = retired;
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

xrudpresult xrtRudpSentCheckAcknowledge(const xrudpsentwindow* w, const xrudpack* ack,
	uint64_t now, size_t* count, xrudpackchange* change)
{
	xrudpackchange next;
	xrudpresult result;
	size_t needed = 0;
	if (count != NULL) *count = 0;
	if (w == NULL || ack == NULL || count == NULL || change == NULL || now < w->LastNow) return XRUDP_ARGUMENT;
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
	*change = next; *count = needed;
	return XRUDP_OK;
}

xrudpresult xrtRudpSentAcknowledge(xrudpsentwindow* w, const xrudpack* ack,
	uint64_t now, xrudpsentevent* events, size_t capacity, size_t* count,
	xrudpackchange* change)
{
	xrudpackchange next;
	size_t needed, written = 0;
	xrudpresult result;
	if (count != NULL) *count = 0;
	if (count == NULL || change == NULL || (events == NULL && capacity != 0)) return XRUDP_ARGUMENT;
	result = xrtRudpSentCheckAcknowledge(w, ack, now, &needed, &next);
	if (result != XRUDP_OK) return result;
	if (needed > capacity) return XRUDP_BACKPRESSURE;
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

bool xrtRudpReceiveReady(const xrudpreceive* r)
{
	return r != NULL && (r->Reset || (r->HasFinal && r->ReadOffset == r->FinalOffset) ||
		(r->Buffered != 0 && xrudp__receive_has(r, xrudp__receive_slot(r, r->ReadOffset))));
}

xrudpresult xrtRudpReceiveRestart(xrudpreceive* r, uint64_t limit)
{
	if (r == NULL || limit > r->Capacity) return XRUDP_ARGUMENT;
	if (!r->Reset && !(r->HasFinal && r->ReadOffset == r->FinalOffset) &&
		(r->ReadOffset != 0 || r->HighestOffset != 0 || r->HasFinal || r->Buffered != 0)) return XRUDP_BACKPRESSURE;
	uint32_t capacity = r->Capacity;
	xrtSecureZero(r, sizeof(*r) + capacity + capacity / 8); r->Capacity = capacity; r->Limit = limit;
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

static void xrudp__receive_commit(xrudpreceive* r, uint64_t offset,
	const void* data, size_t size, bool fin)
{
	if (r->Reset) return;
	for (size_t i = 0; i < size; ++i) {
		size_t slot;
		if (offset + i < r->ReadOffset) continue;
		slot = xrudp__receive_slot(r, offset + i);
		if (!xrudp__receive_has(r, slot)) ++r->Buffered;
		r->Data[slot] = ((const unsigned char*)data)[i];
		r->Data[r->Capacity + slot / 8] |= (unsigned char)(1u << (slot % 8));
	}
	if (offset + size > r->HighestOffset) r->HighestOffset = offset + size;
	if (fin) { r->HasFinal = true; r->FinalOffset = offset + size; }
}

xrudpresult xrtRudpReceivePush(xrudpreceive* r, uint64_t offset,
	const void* data, size_t size, bool fin, xrudpreceivechange* change)
{
	xrudpreceivechange next;
	xrudpresult result;
	if (change == NULL) return XRUDP_ARGUMENT;
	result = xrtRudpReceiveCheck(r, offset, data, size, fin, &next);
	if (result != XRUDP_OK && result != XRUDP_DUPLICATE) return result;
	if (result == XRUDP_OK) xrudp__receive_commit(r, offset, data, size, fin);
	*change = next;
	return result;
}

xrudpresult xrtRudpReceiveCheckBatch(const xrudpreceiveframe* frames,
	size_t count, uint64_t credit, xrudpreceivebatchchange* change)
{
	struct plan { const xrudpreceive* Receive; uint64_t Highest, Final; bool HasFinal; } plans[XRUDP_FRAMES_MAX];
	xrudpreceivebatchchange next;
	size_t streams = 0, wireSize = 0;
	if (frames == NULL || change == NULL || count == 0 || count > XRUDP_FRAMES_MAX) return XRUDP_ARGUMENT;
	memset(&next, 0, sizeof(next));
	for (size_t i = 0; i < count; ++i) {
		const xrudpreceiveframe* f = &frames[i];
		xrudpreceivechange single;
		xrudpresult result = xrtRudpReceiveCheck(f->Receive, f->Offset, f->Data, f->Size, f->Fin, &single);
		if (result != XRUDP_OK && result != XRUDP_DUPLICATE) return result;
		if (f->Size + 16 > XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE - wireSize) return XRUDP_LIMIT;
		wireSize += f->Size + 16;
		const xrudpreceive* r = f->Receive;
		size_t p;
		for (p = 0; p < streams && plans[p].Receive != r; ++p) {}
		if (p == streams) {
			plans[p].Receive = r; plans[p].Highest = r->HighestOffset;
			plans[p].Final = r->FinalOffset; plans[p].HasFinal = r->HasFinal; ++streams;
		}
		uint64_t end = f->Offset + f->Size;
		if ((plans[p].HasFinal && (end > plans[p].Final || (f->Fin && end != plans[p].Final))) ||
			(f->Fin && end < plans[p].Highest)) return XRUDP_MALFORMED;
		if (f->Fin && !plans[p].HasFinal) {
			plans[p].HasFinal = true; plans[p].Final = end; ++next.NewFinals;
		}
		if (end > plans[p].Highest) plans[p].Highest = end;
		if (r->Reset) continue;
		for (size_t j = 0; j < i; ++j) {
			const xrudpreceiveframe* old = &frames[j];
			if (old->Receive != r) continue;
			uint64_t first = f->Offset > old->Offset ? f->Offset : old->Offset;
			uint64_t last = end < old->Offset + old->Size ? end : old->Offset + old->Size;
			if (first < r->ReadOffset) first = r->ReadOffset;
			if (first < last && memcmp((const unsigned char*)f->Data + (size_t)(first - f->Offset),
				(const unsigned char*)old->Data + (size_t)(first - old->Offset), (size_t)(last - first)) != 0)
				return XRUDP_MALFORMED;
		}
		for (size_t k = 0; k < f->Size; ++k) {
			uint64_t position = f->Offset + k;
			bool covered = false;
			if (position < r->ReadOffset || xrudp__receive_has(r, xrudp__receive_slot(r, position))) continue;
			for (size_t j = 0; j < i; ++j)
				if (frames[j].Receive == r && position >= frames[j].Offset && position - frames[j].Offset < frames[j].Size) {
					covered = true; break;
				}
			if (!covered) ++next.NewBytes;
		}
	}
	for (size_t i = 0; i < streams; ++i) {
		uint64_t delta = plans[i].Highest - plans[i].Receive->HighestOffset;
		if (delta > credit - next.HighestDelta) return XRUDP_MALFORMED;
		next.HighestDelta += delta;
	}
	next.Streams = (uint32_t)streams; *change = next;
	return next.HighestDelta || next.NewBytes || next.NewFinals ? XRUDP_OK : XRUDP_DUPLICATE;
}

xrudpresult xrtRudpReceivePushBatch(const xrudpreceiveframe* frames,
	size_t count, uint64_t credit, xrudpreceivebatchchange* change)
{
	xrudpreceivebatchchange next;
	xrudpresult result;
	if (change == NULL) return XRUDP_ARGUMENT;
	result = xrtRudpReceiveCheckBatch(frames, count, credit, &next);
	if (result != XRUDP_OK && result != XRUDP_DUPLICATE) return result;
	if (result == XRUDP_OK)
		for (size_t i = 0; i < count; ++i)
			xrudp__receive_commit(frames[i].Receive, frames[i].Offset, frames[i].Data, frames[i].Size, frames[i].Fin);
	*change = next;
	return result;
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

xrudpresult xrtRudpTransmitRestart(xrudptransmit* t, uint64_t limit)
{
	if (t == NULL) return XRUDP_ARGUMENT;
	if (!t->Cancelled && !(t->FinQueued && t->FinAcked && t->Base == t->End) &&
		(t->Base != 0 || t->End != 0 || t->Highest != 0 || t->FinQueued)) return XRUDP_BACKPRESSURE;
	xrtSecureZero(t->Data, t->Allocation - sizeof(*t));
	t->Base = t->End = t->Highest = t->Pending = 0; t->Limit = limit;
	t->FinQueued = t->FinSent = t->FinAcked = t->FinReady = t->Cancelled = false;
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


/* ========================================================================== */
/* source: extlibs/xrudp/src/crypto/rudp_handshake.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_HANDSHAKE)
#include <string.h>

struct xrudphandshake {
	xrudprole Role;
	xrudphandshakephase Phase;
	xrudpresult Failure;
	xrudpnoise* Noise;
	xrudpcipher* Cipher;
	xrudpparameters Local, Peer;
	uint64_t Deadline, Next, LastNow;
	uint32_t Attempts;
	bool Taken;
	unsigned char Static[32], Pin[32], Scope[16], Client[16], Server[16], Hello[96], Cookie[40], Public[32];
	unsigned char LastInput[XRUDP_HANDSHAKE_PACKET_MAX], Output[XRUDP_HANDSHAKE_PACKET_MAX];
	size_t LastSize, OutputSize;
	xrudpnoisetrustproc Trust;
	void* TrustContext;
};

static bool xrudp__hs_nonzero(const void* data, size_t size)
{
	const unsigned char* p = (const unsigned char*)data; unsigned char bits = 0;
	if (p == NULL) return false;
	for (size_t i = 0; i < size; ++i) bits |= p[i];
	return bits != 0;
}
static void xrudp__hs_put(unsigned char* p, uint64_t value, size_t size)
{
	for (size_t i = 0; i < size; ++i) p[size - 1 - i] = (unsigned char)(value >> (i * 8));
}
static uint64_t xrudp__hs_get(const unsigned char* p, size_t size)
{
	uint64_t value = 0; for (size_t i = 0; i < size; ++i) value = (value << 8) | p[i]; return value;
}
static bool xrudp__hs_parameters(const xrudpparameters* p)
{
	return p != NULL && p->ConnectionCredit <= UINT64_C(67108864) && p->StreamCredit <= 1048576 &&
		p->MaxStreams >= 1 && p->MaxStreams <= 1024 && p->IdleTimeoutMs >= 1000 && p->IdleTimeoutMs <= 3600000 &&
		p->MaxAckDelayUs <= 25000 && p->DatagramSize >= 512 && p->DatagramSize <= XRUDP_PACKET_MAX && p->Reserved == 0;
}
void xrtRudpParametersDefault(xrudpparameters* p)
{
	if (p != NULL) {
		memset(p, 0, sizeof(*p)); p->ConnectionCredit = 1048576; p->StreamCredit = 65536;
		p->MaxStreams = 16; p->IdleTimeoutMs = 30000; p->MaxAckDelayUs = 25000; p->DatagramSize = 1200;
	}
}
xrudpresult xrtRudpParametersEncode(const xrudpparameters* p, void* output, size_t capacity)
{
	unsigned char wire[32];
	if (!xrudp__hs_parameters(p) || output == NULL) return XRUDP_ARGUMENT;
	if (capacity < sizeof(wire)) return XRUDP_LIMIT;
	xrudp__hs_put(wire, p->ConnectionCredit, 8); xrudp__hs_put(wire + 8, p->StreamCredit, 4);
	xrudp__hs_put(wire + 12, p->MaxStreams, 4); xrudp__hs_put(wire + 16, p->IdleTimeoutMs, 4);
	xrudp__hs_put(wire + 20, p->MaxAckDelayUs, 4); xrudp__hs_put(wire + 24, p->DatagramSize, 4);
	memset(wire + 28, 0, 4); memmove(output, wire, sizeof(wire)); return XRUDP_OK;
}
xrudpresult xrtRudpParametersDecode(const void* data, size_t size, xrudpparameters* output)
{
	const unsigned char* p = (const unsigned char*)data; xrudpparameters next;
	if (data == NULL || output == NULL) return XRUDP_ARGUMENT;
	if (size != 32) return XRUDP_MALFORMED;
	memset(&next, 0, sizeof(next)); next.ConnectionCredit = xrudp__hs_get(p, 8);
	next.StreamCredit = (uint32_t)xrudp__hs_get(p + 8, 4); next.MaxStreams = (uint32_t)xrudp__hs_get(p + 12, 4);
	next.IdleTimeoutMs = (uint32_t)xrudp__hs_get(p + 16, 4); next.MaxAckDelayUs = (uint32_t)xrudp__hs_get(p + 20, 4);
	next.DatagramSize = (uint32_t)xrudp__hs_get(p + 24, 4); next.Reserved = (uint32_t)xrudp__hs_get(p + 28, 4);
	if (!xrudp__hs_parameters(&next)) return XRUDP_MALFORMED;
	*output = next; return XRUDP_OK;
}
size_t xrtRudpHandshakeAllocationSize(void)
{
	return sizeof(xrudphandshake) + xrtRudpNoiseAllocationSize() + xrtRudpCipherAllocationSize();
}
static xrudpresult xrudp__hs_packet(xrudppackettype type, uint64_t number, const void* id,
	const void* data, size_t size, void* output, size_t capacity, size_t* written)
{
	xrudpwireheader h; memset(&h, 0, sizeof(h)); h.Type = type; h.PacketNumber = number;
	h.PayloadSize = (uint16_t)size; memcpy(h.ConnectionId, id, 16);
	return xrtRudpPacketEncode(output, capacity, written, &h, data, size);
}
static bool xrudp__hs_hello(const unsigned char* hello)
{
	return xrudp__hs_nonzero(hello, 16) && xrudp__hs_nonzero(hello + 16, 32) && xrudp__hs_nonzero(hello + 48, 16);
}
static bool xrudp__hs_hash(const void* scope, const unsigned char hello[96], unsigned char digest[32])
{
	unsigned char packet[144]; size_t size;
	return xrudp__hs_packet(XRUDP_PACKET_HELLO, 1, scope, hello, 96, packet, sizeof(packet), &size) == XRUDP_OK &&
		xrtSha256(packet, size, digest);
}
static bool xrudp__hs_id(const unsigned char digest[32], const unsigned char cookie[40], unsigned char id[16])
{
	static const char domain[] = "XRUDP001/server-id";
	xsha256 hash; unsigned char output[32];
	xrtSha256Init(&hash);
	bool ok = xrtSha256Update(&hash, domain, sizeof(domain) - 1) && xrtSha256Update(&hash, digest, 32) &&
		xrtSha256Update(&hash, cookie, 40) && xrtSha256Final(&hash, output);
	if (ok) memcpy(id, output, 16);
	xrtSecureZero(&hash, sizeof(hash)); xrtSecureZero(output, sizeof(output)); return ok && xrudp__hs_nonzero(id, 16);
}
static size_t xrudp__hs_prologue(const xrudphandshake* h, unsigned char output[192])
{
	static const char domain[] = "XRUDP001/IK/prologue";
	size_t size = sizeof(domain) - 1;
	memcpy(output, domain, size); memcpy(output + size, h->Scope, 16); size += 16;
	memcpy(output + size, h->Hello, 96); size += 96; memcpy(output + size, h->Server, 16); return size + 16;
}
static xrudpresult xrudp__hs_config(const xrudphandshakeconfig* c, bool responder, uint64_t now, uint64_t* deadline)
{
	uint64_t timeout;
	if (c == NULL || !xrudp__hs_nonzero(c->StaticPrivate, 32) || !xrudp__hs_nonzero(c->Scope, 16) ||
		(!responder && !xrudp__hs_nonzero(c->PeerPublic, 32)) ||
		(responder && (!xrudp__hs_nonzero(c->LocalPublic, 32) || (c->PeerPublic == NULL && c->Trust == NULL))) ||
		!xrudp__hs_parameters(&c->Parameters)) return XRUDP_ARGUMENT;
	timeout = c->TimeoutUs ? c->TimeoutUs : XRUDP_HANDSHAKE_DEFAULT_TIMEOUT_US;
	if (timeout < 1000000 || timeout > XRUDP_HANDSHAKE_MAX_TIMEOUT_US || now > UINT64_MAX - timeout) return XRUDP_ARGUMENT;
	*deadline = now + timeout; return XRUDP_OK;
}
static xrudphandshake* xrudp__hs_alloc(const xrudphandshakeconfig* c, xrudprole role, uint64_t now, uint64_t deadline)
{
	xrudphandshake* h = (xrudphandshake*)xrtMalloc(sizeof(*h));
	if (h == NULL) return NULL;
	memset(h, 0, sizeof(*h)); h->Role = role; h->Local = c->Parameters;
	h->Deadline = deadline; h->Next = h->LastNow = now; h->Trust = c->Trust; h->TrustContext = c->TrustContext;
	memcpy(h->Static, c->StaticPrivate, 32); memcpy(h->Scope, c->Scope, 16);
	if (c->PeerPublic != NULL) memcpy(h->Pin, c->PeerPublic, 32);
	return h;
}
void xrtRudpHandshakeDestroy(xrudphandshake* h)
{
	if (h != NULL) { xrtRudpNoiseDestroy(h->Noise); xrtRudpCipherDestroy(h->Cipher); xrtSecureZero(h, sizeof(*h)); xrtFree(h); }
}
static xrudpresult xrudp__hs_fail(xrudphandshake* h, xrudpresult result)
{
	h->Phase = XRUDP_HANDSHAKE_FAILED; h->Failure = result;
	xrtRudpNoiseDestroy(h->Noise); h->Noise = NULL; xrtRudpCipherDestroy(h->Cipher); h->Cipher = NULL;
	xrtSecureZero(h->Static, sizeof(h->Static)); return result;
}
xrudpresult xrtRudpHandshakeCreate(const xrudphandshakeconfig* c, uint64_t now, xrudphandshake** output)
{
	uint64_t deadline;
	xrudpresult result = xrudp__hs_config(c, false, now, &deadline);
	if (result != XRUDP_OK || output == NULL) return result != XRUDP_OK ? result : XRUDP_ARGUMENT;
	xrudphandshake* h = xrudp__hs_alloc(c, XRUDP_INITIATOR, now, deadline);
	if (h == NULL) return XRUDP_NO_MEMORY;
	if (!xrtSecureRandom(h->Client, 16) || !xrtSecureRandom(h->Hello + 48, 16) ||
		!xrudp__hs_nonzero(h->Client, 16) || !xrudp__hs_nonzero(h->Hello + 48, 16) ||
		xrtConstTimeEqual(h->Client, h->Scope, 16)) { xrtRudpHandshakeDestroy(h); return XRUDP_IO_ERROR; }
	memcpy(h->Hello, h->Client, 16); memcpy(h->Hello + 16, h->Pin, 32);
	if (c->ApplicationContext != NULL) memcpy(h->Hello + 64, c->ApplicationContext, 32);
	result = xrudp__hs_packet(XRUDP_PACKET_HELLO, 1, h->Scope, h->Hello, 96, h->Output, sizeof(h->Output), &h->OutputSize);
	if (result != XRUDP_OK) { xrtRudpHandshakeDestroy(h); return result; }
	h->Phase = XRUDP_HANDSHAKE_RETRY; *output = h; return XRUDP_OK;
}
xrudpresult xrtRudpHandshakeRetry(const void* key, const void* scope, const void* publicKey,
	const xrudpaddress* source, uint64_t now, const void* hello, size_t size,
	void* output, size_t capacity, size_t* written)
{
	xrudpwirepacket packet; unsigned char body[88], digest[32], cookie[40]; size_t resultSize;
	if (written != NULL) *written = 0;
	if (!xrudp__hs_nonzero(key, 32) || !xrudp__hs_nonzero(scope, 16) || !xrudp__hs_nonzero(publicKey, 32) ||
		output == NULL || written == NULL || source == NULL || hello == NULL) return XRUDP_ARGUMENT;
	if (xrtRudpPacketDecode(hello, size, &packet) != XRUDP_OK || packet.Header.Type != XRUDP_PACKET_HELLO ||
		packet.Header.PacketNumber != 1 || packet.Header.PayloadSize != 96 || !xrudp__hs_hello(packet.Payload) ||
		!xrtConstTimeEqual(packet.Header.ConnectionId, scope, 16) || !xrtConstTimeEqual(packet.Payload + 16, publicKey, 32) ||
		xrtConstTimeEqual(packet.Payload, scope, 16)) return XRUDP_AUTH_FAILED;
	if (!xrtSha256(hello, size, digest)) return XRUDP_IO_ERROR;
	xrudpresult result = xrtRudpCookieCreate(key, scope, digest, source, now, cookie, sizeof(cookie));
	if (result != XRUDP_OK) return result;
	if (!xrudp__hs_id(digest, cookie, body)) return XRUDP_IO_ERROR;
	if (xrtConstTimeEqual(body, packet.Payload, 16) || xrtConstTimeEqual(body, scope, 16)) return XRUDP_AUTH_FAILED;
	memcpy(body + 16, digest, 32); memcpy(body + 48, cookie, 40);
	result = xrudp__hs_packet(XRUDP_PACKET_RETRY, 1, packet.Payload, body, sizeof(body), output, capacity, &resultSize);
	if (result == XRUDP_OK) *written = resultSize;
	return result;
}
static xrudpresult xrudp__hs_noise(xrudphandshake* h)
{
	unsigned char prologue[192];
	xrudpnoiseconfig c; memset(&c, 0, sizeof(c)); c.Role = h->Role; c.StaticPrivate = h->Static;
	c.PeerPublic = xrudp__hs_nonzero(h->Pin, 32) ? h->Pin : NULL;
	c.PrologueSize = xrudp__hs_prologue(h, prologue); c.Prologue = prologue;
	c.Trust = h->Trust; c.TrustContext = h->TrustContext;
	return xrtRudpNoiseCreate(&c, &h->Noise);
}
static xrudpresult xrudp__hs_peer(xrudphandshake* h)
{
	unsigned char wire[XRUDP_NOISE_PARAMETERS_MAX]; size_t size;
	xrudpnoiseinfo info;
	xrudpresult result = xrtRudpNoisePeerParameters(h->Noise, wire, sizeof(wire), &size);
	if (result == XRUDP_OK) result = xrtRudpParametersDecode(wire, size, &h->Peer);
	if (result != XRUDP_OK) return result;
	result = xrtRudpNoiseInfo(h->Noise, &info);
	if (result == XRUDP_OK) memcpy(h->Public, info.PeerPublic, 32);
	return result;
}
static xrudpresult xrudp__hs_prepare(xrudphandshake* h)
{
	unsigned char plain[40] = {XRUDP_FRAME_PING,0,0,0,XRUDP_FRAME_KEY_CONFIRM,0,0,4,0,0,0,1};
	xrudpresult result;
	if (h->Cipher == NULL) {
		result = xrtRudpNoiseCreateCipher(h->Noise,
			h->Role == XRUDP_INITIATOR ? h->Client : h->Server,
			h->Role == XRUDP_INITIATOR ? h->Server : h->Client, 0, &h->Cipher);
		if (result != XRUDP_OK) return result;
		xrtRudpNoiseDestroy(h->Noise); h->Noise = NULL; xrtSecureZero(h->Static, sizeof(h->Static));
	}
	if (h->Role == XRUDP_INITIATOR && h->OutputSize == 0)
		return xrtRudpCipherSeal(h->Cipher, plain, 12, h->Output, sizeof(h->Output), &h->OutputSize);
	return XRUDP_OK;
}
xrudpresult xrtRudpHandshakeAccept(const xrudphandshakeconfig* c, const void* key, const void* previous,
	const xrudpaddress* source, uint64_t now, const void* data, size_t size, xrudphandshake** output)
{
	uint64_t deadline; unsigned char digest[32], id[16], body[80], params[32];
	xrudpwirepacket packet; size_t noiseSize;
	xrudpresult result = xrudp__hs_config(c, true, now, &deadline);
	if (result != XRUDP_OK || output == NULL || data == NULL || source == NULL || !xrudp__hs_nonzero(key, 32) ||
		(previous != NULL && !xrudp__hs_nonzero(previous, 32)))
		return result != XRUDP_OK ? result : XRUDP_ARGUMENT;
	if (xrtRudpPacketDecode(data, size, &packet) != XRUDP_OK || packet.Header.Type != XRUDP_PACKET_HANDSHAKE ||
		packet.Header.PacketNumber != 1 || packet.Header.PayloadSize != 264 || !xrudp__hs_hello(packet.Payload) ||
		!xrtConstTimeEqual(packet.Payload + 16, c->LocalPublic, 32) ||
		(c->ApplicationContext != NULL && !xrtConstTimeEqual(c->ApplicationContext, packet.Payload + 64, 32))) return XRUDP_AUTH_FAILED;
	if (!xrudp__hs_hash(c->Scope, packet.Payload, digest)) return XRUDP_IO_ERROR;
	result = xrtRudpCookieVerify(key, previous, c->Scope, digest, source, now, 0, packet.Payload + 96, 40);
	if (result != XRUDP_OK) return result;
	if (!xrudp__hs_id(digest, packet.Payload + 96, id)) return XRUDP_IO_ERROR;
	if (!xrtConstTimeEqual(id, packet.Header.ConnectionId, 16) || xrtConstTimeEqual(id, c->Scope, 16) ||
		xrtConstTimeEqual(id, packet.Payload, 16)) return XRUDP_AUTH_FAILED;
	xrudphandshake* h = xrudp__hs_alloc(c, XRUDP_RESPONDER, now, deadline);
	if (h == NULL) return XRUDP_NO_MEMORY;
	memcpy(h->Server, id, 16); memcpy(h->Client, packet.Payload, 16); memcpy(h->Hello, packet.Payload, 96);
	result = xrudp__hs_noise(h);
	if (result == XRUDP_OK) result = xrtRudpNoiseRead(h->Noise, packet.Payload + 136, 128);
	if (result == XRUDP_OK) result = xrtRudpParametersEncode(&h->Local, params, sizeof(params));
	if (result == XRUDP_OK) result = xrtRudpNoiseWrite(h->Noise, params, sizeof(params), body, sizeof(body), &noiseSize);
	if (result == XRUDP_OK) result = xrudp__hs_peer(h);
	if (result == XRUDP_OK) result = xrudp__hs_packet(XRUDP_PACKET_HANDSHAKE, 2, h->Client, body, noiseSize,
		h->Output, sizeof(h->Output), &h->OutputSize);
	if (result == XRUDP_OK) result = xrudp__hs_prepare(h);
	if (result != XRUDP_OK) { xrtRudpHandshakeDestroy(h); return result; }
	memcpy(h->LastInput, data, size); h->LastSize = size; h->Phase = XRUDP_HANDSHAKE_PROOF;
	*output = h; return XRUDP_OK;
}
static xrudpresult xrudp__hs_retry_input(xrudphandshake* h, const xrudpwirepacket* packet, uint64_t now)
{
	unsigned char digest[32], id[16], params[32], body[264]; size_t noiseSize, packetSize;
	xrudphandshake candidate = *h;
	if (packet->Header.Type != XRUDP_PACKET_RETRY || packet->Header.PacketNumber != 1 || packet->Header.PayloadSize != 88 ||
		!xrtConstTimeEqual(packet->Header.ConnectionId, h->Client, 16)) return XRUDP_AUTH_FAILED;
	if (!xrudp__hs_hash(h->Scope, h->Hello, digest) || !xrudp__hs_id(digest, packet->Payload + 48, id)) return XRUDP_IO_ERROR;
	if (!xrtConstTimeEqual(digest, packet->Payload + 16, 32) || !xrtConstTimeEqual(id, packet->Payload, 16) ||
		xrtConstTimeEqual(id, h->Client, 16) || xrtConstTimeEqual(id, h->Scope, 16)) return XRUDP_AUTH_FAILED;
	memcpy(candidate.Server, id, 16); memcpy(candidate.Cookie, packet->Payload + 48, 40);
	xrudpresult result = xrudp__hs_noise(&candidate);
	if (result == XRUDP_OK) result = xrtRudpParametersEncode(&h->Local, params, sizeof(params));
	if (result == XRUDP_OK) result = xrtRudpNoiseWrite(candidate.Noise, params, sizeof(params), body + 136, 128, &noiseSize);
	if (result == XRUDP_OK) {
		memcpy(body, h->Hello, 96); memcpy(body + 96, candidate.Cookie, 40);
		result = xrudp__hs_packet(XRUDP_PACKET_HANDSHAKE, 1, candidate.Server, body, 136 + noiseSize,
			candidate.Output, sizeof(candidate.Output), &packetSize);
	}
	if (result == XRUDP_OK) {
		candidate.OutputSize = packetSize; candidate.Next = now; candidate.Attempts = 0; candidate.Phase = XRUDP_HANDSHAKE_NOISE;
		xrtSecureZero(candidate.Static, sizeof(candidate.Static)); *h = candidate;
	} else xrtRudpNoiseDestroy(candidate.Noise);
	xrtSecureZero(&candidate, sizeof(candidate)); return result;
}
static bool xrudp__hs_proof(const unsigned char* plain, size_t size, bool reply)
{
	static const unsigned char prefix[12] = {XRUDP_FRAME_PING,0,0,0,XRUDP_FRAME_KEY_CONFIRM,0,0,4,0,0,0,1};
	if (size != (reply ? 40u : 12u) || memcmp(plain, prefix, 12) != 0) return false;
	if (!reply) return true;
	static const unsigned char ackHead[12] = {XRUDP_FRAME_ACK,0,0,24,0,0,0,0,0,1,0,0};
	return memcmp(plain + 12, ackHead, 12) == 0 && xrudp__hs_get(plain + 24, 8) == 1 && xrudp__hs_get(plain + 32, 8) == 1;
}
xrudpresult xrtRudpHandshakeInput(xrudphandshake* h, const void* data, size_t size, uint64_t now)
{
	xrudpwirepacket packet; xrudpresult result;
	if (h == NULL || data == NULL || now < h->LastNow) return XRUDP_ARGUMENT;
	if (h->Phase == XRUDP_HANDSHAKE_FAILED) return h->Failure;
	if (h->Phase != XRUDP_HANDSHAKE_ESTABLISHED && now >= h->Deadline) return xrudp__hs_fail(h, XRUDP_TIMEOUT);
	if (size == h->LastSize && memcmp(data, h->LastInput, size) == 0) return XRUDP_DUPLICATE;
	if (size > sizeof(h->LastInput) || xrtRudpPacketDecode(data, size, &packet) != XRUDP_OK) return XRUDP_AUTH_FAILED;
	if (h->Phase == XRUDP_HANDSHAKE_ESTABLISHED) return XRUDP_AUTH_FAILED;
	if (h->Phase == XRUDP_HANDSHAKE_RETRY) result = xrudp__hs_retry_input(h, &packet, now);
	else if (h->Phase == XRUDP_HANDSHAKE_NOISE) {
		if (packet.Header.Type != XRUDP_PACKET_HANDSHAKE || packet.Header.PacketNumber != 2 || packet.Header.PayloadSize != 80 ||
			!xrtConstTimeEqual(packet.Header.ConnectionId, h->Client, 16)) return XRUDP_AUTH_FAILED;
		result = xrtRudpNoiseRead(h->Noise, packet.Payload, 80);
		if (result == XRUDP_OK) {
			result = xrudp__hs_peer(h);
			if (result != XRUDP_OK) return xrudp__hs_fail(h, result);
			h->Phase = XRUDP_HANDSHAKE_PROOF; h->OutputSize = 0; h->Attempts = 0; h->Next = now;
			result = xrudp__hs_prepare(h);
		}
	} else {
		/* Capacity/length is not authenticated until Open verifies the tag. Never
		 * classify a caller scratch LIMIT as authenticated peer misconduct. */
		unsigned char plain[XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE]; size_t length;
		if (h->Cipher == NULL) { result = xrudp__hs_prepare(h); if (result != XRUDP_OK) return result; }
		if (packet.Header.Type != XRUDP_PACKET_DATA || packet.Header.PacketNumber != 1 || packet.Header.KeyEpoch != 1)
			return XRUDP_AUTH_FAILED;
		result = xrtRudpCipherOpen(h->Cipher, data, size, plain, sizeof(plain), &length);
		if (result == XRUDP_MALFORMED) return xrudp__hs_fail(h, XRUDP_MALFORMED);
		if (result != XRUDP_OK) return result;
		if (!xrudp__hs_proof(plain, length, h->Role == XRUDP_INITIATOR)) return xrudp__hs_fail(h, XRUDP_MALFORMED);
		if (h->Role == XRUDP_RESPONDER) {
			unsigned char reply[40] = {XRUDP_FRAME_PING,0,0,0,XRUDP_FRAME_KEY_CONFIRM,0,0,4,0,0,0,1,
				XRUDP_FRAME_ACK,0,0,24,0,0,0,0,0,1,0,0};
			xrudp__hs_put(reply + 24, 1, 8); xrudp__hs_put(reply + 32, 1, 8);
			result = xrtRudpCipherSeal(h->Cipher, reply, sizeof(reply), h->Output, sizeof(h->Output), &h->OutputSize);
			if (result != XRUDP_OK) return xrudp__hs_fail(h, result);
			h->Next = now; h->Attempts = 0;
		} else h->OutputSize = 0;
		h->Phase = XRUDP_HANDSHAKE_ESTABLISHED;
	}
	if (result == XRUDP_OK) { memcpy(h->LastInput, data, size); h->LastSize = size; h->LastNow = now; }
	return result;
}
xrudpresult xrtRudpHandshakeNext(xrudphandshake* h, uint64_t now, void* output,
	size_t capacity, size_t* written, uint64_t* due)
{
	if (written != NULL) *written = 0;
	if (h == NULL || output == NULL || written == NULL || due == NULL || now < h->LastNow) return XRUDP_ARGUMENT;
	if (h->Phase == XRUDP_HANDSHAKE_FAILED) return h->Failure;
	if (now >= h->Deadline) {
		*due = h->Deadline;
		return h->Phase == XRUDP_HANDSHAKE_ESTABLISHED ? XRUDP_DONE : xrudp__hs_fail(h, XRUDP_TIMEOUT);
	}
	if (h->Phase == XRUDP_HANDSHAKE_PROOF && h->Cipher == NULL) {
		xrudpresult result = xrudp__hs_prepare(h); if (result != XRUDP_OK) return result;
	}
	if (h->OutputSize == 0 || h->Attempts >= 16) {
		*due = h->Deadline;
		return h->Phase == XRUDP_HANDSHAKE_ESTABLISHED ? XRUDP_DONE : XRUDP_TIMEOUT;
	}
	*due = h->Next;
	if (now < h->Next) return XRUDP_TIMEOUT;
	if (capacity < h->OutputSize) return XRUDP_LIMIT;
	memmove(output, h->Output, h->OutputSize); *written = h->OutputSize;
	uint64_t delay = UINT64_C(250000) << (h->Attempts < 3 ? h->Attempts : 3);
	++h->Attempts; h->Next = delay > h->Deadline - now ? h->Deadline : now + delay; h->LastNow = now;
	*due = h->Next; return XRUDP_OK;
}
xrudpresult xrtRudpHandshakeInfo(const xrudphandshake* h, xrudphandshakeinfo* output)
{
	xrudphandshakeinfo info;
	if (h == NULL || output == NULL) return XRUDP_ARGUMENT;
	memset(&info, 0, sizeof(info)); info.Role = h->Role; info.Phase = h->Phase; info.Failure = h->Failure;
	info.Attempts = h->Attempts; info.DeadlineUs = h->Deadline; info.NextSendUs = h->Next; info.KeysTaken = h->Taken;
	memcpy(info.LocalId, h->Role == XRUDP_INITIATOR ? h->Client : h->Server, 16);
	memcpy(info.PeerId, h->Role == XRUDP_INITIATOR ? h->Server : h->Client, 16);
	memcpy(info.ApplicationContext, h->Hello + 64, 32);
	if (h->Phase == XRUDP_HANDSHAKE_PROOF || h->Phase == XRUDP_HANDSHAKE_ESTABLISHED) {
		info.PeerParameters = h->Peer; memcpy(info.PeerPublic, h->Public, 32);
	}
	*output = info; return XRUDP_OK;
}
xrudpresult xrtRudpHandshakeTakeCipher(xrudphandshake* h, xrudpcipher** output)
{
	if (h == NULL || output == NULL) return XRUDP_ARGUMENT;
	if (h->Phase != XRUDP_HANDSHAKE_ESTABLISHED || h->Taken) return XRUDP_BACKPRESSURE;
	*output = h->Cipher; h->Cipher = NULL; h->Taken = true; return XRUDP_OK;
}
#endif


/* ========================================================================== */
/* source: extlibs/xrudp/src/network/rudp_session.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_SESSION)
#include <string.h>

typedef struct xrudp__sessionref {
	uint64_t Number, At, Offset, ConnectionGrant, StreamGrant;
	uint32_t Size, Flags, Stream, StreamsGrant;
	bool Used, Fin, Open, Reset;
} xrudp__sessionref;

#define XRUDP__ORDINAL_MAX UINT32_C(2147483648)
#define XRUDP__NO_STREAM UINT32_MAX
typedef struct xrudp__sessionstream {
	xrudpreceive* Rx;
	xrudptransmit* Tx;
	uint64_t InitialCredit, GrantAcked, RetryAt, ResetFinal;
	uint32_t Id, References, Events, Deficit, ResetReason;
	uint16_t Weight;
	uint8_t Priority;
	bool Local, OpenSent, OpenAcked, ResetQueued, ResetSent, ResetAcked;
	bool LastRead, LastDone, LastEof, LastRetire;
} xrudp__sessionstream;

struct xrudpsession {
	xrudpsessionconfig Config;
	xrudpcipher* Cipher;
	xrudpsentwindow* Sent;
	xrudpreceive* Rx;
	xrudptransmit* Tx;
	xrudp__sessionstream* Streams;
	xrudpcongestion Cc;
	xrudprtt Rtt;
	xrudpackset Ack;
	xrudpsessionsendproc Send;
	void* Context;
	xrudpresult Reason;
	bool Busy, Work, WorkStartPending, AckWanted, ConfirmWanted;
	size_t Allocation;
	uint32_t Slots, PtoCount, Probes, ElicitingSinceAck;
	uint32_t StreamSlots, ActiveStreams, ScheduleCursor, EventCursor;
	uint32_t IncomingNext, OutgoingNext, IncomingLimit, OutgoingLimit, StreamsAcked;
	uint64_t Now, Activity, Progress, IdleUs, PtoAt, AckAt, ControlAt;
	uint64_t GrantRetry, ConnectionLimit, ConnectionReceived, ConnectionSent, PeerLimit;
	uint64_t ConnectionAcked, StreamsRetry, PreviousKeyUntil;
	uint64_t Eliciting[XRUDP_REPLAY_BITS];
	uint64_t ReceivedPackets, SentPackets, LostPackets, ProbeTimeouts, KeyUpdates, InvalidPackets, OutputBackpressure;
	xrudpsentevent* Events;
	xrudp__sessionref Ref[];
};

static uint64_t xrudp__session_add(uint64_t a, uint64_t b) { return a > UINT64_MAX - b ? UINT64_MAX : a + b; }
static uint32_t xrudp__session_get32(const unsigned char* p)
{
	uint32_t v = 0; for (size_t i = 0; i < 4; ++i) v = (v << 8) | p[i]; return v;
}
static uint64_t xrudp__session_get64(const unsigned char* p)
{
	uint64_t v = 0; for (size_t i = 0; i < 8; ++i) v = (v << 8) | p[i]; return v;
}
static void xrudp__session_put32(unsigned char* p, uint32_t v)
{
	for (size_t i = 0; i < 4; ++i) p[3 - i] = (unsigned char)(v >> (i * 8));
}
static void xrudp__session_put64(unsigned char* p, uint64_t v)
{
	for (size_t i = 0; i < 8; ++i) p[7 - i] = (unsigned char)(v >> (i * 8));
}
static bool xrudp__session_good(xrudpresult r) { return r == XRUDP_OK || r == XRUDP_DUPLICATE; }
static bool xrudp__session_terminal(xrudpresult r)
{
	return r == XRUDP_CANCELLED || r == XRUDP_TIMEOUT || r == XRUDP_CLOSED || r == XRUDP_MALFORMED ||
		r == XRUDP_IO_ERROR || r == XRUDP_NO_MEMORY || r == XRUDP_LIMIT;
}
static bool xrudp__stream_done(const xrudp__sessionstream* m)
{
	if (m->ResetQueued) return m->ResetAcked;
	xrudptransmitstate t; (void)xrtRudpTransmitState(m->Tx, &t);
	return t.FinAcknowledged && t.PendingBytes == 0;
}
static bool xrudp__stream_eof(const xrudp__sessionstream* m)
{
	xrudpreceivestate r; (void)xrtRudpReceiveState(m->Rx, &r);
	return r.HasFinal && !r.Reset && r.ReadOffset == r.FinalOffset;
}
static bool xrudp__stream_retire(const xrudp__sessionstream* m)
{
	xrudpreceivestate r; (void)xrtRudpReceiveState(m->Rx, &r);
	return m->Id != 1 && m->OpenAcked && m->References == 0 && xrudp__stream_done(m) &&
		(r.Reset || (r.HasFinal && r.ReadOffset == r.FinalOffset));
}
static void xrudp__stream_note(xrudp__sessionstream* m)
{
	bool read = xrtRudpReceiveReady(m->Rx), done = xrudp__stream_done(m), eof = xrudp__stream_eof(m);
	bool retire = xrudp__stream_retire(m);
	if (read && !m->LastRead) m->Events |= XRUDP_STREAM_READ_READY;
	if (done && !m->LastDone) m->Events |= XRUDP_STREAM_WRITE_DONE;
	if (eof && !m->LastEof) m->Events |= XRUDP_STREAM_READ_EOF;
	if (retire && !m->LastRetire) m->Events |= XRUDP_STREAM_RETIRE_READY;
	if (!retire) m->Events &= ~XRUDP_STREAM_RETIRE_READY;
	m->LastRead = read; m->LastDone = done; m->LastEof = eof; m->LastRetire = retire;
}
static bool xrudp__stream_local(const xrudpsession* s, uint32_t id)
{
	return (id & 1u) == (s->Config.Role == XRUDP_INITIATOR ? 1u : 0u);
}
static uint32_t xrudp__stream_find(const xrudpsession* s, uint32_t id)
{
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Id == id && id != 0) return i;
	return XRUDP__NO_STREAM;
}
static xrudpresult xrudp__stream_access(const xrudpsession* s, uint32_t id, uint32_t* index)
{
	if (s == NULL || s->Busy || id == 0) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	*index = xrudp__stream_find(s, id); if (*index != XRUDP__NO_STREAM) return XRUDP_OK;
	uint32_t next = xrudp__stream_local(s, id) ? s->OutgoingNext : s->IncomingNext;
	return (id / 2 + 1) < next ? XRUDP_CLOSED : XRUDP_ARGUMENT;
}
static void xrudp__stream_activate(xrudpsession* s, uint32_t index, uint32_t id,
	bool local, uint64_t credit, uint8_t priority, uint16_t weight)
{
	xrudp__sessionstream* m = &s->Streams[index]; m->Id = id; m->Local = local;
	m->InitialCredit = credit; m->Priority = priority; m->Weight = weight;
	m->GrantAcked = s->Config.Local.StreamCredit; m->OpenAcked = !local || id == 1; m->RetryAt = s->Now;
	(void)xrtRudpTransmitSetLimit(m->Tx, local ? s->Config.Peer.StreamCredit : credit); ++s->ActiveStreams;
}

void xrtRudpSessionConfigInit(xrudpsessionconfig* c)
{
	if (c == NULL) return;
	memset(c, 0, sizeof(*c)); xrtRudpParametersDefault(&c->Local); xrtRudpParametersDefault(&c->Peer);
	c->Local.MaxStreams = 1; c->StreamCapacity = 1; c->Role = XRUDP_INITIATOR;
	c->ReceiveCapacity = c->TransmitCapacity = 65536;
	c->PacketCapacity = 128; c->ControlPerSecond = 128; c->ProgressTimeoutUs = 30000000;
	c->KeyUpdatePackets = 1048576;
}
void xrtRudpStreamConfigInit(xrudpstreamconfig* c)
{
	if (c != NULL) { memset(c, 0, sizeof(*c)); c->Priority = 1; c->Weight = 1; }
}
static xrudpresult xrudp__session_sizes(const xrudpsessionconfig* c, size_t* own, size_t* total)
{
	unsigned char params[32]; size_t rx, tx, sent; xrudpcongestion cc; xrudprtt rtt;
	if (c == NULL || c->PacketCapacity == 0 || c->PacketCapacity > XRUDP_SESSION_PACKETS_MAX ||
		c->ControlPerSecond < 40 || c->ControlPerSecond > 4096 ||
		(c->Role != XRUDP_INITIATOR && c->Role != XRUDP_RESPONDER) || c->StreamCapacity == 0 ||
		c->StreamCapacity > XRUDP_SESSION_STREAM_CAPACITY_MAX || c->Local.MaxStreams > c->StreamCapacity ||
		c->ProgressTimeoutUs < 1000000 || c->ProgressTimeoutUs > 600000000 ||
		c->KeyUpdatePackets < 8 || c->KeyUpdatePackets > XRUDP_KEY_PACKETS_MAX ||
		xrtRudpParametersEncode(&c->Local, params, sizeof(params)) != XRUDP_OK ||
		xrtRudpParametersEncode(&c->Peer, params, sizeof(params)) != XRUDP_OK ||
		xrtRudpReceiveAllocationSize(c->ReceiveCapacity, &rx) != XRUDP_OK ||
		xrtRudpTransmitAllocationSize(c->TransmitCapacity, &tx) != XRUDP_OK ||
		c->Local.StreamCredit > (c->ReceiveCapacity ? c->ReceiveCapacity : XRUDP_RECEIVE_CAPACITY_DEFAULT) ||
		xrtRudpSentAllocationSize(c->PacketCapacity, &sent) != XRUDP_OK ||
		xrtRudpCongestionInit(&cc, c->MaximumWindow) != XRUDP_OK ||
		xrtRudpRttInit(&rtt, c->InitialRttUs, c->Peer.MaxAckDelayUs) != XRUDP_OK) return XRUDP_ARGUMENT;
	*own = sizeof(xrudpsession) + (c->PacketCapacity + XRUDP_SENT_PROBE_RESERVE) *
		(sizeof(xrudp__sessionref) + sizeof(xrudpsentevent)) + (2 * c->StreamCapacity - 1) * sizeof(xrudp__sessionstream);
	*total = *own + (2 * c->StreamCapacity - 1) * (rx + tx) + sent + xrtRudpCipherAllocationSize();
	return XRUDP_OK;
}
xrudpresult xrtRudpSessionAllocationSize(const xrudpsessionconfig* c, size_t* output)
{
	size_t own, total; xrudpresult r;
	if (output == NULL) return XRUDP_ARGUMENT;
	r = xrudp__session_sizes(c, &own, &total); if (r == XRUDP_OK) *output = total; return r;
}
xrudpresult xrtRudpSessionCreate(const xrudpsessionconfig* c, xrudpcipher* cipher,
	uint64_t now, xrudpsessionsendproc send, void* context, xrudpsession** output)
{
	size_t own, total; xrudpcipherstate state; xrudpresult result;
	if (cipher == NULL || send == NULL || output == NULL || xrudp__session_sizes(c, &own, &total) != XRUDP_OK ||
		xrtRudpCipherState(cipher, &state) != XRUDP_OK || state.NextPacketNumber != 2 ||
		state.LargestReceivedPacket != 1 || state.SendEpoch != 1 || state.ReceiveEpoch != 1 || state.SendKeyPending)
		return XRUDP_ARGUMENT;
	xrudpsession* s = (xrudpsession*)xrtMalloc(own);
	if (s == NULL) return XRUDP_NO_MEMORY;
	memset(s, 0, own); s->Allocation = own; s->Slots = c->PacketCapacity + XRUDP_SENT_PROBE_RESERVE;
	s->Events = (xrudpsentevent*)(s->Ref + s->Slots); s->Config = *c; s->Send = send; s->Context = context;
	s->Streams = (xrudp__sessionstream*)(s->Events + s->Slots); s->StreamSlots = 2 * c->StreamCapacity - 1;
	s->IncomingNext = s->OutgoingNext = 2; s->IncomingLimit = s->StreamsAcked = c->Local.MaxStreams; s->OutgoingLimit = c->Peer.MaxStreams;
	s->Now = s->Activity = s->Progress = now; s->AckAt = UINT64_MAX; s->GrantRetry = now;
	s->ConnectionLimit = s->ConnectionAcked = c->Local.ConnectionCredit;
	s->PeerLimit = c->Peer.ConnectionCredit;
	s->IdleUs = (uint64_t)(c->Local.IdleTimeoutMs < c->Peer.IdleTimeoutMs ? c->Local.IdleTimeoutMs : c->Peer.IdleTimeoutMs) * 1000;
	result = xrtRudpSentCreateAfter(c->PacketCapacity, 1, &s->Sent);
	for (uint32_t i = 0; i < s->StreamSlots && result == XRUDP_OK; ++i) {
		result = xrtRudpReceiveCreate(c->ReceiveCapacity, c->Local.StreamCredit, &s->Streams[i].Rx);
		if (result == XRUDP_OK) result = xrtRudpTransmitCreate(c->TransmitCapacity, 0, &s->Streams[i].Tx);
	}
	if (result != XRUDP_OK) {
		for (uint32_t i = 0; i < s->StreamSlots; ++i) { xrtRudpTransmitDestroy(s->Streams[i].Tx); xrtRudpReceiveDestroy(s->Streams[i].Rx); }
		xrtRudpSentDestroy(s->Sent);
		xrtSecureZero(s, own); xrtFree(s); return result;
	}
	(void)xrtRudpCongestionInit(&s->Cc, c->MaximumWindow); s->Cc.LargestSent = 1;
	(void)xrtRudpRttInit(&s->Rtt, c->InitialRttUs, c->Peer.MaxAckDelayUs);
	xrtRudpAckSetInit(&s->Ack); (void)xrtRudpAckSetRecord(&s->Ack, 1, now);
	xrudp__stream_activate(s, 0, 1, true, c->Local.StreamCredit, 1, 1); s->Streams[0].OpenSent = true;
	s->Rx = s->Streams[0].Rx; s->Tx = s->Streams[0].Tx;
	s->Cipher = cipher; *output = s; (void)total; return XRUDP_OK;
}
static bool xrudp__session_work(xrudpsession* s)
{
	if (s->ConnectionLimit > s->ConnectionAcked || s->IncomingLimit > s->StreamsAcked) return true;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Id != 0) {
		xrudp__sessionstream* m = &s->Streams[i]; xrudptransmitstate tx; xrudpreceivestate rx;
		(void)xrtRudpTransmitState(m->Tx, &tx); (void)xrtRudpReceiveState(m->Rx, &rx);
		if (!m->OpenAcked || (m->ResetQueued && !m->ResetAcked) || tx.PendingBytes ||
			(!m->ResetQueued && tx.FinQueued && !tx.FinAcknowledged) || rx.Limit > m->GrantAcked) return true;
	}
	return false;
}
static void xrudp__session_syncwork(xrudpsession* s)
{
	bool work = xrudp__session_work(s);
	if (work && !s->Work) s->Progress = s->Now;
	s->Work = work;
}
static xrudpresult xrudp__session_abort(xrudpsession* s, xrudpresult reason)
{
	if (s->Reason != XRUDP_OK) return XRUDP_DONE;
	xrudptransmitchange tx; xrudpreceivestate rx; xrudpreceivechange reset; size_t count;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) {
		(void)xrtRudpTransmitCancel(s->Streams[i].Tx, &tx);
		(void)xrtRudpReceiveState(s->Streams[i].Rx, &rx);
		(void)xrtRudpReceiveReset(s->Streams[i].Rx, rx.HighestOffset, (uint32_t)reason, &reset);
		s->Streams[i].References = s->Streams[i].Events = 0;
	}
	(void)xrtRudpSentAbort(s->Sent, s->Events, s->Slots, &count);
	(void)xrtRudpCongestionApply(&s->Cc, s->Events, count, s->Now, true);
	memset(s->Ref, 0, s->Slots * sizeof(*s->Ref)); xrtRudpCipherDestroy(s->Cipher); s->Cipher = NULL;
	s->AckWanted = s->ConfirmWanted = s->Work = s->WorkStartPending = false; s->Probes = 0; s->PtoAt = 0; s->Reason = reason;
	return XRUDP_OK;
}
xrudpresult xrtRudpSessionAbort(xrudpsession* s, xrudpresult reason)
{
	if (s == NULL || s->Busy || !xrudp__session_terminal(reason)) return XRUDP_ARGUMENT;
	return xrudp__session_abort(s, reason);
}
void xrtRudpSessionDestroy(xrudpsession* s)
{
	if (s == NULL) return;
	(void)xrudp__session_abort(s, XRUDP_CANCELLED); xrtRudpCipherDestroy(s->Cipher);
	xrtRudpSentDestroy(s->Sent);
	for (uint32_t i = 0; i < s->StreamSlots; ++i) { xrtRudpTransmitDestroy(s->Streams[i].Tx); xrtRudpReceiveDestroy(s->Streams[i].Rx); }
	size_t size = s->Allocation; xrtSecureZero(s, size); xrtFree(s);
}
xrudpresult xrtRudpSessionState(const xrudpsession* s, xrudpsessionstate* output)
{
	if (s == NULL || output == NULL || s->Busy) return XRUDP_ARGUMENT;
	xrudpsessionstate state; memset(&state, 0, sizeof(state)); state.Reason = s->Reason;
	(void)xrtRudpReceiveState(s->Rx, &state.Receive); (void)xrtRudpTransmitState(s->Tx, &state.Transmit);
	(void)xrtRudpSentState(s->Sent, &state.Sent); state.Congestion = s->Cc; state.Rtt = s->Rtt;
	state.ReceivedConnection = s->ConnectionReceived; state.ReceiveLimit = s->ConnectionLimit;
	state.SentConnection = s->ConnectionSent; state.PeerLimit = s->PeerLimit;
	state.LastNowUs = s->Now; state.LastActivityUs = s->Activity; state.ProgressAtUs = s->Progress;
	state.ReceivedPackets = s->ReceivedPackets; state.SentPackets = s->SentPackets; state.LostPackets = s->LostPackets;
	state.ProbeTimeouts = s->ProbeTimeouts; state.KeyUpdates = s->KeyUpdates; state.InvalidPackets = s->InvalidPackets;
	state.OutputBackpressure = s->OutputBackpressure; state.PtoBackoff = s->PtoCount;
	state.OutstandingProbes = s->Probes; state.AckPending = s->AckWanted;
	state.ActiveStreams = s->ActiveStreams; state.IncomingLimit = s->IncomingLimit; state.OutgoingLimit = s->OutgoingLimit;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Events) ++state.PendingStreamEvents;
	state.ReadReady = xrtRudpReceiveReady(s->Rx); *output = state; return XRUDP_OK;
}
xrudpresult xrtRudpSessionStreamState(const xrudpsession* s, uint32_t id, xrudpstreamstate* output)
{
	uint32_t index; if (output == NULL) return XRUDP_ARGUMENT;
	xrudpresult result = xrudp__stream_access(s, id, &index); if (result != XRUDP_OK) return result;
	const xrudp__sessionstream* m = &s->Streams[index]; xrudpstreamstate state; memset(&state, 0, sizeof(state));
	state.Id = id; state.References = m->References; state.Weight = m->Weight; state.Priority = m->Priority;
	state.Local = m->Local; state.OpenAcknowledged = m->OpenAcked; state.ReadReady = xrtRudpReceiveReady(m->Rx);
	state.WriteDone = xrudp__stream_done(m); state.ResetQueued = m->ResetQueued; state.ResetAcknowledged = m->ResetAcked;
	state.RetireReady = xrudp__stream_retire(m);
	state.ResetReason = m->ResetReason; state.ResetFinal = m->ResetFinal;
	(void)xrtRudpReceiveState(m->Rx, &state.Receive); (void)xrtRudpTransmitState(m->Tx, &state.Transmit);
	*output = state; return XRUDP_OK;
}
xrudpresult xrtRudpSessionStreamOpen(xrudpsession* s, const xrudpstreamconfig* c, uint32_t* output)
{
	if (s == NULL || s->Busy || c == NULL || output == NULL || c->Priority > 3 || c->Weight == 0 || c->Weight > 16) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	if (s->OutgoingNext > XRUDP__ORDINAL_MAX) return XRUDP_LIMIT;
	if (s->OutgoingNext > s->OutgoingLimit) return XRUDP_BACKPRESSURE;
	uint32_t index; for (index = 1; index < s->Config.StreamCapacity && s->Streams[index].Id != 0; ++index) {}
	if (index == s->Config.StreamCapacity) return XRUDP_BACKPRESSURE;
	bool prior = s->Work; uint32_t id = (s->OutgoingNext - 1) * 2 + (s->Config.Role == XRUDP_INITIATOR ? 1u : 0u);
	xrudp__stream_activate(s, index, id, true, s->Config.Local.StreamCredit, c->Priority, c->Weight); ++s->OutgoingNext;
	xrudp__session_syncwork(s); if (!prior && s->Work) s->WorkStartPending = true;
	*output = id; return XRUDP_OK;
}
xrudpresult xrtRudpSessionStreamWrite(xrudpsession* s, uint32_t id, const void* data, size_t size, size_t* written)
{
	if (written != NULL) *written = 0;
	if (written == NULL || (data == NULL && size != 0)) return XRUDP_ARGUMENT;
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	bool prior = s->Work;
	xrudpresult result = xrtRudpTransmitWrite(s->Streams[index].Tx, data, size, written); xrudp__session_syncwork(s);
	if (!prior && s->Work) s->WorkStartPending = true;
	return result;
}
xrudpresult xrtRudpSessionStreamFinish(xrudpsession* s, uint32_t id)
{
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	bool prior = s->Work;
	xrudpresult result = xrtRudpTransmitFinish(s->Streams[index].Tx); xrudp__session_syncwork(s);
	if (!prior && s->Work) s->WorkStartPending = true;
	return result;
}
static void xrudp__session_consumed(xrudpsession* s, uint32_t index, uint64_t amount)
{
	xrudp__sessionstream* m = &s->Streams[index]; xrudpreceivestate rx; (void)xrtRudpReceiveState(m->Rx, &rx);
	if (!rx.Reset) (void)xrtRudpReceiveExtend(m->Rx, xrudp__session_add(rx.ReadOffset, s->Config.Local.StreamCredit));
	m->RetryAt = s->Now;
	s->ConnectionLimit = xrudp__session_add(s->ConnectionLimit, amount); s->GrantRetry = s->Now;
	xrudp__session_syncwork(s);
}
xrudpresult xrtRudpSessionStreamRead(xrudpsession* s, uint32_t id, void* output, size_t capacity, size_t* read)
{
	if (read != NULL) *read = 0;
	if (output == NULL || capacity == 0 || read == NULL) return XRUDP_ARGUMENT;
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	bool prior = s->Work;
	xrudpresult result = xrtRudpReceiveRead(s->Streams[index].Rx, output, capacity, read);
	if (*read != 0) { xrudp__session_consumed(s, index, *read); xrudp__stream_note(&s->Streams[index]); }
	if (!prior && s->Work) s->WorkStartPending = true;
	return result;
}
xrudpresult xrtRudpSessionWrite(xrudpsession* s, const void* data, size_t size, size_t* written)
{
	return xrtRudpSessionStreamWrite(s, 1, data, size, written);
}
xrudpresult xrtRudpSessionRead(xrudpsession* s, void* output, size_t capacity, size_t* read)
{
	return xrtRudpSessionStreamRead(s, 1, output, capacity, read);
}
xrudpresult xrtRudpSessionFinish(xrudpsession* s) { return xrtRudpSessionStreamFinish(s, 1); }
xrudpresult xrtRudpSessionStreamReset(xrudpsession* s, uint32_t id, uint32_t reason)
{
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	xrudp__sessionstream* m = &s->Streams[index];
	if (m->ResetQueued) return m->ResetReason == reason ? XRUDP_DUPLICATE : XRUDP_ARGUMENT;
	if (xrudp__stream_done(m)) return XRUDP_DONE;
	bool prior = s->Work; xrudptransmitstate tx; xrudptransmitchange change;
	(void)xrtRudpTransmitState(m->Tx, &tx); (void)xrtRudpTransmitCancel(m->Tx, &change);
	m->ResetQueued = true; m->ResetReason = reason; m->ResetFinal = tx.HighestSent; m->RetryAt = s->Now;
	xrudp__session_syncwork(s); if (!prior && s->Work) s->WorkStartPending = true; return XRUDP_OK;
}
xrudpresult xrtRudpSessionStreamRetire(xrudpsession* s, uint32_t id)
{
	if (id == 1) return XRUDP_ARGUMENT;
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	xrudp__sessionstream* m = &s->Streams[index];
	if (!xrudp__stream_retire(m)) return XRUDP_BACKPRESSURE;
	bool prior = s->Work, local = m->Local; xrudpreceive* r = m->Rx; xrudptransmit* t = m->Tx;
	if (xrtRudpReceiveRestart(r, s->Config.Local.StreamCredit) != XRUDP_OK || xrtRudpTransmitRestart(t, 0) != XRUDP_OK) return XRUDP_IO_ERROR;
	memset(m, 0, sizeof(*m)); m->Rx = r; m->Tx = t; --s->ActiveStreams;
	if (!local && s->IncomingLimit < XRUDP__ORDINAL_MAX) { ++s->IncomingLimit; s->StreamsRetry = s->Now; }
	xrudp__session_syncwork(s); if (!prior && s->Work) s->WorkStartPending = true; return XRUDP_OK;
}
xrudpresult xrtRudpSessionStreamNextEvent(xrudpsession* s, xrudpstreamevent* output)
{
	if (s == NULL || s->Busy || output == NULL) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	for (uint32_t n = 0; n < s->StreamSlots; ++n) {
		uint32_t i = (s->EventCursor + n) % s->StreamSlots; xrudp__sessionstream* m = &s->Streams[i];
		if (m->Id && m->Events) { output->Id = m->Id; output->Events = m->Events; m->Events = 0; s->EventCursor = (i + 1) % s->StreamSlots; return XRUDP_OK; }
	}
	return XRUDP_DONE;
}
static void xrudp__session_clock(xrudpsession* s, uint64_t now)
{
	s->Now = now;
	/* Clock-free application operations cannot use a stale idle timestamp to
	 * start their deadline. Anchor only this idle->work transition at the next
	 * driver observation; further writes/reads cannot continually rearm it. */
	if (s->WorkStartPending) { s->Progress = now; s->WorkStartPending = false; }
}
static bool xrudp__session_dead(xrudpsession* s, uint64_t now)
{
	return now == UINT64_MAX || now - s->Activity >= s->IdleUs || (s->Work && now - s->Progress >= s->Config.ProgressTimeoutUs);
}
static bool xrudp__session_limited(xrudpsession* s)
{
	if (s->ConnectionSent >= s->PeerLimit) return true;
	uint64_t backlog = 0;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Id) {
		xrudptransmitstate tx; (void)xrtRudpTransmitState(s->Streams[i].Tx, &tx);
		if (!tx.Cancelled && tx.HighestSent < tx.PeerLimit) backlog += tx.QueuedEnd - tx.BaseOffset;
	}
	return backlog < s->Cc.Window;
}
static xrudpresult xrudp__session_feedback(xrudpsession* s, size_t count)
{
	for (size_t i = 0; i < count; ++i) {
		const xrudpsentevent* event = &s->Events[i];
		if (event->UserTag == 0 || event->UserTag > s->Slots) return XRUDP_IO_ERROR;
		xrudp__sessionref* ref = &s->Ref[event->UserTag - 1];
		if (!ref->Used || ref->Number != event->PacketNumber) return XRUDP_IO_ERROR;
		if (ref->Stream != XRUDP__NO_STREAM && ref->Stream >= s->StreamSlots) return XRUDP_IO_ERROR;
		if (ref->Stream == XRUDP__NO_STREAM && (ref->Size || ref->Fin || ref->Open || ref->Reset || ref->StreamGrant)) return XRUDP_IO_ERROR;
		xrudp__sessionstream* m = ref->Stream < s->StreamSlots ? &s->Streams[ref->Stream] : NULL;
		if (m != NULL && (m->Id == 0 || m->References == 0)) return XRUDP_IO_ERROR;
		if (m != NULL && !m->ResetQueued && (ref->Size || ref->Fin)) {
			xrudptransmitchange change;
			xrudpresult result = xrtRudpTransmitResolve(m->Tx, ref->Offset, ref->Size, ref->Fin, event->Kind == XRUDP_SENT_ACKED, &change);
			if (!xrudp__session_good(result)) return XRUDP_IO_ERROR;
			if (change.NewlyAckedBytes || change.NewFinAcknowledged) s->Progress = s->Now;
			xrudptransmitstate tx; (void)xrtRudpTransmitState(m->Tx, &tx);
			if (change.ReleasedBytes && !tx.FinQueued) m->Events |= XRUDP_STREAM_WRITE_READY;
		}
		if (event->Kind == XRUDP_SENT_ACKED) {
			if (ref->ConnectionGrant > s->ConnectionAcked) { s->ConnectionAcked = ref->ConnectionGrant; s->Progress = s->Now; }
			if (ref->StreamsGrant > s->StreamsAcked) { s->StreamsAcked = ref->StreamsGrant; s->Progress = s->Now; }
			if (m != NULL) {
				if (ref->StreamGrant > m->GrantAcked) { m->GrantAcked = ref->StreamGrant; s->Progress = s->Now; }
				if (ref->Open && !m->OpenAcked) { m->OpenAcked = true; s->Progress = s->Now; }
				if (ref->Reset && !m->ResetAcked) { m->ResetAcked = true; s->Progress = s->Now; }
			}
		} else if (event->Kind == XRUDP_SENT_LOST) {
			++s->LostPackets;
			if (ref->ConnectionGrant || ref->StreamGrant) s->GrantRetry = s->Now;
			if (ref->StreamsGrant) s->StreamsRetry = s->Now;
			if (m != NULL && (ref->StreamGrant || ref->Open || ref->Reset)) m->RetryAt = s->Now;
		}
		if (m != NULL) { --m->References; xrudp__stream_note(m); }
		memset(ref, 0, sizeof(*ref));
	}
	return XRUDP_OK;
}
static xrudpresult xrudp__session_losses(xrudpsession* s, size_t before)
{
	uint64_t delay, age; size_t count;
	(void)xrtRudpRttLossDelay(&s->Rtt, &delay); (void)xrtRudpRttPto(&s->Rtt, 0, &age);
	age = xrudp__session_add(age, xrudp__session_add(age, age)); if (age < 1000000) age = 1000000;
	xrudpresult result = xrtRudpSentDetectLoss(s->Sent, delay, age, s->Now, s->Events + before, s->Slots - before, &count);
	if (result != XRUDP_OK && result != XRUDP_DONE) return XRUDP_IO_ERROR;
	result = xrtRudpCongestionApply(&s->Cc, s->Events, before + count, s->Now, xrudp__session_limited(s));
	return result == XRUDP_OK ? xrudp__session_feedback(s, before + count) : XRUDP_IO_ERROR;
}
static void xrudp__session_ackwant(xrudpsession* s, bool immediate)
{
	uint64_t due = immediate ? s->Now : xrudp__session_add(s->Now, s->Config.Local.MaxAckDelayUs);
	if (!s->AckWanted || due < s->AckAt) s->AckAt = due;
	s->AckWanted = true;
}
static void xrudp__session_pto_sync(xrudpsession* s, bool progress)
{
	xrudpsentstate sent; uint64_t interval; (void)xrtRudpSentState(s->Sent, &sent);
	if (progress) { s->PtoCount = 0; s->Probes = 0; s->PtoAt = 0; }
	if (sent.BytesInFlight == 0) { s->PtoAt = 0; s->Probes = 0; }
	else if (s->PtoAt == 0) {
		(void)xrtRudpRttPto(&s->Rtt, s->PtoCount, &interval); s->PtoAt = xrudp__session_add(s->Now, interval);
	}
}

typedef struct xrudp__streamplan {
	xrudp__sessionstream* Stream;
	xrudpreceivestate Rx;
	uint64_t Highest, Final, TxLimit, ResetFinal, Credit;
	uint32_t Index, Id, ResetReason;
	uint16_t Weight;
	uint8_t Priority;
	bool New, HasFinal, HasReset, HasOpen;
} xrudp__streamplan;
static xrudpresult xrudp__plan_get(xrudpsession* s, xrudp__streamplan* plans, size_t* count,
	uint32_t id, bool opening, xrudp__streamplan** output)
{
	*output = NULL;
	if (opening && (id == 1 || xrudp__stream_local(s, id))) return XRUDP_MALFORMED;
	for (size_t i = 0; i < *count; ++i) if (plans[i].Id == id) { *output = &plans[i]; return XRUDP_OK; }
	uint32_t index = xrudp__stream_find(s, id); bool fresh = index == XRUDP__NO_STREAM;
	if (fresh) {
		bool local = xrudp__stream_local(s, id); uint32_t ordinal = id / 2 + 1;
		if (ordinal < (local ? s->OutgoingNext : s->IncomingNext)) return XRUDP_OK; /* Retired, never reopened. */
		if (local || ordinal > s->IncomingLimit) return XRUDP_MALFORMED;
		if (!opening) return XRUDP_BACKPRESSURE; /* OPEN was reordered/lost. */
		for (index = s->Config.StreamCapacity; index < s->StreamSlots; ++index) {
			bool chosen = s->Streams[index].Id != 0;
			for (size_t i = 0; i < *count; ++i) if (plans[i].Index == index) chosen = true;
			if (!chosen) break;
		}
		if (index == s->StreamSlots) return XRUDP_BACKPRESSURE;
	}
	if (*count == XRUDP_FRAMES_MAX) return XRUDP_MALFORMED;
	xrudp__streamplan* plan = &plans[(*count)++]; memset(plan, 0, sizeof(*plan));
	plan->Index = index; plan->Id = id; plan->Stream = &s->Streams[index]; plan->New = fresh;
	(void)xrtRudpReceiveState(plan->Stream->Rx, &plan->Rx);
	xrudptransmitstate tx; (void)xrtRudpTransmitState(plan->Stream->Tx, &tx); plan->TxLimit = tx.PeerLimit;
	plan->Highest = plan->Rx.HighestOffset; plan->HasFinal = plan->Rx.HasFinal != 0; plan->Final = plan->Rx.FinalOffset;
	*output = plan; return XRUDP_OK;
}
static void xrudp__session_epoch(xrudpsession* s, uint32_t previous)
{
	xrudpcipherstate current; (void)xrtRudpCipherState(s->Cipher, &current);
	if (current.ReceiveEpoch == previous) return;
	uint64_t grace; (void)xrtRudpRttPto(&s->Rtt, 0, &grace);
	grace = xrudp__session_add(grace, xrudp__session_add(grace, grace));
	if (grace < 1000000) grace = 1000000;
	if (grace > 60000000) grace = 60000000;
	s->PreviousKeyUntil = xrudp__session_add(s->Now, grace); s->ConfirmWanted = true;
}
xrudpresult xrtRudpSessionInput(xrudpsession* s, const void* packet, size_t size, uint64_t now)
{
	if (s == NULL || s->Busy || packet == NULL || now < s->Now) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	xrudp__session_clock(s, now);
	if (xrudp__session_dead(s, now)) { (void)xrudp__session_abort(s, XRUDP_TIMEOUT); return XRUDP_TIMEOUT; }
	unsigned char plain[XRUDP_PACKET_MAX]; size_t length, cursor = 0;
	xrudpcipherstate previous; xrudpwirepacket parsed;
	(void)xrtRudpCipherState(s->Cipher, &previous);
	xrudpresult result = xrtRudpCipherOpen(s->Cipher, packet, size, plain, sizeof(plain), &length);
	if (result == XRUDP_AUTH_FAILED) { ++s->InvalidPackets; return result; }
	if (result == XRUDP_DUPLICATE || result == XRUDP_TOO_OLD) {
		if (result == XRUDP_DUPLICATE && xrtRudpPacketDecode(packet, size, &parsed) == XRUDP_OK &&
			xrtRudpAckSetContains(&s->Ack, parsed.Header.PacketNumber) &&
			s->Eliciting[parsed.Header.PacketNumber % XRUDP_REPLAY_BITS] == parsed.Header.PacketNumber)
			xrudp__session_ackwant(s, true);
		return result;
	}
	if (result != XRUDP_OK) {
		(void)xrudp__session_abort(s, result == XRUDP_MALFORMED ? XRUDP_MALFORMED : XRUDP_IO_ERROR); return s->Reason;
	}
	(void)xrtRudpPacketDecode(packet, size, &parsed);
	if (parsed.Header.PacketNumber < 2 || size > s->Config.Local.DatagramSize) goto malformed;
	xrudpwireframe frames[XRUDP_FRAMES_MAX]; size_t offsets[XRUDP_FRAMES_MAX], ends[XRUDP_FRAMES_MAX], count = 0;
	while (cursor < length) {
		if (count == XRUDP_FRAMES_MAX) goto malformed;
		offsets[count] = cursor;
		if (xrtRudpFrameNext(plain, length, &cursor, &frames[count]) != XRUDP_OK) goto malformed;
		ends[count++] = cursor;
	}
	xrudp__streamplan plans[XRUDP_FRAMES_MAX]; size_t planCount = 0, opened = 0;
	/* OPEN may follow its STREAM in the same packet. Reserve candidate slots
	 * without exposing any IDs, data, metadata or events until ALL preflight. */
	for (size_t i = 0; i < count; ++i) if (frames[i].Type == XRUDP_FRAME_OPEN) {
		const unsigned char* d = frames[i].Data; xrudp__streamplan* plan;
		result = xrudp__plan_get(s, plans, &planCount, xrudp__session_get32(d), true, &plan);
		if (result == XRUDP_BACKPRESSURE) goto deferred;
		if (result != XRUDP_OK) goto malformed;
		if (plan == NULL) continue;
		uint64_t credit = xrudp__session_get64(d + 4); uint16_t weight = (uint16_t)(((uint16_t)d[14] << 8) | d[15]);
		if (d[12] != 0 || credit > XRUDP_RECEIVE_CAPACITY_MAX) goto malformed;
		if (plan->HasOpen) {
			if (plan->Credit != credit || plan->Priority != d[13] || plan->Weight != weight) goto malformed;
		} else {
			if (!plan->New && (plan->Stream->InitialCredit != credit || plan->Stream->Priority != d[13] || plan->Stream->Weight != weight)) goto malformed;
			plan->HasOpen = true; plan->Credit = credit; plan->Priority = d[13]; plan->Weight = weight;
			if (plan->New) { ++opened; plan->TxLimit = credit; }
		}
	}
	/* Ordered admission bounds lifetime history: every lower ordinal was
	 * already opened, so absent older IDs are known retired with no tombstones.
	 * Future reordered OPENs are dropped without ACK, never called malformed. */
	for (uint32_t n = 0; n < opened; ++n) {
		bool found = false;
		for (size_t j = 0; j < planCount; ++j) if (plans[j].New && plans[j].Id / 2 + 1 == s->IncomingNext + n) found = true;
		if (!found) goto deferred;
	}
	xrudpreceiveframe streams[XRUDP_FRAMES_MAX]; size_t streamCount = 0;
	uint64_t peerLimit = s->PeerLimit; uint32_t outgoingLimit = s->OutgoingLimit;
	bool eliciting = false, closing = false;
	for (size_t i = 0; i < count; ++i) {
		const xrudpwireframe* f = &frames[i]; xrudp__streamplan* plan = NULL;
		if (f->Type != XRUDP_FRAME_ACK && f->Type != XRUDP_FRAME_PADDING && f->Type != XRUDP_FRAME_KEY_CONFIRM) eliciting = true;
		if (f->Type == XRUDP_FRAME_STREAM || f->Type == XRUDP_FRAME_RESET || f->Type == XRUDP_FRAME_MAX_STREAM_DATA) {
			result = xrudp__plan_get(s, plans, &planCount, xrudp__session_get32(f->Data), false, &plan);
			if (result == XRUDP_BACKPRESSURE) goto deferred;
			if (result != XRUDP_OK) goto malformed;
			if (plan == NULL) continue; /* Late contents cannot reopen a retired ID or grant connection credit. */
		}
		switch (f->Type) {
		case XRUDP_FRAME_PADDING: case XRUDP_FRAME_PING: case XRUDP_FRAME_KEY_CONFIRM: case XRUDP_FRAME_OPEN: break;
		case XRUDP_FRAME_ACK: {
			xrudpack ack; xrudpackchange change; size_t events;
			if (xrtRudpAckDecode(plain + offsets[i], ends[i] - offsets[i], &ack) != XRUDP_OK ||
				xrtRudpSentCheckAcknowledge(s->Sent, &ack, now, &events, &change) != XRUDP_OK) goto malformed;
			break;
		}
		case XRUDP_FRAME_STREAM: {
			xrudpreceiveframe* sf = &streams[streamCount++]; sf->Receive = plan->Stream->Rx;
			sf->Offset = xrudp__session_get64(f->Data + 4); sf->Size = f->Size - 12;
			sf->Data = f->Data + 12; sf->Fin = (f->Flags & XRUDP_STREAM_FIN) != 0;
			uint64_t end = sf->Offset + sf->Size;
			if (end > plan->Highest) plan->Highest = end;
			if (sf->Fin) { if (plan->HasFinal && end != plan->Final) goto malformed; plan->HasFinal = true; plan->Final = end; }
			break;
		}
		case XRUDP_FRAME_RESET: {
			uint64_t value = xrudp__session_get64(f->Data + 4); uint32_t reason = xrudp__session_get32(f->Data + 12);
			if (plan->HasReset && (plan->ResetFinal != value || plan->ResetReason != reason)) goto malformed;
			plan->HasReset = true; plan->ResetFinal = value; plan->ResetReason = reason; break;
		}
		case XRUDP_FRAME_MAX_DATA: {
			uint64_t value = xrudp__session_get64(f->Data); if (value > peerLimit) peerLimit = value; break;
		}
		case XRUDP_FRAME_MAX_STREAM_DATA: {
			uint64_t value = xrudp__session_get64(f->Data + 4); if (value > plan->TxLimit) plan->TxLimit = value; break;
		}
		case XRUDP_FRAME_MAX_STREAMS: {
			uint32_t value = xrudp__session_get32(f->Data); if (value > XRUDP__ORDINAL_MAX) goto malformed;
			if (value > outgoingLimit) outgoingLimit = value;
			break;
		}
		case XRUDP_FRAME_CLOSE: closing = true; break;
		default: goto malformed;
		}
	}
	uint64_t charge = 0;
	for (size_t i = 0; i < planCount; ++i) {
		xrudp__streamplan* p = &plans[i];
		if (p->HasFinal && p->Highest > p->Final) goto malformed;
		if (p->HasReset) {
			if (p->ResetFinal < p->Highest || p->ResetFinal > p->Rx.Limit || (p->HasFinal && p->ResetFinal != p->Final) ||
				(p->Rx.Reset && p->ResetReason != p->Rx.ResetReason)) goto malformed;
			p->Highest = p->ResetFinal;
		}
		uint64_t delta = p->Highest - p->Rx.HighestOffset;
		if (delta > s->ConnectionLimit - s->ConnectionReceived - charge) goto malformed;
		charge += delta;
	}
	if (streamCount) {
		xrudpreceivebatchchange change;
		if (!xrudp__session_good(xrtRudpReceiveCheckBatch(streams, streamCount, s->ConnectionLimit - s->ConnectionReceived, &change))) goto malformed;
	}
	/* No peer-dependent rejection, allocation or callback after this point. */
	for (size_t i = 0; i < planCount; ++i) if (plans[i].New) {
		xrudp__streamplan* p = &plans[i];
		xrudp__stream_activate(s, p->Index, p->Id, false, p->Credit, p->Priority, p->Weight);
		p->Stream->Events |= XRUDP_STREAM_OPENED;
	}
	s->IncomingNext += (uint32_t)opened; if (opened) s->Progress = now;
	if (streamCount) {
		xrudpreceivebatchchange change;
		if (!xrudp__session_good(xrtRudpReceivePushBatch(streams, streamCount, s->ConnectionLimit - s->ConnectionReceived, &change))) goto internal;
		if (change.NewBytes || change.NewFinals) s->Progress = now;
	}
	s->ConnectionReceived += charge;
	for (size_t i = 0; i < planCount; ++i) {
		xrudp__streamplan* p = &plans[i];
		if (p->HasReset) {
			xrudpreceivechange change;
			if (!xrudp__session_good(xrtRudpReceiveReset(p->Stream->Rx, p->ResetFinal, p->ResetReason, &change))) goto internal;
			if (change.ConsumedDelta) { s->Progress = now; xrudp__session_consumed(s, p->Index, change.ConsumedDelta); }
		}
		xrudptransmitstate tx; (void)xrtRudpTransmitState(p->Stream->Tx, &tx);
		if (!tx.Cancelled) {
			if (p->TxLimit > tx.PeerLimit) s->Progress = now;
			(void)xrtRudpTransmitSetLimit(p->Stream->Tx, p->TxLimit);
		}
		xrudp__stream_note(p->Stream);
	}
	if (peerLimit > s->PeerLimit || outgoingLimit > s->OutgoingLimit) s->Progress = now;
	s->PeerLimit = peerLimit; s->OutgoingLimit = outgoingLimit;
	size_t eventCount = 0; bool progress = false; xrudpackchange sample; memset(&sample, 0, sizeof(sample));
	for (size_t i = 0; i < count; ++i) if (frames[i].Type == XRUDP_FRAME_ACK) {
		xrudpack ack; xrudpackchange change; size_t events;
		(void)xrtRudpAckDecode(plain + offsets[i], ends[i] - offsets[i], &ack);
		if (xrtRudpSentAcknowledge(s->Sent, &ack, now, s->Events + eventCount, s->Slots - eventCount, &events, &change) != XRUDP_OK) goto internal;
		eventCount += events; if (change.NewlyAckedEliciting) progress = true;
		if (change.RttSampleValid && (!sample.RttSampleValid || change.LargestAcked > sample.LargestAcked)) sample = change;
	}
	if (sample.RttSampleValid) (void)xrtRudpRttSample(&s->Rtt, sample.RttSampleUs, sample.PeerAckDelayUs);
	if (xrudp__session_losses(s, eventCount) != XRUDP_OK) goto internal;
	xrudp__session_epoch(s, previous.ReceiveEpoch);
	result = xrtRudpAckSetRecord(&s->Ack, parsed.Header.PacketNumber, now);
	if (result != XRUDP_OK && result != XRUDP_DUPLICATE && result != XRUDP_TOO_OLD) goto internal;
	if (eliciting) {
		s->Eliciting[parsed.Header.PacketNumber % XRUDP_REPLAY_BITS] = parsed.Header.PacketNumber;
		if (s->ElicitingSinceAck < 2) ++s->ElicitingSinceAck;
		xrudp__session_ackwant(s, s->ElicitingSinceAck >= 2);
	}
	s->Activity = now; ++s->ReceivedPackets; xrudp__session_pto_sync(s, progress); xrudp__session_syncwork(s);
	xrtSecureZero(plain, sizeof(plain));
	if (closing) { (void)xrudp__session_abort(s, XRUDP_CLOSED); return XRUDP_CLOSED; }
	return XRUDP_OK;
deferred:
	/* Crypto authenticated a possible key update, but no connection frames
	 * were admitted. Confirm that epoch without falsely ACKing this packet. */
	xrudp__session_epoch(s, previous.ReceiveEpoch); xrtSecureZero(plain, sizeof(plain)); return XRUDP_BACKPRESSURE;
malformed:
	xrtSecureZero(plain, sizeof(plain)); (void)xrudp__session_abort(s, XRUDP_MALFORMED); return XRUDP_MALFORMED;
internal:
	xrtSecureZero(plain, sizeof(plain)); (void)xrudp__session_abort(s, XRUDP_IO_ERROR); return XRUDP_IO_ERROR;
}

static bool xrudp__session_append(unsigned char* plain, size_t maximum, size_t* used,
	xrudpframetype type, uint8_t flags, const void* data, size_t size)
{
	size_t written;
	if (xrtRudpFrameAppend(plain + *used, maximum - *used, &written, type, flags, data, size) != XRUDP_OK) return false;
	*used += written; return true;
}
static bool xrudp__stream_control(xrudpsession* s, xrudp__sessionstream* m, bool probe)
{
	xrudpreceivestate rx; (void)xrtRudpReceiveState(m->Rx, &rx);
	return (probe || s->Now >= m->RetryAt) &&
		(!m->OpenAcked || (m->ResetQueued && !m->ResetAcked) || rx.Limit > m->GrantAcked);
}
static bool xrudp__stream_data(xrudpsession* s, xrudp__sessionstream* m, bool probe)
{
	if (m->ResetQueued) return false;
	unsigned char byte; xrudptransmitframe frame;
	return xrtRudpTransmitPeek(m->Tx, probe, &byte, 1, &frame) == XRUDP_OK &&
		frame.HighestDelta <= s->PeerLimit - s->ConnectionSent;
}
static uint32_t xrudp__session_select(xrudpsession* s, bool probe)
{
	uint32_t first = XRUDP__NO_STREAM;
	for (uint32_t i = 1; i < s->Config.StreamCapacity; ++i) {
		xrudp__sessionstream* m = &s->Streams[i];
		if (m->Id && !m->OpenAcked && (!m->OpenSent || probe) &&
			(first == XRUDP__NO_STREAM || m->Id < s->Streams[first].Id)) first = i;
	}
	if (first != XRUDP__NO_STREAM) return first;
	for (uint32_t n = 0; n < s->StreamSlots; ++n) {
		uint32_t i = (s->ScheduleCursor + n) % s->StreamSlots; xrudp__sessionstream* m = &s->Streams[i];
		if (m->Id && (xrudp__stream_control(s, m, probe) || xrudp__stream_data(s, m, probe))) return i;
	}
	return XRUDP__NO_STREAM;
}
static xrudpresult xrudp__session_send_inner(xrudpsession* s, uint64_t* due,
	unsigned char* plain, unsigned char* body, unsigned char* packet)
{
	size_t used = 0, ackSize = 0, size, slot;
	uint32_t maximum = s->Config.Peer.DatagramSize < s->Config.Local.DatagramSize ? s->Config.Peer.DatagramSize : s->Config.Local.DatagramSize;
	size_t limit = maximum - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE;
	xrudpcipherstate cipher; (void)xrtRudpCipherState(s->Cipher, &cipher);
	bool probe = s->Probes != 0;
	uint32_t selected = xrudp__session_select(s, probe);
	xrudp__sessionstream* m = selected != XRUDP__NO_STREAM ? &s->Streams[selected] : NULL;
	xrudpreceivestate rx; memset(&rx, 0, sizeof(rx)); if (m) (void)xrtRudpReceiveState(m->Rx, &rx);
	bool open = m && m->Local && m->Id != 1 && !m->OpenAcked;
	bool reset = m && m->ResetQueued && !m->ResetAcked && (probe || !m->ResetSent || s->Now >= m->RetryAt);
	bool streamGrant = m && rx.Limit > m->GrantAcked && (probe || s->Now >= m->RetryAt);
	bool grant = s->ConnectionLimit > s->ConnectionAcked && s->Now >= s->GrantRetry;
	bool streamsGrant = s->IncomingLimit > s->StreamsAcked && s->Now >= s->StreamsRetry;
	bool data = false, eliciting = open || reset || streamGrant || grant || streamsGrant || probe || s->ConfirmWanted;
	uint32_t quantum = m ? XRUDP_PACKET_MAX * (m->Weight < 16 ? m->Weight : 16u) * (4u - m->Priority) : 0;
	uint32_t deficit = m && m->Deficit ? m->Deficit : quantum;
	if (s->AckWanted) {
		xrudpackset ack = s->Ack; size_t ranges = (limit - 12 - 128) / 16;
		if (ack.Count > ranges) ack.Count = (uint16_t)ranges;
		if (xrtRudpAckSetEncode(&ack, s->Now, plain, limit, &used) != XRUDP_OK) return XRUDP_IO_ERROR;
		ackSize = used;
	}
	if (grant) {
		xrudp__session_put64(body, s->ConnectionLimit);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_MAX_DATA, 0, body, 8)) return XRUDP_IO_ERROR;
	}
	if (streamsGrant) {
		xrudp__session_put32(body, s->IncomingLimit);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_MAX_STREAMS, 0, body, 4)) return XRUDP_IO_ERROR;
	}
	if (open) {
		xrudp__session_put32(body, m->Id); xrudp__session_put64(body + 4, m->InitialCredit);
		body[12] = 0; body[13] = m->Priority; body[14] = (unsigned char)(m->Weight >> 8); body[15] = (unsigned char)m->Weight;
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_OPEN, 0, body, 16)) return XRUDP_IO_ERROR;
	}
	if (streamGrant) {
		xrudp__session_put32(body, m->Id); xrudp__session_put64(body + 4, rx.Limit);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_MAX_STREAM_DATA, 0, body, 12)) return XRUDP_IO_ERROR;
	}
	if (reset) {
		xrudp__session_put32(body, m->Id); xrudp__session_put64(body + 4, m->ResetFinal); xrudp__session_put32(body + 12, m->ResetReason);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_RESET, 0, body, 16)) return XRUDP_IO_ERROR;
	}
	xrudptransmitframe frame; memset(&frame, 0, sizeof(frame));
	if (m && !m->ResetQueued) {
		size_t chunk = limit - used - 24;
		uint32_t overhead = (uint32_t)(used + 24 + XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE);
		size_t service = deficit > overhead ? deficit - overhead : 1;
		if (chunk > service) chunk = service;
		xrudpresult peek = xrtRudpTransmitPeek(m->Tx, probe, body + 12, chunk, &frame);
		if (peek == XRUDP_OK) {
			uint64_t remaining = s->PeerLimit - s->ConnectionSent;
			if (frame.HighestDelta > remaining) {
				uint64_t trim = frame.HighestDelta - remaining;
				if (trim >= frame.Size) { frame.Size = 0; frame.Fin = 0; }
				else { frame.Size -= (uint32_t)trim; frame.Fin = 0; }
			}
			if (frame.Size || frame.Fin) {
				xrudp__session_put32(body, m->Id); xrudp__session_put64(body + 4, frame.Offset);
				if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_STREAM, frame.Fin ? XRUDP_STREAM_FIN : 0, body, frame.Size + 12)) return XRUDP_IO_ERROR;
				data = eliciting = true;
			}
		} else if (peek != XRUDP_BACKPRESSURE && peek != XRUDP_DONE) return XRUDP_IO_ERROR;
	}
	if (eliciting && !data && !open && !reset && !streamGrant && !grant && !streamsGrant)
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_PING, 0, NULL, 0)) return XRUDP_IO_ERROR;
	if (used == 0) return XRUDP_DONE;
	if (eliciting) {
		xrudp__session_put32(body, cipher.ReceiveEpoch);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_KEY_CONFIRM, 0, body, 4)) return XRUDP_IO_ERROR;
	}
	uint32_t flags = eliciting ? XRUDP_SENT_ACK_ELICITING : 0;
	if (probe) flags |= XRUDP_SENT_PROBE;
	uint64_t at = s->Now;
	xrudpresult admission = xrtRudpCongestionCanSend(&s->Cc, s->Now, s->Rtt.SmoothedUs,
		(uint32_t)(used + XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE), flags, &at);
	if (!xrtRudpSentCanRecord(s->Sent, probe)) admission = XRUDP_BACKPRESSURE;
	if (admission != XRUDP_OK) {
		if (admission == XRUDP_TIMEOUT && at < *due) *due = at;
		if (ackSize == 0 || !xrtRudpSentCanRecord(s->Sent, false)) return XRUDP_DONE;
		used = ackSize; data = grant = streamsGrant = streamGrant = open = reset = probe = eliciting = false; flags = 0;
	}
	if (!eliciting) {
		at = s->AckAt > s->ControlAt ? s->AckAt : s->ControlAt;
		if (s->Now < at) { if (at < *due) *due = at; return XRUDP_DONE; }
	}
	for (slot = 0; slot < s->Slots && s->Ref[slot].Used; ++slot) {}
	if (slot == s->Slots) return XRUDP_IO_ERROR;
	if (cipher.PacketsInSendEpoch >= s->Config.KeyUpdatePackets && !cipher.SendKeyPending) {
		if (xrtRudpCipherUpdateKey(s->Cipher) != XRUDP_OK) return XRUDP_IO_ERROR;
		++s->KeyUpdates;
	}
	xrudpresult result = xrtRudpCipherSeal(s->Cipher, plain, used, packet, XRUDP_PACKET_MAX, &size);
	if (result != XRUDP_OK) return result == XRUDP_LIMIT ? XRUDP_LIMIT : XRUDP_IO_ERROR;
	result = s->Send(packet, size, s->Context);
	if (result == XRUDP_BACKPRESSURE || result == XRUDP_NO_MEMORY) {
		++s->OutputBackpressure; at = xrudp__session_add(s->Now, 1000); if (at < *due) *due = at;
		return XRUDP_DONE;
	}
	if (result != XRUDP_OK) return XRUDP_IO_ERROR;
	(void)xrtRudpCipherState(s->Cipher, &cipher); uint64_t number = cipher.NextPacketNumber - 1;
	if (xrtRudpSentRecord(s->Sent, number, s->Now, (uint32_t)size, flags, slot + 1) != XRUDP_OK ||
		xrtRudpCongestionOnSent(&s->Cc, number, s->Now, s->Rtt.SmoothedUs, (uint32_t)size, flags) != XRUDP_OK) return XRUDP_IO_ERROR;
	xrudp__sessionref* ref = &s->Ref[slot]; memset(ref, 0, sizeof(*ref)); ref->Used = true;
	ref->Number = number; ref->At = s->Now; ref->Flags = flags; ref->Stream = XRUDP__NO_STREAM;
	if (data || open || reset || streamGrant) { ref->Stream = selected; ++m->References; }
	if (data) {
		xrudptransmitchange change;
		if (xrtRudpTransmitMarkSent(m->Tx, frame.Offset, frame.Size, frame.Fin != 0, &change) != XRUDP_OK) return XRUDP_IO_ERROR;
		s->ConnectionSent += change.HighestDelta; ref->Offset = frame.Offset; ref->Size = frame.Size; ref->Fin = frame.Fin != 0;
	}
	uint64_t interval; (void)xrtRudpRttPto(&s->Rtt, 0, &interval);
	if (open) { ref->Open = true; m->OpenSent = true; }
	if (reset) { ref->Reset = true; m->ResetSent = true; }
	if (streamGrant) ref->StreamGrant = rx.Limit;
	if (open || reset || streamGrant) m->RetryAt = xrudp__session_add(s->Now, interval);
	if (grant) { ref->ConnectionGrant = s->ConnectionLimit; s->GrantRetry = xrudp__session_add(s->Now, interval); }
	if (streamsGrant) { ref->StreamsGrant = s->IncomingLimit; s->StreamsRetry = xrudp__session_add(s->Now, interval); }
	if (ref->Stream != XRUDP__NO_STREAM) {
		m->Deficit = deficit > size ? deficit - (uint32_t)size : 0;
		s->ScheduleCursor = m->Deficit ? selected : (selected + 1) % s->StreamSlots;
		xrudp__stream_note(m);
	}
	if (ackSize) { s->AckWanted = false; s->AckAt = UINT64_MAX; s->ElicitingSinceAck = 0; }
	if (probe) --s->Probes;
	if (!eliciting) s->ControlAt = xrudp__session_add(s->Now, (1000000u + s->Config.ControlPerSecond - 1) / s->Config.ControlPerSecond);
	if (eliciting) s->ConfirmWanted = false;
	++s->SentPackets; xrudp__session_pto_sync(s, false); return XRUDP_OK;
}

static xrudpresult xrudp__session_send(xrudpsession* s, uint64_t* due)
{
	unsigned char plain[XRUDP_PACKET_MAX], body[XRUDP_PACKET_MAX], packet[XRUDP_PACKET_MAX];
	xrudpresult result = xrudp__session_send_inner(s, due, plain, body, packet);
	xrtSecureZero(plain, sizeof(plain)); xrtSecureZero(body, sizeof(body)); xrtSecureZero(packet, sizeof(packet));
	return result;
}

xrudpresult xrtRudpSessionPoll(xrudpsession* s, uint64_t now, uint64_t* output)
{
	if (s == NULL || s->Busy || output == NULL || now < s->Now) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	xrudp__session_clock(s, now);
	if (xrudp__session_dead(s, now)) { (void)xrudp__session_abort(s, XRUDP_TIMEOUT); return XRUDP_TIMEOUT; }
	s->Busy = true; xrudpresult result = xrudp__session_losses(s, 0);
	if (result != XRUDP_OK) goto failure;
	xrudp__session_pto_sync(s, false); xrudp__session_syncwork(s);
	if (s->PtoAt && now >= s->PtoAt) {
		if (s->PtoCount < 32) ++s->PtoCount;
		s->Probes = 2; ++s->ProbeTimeouts; uint64_t interval;
		(void)xrtRudpRttPto(&s->Rtt, s->PtoCount, &interval); s->PtoAt = xrudp__session_add(now, interval);
	}
	if (s->PreviousKeyUntil && now >= s->PreviousKeyUntil) { xrtRudpCipherRetirePrevious(s->Cipher); s->PreviousKeyUntil = 0; }
	uint64_t due = xrudp__session_add(s->Activity, s->IdleUs);
	if (s->Work && xrudp__session_add(s->Progress, s->Config.ProgressTimeoutUs) < due) due = xrudp__session_add(s->Progress, s->Config.ProgressTimeoutUs);
	for (size_t i = 0; i < XRUDP_SESSION_POLL_BURST; ++i) {
		result = xrudp__session_send(s, &due);
		if (result == XRUDP_DONE) break;
		if (result != XRUDP_OK) goto failure;
		if (i + 1 == XRUDP_SESSION_POLL_BURST && xrudp__session_add(now, 1000) < due) due = xrudp__session_add(now, 1000);
	}
	if (s->PtoAt && s->PtoAt < due) due = s->PtoAt;
	if (s->PreviousKeyUntil && s->PreviousKeyUntil < due) due = s->PreviousKeyUntil;
	if (s->AckWanted && xrtRudpSentCanRecord(s->Sent, false)) {
		uint64_t at = s->AckAt > s->ControlAt ? s->AckAt : s->ControlAt; if (at < due) due = at;
	}
	if (s->ConnectionLimit > s->ConnectionAcked && s->GrantRetry > now && s->GrantRetry < due) due = s->GrantRetry;
	if (s->IncomingLimit > s->StreamsAcked && s->StreamsRetry > now && s->StreamsRetry < due) due = s->StreamsRetry;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Id) {
		xrudp__sessionstream* m = &s->Streams[i]; xrudpreceivestate rx; (void)xrtRudpReceiveState(m->Rx, &rx);
		if ((!m->OpenAcked || (m->ResetQueued && !m->ResetAcked) || rx.Limit > m->GrantAcked) && m->RetryAt > now && m->RetryAt < due) due = m->RetryAt;
	}
	uint64_t delay, age; xrudpsentstate sent;
	(void)xrtRudpRttLossDelay(&s->Rtt, &delay); (void)xrtRudpRttPto(&s->Rtt, 0, &age);
	age = xrudp__session_add(age, xrudp__session_add(age, age)); if (age < 1000000) age = 1000000;
	(void)xrtRudpSentState(s->Sent, &sent);
	for (size_t i = 0; i < s->Slots; ++i) if (s->Ref[i].Used) {
		xrudp__sessionref* ref = &s->Ref[i]; uint64_t at = UINT64_MAX;
		if (!(ref->Flags & XRUDP_SENT_ACK_ELICITING)) at = xrudp__session_add(ref->At, age);
		else if (ref->Number < sent.LargestAcked) at = xrudp__session_add(ref->At, delay);
		if (at < due) due = at;
	}
	/* Saturating time does not grant an infinite lease. Caller can terminate at
	 * UINT64_MAX; ordinary deadlines always advance by at least 1ms. */
	if (due <= now) due = xrudp__session_add(now, 1000);
	s->Busy = false; *output = due; return XRUDP_OK;
failure:
	s->Busy = false; (void)xrudp__session_abort(s, xrudp__session_terminal(result) ? result : XRUDP_IO_ERROR); return s->Reason;
}
#endif


/* ========================================================================== */
/* source: extlibs/xrudp/src/network/rudp_endpoint.c */
/* ========================================================================== */

#if defined(XRUDP_FEATURE_RUDP_ENDPOINT)
#include <string.h>

struct xrudppeer {
	xatomic32 References, State;
	xrudpendpoint* Endpoint;
	xnetaddr Address;
	xrudphandshake* Handshake;
	xrudpcipher* Cipher; /* Owned until Session admission; then borrowed for final abort CLOSE only. */
	xrudpsession* Session;
	xrudpsessionstate Snapshot;
	xrudphandshakeinfo Info;
	uint64_t NextPoll, SessionPoll, NotifyAt;
	size_t Slot, RuntimeBytes, PendingSize;
	uint32_t PendingEvents;
	bool FinalQueued, Notifying, HadSession;
	unsigned char Pending[XRUDP_HANDSHAKE_PACKET_MAX];
};

typedef struct xrudp__addressrate {
	xrudpaddress Address;
	uint64_t Second;
	uint32_t Count;
	bool Used;
} xrudp__addressrate;

struct xrudpendpoint {
	xatomic32 References, State;
	xatomic64 Reserved;
	xnetworker* Worker;
	xnetengine* Engine;
	xnetudp* Udp; /* Stable owned handle; CLOSED UDP no longer holds Engine. */
	xrudpendpointconfig Config;
	xrudpendpointevents Events;
	void* Context;
	size_t Allocation, PeerRuntime;
	uint64_t Timer, TimerDue, LastNow, RateSecond;
	double Epoch;
	uint32_t RateCount, Peers, HalfOpen;
	bool UdpClosed, CloseNotified;
	xrudpresult CloseReason;
	xerror* LocalError;
	unsigned char Private[32], Public[32], Scope[16], Pin[32], Application[32], CookieKey[32];
	xrudpendpointinfo Stats;
	xrudp__addressrate Rates[XRUDP_ENDPOINT_ADDRESS_BUCKETS];
	xrudppeer* Slots[];
};

static void xrudp__endpoint_schedule(xrudpendpoint* e);
static void xrudp__endpoint_finish(xrudpendpoint* e, const xerror* error);

static bool xrudp__endpoint_nonzero(const void* data, size_t size)
{
	const unsigned char* p = (const unsigned char*)data; unsigned char bits = 0;
	if (p == NULL) return false;
	for (size_t i = 0; i < size; ++i) bits |= p[i];
	return bits != 0;
}
static bool xrudp__endpoint_owner(const xrudpendpoint* e)
{
	return e != NULL && xrtRudpEndpointState(e) != XRUDP_ENDPOINT_CLOSED && xrtNetWorkerIsCurrent(e->Worker);
}
static uint64_t xrudp__endpoint_now(xrudpendpoint* e)
{
	double elapsed = xrtTimer() - e->Epoch;
	uint64_t now = elapsed >= 0.0 && elapsed < (double)UINT64_MAX / 1000000.0 ? (uint64_t)(elapsed * 1000000.0) : e->LastNow;
	if (now > e->LastNow) e->LastNow = now;
	return e->LastNow;
}
static bool xrudp__endpoint_address(const xnetaddr* source, xrudpaddress* output)
{
	if (source == NULL || source->Port == 0 || (source->Family != XNET_FAMILY_IPV4 && source->Family != XNET_FAMILY_IPV6) ||
		xrtNetAddrIsUnspecified(source) || xrtNetAddrIsMulticast(source) || xrtNetAddrIsMapped(source)) return false;
	if (source->Family == XNET_FAMILY_IPV4 && (source->Scope != 0 || source->Address[0] == 0 || source->Address[0] >= 224 ||
		xrudp__endpoint_nonzero(source->Address + 4, 12))) return false;
	if (source->Family == XNET_FAMILY_IPV6 && xrtNetAddrIsLinkLocal(source) && source->Scope == 0) return false;
	memset(output, 0, sizeof(*output)); output->Family = (uint8_t)source->Family;
	output->Port = source->Port; output->Zone = source->Scope; memcpy(output->Address, source->Address, 16);
	return true;
}
xrudpendpoint* xrtRudpEndpointRef(xrudpendpoint* e)
{
	if (e != NULL) (void)xrtAtomic32FetchAdd(&e->References, 1, XMEMORY_RELAXED);
	return e;
}
void xrtRudpEndpointDestroy(xrudpendpoint* e)
{
	if (e != NULL && xrtAtomic32FetchSub(&e->References, 1, XMEMORY_ACQ_REL) == 1) {
		xrtNetUdpDestroy(e->Udp); xrtErrorFree(e->LocalError);
		size_t size = e->Allocation; xrtSecureZero(e, size); xrtFree(e);
	}
}
xrudppeer* xrtRudpPeerRef(xrudppeer* p)
{
	if (p != NULL) (void)xrtAtomic32FetchAdd(&p->References, 1, XMEMORY_RELAXED);
	return p;
}
void xrtRudpPeerDestroy(xrudppeer* p)
{
	if (p != NULL && xrtAtomic32FetchSub(&p->References, 1, XMEMORY_ACQ_REL) == 1) {
		xrudpendpoint* e = p->Endpoint;
		(void)xrtAtomic64FetchSub(&e->Reserved, sizeof(*p), XMEMORY_ACQ_REL);
		xrtSecureZero(p, sizeof(*p)); xrtFree(p); xrtRudpEndpointDestroy(e);
	}
}
xrudpendpointstate xrtRudpEndpointState(const xrudpendpoint* e)
{
	return e == NULL ? XRUDP_ENDPOINT_CLOSED : (xrudpendpointstate)xrtAtomic32Load(&e->State, XMEMORY_ACQUIRE);
}
xrudppeerstate xrtRudpPeerState(const xrudppeer* p)
{
	return p == NULL ? XRUDP_PEER_CLOSED : (xrudppeerstate)xrtAtomic32Load(&p->State, XMEMORY_ACQUIRE);
}
static void xrudp__peer_close(xrudppeer* p, xrudpresult reason)
{
	xrudpendpoint* e = p->Endpoint; xrudppeerstate previous = xrtRudpPeerState(p);
	if (previous == XRUDP_PEER_CLOSED) return;
	e->Slots[p->Slot] = NULL; --e->Peers; if (previous == XRUDP_PEER_NEGOTIATING) --e->HalfOpen;
	xrtRudpHandshakeDestroy(p->Handshake); p->Handshake = NULL;
	if (p->Session != NULL) {
		(void)xrtRudpSessionAbort(p->Session, reason); (void)xrtRudpSessionState(p->Session, &p->Snapshot);
		xrtRudpSessionDestroy(p->Session); p->Session = NULL;
	} else xrtRudpCipherDestroy(p->Cipher);
	p->Cipher = NULL;
	(void)xrtAtomic64FetchSub(&e->Reserved, p->RuntimeBytes, XMEMORY_ACQ_REL); p->RuntimeBytes = 0;
	p->PendingSize = 0; p->PendingEvents = 0;
	/* Publish the closed snapshot only after all mutable session work. */
	xrtAtomic32Store(&p->State, XRUDP_PEER_CLOSED, XMEMORY_RELEASE);
	if (e->Events.PeerClose != NULL) e->Events.PeerClose(e, p, reason, e->Context);
	xrtRudpPeerDestroy(p); /* Slot ownership; caller must hold its own temporary reference. */
}
static xrudppeer* xrudp__endpoint_find(xrudpendpoint* e, const unsigned char id[16])
{
	for (size_t i = 0; i < e->Config.MaxPeers; ++i)
		if (e->Slots[i] != NULL && memcmp(e->Slots[i]->Info.LocalId, id, 16) == 0) return e->Slots[i];
	return NULL;
}
static bool xrudp__endpoint_reserve(xrudpendpoint* e, size_t* slot)
{
	size_t cost = sizeof(xrudppeer) + e->PeerRuntime;
	if (e->Peers >= e->Config.MaxPeers || e->HalfOpen >= e->Config.MaxHalfOpen ||
		cost > e->Config.MemoryLimit - (size_t)xrtAtomic64Load(&e->Reserved, XMEMORY_ACQUIRE)) return false;
	for (*slot = 0; *slot < e->Config.MaxPeers && e->Slots[*slot] != NULL; ++*slot) {}
	if (*slot == e->Config.MaxPeers) return false;
	(void)xrtAtomic64FetchAdd(&e->Reserved, cost, XMEMORY_ACQ_REL); return true;
}
static void xrudp__endpoint_unreserve(xrudpendpoint* e)
{
	(void)xrtAtomic64FetchSub(&e->Reserved, sizeof(xrudppeer) + e->PeerRuntime, XMEMORY_ACQ_REL);
}
static xrudppeer* xrudp__endpoint_peer(xrudpendpoint* e, size_t slot, const xnetaddr* address, xrudphandshake* handshake)
{
	xrudppeer* p = (xrudppeer*)xrtMalloc(sizeof(*p));
	if (p == NULL) return NULL;
	memset(p, 0, sizeof(*p)); xrtAtomic32Init(&p->References, 1); xrtAtomic32Init(&p->State, XRUDP_PEER_NEGOTIATING);
	p->Endpoint = xrtRudpEndpointRef(e); p->Address = *address; p->Handshake = handshake; p->Slot = slot;
	p->RuntimeBytes = e->PeerRuntime; p->SessionPoll = UINT64_MAX; (void)xrtRudpHandshakeInfo(handshake, &p->Info);
	p->NextPoll = e->LastNow; e->Slots[slot] = p; ++e->Peers; ++e->HalfOpen; return p;
}
static bool xrudp__endpoint_rate(xrudpendpoint* e, const xrudpaddress* address, uint64_t now)
{
	uint64_t second = now / 1000000;
	if (e->RateSecond != second) { e->RateSecond = second; e->RateCount = 0; }
	if (e->RateCount >= e->Config.HandshakesPerSecond) return false;
	size_t slot = XRUDP_ENDPOINT_ADDRESS_BUCKETS, reuse = XRUDP_ENDPOINT_ADDRESS_BUCKETS;
	for (size_t i = 0; i < XRUDP_ENDPOINT_ADDRESS_BUCKETS; ++i) {
		xrudp__addressrate* r = &e->Rates[i];
		if (r->Used && r->Address.Family == address->Family && r->Address.Zone == address->Zone &&
			memcmp(r->Address.Address, address->Address, 16) == 0) { slot = i; break; }
		if (!r->Used || r->Second < second) reuse = i;
	}
	if (slot == XRUDP_ENDPOINT_ADDRESS_BUCKETS) {
		if (reuse == XRUDP_ENDPOINT_ADDRESS_BUCKETS) return false;
		slot = reuse; memset(&e->Rates[slot], 0, sizeof(e->Rates[slot])); e->Rates[slot].Used = true;
		e->Rates[slot].Address = *address;
	}
	xrudp__addressrate* r = &e->Rates[slot];
	if (r->Second != second) { r->Second = second; r->Count = 0; }
	if (r->Count >= e->Config.AddressHandshakesPerSecond) return false;
	++r->Count; ++e->RateCount; return true;
}
static xnetresult xrudp__endpoint_send(xrudpendpoint* e, const xnetaddr* address, const void* data, size_t size)
{
	xnetresult result = xrtNetUdpSendTo(e->Udp, address, data, size);
	if (result == XNET_RESULT_OK) ++e->Stats.Sent;
	else if (result != XNET_RESULT_AGAIN && xrtRudpEndpointState(e) == XRUDP_ENDPOINT_OPEN) {
		if (xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY) ++e->Stats.CapacityDropped;
		else ++e->Stats.IoErrors;
	}
	return result;
}
static xrudpresult xrudp__peer_send(const void* data, size_t size, void* context)
{
	xrudppeer* p = (xrudppeer*)context; xrudpendpoint* e = p->Endpoint;
	if (xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return XRUDP_BACKPRESSURE;
	xnetresult r = xrudp__endpoint_send(e, &p->Address, data, size);
	if (r == XNET_RESULT_OK) return XRUDP_OK;
	if (r == XNET_RESULT_AGAIN || xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return XRUDP_BACKPRESSURE;
	return xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XRUDP_NO_MEMORY : XRUDP_IO_ERROR;
}
static bool xrudp__peer_eof(const xrudpsessionstate* s)
{
	return s->Receive.HasFinal && !s->Receive.Reset && s->Receive.ReadOffset == s->Receive.FinalOffset;
}
static bool xrudp__peer_write_done(const xrudpsessionstate* s)
{
	return s->Transmit.FinAcknowledged && s->Transmit.PendingBytes == 0;
}
static void xrudp__peer_capture(xrudppeer* p)
{
	xrudpsessionstate next;
	if (p->Session == NULL || xrtRudpSessionState(p->Session, &next) != XRUDP_OK) return;
	if (next.ReadReady && !p->Snapshot.ReadReady) p->PendingEvents |= XRUDP_PEER_READ_READY;
	if (next.Transmit.BaseOffset > p->Snapshot.Transmit.BaseOffset && !next.Transmit.FinQueued) p->PendingEvents |= XRUDP_PEER_WRITE_READY;
	if (xrudp__peer_write_done(&next) && !xrudp__peer_write_done(&p->Snapshot)) p->PendingEvents |= XRUDP_PEER_WRITE_DONE;
	if (xrudp__peer_eof(&next) && !xrudp__peer_eof(&p->Snapshot)) p->PendingEvents |= XRUDP_PEER_READ_EOF;
	if (next.OutgoingLimit > p->Snapshot.OutgoingLimit) p->PendingEvents |= XRUDP_PEER_STREAMS_READY;
	p->Snapshot = next;
}
static void xrudp__peer_notify(xrudppeer* p)
{
	xrudpendpoint* e = p->Endpoint;
	if (p->Notifying || (p->PendingEvents == 0 && p->Snapshot.PendingStreamEvents == 0) ||
		xrudp__endpoint_now(e) < p->NotifyAt || xrtRudpPeerState(p) != XRUDP_PEER_CONFIRMED ||
		xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
	uint32_t events = p->PendingEvents; p->PendingEvents = 0;
	xrudpstreamevent streams[XRUDP_PEER_STREAM_EVENT_BURST]; size_t count = 0;
	/* Snapshot before entering application code. Newly generated hints cannot
	 * recursively enter this batch or make its work unbounded. */
	while (count < XRUDP_PEER_STREAM_EVENT_BURST &&
		xrtRudpSessionStreamNextEvent(p->Session, &streams[count]) == XRUDP_OK) ++count;
	xrudp__peer_capture(p); p->Notifying = true;
	uint64_t now = xrudp__endpoint_now(e); p->NotifyAt = now > UINT64_MAX - 1000 ? UINT64_MAX : now + 1000;
	if (events != 0 && e->Events.Changed != NULL) e->Events.Changed(e, p, events, e->Context);
	for (size_t i = 0; i < count && xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED &&
		xrtRudpEndpointState(e) == XRUDP_ENDPOINT_OPEN; ++i) {
		xrudpstreamstate state;
		if (e->Events.StreamChanged != NULL && xrtRudpSessionStreamState(p->Session, streams[i].Id, &state) == XRUDP_OK)
			e->Events.StreamChanged(e, p, &streams[i], e->Context);
	}
	p->Notifying = false;
	now = xrudp__endpoint_now(e); p->NotifyAt = now > UINT64_MAX - 1000 ? UINT64_MAX : now + 1000;
}
static void xrudp__peer_pump(xrudppeer* p, uint64_t now)
{
	xrudpendpoint* e = p->Endpoint;
	if (xrtRudpPeerState(p) == XRUDP_PEER_CLOSED || xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
	if (p->Handshake != NULL) {
		/* A retained packet waiting for UDP capacity must not bypass the
		 * absolute handshake deadline by skipping HandshakeNext forever. */
		if (xrtRudpPeerState(p) == XRUDP_PEER_NEGOTIATING && now >= p->Info.DeadlineUs) {
			xrudp__peer_close(p, XRUDP_TIMEOUT); return;
		}
		if (xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED && now >= p->Info.DeadlineUs) {
			xrtRudpHandshakeDestroy(p->Handshake); p->Handshake = NULL; p->NextPoll = UINT64_MAX;
		} else {
			if (p->PendingSize == 0 && now >= p->NextPoll) {
				uint64_t due = p->Info.DeadlineUs;
				xrudpresult result = xrtRudpHandshakeNext(p->Handshake, now, p->Pending, sizeof(p->Pending), &p->PendingSize, &due);
				p->NextPoll = due; (void)xrtRudpHandshakeInfo(p->Handshake, &p->Info);
				if (p->Info.Phase == XRUDP_HANDSHAKE_FAILED || (result != XRUDP_OK && result != XRUDP_TIMEOUT && result != XRUDP_DONE)) {
					xrudp__peer_close(p, p->Info.Phase == XRUDP_HANDSHAKE_FAILED ? p->Info.Failure : result); return;
				}
			}
			/* Allocate the full session before admitting the final server proof
			 * to UDP; Confirmed never exposes a peer lacking receive resources. */
			if (p->Info.Phase == XRUDP_HANDSHAKE_ESTABLISHED && p->Session == NULL) {
				if (xrtRudpHandshakeTakeCipher(p->Handshake, &p->Cipher) != XRUDP_OK) { xrudp__peer_close(p, XRUDP_IO_ERROR); return; }
				(void)xrtRudpHandshakeInfo(p->Handshake, &p->Info);
				xrudpsessionconfig config = e->Config.Session; config.Peer = p->Info.PeerParameters; config.Role = p->Info.Role;
				xrudpresult result = xrtRudpSessionCreate(&config, p->Cipher, now, xrudp__peer_send, p, &p->Session);
				if (result != XRUDP_OK) { if (result == XRUDP_NO_MEMORY) ++e->Stats.CapacityDropped; xrudp__peer_close(p, result); return; }
				p->HadSession = true; p->SessionPoll = now; (void)xrtRudpSessionState(p->Session, &p->Snapshot);
			}
			if (p->PendingSize != 0) {
				xnetresult sent = xrudp__endpoint_send(e, &p->Address, p->Pending, p->PendingSize);
				if (sent == XNET_RESULT_OK) {
					if (p->Info.Phase == XRUDP_HANDSHAKE_ESTABLISHED && p->Info.Role == XRUDP_RESPONDER) p->FinalQueued = true;
					p->PendingSize = 0;
				} else if (sent != XNET_RESULT_AGAIN) {
					xrudpresult reason = xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN ? XRUDP_CANCELLED :
						(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XRUDP_NO_MEMORY : XRUDP_IO_ERROR);
					xrudp__peer_close(p, reason); return;
				}
			}
			if (xrtRudpPeerState(p) == XRUDP_PEER_NEGOTIATING && p->Info.Phase == XRUDP_HANDSHAKE_ESTABLISHED &&
				(p->Info.Role == XRUDP_INITIATOR || p->FinalQueued)) {
				--e->HalfOpen;
				xrtAtomic32Store(&p->State, XRUDP_PEER_CONFIRMED, XMEMORY_RELEASE);
				if (e->Events.Confirmed != NULL) e->Events.Confirmed(e, p, e->Context);
				if (xrtRudpPeerState(p) == XRUDP_PEER_CLOSED || xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
			}
		}
	}
	if (xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED) {
		if (now >= p->SessionPoll) {
			xrudpresult result = xrtRudpSessionPoll(p->Session, now, &p->SessionPoll);
			if (result != XRUDP_OK) { xrudp__peer_close(p, result); return; }
			xrudp__peer_capture(p);
		}
		xrudp__peer_notify(p);
	}
}
static void xrudp__endpoint_timer(xnetworker* worker, uint64 id, xnetresult result, ptr context)
{
	xrudpendpoint* e = (xrudpendpoint*)context; (void)worker;
	if (e->Timer == id) { e->Timer = 0; e->TimerDue = 0; }
	if (result == XNET_RESULT_OK && xrtRudpEndpointState(e) == XRUDP_ENDPOINT_OPEN) {
		uint64_t now = xrudp__endpoint_now(e);
		for (size_t i = 0; i < e->Config.MaxPeers; ++i) if (e->Slots[i] != NULL) {
			xrudppeer* p = xrtRudpPeerRef(e->Slots[i]); xrudp__peer_pump(p, now); xrtRudpPeerDestroy(p);
		}
		xrudp__endpoint_schedule(e);
	}
	xrudp__endpoint_finish(e, NULL); xrtRudpEndpointDestroy(e);
}
static void xrudp__endpoint_schedule(xrudpendpoint* e)
{
	if (e->Peers == 0 || xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
	uint64_t now = xrudp__endpoint_now(e), due = UINT64_MAX;
	for (size_t i = 0; i < e->Config.MaxPeers; ++i) if (e->Slots[i] != NULL) {
		xrudppeer* p = e->Slots[i]; uint64_t next = p->PendingSize ? now : p->NextPoll;
		if (xrtRudpPeerState(p) == XRUDP_PEER_NEGOTIATING && p->Info.DeadlineUs < next) next = p->Info.DeadlineUs;
		if (xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED && p->SessionPoll < next) next = p->SessionPoll;
		if (p->PendingEvents != 0 || p->Snapshot.PendingStreamEvents != 0) {
			uint64_t notify = p->NotifyAt > now ? p->NotifyAt : now; if (notify < next) next = notify;
		}
		if (next < due) due = next;
	}
	uint64_t milliseconds = due > now ? (due - now) / 1000 + ((due - now) % 1000 != 0) : 1;
	if (milliseconds == 0) milliseconds = 1;
	if (milliseconds > 1000) milliseconds = 1000;
	due = now > UINT64_MAX - milliseconds * 1000 ? UINT64_MAX : now + milliseconds * 1000;
	if (e->Timer != 0) {
		if (due >= e->TimerDue) return;
		if (!xrtNetEngineTimerCancelCurrent(e->Engine, e->Timer)) return; /* Already queued: it will run next. */
		if (xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
	}
	xrtRudpEndpointRef(e);
	e->TimerDue = due;
	e->Timer = xrtNetEngineAfter(e->Engine, xrtNetWorkerIndex(e->Worker), (int64)milliseconds, xrudp__endpoint_timer, e);
	if (e->Timer == 0) {
		e->LocalError = xrtErrorRef(xrtGetError()); ++e->Stats.IoErrors;
		xrtRudpEndpointDestroy(e); (void)xrtRudpEndpointClose(e);
	}
}
static void xrudp__endpoint_receive(xnetudp* udp, const xnetudpmessage* message, ptr context)
{
	xrudpendpoint* e = (xrudpendpoint*)context; (void)udp;
	if (xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
	++e->Stats.Received; xrudpaddress address; xrudpwirepacket packet;
	if ((message->Flags & XNET_UDP_MESSAGE_TRUNCATED) != 0 || !xrudp__endpoint_address(&message->Remote, &address) ||
		xrtRudpPacketDecode(message->Data, message->Size, &packet) != XRUDP_OK) { ++e->Stats.Invalid; return; }
	uint64_t now = xrudp__endpoint_now(e); xrudppeer* p = xrudp__endpoint_find(e, packet.Header.ConnectionId);
	if (p != NULL) {
		if (!xrtNetAddrEqual(&p->Address, &message->Remote)) { ++e->Stats.AddressDropped; return; }
		xrtRudpPeerRef(p);
		if (xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED && packet.Header.Type == XRUDP_PACKET_DATA && packet.Header.PacketNumber >= 2) {
			xrudpresult result = xrtRudpSessionInput(p->Session, message->Data, message->Size, now);
			if (result == XRUDP_AUTH_FAILED) ++e->Stats.Invalid;
			else if (result != XRUDP_OK && result != XRUDP_DUPLICATE && result != XRUDP_TOO_OLD && result != XRUDP_BACKPRESSURE) xrudp__peer_close(p, result);
			if (xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED) {
				xrudp__peer_capture(p); p->SessionPoll = now; xrudp__peer_pump(p, now);
			}
		} else if (p->Handshake != NULL) {
			xrudpresult result = xrtRudpHandshakeInput(p->Handshake, message->Data, message->Size, now);
			(void)xrtRudpHandshakeInfo(p->Handshake, &p->Info);
			if (p->Info.Phase == XRUDP_HANDSHAKE_FAILED) xrudp__peer_close(p, p->Info.Failure);
			else if (result == XRUDP_NO_MEMORY || result == XRUDP_IO_ERROR) xrudp__peer_close(p, result);
			else {
				if (result == XRUDP_OK) { p->PendingSize = 0; p->NextPoll = now; }
				if (result == XRUDP_AUTH_FAILED) ++e->Stats.Invalid;
				xrudp__peer_pump(p, now);
			}
		}
		xrtRudpPeerDestroy(p); xrudp__endpoint_schedule(e); return;
	}
	if (!e->Config.Listen) { ++e->Stats.Invalid; return; }
	if (!xrudp__endpoint_rate(e, &address, now)) { ++e->Stats.RateDropped; return; }
	if (packet.Header.Type == XRUDP_PACKET_HELLO) {
		unsigned char response[XRUDP_HANDSHAKE_PACKET_MAX]; size_t size;
		xrudpresult result = xrtRudpHandshakeRetry(e->CookieKey, e->Scope, e->Public, &address, now,
			message->Data, message->Size, response, sizeof(response), &size);
		if (result == XRUDP_OK) (void)xrudp__endpoint_send(e, &message->Remote, response, size);
		else ++e->Stats.Invalid;
		return;
	}
	if (packet.Header.Type != XRUDP_PACKET_HANDSHAKE) { ++e->Stats.Invalid; return; }
	size_t slot;
	if (!xrudp__endpoint_reserve(e, &slot)) { ++e->Stats.CapacityDropped; return; }
	xrudphandshakeconfig c = {0}; c.StaticPrivate = e->Private; c.LocalPublic = e->Public; c.Scope = e->Scope;
	c.PeerPublic = e->Config.PeerPublic; c.ApplicationContext = e->Config.ApplicationContext;
	c.Trust = e->Config.Trust; c.TrustContext = e->Config.TrustContext; c.Parameters = e->Config.Session.Local; c.TimeoutUs = e->Config.HandshakeTimeoutUs;
	xrudphandshake* h = NULL;
	xrudpresult result = xrtRudpHandshakeAccept(&c, e->CookieKey, NULL, &address, now, message->Data, message->Size, &h);
	if (result != XRUDP_OK) {
		xrudp__endpoint_unreserve(e);
		if (result == XRUDP_NO_MEMORY) ++e->Stats.CapacityDropped;
		else if (result == XRUDP_IO_ERROR) ++e->Stats.IoErrors;
		else ++e->Stats.Invalid;
		return;
	}
	p = xrudp__endpoint_peer(e, slot, &message->Remote, h);
	if (p == NULL) { xrtRudpHandshakeDestroy(h); xrudp__endpoint_unreserve(e); ++e->Stats.CapacityDropped; return; }
	xrtRudpPeerRef(p); xrudp__peer_pump(p, now); xrtRudpPeerDestroy(p); xrudp__endpoint_schedule(e);
}
static void xrudp__endpoint_open(xnetudp* udp, ptr context)
{
	xrudpendpoint* e = (xrudpendpoint*)context; (void)udp;
	uint32 expected = XRUDP_ENDPOINT_OPENING;
	if (!xrtAtomic32CompareExchange(&e->State, &expected, XRUDP_ENDPOINT_OPEN, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {
		(void)xrtNetUdpAbort(e->Udp); return;
	}
	if (e->Events.Open != NULL) e->Events.Open(e, e->Context);
}
static void xrudp__endpoint_error(xnetudp* udp, const xerror* error, ptr context)
{
	xrudpendpoint* e = (xrudpendpoint*)context; (void)udp; (void)error; ++e->Stats.IoErrors;
}
static void xrudp__endpoint_finish(xrudpendpoint* e, const xerror* error)
{
	if (!e->UdpClosed || e->Timer != 0 || e->CloseNotified) return;
	e->CloseNotified = true; xrtSecureZero(e->Private, 32); xrtSecureZero(e->CookieKey, 32);
	xrtAtomic32Store(&e->State, XRUDP_ENDPOINT_CLOSED, XMEMORY_RELEASE);
	if (error == NULL) error = e->LocalError != NULL ? e->LocalError : xrtNetUdpError(e->Udp);
	if (e->Events.Close != NULL) e->Events.Close(e, e->CloseReason, error, e->Context);
	xrtRudpEndpointDestroy(e); /* Internal I/O lifetime. */
}
static void xrudp__endpoint_closed(xnetudp* udp, xnetresult result, const xerror* error, ptr context)
{
	xrudpendpoint* e = xrtRudpEndpointRef((xrudpendpoint*)context); (void)udp;
	xrudpendpointstate previous = xrtRudpEndpointState(e);
	xrtAtomic32Store(&e->State, XRUDP_ENDPOINT_CLOSING, XMEMORY_RELEASE);
	e->CloseReason = e->LocalError != NULL ? (xrtErrorKind(e->LocalError) == XERR_MEMORY ? XRUDP_NO_MEMORY : XRUDP_IO_ERROR) :
		(previous == XRUDP_ENDPOINT_CLOSING ? XRUDP_CANCELLED : (result == XNET_RESULT_ERROR ? XRUDP_IO_ERROR : XRUDP_CLOSED));
	for (size_t i = 0; i < e->Config.MaxPeers; ++i) if (e->Slots[i] != NULL) {
		xrudppeer* p = xrtRudpPeerRef(e->Slots[i]); xrudp__peer_close(p, e->CloseReason); xrtRudpPeerDestroy(p);
	}
	e->UdpClosed = true;
	if (e->Timer != 0) (void)xrtNetEngineTimerCancelCurrent(e->Engine, e->Timer);
	xrudp__endpoint_finish(e, error); xrtRudpEndpointDestroy(e);
}
void xrtRudpEndpointConfigInit(xrudpendpointconfig* c)
{
	if (c == NULL) return;
	memset(c, 0, sizeof(*c)); xrtRudpSessionConfigInit(&c->Session);
	c->MaxPeers = 64; c->MaxHalfOpen = 16; c->MemoryLimit = 16u * 1024u * 1024u;
	c->HandshakesPerSecond = 128; c->AddressHandshakesPerSecond = 8;
}
xrudpresult xrtRudpEndpointBind(xnetworker* worker, const xnetaddr* local,
	const xrudpendpointconfig* config, const xrudpendpointevents* events, void* context, xrudpendpoint** output)
{
	size_t sessionBytes; xrudpsessionconfig session;
	if (config == NULL) return XRUDP_ARGUMENT;
	session = config->Session; session.Peer = session.Local;
	if (worker == NULL || !xrtNetWorkerIsCurrent(worker) || local == NULL || config == NULL || output == NULL ||
		(local->Family != XNET_FAMILY_IPV4 && local->Family != XNET_FAMILY_IPV6) ||
		!xrudp__endpoint_nonzero(config->StaticPrivate, 32) || (config->Scope != NULL && !xrudp__endpoint_nonzero(config->Scope, 16)) ||
		(config->PeerPublic != NULL && !xrudp__endpoint_nonzero(config->PeerPublic, 32)) ||
		(config->Listen && config->PeerPublic == NULL && config->Trust == NULL) || config->MaxPeers == 0 || config->MaxPeers > XRUDP_ENDPOINT_PEERS_MAX ||
		config->MaxHalfOpen == 0 || config->MaxHalfOpen > config->MaxPeers || config->HandshakesPerSecond == 0 || config->HandshakesPerSecond > 65536 ||
		config->AddressHandshakesPerSecond == 0 || config->AddressHandshakesPerSecond > config->HandshakesPerSecond ||
		config->MemoryLimit > 1024u * 1024u * 1024u || (config->HandshakeTimeoutUs != 0 && (config->HandshakeTimeoutUs < 1000000 || config->HandshakeTimeoutUs > XRUDP_HANDSHAKE_MAX_TIMEOUT_US)) ||
		xrtRudpSessionAllocationSize(&session, &sessionBytes) != XRUDP_OK) return XRUDP_ARGUMENT;
	size_t size = sizeof(xrudpendpoint) + config->MaxPeers * sizeof(xrudppeer*);
	size_t runtime = xrtRudpHandshakeAllocationSize() + sessionBytes - xrtRudpCipherAllocationSize();
	if (config->MemoryLimit < size + sizeof(xrudppeer) + runtime) return XRUDP_ARGUMENT;
	xrudpendpoint* e = (xrudpendpoint*)xrtMalloc(size);
	if (e == NULL) return XRUDP_NO_MEMORY;
	memset(e, 0, size); e->Allocation = size; e->Worker = worker; e->Engine = xrtNetWorkerEngine(worker);
	e->Config = *config; e->Config.Session = session; e->PeerRuntime = runtime;
	e->Context = context; e->Epoch = xrtTimer(); e->RateSecond = UINT64_MAX;
	if (events != NULL) e->Events = *events;
	xrtAtomic32Init(&e->References, 2); xrtAtomic32Init(&e->State, XRUDP_ENDPOINT_OPENING); xrtAtomic64Init(&e->Reserved, size);
	memcpy(e->Private, config->StaticPrivate, 32); e->Config.StaticPrivate = e->Private;
	if (config->Scope != NULL) memcpy(e->Scope, config->Scope, 16);
	else if (!xrtSecureRandom(e->Scope, 16)) goto random_failure;
	if (!xrudp__endpoint_nonzero(e->Scope, 16) || !xrtSecureRandom(e->CookieKey, 32) || !xrudp__endpoint_nonzero(e->CookieKey, 32) ||
		!xrtX25519Public(e->Private, e->Public)) goto random_failure;
	e->Config.Scope = e->Scope;
	if (config->PeerPublic != NULL) { memcpy(e->Pin, config->PeerPublic, 32); e->Config.PeerPublic = e->Pin; }
	if (config->ApplicationContext != NULL) { memcpy(e->Application, config->ApplicationContext, 32); e->Config.ApplicationContext = e->Application; }
	xnetudpconfig udp; xrtNetUdpConfigInit(&udp); udp.ReceiveSize = XRUDP_PACKET_MAX; udp.ReceiveBatch = 8;
	udp.ReceiveQueueLimit = 0; udp.ReceiveQueueByteLimit = 0; udp.Truncation = XNET_UDP_TRUNCATE_DROP;
	udp.SendLimit = 65536; udp.SendPacketLimit = 64; udp.SendHighWater = 49152; udp.SendLowWater = 16384;
	udp.ReceiveBuffer = udp.SendBuffer = 262144; udp.IPv6Only = true;
	xnetudpevents ev; memset(&ev, 0, sizeof(ev)); ev.Open = xrudp__endpoint_open; ev.Receive = xrudp__endpoint_receive;
	ev.Error = xrudp__endpoint_error; ev.Close = xrudp__endpoint_closed;
	e->Udp = xrtNetUdpBind(e->Engine, local, xrtNetWorkerIndex(worker), &udp, &ev, e);
	if (e->Udp == NULL) {
		xrudpresult result = xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XRUDP_NO_MEMORY : XRUDP_IO_ERROR;
		xrtRudpEndpointDestroy(e); xrtRudpEndpointDestroy(e); return result;
	}
	*output = e; return XRUDP_OK;
random_failure:
	xrtRudpEndpointDestroy(e); xrtRudpEndpointDestroy(e); return XRUDP_IO_ERROR;
}
xrudpresult xrtRudpEndpointConnect(xrudpendpoint* e, const xnetaddr* remote,
	const xrudpconnectconfig* config, xrudppeer** output)
{
	xrudpaddress address;
	if (!xrudp__endpoint_owner(e) || config == NULL || output == NULL || !xrudp__endpoint_address(remote, &address) ||
		!xrudp__endpoint_nonzero(config->PeerPublic, 32) || !xrudp__endpoint_nonzero(config->Scope, 16)) return XRUDP_ARGUMENT;
	if (xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return XRUDP_CLOSED;
	xnetaddr local; if (!xrtNetUdpLocal(e->Udp, &local) || remote->Family != local.Family) return XRUDP_ARGUMENT;
	size_t slot; if (!xrudp__endpoint_reserve(e, &slot)) return XRUDP_BACKPRESSURE;
	xrudphandshakeconfig c = {0}; c.StaticPrivate = e->Private; c.PeerPublic = config->PeerPublic; c.Scope = config->Scope;
	c.ApplicationContext = config->ApplicationContext; c.Parameters = e->Config.Session.Local; c.TimeoutUs = e->Config.HandshakeTimeoutUs;
	xrudphandshake* h = NULL;
	xrudpresult result = xrtRudpHandshakeCreate(&c, xrudp__endpoint_now(e), &h);
	if (result != XRUDP_OK) { xrudp__endpoint_unreserve(e); return result; }
	xrudphandshakeinfo info; (void)xrtRudpHandshakeInfo(h, &info);
	if (xrudp__endpoint_find(e, info.LocalId) != NULL) { xrtRudpHandshakeDestroy(h); xrudp__endpoint_unreserve(e); return XRUDP_IO_ERROR; }
	xrudppeer* p = xrudp__endpoint_peer(e, slot, remote, h);
	if (p == NULL) { xrtRudpHandshakeDestroy(h); xrudp__endpoint_unreserve(e); return XRUDP_NO_MEMORY; }
	*output = xrtRudpPeerRef(p); xrtRudpPeerRef(p); xrudp__peer_pump(p, e->LastNow); xrtRudpPeerDestroy(p);
	xrudp__endpoint_schedule(e); return XRUDP_OK;
}
xrudpresult xrtRudpEndpointInfo(const xrudpendpoint* e, xrudpendpointinfo* output)
{
	if (e == NULL || output == NULL || (xrtRudpEndpointState(e) != XRUDP_ENDPOINT_CLOSED && !xrudp__endpoint_owner(e))) return XRUDP_ARGUMENT;
	xrudpendpointinfo info = e->Stats;
	if (!xrtNetUdpLocal(e->Udp, &info.Local)) return XRUDP_IO_ERROR;
	memcpy(info.Public, e->Public, 32); memcpy(info.Scope, e->Scope, 16); info.Peers = e->Peers; info.HalfOpen = e->HalfOpen;
	info.ReservedBytes = (size_t)xrtAtomic64Load(&e->Reserved, XMEMORY_ACQUIRE); info.MemoryLimit = e->Config.MemoryLimit;
	*output = info; return XRUDP_OK;
}
xrudpresult xrtRudpPeerInfo(const xrudppeer* p, xrudphandshakeinfo* output, xnetaddr* address)
{
	if (p == NULL || output == NULL || address == NULL ||
		(xrtRudpPeerState(p) != XRUDP_PEER_CLOSED && !xrudp__endpoint_owner(p->Endpoint))) return XRUDP_ARGUMENT;
	*output = p->Info; *address = p->Address; return XRUDP_OK;
}
static xrudpresult xrudp__peer_io(const xrudppeer* p)
{
	if (p == NULL) return XRUDP_ARGUMENT;
	if (xrtRudpPeerState(p) == XRUDP_PEER_CLOSED) return XRUDP_CLOSED;
	if (!xrudp__endpoint_owner(p->Endpoint)) return XRUDP_ARGUMENT;
	if (xrtRudpEndpointState(p->Endpoint) != XRUDP_ENDPOINT_OPEN) return XRUDP_CLOSED;
	return xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED ? XRUDP_OK : XRUDP_BACKPRESSURE;
}
static void xrudp__peer_wake(xrudppeer* p)
{
	xrudp__peer_capture(p); p->SessionPoll = xrudp__endpoint_now(p->Endpoint); xrudp__endpoint_schedule(p->Endpoint);
}
xrudpresult xrtRudpPeerWrite(xrudppeer* p, const void* data, size_t size, size_t* written)
{
	return xrtRudpPeerStreamWrite(p, 1, data, size, written);
}
xrudpresult xrtRudpPeerStreamWrite(xrudppeer* p, uint32_t id, const void* data, size_t size, size_t* written)
{
	if (written != NULL) *written = 0;
	if (written == NULL || (data == NULL && size != 0)) return XRUDP_ARGUMENT;
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionStreamWrite(p->Session, id, data, size, written);
	if (*written != 0) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerRead(xrudppeer* p, void* output, size_t capacity, size_t* read)
{
	return xrtRudpPeerStreamRead(p, 1, output, capacity, read);
}
xrudpresult xrtRudpPeerStreamRead(xrudppeer* p, uint32_t id, void* output, size_t capacity, size_t* read)
{
	if (read != NULL) *read = 0;
	if (read == NULL || output == NULL || capacity == 0) return XRUDP_ARGUMENT;
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionStreamRead(p->Session, id, output, capacity, read);
	if (*read != 0) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerFinish(xrudppeer* p)
{
	return xrtRudpPeerStreamFinish(p, 1);
}
xrudpresult xrtRudpPeerStreamFinish(xrudppeer* p, uint32_t id)
{
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionStreamFinish(p->Session, id);
	if (result == XRUDP_OK) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerStreamOpen(xrudppeer* p, const xrudpstreamconfig* config, uint32_t* id)
{
	if (config == NULL || id == NULL) return XRUDP_ARGUMENT;
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionStreamOpen(p->Session, config, id);
	if (result == XRUDP_OK) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerStreamState(const xrudppeer* p, uint32_t id, xrudpstreamstate* output)
{
	if (output == NULL) return XRUDP_ARGUMENT;
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	return xrtRudpSessionStreamState(p->Session, id, output);
}
xrudpresult xrtRudpPeerStreamReset(xrudppeer* p, uint32_t id, uint32_t reason)
{
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionStreamReset(p->Session, id, reason);
	if (result == XRUDP_OK) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerStreamRetire(xrudppeer* p, uint32_t id)
{
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionStreamRetire(p->Session, id);
	if (result == XRUDP_OK) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerSessionState(const xrudppeer* p, xrudpsessionstate* output)
{
	if (p == NULL || output == NULL) return XRUDP_ARGUMENT;
	if (xrtRudpPeerState(p) == XRUDP_PEER_CLOSED) {
		if (!p->HadSession) return XRUDP_CLOSED;
		*output = p->Snapshot; return XRUDP_OK;
	}
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	return xrtRudpSessionState(p->Session, output);
}
xrudpresult xrtRudpPeerAbort(xrudppeer* p)
{
	if (p == NULL) return XRUDP_ARGUMENT;
	if (xrtRudpPeerState(p) == XRUDP_PEER_CLOSED) return XRUDP_DONE;
	if (!xrudp__endpoint_owner(p->Endpoint)) return XRUDP_ARGUMENT;
	xrtRudpPeerRef(p);
	if (p->Cipher != NULL && xrtRudpEndpointState(p->Endpoint) == XRUDP_ENDPOINT_OPEN) {
		unsigned char frames[8] = {XRUDP_FRAME_CLOSE,0,0,4,0,0,0,XRUDP_CANCELLED}, packet[80]; size_t size;
		if (xrtRudpCipherSeal(p->Cipher, frames, sizeof(frames), packet, sizeof(packet), &size) == XRUDP_OK)
			(void)xrudp__endpoint_send(p->Endpoint, &p->Address, packet, size);
	}
	xrudp__peer_close(p, XRUDP_CANCELLED); xrtRudpPeerDestroy(p); return XRUDP_OK;
}
xrudpresult xrtRudpEndpointClose(xrudpendpoint* e)
{
	if (e == NULL) return XRUDP_ARGUMENT;
	xrtRudpEndpointRef(e); uint32 state = xrtAtomic32Load(&e->State, XMEMORY_ACQUIRE);
	while (state < XRUDP_ENDPOINT_CLOSING && !xrtAtomic32CompareExchange(&e->State, &state, XRUDP_ENDPOINT_CLOSING, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {}
	if (state < XRUDP_ENDPOINT_CLOSED) (void)xrtNetUdpAbort(e->Udp);
	xrtRudpEndpointDestroy(e); return XRUDP_OK;
}
#endif

#endif
