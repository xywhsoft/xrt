#ifndef XRUDP_NOISE_H
#define XRUDP_NOISE_H

#include <xrudp/features.h>
#include <xrudp/crypto.h>

#if defined(XRUDP_FEATURE_RUDP_NOISE)

#if !defined(XRT_FEATURE_CRYPTO_X25519)
#error "rudp_noise requires crypto_x25519 (XRT_FEATURE_CRYPTO_X25519)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_HKDF_SHA256)
#error "rudp_noise requires crypto_hkdf_sha256 (XRT_FEATURE_CRYPTO_HKDF_SHA256)"
#endif
#if !defined(XRT_FEATURE_RANDOM_SECURE)
#error "rudp_noise requires random_secure (XRT_FEATURE_RANDOM_SECURE)"
#endif

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
