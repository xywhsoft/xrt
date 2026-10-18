#ifndef XRUDP_TRANSMIT_H
#define XRUDP_TRANSMIT_H

#include <xrudp/features.h>
#include <xrudp/wire.h>

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
/* Atomically queues one u32 big-endian length plus payload. Empty messages
 * are valid. LIMIT means Size+4 exceeds this ring's total capacity; temporary
 * lack of free capacity is BACKPRESSURE. Every failure leaves the ring intact.
 * This does not negotiate peer message limits or implement message delivery. */
XRUDP_API xrudpresult xrtRudpTransmitMessageWrite(xrudptransmit* pTransmit,
	const void* pData, size_t iSize);
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
