#ifndef XRUDP_ENDPOINT_H
#define XRUDP_ENDPOINT_H

#include <xrudp/features.h>
#include <xrudp/session.h>
#include <xrudp/discovery.h>
#include <xrudp/punch.h>
#include <xrt/udp.h>

#if defined(XRUDP_FEATURE_RUDP_ENDPOINT)

#define XRUDP_ENDPOINT_PEERS_MAX 256u
#define XRUDP_ENDPOINT_ADDRESS_BUCKETS 64u
#define XRUDP_PEER_READ_READY 1u
#define XRUDP_PEER_WRITE_READY 2u
#define XRUDP_PEER_WRITE_DONE 4u
#define XRUDP_PEER_READ_EOF 8u
#define XRUDP_PEER_STREAMS_READY 16u
#define XRUDP_PEER_DATAGRAM_READ_READY 32u
#define XRUDP_PEER_DATAGRAM_WRITE_READY 64u
#define XRUDP_PEER_WRITE_STOPPED 128u
#define XRUDP_PEER_READ_DONE 256u
#define XRUDP_PEER_SHUTDOWN_REQUESTED 512u
#define XRUDP_PEER_PATH_CHANGED 1024u
#define XRUDP_PEER_PATH_FAILED 2048u
#define XRUDP_PEER_MTU_CHANGED 4096u
#define XRUDP_PEER_STREAM_EVENT_BURST 8u
#define XRUDP_ENDPOINT_DISCOVERIES_MAX 4u
#define XRUDP_ENDPOINT_PUNCHES_MAX 2u
#define XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX 8u

/* Authenticated, return-path checked address hints. These do not admit
 * application data or replace the pinned Noise handshake. No keys/nonces are
 * exposed. Reply = our request returned; Confirmed = peer returned our cookie. */
typedef struct xrudpcheckstate {
	xnetaddr Reply[XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX];
	xnetaddr Confirmed[XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX];
	uint32_t Replies, Confirmations, Sent, Rejected;
	uint64_t DeadlineUs;
	xrudpresult Reason;
	bool Active;
} xrudpcheckstate;

typedef struct xrudpendpoint xrudpendpoint;
typedef struct xrudppeer xrudppeer;

/* Exactly once on the owner, deferred beyond Discover/Cancel. Result/error
 * borrowed for this call; no transaction nonce is exposed. Keep Context alive
 * through this callback, which precedes endpoint Close on shutdown. May ref/
 * destroy owned handles, cancel/start discoveries or close the endpoint.
 * No recursive discovery callbacks. A result is an address hint, not identity. */
typedef void (*xrudpdiscoveryproc)(xrudpendpoint* pEndpoint, uint64_t iId,
	const xrudpdiscoverystate* pResult, const xerror* pError, void* pContext);
/* Same deferred ownership/reentrancy rules as discovery. DONE reports only
 * local burst admission, never reachability/authentication. Error is the first
 * hard route error, if any, borrowed for this callback. */
typedef void (*xrudppunchproc)(xrudpendpoint* pEndpoint, uint64_t iId,
	const xrudppunchstate* pResult, const xerror* pError, void* pContext);
typedef enum xrudpendpointstate {
	XRUDP_ENDPOINT_OPENING = 0,
	XRUDP_ENDPOINT_OPEN,
	XRUDP_ENDPOINT_CLOSING,
	XRUDP_ENDPOINT_CLOSED
} xrudpendpointstate;
typedef enum xrudppeerstate {
	XRUDP_PEER_NEGOTIATING = 0,
	XRUDP_PEER_CONFIRMED,
	XRUDP_PEER_CLOSED
} xrudppeerstate;

/* All callbacks run on the bound Worker and borrow their arguments. Ref a peer
 * to keep its handle beyond the callback. Confirmed means identity/address/key
 * proof AND default reliable session admission have completed.
 * Callbacks may close/ref/destroy owned handles, but must not block the Worker.
 * Keep Context alive until the final Events.Close callback; release it there
 * or after it returns. EndpointClose is only an asynchronous close request. */
