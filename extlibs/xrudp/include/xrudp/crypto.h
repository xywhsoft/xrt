#ifndef XRUDP_CRYPTO_H
#define XRUDP_CRYPTO_H

#include <xrudp/features.h>
#include <xrudp/wire.h>

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
