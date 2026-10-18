#ifndef XRUDP_PUNCH_H
#define XRUDP_PUNCH_H
#include <xrudp/features.h>
#include <xrudp/stun.h>
#if defined(XRUDP_FEATURE_RUDP_PUNCH)

#define XRUDP_PUNCH_CANDIDATES_MAX 8u
#define XRUDP_PUNCH_INDICATIONS_MAX 6u
#define XRUDP_PUNCH_DEFAULT_TIMEOUT_MS 8000u
#define XRUDP_PUNCH_INTERVAL_US UINT64_C(50000)

/* No connectivity or identity claim: DONE means this candidate's six local
 * UDP admissions, not delivery. OK=pending; IO_ERROR disables just this
 * candidate. Cancel/expiry finish remaining candidates without more output. */
typedef struct xrudppunchcandidate {
	xrudpaddress Address;
	xrudpresult Reason;
	uint32_t Sent;
} xrudppunchcandidate;

/* Safe to inspect/log; no transaction IDs. Reason DONE means all bursts were
 * admitted; IO_ERROR means one or more destinations failed, after the others
 * finished. Only authenticated Noise confirmation establishes the peer.
 * NextUs includes global spacing and the absolute, nonrenewable deadline;
 * it may already be due. The adapter must supply a nondecreasing clock. */
typedef struct xrudppunchstate {
	xrudppunchcandidate Candidates[XRUDP_PUNCH_CANDIDATES_MAX];
	xrudpresult Reason;
	uint32_t Count, Sent, Completed, Failed;
	uint64_t StartedUs, DeadlineUs, NextUs;
} xrudppunchstate;

/* Caller-owned fixed storage, serialized owner. Init needs secure randomness;
 * Next/Sent/Reject/Cancel/State do not allocate, perform I/O or need RNG.
 * Underscore fields are private, not serialization/logging interfaces.
 * Object, input addresses, I/O buffers and output views must not overlap. */
typedef struct xrudppunch {
	xrudppunchstate State;
	uint64_t _Now, _SendAt, _Due[XRUDP_PUNCH_CANDIDATES_MAX];
	unsigned char _Transactions[XRUDP_PUNCH_CANDIDATES_MAX][XRUDP_PUNCH_INDICATIONS_MAX][XRUDP_STUN_TRANSACTION_SIZE];
	uint32_t _Cursor, _Offered;
} xrudppunch;

XRT_EXTERN_C_BEGIN
/* Trusted application policy authorizes 1..8 distinct, valid unicast targets
 * before Init. They cannot come from unauthenticated UDP input. No DNS,
 * destination scanning/prediction or source-learned targets. Multiple address
 * families are allowed only if the adapter routes each on its correct socket.
 * Timeout=0 is 8s; explicit 1000..10000ms. Failure preserves the object.
 * This outbound-only primitive opens NAT filters. It neither processes nor
 * answers input and cannot replace account ACL, peer key/scope/context pins.
 * The adapter MUST use the socket that receives the real handshake/business.
 * Across jobs, additionally enforce one admitted indication per 50ms/socket. */
XRUDP_API xrudpresult xrtRudpPunchInit(xrudppunch* pPunch, const xrudpaddress* pCandidates,
	size_t iCount, uint64_t iNowUs, uint32_t iTimeoutMs);
/* Fair round robin across due candidates. A successful offer repeats the
 * same bytes/target/index until Sent/Reject; local backpressure consumes no
 * indication or cursor. Fresh transaction for each admitted indication.
 * Capacity >=28. Written is always zeroed; other outputs/state preserved for
 * invalid arguments. TIMEOUT with State.Reason OK means wait until NextUs;
 * terminal DONE/IO_ERROR/TIMEOUT/CANCELLED must be distinguished via State.
 * Candidate index is valid only for the current offer in this generation.
 * Calling Init invalidates old offers; adapters must drain sends first. */
XRUDP_API xrudpresult xrtRudpPunchNext(xrudppunch* pPunch, uint64_t iNowUs,
	void* pOutput, size_t iCapacity, size_t* pWritten, xrudpaddress* pTarget,
	uint32_t* pIndex, uint64_t* pNextUs);
/* Only after the exact offered datagram was admitted by UDP. Per-candidate
 * gaps 200/400/800/1600/2000ms; no catch-up burst after scheduler delays.
 * Consumed offers cannot be reused. Last admission may return DONE/IO_ERROR. */
XRUDP_API xrudpresult xrtRudpPunchSent(xrudppunch* pPunch, uint32_t iIndex, uint64_t iNowUs);
/* Hard route failure only; temporarily full/memory/AGAIN leaves the offer
 * pending. Disables this destination, then fairly services the rest. Does
 * not consume Sent or permit an earlier send by another candidate. */
XRUDP_API xrudpresult xrtRudpPunchReject(xrudppunch* pPunch, uint32_t iIndex, uint64_t iNowUs);
/* Cancel on real peer confirmation, relay fallback, ACL change or close.
 * No inline callbacks; already terminal returns DONE. */
XRUDP_API xrudpresult xrtRudpPunchCancel(xrudppunch* pPunch);
XRUDP_API xrudpresult xrtRudpPunchState(const xrudppunch* pPunch, xrudppunchstate* pState);
XRT_EXTERN_C_END
#endif
#endif
