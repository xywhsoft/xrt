#ifndef XRUDP_RECEIVE_H
#define XRUDP_RECEIVE_H

#include <xrudp/features.h>
#include <xrudp/wire.h>

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
	uint32_t Stopped; /* Local delivery cancellation; peer final size still unknown. */
	uint32_t StopReason;
} xrudpreceivestate;

typedef struct xrudpreceivechange {
	uint64_t HighestDelta; /* Connection flow-control cost, includes holes. */
	uint64_t NewBytes; /* Newly buffered bytes, excludes duplicates/consumed data. */
	uint64_t ConsumedDelta; /* RESET/Stop/discard releases charged logical offsets. */
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
	uint64_t ConsumedDelta; /* HighestDelta of already stopped streams, once each. */
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
/* A Read can return bytes or terminal EOF/RESET/local Stop now (no hole wait). */
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
 * ConsumedDelta counts each stopped stream's new logical maximum once, not
 * each duplicate/overlapping frame; normal streams still buffer NewBytes.
 * CheckBatch is pure; PushBatch rechecks and commits only after ALL validation.
 * Inputs and output objects must be disjoint from every receive object.
 * The connection driver must ALSO preflight ACK/control/OPEN/RESET frames,
 * authorization and lifecycle before invoking PushBatch and recording an ACK. */
XRUDP_API xrudpresult xrtRudpReceiveCheckBatch(const xrudpreceiveframe* pFrames,
	size_t iCount, uint64_t iRemainingConnectionCredit, xrudpreceivebatchchange* pChange);
XRUDP_API xrudpresult xrtRudpReceivePushBatch(const xrudpreceiveframe* pFrames,
	size_t iCount, uint64_t iRemainingConnectionCredit, xrudpreceivebatchchange* pChange);
/* Copies and consumes only contiguous data. BACKPRESSURE means a receive hole;
 * DONE is clean EOF, CANCELLED is peer RESET/local Stop, with zero read count.
 * EOF/RESET are never inferred merely from an empty buffer. Capacity must >0.
 * Successful consumption does not itself advertise additional peer credit. */
XRUDP_API xrudpresult xrtRudpReceiveRead(xrudpreceive* pReceive,
	void* pOutput, size_t iCapacity, size_t* pRead);
/* Pure copy at an absolute offset in [ReadOffset,Limit]. Stops at the first
 * hole, never consumes data or grants credit. Same EOF/RESET results as Read;
 * *pRead=0 on failure. Useful for validating a message prefix before committing
 * an authenticated packet's ACK/data transaction. No allocation. */
XRUDP_API xrudpresult xrtRudpReceivePeek(const xrudpreceive* pReceive,
	uint64_t iOffset, void* pOutput, size_t iCapacity, size_t* pRead);
/* Monotonic explicit credit, <=ReadOffset+Capacity, saturating at UINT64_MAX.
 * Owner sends/retransmits the latest MAX_STREAM_DATA value. */
XRUDP_API xrudpresult xrtRudpReceiveExtend(xrudpreceive* pReceive, uint64_t iLimit);
/* Locally stop delivery and clear all buffered plaintext/bitmap. Consume all
 * already charged offsets, including holes, exactly once; never invent a peer
 * final size or enlarge the advertised limit. Later authenticated STREAMs are
 * validated for credit/offset/final-size and discarded without copying, with
 * ConsumedDelta tracking only each new maximum. Read/Peek return CANCELLED.
 * Discarded plaintext overlaps are not compared; offset/final constraints stay.
 * Extend is CANCELLED; Restart waits for a real peer FIN/RESET. Same reason is
 * DUPLICATE, changing a stopped reason is ARGUMENT. Already RESET/consumed EOF
 * is DUPLICATE without changing its existing terminal result. No allocation.
 * This is a buffer primitive: the driver must reliably notify the peer and
 * use ConsumedDelta to release CONNECTION credit without granting this stream. */
XRUDP_API xrudpresult xrtRudpReceiveStop(xrudpreceive* pReceive,
	uint32_t iReason, xrudpreceivechange* pChange);
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
