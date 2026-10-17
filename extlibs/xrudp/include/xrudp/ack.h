#ifndef XRUDP_ACK_H
#define XRUDP_ACK_H

#include <xrudp/features.h>
#include <xrudp/wire.h>

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
