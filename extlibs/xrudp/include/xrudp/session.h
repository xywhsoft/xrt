#ifndef XRUDP_SESSION_H
#define XRUDP_SESSION_H

#include <xrudp/features.h>
#include <xrudp/handshake.h>
#include <xrudp/congestion.h>
#include <xrudp/timing.h>
#include <xrudp/receive.h>
#include <xrudp/transmit.h>
#include <xrudp/message.h>
#include <xrudp/datagram.h>

#if defined(XRUDP_FEATURE_RUDP_SESSION)

#define XRUDP_SESSION_PACKETS_MAX 256u
#define XRUDP_SESSION_POLL_BURST 8u
#define XRUDP_SESSION_STREAM_CAPACITY_MAX 64u
#define XRUDP_STREAM_OPENED 1u
#define XRUDP_STREAM_READ_READY 2u
#define XRUDP_STREAM_WRITE_READY 4u
#define XRUDP_STREAM_WRITE_DONE 8u
#define XRUDP_STREAM_READ_EOF 16u
#define XRUDP_STREAM_RETIRE_READY 32u
#define XRUDP_STREAM_BYTES 0u
#define XRUDP_STREAM_MESSAGES 1u
#define XRUDP_SESSION_MESSAGE_STAGE_BYTES 4096u

typedef struct xrudpsession xrudpsession;
/* Called only by Poll, serialized with the entire session. Must COPY the
 * datagram before returning OK (UDP admission, not delivery). May return
 * BACKPRESSURE/NO_MEMORY for temporary local admission failure, IO_ERROR for
 * a terminal transport failure. Must not reenter/destroy this session. */
typedef xrudpresult (*xrudpsessionsendproc)(const void* pPacket, size_t iSize, void* pContext);

typedef struct xrudpsessionconfig {
	xrudpparameters Local, Peer; /* Authenticated, already negotiated parameters. */
	xrudpdatagramconfig Datagrams; /* All zero disables; otherwise fixed local queue budget. */
	xrudprole Role; /* Trusted handshake role, not chosen from a DATA frame. */
	uint32_t StreamCapacity; /* Per-origin pool, includes implicit stream 1; 1..64. */
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
	xrudpdatagramstate Datagrams;
	uint64_t DatagramAcknowledged, DatagramLost, DatagramDisabledDropped;
	uint32_t DatagramSendLimit; /* Local queue/negotiated packet size bound. */
	uint64_t ReceivedConnection, ReceiveLimit, SentConnection, PeerLimit;
	uint64_t LastNowUs, LastActivityUs, ProgressAtUs;
	uint64_t ReceivedPackets, SentPackets, LostPackets, ProbeTimeouts, KeyUpdates;
	uint64_t InvalidPackets, OutputBackpressure;
	uint32_t PtoBackoff, OutstandingProbes;
	uint32_t ActiveStreams, PendingStreamEvents;
	uint32_t IncomingLimit, OutgoingLimit;
	bool AckPending, ReadReady;
} xrudpsessionstate;

typedef struct xrudpstreamconfig {
	uint8_t Kind; /* BYTES or MESSAGES; default byte stream 1 remains BYTES. */
	uint8_t Priority; /* 0..3; bounded weighted deficit service, no starvation. */
	uint16_t Weight; /* 1..16 local policy. Wire weights are capped to 16 for service. */
} xrudpstreamconfig;
typedef struct xrudpstreamstate {
	uint32_t Id, References;
	uint16_t Weight;
	uint8_t Priority, Kind;
	bool Local, OpenAcknowledged, ReadReady, WriteDone, RetireReady;
	bool ResetQueued, ResetAcknowledged;
	uint32_t ResetReason;
	uint64_t ResetFinal;
	uint32_t SendMessageLimit; /* min(peer authenticated bound, local TX capacity-4). */
	xrudpmessagestate Message; /* Fixed receive staging; zero for byte streams. */
	xrudpreceivestate Receive;
	xrudptransmitstate Transmit;
} xrudpstreamstate;
typedef struct xrudpstreamevent { uint32_t Id, Events; } xrudpstreamevent;

