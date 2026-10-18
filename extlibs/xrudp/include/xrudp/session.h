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
#define XRUDP_STREAM_WRITE_STOPPED 64u
#define XRUDP_STREAM_READ_DONE 128u
#define XRUDP_STREAM_BYTES 0u
#define XRUDP_STREAM_MESSAGES 1u
#define XRUDP_SESSION_MESSAGE_STAGE_BYTES 4096u

typedef struct xrudpsession xrudpsession;
/* Called only by Poll, serialized with the entire session. Must COPY the
 * datagram before returning OK (UDP admission, not delivery). May return
 * BACKPRESSURE/NO_MEMORY for temporary local admission failure, IO_ERROR for
 * a terminal transport failure. Must not reenter/destroy this session. */
typedef xrudpresult (*xrudpsessionsendproc)(const void* pPacket, size_t iSize, void* pContext);
typedef xrudpresult (*xrudppathsendproc)(const void* pPacket, size_t iSize,
	const xrudpaddress* pDestination, void* pContext);

typedef struct xrudppathstate {
	xrudpaddress Active, Candidate;
	xrudpresult Failure; /* Last candidate failure, separate from connection Reason. */
	uint64_t DeadlineUs, NextProbeUs, CooldownUntilUs;
	uint64_t Validations, Failures, AddressDropped, Invalid, RateDropped;
	uint64_t ReceivedUnvalidated, SentUnvalidated;
	uint32_t Attempts, CandidateReceived, CandidateSent;
	bool Enabled, Validating;
} xrudppathstate;

typedef struct xrudpmtustate {
	uint32_t Effective, Ceiling, ProbeSize, Attempts, LargeSize; /* UDP payload bytes. */
	uint64_t ProbePacket, ProbeDeadlineUs, NextProbeUs, RaiseAfterUs, LargePacket, LargeDeadlineUs;
	uint64_t ProbesSent, ProbesAcknowledged, ProbeFailures, Fallbacks;
	xrudpresult Failure; /* TIMEOUT: unable to confirm the required 512B base. */
	bool Enabled, BaseConfirmed;
} xrudpmtustate;

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
	/* Raw Session default false: transport must guarantee no IP fragmentation
	 * before enabling. Native Endpoint default true and enforces UDP PMTU_PROBE.
	 * Starts at 512B, authenticated padded-PING ACKs raise to 768/1024/ceiling.
	 * Max 3 admitted probes per size, >=1s/3 PTO interval; failed growth waits
	 * 60s. Required 512B failure terminates. Independent large-packet stalls
	 * reduce to 512 and requeue reliable ranges, never game payload. */
	bool DiscoverMtu;
	/* Optional idle PING, 0 disabled (default), otherwise 1s..1h. Local policy,
	 * no wire negotiation. Effective interval is capped at one third of the
	 * negotiated idle timeout. Uses normal congestion/pacing/packet slots;
	 * no application progress, extra allocation or new probe reserve. Suspended
	 * while business work, PTO or graceful shutdown already drives the session. */
	uint64_t KeepAliveUs;
} xrudpsessionconfig;

typedef struct xrudpshutdownstate {
	uint32_t LocalFinalOrdinal, PeerFinalOrdinal, LocalTimeoutMs, PeerTimeoutMs;
	uint64_t DeadlineUs, LingerUntilUs; /* Local monotonic; 0 before first clock/no request. */
	bool Requested, PeerRequested, Ready, DeclarationAcknowledged;
	bool DrainedAcknowledged, PeerDrained, PeerDrainedAcknowledged, Succeeded;
} xrudpshutdownstate;

