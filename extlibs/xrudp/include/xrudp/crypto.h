#ifndef XRUDP_CRYPTO_H
#define XRUDP_CRYPTO_H

#include <xrudp/features.h>
#include <xrudp/wire.h>

#if defined(XRUDP_FEATURE_RUDP_CRYPTO)

#if !defined(XRT_FEATURE_CRYPTO_CHACHA20_POLY1305)
#error "rudp_crypto requires crypto_chacha20_poly1305 (XRT_FEATURE_CRYPTO_CHACHA20_POLY1305)"
#endif

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
 * Seal automatically classifies short (<=128B total) path-only controls as
 * authenticated PATH after packet 1; DATA/PATH share keys and packet numbers.
 * Open rejects business/ACK/PING disguised as PATH before committing state.
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
/* Same authenticated result/plaintext checks as Open, but NEVER commits replay,
 * receive keys or send-key confirmations. Intended for unvalidated-address
 * probes; success is not transport admission and must not generate data ACKs.
 * A subsequent Open must perform the actual commit. No allocation; a private
 * stack copy (including keys) is securely erased on every return. */
XRUDP_API xrudpresult xrtRudpCipherInspect(const xrudpcipher* pCipher,
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