typedef struct xrudpendpointevents {
	void (*Open)(xrudpendpoint* pEndpoint, void* pContext);
	void (*Confirmed)(xrudpendpoint* pEndpoint, xrudppeer* pPeer, void* pContext);
	void (*PeerClose)(xrudpendpoint* pEndpoint, xrudppeer* pPeer, xrudpresult iReason, void* pContext);
	void (*Close)(xrudpendpoint* pEndpoint, xrudpresult iReason, const xerror* pError, void* pContext);
	/* Default stream/connection edge hints; drain Read to BACKPRESSURE/EOF/RESET. WRITE_READY means
	 * ACK returned buffer capacity; WRITE_DONE is all bytes+FIN acknowledged.
	 * Mutations inside a callback defer further Changed delivery to another
	 * worker turn; no recursive Changed callbacks. Borrowed peer and context. */
	void (*Changed)(xrudpendpoint* pEndpoint, xrudppeer* pPeer, uint32_t iEvents, void* pContext);
	/* All typed streams, including byte stream ID 1; XRUDP_STREAM_* masks. At most eight
	 * coalesced stream hints per dispatch. OPENED is remote-origin only. The
	 * borrowed event is valid only during this callback. Callbacks may perform
	 * stream operations or close/drop owned handles; never recursively invoked.
	 * Hints generated inside callbacks wait for a later dispatch (>=1ms).
	 * A retired ID's captured hint is discarded. Changed independently reports
	 * default FIN/read hints and STREAMS_READY when cumulative peer OPEN credit
	 * grows. After Open backpressure, retry on STREAMS_READY or local retirement.
	 * RETIRE_READY hints require a fresh State/Retire check: another control
	 * output may temporarily acquire a packet reference after the snapshot.
	 * PeerAbort may synchronously call PeerClose; StreamChanged never reenters. */
	void (*StreamChanged)(xrudpendpoint* pEndpoint, xrudppeer* pPeer, const xrudpstreamevent* pEvent, void* pContext);
} xrudpendpointevents;

typedef struct xrudpendpointconfig {
	const void* StaticPrivate; /* 32 bytes, copied; securely erased on Close. */
	const void* Scope; /* Optional nonzero 16 bytes; NULL creates a random scope. */
	const void* PeerPublic; /* Optional listener pin; pin or Trust required when Listen. */
	const void* ApplicationContext; /* Optional exact listener context, copied. */
	xrudpnoisetrustproc Trust;
	void* TrustContext;
	xrudpsessionconfig Session; /* Local advertised; Peer/Role supplied by authenticated handshake. */
	uint64_t HandshakeTimeoutUs;
	size_t MemoryLimit; /* Peak endpoint/peer/handshake/session reservation bytes.
	                    * XRT engine pools/kernel socket buffers have own limits. */
	uint32_t MaxPeers;
	uint32_t MaxHalfOpen;
	uint32_t HandshakesPerSecond;
	uint32_t AddressHandshakesPerSecond; /* IP (not port); fixed 64-entry table. */
	bool Listen;
	/* Opt-in, same-socket authenticated connectivity checks for a single pinned
	 * peer. Requires PeerPublic and ApplicationContext. At most 10s from Bind
	 * admission; at most eight destinations and observations, no per-packet heap.
	 * Both roles may Listen; only the application's chosen dialer Connects. */
	bool ConnectivityChecks;
	uint32_t ConnectivityTimeoutMs; /* Zero = 10000; application may shorten. */
    /* Optional Future adapter quota, both zero disables. Up to 64 retained
     * requests/results, 16MiB copied bytes+nodes in total. Requires the
     * rudp_endpoint_future module. Reserved within MemoryLimit at Bind. */
    uint32_t AsyncOperations;
    size_t AsyncBytes;
} xrudpendpointconfig;

typedef struct xrudpconnectconfig {
	const void* PeerPublic; /* Trusted 32-byte public key, mandatory. */
	const void* Scope; /* 16-byte listener scope from trusted discovery. */
	const void* ApplicationContext; /* Optional 32-byte binding, copied into handshake. */
} xrudpconnectconfig;

typedef struct xrudpendpointinfo {
	xnetaddr Local;
	unsigned char Public[32], Scope[16];
	uint32_t Peers, HalfOpen;
	size_t ReservedBytes, MemoryLimit;
	uint64_t Received, Sent, Invalid, RateDropped, AddressDropped, CapacityDropped, IoErrors;
	uint32_t Discoveries; /* Includes terminal results waiting for their callback. */
	uint64_t DiscoveryStarted, DiscoverySucceeded, DiscoveryFailed, DiscoveryBackpressure;
	uint32_t Punches; /* Includes completed bursts waiting for their callback. */
	uint64_t PunchStarted, PunchCompleted, PunchFailed, PunchBackpressure;
} xrudpendpointinfo;

XRT_EXTERN_C_BEGIN