typedef struct xrudpsessionstate {
	xrudpresult Reason; /* OK while active; terminal reasons are sticky. */
	xrudpshutdownstate Shutdown;
	xrudppathstate Path;
	xrudpmtustate Mtu;
	xrudpreceivestate Receive;
	xrudptransmitstate Transmit;
	xrudpsentstate Sent;
	xrudpcongestion Congestion;
	xrudprtt Rtt;
	xrudpdatagramstate Datagrams;
	uint64_t DatagramAcknowledged, DatagramLost, DatagramDisabledDropped;
	uint32_t DatagramSendLimit; /* Current path TX limit less overhead/local queue bound. */
	uint64_t ReceivedConnection, ReceiveLimit, SentConnection, PeerLimit;
	uint64_t LastNowUs, LastActivityUs, ProgressAtUs;
	uint64_t ReceivedPackets, SentPackets, LostPackets, ProbeTimeouts, KeyUpdates;
	uint64_t InvalidPackets, OutputBackpressure;
	uint32_t PtoBackoff, OutstandingProbes;
	uint32_t ActiveStreams, PendingStreamEvents;
	uint32_t PendingReceiveStops; /* Waiting for real final size, independently bounded. */
	uint32_t PendingSendResets; /* Waiting for RESET ACK, independently bounded. */
	uint32_t IncomingLimit, OutgoingLimit;
	uint32_t WriteStopReason; /* Default stream's authenticated peer request. */
	bool AckPending, ReadReady, ReadDone, WriteDone, WriteStopped;
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
	bool Local, OpenAcknowledged, ReadReady, ReadDone, WriteDone, RetireReady;
	bool ResetQueued, ResetAcknowledged;
	bool StopQueued, StopSent, StopComplete, PeerStopped;
	uint32_t ResetReason;
	uint32_t PeerStopReason;
	uint64_t ResetFinal;
	uint64_t ReceiveStopDeadlineUs; /* 0 before first clock observation/after real final. */
	uint64_t ResetDeadlineUs; /* 0 before first clock observation/after RESET ACK. */
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
/* Enable addressed output before any session packet input/output, using the
 * address proved by the handshake. Same Context as Create; callback admission
 * contract is identical to Send. Once enabled, use PathInput, never Input.
 * One fixed candidate, random 16-byte proof, max three challenges/3s and a 3s
 * cooldown after failure; unvalidated output <=3x fresh authenticated bytes. An explicit
 * trusted ProbePath grants only 336 initial bytes for opening a NAT mapping.
 * Response must match the pending token and candidate source address. This is
 * xrudp's single-path contract, not QUIC migration/MTU validation.
 * Fresh authenticated traffic can revalidate a previous address immediately;
 * unvalidated PNs are spent across failed attempts, never credited twice.
 * A passive response older than newer active traffic cannot select a path.
 * Candidate failure leaves the active connection intact. No allocation. */
XRUDP_API xrudpresult xrtRudpSessionEnablePaths(xrudpsession* pSession,
	const xrudpaddress* pVerified, xrudppathsendproc pSend);
/* Trusted discovery supplies the address; no untrusted frame chooses a target.
 * Now clocks the session; existing connection deadlines still apply. Rejects
 * new probes during local shutdown, but existing validation may finish draining.
 * DUPLICATE for active/current candidate, BACKPRESSURE for cooldown/other
 * candidate. Its new matching proof may select the caller's preferred target
 * even while newer traffic still arrives on the current path. Queries/stats
 * are in SessionState.Path; never expose probe tokens. */
XRUDP_API xrudpresult xrtRudpSessionProbePath(xrudpsession* pSession,
	const xrudpaddress* pCandidate, uint64_t iNowUs);
XRUDP_API xrudpresult xrtRudpSessionPathInput(xrudpsession* pSession,
	const xrudpaddress* pSource, const void* pPacket, size_t iSize, uint64_t iNowUs);
XRUDP_API xrudpresult xrtRudpSessionWrite(xrudpsession* pSession,
	const void* pData, size_t iSize, size_t* pWritten);
XRUDP_API xrudpresult xrtRudpSessionFinish(xrudpsession* pSession);
/* Bounded two-sided reliable drain. TimeoutMs=1000..600000, measured from the
 * next owner clock observation; duplicates cannot renew it. Existing idle,
 * useful-progress and STOP/RESET deadlines may expire sooner. Forbid new
 * opens/writes, FIN every admitted sending half, cancel queued game datagrams.
 * Reads/Stop/Retire remain available. PeerRequested in State asks the peer
 * application to finish existing responses and explicitly Shutdown; it never
 * auto-FINs asynchronous replies. Peer request also forbids new local opens.
 * Success requires both declarations, all admitted reliable halves drained,
 * peer DRAINED, own DRAINED ACK, and real UDP admission of its final ACK. Keys
 * remain during a finite nonrenewable linger for lost final ACKs. CLOSED plus
 * Shutdown.Succeeded distinguishes clean drain from abort/error/timeout.
 * This is transport drain, not a business receipt or delivery of datagrams.
 * No allocation; same timeout returns DUPLICATE, changed timeout ARGUMENT. */
XRUDP_API xrudpresult xrtRudpSessionShutdown(xrudpsession* pSession, uint32_t iTimeoutMs);
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
 * discarded, never-sent buffers do not consume peer flow credit. RESET must
 * be ACKed within its own ProgressTimeoutUs interval, anchored at the next
 * clock observation. Other traffic, duplicate Reset and retries cannot renew
 * it; timeout closes the connection. Valid ACK cancels this deadline. */
XRUDP_API xrudpresult xrtRudpSessionStreamReset(xrudpsession* pSession, uint32_t iId, uint32_t iReason);
/* Cancel only local reception, clear unread plaintext/staged messages, freeze
 * stream credit and reliably request peer STOP_SENDING. Reverse sending and
 * other streams remain live. Reads/MessagePeek return CANCELLED; discarded
 * logical offsets release connection credit exactly once. Same reason is
 * DUPLICATE, changing it is ARGUMENT; already consumed EOF/RESET is DUPLICATE.
 * StopComplete means this request was ACKed OR a real peer FIN/RESET arrived;
 * retirement still requires that real final size and every old packet copy.
 * READ_DONE reports a fully consumed EOF, peer RESET, or stopped reception with
 * a real final size. It is distinct from clean READ_EOF and from local intent.
 * Waiting for that final size has its own ProgressTimeoutUs deadline, anchored
 * at the next owner clock observation; ACKs/other traffic/repeated Stop cannot
 * renew it. Expiry closes the connection without inventing a peer final size.
 * Peer requests produce WRITE_STOPPED and cancel only local sending, with a
 * RESET charging actual highest sent, never buffered but unsent bytes.
 * No allocation; call Poll after success. Default Stop is stream 1. */
XRUDP_API xrudpresult xrtRudpSessionStop(xrudpsession* pSession, uint32_t iReason);
XRUDP_API xrudpresult xrtRudpSessionStreamStop(xrudpsession* pSession, uint32_t iId, uint32_t iReason);
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
