#ifndef XRUDP_HANDSHAKE_H
#define XRUDP_HANDSHAKE_H

#include <xrudp/features.h>
#include <xrudp/noise.h>
#include <xrudp/cookie.h>

#if defined(XRUDP_FEATURE_RUDP_HANDSHAKE)

#if !defined(XRUDP_FEATURE_RUDP_COOKIE)
#error "rudp_handshake requires rudp_cookie (XRUDP_FEATURE_RUDP_COOKIE)"
#endif

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
	uint32_t MaxMessageSize; /* Authenticated receive payload bound; 0 disables message streams. */
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
