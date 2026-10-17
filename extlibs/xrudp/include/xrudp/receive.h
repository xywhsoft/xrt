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