XRUDP_API void xrtRudpEndpointConfigInit(xrudpendpointconfig* pConfig);
/* Must be called on a running Engine Worker. Binding and all mutable operations
 * are serialized on that Worker; no private engine/thread is created. Native
 * IPv4/IPv6 are supported as separate endpoints; IPv6 is v6-only.
 * Bind success returns one owned reference plus an internal I/O lifetime.
 * The endpoint owns UDP queues (64 packets/64KiB send; bounded push receive).
 * Context/Open callback cannot run before Bind returns on this Worker. */
XRUDP_API xrudpresult xrtRudpEndpointBind(xnetworker* pWorker, const xnetaddr* pLocal,
	const xrudpendpointconfig* pConfig, const xrudpendpointevents* pEvents,
	void* pContext, xrudpendpoint** ppEndpoint);
XRUDP_API xrudpendpoint* xrtRudpEndpointRef(xrudpendpoint* pEndpoint);
XRUDP_API void xrtRudpEndpointDestroy(xrudpendpoint* pEndpoint);
/* Thread-safe states. Info requires the owner while live; after CLOSED it is
 * an immutable snapshot and may be read even after the Engine is destroyed. */
XRUDP_API xrudpendpointstate xrtRudpEndpointState(const xrudpendpoint* pEndpoint);
XRUDP_API xrudppeerstate xrtRudpPeerState(const xrudppeer* pPeer);
XRUDP_API xrudpresult xrtRudpEndpointInfo(const xrudpendpoint* pEndpoint, xrudpendpointinfo* pInfo);
/* Owner-only, same UDP socket/port as handshake+business. Up to four fixed
 * tasks share the existing timer/queue, globally at most one send per 50ms.
 * Server is chosen by trusted application policy, same family as this socket.
 * No hostname resolution, credentials or redirect. Core timeout semantics.
 * Success reserves a monotonic nonreused ID and one callback; failure preserves
 * *pId and never calls it. Context ownership ends only at that callback.
 * Zero peers is supported. Fixed task storage participates in MemoryLimit;
 * XRT timer/UDP/error objects retain their own budgets. CLOSED is Engine-safe. */
XRUDP_API xrudpresult xrtRudpEndpointDiscover(xrudpendpoint* pEndpoint, const xnetaddr* pServer,
	uint32_t iTimeoutMs, bool bFingerprint, xrudpdiscoveryproc pCallback, void* pContext, uint64_t* pId);
/* Owner-only, no inline callback. OK=requested cancellation, DONE=unknown or
 * already terminal. Late cancellation cannot replace a completed result or
 * renew deadlines. Endpoint close cancels pending jobs before Close event. */
XRUDP_API xrudpresult xrtRudpEndpointDiscoveryCancel(xrudpendpoint* pEndpoint, uint64_t iId);
/* Owner-only, at most two fixed jobs with 1..8 trusted destinations each,
 * same family/socket/port as this endpoint's handshake and business traffic.
 * No new socket/engine/thread, peer/halfopen allocation or input responder.
 * Shares mapping's aggregate >=50ms send gate and fair access with peers.
 * Fixed storage counts against MemoryLimit. IDs are monotonic/nonreused in
 * their punch namespace. Failure preserves *pId, never calls Callback.
 * Context must survive through its exactly-once deferred callback, before
 * endpoint Close. Cancel after real authenticated peer confirmation or relay
 * fallback; this primitive does NOT automatically select/connect a candidate. */
XRUDP_API xrudpresult xrtRudpEndpointPunch(xrudpendpoint* pEndpoint,
	const xnetaddr* pCandidates, size_t iCount, uint32_t iTimeoutMs,
	xrudppunchproc pCallback, void* pContext, uint64_t* pId);
/* Owner-only, no inline callback. Pending cancellation returns OK; unknown or
 * already terminal DONE. Closing endpoint returns CLOSED, Engine-safe. */
XRUDP_API xrudpresult xrtRudpEndpointPunchCancel(xrudpendpoint* pEndpoint, uint64_t iId);
/* Owner-only, once per endpoint, 1..8 trusted unicast destinations. At most
 * five requests/destination under a fixed 1..10000ms deadline capped by the
 * endpoint's check lifetime. Return-path proof uses X25519/HKDF/HMAC and a
 * source-bound cookie, with separate packet directions and a fresh challenge.
 * Requests/responses/confirms have equal size; aggregate auxiliary admission
 * is paced at >=50ms. Learning never relaxes Noise's source/key validation. */
