#ifndef XRUDP_SESSION_H
#define XRUDP_SESSION_H

#include <xrudp/features.h>
#include <xrudp/handshake.h>
#include <xrudp/congestion.h>
#include <xrudp/timing.h>
#include <xrudp/receive.h>
#include <xrudp/transmit.h>

#if defined(XRUDP_FEATURE_RUDP_SESSION)

#define XRUDP_SESSION_PACKETS_MAX 256u
#define XRUDP_SESSION_POLL_BURST 8u

typedef struct xrudpsession xrudpsession;
/* Called only by Poll, serialized with the entire session. Must COPY the
 * datagram before returning OK (UDP admission, not delivery). May return
 * BACKPRESSURE/NO_MEMORY for temporary local admission failure, IO_ERROR for
 * a terminal transport failure. Must not reenter/destroy this session. */
typedef xrudpresult (*xrudpsessionsendproc)(const void* pPacket, size_t iSize, void* pContext);

typedef struct xrudpsessionconfig {
	xrudpparameters Local, Peer; /* Authenticated, already negotiated parameters. */
	uint32_t ReceiveCapacity, TransmitCapacity;
	uint32_t PacketCapacity; /* 1..256 normal slots plus bounded probe reserve. */
	uint32_t ControlPerSecond; /* ACK-only output limit, 40..4096. */
	uint64_t MaximumWindow, InitialRttUs;
	uint64_t ProgressTimeoutUs; /* 1s..10min, default 30s; cannot be refreshed by PING. */
	uint64_t KeyUpdatePackets; /* Local rotation threshold, 8..2^24. */
} xrudpsessionconfig;

typedef struct xrudpsessionstate {
	xrudpresult Reason; /* OK while active; terminal reasons are sticky. */
	xrudpreceivestate Receive;
	xrudptransmitstate Transmit;
	xrudpsentstate Sent;
	xrudpcongestion Congestion;
	xrudprtt Rtt;
	uint64_t ReceivedConnection, ReceiveLimit, SentConnection, PeerLimit;
	uint64_t LastNowUs, LastActivityUs, ProgressAtUs;
	uint64_t ReceivedPackets, SentPackets, LostPackets, ProbeTimeouts, KeyUpdates;
	uint64_t InvalidPackets, OutputBackpressure;
	uint32_t PtoBackoff, OutstandingProbes;
	bool AckPending;
} xrudpsessionstate;

XRT_EXTERN_C_BEGIN

/* First connection profile: implicit bidirectional byte stream 1, admitted by
 * the completed handshake, Local.MaxStreams must be 1. Additional OPEN, game
 * datagrams and path migration are not enabled by this profile yet.
 * No private thread/socket, no callbacks except serialized output admission.
 * Every method requires one owner; all caller buffers must be disjoint. */
XRUDP_API void xrtRudpSessionConfigInit(xrudpsessionconfig* pConfig);
/* Total logical bytes including the cipher transferred at Create; excludes
 * allocator overhead and the caller's UDP queues. Reserve BEFORE admission. */
XRUDP_API xrudpresult xrtRudpSessionAllocationSize(const xrudpsessionconfig* pConfig, size_t* pSize);
/* Cipher must be exclusively owned and promoted from completed proof: next PN
 * 2, largest received >=1, epoch 1. Ownership transfers ONLY on success; output
 * remains unchanged on failure. Session initializes sent/ACK proof prefix 1.
 * Config identity/parameters must come from the authenticated handshake. */
XRUDP_API xrudpresult xrtRudpSessionCreate(const xrudpsessionconfig* pConfig,
	xrudpcipher* pCipher, uint64_t iNowUs, xrudpsessionsendproc pSend,
	void* pContext, xrudpsession** ppSession);
XRUDP_API void xrtRudpSessionDestroy(xrudpsession* pSession);
XRUDP_API xrudpresult xrtRudpSessionState(const xrudpsession* pSession, xrudpsessionstate* pState);
XRUDP_API xrudpresult xrtRudpSessionWrite(xrudpsession* pSession,
	const void* pData, size_t iSize, size_t* pWritten);
XRUDP_API xrudpresult xrtRudpSessionFinish(xrudpsession* pSession);
/* Read releases stream and connection credit; grants are retransmitted until
 * validated ACK. DONE is clean stream EOF, not implicit connection closure. */
XRUDP_API xrudpresult xrtRudpSessionRead(xrudpsession* pSession,
	void* pOutput, size_t iCapacity, size_t* pRead);
/* Authenticate, preflight ALL ACK/data/reset/control state, then commit once.
 * Malformed authenticated input terminates the session; no partial delivery or
 * false acknowledgement. Noise/forgery/old/duplicate packets do not terminate.
 * Caller must first enforce the original verified source address. */
XRUDP_API xrudpresult xrtRudpSessionInput(xrudpsession* pSession,
	const void* pPacket, size_t iSize, uint64_t iNowUs);
/* Bounded maintenance/output (<=8 datagrams), no allocation. Due is the next
 * monotonic microsecond service deadline, also when output is backpressured.
 * Call again after Write/Finish/Read/Input and at Due; no blocking or sleeps.
 * Time must never go backwards. Finite idle AND useful-progress deadlines apply. */
XRUDP_API xrudpresult xrtRudpSessionPoll(xrudpsession* pSession,
	uint64_t iNowUs, uint64_t* pDueUs);
/* Local cancellation, no output/allocation. Clears plaintext and drains unique
 * packet ownership. Caller coordinates any best-effort remote CLOSE separately. */
XRUDP_API xrudpresult xrtRudpSessionAbort(xrudpsession* pSession, xrudpresult iReason);

XRT_EXTERN_C_END
#endif
#endif