XRT_EXTERN_C_BEGIN

/* Implicit bidirectional byte stream 1 is admitted by the completed handshake.
 * StreamCapacity reserves 2*N-1 fixed receive/transmit pairs at construction;
 * Local.MaxStreams <= N. When Local.MaxMessageSize>0, all 2*N-2 extra slots
 * also reserve one fixed message assembler. OPEN/read/write/retire never
 * allocate. Optional datagram queues are also reserved at construction.
 * Path migration remains a separate layer.
 * No private thread/socket, no callbacks except serialized output admission.
 * Every method requires one owner; all caller buffers must be disjoint. */
XRUDP_API void xrtRudpSessionConfigInit(xrudpsessionconfig* pConfig);
XRUDP_API void xrtRudpStreamConfigInit(xrudpstreamconfig* pConfig);
/* Total logical bytes including the cipher transferred at Create; excludes
 * allocator overhead and the caller's UDP queues. Reserve BEFORE admission. */
XRUDP_API xrudpresult xrtRudpSessionAllocationSize(const xrudpsessionconfig* pConfig, size_t* pSize);
/* Cipher must be exclusively owned and promoted from completed proof: next PN
 * 2, largest received 1, epoch 1. Ownership transfers ONLY on success; output
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
/* New bidirectional typed stream. MESSAGES requires both authenticated message
 * bounds to be nonzero. IDs never reused: initiator odd, responder
 * even; default ID 1 consumes ordinal 1 in both pools. Success permits bounded
 * writing before OPEN ACK; output repeats OPEN until acknowledged. */
XRUDP_API xrudpresult xrtRudpSessionStreamOpen(xrudpsession* pSession,
	const xrudpstreamconfig* pConfig, uint32_t* pId);
XRUDP_API xrudpresult xrtRudpSessionStreamState(const xrudpsession* pSession,
	uint32_t iId, xrudpstreamstate* pState);
XRUDP_API xrudpresult xrtRudpSessionStreamWrite(xrudpsession* pSession,
	uint32_t iId, const void* pData, size_t iSize, size_t* pWritten);
XRUDP_API xrudpresult xrtRudpSessionStreamRead(xrudpsession* pSession,
	uint32_t iId, void* pOutput, size_t iCapacity, size_t* pRead);
/* Message-only APIs; raw StreamRead/Write reject a message stream. Write is
 * atomic even for empty messages. LIMIT exceeds peer/local SendMessageLimit;
 * BACKPRESSURE is temporary ring capacity. Peek is pure. Read returns a whole
 * payload; TRUNCATED reports required size without consuming/writing bytes.
 * NULL/0 can query a nonempty record or consume an empty one. READ_READY is a
 * full message or EOF/RESET, never a partial body. Call Poll after a successful
 * Read, INCLUDING an empty message, to stage the next record and return credit.
 * Poll/Input stage at most MESSAGE_STAGE_BYTES each, with round-robin service.
 * Invalid head length/truncated FIN is MALFORMED and terminates the session. */
XRUDP_API xrudpresult xrtRudpSessionMessageWrite(xrudpsession* pSession,
	uint32_t iId, const void* pData, size_t iSize);
XRUDP_API xrudpresult xrtRudpSessionMessagePeek(const xrudpsession* pSession,
	uint32_t iId, size_t* pSize);
XRUDP_API xrudpresult xrtRudpSessionMessageRead(xrudpsession* pSession,
	uint32_t iId, void* pOutput, size_t iCapacity, size_t* pSize);
XRUDP_API xrudpresult xrtRudpSessionStreamFinish(xrudpsession* pSession, uint32_t iId);
/* Reset only the local sending half. Final size is actual highest sent, so
 * discarded, never-sent buffers do not consume peer flow credit. */
XRUDP_API xrudpresult xrtRudpSessionStreamReset(xrudpsession* pSession, uint32_t iId, uint32_t iReason);
/* Reclaim after both halves terminate/consume and ALL packet copies drain.
 * Cannot retire default stream 1. Retiring a peer-origin stream reliably
 * replenishes cumulative MAX_STREAMS; late frames for retired IDs are ignored. */