XRUDP_API xrudpresult xrtRudpEndpointCheck(xrudpendpoint* pEndpoint,
	const xnetaddr* pCandidates, size_t iCount, uint32_t iTimeoutMs);
/* Owner-only while live, immutable after CLOSED. No callback is made; the
 * owning application polls bounded observations and chooses its dialer. */
XRUDP_API xrudpresult xrtRudpEndpointCheckInfo(const xrudpendpoint* pEndpoint, xrudpcheckstate* pState);
/* Owner-only, idempotent. Erases credentials/pending packets, retains only
 * public observations. Closing also cancels checks before Close delivery. */
XRUDP_API xrudpresult xrtRudpEndpointCheckCancel(xrudpendpoint* pEndpoint);
XRUDP_API xrudpresult xrtRudpPeerInfo(const xrudppeer* pPeer, xrudphandshakeinfo* pInfo, xnetaddr* pAddress);
/* Owner-only; success returns an additional owned peer reference. Account ACL,
 * peer pin and scope must come from trusted application discovery. */
XRUDP_API xrudpresult xrtRudpEndpointConnect(xrudpendpoint* pEndpoint, const xnetaddr* pRemote,
	const xrudpconnectconfig* pConfig, xrudppeer** ppPeer);
XRUDP_API xrudppeer* xrtRudpPeerRef(xrudppeer* pPeer);
XRUDP_API void xrtRudpPeerDestroy(xrudppeer* pPeer);
/* Owner-only nonblocking default stream. Write copies a bounded prefix;
 * success is buffering, not delivery. Finish forbids further writes. Read
 * returns DONE at clean EOF, CANCELLED at peer RESET. Confirmed allows the
 * first write; subsequent capacity/EOF changes arrive as Changed hints. */
XRUDP_API xrudpresult xrtRudpPeerWrite(xrudppeer* pPeer, const void* pData, size_t iSize, size_t* pWritten);
XRUDP_API xrudpresult xrtRudpPeerRead(xrudppeer* pPeer, void* pOutput, size_t iCapacity, size_t* pRead);
XRUDP_API xrudpresult xrtRudpPeerFinish(xrudppeer* pPeer);
/* Owner-only SessionShutdown; existing reads remain available while draining.
 * Changed reports SHUTDOWN_REQUESTED once for the authenticated peer request.
 * Finish asynchronous responses, then explicitly Shutdown. PeerClose returns
 * CLOSED for a completed drain; PeerSessionState.Shutdown.Succeeded proves it.
 * Before Confirmed BACKPRESSURE, after CLOSED safe without Engine access. */
XRUDP_API xrudpresult xrtRudpPeerShutdown(xrudppeer* pPeer, uint32_t iTimeoutMs);
/* Owner-only, trusted discovery address, same IP family as the bound socket.
 * Starts bounded reachability validation without switching business output.
 * Candidate timeout leaves the peer live and emits PATH_FAILED; successful
 * validation emits PATH_CHANGED and PeerInfo returns the new active address.
 * SessionState.Path exposes bounded counters/deadlines; no challenge secrets.
 * This validates address reachability, not path MTU or local socket migration. */
XRUDP_API xrudpresult xrtRudpPeerProbePath(xrudppeer* pPeer, const xnetaddr* pCandidate);
/* Owner-only advanced typed streams; capacity/IDs/half-close/retirement follow
 * SessionStream*. No per-stream heap allocation. Before Confirmed return
 * BACKPRESSURE; after peer close return CLOSED, without accessing the Engine.
 * A stream state is live-only (the aggregate session snapshot survives Close).
 * Open output is unchanged on failure; Read/Write always set their count=0
 * before failing. Read RESET is CANCELLED without closing the connection. */
XRUDP_API xrudpresult xrtRudpPeerStreamOpen(xrudppeer* pPeer, const xrudpstreamconfig* pConfig, uint32_t* pId);
XRUDP_API xrudpresult xrtRudpPeerStreamState(const xrudppeer* pPeer, uint32_t iId, xrudpstreamstate* pState);
XRUDP_API xrudpresult xrtRudpPeerStreamWrite(xrudppeer* pPeer, uint32_t iId, const void* pData, size_t iSize, size_t* pWritten);
XRUDP_API xrudpresult xrtRudpPeerStreamRead(xrudppeer* pPeer, uint32_t iId, void* pOutput, size_t iCapacity, size_t* pRead);
/* Message-only whole-payload APIs, following SessionMessage*. Configure
 * Session.Local.MaxMessageSize before Bind; its staging memory participates in
 * endpoint peak reservation. Existing StreamChanged reports full message/EOF/
 * RESET readiness. Empty successful Read/Write also schedules the next turn.
 * No per-message allocation; Engine timers/UDP queues retain their own budgets.
 * Only the owner may call while confirmed, CLOSED thereafter. */
