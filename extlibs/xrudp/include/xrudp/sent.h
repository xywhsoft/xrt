#ifndef XRUDP_SENT_H
#define XRUDP_SENT_H

#include <xrudp/features.h>
#include <xrudp/ack.h>

#if defined(XRUDP_FEATURE_RUDP_SENT)

#define XRUDP_SENT_CAPACITY_DEFAULT 256u
#define XRUDP_SENT_CAPACITY_MAX 4096u
#define XRUDP_SENT_PROBE_RESERVE 32u
#define XRUDP_SENT_ACK_ELICITING 1u
#define XRUDP_SENT_PROBE 2u
#define XRUDP_SENT_PADDING 4u
#define XRUDP_SENT_MTU 8u
#define XRUDP_SENT_ACK_ONLY 16u

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
/* Pure ACK metadata may use spare slots from the same fixed reserve without
 * consuming flight/congestion or creating a PTO probe. Must record exactly
 * ACK_ONLY (no other flags), never business/padding/PING. This avoids circular
 * ACK starvation when both directions have filled a one-slot normal window.
 * Shares the total fixed cap but leaves two slots exclusively for true PTO;
 * metadata expiry remains bounded by the driver. */
XRUDP_API bool xrtRudpSentCanRecordAck(const xrudpsentwindow* pWindow);
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
/* After validated path migration, finish all old pending flight as LOST (data/
 * padding) or EXPIRED (ACK-only), WITHOUT stopping this ledger or resetting its
 * global packet-number/ACK watermark. The driver requeues reliable references
 * and resets old-path congestion/RTT after consuming these unique events.
 * No packet is acknowledged by this operation. Atomic capacity/timing checks;
 * failure leaves state/events unchanged and Count=0. Empty is DONE and does
 * not advance time. Old ACKs never produce ownership/growth again. */
XRUDP_API xrudpresult xrtRudpSentDiscardPath(xrudpsentwindow* pWindow, uint64_t iNowUs,
	xrudpsentevent* pEvents, size_t iCapacity, size_t* pCount);
/* Retire one locally timed-out packet, without acknowledging it or stopping
 * the ledger. Produces LOST for in-flight bytes, EXPIRED for pure ACKs. An
 * already finished/retired PN is DONE and leaves Event/time unchanged; future
 * or retained unsent gaps are ARGUMENT. Useful for a size probe whose timeout
 * must release its normal slot even when no higher PN was acknowledged.
 * MTU marks an ACK-eliciting padded packet >=512B, never a PTO reserve probe.
 * It still consumes congestion/pacing/flight; loss alone is not congestion
 * evidence, and its ACK alone does not grow the congestion window. */
XRUDP_API xrudpresult xrtRudpSentDiscardPacket(xrudpsentwindow* pWindow,
	uint64_t iPacketNumber, uint64_t iNowUs, xrudpsentevent* pEvent);
/* Complete pending packets as CANCELLED, then stop accepting new records.
 * Caller drains/cancels higher-level stream operations before Destroy. */
XRUDP_API xrudpresult xrtRudpSentAbort(xrudpsentwindow* pWindow,
	xrudpsentevent* pEvents, size_t iCapacity, size_t* pCount);

XRT_EXTERN_C_END

#endif
#endif