XRUDP_API xrudpresult xrtRudpSessionStreamRetire(xrudpsession* pSession, uint32_t iId);
/* Coalesced per-stream edge hints, including remote OPEN. DONE when none.
 * No callbacks, no allocation; explicit retirement discards pending hints.
 * RETIRE_READY follows both half terminations and all packet copies draining.
 * Later output may reacquire a control reference: recheck State/Retire. */
XRUDP_API xrudpresult xrtRudpSessionStreamNextEvent(xrudpsession* pSession, xrudpstreamevent* pEvent);
/* Optional unreliable datagrams. Configure local channel IDs/modes at both
 * endpoints before traffic; unknown/disabled/full channels drop authenticated
 * datagrams without allocating or rejecting reliable frames in the same packet.
 * ORDERED/LATEST use sequence suppression, never loss retransmission. Queues
 * share the connection's cipher/ACK/sent ledger/NewReno/pacing with streams.
 * Both traffic classes get packet service when continuously backlogged; PTO
 * keeps its existing hard budget and never retransmits a game payload.
 * Write/Read clock the owner at Now; deadlines/lifetimes are local monotonic us.
 * No fragmenting: Write above DatagramSendLimit returns LIMIT. Sequence output
 * is unchanged on failure. Read includes empty datagrams and TRUNCATED info.
 * These core methods allocate nothing. Call Poll after mutable operations. */
XRUDP_API xrudpresult xrtRudpSessionDatagramConfigure(xrudpsession* pSession, const xrudpdatagramchannel* pChannel);
XRUDP_API xrudpresult xrtRudpSessionDatagramChannelState(const xrudpsession* pSession,
	uint32_t iChannel, xrudpdatagramchannelstate* pState);
XRUDP_API xrudpresult xrtRudpSessionDatagramWrite(xrudpsession* pSession, uint32_t iChannel,
	const void* pData, size_t iSize, uint64_t iNowUs, uint64_t iDeadlineUs, uint64_t* pSequence);
XRUDP_API xrudpresult xrtRudpSessionDatagramPeek(const xrudpsession* pSession, xrudpdatagraminfo* pInfo);
XRUDP_API xrudpresult xrtRudpSessionDatagramRead(xrudpsession* pSession, void* pOutput,
	size_t iCapacity, uint64_t iNowUs, xrudpdatagraminfo* pInfo);
/* Authenticate, preflight ALL ACK/data/reset/control state, then commit once.
 * Malformed authenticated input terminates the session; no partial delivery or
 * false acknowledgement. Noise/forgery/old/duplicate packets do not terminate.
 * BACKPRESSURE means an OPEN gap or unavailable admission slot: no connection
 * frames commit and this PN is NOT ACKed. Retransmit contents with a fresh PN.
 * An authenticated receive key update may still be confirmed without that ACK.
 * Caller must first enforce the original verified source address. */
XRUDP_API xrudpresult xrtRudpSessionInput(xrudpsession* pSession,
	const void* pPacket, size_t iSize, uint64_t iNowUs);
/* Bounded maintenance/output (<=8 datagrams), no allocation. Due is the next
 * monotonic microsecond service deadline, also when output is backpressured.
 * Call again after Write/Finish/Read/Input and at Due; no blocking or sleeps.
 * Time must never go backwards. Finite idle AND useful-progress deadlines apply.
 * New local work after idle starts its progress deadline at the first following
 * Poll/Input clock observation. Additional local operations cannot renew it. */
XRUDP_API xrudpresult xrtRudpSessionPoll(xrudpsession* pSession,
	uint64_t iNowUs, uint64_t* pDueUs);
/* Local cancellation, no output/allocation. Clears plaintext and drains unique
 * packet ownership. Caller coordinates any best-effort remote CLOSE separately. */
XRUDP_API xrudpresult xrtRudpSessionAbort(xrudpsession* pSession, xrudpresult iReason);

XRT_EXTERN_C_END
#endif
#endif