XRUDP_API xrudpresult xrtRudpPeerMessageWrite(xrudppeer* pPeer, uint32_t iId, const void* pData, size_t iSize);
XRUDP_API xrudpresult xrtRudpPeerMessagePeek(const xrudppeer* pPeer, uint32_t iId, size_t* pSize);
XRUDP_API xrudpresult xrtRudpPeerMessageRead(xrudppeer* pPeer, uint32_t iId, void* pOutput, size_t iCapacity, size_t* pSize);
/* Owner-only datagrams, after Confirmed. Bind reserves Session.Datagrams;
 * all-zero configuration disables the local queues. Register matching IDs/
 * modes at both peers before traffic. Unknown/full channels silently drop
 * unreliable input while preserving reliable frames in the same packet.
 * Changed reports DATAGRAM_READ_READY / WRITE_READY hints. Read may be bounded
 * by the application: after a successful Read, remaining records schedule a
 * later hint (>=1ms), never a recursive callback. Empty reads are successful.
 * Lifetime=0 disables per-record send expiry; otherwise <=10min, measured
 * from this native call's fresh local clock. No peer wall clock is involved.
 * Timed operations that discover a TERMINAL session timeout may synchronously
 * call PeerClose; an individual expired write does not close the connection.
 * No per-datagram allocation; native timer/UDP admission uses Engine budgets. */
XRUDP_API xrudpresult xrtRudpPeerDatagramConfigure(xrudppeer* pPeer, const xrudpdatagramchannel* pChannel);
XRUDP_API xrudpresult xrtRudpPeerDatagramChannelState(const xrudppeer* pPeer, uint32_t iChannel, xrudpdatagramchannelstate* pState);
XRUDP_API xrudpresult xrtRudpPeerDatagramWrite(xrudppeer* pPeer, uint32_t iChannel,
	const void* pData, size_t iSize, uint64_t iLifetimeUs, uint64_t* pSequence);
XRUDP_API xrudpresult xrtRudpPeerDatagramPeek(const xrudppeer* pPeer, xrudpdatagraminfo* pInfo);
XRUDP_API xrudpresult xrtRudpPeerDatagramRead(xrudppeer* pPeer, void* pOutput, size_t iCapacity, xrudpdatagraminfo* pInfo);
XRUDP_API xrudpresult xrtRudpPeerStreamFinish(xrudppeer* pPeer, uint32_t iId);
XRUDP_API xrudpresult xrtRudpPeerStreamReset(xrudppeer* pPeer, uint32_t iId, uint32_t iReason);
/* Owner-only receive-half cancellation, following SessionStreamStop, for byte
 * and message streams. Other direction/streams keep operating. Changed reports
 * WRITE_STOPPED for stream 1; StreamChanged reports it for any stream. Closed
 * handles return CLOSED without touching Engine. Success schedules maintenance;
 * callbacks remain deferred and may abort/drop the peer/close the endpoint. */
XRUDP_API xrudpresult xrtRudpPeerStop(xrudppeer* pPeer, uint32_t iReason);
XRUDP_API xrudpresult xrtRudpPeerStreamStop(xrudppeer* pPeer, uint32_t iId, uint32_t iReason);
XRUDP_API xrudpresult xrtRudpPeerStreamRetire(xrudppeer* pPeer, uint32_t iId);
/* Owner while live; immutable snapshot after CLOSED, safe after Engine destroy.
 * BACKPRESSURE before admission, CLOSED when no session was ever admitted. */
XRUDP_API xrudpresult xrtRudpPeerSessionState(const xrudppeer* pPeer, xrudpsessionstate* pState);
XRUDP_API xrudpresult xrtRudpPeerAbort(xrudppeer* pPeer);
/* Thread-safe, idempotent request, no allocation. Close callback runs once after
 * peer callbacks and timer cleanup. Dropping a handle alone does not close it.
 * XRT refuses Engine Stop while UDP objects are live: request endpoint Close
 * and await its terminal callback BEFORE Stop. Closed external handles retain
 * no engine resources and may be destroyed after the Engine. */
XRUDP_API xrudpresult xrtRudpEndpointClose(xrudpendpoint* pEndpoint);

XRT_EXTERN_C_END
#endif
#endif
